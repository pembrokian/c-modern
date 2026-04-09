#ifndef C_MODERN_COMPILER_SEMA_CONST_EVAL_H_
#define C_MODERN_COMPILER_SEMA_CONST_EVAL_H_

#include <optional>
#include <string>
#include <string_view>

#include "compiler/sema/check.h"

namespace mc::sema {

std::string RenderConstValue(const ConstValue& value);
ConstValue MakeConstValue(bool value);
ConstValue MakeConstValue(std::int64_t value);
ConstValue MakeConstValue(double value);
ConstValue MakeEnumConstValue(Type enum_type,
							  std::string variant_name,
							  std::int64_t variant_tag,
							  std::vector<std::string> field_names = {},
							  std::vector<ConstValue> elements = {});

std::optional<ConstValue> ParseLiteralConstValue(const ast::Expr& expr);
std::optional<ConstValue> EvaluateConstUnaryOp(std::string_view op, ConstValue value);
std::optional<ConstValue> EvaluateConstBinaryOp(std::string_view op, const ConstValue& left, const ConstValue& right);
std::optional<ConstValue> ParseGlobalConstValue(const GlobalSummary& global, std::string_view name);

}  // namespace mc::sema

#endif  // C_MODERN_COMPILER_SEMA_CONST_EVAL_H_