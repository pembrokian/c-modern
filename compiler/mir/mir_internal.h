#ifndef C_MODERN_COMPILER_MIR_MIR_INTERNAL_H_
#define C_MODERN_COMPILER_MIR_MIR_INTERNAL_H_

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "compiler/mir/mir.h"

namespace mc::mir {

struct ValueInfo {
    std::string value;
    sema::Type type;
};

struct ValuePiece {
    ValueInfo value;
    mc::support::SourceSpan span;
};

enum class SpecialCallKind {
    kNone,
    kMmioPtr,
    kBufferNew,
    kBufferFree,
    kArenaNew,
    kSliceFromBuffer,
    kVolatileLoad,
    kVolatileStore,
    kAtomicLoad,
    kAtomicStore,
    kAtomicExchange,
    kAtomicCompareExchange,
    kAtomicFetchAdd,
};

enum class ExplicitConversionKind {
    kGeneric,
    kNumeric,
    kDistinct,
    kPointerToInt,
    kIntToPointer,
    kArrayToSlice,
    kBufferToSlice,
};

std::string_view ToString(TypeDecl::Kind kind);
std::string_view ToString(Instruction::Kind kind);
std::string_view ToString(Instruction::TargetKind kind);
std::string_view ToString(Instruction::ArithmeticSemantics semantics);
std::string_view ToString(ast::Expr::Kind kind);

std::string VariantTypeName(std::string_view variant_name);
std::string VariantLeafName(std::string_view variant_name);
std::string CombineQualifiedName(const ast::Expr& expr);
std::string RenderTypeExprInline(const ast::TypeExpr& type);
std::string RenderExprInline(const ast::Expr& expr);

void WriteLine(std::ostringstream& stream, int indent, const std::string& text);

sema::Type InstantiateMirAliasedType(const TypeDecl& type_decl, const sema::Type& instantiated_type);
std::vector<std::pair<std::string, sema::Type>> InstantiateMirFields(const TypeDecl& type_decl,
                                                                     const sema::Type& instantiated_type);
std::optional<VariantDecl> InstantiateMirVariantDecl(const TypeDecl& type_decl,
                                                     const sema::Type& instantiated_type,
                                                     std::string_view variant_name);

sema::Type CanonicalMirType(const Module& module, const sema::Type& type);
bool IsAddressOfLvalueKind(ast::Expr::Kind kind);
bool IsIntegerType(const Module& module, const sema::Type& type);
bool IsFloatType(const Module& module, const sema::Type& type);
bool IsNumericType(const Module& module, const sema::Type& type);
bool IsWraparoundBinaryOp(std::string_view op);
bool IsIntegerLikeType(const Module& module, const sema::Type& type);
bool IsBoolType(const Module& module, const sema::Type& type);
std::string_view LeafTypeName(std::string_view name);
bool IsNamedTypeFamily(const sema::Type& type, std::string_view family_name);
bool IsMemoryOrderType(const Module& module, const sema::Type& type);
bool IsPointerLikeType(const Module& module, const sema::Type& type);
bool IsUintPtrConvertibleType(const Module& module, const sema::Type& type);
bool IsUintPtrType(const Module& module, const sema::Type& type);
bool IsBuiltinNamedNonAggregate(const sema::Type& type);
bool IsBuiltinNamedNonAggregate(const Module& module, const sema::Type& type);
bool IsUsizeCompatibleType(const sema::Type& type);
bool IsUsizeCompatibleType(const Module& module, const sema::Type& type);
bool IsAssignableCanonicalTypes(const sema::Type& expected, const sema::Type& actual);
bool IsAssignableType(const Module& module, const sema::Type& expected, const sema::Type& actual);
bool HasCompatibleCanonicalNumericTypes(const sema::Type& left, const sema::Type& right);
bool HasCompatibleNumericTypes(const Module& module, const sema::Type& left, const sema::Type& right);
bool HasCompatibleComparisonTypes(const Module& module, const sema::Type& left, const sema::Type& right);
bool AreEquivalentTypes(const Module& module, const sema::Type& left, const sema::Type& right);
sema::Type RangeElementType(const sema::Type& range_type);
sema::Type IterableElementType(const sema::Type& iterable_type);
sema::Type StripMirAliasOrDistinct(const Module& module, sema::Type type);
std::optional<sema::Type> PointerPointeeType(const sema::Type& type);
std::optional<sema::Type> AtomicElementType(const Module& module, const sema::Type& pointer_type);

SpecialCallKind ClassifySpecialCall(std::string_view callee_name);
bool IsDedicatedCallSurface(std::string_view callee_name);
bool HasAtomicOrderMetadata(const Instruction& instruction);
bool HasCompareExchangeOrderMetadata(const Instruction& instruction);
std::string AtomicMetadataText(const Instruction& instruction);
ExplicitConversionKind ClassifyMirConversion(const Module& module, const sema::Type& source_type, const sema::Type& target_type);
Instruction::ArithmeticSemantics ClassifyBinaryArithmeticSemantics(const Module& module,
                                                                   std::string_view op,
                                                                   const sema::Type& result_type);

std::size_t ProcedureParamCount(const sema::Type& type);
std::vector<sema::Type> ProcedureParamTypes(const sema::Type& type);
std::vector<sema::Type> ProcedureReturnTypes(const sema::Type& type);
sema::Type ProcedureResultType(const sema::Type& type);
std::vector<sema::Type> CallReturnTypes(const sema::Type& result_type);

const TypeDecl* FindMirTypeDecl(const Module& module, std::string_view name);
const GlobalDecl* FindMirGlobalDecl(const Module& module, std::string_view name);
std::optional<sema::Type> FindMirFieldType(const Module& module,
                                           const sema::Type& base_type,
                                           std::string_view field_name);
const Function* FindMirFunction(const Module& module, std::string_view name);
sema::Type FunctionProcedureType(const Function& function);
bool MatchesGenericFunctionType(const Module& module,
                                const Function& function,
                                const sema::Type& actual_type);
const VariantDecl* FindMirVariantDecl(const TypeDecl& type_decl, std::string_view variant_name);
std::string_view PrimaryTargetName(const Instruction& instruction);
std::string CanonicalVariantDisplayName(const sema::Type& selector_type, std::string_view variant_name);
bool MatchesTargetDisplay(std::string_view target, const sema::Type& type);

}  // namespace mc::mir

#endif  // C_MODERN_COMPILER_MIR_MIR_INTERNAL_H_