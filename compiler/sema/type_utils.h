#ifndef C_MODERN_COMPILER_SEMA_TYPE_UTILS_H_
#define C_MODERN_COMPILER_SEMA_TYPE_UTILS_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "compiler/sema/check.h"

namespace mc::sema {

enum class TypeStripMode {
    kAliasesOnly,
    kAliasesAndDistinct,
};

Type SubstituteTypeParams(Type type,
                          const std::vector<std::string>& type_params,
                          const std::vector<Type>& type_args);

bool IsBuiltinNamedType(std::string_view name);
Type InstantiateTypeDeclAliasedType(const TypeDeclSummary& type_decl, const Type& instantiated_type);
std::vector<std::pair<std::string, Type>> InstantiateTypeDeclFields(const TypeDeclSummary& type_decl,
                                                                    const Type& instantiated_type);
VariantSummary InstantiateVariantSummary(const VariantSummary& variant,
                                         const TypeDeclSummary& type_decl,
                                         const Type& instantiated_type);
Type StripType(Type type, const Module& module, TypeStripMode mode);
Type ResolveCallableShell(Type type, const Module& module);
bool IsNoAliasEligibleType(Type type, const Module& module);
bool IsErrorType(Type type, const Module& module);
bool IsAddressableExpr(const ast::Expr& expr);
bool IsValidDistinctBaseType(const Type& type, const Module& module);
Type InferExplicitConversionTarget(Type target, const Type& actual, const Module& module);
bool IsExplicitlyConvertible(const Type& expected, const Type& actual, const Module& module);
Type InferLiteralType(const ast::Expr& expr);
Type MergeNumericTypes(const Type& left, const Type& right, const Module& module);
bool IsAssignable(const Type& expected, const Type& actual, const Module& module);

}  // namespace mc::sema

#endif  // C_MODERN_COMPILER_SEMA_TYPE_UTILS_H_