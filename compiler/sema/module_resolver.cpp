#include "compiler/sema/module_resolver.h"

#include <algorithm>
#include <unordered_set>

#include "compiler/sema/const_eval.h"
#include "compiler/sema/type_utils.h"

namespace mc::sema {
namespace {

bool DeclHasPrivateAttribute(const ast::Decl& decl) {
    return std::any_of(decl.attributes.begin(), decl.attributes.end(), [](const ast::Attribute& attribute) {
        return attribute.name == "private";
    });
}

void RecordHiddenDeclNames(const ast::Decl& decl,
                          Module& module) {
    if (!DeclHasPrivateAttribute(decl)) {
        return;
    }

    switch (decl.kind) {
        case ast::Decl::Kind::kFunc:
        case ast::Decl::Kind::kExternFunc:
            if (!decl.name.empty()) {
                module.hidden_value_names.insert(decl.name);
            }
            break;
        case ast::Decl::Kind::kConst:
        case ast::Decl::Kind::kVar:
            for (const auto& name : decl.pattern.names) {
                module.hidden_value_names.insert(name);
            }
            break;
        case ast::Decl::Kind::kStruct:
        case ast::Decl::Kind::kEnum:
        case ast::Decl::Kind::kDistinct:
        case ast::Decl::Kind::kTypeAlias:
            if (!decl.name.empty()) {
                module.hidden_type_names.insert(decl.name);
            }
            break;
    }
}

Type RewriteImportedTypeNames(Type type,
                              std::string_view module_name,
                              const std::unordered_set<std::string>& local_type_names,
                              const std::vector<std::string>& type_params = {}) {
    for (auto& subtype : type.subtypes) {
        subtype = RewriteImportedTypeNames(std::move(subtype), module_name, local_type_names, type_params);
    }

    if (type.kind != Type::Kind::kNamed || type.name.empty() || type.name.find('.') != std::string::npos) {
        return type;
    }
    if (IsBuiltinNamedType(type.name) || std::find(type_params.begin(), type_params.end(), type.name) != type_params.end()) {
        return type;
    }
    if (local_type_names.contains(type.name)) {
        type.name = QualifyImportedName(module_name, type.name);
    }
    return type;
}

ConstValue RewriteImportedConstValue(ConstValue value,
                                     std::string_view module_name,
                                     const std::unordered_set<std::string>& local_type_names,
                                     const std::vector<std::string>& type_params = {}) {
    if (value.kind == ConstValue::Kind::kEnum) {
        value.enum_type = RewriteImportedTypeNames(std::move(value.enum_type), module_name, local_type_names, type_params);
    }
    for (auto& element : value.elements) {
        element = RewriteImportedConstValue(std::move(element), module_name, local_type_names, type_params);
    }
    value.text = RenderConstValue(value);
    return value;
}

bool EquivalentLayoutInfo(const LayoutInfo& left, const LayoutInfo& right) {
    return left.valid == right.valid && left.size == right.size && left.alignment == right.alignment &&
           left.field_offsets == right.field_offsets;
}

bool EquivalentVariantSummary(const VariantSummary& left, const VariantSummary& right) {
    if (left.name != right.name || left.payload_fields.size() != right.payload_fields.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.payload_fields.size(); ++index) {
        if (left.payload_fields[index].first != right.payload_fields[index].first ||
            !(left.payload_fields[index].second == right.payload_fields[index].second)) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::string QualifyImportedName(std::string_view module_name, std::string_view name) {
    return std::string(module_name) + "." + std::string(name);
}

TypeDeclSummary RewriteImportedTypeDecl(const TypeDeclSummary& type_decl,
                                        std::string_view module_name,
                                        const std::unordered_set<std::string>& local_type_names) {
    TypeDeclSummary qualified = type_decl;
    qualified.name = type_decl.name.find('.') == std::string::npos ? QualifyImportedName(module_name, type_decl.name) : type_decl.name;
    for (auto& field : qualified.fields) {
        field.second = RewriteImportedTypeNames(std::move(field.second), module_name, local_type_names, qualified.type_params);
    }
    for (auto& variant : qualified.variants) {
        for (auto& payload_field : variant.payload_fields) {
            payload_field.second = RewriteImportedTypeNames(std::move(payload_field.second), module_name, local_type_names, qualified.type_params);
        }
    }
    qualified.aliased_type = RewriteImportedTypeNames(std::move(qualified.aliased_type), module_name, local_type_names, qualified.type_params);
    return qualified;
}

bool EquivalentTypeDeclSummary(const TypeDeclSummary& left, const TypeDeclSummary& right) {
    if (left.kind != right.kind || left.name != right.name || left.type_params != right.type_params ||
        left.is_packed != right.is_packed || left.is_abi_c != right.is_abi_c ||
        !(left.aliased_type == right.aliased_type) || !EquivalentLayoutInfo(left.layout, right.layout) ||
        left.fields.size() != right.fields.size() || left.variants.size() != right.variants.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.fields.size(); ++index) {
        if (left.fields[index].first != right.fields[index].first ||
            !(left.fields[index].second == right.fields[index].second)) {
            return false;
        }
    }

    for (std::size_t index = 0; index < left.variants.size(); ++index) {
        if (!EquivalentVariantSummary(left.variants[index], right.variants[index])) {
            return false;
        }
    }
    return true;
}

Module BuildImportVisibleModuleSurface(const Module& module,
                                       const ast::SourceFile& source_file) {
    Module visible;
    visible.module_kind = module.module_kind;
    visible.package_identity = module.package_identity;
    visible.imports = module.imports;

    std::unordered_set<std::string> private_type_names;
    for (const auto& decl : source_file.decls) {
        RecordHiddenDeclNames(decl, visible);
        if (decl.kind == ast::Decl::Kind::kStruct || decl.kind == ast::Decl::Kind::kEnum ||
            decl.kind == ast::Decl::Kind::kDistinct || decl.kind == ast::Decl::Kind::kTypeAlias) {
            if (DeclHasPrivateAttribute(decl) && !decl.name.empty()) {
                private_type_names.insert(decl.name);
            }
        }
    }

    for (const auto& function : module.functions) {
        if (!visible.hidden_value_names.contains(function.name)) {
            visible.functions.push_back(function);
        }
    }

    for (const auto& global : module.globals) {
        GlobalSummary filtered = global;
        filtered.names.clear();
        filtered.constant_values.clear();
        filtered.zero_initialized_values.clear();
        for (std::size_t index = 0; index < global.names.size(); ++index) {
            if (visible.hidden_value_names.contains(global.names[index])) {
                continue;
            }
            filtered.names.push_back(global.names[index]);
            if (index < global.constant_values.size()) {
                filtered.constant_values.push_back(global.constant_values[index]);
            }
            if (index < global.zero_initialized_values.size()) {
                filtered.zero_initialized_values.push_back(global.zero_initialized_values[index]);
            }
        }
        if (!filtered.names.empty()) {
            visible.globals.push_back(std::move(filtered));
        }
    }

    for (const auto& type_decl : module.type_decls) {
        if (private_type_names.contains(type_decl.name)) {
            continue;
        }
        visible.type_decls.push_back(type_decl);
    }

    BuildModuleLookupMaps(visible);
    return visible;
}

Module RewriteImportedModuleSurfaceTypes(const Module& module,
                                         std::string_view module_name) {
    Module rewritten = module;
    std::unordered_set<std::string> local_type_names;
    for (const auto& type_decl : module.type_decls) {
        local_type_names.insert(type_decl.name);
    }

    for (auto& type_decl : rewritten.type_decls) {
        type_decl = RewriteImportedTypeDecl(type_decl, module_name, local_type_names);
    }
    for (auto& global : rewritten.globals) {
        global.type = RewriteImportedTypeNames(std::move(global.type), module_name, local_type_names);
        for (auto& value : global.constant_values) {
            if (!value.has_value()) {
                continue;
            }
            value = RewriteImportedConstValue(std::move(*value), module_name, local_type_names);
        }
    }
    for (auto& function : rewritten.functions) {
        for (auto& param_type : function.param_types) {
            param_type = RewriteImportedTypeNames(std::move(param_type), module_name, local_type_names, function.type_params);
        }
        for (auto& return_type : function.return_types) {
            return_type = RewriteImportedTypeNames(std::move(return_type), module_name, local_type_names, function.type_params);
        }
    }
    return rewritten;
}

}  // namespace mc::sema