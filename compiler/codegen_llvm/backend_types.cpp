#include "compiler/codegen_llvm/backend_internal.h"

#include <algorithm>
#include <sstream>

#include "compiler/sema/type_utils.h"

namespace mc::codegen_llvm {

sema::Type InstantiateAliasedType(const mir::TypeDecl& type_decl, const sema::Type& instantiated_type) {
    sema::Type aliased_type = type_decl.aliased_type;
    if (!type_decl.type_params.empty()) {
        aliased_type = sema::SubstituteTypeParams(std::move(aliased_type), type_decl.type_params, instantiated_type.subtypes);
    }
    return aliased_type;
}

std::vector<std::pair<std::string, sema::Type>> InstantiateFields(const mir::TypeDecl& type_decl,
                                                                  const sema::Type& instantiated_type) {
    std::vector<std::pair<std::string, sema::Type>> fields = type_decl.fields;
    if (type_decl.type_params.empty()) {
        return fields;
    }
    for (auto& field : fields) {
        field.second = sema::SubstituteTypeParams(std::move(field.second), type_decl.type_params, instantiated_type.subtypes);
    }
    return fields;
}

bool IsAggregateType(const BackendTypeInfo& type_info) {
    return !type_info.backend_name.empty() &&
           (type_info.backend_name.front() == '{' || type_info.backend_name.front() == '[' || type_info.backend_name.front() == '<');
}

namespace {

std::optional<BackendTypeInfo> LowerTypeInfoImpl(const mir::Module& module,
                                                 const sema::Type& type,
                                                 bool aggregate_field_storage);

std::size_t AlignTo(std::size_t value,
                    std::size_t alignment) {
    if (alignment == 0) {
        return value;
    }
    const std::size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
}

std::optional<BackendTypeInfo> LowerStructTypeInfo(const mir::Module& module,
                                                   const mir::TypeDecl& type_decl,
                                                   const sema::Type& original_type) {
    BackendTypeInfo info {
        .source_name = sema::FormatType(original_type),
    };

    if (type_decl.kind != mir::TypeDecl::Kind::kStruct) {
        return std::nullopt;
    }

    std::ostringstream backend_name;
    if (type_decl.is_packed) {
        backend_name << "<{";
    } else {
        backend_name << '{';
    }

    std::size_t size = 0;
    std::size_t alignment = 1;
    const auto fields = InstantiateFields(type_decl, original_type);
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto lowered_field = LowerTypeInfoImpl(module, fields[index].second, true);
        if (!lowered_field.has_value()) {
            return std::nullopt;
        }
        const std::size_t field_alignment = type_decl.is_packed ? 1 : lowered_field->alignment;

        if (index > 0) {
            backend_name << ", ";
        }
        backend_name << lowered_field->backend_name;

        if (!type_decl.is_packed) {
            size = AlignTo(size, field_alignment);
        }
        size += lowered_field->size;
        alignment = std::max(alignment, field_alignment);
    }

