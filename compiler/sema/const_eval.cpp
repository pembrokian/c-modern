#include "compiler/sema/const_eval.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>

namespace mc::sema {
namespace {

std::int64_t WrapAddInt64(std::int64_t lhs, std::int64_t rhs) {
    return std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(lhs) + std::bit_cast<std::uint64_t>(rhs));
}

std::int64_t WrapSubInt64(std::int64_t lhs, std::int64_t rhs) {
    return std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(lhs) - std::bit_cast<std::uint64_t>(rhs));
}

std::int64_t WrapMulInt64(std::int64_t lhs, std::int64_t rhs) {
    return std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(lhs) * std::bit_cast<std::uint64_t>(rhs));
}

std::optional<std::int64_t> CheckedNegateInt64(std::int64_t value) {
    if (value == std::numeric_limits<std::int64_t>::min()) {
        return std::nullopt;
    }
    return -value;
}

std::optional<std::int64_t> CheckedDivideInt64(std::int64_t lhs, std::int64_t rhs) {
    if (rhs == 0 || (lhs == std::numeric_limits<std::int64_t>::min() && rhs == -1)) {
        return std::nullopt;
    }
    return lhs / rhs;
}

std::optional<std::int64_t> CheckedRemainderInt64(std::int64_t lhs, std::int64_t rhs) {
    if (rhs == 0 || (lhs == std::numeric_limits<std::int64_t>::min() && rhs == -1)) {
        return std::nullopt;
    }
    return lhs % rhs;
}

std::optional<std::int64_t> CheckedShiftLeftInt64(std::int64_t lhs, std::int64_t rhs) {
    if (rhs < 0 || rhs >= 64) {
        return std::nullopt;
    }
    const auto shifted = static_cast<std::uint64_t>(lhs) << static_cast<std::uint64_t>(rhs);
    return std::bit_cast<std::int64_t>(shifted);
}

std::optional<std::int64_t> CheckedShiftRightInt64(std::int64_t lhs, std::int64_t rhs) {
    if (rhs < 0 || rhs >= 64) {
        return std::nullopt;
    }
    return lhs >> rhs;
}

std::optional<std::int64_t> BitwiseAndInt64(std::int64_t lhs, std::int64_t rhs) {
    return lhs & rhs;
}

std::optional<std::int64_t> BitwiseOrInt64(std::int64_t lhs, std::int64_t rhs) {
    return lhs | rhs;
}

std::optional<std::int64_t> BitwiseXorInt64(std::int64_t lhs, std::int64_t rhs) {
    return lhs ^ rhs;
}

using CheckedIntBinaryOp = std::optional<std::int64_t> (*)(std::int64_t, std::int64_t);

std::optional<std::int64_t> WrapAddInt64Checked(std::int64_t lhs, std::int64_t rhs) {
    return WrapAddInt64(lhs, rhs);
}

std::optional<std::int64_t> WrapSubInt64Checked(std::int64_t lhs, std::int64_t rhs) {
    return WrapSubInt64(lhs, rhs);
}

std::optional<std::int64_t> WrapMulInt64Checked(std::int64_t lhs, std::int64_t rhs) {
    return WrapMulInt64(lhs, rhs);
}

struct IntBinaryOpEntry {
    std::string_view op;
    CheckedIntBinaryOp evaluate;
};

constexpr IntBinaryOpEntry kConstIntegerBinaryOps[] = {
    {"+", WrapAddInt64Checked},
    {"-", WrapSubInt64Checked},
    {"*", WrapMulInt64Checked},
    {"/", CheckedDivideInt64},
    {"%", CheckedRemainderInt64},
    {"<<", CheckedShiftLeftInt64},
    {">>", CheckedShiftRightInt64},
    {"&", BitwiseAndInt64},
    {"|", BitwiseOrInt64},
    {"^", BitwiseXorInt64},
};

CheckedIntBinaryOp FindConstIntegerBinaryOp(std::string_view op) {
    for (const auto& entry : kConstIntegerBinaryOps) {
        if (entry.op == op) {
            return entry.evaluate;
        }
    }
    return nullptr;
}

