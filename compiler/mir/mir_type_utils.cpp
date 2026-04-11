#include "compiler/mir/mir_internal.h"

#include <cassert>
#include <cstddef>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "compiler/sema/type_utils.h"
#include "compiler/sema/type_predicates.h"

namespace mc::mir {

using mc::ast::Expr;

sema::Type InstantiateMirAliasedType(const TypeDecl& type_decl, const sema::Type& instantiated_type) {
    sema::Type aliased_type = type_decl.aliased_type;
    if (!type_decl.type_params.empty()) {
        aliased_type = sema::SubstituteTypeParams(std::move(aliased_type), type_decl.type_params, instantiated_type.subtypes);
    }
    return aliased_type;
}

std::vector<std::pair<std::string, sema::Type>> InstantiateMirFields(const TypeDecl& type_decl,
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

std::optional<VariantDecl> InstantiateMirVariantDecl(const TypeDecl& type_decl,
                                                     const sema::Type& instantiated_type,
                                                     std::string_view variant_name) {
    const VariantDecl* variant = FindMirVariantDecl(type_decl, variant_name);
    if (variant == nullptr) {
        return std::nullopt;
    }
    VariantDecl instantiated = *variant;
    if (type_decl.type_params.empty()) {
        return instantiated;
    }
    for (auto& payload_field : instantiated.payload_fields) {
        payload_field.second =
            sema::SubstituteTypeParams(std::move(payload_field.second), type_decl.type_params, instantiated_type.subtypes);
    }
    return instantiated;
}

bool IsAddressOfLvalueKind(Expr::Kind kind) {
    switch (kind) {
        case Expr::Kind::kName:
        case Expr::Kind::kQualifiedName:
        case Expr::Kind::kField:
        case Expr::Kind::kDerefField:
        case Expr::Kind::kIndex:
        case Expr::Kind::kParen:
            return true;
        default:
            return false;
    }
}

sema::Type CanonicalMirType(const Module& module, const sema::Type& type) {
    return sema::CanonicalizeBuiltinType(StripMirAliasOrDistinct(module, type));
}

sema::Type RepresentationMirType(const Module& module, const sema::Type& type) {
    sema::Type representation = StripMirAliasOrDistinct(module, type);
    while (representation.kind == sema::Type::Kind::kConst && !representation.subtypes.empty()) {
        representation = StripMirAliasOrDistinct(module, std::move(representation.subtypes.front()));
    }

    representation = sema::CanonicalizeBuiltinType(std::move(representation));
    for (auto& subtype : representation.subtypes) {
        subtype = RepresentationMirType(module, subtype);
    }
    return representation;
}

bool IsIntegerType(const Module& module, const sema::Type& type) {
    return sema::IsIntegerType(CanonicalMirType(module, type));
}

bool IsFloatType(const Module& module, const sema::Type& type) {
    return sema::IsFloatType(CanonicalMirType(module, type));
}

bool IsNumericType(const Module& module, const sema::Type& type) {
    return sema::IsNumericType(CanonicalMirType(module, type));
}

bool IsWraparoundBinaryOp(std::string_view op) {
    return op == "+" || op == "-" || op == "*";
}

bool IsIntegerLikeType(const Module& module, const sema::Type& type) {
    return sema::IsIntegerLikeType(CanonicalMirType(module, type));
}

bool IsBoolType(const Module& module, const sema::Type& type) {
    return sema::IsBoolType(CanonicalMirType(module, type));
}

std::string_view LeafTypeName(std::string_view name) {
    const std::size_t separator = name.rfind('.');
    if (separator == std::string_view::npos) {
        return name;
    }
    return name.substr(separator + 1);
}

bool IsNamedTypeFamily(const sema::Type& type, std::string_view family_name) {
    return type.kind == sema::Type::Kind::kNamed && LeafTypeName(type.name) == family_name;
}

bool IsMemoryOrderType(const Module& module, const sema::Type& type) {
    return IsNamedTypeFamily(StripMirAliasOrDistinct(module, type), "MemoryOrder");
}

bool IsPointerLikeType(const Module& module, const sema::Type& type) {
    return sema::IsPointerLikeType(CanonicalMirType(module, type));
}

bool IsUintPtrConvertibleType(const Module& module, const sema::Type& type) {
    return sema::IsUintPtrConvertibleType(CanonicalMirType(module, type));
}

bool IsUintPtrType(const Module& module, const sema::Type& type) {
    return sema::IsUintPtrType(CanonicalMirType(module, type));
}

bool IsBuiltinNamedNonAggregate(const sema::Type& type) {
    const sema::Type canonical = sema::CanonicalizeBuiltinType(type);
    if (canonical.kind == sema::Type::Kind::kBool || canonical.kind == sema::Type::Kind::kString) {
        return true;
    }
    if (canonical.kind != sema::Type::Kind::kNamed) {
        return false;
    }
    return sema::IsIntegerType(canonical) || sema::IsFloatType(canonical) || canonical.name == "bool";
}

bool IsBuiltinNamedNonAggregate(const Module& module, const sema::Type& type) {
    return IsBuiltinNamedNonAggregate(sema::CanonicalizeBuiltinType(StripMirAliasOrDistinct(module, type)));
}

bool IsUsizeCompatibleType(const sema::Type& type) {
    return type == sema::NamedType("usize") || type.kind == sema::Type::Kind::kIntLiteral;
}

bool IsUsizeCompatibleType(const Module& module, const sema::Type& type) {
    return IsUsizeCompatibleType(sema::CanonicalizeBuiltinType(StripMirAliasOrDistinct(module, type)));
}

bool IsAssignableCanonicalTypes(const sema::Type& expected, const sema::Type& actual) {
    if (sema::IsUnknown(expected) || sema::IsUnknown(actual)) {
        return true;
    }
    if (expected == actual) {
        return true;
    }
    if (sema::IsIntegerType(expected) && actual.kind == sema::Type::Kind::kIntLiteral) {
        return true;
    }
    if (sema::IsFloatType(expected) && actual.kind == sema::Type::Kind::kFloatLiteral) {
        return true;
    }
    if (sema::IsPointerLikeType(expected) && actual.kind == sema::Type::Kind::kNil) {
        return true;
    }
    return false;
}

bool IsAssignableType(const Module& module, const sema::Type& expected, const sema::Type& actual) {
    return IsAssignableCanonicalTypes(CanonicalMirType(module, expected), CanonicalMirType(module, actual));
}

bool HasCompatibleCanonicalNumericTypes(const sema::Type& left, const sema::Type& right) {
    if (sema::IsUnknown(left) || sema::IsUnknown(right)) {
        return true;
    }
    if (left == right) {
        return sema::IsNumericType(left);
    }
    if (sema::IsIntegerType(left) && right.kind == sema::Type::Kind::kIntLiteral) {
        return true;
    }
    if (sema::IsIntegerType(right) && left.kind == sema::Type::Kind::kIntLiteral) {
        return true;
    }
    if (sema::IsFloatType(left) && right.kind == sema::Type::Kind::kFloatLiteral) {
        return true;
    }
    if (sema::IsFloatType(right) && left.kind == sema::Type::Kind::kFloatLiteral) {
        return true;
    }
    if ((left.kind == sema::Type::Kind::kFloatLiteral && right.kind == sema::Type::Kind::kIntLiteral) ||
        (right.kind == sema::Type::Kind::kFloatLiteral && left.kind == sema::Type::Kind::kIntLiteral)) {
        return true;
    }
    return false;
}

bool HasCompatibleNumericTypes(const Module& module, const sema::Type& left, const sema::Type& right) {
    return HasCompatibleCanonicalNumericTypes(CanonicalMirType(module, left), CanonicalMirType(module, right));
}

bool HasCompatibleComparisonTypes(const Module& module, const sema::Type& left, const sema::Type& right) {
    return HasCompatibleNumericTypes(module, left, right) || IsAssignableType(module, left, right) || IsAssignableType(module, right, left);
}

bool AreEquivalentTypes(const Module& module, const sema::Type& left, const sema::Type& right) {
    return IsAssignableType(module, left, right) && IsAssignableType(module, right, left);
}

sema::Type RangeElementType(const sema::Type& range_type) {
    if (range_type.kind == sema::Type::Kind::kRange && !range_type.subtypes.empty()) {
        return range_type.subtypes.front();
    }
    return sema::UnknownType();
}

sema::Type IterableElementType(const sema::Type& iterable_type) {
    if (iterable_type.kind == sema::Type::Kind::kArray && !iterable_type.subtypes.empty()) {
        return iterable_type.subtypes.front();
    }
    if (iterable_type.kind == sema::Type::Kind::kNamed &&
        (IsNamedTypeFamily(iterable_type, "Slice") || IsNamedTypeFamily(iterable_type, "Buffer")) &&
        !iterable_type.subtypes.empty()) {
        return iterable_type.subtypes.front();
    }
    return sema::UnknownType();
}

sema::Type StripMirAliasOrDistinct(const Module& module, sema::Type type) {
    std::unordered_set<std::string> visited;
    while (type.kind == sema::Type::Kind::kNamed) {
        const TypeDecl* type_decl = FindMirTypeDecl(module, type.name);
        if (type_decl == nullptr) {
            break;
        }
        if (type_decl->kind != TypeDecl::Kind::kDistinct && type_decl->kind != TypeDecl::Kind::kAlias) {
            break;
        }
        if (!visited.insert(type.name).second) {
            break;
        }
        type = InstantiateMirAliasedType(*type_decl, type);
    }
    return type;
}

std::optional<sema::Type> PointerPointeeType(const sema::Type& type) {
    if (type.kind != sema::Type::Kind::kPointer || type.subtypes.empty()) {
        return std::nullopt;
    }
    return type.subtypes.front();
}

std::optional<sema::Type> AtomicElementType(const Module& module, const sema::Type& pointer_type) {
    const auto pointee = PointerPointeeType(StripMirAliasOrDistinct(module, pointer_type));
    if (!pointee.has_value()) {
        return std::nullopt;
    }
    const sema::Type stripped_pointee = StripMirAliasOrDistinct(module, *pointee);
    if (stripped_pointee.kind != sema::Type::Kind::kNamed || !IsNamedTypeFamily(stripped_pointee, "Atomic") ||
        stripped_pointee.subtypes.empty()) {
        return std::nullopt;
    }
    return stripped_pointee.subtypes.front();
}

ExplicitConversionKind ClassifyMirConversion(const Module& module, const sema::Type& source_type, const sema::Type& target_type) {
    if (target_type.kind == sema::Type::Kind::kNamed && IsNamedTypeFamily(target_type, "Slice")) {
        if (source_type.kind == sema::Type::Kind::kArray) {
            return ExplicitConversionKind::kArrayToSlice;
        }
        if (source_type.kind == sema::Type::Kind::kNamed && IsNamedTypeFamily(source_type, "Buffer")) {
            return ExplicitConversionKind::kBufferToSlice;
        }
    }

    const sema::Type stripped_source = StripMirAliasOrDistinct(module, source_type);
    const sema::Type stripped_target = StripMirAliasOrDistinct(module, target_type);
    if (sema::IsUintPtrConvertibleType(stripped_source) && sema::IsUintPtrType(stripped_target)) {
        return ExplicitConversionKind::kPointerToInt;
    }
    if (sema::IsUintPtrType(stripped_source) && sema::IsUintPtrConvertibleType(stripped_target)) {
        return ExplicitConversionKind::kIntToPointer;
    }
    const bool source_wrapped = stripped_source != source_type;
    const bool target_wrapped = stripped_target != target_type;
    if ((source_wrapped || target_wrapped) && stripped_source == stripped_target) {
        return ExplicitConversionKind::kDistinct;
    }

    if (sema::IsNumericType(stripped_source) && sema::IsNumericType(stripped_target)) {
        return ExplicitConversionKind::kNumeric;
    }

    return ExplicitConversionKind::kGeneric;
}

bool HasSameMirRepresentation(const Module& module, const sema::Type& source_type, const sema::Type& target_type) {
    return RepresentationMirType(module, source_type) == RepresentationMirType(module, target_type);
}

Instruction::ArithmeticSemantics ClassifyBinaryArithmeticSemantics(const Module& module,
                                                                   std::string_view op,
                                                                   const sema::Type& result_type) {
    if (!IsWraparoundBinaryOp(op)) {
        return Instruction::ArithmeticSemantics::kNone;
    }

    const sema::Type stripped_result = StripMirAliasOrDistinct(module, result_type);
    if (sema::IsIntegerType(stripped_result)) {
        return Instruction::ArithmeticSemantics::kWrap;
    }
    return Instruction::ArithmeticSemantics::kNone;
}

std::size_t ProcedureParamCount(const sema::Type& type) {
    if (type.kind != sema::Type::Kind::kProcedure || type.metadata.empty()) {
        return 0;
    }
    const auto parsed = mc::support::ParseArrayLength(type.metadata);
    assert(parsed.has_value() && "procedure metadata must encode parameter count");
    return parsed.value_or(0);
}

std::vector<sema::Type> ProcedureParamTypes(const sema::Type& type) {
    if (type.kind != sema::Type::Kind::kProcedure) {
        return {};
    }
    const std::size_t param_count = ProcedureParamCount(type);
    if (param_count > type.subtypes.size()) {
        return {};
    }
    return std::vector<sema::Type>(type.subtypes.begin(),
                                   type.subtypes.begin() + static_cast<std::ptrdiff_t>(param_count));
}

std::vector<sema::Type> ProcedureReturnTypes(const sema::Type& type) {
    if (type.kind != sema::Type::Kind::kProcedure) {
        return {};
    }
    const std::size_t param_count = ProcedureParamCount(type);
    if (param_count >= type.subtypes.size()) {
        return {};
    }
    return std::vector<sema::Type>(type.subtypes.begin() + static_cast<std::ptrdiff_t>(param_count), type.subtypes.end());
}

sema::Type ProcedureResultType(const sema::Type& type) {
    const std::vector<sema::Type> returns = ProcedureReturnTypes(type);
    if (returns.empty()) {
        return sema::VoidType();
    }
    if (returns.size() == 1) {
        return returns.front();
    }
    return sema::TupleType(returns);
}

std::vector<sema::Type> CallReturnTypes(const sema::Type& result_type) {
    if (result_type.kind == sema::Type::Kind::kVoid) {
        return {};
    }
    if (result_type.kind == sema::Type::Kind::kTuple) {
        return result_type.subtypes;
    }
    return {result_type};
}

}  // namespace mc::mir