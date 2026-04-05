#include "compiler/sema/type_utils.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "compiler/sema/type_predicates.h"

namespace mc::sema {
namespace {

std::optional<Type> InferSliceElementType(const Type& type) {
    if (type.kind == Type::Kind::kArray && !type.subtypes.empty()) {
        return type.subtypes.front();
    }
    if (type.kind == Type::Kind::kNamed && (type.name == "Slice" || type.name == "Buffer") && !type.subtypes.empty()) {
        return type.subtypes.front();
    }
    return std::nullopt;
}

bool HasSuffix(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

bool ShouldStripTypeDeclKind(ast::Decl::Kind kind, TypeStripMode mode) {
    return kind == ast::Decl::Kind::kTypeAlias || (mode == TypeStripMode::kAliasesAndDistinct && kind == ast::Decl::Kind::kDistinct);
}

Type StripTypeInternal(Type type, const Module& module, TypeStripMode mode, std::unordered_set<std::string>& active_names) {
    for (auto& subtype : type.subtypes) {
        subtype = StripTypeInternal(std::move(subtype), module, mode, active_names);
    }

    if (type.kind != Type::Kind::kNamed) {
        return type;
    }

    const TypeDeclSummary* type_decl = FindTypeDecl(module, type.name);
    if (type_decl == nullptr || !ShouldStripTypeDeclKind(type_decl->kind, mode)) {
        return type;
    }

    if (!active_names.insert(type.name).second) {
        return type;
    }

    Type expanded = type_decl->aliased_type;
    if (!type_decl->type_params.empty()) {
        expanded = SubstituteTypeParams(std::move(expanded), type_decl->type_params, type.subtypes);
    }
    expanded = StripTypeInternal(std::move(expanded), module, mode, active_names);
    active_names.erase(type.name);
    return expanded;
}

bool IsNumericTypeWithModule(const Type& type, const Module& module) {
    const Type stripped = CanonicalizeBuiltinType(StripType(type, module, TypeStripMode::kAliasesOnly));
    return mc::sema::IsNumericType(stripped);
}

bool IsIntegerLikeTypeWithModule(const Type& type, const Module& module) {
    const Type stripped = CanonicalizeBuiltinType(StripType(type, module, TypeStripMode::kAliasesOnly));
    return mc::sema::IsIntegerLikeType(stripped);
}

bool IsPointerLikeTypeWithModule(const Type& type, const Module& module) {
    return mc::sema::IsPointerLikeType(StripType(type, module, TypeStripMode::kAliasesOnly));
}

bool IsUintPtrConvertibleTypeWithModule(const Type& type, const Module& module) {
    return mc::sema::IsUintPtrConvertibleType(StripType(type, module, TypeStripMode::kAliasesOnly));
}

bool IsUintPtrTypeWithModule(const Type& type, const Module& module) {
    const Type stripped = StripType(type, module, TypeStripMode::kAliasesOnly);
    return mc::sema::IsUintPtrType(stripped);
}

}  // namespace

Type SubstituteTypeParams(Type type, const std::vector<std::string>& type_params, const std::vector<Type>& type_args) {
    for (auto& subtype : type.subtypes) {
        subtype = SubstituteTypeParams(std::move(subtype), type_params, type_args);
    }

    if (type.kind != Type::Kind::kNamed) {
        return type;
    }

    for (std::size_t index = 0; index < type_params.size() && index < type_args.size(); ++index) {
        if (type.name == type_params[index]) {
            return type_args[index];
        }
    }

    return type;
}

bool IsBuiltinNamedType(std::string_view name) {
    return mc::sema::IsIntegerTypeName(name) || mc::sema::IsFloatTypeName(name) || name == "bool" || name == "string" || name == "str" ||
           name == "cstr" || name == "void" || name == "Slice" || name == "Buffer" || name == "Result";
}

Type InstantiateTypeDeclAliasedType(const TypeDeclSummary& type_decl, const Type& instantiated_type) {
    Type aliased_type = type_decl.aliased_type;
    if (!type_decl.type_params.empty()) {
        aliased_type = SubstituteTypeParams(std::move(aliased_type), type_decl.type_params, instantiated_type.subtypes);
    }
    return aliased_type;
}

std::vector<std::pair<std::string, Type>> InstantiateTypeDeclFields(const TypeDeclSummary& type_decl,
                                                                    const Type& instantiated_type) {
    std::vector<std::pair<std::string, Type>> fields = type_decl.fields;
    if (type_decl.type_params.empty()) {
        return fields;
    }
    for (auto& field : fields) {
        field.second = SubstituteTypeParams(std::move(field.second), type_decl.type_params, instantiated_type.subtypes);
    }
    return fields;
}

VariantSummary InstantiateVariantSummary(const VariantSummary& variant,
                                         const TypeDeclSummary& type_decl,
                                         const Type& instantiated_type) {
    VariantSummary instantiated = variant;
    if (type_decl.type_params.empty()) {
        return instantiated;
    }
    for (auto& payload_field : instantiated.payload_fields) {
        payload_field.second = SubstituteTypeParams(std::move(payload_field.second), type_decl.type_params, instantiated_type.subtypes);
    }
    return instantiated;
}

Type StripType(Type type, const Module& module, TypeStripMode mode) {
    std::unordered_set<std::string> active_names;
    return StripTypeInternal(std::move(type), module, mode, active_names);
}

Type ResolveCallableShell(Type type, const Module& module) {
    std::unordered_set<std::string> active_names;
    while (type.kind == Type::Kind::kNamed) {
        const TypeDeclSummary* type_decl = FindTypeDecl(module, type.name);
        if (type_decl == nullptr || type_decl->kind != ast::Decl::Kind::kTypeAlias) {
            break;
        }
        if (!active_names.insert(type.name).second) {
            break;
        }

        Type expanded = type_decl->aliased_type;
        if (!type_decl->type_params.empty()) {
            expanded = SubstituteTypeParams(std::move(expanded), type_decl->type_params, type.subtypes);
        }
        type = std::move(expanded);
    }
    return type;
}

bool IsNoAliasEligibleType(Type type, const Module& module) {
    type = StripType(std::move(type), module, TypeStripMode::kAliasesOnly);
    while (type.kind == Type::Kind::kConst && !type.subtypes.empty()) {
        type = StripType(std::move(type.subtypes.front()), module, TypeStripMode::kAliasesOnly);
    }
    return mc::sema::IsPointerLikeType(type);
}

bool IsErrorType(Type type, const Module& module) {
    type = StripType(std::move(type), module, TypeStripMode::kAliasesAndDistinct);
    type = CanonicalizeBuiltinType(std::move(type));
    return type.kind == Type::Kind::kNamed && (type.name == "Error" || HasSuffix(type.name, ".Error"));
}

bool IsAddressableExpr(const ast::Expr& expr) {
    switch (expr.kind) {
        case ast::Expr::Kind::kName:
        case ast::Expr::Kind::kQualifiedName:
        case ast::Expr::Kind::kField:
        case ast::Expr::Kind::kDerefField:
        case ast::Expr::Kind::kIndex:
            return true;
        case ast::Expr::Kind::kParen:
            return expr.left != nullptr && IsAddressableExpr(*expr.left);
        default:
            return false;
    }
}

bool IsValidDistinctBaseType(const Type& type, const Module& module) {
    const Type stripped = CanonicalizeBuiltinType(StripType(type, module, TypeStripMode::kAliasesOnly));
    if (stripped.kind == Type::Kind::kBool || stripped.kind == Type::Kind::kPointer) {
        return true;
    }
    if (stripped.kind != Type::Kind::kNamed) {
        return false;
    }
    return mc::sema::IsIntegerTypeName(stripped.name) || mc::sema::IsFloatTypeName(stripped.name);
}

Type InferExplicitConversionTarget(Type target, const Type& actual, const Module& module) {
    if (target.kind == Type::Kind::kNamed && target.name == "Slice" && target.subtypes.empty()) {
        if (const auto element_type = InferSliceElementType(actual); element_type.has_value()) {
            target.subtypes.push_back(*element_type);
            return target;
        }
    }

    const Type stripped_target = StripType(target, module, TypeStripMode::kAliasesAndDistinct);
    if (stripped_target.kind == Type::Kind::kNamed && stripped_target.name == "Slice" && stripped_target.subtypes.empty()) {
        Type inferred = stripped_target;
        if (const auto element_type = InferSliceElementType(actual); element_type.has_value()) {
            inferred.subtypes.push_back(*element_type);
            return inferred;
        }
    }

    return target;
}

bool IsExplicitlyConvertible(const Type& expected, const Type& actual, const Module& module) {
    if (IsAssignable(expected, actual, module)) {
        return true;
    }

    if (expected.kind == Type::Kind::kNamed && expected.name == "Slice" && !expected.subtypes.empty()) {
        if (const auto element_type = InferSliceElementType(actual); element_type.has_value()) {
            return IsAssignable(expected.subtypes.front(), *element_type, module);
        }
    }

    const Type stripped_expected = StripType(expected, module, TypeStripMode::kAliasesAndDistinct);
    const Type stripped_actual = StripType(actual, module, TypeStripMode::kAliasesAndDistinct);
    if (IsAssignable(stripped_expected, stripped_actual, module)) {
        return true;
    }

    if (stripped_expected.kind == Type::Kind::kNamed && stripped_expected.name == "Slice" && !stripped_expected.subtypes.empty()) {
        if (const auto element_type = InferSliceElementType(stripped_actual); element_type.has_value()) {
            return IsAssignable(stripped_expected.subtypes.front(), *element_type, module);
        }
    }

    if ((IsUintPtrConvertibleTypeWithModule(stripped_expected, module) && IsUintPtrTypeWithModule(stripped_actual, module)) ||
        (IsUintPtrTypeWithModule(stripped_expected, module) && IsUintPtrConvertibleTypeWithModule(stripped_actual, module))) {
        return true;
    }

    return IsNumericTypeWithModule(stripped_expected, module) && IsNumericTypeWithModule(stripped_actual, module);
}

Type InferLiteralType(const ast::Expr& expr) {
    if (expr.text == "true" || expr.text == "false") {
        return BoolType();
    }
    if (expr.text == "nil") {
        return NilType();
    }
    if (!expr.text.empty() && expr.text.front() == '"') {
        return StringType();
    }
    if (expr.text.find_first_of(".eE") != std::string::npos) {
        return FloatLiteralType();
    }
    return IntLiteralType();
}

Type MergeNumericTypes(const Type& left, const Type& right, const Module& module) {
    const Type stripped_left = CanonicalizeBuiltinType(StripType(left, module, TypeStripMode::kAliasesOnly));
    const Type stripped_right = CanonicalizeBuiltinType(StripType(right, module, TypeStripMode::kAliasesOnly));
    if (IsUnknown(stripped_left) || IsUnknown(stripped_right)) {
        return UnknownType();
    }
    if (stripped_left == stripped_right) {
        return stripped_left;
    }
    if (stripped_left.kind == Type::Kind::kNamed && stripped_right.kind == Type::Kind::kIntLiteral &&
        IsIntegerTypeName(stripped_left.name)) {
        return stripped_left;
    }
    if (stripped_right.kind == Type::Kind::kNamed && stripped_left.kind == Type::Kind::kIntLiteral &&
        IsIntegerTypeName(stripped_right.name)) {
        return stripped_right;
    }
    if (stripped_left.kind == Type::Kind::kNamed && stripped_right.kind == Type::Kind::kFloatLiteral &&
        IsFloatTypeName(stripped_left.name)) {
        return stripped_left;
    }
    if (stripped_right.kind == Type::Kind::kNamed && stripped_left.kind == Type::Kind::kFloatLiteral &&
        IsFloatTypeName(stripped_right.name)) {
        return stripped_right;
    }
    if (stripped_left.kind == Type::Kind::kFloatLiteral && stripped_right.kind == Type::Kind::kIntLiteral) {
        return stripped_left;
    }
    if (stripped_right.kind == Type::Kind::kFloatLiteral && stripped_left.kind == Type::Kind::kIntLiteral) {
        return stripped_right;
    }
    return UnknownType();
}

bool IsAssignable(const Type& expected, const Type& actual, const Module& module) {
    const Type canonical_expected = CanonicalizeBuiltinType(StripType(expected, module, TypeStripMode::kAliasesOnly));
    const Type canonical_actual = CanonicalizeBuiltinType(StripType(actual, module, TypeStripMode::kAliasesOnly));
    if (IsUnknown(canonical_expected) || IsUnknown(canonical_actual)) {
        return true;
    }
    if (canonical_expected == canonical_actual) {
        return true;
    }
    if (canonical_expected.kind == Type::Kind::kNamed && canonical_actual.kind == Type::Kind::kIntLiteral &&
        IsIntegerTypeName(canonical_expected.name)) {
        return true;
    }
    if (canonical_expected.kind == Type::Kind::kNamed && canonical_actual.kind == Type::Kind::kFloatLiteral &&
        IsFloatTypeName(canonical_expected.name)) {
        return true;
    }
    if (IsPointerLikeTypeWithModule(canonical_expected, module) && canonical_actual.kind == Type::Kind::kNil) {
        return true;
    }
    return false;
}

}  // namespace mc::sema