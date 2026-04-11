#include "compiler/sema/layout.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "compiler/sema/type_utils.h"

namespace mc::sema {

namespace {

std::size_t AlignTo(std::size_t value, std::size_t alignment) {
    if (alignment == 0) {
        return value;
    }
    const std::size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
}

TypeDeclSummary* FindMutableTypeDecl(Module& module, std::string_view name) {
    for (auto& type_decl : module.type_decls) {
        if (type_decl.name == name) {
            return &type_decl;
        }
    }
    return nullptr;
}

class LayoutComputer {
  public:
    LayoutComputer(Module& module, const LayoutReportFn& report)
        : module_(module), report_(report) {}

    LayoutInfo Compute(const Type& type,
                       const support::SourceSpan& span,
                       std::unordered_set<std::string>& active_types) {
        switch (type.kind) {
            case Type::Kind::kBool:
                return {.valid = true, .size = 1, .alignment = 1};
            case Type::Kind::kString:
                return {.valid = true, .size = 16, .alignment = 8};
            case Type::Kind::kPointer:
                return {.valid = true, .size = 8, .alignment = 8};
            case Type::Kind::kProcedure:
                return {.valid = true, .size = 8, .alignment = 8};
            case Type::Kind::kConst:
                return type.subtypes.empty() ? LayoutInfo {} : Compute(type.subtypes.front(), span, active_types);
            case Type::Kind::kArray: {
                if (type.subtypes.empty()) {
                    return {};
                }
                const LayoutInfo element_layout = Compute(type.subtypes.front(), span, active_types);
                if (!element_layout.valid) {
                    return {};
                }
                const auto length = mc::support::ParseArrayLength(type.metadata);
                if (!length.has_value()) {
                    Report(span, "array layout requires an integer constant length, got " + type.metadata);
                    return {};
                }
                return {
                    .valid = true,
                    .size = element_layout.size * *length,
                    .alignment = element_layout.alignment,
                };
            }
            case Type::Kind::kNamed:
                break;
            case Type::Kind::kUnknown:
            case Type::Kind::kVoid:
            case Type::Kind::kNil:
            case Type::Kind::kIntLiteral:
            case Type::Kind::kFloatLiteral:
            case Type::Kind::kTuple:
            case Type::Kind::kRange:
                return {};
        }

        if (type.name == "i8" || type.name == "u8") {
            return {.valid = true, .size = 1, .alignment = 1};
        }
        if (type.name == "i16" || type.name == "u16") {
            return {.valid = true, .size = 2, .alignment = 2};
        }
        if (type.name == "i32" || type.name == "u32" || type.name == "f32") {
            return {.valid = true, .size = 4, .alignment = 4};
        }
        if (type.name == "i64" || type.name == "u64" || type.name == "isize" || type.name == "usize" || type.name == "uintptr" ||
            type.name == "f64") {
            return {.valid = true, .size = 8, .alignment = 8};
        }
        if (type.name == "Slice") {
            return {.valid = true, .size = 16, .alignment = 8};
        }
        if (type.name == "Buffer") {
            return {.valid = true, .size = 32, .alignment = 8};
        }

        TypeDeclSummary* type_decl = FindMutableTypeDecl(module_, type.name);
        if (type_decl == nullptr) {
            Report(span, "layout is not available for type " + FormatType(type));
            return {};
        }

        if (type_decl->kind == ast::Decl::Kind::kDistinct || type_decl->kind == ast::Decl::Kind::kTypeAlias) {
            return Compute(InstantiateTypeDeclAliasedType(*type_decl, type), span, active_types);
        }

        if (type_decl->kind == ast::Decl::Kind::kEnum) {
            return ComputeEnumLayout(*type_decl, type, span, active_types);
        }

        if (type_decl->kind != ast::Decl::Kind::kStruct) {
            Report(span, "layout is not available for type " + type_decl->name + " in bootstrap sema");
            return {};
        }

        return ComputeStructLayout(*type_decl, type, span, active_types);
    }

  private:
    void Report(const support::SourceSpan& span, const std::string& message) const {
        if (report_) {
            report_(span, message);
        }
    }