bool IsConstComparisonOp(std::string_view op) {
    return op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=";
}

bool EvaluateOrderedComparison(std::string_view op, double lhs, double rhs) {
    if (op == "==") {
        return lhs == rhs;
    }
    if (op == "!=") {
        return lhs != rhs;
    }
    if (op == "<") {
        return lhs < rhs;
    }
    if (op == ">") {
        return lhs > rhs;
    }
    if (op == "<=") {
        return lhs <= rhs;
    }
    return lhs >= rhs;
}

bool EvaluateOrderedComparison(std::string_view op, std::int64_t lhs, std::int64_t rhs) {
    if (op == "==") {
        return lhs == rhs;
    }
    if (op == "!=") {
        return lhs != rhs;
    }
    if (op == "<") {
        return lhs < rhs;
    }
    if (op == ">") {
        return lhs > rhs;
    }
    if (op == "<=") {
        return lhs <= rhs;
    }
    return lhs >= rhs;
}

std::optional<ConstValue> EvaluateConstComparison(std::string_view op, const ConstValue& left, const ConstValue& right) {
    if (!IsConstComparisonOp(op)) {
        return std::nullopt;
    }
    if (left.kind == ConstValue::Kind::kFloat || right.kind == ConstValue::Kind::kFloat) {
        const double lhs = left.kind == ConstValue::Kind::kFloat ? left.float_value : left.integer_value;
        const double rhs = right.kind == ConstValue::Kind::kFloat ? right.float_value : right.integer_value;
        return MakeConstValue(EvaluateOrderedComparison(op, lhs, rhs));
    }
    if (left.kind != ConstValue::Kind::kInteger || right.kind != ConstValue::Kind::kInteger) {
        return std::nullopt;
    }
    return MakeConstValue(EvaluateOrderedComparison(op, left.integer_value, right.integer_value));
}

std::optional<ConstValue> EvaluateConstLogicalBinary(std::string_view op, const ConstValue& left, const ConstValue& right) {
    if ((op != "&&" && op != "||") || left.kind != ConstValue::Kind::kBool || right.kind != ConstValue::Kind::kBool) {
        return std::nullopt;
    }
    return MakeConstValue(op == "&&" ? (left.bool_value && right.bool_value) : (left.bool_value || right.bool_value));
}

std::optional<ConstValue> EvaluateConstFloatBinary(std::string_view op, double lhs, double rhs) {
    if (op == "/" && rhs == 0.0) {
        return std::nullopt;
    }
    if (op == "+") {
        return MakeConstValue(lhs + rhs);
    }
    if (op == "-") {
        return MakeConstValue(lhs - rhs);
    }
    if (op == "*") {
        return MakeConstValue(lhs * rhs);
    }
    if (op == "/") {
        return MakeConstValue(lhs / rhs);
    }
    return std::nullopt;
}

}  // namespace

std::string RenderConstValue(const ConstValue& value) {
    switch (value.kind) {
        case ConstValue::Kind::kBool:
            return value.bool_value ? "true" : "false";
        case ConstValue::Kind::kInteger:
            return std::to_string(value.integer_value);
        case ConstValue::Kind::kFloat: {
            std::ostringstream stream;
            stream << value.float_value;
            return stream.str();
        }
        case ConstValue::Kind::kString:
            return value.text;
        case ConstValue::Kind::kNil:
            return "nil";
        case ConstValue::Kind::kAggregate: {
            std::ostringstream stream;
            stream << '{';
            for (std::size_t index = 0; index < value.elements.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                if (index < value.field_names.size() && !value.field_names[index].empty()) {
                    stream << value.field_names[index] << ": ";
                }
                stream << RenderConstValue(value.elements[index]);
            }
            stream << '}';
            return stream.str();
        }
    }

    return "?";
}

ConstValue MakeConstValue(bool value) {
    ConstValue result;
    result.kind = ConstValue::Kind::kBool;
    result.bool_value = value;
    result.text = RenderConstValue(result);
    return result;
}