    if (type_decl.is_packed) {
        backend_name << "}>";
    } else {
        backend_name << '}';
    }
    info.backend_name = backend_name.str();
    info.alignment = alignment;
    info.size = type_decl.is_packed ? size : AlignTo(size, alignment);
    return info;
}

std::optional<BackendTypeInfo> LowerTypeInfoImpl(const mir::Module& module,
                                                 const sema::Type& type,
                                                 bool aggregate_field_storage) {
    BackendTypeInfo info {
        .source_name = sema::FormatType(type),
    };

    const auto use_backend_type = [&](std::string backend_name,
                                      std::size_t size,
                                      std::size_t alignment) -> std::optional<BackendTypeInfo> {
        info.backend_name = std::move(backend_name);
        info.size = size;
        info.alignment = alignment;
        return info;
    };

    switch (type.kind) {
        case sema::Type::Kind::kBool:
            return use_backend_type(aggregate_field_storage ? "i8" : "i1", 1, 1);
        case sema::Type::Kind::kString:
            return use_backend_type("{ptr, i64}", 16, 8);
        case sema::Type::Kind::kIntLiteral:
            return use_backend_type("i64", 8, 8);
        case sema::Type::Kind::kFloatLiteral:
            return use_backend_type("double", 8, 8);
        case sema::Type::Kind::kNamed:
            if (type.name == "bool") {
                return use_backend_type(aggregate_field_storage ? "i8" : "i1", 1, 1);
            }
            if (type.name == "i8" || type.name == "u8") {
                return use_backend_type("i8", 1, 1);
            }
            if (type.name == "i16" || type.name == "u16") {
                return use_backend_type("i16", 2, 2);
            }
            if (type.name == "i32" || type.name == "u32") {
                return use_backend_type("i32", 4, 4);
            }
            if (type.name == "i64" || type.name == "u64" || type.name == "isize" || type.name == "usize" ||
                type.name == "uintptr") {
                return use_backend_type("i64", 8, 8);
            }
            if (type.name == "f32") {
                return use_backend_type("float", 4, 4);
            }
            if (type.name == "f64") {
                return use_backend_type("double", 8, 8);
            }
            if (type.name == "str" || type.name == "string" || type.name == "cstr") {
                return use_backend_type("{ptr, i64}", 16, 8);
            }
            if (type.name == "Slice") {
                return use_backend_type("{ptr, i64}", 16, 8);
            }
            if (type.name == "Buffer") {
                return use_backend_type("{ptr, i64, i64, ptr}", 32, 8);
            }
            if (const mir::TypeDecl* type_decl = FindTypeDecl(module, type.name)) {
                if (type_decl->kind == mir::TypeDecl::Kind::kAlias || type_decl->kind == mir::TypeDecl::Kind::kDistinct) {
                    auto lowered = LowerTypeInfoImpl(module, InstantiateAliasedType(*type_decl, type), aggregate_field_storage);
                    if (!lowered.has_value()) {
                        return std::nullopt;
                    }
                    lowered->source_name = info.source_name;
                    return lowered;
                }
                if (type_decl->kind == mir::TypeDecl::Kind::kStruct) {
                    return LowerStructTypeInfo(module, *type_decl, type);
                }
                if (type_decl->kind == mir::TypeDecl::Kind::kEnum) {
                    auto lowered = LowerEnumLayout(module, *type_decl, type);
                    if (!lowered.has_value()) {
                        return std::nullopt;
                    }
                    return lowered->aggregate_type;
                }
            }
            return std::nullopt;
        case sema::Type::Kind::kPointer:
            return use_backend_type("ptr", 8, 8);
        case sema::Type::Kind::kConst: {
            if (type.subtypes.empty()) {
                return std::nullopt;
            }
            auto lowered = LowerTypeInfoImpl(module, type.subtypes.front(), aggregate_field_storage);
            if (!lowered.has_value()) {
                return std::nullopt;
            }
            lowered->source_name = info.source_name;
            return lowered;
        }
        case sema::Type::Kind::kArray: {
            if (type.subtypes.empty()) {
                return std::nullopt;
            }
            auto element = LowerTypeInfoImpl(module, type.subtypes.front(), aggregate_field_storage);
            const auto length = mc::support::ParseArrayLength(type.metadata);
            if (!element.has_value() || !length.has_value()) {
                return std::nullopt;
            }
            return use_backend_type("[" + std::to_string(*length) + " x " + element->backend_name + "]",
                                    element->size * *length,
                                    element->alignment);
        }
        case sema::Type::Kind::kTuple: {
            std::ostringstream backend_name;
            backend_name << '{';
            std::size_t size = 0;
            std::size_t alignment = 1;
            for (std::size_t index = 0; index < type.subtypes.size(); ++index) {
                auto lowered_field = LowerTypeInfoImpl(module, type.subtypes[index], aggregate_field_storage);
                if (!lowered_field.has_value()) {
                    return std::nullopt;
                }
                if (index > 0) {
                    backend_name << ", ";
                }
                backend_name << lowered_field->backend_name;
                size = AlignTo(size, lowered_field->alignment);
                size += lowered_field->size;
                alignment = std::max(alignment, lowered_field->alignment);
            }
            backend_name << '}';
            return use_backend_type(backend_name.str(), AlignTo(size, alignment), alignment);
        }
        case sema::Type::Kind::kProcedure:
            return use_backend_type("ptr", 8, 8);
        case sema::Type::Kind::kNil:
            return use_backend_type("ptr", 8, 8);
        default:
            return std::nullopt;
    }
}

}  // namespace