    LayoutInfo ComputeEnumLayout(TypeDeclSummary& type_decl,
                                 const Type& type,
                                 const support::SourceSpan& span,
                                 std::unordered_set<std::string>& active_types) {
        if (type_decl.type_params.empty() && type_decl.layout.valid) {
            return type_decl.layout;
        }

        const std::string active_key = FormatType(type);
        if (!active_types.insert(active_key).second) {
            Report(span, "type layout cycle requires indirection: " + active_key);
            return {};
        }

        LayoutInfo layout;
        layout.valid = true;
        std::size_t payload_size = 0;
        std::size_t payload_alignment = 1;
        for (const auto& variant : type_decl.variants) {
            std::size_t variant_size = 0;
            std::size_t variant_alignment = 1;
            for (const auto& payload_field : variant.payload_fields) {
                Type field_type = payload_field.second;
                if (!type_decl.type_params.empty()) {
                    field_type = SubstituteTypeParams(std::move(field_type), type_decl.type_params, type.subtypes);
                }
                const LayoutInfo field_layout = Compute(field_type, span, active_types);
                if (!field_layout.valid) {
                    active_types.erase(active_key);
                    return {};
                }
                variant_size = AlignTo(variant_size, field_layout.alignment);
                variant_size += field_layout.size;
                variant_alignment = std::max(variant_alignment, field_layout.alignment);
            }
            variant_size = AlignTo(variant_size, variant_alignment);
            payload_size = std::max(payload_size, variant_size);
            payload_alignment = std::max(payload_alignment, variant_alignment);
        }

        // Bootstrap enum layout matches the backend's current tagged-union
        // lowering: an i64 tag plus enough payload storage for the largest
        // instantiated variant payload.
        layout.alignment = std::max<std::size_t>(8, payload_alignment);
        layout.size = AlignTo(8 + payload_size, layout.alignment);

        active_types.erase(active_key);
        if (type_decl.type_params.empty()) {
            type_decl.layout = layout;
        }
        return layout;
    }

    LayoutInfo ComputeStructLayout(TypeDeclSummary& type_decl,
                                   const Type& type,
                                   const support::SourceSpan& span,
                                   std::unordered_set<std::string>& active_types) {
        if (type_decl.type_params.empty() && type_decl.layout.valid) {
            return type_decl.layout;
        }

        const std::string active_key = FormatType(type);
        if (!active_types.insert(active_key).second) {
            Report(span, "type layout cycle requires indirection: " + active_key);
            return {};
        }

        LayoutInfo layout;
        layout.valid = true;
        layout.alignment = type_decl.is_packed ? 1 : 0;
        std::size_t size = 0;
        const auto instantiated_fields = InstantiateTypeDeclFields(type_decl, type);
        for (const auto& field : instantiated_fields) {
            const LayoutInfo field_layout = Compute(field.second, span, active_types);
            if (!field_layout.valid) {
                active_types.erase(active_key);
                return {};
            }
            const std::size_t field_alignment = type_decl.is_packed ? 1 : field_layout.alignment;
            size = AlignTo(size, field_alignment);
            layout.field_offsets.push_back(size);
            size += field_layout.size;
            layout.alignment = std::max(layout.alignment, field_alignment);
        }
        layout.size = AlignTo(size, layout.alignment);

        active_types.erase(active_key);
        if (type_decl.type_params.empty()) {
            type_decl.layout = layout;
        }
        return layout;
    }

    Module& module_;
    const LayoutReportFn& report_;
};

}  // namespace

LayoutInfo ComputeTypeLayout(Module& module,
                             const Type& type,
                             const support::SourceSpan& span,
                             const LayoutReportFn& report) {
    std::unordered_set<std::string> active_types;
    LayoutComputer computer(module, report);
    return computer.Compute(type, span, active_types);
}

void ComputeTypeLayouts(Module& module,
                        const support::SourceSpan& span,
                        const LayoutReportFn& report) {
    LayoutComputer computer(module, report);
    std::unordered_set<std::string> active_types;
    for (auto& type_decl : module.type_decls) {
        if (type_decl.kind != ast::Decl::Kind::kStruct && type_decl.kind != ast::Decl::Kind::kEnum) {
            continue;
        }
        if (!type_decl.type_params.empty() || type_decl.layout.valid) {
            continue;
        }
        (void)computer.Compute(NamedType(type_decl.name), span, active_types);
        active_types.clear();
    }
}

}  // namespace mc::sema