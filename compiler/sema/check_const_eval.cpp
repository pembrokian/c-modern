#include "compiler/sema/check_internal.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_set>

#include "compiler/sema/const_eval.h"
#include "compiler/sema/module_resolver.h"
#include "compiler/sema/type_predicates.h"
#include "compiler/sema/type_utils.h"

namespace mc::sema {

using mc::ast::Decl;
using mc::ast::Expr;

std::optional<ConstValue> ConstExprEvaluator::EvaluateTopLevelConst(std::string_view name,
                                                                    bool report_errors,
                                                                    std::unordered_set<std::string>& active_names) {
    const std::string key(name);
    if (!active_names.insert(key).second) {
        if (report_errors) {
            context_.report(context_.source_span, "compile-time constant cycle detected for " + key);
        }
        return std::nullopt;
    }

    const auto cached = context_.const_eval_cache.find(key);
    if (cached != context_.const_eval_cache.end()) {
        active_names.erase(key);
        return cached->second;
    }

    const auto binding = context_.find_top_level_binding_decl(name, Decl::Kind::kConst);
    if (!binding.has_value() || binding->first == nullptr) {
        active_names.erase(key);
        return std::nullopt;
    }

    const auto value = EvaluateConstExpr(*binding->first->values[binding->second], report_errors, active_names);
    if (value.has_value()) {
        context_.const_eval_cache[key] = *value;
    }
    active_names.erase(key);
    return value;
}

std::optional<ConstValue> ConstExprEvaluator::EvaluateConstBinaryExpr(const Expr& expr,
                                                                      bool report_errors,
                                                                      std::unordered_set<std::string>& active_names) {
    if (expr.left == nullptr || expr.right == nullptr) {
        return std::nullopt;
    }
    if (expr.text == "&&" || expr.text == "||") {
        const auto left = EvaluateConstExpr(*expr.left, report_errors, active_names);
        if (!left.has_value() || left->kind != ConstValue::Kind::kBool) {
            return std::nullopt;
        }
        if (expr.text == "&&" && !left->bool_value) {
            return MakeConstValue(false);
        }
        if (expr.text == "||" && left->bool_value) {
            return MakeConstValue(true);
        }
        const auto right = EvaluateConstExpr(*expr.right, report_errors, active_names);
        if (!right.has_value()) {
            return std::nullopt;
        }
        return EvaluateConstBinaryOp(expr.text, *left, *right);
    }

    const auto left = EvaluateConstExpr(*expr.left, report_errors, active_names);
    const auto right = EvaluateConstExpr(*expr.right, report_errors, active_names);
    if (!left.has_value() || !right.has_value()) {
        return std::nullopt;
    }
    return EvaluateConstBinaryOp(expr.text, *left, *right);
}

std::optional<ConstExprEvaluator::EnumConstDesignator> ConstExprEvaluator::ResolveEnumConstDesignator(const Expr& expr,
                                                                                                       bool report_errors) {
    const Expr* type_expr = nullptr;
    std::string enum_name;
    std::string variant_name;

    if (expr.kind == Expr::Kind::kQualifiedName) {
        type_expr = &expr;
        enum_name = expr.text;
        variant_name = expr.secondary_text;
    } else if (expr.kind == Expr::Kind::kField && expr.left != nullptr &&
               (expr.left->kind == Expr::Kind::kName || expr.left->kind == Expr::Kind::kQualifiedName)) {
        type_expr = expr.left.get();
        enum_name = context_.combine_qualified_name(*expr.left);
        variant_name = expr.text;
    } else {
        return std::nullopt;
    }

    const TypeDeclSummary* type_decl = context_.lookup_local_type_decl(enum_name);
    if (type_decl == nullptr || type_decl->kind != Decl::Kind::kEnum || type_expr == nullptr) {
        return std::nullopt;
    }

    const std::size_t expected = type_decl->type_params.size();
    const std::size_t actual = type_expr->type_args.size();
    if (expected != actual) {
        if (report_errors && actual != 0) {
            context_.report(expr.span,
                            "generic type " + enum_name + " expects " + std::to_string(expected) +
                                " type arguments but got " + std::to_string(actual));
        }
        if (expected != 0 || actual != 0) {
            return std::nullopt;
        }
    }

    Type enum_type = NamedType(type_decl->name);
    enum_type.subtypes.reserve(actual);
    for (const auto& type_arg : type_expr->type_args) {
        const auto current_type_params = context_.current_type_params();
        context_.validate_type_expr(type_arg.get(), current_type_params, type_arg->span);
        enum_type.subtypes.push_back(context_.semantic_type_from_ast(type_arg.get(), current_type_params));
    }

    for (std::size_t index = 0; index < type_decl->variants.size(); ++index) {
        if (type_decl->variants[index].name != variant_name) {
            continue;
        }
        return EnumConstDesignator {
            .enum_type = enum_type,
            .variant_name = variant_name,
            .payload_fields = InstantiateVariantSummary(type_decl->variants[index], *type_decl, enum_type).payload_fields,
            .variant_index = index,
        };
    }

    return std::nullopt;
}

std::optional<ConstValue> ConstExprEvaluator::EvaluateEnumConst(const Expr& designator,
                                                                const std::vector<std::unique_ptr<Expr>>* payload_args,
                                                                bool report_errors,
                                                                std::unordered_set<std::string>& active_names) {
    const auto resolved = ResolveEnumConstDesignator(designator, report_errors);
    if (!resolved.has_value()) {
        return std::nullopt;
    }

    const std::size_t arg_count = payload_args != nullptr ? payload_args->size() : 0;
    if (resolved->payload_fields.size() != arg_count) {
        if (report_errors) {
            context_.report(designator.span,
                            "enum constant " + FormatType(resolved->enum_type) + '.' + resolved->variant_name +
                                " expects " + std::to_string(resolved->payload_fields.size()) +
                                " payload values but got " + std::to_string(arg_count));
        }
        return std::nullopt;
    }

    std::vector<std::string> field_names;
    std::vector<ConstValue> elements;
    field_names.reserve(arg_count);
    elements.reserve(arg_count);
    for (std::size_t index = 0; index < arg_count; ++index) {
        const auto field_value = EvaluateConstExpr(*(*payload_args)[index], report_errors, active_names);
        if (!field_value.has_value()) {
            return std::nullopt;
        }
        field_names.push_back(resolved->payload_fields[index].first);
        elements.push_back(*field_value);
    }

    return MakeEnumConstValue(resolved->enum_type,
                              resolved->variant_name,
                              static_cast<std::int64_t>(resolved->variant_index),
                              std::move(field_names),
                              std::move(elements));
}

std::optional<ConstValue> ConstExprEvaluator::EvaluateConstExpr(const Expr& expr,
                                                                bool report_errors,
                                                                std::unordered_set<std::string>& active_names) {
    switch (expr.kind) {
        case Expr::Kind::kName:
            if (expr.type_args.empty()) {
                if (const auto* function = FindFunctionSignature(context_.module, expr.text);
                    function != nullptr && function->type_params.empty()) {
                    return MakeProcedureConstValue(expr.text);
                }
            }
            return EvaluateTopLevelConst(expr.text, report_errors, active_names);
        case Expr::Kind::kQualifiedName:
            if (const Module* imported_module = context_.find_imported_module(expr.text); imported_module != nullptr) {
                if (expr.type_args.empty()) {
                    if (const auto* function = FindFunctionSignature(*imported_module, expr.secondary_text);
                        function != nullptr && function->type_params.empty()) {
                        return MakeProcedureConstValue(QualifyImportedName(expr.text, expr.secondary_text));
                    }
                }
                if (const auto* global = FindGlobalSummary(*imported_module, expr.secondary_text);
                    global != nullptr && global->is_const) {
                    return ParseGlobalConstValue(*global, expr.secondary_text);
                }
            }
            return EvaluateEnumConst(expr, nullptr, report_errors, active_names);
        case Expr::Kind::kLiteral:
            return ParseLiteralConstValue(expr);
        case Expr::Kind::kUnary: {
            if (expr.left == nullptr) {
                return std::nullopt;
            }
            const auto operand = EvaluateConstExpr(*expr.left, report_errors, active_names);
            if (!operand.has_value()) {
                return std::nullopt;
            }
            return EvaluateConstUnaryOp(expr.text, *operand);
        }
        case Expr::Kind::kBinary:
            return EvaluateConstBinaryExpr(expr, report_errors, active_names);
        case Expr::Kind::kCall: {
            if (expr.type_target == nullptr && expr.left != nullptr) {
                if (const auto enum_const = EvaluateEnumConst(*expr.left, &expr.args, report_errors, active_names);
                    enum_const.has_value()) {
                    return enum_const;
                }
            }
            if (expr.type_target == nullptr || expr.args.size() != 1) {
                return std::nullopt;
            }
            const auto operand = EvaluateConstExpr(*expr.args.front(), report_errors, active_names);
            if (!operand.has_value()) {
                return std::nullopt;
            }
            const auto current_type_params = context_.current_type_params();
            context_.validate_type_expr(expr.type_target.get(), current_type_params, expr.type_target->span);
            const Type target_type = context_.semantic_type_from_ast(expr.type_target.get(), current_type_params);
            if (expr.bare_type_target_syntax) {
                const Type stripped_target = CanonicalizeBuiltinType(
                    StripType(target_type, context_.module, TypeStripMode::kAliasesAndDistinct));
                if (!mc::sema::IsIntegerType(stripped_target) || operand->kind != ConstValue::Kind::kInteger) {
                    return std::nullopt;
                }
            }
            return context_.convert_const_value(target_type, *operand);
        }
        case Expr::Kind::kField:
            return EvaluateEnumConst(expr, nullptr, report_errors, active_names);
        case Expr::Kind::kEmptyCollection: {
            Type collection_type = context_.resolve_aggregate_init_type(expr);
            if (IsUnknown(collection_type)) {
                return std::nullopt;
            }

            const Type resolved_collection = StripType(collection_type, context_.module, TypeStripMode::kAliasesOnly);
            ConstValue result;
            result.kind = ConstValue::Kind::kAggregate;
            if (resolved_collection.kind == Type::Kind::kArray) {
                const auto length = mc::support::ParseArrayLength(resolved_collection.metadata);
                if (!length.has_value() || *length != 0) {
                    return std::nullopt;
                }
                result.text = RenderConstValue(result);
                return result;
            }

            if (resolved_collection.kind != Type::Kind::kNamed || resolved_collection.name != "Slice") {
                return std::nullopt;
            }

            ConstValue nil_value;
            nil_value.kind = ConstValue::Kind::kNil;
            nil_value.text = "nil";
            result.field_names = {"ptr", "len"};
            result.elements.push_back(std::move(nil_value));
            result.elements.push_back(MakeConstValue(static_cast<std::int64_t>(0)));
            result.text = RenderConstValue(result);
            return result;
        }
        case Expr::Kind::kAggregateInit: {
            Type aggregate_type = context_.resolve_aggregate_init_type(expr);
            if (IsUnknown(aggregate_type)) {
                return std::nullopt;
            }

            context_.apply_nested_aggregate_type_hints(const_cast<Expr&>(expr), aggregate_type);

            const Type resolved_aggregate = StripType(aggregate_type, context_.module, TypeStripMode::kAliasesOnly);
            if (resolved_aggregate.kind == Type::Kind::kArray) {
                if (resolved_aggregate.subtypes.empty()) {
                    return std::nullopt;
                }
                const auto length = mc::support::ParseArrayLength(resolved_aggregate.metadata);
                if (!length.has_value() || *length != expr.field_inits.size()) {
                    return std::nullopt;
                }

                ConstValue result;
                result.kind = ConstValue::Kind::kAggregate;
                result.elements.reserve(expr.field_inits.size());
                for (const auto& field_init : expr.field_inits) {
                    if (field_init.has_name || field_init.value == nullptr) {
                        return std::nullopt;
                    }
                    const auto field_value = EvaluateConstExpr(*field_init.value, report_errors, active_names);
                    if (!field_value.has_value()) {
                        return std::nullopt;
                    }
                    result.elements.push_back(*field_value);
                }
                result.text = RenderConstValue(result);
                return result;
            }

            const TypeDeclSummary* type_decl = context_.lookup_struct_type(resolved_aggregate);
            const auto builtin_fields = BuiltinAggregateFields(resolved_aggregate);
            const auto instantiated_fields =
                type_decl != nullptr ? InstantiateTypeDeclFields(*type_decl, resolved_aggregate)
                                     : std::vector<std::pair<std::string, Type>> {};
            const auto& field_source = type_decl != nullptr ? instantiated_fields : builtin_fields;
            if (type_decl == nullptr && field_source.empty()) {
                return std::nullopt;
            }

            std::vector<std::optional<ConstValue>> ordered_values(field_source.size());
            std::vector<std::string> ordered_names(field_source.size());
            for (std::size_t index = 0; index < field_source.size(); ++index) {
                ordered_names[index] = field_source[index].first;
            }

            for (std::size_t index = 0; index < expr.field_inits.size(); ++index) {
                const auto& field_init = expr.field_inits[index];
                if (field_init.value == nullptr) {
                    return std::nullopt;
                }
                const auto field_value = EvaluateConstExpr(*field_init.value, report_errors, active_names);
                if (!field_value.has_value()) {
                    return std::nullopt;
                }

                std::size_t target_index = index;
                if (field_init.has_name) {
                    const auto found = std::find_if(field_source.begin(), field_source.end(), [&](const auto& field) {
                        return field.first == field_init.name;
                    });
                    if (found == field_source.end()) {
                        return std::nullopt;
                    }
                    target_index = static_cast<std::size_t>(std::distance(field_source.begin(), found));
                } else if (target_index >= field_source.size()) {
                    return std::nullopt;
                }

                if (target_index >= ordered_values.size() || ordered_values[target_index].has_value()) {
                    return std::nullopt;
                }
                ordered_values[target_index] = *field_value;
            }

            ConstValue result;
            result.kind = ConstValue::Kind::kAggregate;
            result.field_names = std::move(ordered_names);
            result.elements.reserve(ordered_values.size());
            for (const auto& field_value : ordered_values) {
                if (!field_value.has_value()) {
                    return std::nullopt;
                }
                result.elements.push_back(*field_value);
            }
            result.text = RenderConstValue(result);
            return result;
        }
        case Expr::Kind::kRecordUpdate: {
            if (expr.left == nullptr) {
                return std::nullopt;
            }

            const auto base_value = EvaluateConstExpr(*expr.left, report_errors, active_names);
            if (!base_value.has_value() || base_value->kind != ConstValue::Kind::kAggregate || base_value->field_names.empty()) {
                return std::nullopt;
            }

            std::vector<std::optional<ConstValue>> ordered_values(base_value->elements.size());
            for (std::size_t index = 0; index < base_value->elements.size(); ++index) {
                ordered_values[index] = base_value->elements[index];
            }

            std::unordered_set<std::string> updated_fields;
            for (const auto& field_init : expr.field_inits) {
                if (!field_init.has_name || field_init.value == nullptr) {
                    return std::nullopt;
                }
                if (!updated_fields.insert(field_init.name).second) {
                    return std::nullopt;
                }

                const auto found = std::find(base_value->field_names.begin(), base_value->field_names.end(), field_init.name);
                if (found == base_value->field_names.end()) {
                    return std::nullopt;
                }

                const auto field_value = EvaluateConstExpr(*field_init.value, report_errors, active_names);
                if (!field_value.has_value()) {
                    return std::nullopt;
                }

                const std::size_t target_index = static_cast<std::size_t>(std::distance(base_value->field_names.begin(), found));
                ordered_values[target_index] = *field_value;
            }

            ConstValue result;
            result.kind = ConstValue::Kind::kAggregate;
            result.field_names = base_value->field_names;
            result.elements.reserve(ordered_values.size());
            for (const auto& field_value : ordered_values) {
                if (!field_value.has_value()) {
                    return std::nullopt;
                }
                result.elements.push_back(*field_value);
            }
            result.text = RenderConstValue(result);
            return result;
        }
        case Expr::Kind::kParen:
            return expr.left != nullptr ? EvaluateConstExpr(*expr.left, report_errors, active_names) : std::nullopt;
        default:
            return std::nullopt;
    }
}

std::optional<std::size_t> ConstExprEvaluator::EvaluateArrayLength(const Expr* expr, bool report_errors) {
    if (expr == nullptr) {
        return std::nullopt;
    }
    std::unordered_set<std::string> active_names;
    const auto value = EvaluateConstExpr(*expr, report_errors, active_names);
    if (!value.has_value() || value->kind != ConstValue::Kind::kInteger || value->integer_value < 0) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(value->integer_value);
}

}  // namespace mc::sema