void ReportBackendError(const std::filesystem::path& source_path,
                        const std::string& message,
                        support::DiagnosticSink& diagnostics) {
    diagnostics.Report({
        .file_path = source_path,
        .span = support::kDefaultSourceSpan,
        .severity = support::DiagnosticSeverity::kError,
        .message = message,
    });
}

const mir::TypeDecl* FindTypeDecl(const mir::Module& module,
                                  std::string_view name) {
    for (const auto& type_decl : module.type_decls) {
        if (type_decl.name == name) {
            return &type_decl;
        }
    }

    return nullptr;
}

std::string VariantLeafName(std::string_view variant_name) {
    const std::size_t separator = variant_name.rfind('.');
    if (separator == std::string_view::npos) {
        return std::string(variant_name);
    }
    return std::string(variant_name.substr(separator + 1));
}

const mir::VariantDecl* FindVariantDecl(const mir::TypeDecl& type_decl,
                                        std::string_view variant_name,
                                        std::size_t* variant_index) {
    const std::string leaf_name = VariantLeafName(variant_name);
    for (std::size_t index = 0; index < type_decl.variants.size(); ++index) {
        if (type_decl.variants[index].name == leaf_name) {
            if (variant_index != nullptr) {
                *variant_index = index;
            }
            return &type_decl.variants[index];
        }
    }
    return nullptr;
}

std::optional<std::size_t> ParseBackendArrayLength(std::string_view text) {
    if (text.size() < 2 || text.front() != '[') {
        return std::nullopt;
    }

    const std::size_t marker = text.find(" x ");
    if (marker == std::string_view::npos || marker <= 1) {
        return std::nullopt;
    }

    return mc::support::ParseArrayLength(text.substr(1, marker - 1));
}

std::optional<EnumBackendLayout> LowerEnumLayout(const mir::Module& module,
                                                 const mir::TypeDecl& type_decl,
                                                 const sema::Type& original_type) {
    if (type_decl.kind != mir::TypeDecl::Kind::kEnum) {
        return std::nullopt;
    }

    EnumBackendLayout layout;
    layout.aggregate_type.source_name = sema::FormatType(original_type);
    // Bootstrap enum tags are always lowered as i64. Revisit once the backend
    // models ABI-sized discriminants explicitly.
    for (const auto& raw_variant : type_decl.variants) {
        mir::VariantDecl variant = raw_variant;
        if (!type_decl.type_params.empty()) {
            for (auto& field : variant.payload_fields) {
                field.second = sema::SubstituteTypeParams(std::move(field.second), type_decl.type_params, original_type.subtypes);
            }
        }
        std::vector<BackendTypeInfo> variant_field_types;
        std::vector<std::size_t> variant_payload_offsets;
        std::size_t variant_size = 0;
        std::size_t variant_alignment = 1;
        for (const auto& field : variant.payload_fields) {
            const auto lowered_field = LowerTypeInfo(module, field.second);
            if (!lowered_field.has_value()) {
                return std::nullopt;
            }

            variant_size = AlignTo(variant_size, lowered_field->alignment);
            variant_payload_offsets.push_back(variant_size);
            variant_size += lowered_field->size;
            variant_alignment = std::max(variant_alignment, lowered_field->alignment);
            variant_field_types.push_back(*lowered_field);
        }

        variant_size = AlignTo(variant_size, variant_alignment);
        layout.payload_size = std::max(layout.payload_size, variant_size);
        layout.payload_alignment = std::max(layout.payload_alignment, variant_alignment);
        layout.variant_field_types.push_back(std::move(variant_field_types));
        layout.variant_payload_offsets.push_back(std::move(variant_payload_offsets));
    }

    std::ostringstream backend_name;
    if (layout.payload_size == 0) {
        backend_name << "{i64}";
    } else {
        layout.payload_type = {
            .source_name = layout.aggregate_type.source_name + ".payload",
            .backend_name = "[" + std::to_string(layout.payload_size) + " x i8]",
            .size = layout.payload_size,
            .alignment = 1,
        };
        backend_name << "{i64, " << layout.payload_type.backend_name << '}';
    }

    layout.aggregate_type.backend_name = backend_name.str();
    layout.aggregate_type.alignment = std::max<std::size_t>(8, layout.payload_alignment);
    layout.aggregate_type.size = AlignTo(8 + layout.payload_size, layout.aggregate_type.alignment);
    return layout;
}