ConstValue MakeConstValue(std::int64_t value) {
    ConstValue result;
    result.kind = ConstValue::Kind::kInteger;
    result.integer_value = value;
    result.text = RenderConstValue(result);
    return result;
}

ConstValue MakeConstValue(double value) {
    ConstValue result;
    result.kind = ConstValue::Kind::kFloat;
    result.float_value = value;
    result.text = RenderConstValue(result);
    return result;
}

std::optional<ConstValue> ParseLiteralConstValue(const ast::Expr& expr) {
    if (expr.text == "true") {
        return MakeConstValue(true);
    }
    if (expr.text == "false") {
        return MakeConstValue(false);
    }
    if (expr.text == "nil") {
        ConstValue value;
        value.kind = ConstValue::Kind::kNil;
        value.text = "nil";
        return value;
    }
    if (!expr.text.empty() && expr.text.front() == '"') {
        ConstValue value;
        value.kind = ConstValue::Kind::kString;
        value.text = expr.text;
        return value;
    }
    if (expr.float_literal_value.has_value()) {
        return MakeConstValue(*expr.float_literal_value);
    }
    if (expr.integer_literal_value.has_value()) {
        return MakeConstValue(*expr.integer_literal_value);
    }
    return std::nullopt;
}

std::optional<ConstValue> EvaluateConstUnaryOp(std::string_view op, ConstValue value) {
    if (op == "+") {
        return value.kind == ConstValue::Kind::kInteger || value.kind == ConstValue::Kind::kFloat ? std::optional<ConstValue>(value)
                                                                                                    : std::nullopt;
    }
    if (op == "-") {
        if (value.kind == ConstValue::Kind::kInteger) {
            const auto negated = CheckedNegateInt64(value.integer_value);
            return negated.has_value() ? std::optional<ConstValue>(MakeConstValue(*negated)) : std::nullopt;
        }
        if (value.kind == ConstValue::Kind::kFloat) {
            return MakeConstValue(-value.float_value);
        }
        return std::nullopt;
    }
    if (op == "!") {
        return value.kind == ConstValue::Kind::kBool ? std::optional<ConstValue>(MakeConstValue(!value.bool_value)) : std::nullopt;
    }
    return std::nullopt;
}

std::optional<ConstValue> EvaluateConstBinaryOp(std::string_view op, const ConstValue& left, const ConstValue& right) {
    if (const auto logical = EvaluateConstLogicalBinary(op, left, right); logical.has_value()) {
        return logical;
    }
    if (const auto comparison = EvaluateConstComparison(op, left, right); comparison.has_value()) {
        return comparison;
    }
    if (left.kind == ConstValue::Kind::kFloat || right.kind == ConstValue::Kind::kFloat) {
        const double lhs = left.kind == ConstValue::Kind::kFloat ? left.float_value : left.integer_value;
        const double rhs = right.kind == ConstValue::Kind::kFloat ? right.float_value : right.integer_value;
        return EvaluateConstFloatBinary(op, lhs, rhs);
    }
    if (left.kind != ConstValue::Kind::kInteger || right.kind != ConstValue::Kind::kInteger) {
        return std::nullopt;
    }
    const CheckedIntBinaryOp evaluate = FindConstIntegerBinaryOp(op);
    if (evaluate == nullptr) {
        return std::nullopt;
    }
    const auto value = evaluate(left.integer_value, right.integer_value);
    return value.has_value() ? std::optional<ConstValue>(MakeConstValue(*value)) : std::nullopt;
}

std::optional<ConstValue> ParseGlobalConstValue(const GlobalSummary& global, std::string_view name) {
    auto it = std::find(global.names.begin(), global.names.end(), name);
    if (it == global.names.end()) {
        return std::nullopt;
    }
    const std::size_t index = static_cast<std::size_t>(std::distance(global.names.begin(), it));
    if (index >= global.constant_values.size() || !global.constant_values[index].has_value()) {
        return std::nullopt;
    }
    return global.constant_values[index];
}

}  // namespace mc::sema