std::optional<BackendTypeInfo> LowerTypeInfo(const mir::Module& module,
                                             const sema::Type& type) {
    return LowerTypeInfoImpl(module, type, false);
}

std::optional<BackendTypeInfo> LowerAggregateFieldStorageType(const mir::Module& module,
                                                              const sema::Type& aggregate_type,
                                                              std::string_view field_name) {
    const auto builtin_fields = sema::BuiltinAggregateFields(aggregate_type);
    if (!builtin_fields.empty()) {
        for (const auto& field : builtin_fields) {
            if (field.first == field_name) {
                return LowerTypeInfoImpl(module, field.second, true);
            }
        }
        return std::nullopt;
    }

    if (aggregate_type.kind != sema::Type::Kind::kNamed) {
        return std::nullopt;
    }

    const mir::TypeDecl* type_decl = FindTypeDecl(module, aggregate_type.name);
    if (type_decl == nullptr || type_decl->kind != mir::TypeDecl::Kind::kStruct) {
        return std::nullopt;
    }

    const auto fields = InstantiateFields(*type_decl, aggregate_type);
    for (const auto& field : fields) {
        if (field.first == field_name) {
            return LowerTypeInfoImpl(module, field.second, true);
        }
    }

    return std::nullopt;
}

std::string FormatTypeInfo(const BackendTypeInfo& type_info) {
    std::ostringstream stream;
    stream << type_info.source_name << " [repr=" << type_info.backend_name << ", size=" << type_info.size
           << ", align=" << type_info.alignment << ']';
    return stream.str();
}

bool LowerInstructionType(const mir::Module& module,
                          const sema::Type& type,
                          const std::filesystem::path& source_path,
                          support::DiagnosticSink& diagnostics,
                          const std::string& context,
                          BackendTypeInfo& type_info) {
    auto lowered = LowerTypeInfo(module, type);
    if (lowered.has_value()) {
        type_info = std::move(*lowered);
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap backend cannot map MIR type '" + sema::FormatType(type) +
                           "' to a Darwin arm64 representation in " + context,
                       diagnostics);
    return false;
}

std::string FormatReturnTypes(const std::vector<BackendTypeInfo>& types) {
    std::ostringstream stream;
    stream << '[';
    for (std::size_t index = 0; index < types.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << FormatTypeInfo(types[index]);
    }
    stream << ']';
    return stream.str();
}

std::optional<BackendTypeInfo> LowerFunctionReturnType(const mir::Module& module,
                                                       const std::vector<sema::Type>& return_types) {
    if (return_types.empty()) {
        return std::nullopt;
    }
    if (return_types.size() == 1) {
        return LowerTypeInfo(module, return_types.front());
    }
    return LowerTypeInfo(module, sema::TupleType(return_types));
}

}  // namespace mc::codegen_llvm