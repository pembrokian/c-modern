#include "compiler/sema/check.h"
#include "compiler/sema/const_eval.h"
#include "compiler/sema/layout.h"
#include "compiler/sema/module_resolver.h"
#include "compiler/sema/type_utils.h"
#include "compiler/sema/type_predicates.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "compiler/lex/lexer.h"
#include "compiler/parse/parser.h"
#include "compiler/support/module_paths.h"

namespace mc::sema {

namespace {

using mc::ast::Decl;
using mc::ast::Expr;
using mc::ast::Stmt;
using mc::support::DiagnosticSeverity;

struct ValueBinding {
    Type type;
    bool is_mutable = true;
};

struct ExprTypeValue {
    Type type;
    mc::support::SourceSpan span;
};

enum class VisitState {
    kVisiting,
    kDone,
};

using ImportedModules = std::unordered_map<std::string, Module>;

struct ResolvedImportPath {
    std::filesystem::path path;
    std::optional<std::string> package_identity;
};

std::string JoinPaths(const std::vector<std::filesystem::path>& paths) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < paths.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << paths[index].generic_string();
    }
    return stream.str();
}

bool IsInternalModulePath(const std::filesystem::path& path) {
    return mc::support::IsInternalModulePath(path);
}

bool HasPrivateAttribute(const std::vector<ast::Attribute>& attributes) {
    return std::any_of(attributes.begin(), attributes.end(), [](const ast::Attribute& attribute) {
        return attribute.name == "private";
    });
}

std::string DeclDisplayName(const Decl& decl) {
    switch (decl.kind) {
        case Decl::Kind::kFunc:
        case Decl::Kind::kExternFunc:
        case Decl::Kind::kStruct:
        case Decl::Kind::kEnum:
        case Decl::Kind::kDistinct:
        case Decl::Kind::kTypeAlias:
            return decl.name;
        case Decl::Kind::kConst:
        case Decl::Kind::kVar:
            return !decl.pattern.names.empty() ? decl.pattern.names.front() : std::string("<binding>");
    }
    return "<decl>";
}

std::optional<ResolvedImportPath> ResolveImportPathForCheck(const std::filesystem::path& importer_path,
                                                            std::string_view module_name,
                                                            const std::vector<std::filesystem::path>& import_roots,
                                                            const std::optional<std::string>& importer_package_identity,
                                                            const std::function<std::optional<std::string>(const std::filesystem::path&)>&
                                                                package_identity_for_source,
                                                            support::DiagnosticSink& diagnostics,
                                                            const mc::support::SourceSpan& span) {
    std::vector<std::filesystem::path> search_roots;
    search_roots.reserve(import_roots.size() + 1);
    search_roots.push_back(importer_path.parent_path());
    for (const auto& root : import_roots) {
        search_roots.push_back(root);
    }

    std::vector<std::filesystem::path> matches;
    std::unordered_set<std::string> seen_matches;
    std::optional<std::string> rejected_internal_reason;
    std::optional<std::string> matched_package_identity;

    for (const auto& root : search_roots) {
        const std::filesystem::path candidate = std::filesystem::absolute(root / (std::string(module_name) + ".mc")).lexically_normal();
        if (!std::filesystem::exists(candidate)) {
            continue;
        }
        if (!seen_matches.insert(candidate.generic_string()).second) {
            continue;
        }

        if (IsInternalModulePath(candidate)) {
            if (!importer_package_identity.has_value()) {
                rejected_internal_reason =
                    "direct source mode does not support importing internal module: " + std::string(module_name);
                continue;
            }

            const auto candidate_package_identity = package_identity_for_source
                ? package_identity_for_source(candidate)
                : std::nullopt;
            if (!candidate_package_identity.has_value()) {
                rejected_internal_reason = "unable to determine package identity for internal module: " + candidate.generic_string();
                continue;
            }
            if (*candidate_package_identity != *importer_package_identity) {
                rejected_internal_reason = "module " + std::string(module_name) + " is internal to package '" +
                    *candidate_package_identity + "' and cannot be imported from package '" + *importer_package_identity + "'";
                continue;
            }
            matched_package_identity = candidate_package_identity;
        }

        matches.push_back(candidate);
    }

    if (matches.size() == 1) {
        return ResolvedImportPath {
            .path = matches.front(),
            .package_identity = matched_package_identity,
        };
    }

    if (matches.empty()) {
        diagnostics.Report({
            .file_path = importer_path,
            .span = span,
            .severity = DiagnosticSeverity::kError,
            .message = rejected_internal_reason.has_value()
                ? *rejected_internal_reason
                : "unable to resolve import module: " + std::string(module_name),
        });
        return std::nullopt;
    }

    diagnostics.Report({
        .file_path = importer_path,
        .span = span,
        .severity = DiagnosticSeverity::kError,
        .message = "ambiguous import module '" + std::string(module_name) + "' matched multiple roots: " + JoinPaths(matches),
    });
    return std::nullopt;
}

std::size_t ProcedureParamCount(const Type& type) {
    if (type.kind != Type::Kind::kProcedure || type.metadata.empty()) {
        return 0;
    }
    return mc::support::ParseArrayLength(type.metadata).value_or(0);
}

std::string CombineQualifiedName(const Expr& expr) {
    if (expr.kind == Expr::Kind::kQualifiedName) {
        return expr.text + "." + expr.secondary_text;
    }
    return expr.text;
}

std::optional<Expr> FlattenQualifiedBaseExpr(const Expr& expr) {
    if (expr.kind != Expr::Kind::kQualifiedName || expr.text.empty()) {
        return std::nullopt;
    }

    Expr base_expr;
    base_expr.span = expr.span;
    const std::size_t last_dot = expr.text.rfind('.');
    if (last_dot == std::string::npos) {
        base_expr.kind = Expr::Kind::kName;
        base_expr.text = expr.text;
        return base_expr;
    }

    base_expr.kind = Expr::Kind::kQualifiedName;
    base_expr.text = expr.text.substr(0, last_dot);
    base_expr.secondary_text = expr.text.substr(last_dot + 1);
    return base_expr;
}

bool IsNumericType(const Type& type, const Module& module) {
    const Type stripped = CanonicalizeBuiltinType(StripType(type, module, TypeStripMode::kAliasesOnly));
    return mc::sema::IsNumericType(stripped);
}

bool IsIntegerLikeType(const Type& type, const Module& module) {
    const Type stripped = CanonicalizeBuiltinType(StripType(type, module, TypeStripMode::kAliasesOnly));
    return mc::sema::IsIntegerLikeType(stripped);
}

bool IsBoolType(const Type& type, const Module& module) {
    const Type stripped = CanonicalizeBuiltinType(StripType(type, module, TypeStripMode::kAliasesOnly));
    return mc::sema::IsBoolType(stripped);
}

bool IsPointerLikeType(const Type& type, const Module& module) {
    return mc::sema::IsPointerLikeType(StripType(type, module, TypeStripMode::kAliasesOnly));
}

bool IsUintPtrConvertibleType(const Type& type, const Module& module) {
    return mc::sema::IsUintPtrConvertibleType(StripType(type, module, TypeStripMode::kAliasesOnly));
}

bool IsUintPtrType(const Type& type, const Module& module) {
    const Type stripped = StripType(type, module, TypeStripMode::kAliasesOnly);
    return mc::sema::IsUintPtrType(stripped);
}

std::string_view ToString(Decl::Kind kind) {
    switch (kind) {
        case Decl::Kind::kFunc:
            return "func";
        case Decl::Kind::kExternFunc:
            return "extern_func";
        case Decl::Kind::kStruct:
            return "struct";
        case Decl::Kind::kEnum:
            return "enum";
        case Decl::Kind::kDistinct:
            return "distinct";
        case Decl::Kind::kTypeAlias:
            return "alias";
        case Decl::Kind::kConst:
            return "const";
        case Decl::Kind::kVar:
            return "var";
    }

    return "decl";
}

void WriteIndent(std::ostringstream& stream, int indent) {
    for (int count = 0; count < indent; ++count) {
        stream << "  ";
    }
}

void WriteLine(std::ostringstream& stream, int indent, const std::string& text) {
    WriteIndent(stream, indent);
    stream << text << '\n';
}

class Checker {
  public:
    Checker(const ast::SourceFile& source_file,
            const std::filesystem::path& file_path,
                        std::optional<std::string> current_package_identity,
            const ImportedModules* imported_modules,
            support::DiagnosticSink& diagnostics,
            std::vector<std::filesystem::path> module_part_paths = {},
            std::size_t module_part_line_stride = 0)
                : source_file_(source_file),
                    file_path_(file_path),
                    current_package_identity_(std::move(current_package_identity)),
                    imported_modules_(imported_modules),
                    diagnostics_(diagnostics),
                    module_part_paths_(std::move(module_part_paths)),
                    module_part_line_stride_(module_part_line_stride) {}

    CheckResult Run() {
        auto module = std::make_unique<Module>();
        module_ = module.get();
                module_->module_kind = source_file_.module_kind;
                if (current_package_identity_.has_value()) {
                        module_->package_identity = *current_package_identity_;
                }

        for (const auto& import_decl : source_file_.imports) {
            module_->imports.push_back(import_decl.module_name);
        }

        // Checker phase ordering matters:
        //   1. SeedImportedSymbols  — must run first; populates value_symbols_
        //      and type_symbols_ from imported modules so that CollectTopLevelDecls
        //      can detect conflicts with exported names.
        //   2. CollectTopLevelDecls — populates the module's top-level summaries.
        //      Type and function lookup before this point is incomplete.
        //   3. BuildCheckerLookupMaps — refreshes checker-private O(1) lookups
        //      over the collected top-level summaries.
        //   4. ValidateTypeDecls   — validates struct/enum/distinct/alias fields
        //      and triggers ComputeTypeLayouts.
        //   5. ValidateVisibleDecls — validates @private and public surface use.
        //   6. ValidateGlobals     — checks global const/var declarations.
        //   7. ValidateFunctions   — type-checks function bodies using the fully
        //      populated module summary.
        SeedImportedSymbols();
        CollectTopLevelDecls();
        BuildCheckerLookupMaps();
        ValidateDeclVisibility();
        ValidateTypeDecls();
        ValidateVisibleDeclTypes();
        ValidateGlobals();
        ValidateFunctionSignatures();
        ValidateFunctions();
        BuildModuleLookupMaps(*module);

        return {
            .module = std::move(module),
            .ok = !diagnostics_.HasErrors(),
        };
    }

  private:
    void Report(const mc::support::SourceSpan& span, const std::string& message) {
        std::filesystem::path path = file_path_;
        support::SourceSpan resolved = span;
        if (module_part_line_stride_ > 0 && !module_part_paths_.empty()) {
            const std::size_t part = span.begin.line / module_part_line_stride_;
            if (part < module_part_paths_.size()) {
                path = module_part_paths_[part];
                resolved.begin.line = span.begin.line - part * module_part_line_stride_;
                resolved.end.line = span.end.line - part * module_part_line_stride_;
            }
        }
        diagnostics_.Report({
            .file_path = path,
            .span = resolved,
            .severity = DiagnosticSeverity::kError,
            .message = message,
        });
    }

    struct TopLevelBindingDeclRef {
        const Decl* decl = nullptr;
        std::size_t index = 0;
    };

    std::optional<TopLevelBindingDeclRef> FindTopLevelBindingDecl(std::string_view name, Decl::Kind kind) const {
        for (const auto& decl : source_file_.decls) {
            if (decl.kind != kind) {
                continue;
            }
            for (std::size_t index = 0; index < decl.pattern.names.size(); ++index) {
                if (decl.pattern.names[index] != name) {
                    continue;
                }
                if (index >= decl.values.size()) {
                    return std::nullopt;
                }
                return TopLevelBindingDeclRef {
                    .decl = &decl,
                    .index = index,
                };
            }
        }
        return std::nullopt;
    }

    const Decl* FindTopLevelGlobalDecl(std::string_view name, Decl::Kind kind) const {
        const auto binding = FindTopLevelBindingDecl(name, kind);
        if (!binding.has_value() || binding->decl == nullptr) {
            return nullptr;
        }
        return binding->decl;
    }

    Type SemanticTypeFromAst(const ast::TypeExpr* type_expr, const std::vector<std::string>& type_params) {
        if (type_expr == nullptr) {
            return UnknownType();
        }

        switch (type_expr->kind) {
            case ast::TypeExpr::Kind::kNamed: {
                Type type = NamedOrBuiltinType(type_expr->name);
                type.subtypes.reserve(type_expr->type_args.size());
                for (const auto& type_arg : type_expr->type_args) {
                    type.subtypes.push_back(SemanticTypeFromAst(type_arg.get(), type_params));
                }
                return type;
            }
            case ast::TypeExpr::Kind::kPointer:
                return PointerType(SemanticTypeFromAst(type_expr->inner.get(), type_params));
            case ast::TypeExpr::Kind::kConst:
                return ConstType(SemanticTypeFromAst(type_expr->inner.get(), type_params));
            case ast::TypeExpr::Kind::kArray: {
                Type type = TypeFromAst(type_expr);
                if (type_expr->inner != nullptr) {
                    type.subtypes.clear();
                    type.subtypes.push_back(SemanticTypeFromAst(type_expr->inner.get(), type_params));
                }
                const auto length = EvaluateArrayLength(type_expr->length_expr.get(), false);
                if (length.has_value()) {
                    type.metadata = std::to_string(*length);
                }
                return type;
            }
            case ast::TypeExpr::Kind::kParen:
                return SemanticTypeFromAst(type_expr->inner.get(), type_params);
            case ast::TypeExpr::Kind::kProcedure: {
                std::vector<Type> params;
                params.reserve(type_expr->params.size());
                for (const auto& param : type_expr->params) {
                    params.push_back(SemanticTypeFromAst(param.get(), type_params));
                }
                std::vector<Type> returns;
                returns.reserve(type_expr->returns.size());
                for (const auto& ret : type_expr->returns) {
                    returns.push_back(SemanticTypeFromAst(ret.get(), type_params));
                }
                return ProcedureType(std::move(params), std::move(returns));
            }
        }

        return UnknownType();
    }

    Type NormalizeAggregateExpectedType(Type expected) const {
        expected = CanonicalizeBuiltinType(std::move(expected));
        while (expected.kind == Type::Kind::kConst && !expected.subtypes.empty()) {
            expected = CanonicalizeBuiltinType(expected.subtypes.front());
        }
        return expected;
    }

    Type ResolveAggregateInitType(const Expr& expr) {
        if (expr.type_target != nullptr) {
            ValidateTypeExpr(expr.type_target.get(), CurrentTypeParams(), expr.type_target->span);
            return SemanticTypeFromAst(expr.type_target.get(), CurrentTypeParams());
        }
        if (expr.left != nullptr) {
            Type aggregate_type = NamedOrBuiltinType(CombineQualifiedName(*expr.left));
            for (const auto& type_arg : expr.left->type_args) {
                ValidateTypeExpr(type_arg.get(), CurrentTypeParams(), type_arg->span);
                aggregate_type.subtypes.push_back(SemanticTypeFromAst(type_arg.get(), CurrentTypeParams()));
            }
            return aggregate_type;
        }
        return UnknownType();
    }

    std::vector<std::pair<std::string, Type>> ResolveAggregateFieldTypes(const Type& aggregate_type) const {
        const Type normalized = NormalizeAggregateExpectedType(aggregate_type);
        const Type resolved_aggregate = StripType(normalized, *module_, TypeStripMode::kAliasesOnly);
        if (resolved_aggregate.kind == Type::Kind::kArray) {
            return {};
        }

        const auto builtin_fields = BuiltinAggregateFields(resolved_aggregate);
        if (!builtin_fields.empty()) {
            return builtin_fields;
        }

        if (const TypeDeclSummary* type_decl = LookupStructType(resolved_aggregate); type_decl != nullptr) {
            return InstantiateTypeDeclFields(*type_decl, resolved_aggregate);
        }

        return {};
    }

    void ApplyNestedAggregateTypeHints(Expr& expr, const Type& aggregate_type) {
        const Type normalized = NormalizeAggregateExpectedType(aggregate_type);
        const Type resolved_aggregate = StripType(normalized, *module_, TypeStripMode::kAliasesOnly);
        if (resolved_aggregate.kind == Type::Kind::kArray) {
            if (resolved_aggregate.subtypes.empty()) {
                return;
            }
            for (auto& field_init : expr.field_inits) {
                if (field_init.value != nullptr) {
                    ApplyExpectedAggregateType(*field_init.value, resolved_aggregate.subtypes.front());
                }
            }
            return;
        }

        const auto field_source = ResolveAggregateFieldTypes(resolved_aggregate);
        if (field_source.empty()) {
            return;
        }

        for (std::size_t index = 0; index < expr.field_inits.size(); ++index) {
            auto& field_init = expr.field_inits[index];
            if (field_init.value == nullptr) {
                continue;
            }

            Type field_type = UnknownType();
            if (field_init.has_name) {
                const auto found = std::find_if(field_source.begin(), field_source.end(), [&](const auto& field) {
                    return field.first == field_init.name;
                });
                if (found != field_source.end()) {
                    field_type = found->second;
                }
            } else if (index < field_source.size()) {
                field_type = field_source[index].second;
            }

            if (!IsUnknown(field_type)) {
                ApplyExpectedAggregateType(*field_init.value, field_type);
            }
        }
    }

    void ApplyExpectedAggregateType(Expr& expr, const Type& expected_type) {
        if (expr.kind != Expr::Kind::kAggregateInit) {
            return;
        }

        const Type normalized = NormalizeAggregateExpectedType(expected_type);
        if (IsUnknown(normalized)) {
            return;
        }

        if (expr.type_target == nullptr && expr.left == nullptr) {
            expr.type_target = TypeToAst(normalized);
        }

        const Type aggregate_type = ResolveAggregateInitType(expr);
        if (!IsUnknown(aggregate_type)) {
            ApplyNestedAggregateTypeHints(expr, aggregate_type);
        }
    }

    std::optional<ConstValue> EvaluateTopLevelConst(std::string_view name, bool report_errors,
                                                    std::unordered_set<std::string>& active_names) {
        const std::string key(name);
        // active_names is scoped to a single top-level initializer walk so
        // recursive references diagnose cycles without polluting later const
        // evaluations for unrelated globals.
        if (!active_names.insert(key).second) {
            if (report_errors) {
                Report(source_file_.span, "compile-time constant cycle detected for " + key);
            }
            return std::nullopt;
        }

        const auto cached = const_eval_cache_.find(key);
        if (cached != const_eval_cache_.end()) {
            active_names.erase(key);
            return cached->second;
        }

        const auto binding = FindTopLevelBindingDecl(name, Decl::Kind::kConst);
        if (!binding.has_value() || binding->decl == nullptr) {
            active_names.erase(key);
            return std::nullopt;
        }

        const auto value = EvaluateConstExpr(*binding->decl->values[binding->index], report_errors, active_names);
        if (value.has_value()) {
            const_eval_cache_[key] = *value;
        }
        active_names.erase(key);
        return value;
    }

    std::optional<ConstValue> ConvertConstValue(Type target_type, const ConstValue& value) {
        const Type canonical_target = CanonicalizeBuiltinType(StripType(std::move(target_type), *module_, TypeStripMode::kAliasesAndDistinct));
        if (canonical_target.kind == Type::Kind::kBool) {
            return value.kind == ConstValue::Kind::kBool ? std::optional<ConstValue>(value) : std::nullopt;
        }
        if (canonical_target.kind == Type::Kind::kString) {
            return value.kind == ConstValue::Kind::kString ? std::optional<ConstValue>(value) : std::nullopt;
        }
        if (canonical_target.kind == Type::Kind::kPointer) {
            return value.kind == ConstValue::Kind::kNil ? std::optional<ConstValue>(value) : std::nullopt;
        }
        if (canonical_target.kind == Type::Kind::kNamed && IsFloatTypeName(canonical_target.name)) {
            if (value.kind == ConstValue::Kind::kFloat) {
                return value;
            }
            if (value.kind == ConstValue::Kind::kInteger) {
                return MakeConstValue(static_cast<double>(value.integer_value));
            }
            return std::nullopt;
        }
        if (canonical_target.kind == Type::Kind::kNamed && IsIntegerTypeName(canonical_target.name)) {
            if (value.kind == ConstValue::Kind::kInteger) {
                return value;
            }
            if (value.kind == ConstValue::Kind::kFloat) {
                if (!std::isfinite(value.float_value)) {
                    return std::nullopt;
                }
                const long double truncated = std::trunc(static_cast<long double>(value.float_value));
                const long double min_value = static_cast<long double>(std::numeric_limits<std::int64_t>::min());
                const long double max_exclusive = static_cast<long double>(std::numeric_limits<std::int64_t>::max()) + 1.0L;
                if (truncated < min_value || truncated >= max_exclusive) {
                    return std::nullopt;
                }
                return MakeConstValue(static_cast<std::int64_t>(truncated));
            }
            return std::nullopt;
        }
        if (canonical_target.kind == Type::Kind::kNamed) {
            const Type stripped_value_type = CanonicalizeBuiltinType(StripType(value.enum_type, *module_, TypeStripMode::kAliasesAndDistinct));
            if (value.kind == ConstValue::Kind::kEnum && stripped_value_type == canonical_target) {
                return value;
            }
        }
        return std::nullopt;
    }

    std::optional<ConstValue> EvaluateConstBinaryExpr(const Expr& expr,
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

    struct EnumConstDesignator {
        Type enum_type;
        std::string variant_name;
        std::vector<std::pair<std::string, Type>> payload_fields;
        std::size_t variant_index = 0;
    };

    std::optional<EnumConstDesignator> ResolveEnumConstDesignator(const Expr& expr, bool report_errors) {
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
            enum_name = CombineQualifiedName(*expr.left);
            variant_name = expr.text;
        } else {
            return std::nullopt;
        }

        const TypeDeclSummary* type_decl = LookupLocalTypeDecl(enum_name);
        if (type_decl == nullptr || type_decl->kind != Decl::Kind::kEnum || type_expr == nullptr) {
            return std::nullopt;
        }

        const std::size_t expected = type_decl->type_params.size();
        const std::size_t actual = type_expr->type_args.size();
        if (expected != actual) {
            if (report_errors && actual != 0) {
                Report(expr.span,
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
            ValidateTypeExpr(type_arg.get(), CurrentTypeParams(), type_arg->span);
            enum_type.subtypes.push_back(SemanticTypeFromAst(type_arg.get(), CurrentTypeParams()));
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

    std::optional<ConstValue> EvaluateEnumConst(const Expr& designator,
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
                Report(designator.span,
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

    bool IsConstExpr(const Expr& expr, bool report_errors, std::unordered_set<std::string>& active_names) {
        if (EvaluateConstExpr(expr, report_errors, active_names).has_value()) {
            return true;
        }

        switch (expr.kind) {
            case Expr::Kind::kAggregateInit:
                for (const auto& field_init : expr.field_inits) {
                    if (field_init.value == nullptr || !IsConstExpr(*field_init.value, report_errors, active_names)) {
                        return false;
                    }
                }
                return true;
            case Expr::Kind::kParen:
                return expr.left != nullptr && IsConstExpr(*expr.left, report_errors, active_names);
            default:
                return false;
        }
    }

    std::optional<ConstValue> EvaluateConstExpr(const Expr& expr, bool report_errors,
                                                std::unordered_set<std::string>& active_names) {
        switch (expr.kind) {
            case Expr::Kind::kName:
                return EvaluateTopLevelConst(expr.text, report_errors, active_names);
            case Expr::Kind::kQualifiedName:
                if (const Module* imported_module = FindImportedModule(expr.text); imported_module != nullptr) {
                    if (const auto* global = FindGlobalSummary(*imported_module, expr.secondary_text); global != nullptr) {
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
            case Expr::Kind::kBinary: {
                return EvaluateConstBinaryExpr(expr, report_errors, active_names);
            }
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
                ValidateTypeExpr(expr.type_target.get(), CurrentTypeParams(), expr.type_target->span);
                const Type target_type = SemanticTypeFromAst(expr.type_target.get(), CurrentTypeParams());
                if (expr.bare_type_target_syntax) {
                    const Type stripped_target = CanonicalizeBuiltinType(StripType(target_type, *module_, TypeStripMode::kAliasesAndDistinct));
                    if (!mc::sema::IsIntegerType(stripped_target) || operand->kind != ConstValue::Kind::kInteger) {
                        return std::nullopt;
                    }
                }
                return ConvertConstValue(target_type, *operand);
            }
            case Expr::Kind::kField:
                return EvaluateEnumConst(expr, nullptr, report_errors, active_names);
            case Expr::Kind::kAggregateInit: {
                Type aggregate_type = ResolveAggregateInitType(expr);
                if (IsUnknown(aggregate_type)) {
                    return std::nullopt;
                }

                ApplyNestedAggregateTypeHints(const_cast<Expr&>(expr), aggregate_type);

                const Type resolved_aggregate = StripType(aggregate_type, *module_, TypeStripMode::kAliasesOnly);
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

                const TypeDeclSummary* type_decl = LookupStructType(resolved_aggregate);
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

    std::optional<std::size_t> EvaluateArrayLength(const Expr* expr, bool report_errors) {
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

    void SeedImportedSymbols() {
        if (imported_modules_ == nullptr) {
            return;
        }

        const auto lookup_seeded_type = [&](std::string_view name) -> const TypeDeclSummary* {
            for (const auto& type_decl : module_->type_decls) {
                if (type_decl.name == name) {
                    return &type_decl;
                }
            }
            return nullptr;
        };

        std::unordered_set<std::string> seen_import_names;
        for (const auto& import_decl : source_file_.imports) {
            if (!seen_import_names.insert(import_decl.module_name).second) {
                Report(import_decl.span, "duplicate import module: " + import_decl.module_name);
                continue;
            }
            const Module* imported_module = FindImportedModule(import_decl.module_name);
            if (imported_module == nullptr) {
                Report(import_decl.span, "internal error: missing checked import module: " + import_decl.module_name);
                continue;
            }

            std::unordered_set<std::string> imported_type_names;
            for (const auto& type_decl : imported_module->type_decls) {
                imported_type_names.insert(type_decl.name);
            }

            for (const auto& type_decl : imported_module->type_decls) {
                TypeDeclSummary qualified_type = RewriteImportedTypeDecl(type_decl, import_decl.module_name, imported_type_names);
                if (!type_symbols_.emplace(qualified_type.name, qualified_type.kind).second) {
                    const TypeDeclSummary* existing_type = lookup_seeded_type(qualified_type.name);
                    if (existing_type != nullptr && EquivalentTypeDeclSummary(*existing_type, qualified_type)) {
                        continue;
                    }
                    Report(import_decl.span, "duplicate imported type symbol: " + qualified_type.name);
                    continue;
                }
                module_->type_decls.push_back(std::move(qualified_type));
            }
        }
    }

    const Module* FindImportedModule(std::string_view name) const {
        if (imported_modules_ == nullptr) {
            return nullptr;
        }

        const auto found = imported_modules_->find(std::string(name));
        if (found == imported_modules_->end()) {
            return nullptr;
        }
        return &found->second;
    }

    void CollectTopLevelDecls() {
        for (const auto& decl : source_file_.decls) {
            switch (decl.kind) {
                case Decl::Kind::kFunc:
                case Decl::Kind::kExternFunc:
                    if (!value_symbols_.emplace(decl.name, decl.kind).second) {
                        Report(decl.span, "duplicate top-level value symbol: " + decl.name);
                        break;
                    }
                    module_->functions.push_back(BuildFunctionSignature(decl));
                    break;
                case Decl::Kind::kStruct:
                case Decl::Kind::kEnum:
                case Decl::Kind::kDistinct:
                case Decl::Kind::kTypeAlias:
                    if (!type_symbols_.emplace(decl.name, decl.kind).second) {
                        Report(decl.span, "duplicate top-level type symbol: " + decl.name);
                        break;
                    }
                    module_->type_decls.push_back(BuildTypeDeclSummary(decl));
                    break;
                case Decl::Kind::kConst:
                case Decl::Kind::kVar: {
                    GlobalSummary global;
                    global.is_const = decl.kind == Decl::Kind::kConst;
                    global.type = SemanticTypeFromAst(decl.type_ann.get(), {});
                    for (const auto& name : decl.pattern.names) {
                        if (!value_symbols_.emplace(name, decl.kind).second) {
                            Report(decl.span, "duplicate top-level value symbol: " + name);
                            continue;
                        }
                        global.names.push_back(name);
                        global_symbols_[name] = {
                            .type = global.type,
                            .is_mutable = decl.kind == Decl::Kind::kVar,
                        };
                    }
                    module_->globals.push_back(std::move(global));
                    break;
                }
            }
        }
    }

    void ValidateDeclVisibility() {
        for (const auto& decl : source_file_.decls) {
            bool saw_private = false;
            for (const auto& attribute : decl.attributes) {
                if (attribute.name != "private") {
                    continue;
                }
                if (!attribute.args.empty()) {
                    Report(attribute.span, "@private does not take arguments");
                }
                if (saw_private) {
                    Report(attribute.span, "duplicate @private attribute on declaration " + DeclDisplayName(decl));
                    continue;
                }
                saw_private = true;
            }

            if (!saw_private) {
                continue;
            }

            switch (decl.kind) {
                case Decl::Kind::kFunc:
                case Decl::Kind::kExternFunc:
                    if (!decl.name.empty()) {
                        private_value_names_.insert(decl.name);
                    }
                    break;
                case Decl::Kind::kConst:
                case Decl::Kind::kVar:
                    for (const auto& name : decl.pattern.names) {
                        private_value_names_.insert(name);
                    }
                    break;
                case Decl::Kind::kStruct:
                case Decl::Kind::kEnum:
                case Decl::Kind::kDistinct:
                case Decl::Kind::kTypeAlias:
                    if (!decl.name.empty()) {
                        private_type_names_.insert(decl.name);
                    }
                    break;
            }
        }
    }

    void CollectVisiblePrivateTypeReferences(const Type& type,
                                             const std::vector<std::string>& local_type_params,
                                             std::unordered_set<std::string>& leaked_private_types) const {
        for (const auto& subtype : type.subtypes) {
            CollectVisiblePrivateTypeReferences(subtype, local_type_params, leaked_private_types);
        }

        if (type.kind != Type::Kind::kNamed || type.name.empty() || type.name.find('.') != std::string::npos) {
            return;
        }
        if (IsBuiltinNamedType(type.name) ||
            std::find(local_type_params.begin(), local_type_params.end(), type.name) != local_type_params.end()) {
            return;
        }
        if (private_type_names_.contains(type.name)) {
            leaked_private_types.insert(type.name);
        }
    }

    void ReportVisiblePrivateTypeLeaks(const Decl& decl,
                                       std::string_view declaration_kind,
                                       const std::unordered_set<std::string>& leaked_private_types) {
        for (const auto& type_name : leaked_private_types) {
            Report(decl.span,
                   "public " + std::string(declaration_kind) + " " + DeclDisplayName(decl) +
                       " references private type " + type_name);
        }
    }

    void ValidateVisibleDeclTypes() {
        for (const auto& decl : source_file_.decls) {
            if (HasPrivateAttribute(decl.attributes)) {
                continue;
            }

            std::unordered_set<std::string> leaked_private_types;
            switch (decl.kind) {
                case Decl::Kind::kFunc:
                case Decl::Kind::kExternFunc:
                    for (const auto& param : decl.params) {
                        if (param.type != nullptr) {
                            CollectVisiblePrivateTypeReferences(SemanticTypeFromAst(param.type.get(), decl.type_params),
                                                               decl.type_params,
                                                               leaked_private_types);
                        }
                    }
                    for (const auto& return_type : decl.return_types) {
                        if (return_type != nullptr) {
                            CollectVisiblePrivateTypeReferences(SemanticTypeFromAst(return_type.get(), decl.type_params),
                                                               decl.type_params,
                                                               leaked_private_types);
                        }
                    }
                    ReportVisiblePrivateTypeLeaks(decl, "function", leaked_private_types);
                    break;
                case Decl::Kind::kConst:
                case Decl::Kind::kVar:
                    if (decl.type_ann != nullptr) {
                        CollectVisiblePrivateTypeReferences(SemanticTypeFromAst(decl.type_ann.get(), {}), {}, leaked_private_types);
                    }
                    ReportVisiblePrivateTypeLeaks(decl, decl.kind == Decl::Kind::kConst ? "global" : "global", leaked_private_types);
                    break;
                case Decl::Kind::kStruct:
                case Decl::Kind::kEnum:
                case Decl::Kind::kDistinct:
                case Decl::Kind::kTypeAlias:
                    for (const auto& field : decl.fields) {
                        if (field.type != nullptr) {
                            CollectVisiblePrivateTypeReferences(SemanticTypeFromAst(field.type.get(), decl.type_params),
                                                               decl.type_params,
                                                               leaked_private_types);
                        }
                    }
                    for (const auto& variant : decl.variants) {
                        for (const auto& field : variant.payload_fields) {
                            if (field.type != nullptr) {
                                CollectVisiblePrivateTypeReferences(SemanticTypeFromAst(field.type.get(), decl.type_params),
                                                                   decl.type_params,
                                                                   leaked_private_types);
                            }
                        }
                    }
                    if (decl.aliased_type != nullptr) {
                        CollectVisiblePrivateTypeReferences(SemanticTypeFromAst(decl.aliased_type.get(), decl.type_params),
                                                           decl.type_params,
                                                           leaked_private_types);
                    }
                    ReportVisiblePrivateTypeLeaks(decl, "type", leaked_private_types);
                    break;
            }
        }
    }

    FunctionSignature BuildFunctionSignature(const Decl& decl) {
        FunctionSignature signature;
        signature.name = decl.name;
        signature.is_extern = decl.kind == Decl::Kind::kExternFunc;
        signature.extern_abi = decl.extern_abi;
        signature.type_params = decl.type_params;
        for (const auto& param : decl.params) {
            signature.param_types.push_back(SemanticTypeFromAst(param.type.get(), decl.type_params));
            bool has_noalias = false;
            for (const auto& attribute : param.attributes) {
                if (attribute.name == "noalias") {
                    has_noalias = true;
                }
            }
            signature.param_is_noalias.push_back(has_noalias);
        }
        for (const auto& return_type : decl.return_types) {
            signature.return_types.push_back(SemanticTypeFromAst(return_type.get(), decl.type_params));
        }
        return signature;
    }

    TypeDeclSummary BuildTypeDeclSummary(const Decl& decl) {
        TypeDeclSummary summary;
        summary.kind = decl.kind;
        summary.name = decl.name;
        summary.type_params = decl.type_params;
        summary.attributes = decl.attributes;
        for (const auto& field : decl.fields) {
            summary.fields.emplace_back(field.name, SemanticTypeFromAst(field.type.get(), decl.type_params));
        }
        for (const auto& variant : decl.variants) {
            VariantSummary variant_summary;
            variant_summary.name = variant.name;
            for (const auto& field : variant.payload_fields) {
                variant_summary.payload_fields.emplace_back(field.name, SemanticTypeFromAst(field.type.get(), decl.type_params));
            }
            summary.variants.push_back(std::move(variant_summary));
        }
        summary.aliased_type = SemanticTypeFromAst(decl.aliased_type.get(), decl.type_params);
        return summary;
    }

    bool IsKnownTypeName(const std::string& name, const std::vector<std::string>& type_params) const {
        return IsBuiltinNamedType(name) || LookupLocalTypeDecl(name) != nullptr ||
               std::find(type_params.begin(), type_params.end(), name) != type_params.end();
    }

    std::vector<std::string> CurrentTypeParams() const {
        return current_function_ != nullptr ? current_function_->type_params : std::vector<std::string> {};
    }

    Type InstantiateFunctionType(const FunctionSignature& function, const Expr& expr, std::string_view display_name) {
        if (function.type_params.empty()) {
            if (!expr.type_args.empty()) {
                Report(expr.span, "function " + std::string(display_name) + " does not accept type arguments");
                return UnknownType();
            }
            return ProcedureType(function.param_types, function.return_types);
        }

        if (expr.type_args.empty()) {
            Report(expr.span, "generic function " + std::string(display_name) + " requires explicit type arguments");
            return UnknownType();
        }

        if (expr.type_args.size() != function.type_params.size()) {
            Report(expr.span,
                   "generic function " + std::string(display_name) + " expects " + std::to_string(function.type_params.size()) +
                       " type arguments but got " + std::to_string(expr.type_args.size()));
            return UnknownType();
        }

        std::vector<Type> type_args;
        type_args.reserve(expr.type_args.size());
        for (const auto& type_arg : expr.type_args) {
            ValidateTypeExpr(type_arg.get(), CurrentTypeParams(), type_arg->span);
            type_args.push_back(SemanticTypeFromAst(type_arg.get(), CurrentTypeParams()));
        }

        std::vector<Type> param_types;
        param_types.reserve(function.param_types.size());
        for (const auto& param_type : function.param_types) {
            param_types.push_back(SubstituteTypeParams(param_type, function.type_params, type_args));
        }

        std::vector<Type> return_types;
        return_types.reserve(function.return_types.size());
        for (const auto& return_type : function.return_types) {
            return_types.push_back(SubstituteTypeParams(return_type, function.type_params, type_args));
        }

        return ProcedureType(std::move(param_types), std::move(return_types));
    }

    TypeDeclSummary* FindMutableTypeDecl(std::string_view name) {
        for (auto& type_decl : module_->type_decls) {
            if (type_decl.name == name) {
                return &type_decl;
            }
        }
        return nullptr;
    }

    void ValidateTypeExpr(const ast::TypeExpr* type_expr, const std::vector<std::string>& type_params, const mc::support::SourceSpan& span) {
        if (type_expr == nullptr) {
            return;
        }

        switch (type_expr->kind) {
            case ast::TypeExpr::Kind::kNamed:
                if (!IsKnownTypeName(type_expr->name, type_params)) {
                    Report(span, "unknown type: " + type_expr->name);
                } else if (const auto* type_decl = LookupLocalTypeDecl(type_expr->name); type_decl != nullptr) {
                    const std::size_t expected = type_decl->type_params.size();
                    const std::size_t actual = type_expr->type_args.size();
                    if (expected == 0 && actual != 0) {
                        Report(span, "type " + type_expr->name + " does not accept type arguments");
                    } else if (expected != 0 && actual != expected) {
                        Report(span,
                               "generic type " + type_expr->name + " expects " + std::to_string(expected) +
                                   " type arguments but got " + std::to_string(actual));
                    }
                } else if (std::find(type_params.begin(), type_params.end(), type_expr->name) != type_params.end() &&
                           !type_expr->type_args.empty()) {
                    Report(span, "type parameter " + type_expr->name + " does not accept type arguments");
                }
                for (const auto& type_arg : type_expr->type_args) {
                    ValidateTypeExpr(type_arg.get(), type_params, type_arg->span);
                }
                return;
            case ast::TypeExpr::Kind::kPointer:
            case ast::TypeExpr::Kind::kConst:
            case ast::TypeExpr::Kind::kParen:
                ValidateTypeExpr(type_expr->inner.get(), type_params, type_expr->span);
                return;
            case ast::TypeExpr::Kind::kArray:
                ValidateTypeExpr(type_expr->inner.get(), type_params, type_expr->span);
                if (type_expr->length_expr != nullptr && !EvaluateArrayLength(type_expr->length_expr.get(), true).has_value()) {
                    Report(type_expr->length_expr->span, "array length must be an integer compile-time constant");
                }
                return;
            case ast::TypeExpr::Kind::kProcedure:
                for (const auto& param : type_expr->params) {
                    ValidateTypeExpr(param.get(), type_params, param->span);
                }
                for (const auto& ret : type_expr->returns) {
                    ValidateTypeExpr(ret.get(), type_params, ret->span);
                }
                return;
        }
    }

    void ValidateTypeDecls() {
        for (const auto& decl : source_file_.decls) {
            if (decl.kind != Decl::Kind::kStruct && decl.kind != Decl::Kind::kEnum && decl.kind != Decl::Kind::kDistinct &&
                decl.kind != Decl::Kind::kTypeAlias) {
                continue;
            }

            TypeDeclSummary* summary = FindMutableTypeDecl(decl.name);
            if (summary != nullptr) {
                ValidateTypeDeclAttributes(decl, *summary);
            }

            std::unordered_set<std::string> field_names;
            for (const auto& field : decl.fields) {
                if (!field_names.insert(field.name).second) {
                    Report(field.span, "duplicate field name in type " + decl.name + ": " + field.name);
                }
                ValidateTypeExpr(field.type.get(), decl.type_params, field.span);
            }

            std::unordered_set<std::string> variant_names;
            for (const auto& variant : decl.variants) {
                if (!variant_names.insert(variant.name).second) {
                    Report(variant.span, "duplicate variant name in type " + decl.name + ": " + variant.name);
                }
                std::unordered_set<std::string> payload_names;
                for (const auto& field : variant.payload_fields) {
                    if (!payload_names.insert(field.name).second) {
                        Report(field.span, "duplicate payload field name in variant " + variant.name + ": " + field.name);
                    }
                    ValidateTypeExpr(field.type.get(), decl.type_params, field.span);
                }
            }

            ValidateTypeExpr(decl.aliased_type.get(), decl.type_params, decl.span);
            if (decl.kind == Decl::Kind::kDistinct) {
                const Type aliased_type = SemanticTypeFromAst(decl.aliased_type.get(), decl.type_params);
                if (!IsUnknown(aliased_type) && !IsValidDistinctBaseType(aliased_type, *module_)) {
                    Report(decl.span, "distinct type " + decl.name + " must use a scalar base type in v0.2");
                }
            }
        }

        mc::sema::ComputeTypeLayouts(*module_,
                                     source_file_.span,
                                     [this](const mc::support::SourceSpan& span, const std::string& message) {
                                         Report(span, message);
                                     });
    }

    void ValidateTypeDeclAttributes(const Decl& decl, TypeDeclSummary& summary) {
        bool saw_packed = false;
        bool saw_abi_c = false;
        for (const auto& attribute : decl.attributes) {
            if (attribute.name == "packed") {
                if (decl.kind != Decl::Kind::kStruct) {
                    Report(attribute.span, "@packed is only supported on struct declarations");
                    continue;
                }
                if (!attribute.args.empty()) {
                    Report(attribute.span, "@packed does not take arguments");
                    continue;
                }
                if (saw_packed) {
                    Report(attribute.span, "duplicate @packed attribute on type " + decl.name);
                    continue;
                }
                saw_packed = true;
                summary.is_packed = true;
                continue;
            }

            if (attribute.name == "abi") {
                if (decl.kind != Decl::Kind::kStruct) {
                    Report(attribute.span, "@abi(...) is only supported on struct declarations");
                    continue;
                }
                if (attribute.args.size() != 1 || !attribute.args.front().is_identifier || attribute.args.front().value != "c") {
                    Report(attribute.span, "only @abi(c) is supported in bootstrap sema");
                    continue;
                }
                if (saw_abi_c) {
                    Report(attribute.span, "duplicate @abi(c) attribute on type " + decl.name);
                    continue;
                }
                saw_abi_c = true;
                summary.is_abi_c = true;
            }
        }
    }

    void ValidateGlobals() {
        assert(scopes_.empty());
        std::size_t global_index = 0;
        for (const auto& decl : source_file_.decls) {
            if (decl.kind != Decl::Kind::kConst && decl.kind != Decl::Kind::kVar) {
                continue;
            }

            ValidateTypeExpr(decl.type_ann.get(), {}, decl.span);
            const Type declared_type = SemanticTypeFromAst(decl.type_ann.get(), {});
            if (global_index < module_->globals.size()) {
                module_->globals[global_index].type = declared_type;
                module_->globals[global_index].constant_values.assign(module_->globals[global_index].names.size(), std::nullopt);
                module_->globals[global_index].zero_initialized_values.assign(module_->globals[global_index].names.size(), false);
            }
            if (decl.has_initializer && decl.pattern.names.size() != decl.values.size()) {
                Report(decl.span, "binding requires one initializer per bound name");
                ++global_index;
                continue;
            }
            if (!decl.has_initializer && decl.kind == Decl::Kind::kVar) {
                if (global_index < module_->globals.size()) {
                    for (std::size_t index = 0; index < module_->globals[global_index].zero_initialized_values.size(); ++index) {
                        module_->globals[global_index].zero_initialized_values[index] = true;
                    }
                }
                ++global_index;
                continue;
            }
            for (std::size_t index = 0; index < decl.values.size(); ++index) {
                if (!IsUnknown(declared_type)) {
                    ApplyExpectedAggregateType(*decl.values[index], declared_type);
                }
                // Global validation runs without any pushed local scopes. AnalyzeExpr
                // therefore resolves names through global_symbols_, which was seeded
                // during CollectTopLevelDecls before ValidateGlobals runs.
                const Type value_type = AnalyzeExpr(*decl.values[index]);
                if (!IsUnknown(declared_type) && !IsAssignable(declared_type, value_type, *module_)) {
                    Report(decl.values[index]->span,
                           "global initializer type mismatch for " + decl.pattern.names[index] + ": expected " +
                               FormatType(declared_type) + ", got " + FormatType(value_type));
                }

                std::unordered_set<std::string> active_names;
                const auto const_value = EvaluateConstExpr(*decl.values[index], true, active_names);
                if (!const_value.has_value()) {
                    active_names.clear();
                }
                if (!const_value.has_value() && !IsConstExpr(*decl.values[index], true, active_names)) {
                    Report(decl.values[index]->span,
                           std::string("top-level ") + (decl.kind == Decl::Kind::kConst ? "const" : "var") +
                               " initializer must be a compile-time constant");
                    continue;
                }
                if (global_index < module_->globals.size() && index < module_->globals[global_index].constant_values.size()) {
                    module_->globals[global_index].constant_values[index] = *const_value;
                }
            }
            ++global_index;
        }
    }

    void ValidateFunctionSignatures() {
        for (const auto& decl : source_file_.decls) {
            if (decl.kind != Decl::Kind::kFunc && decl.kind != Decl::Kind::kExternFunc) {
                continue;
            }

            const FunctionSignature* signature = LookupLocalFunctionSignature(decl.name);
            if (signature == nullptr) {
                continue;
            }

            ValidateFunctionParamAttributes(decl, *signature);
            ValidateFunctionReturnContract(decl, *signature);
        }
    }

    void ValidateFunctions() {
        for (const auto& decl : source_file_.decls) {
            if (decl.kind != Decl::Kind::kFunc) {
                continue;
            }

            const FunctionSignature* signature = LookupLocalFunctionSignature(decl.name);
            if (signature == nullptr || decl.body == nullptr) {
                continue;
            }

            current_function_ = signature;
            loop_depth_ = 0;
            scopes_.clear();
            PushScope();
            for (std::size_t index = 0; index < decl.params.size(); ++index) {
                ValidateTypeExpr(decl.params[index].type.get(), signature->type_params, decl.params[index].span);
                BindValue(decl.params[index].name, signature->param_types[index], false, decl.params[index].span);
            }
            for (const auto& return_type : decl.return_types) {
                ValidateTypeExpr(return_type.get(), signature->type_params, return_type->span);
            }
            CheckStmt(*decl.body);
            if (!signature->return_types.empty() && !StmtAlwaysReturns(*decl.body)) {
                Report(decl.span, "function " + decl.name + " may exit without returning a value");
            }
            PopScope();
            current_function_ = nullptr;
        }
    }

    void ValidateFunctionParamAttributes(const Decl& decl, const FunctionSignature& signature) {
        for (std::size_t index = 0; index < decl.params.size() && index < signature.param_types.size(); ++index) {
            bool saw_noalias = false;
            for (const auto& attribute : decl.params[index].attributes) {
                if (attribute.name == "private") {
                    Report(attribute.span, "@private is only supported on top-level declarations");
                    continue;
                }
                if (attribute.name != "noalias") {
                    continue;
                }
                if (!attribute.args.empty()) {
                    Report(attribute.span, "@noalias does not take arguments");
                    continue;
                }
                if (saw_noalias) {
                    Report(attribute.span, "duplicate @noalias attribute on parameter " + decl.params[index].name);
                    continue;
                }
                saw_noalias = true;
            }
            if (saw_noalias && !IsNoAliasEligibleType(signature.param_types[index], *module_)) {
                Report(decl.params[index].span, "@noalias parameter must have pointer type");
            }
        }
    }

    void ValidateFunctionReturnContract(const Decl& decl, const FunctionSignature& signature) {
        if (signature.return_types.empty()) {
            return;
        }

        for (std::size_t index = 0; index + 1 < signature.return_types.size(); ++index) {
            if (index < decl.return_types.size() && IsErrorType(signature.return_types[index], *module_)) {
                Report(decl.return_types[index]->span, "Error return type must appear last in function signature");
            }
        }
    }

    bool StmtListAlwaysReturns(const std::vector<std::unique_ptr<Stmt>>& statements) const {
        for (const auto& stmt : statements) {
            if (stmt != nullptr && StmtAlwaysReturns(*stmt)) {
                return true;
            }
        }
        return false;
    }

    bool ExprAlwaysTerminates(const Expr& expr) const {
        return IsPanicBuiltinCall(expr);
    }

    bool StmtAlwaysReturns(const Stmt& stmt) const {
        switch (stmt.kind) {
            case Stmt::Kind::kReturn:
                return true;
            case Stmt::Kind::kExpr:
                return !stmt.exprs.empty() && stmt.exprs.front() != nullptr && ExprAlwaysTerminates(*stmt.exprs.front());
            case Stmt::Kind::kBlock:
                return StmtListAlwaysReturns(stmt.statements);
            case Stmt::Kind::kIf:
                return stmt.then_branch != nullptr && stmt.else_branch != nullptr && StmtAlwaysReturns(*stmt.then_branch) &&
                       StmtAlwaysReturns(*stmt.else_branch);
            case Stmt::Kind::kSwitch:
                if (!stmt.has_default_case) {
                    return false;
                }
                for (const auto& switch_case : stmt.switch_cases) {
                    if (!StmtListAlwaysReturns(switch_case.statements)) {
                        return false;
                    }
                }
                return StmtListAlwaysReturns(stmt.default_case.statements);
            default:
                return false;
        }
    }

    void PushScope() {
        scopes_.push_back({});
    }

    void PopScope() {
        scopes_.pop_back();
    }

    bool BindValue(const std::string& name, const Type& type, bool is_mutable, const mc::support::SourceSpan& span) {
        auto& scope = scopes_.back();
        if (scope.contains(name)) {
            Report(span, "duplicate local binding: " + name);
            return false;
        }
        scope[name] = {
            .type = type,
            .is_mutable = is_mutable,
        };
        return true;
    }

    std::optional<ValueBinding> LookupValue(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            const auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        const auto found_global = global_symbols_.find(name);
        if (found_global != global_symbols_.end()) {
            return found_global->second;
        }
        return std::nullopt;
    }

    void RecordExprType(const Expr& expr, const Type& type) {
        module_->expr_types[expr.span] = type;
    }

    void RecordBindingOrAssignFact(const Stmt& stmt, std::vector<BindingOrAssignResolution> resolutions) {
        module_->binding_or_assign_facts[stmt.span] = {
            .span = stmt.span,
            .resolutions = std::move(resolutions),
        };
    }

    void RecordForInFact(const Stmt& stmt, ForInResolution resolution) {
        module_->for_in_facts[stmt.span] = {
            .span = stmt.span,
            .resolution = resolution,
        };
    }

    bool IsPanicBuiltinCall(const Expr& expr) const {
        if (expr.kind != Expr::Kind::kCall || expr.left == nullptr || expr.type_target != nullptr) {
            return false;
        }
        if (expr.left->kind == Expr::Kind::kName) {
            return expr.left->text == "panic";
        }
        if (expr.left->kind == Expr::Kind::kQualifiedName) {
            return CombineQualifiedName(*expr.left) == "panic";
        }
        return false;
    }

    Type AnalyzePanicBuiltinCall(const Expr& expr, const std::vector<Type>& arg_types) {
        if (expr.args.size() != 1) {
            Report(expr.span,
                   "panic(code) expects exactly 1 argument but got " + std::to_string(expr.args.size()));
            return VoidType();
        }

        const Type& code_type = arg_types.front();
        if (!IsUnknown(code_type) && !IsIntegerLikeType(code_type, *module_)) {
            Report(expr.args.front()->span,
                   "panic(code) requires an integer fault code but got " + FormatType(code_type));
        }

        std::unordered_set<std::string> active_names;
        const auto code_value = EvaluateConstExpr(*expr.args.front(), true, active_names);
        if (!code_value.has_value() || code_value->kind != ConstValue::Kind::kInteger) {
            Report(expr.args.front()->span, "panic(code) requires a compile-time integer fault code");
            return VoidType();
        }
        if (code_value->integer_value < 0) {
            Report(expr.args.front()->span, "panic(code) requires a non-negative fault code");
        }

        return VoidType();
    }

    bool HasDuplicateBindingOrAssignBindNames(const Stmt& stmt,
                                              const std::vector<BindingOrAssignResolution>& resolutions) {
        std::unordered_set<std::string> seen_bind_names;
        bool has_duplicates = false;
        for (std::size_t index = 0; index < stmt.pattern.names.size() && index < resolutions.size(); ++index) {
            if (resolutions[index] != BindingOrAssignResolution::kBind) {
                continue;
            }
            if (!seen_bind_names.insert(stmt.pattern.names[index]).second) {
                Report(stmt.span, "duplicate local binding: " + stmt.pattern.names[index]);
                has_duplicates = true;
            }
        }
        return has_duplicates;
    }

    void BuildCheckerLookupMaps() {
        checker_function_lookup_.clear();
        checker_function_lookup_.reserve(module_->functions.size());
        for (const auto& function : module_->functions) {
            checker_function_lookup_.emplace(function.name, &function);
        }

        checker_type_decl_lookup_.clear();
        checker_type_decl_lookup_.reserve(module_->type_decls.size());
        for (const auto& type_decl : module_->type_decls) {
            checker_type_decl_lookup_.emplace(type_decl.name, &type_decl);
        }

        std::size_t global_name_count = 0;
        for (const auto& global : module_->globals) {
            global_name_count += global.names.size();
        }
        checker_global_lookup_.clear();
        checker_global_lookup_.reserve(global_name_count);
        for (const auto& global : module_->globals) {
            for (const auto& name : global.names) {
                checker_global_lookup_.emplace(name, &global);
            }
        }
    }

    const FunctionSignature* LookupLocalFunctionSignature(std::string_view name) const {
        const auto found = checker_function_lookup_.find(std::string(name));
        if (found != checker_function_lookup_.end()) {
            return found->second;
        }
        return nullptr;
    }

    const TypeDeclSummary* LookupLocalTypeDecl(std::string_view name) const {
        const auto found = checker_type_decl_lookup_.find(std::string(name));
        if (found != checker_type_decl_lookup_.end()) {
            return found->second;
        }
        return nullptr;
    }

    const GlobalSummary* LookupLocalGlobalSummary(std::string_view name) const {
        const auto found = checker_global_lookup_.find(std::string(name));
        if (found != checker_global_lookup_.end()) {
            return found->second;
        }
        return nullptr;
    }

    const TypeDeclSummary* LookupStructType(const Type& type) const {
        const Type stripped = StripType(type, *module_, TypeStripMode::kAliasesOnly);
        if (stripped.kind != Type::Kind::kNamed) {
            return nullptr;
        }
        return LookupLocalTypeDecl(stripped.name);
    }

    std::optional<Type> LookupBuiltinMemberType(const Type& raw_base, std::string_view member_name) const {
        const Type base = CanonicalizeBuiltinType(StripType(raw_base, *module_, TypeStripMode::kAliasesOnly));
        if (base.kind == Type::Kind::kNamed && (base.name == "Slice" || base.name == "Buffer")) {
            if (member_name == "ptr" && !base.subtypes.empty()) {
                return PointerType(base.subtypes.front());
            }
            if (member_name == "len" || (base.name == "Buffer" && member_name == "cap")) {
                return NamedType("usize");
            }
            if (base.name == "Buffer" && member_name == "alloc") {
                return PointerType(NamedType("Allocator"));
            }
        }
        if (base.kind == Type::Kind::kString && (member_name == "ptr" || member_name == "len")) {
            if (member_name == "ptr") {
                return PointerType(NamedType("u8"));
            }
            return NamedType("usize");
        }
        return std::nullopt;
    }

    Type AnalyzeFieldSelection(const Type& raw_base, std::string_view member_name, const mc::support::SourceSpan& span) {
        if (const auto builtin_member = LookupBuiltinMemberType(raw_base, member_name); builtin_member.has_value()) {
            return *builtin_member;
        }

        const Type stripped_base = StripType(raw_base, *module_, TypeStripMode::kAliasesOnly);
        const auto builtin_fields = BuiltinAggregateFields(stripped_base);
        if (!builtin_fields.empty()) {
            for (const auto& field : builtin_fields) {
                if (field.first == member_name) {
                    return field.second;
                }
            }
            Report(span, "type " + FormatType(stripped_base) + " has no field named " + std::string(member_name));
            return UnknownType();
        }

        if (stripped_base.kind == Type::Kind::kNamed) {
            if (const auto* type_decl = LookupLocalTypeDecl(stripped_base.name);
                type_decl != nullptr && type_decl->kind == Decl::Kind::kEnum) {
                const auto variant = LookupVariant(stripped_base, member_name);
                if (variant.has_value()) {
                    std::vector<Type> payload_types;
                    payload_types.reserve(variant->payload_fields.size());
                    for (const auto& payload_field : variant->payload_fields) {
                        payload_types.push_back(payload_field.second);
                    }
                    return TupleType(std::move(payload_types));
                }
                Report(span, "type " + type_decl->name + " has no field named " + std::string(member_name));
                return UnknownType();
            }
        }

        const TypeDeclSummary* type_decl = LookupStructType(raw_base);
        if (type_decl != nullptr) {
            const auto instantiated_fields = InstantiateTypeDeclFields(*type_decl, stripped_base);
            for (const auto& field : instantiated_fields) {
                if (field.first == member_name) {
                    return field.second;
                }
            }
            Report(span, "type " + type_decl->name + " has no field named " + std::string(member_name));
        }
        return UnknownType();
    }

    Type AnalyzeQualifiedBoundValue(const Expr& expr, const ValueBinding& binding) {
        if (!expr.type_args.empty()) {
            Report(expr.span, "type arguments apply only to functions: " + CombineQualifiedName(expr));
            return UnknownType();
        }
        return AnalyzeFieldSelection(binding.type, expr.secondary_text, expr.span);
    }

    Type AnalyzeQualifiedGlobalValue(const Expr& expr, const GlobalSummary& global) {
        if (!expr.type_args.empty()) {
            Report(expr.span, "type arguments apply only to functions: " + CombineQualifiedName(expr));
            return UnknownType();
        }
        if (const auto builtin_member = LookupBuiltinMemberType(global.type, expr.secondary_text); builtin_member.has_value()) {
            return *builtin_member;
        }
        return UnknownType();
    }

    Type AnalyzeQualifiedImportedModuleMember(const Expr& expr, const Module& imported_module) {
        if (const auto* function = FindFunctionSignature(imported_module, expr.secondary_text); function != nullptr) {
            return InstantiateFunctionType(*function, expr, CombineQualifiedName(expr));
        }
        if (const auto* type_decl = FindTypeDecl(imported_module, QualifyImportedName(expr.text, expr.secondary_text)); type_decl != nullptr) {
            const std::size_t expected = type_decl->type_params.size();
            const std::size_t actual = expr.type_args.size();
            if (expected == 0 && actual != 0) {
                Report(expr.span, "type " + CombineQualifiedName(expr) + " does not accept type arguments");
                return UnknownType();
            }
            if (expected != 0 && actual != expected) {
                Report(expr.span,
                       "generic type " + CombineQualifiedName(expr) + " expects " + std::to_string(expected) +
                           " type arguments but got " + std::to_string(actual));
                return UnknownType();
            }
            Type instantiated = NamedType(type_decl->name);
            instantiated.subtypes.reserve(actual);
            for (const auto& type_arg : expr.type_args) {
                ValidateTypeExpr(type_arg.get(), CurrentTypeParams(), type_arg->span);
                instantiated.subtypes.push_back(SemanticTypeFromAst(type_arg.get(), CurrentTypeParams()));
            }
            return instantiated;
        }
        if (const auto* global = FindGlobalSummary(imported_module, expr.secondary_text); global != nullptr) {
            if (!expr.type_args.empty()) {
                Report(expr.span, "type arguments apply only to functions: " + CombineQualifiedName(expr));
                return UnknownType();
            }
            return global->type;
        }
        if (imported_module.hidden_value_names.contains(expr.secondary_text) || imported_module.hidden_type_names.contains(expr.secondary_text)) {
            Report(expr.span, "module " + expr.text + " member " + expr.secondary_text + " is private");
        } else {
            Report(expr.span, "module " + expr.text + " has no visible member named " + expr.secondary_text);
        }
        return UnknownType();
    }

    Type AnalyzeQualifiedFunctionOrEnum(const Expr& expr) {
        if (const auto* function = LookupLocalFunctionSignature(expr.text + "." + expr.secondary_text); function != nullptr) {
            return InstantiateFunctionType(*function, expr, CombineQualifiedName(expr));
        }
        if (const auto* type_decl = LookupLocalTypeDecl(expr.text); type_decl != nullptr && type_decl->kind == Decl::Kind::kEnum) {
            const std::size_t expected = type_decl->type_params.size();
            const std::size_t actual = expr.type_args.size();
            if (expected == 0 && actual != 0) {
                Report(expr.span, "type " + expr.text + " does not accept type arguments");
                return UnknownType();
            }
            if (expected != 0 && actual != expected) {
                Report(expr.span,
                       "generic type " + expr.text + " expects " + std::to_string(expected) +
                           " type arguments but got " + std::to_string(actual));
                return UnknownType();
            }

            Type enum_type = NamedType(expr.text);
            enum_type.subtypes.reserve(actual);
            for (const auto& type_arg : expr.type_args) {
                ValidateTypeExpr(type_arg.get(), CurrentTypeParams(), type_arg->span);
                enum_type.subtypes.push_back(SemanticTypeFromAst(type_arg.get(), CurrentTypeParams()));
            }

            for (const auto& variant : type_decl->variants) {
                if (variant.name == expr.secondary_text) {
                    return enum_type;
                }
            }
        }
        if (!expr.type_args.empty()) {
            Report(expr.span, "type arguments apply only to functions: " + CombineQualifiedName(expr));
            return UnknownType();
        }
        return UnknownType();
    }

    Type AnalyzeFlattenedQualifiedValue(const Expr& expr) {
        const auto base_expr = FlattenQualifiedBaseExpr(expr);
        if (!base_expr.has_value()) {
            return UnknownType();
        }

        const Type base_type = AnalyzeExpr(*base_expr);
        if (IsUnknown(base_type)) {
            return UnknownType();
        }

        const Type stripped_base = StripType(base_type, *module_, TypeStripMode::kAliasesOnly);
        if (stripped_base.kind == Type::Kind::kNamed) {
            if (const auto* type_decl = LookupLocalTypeDecl(stripped_base.name);
                type_decl != nullptr && type_decl->kind == Decl::Kind::kEnum) {
                for (const auto& variant : type_decl->variants) {
                    if (variant.name == expr.secondary_text) {
                        return stripped_base;
                    }
                }
            }
        }

        return AnalyzeFieldSelection(base_type, expr.secondary_text, expr.span);
    }

    std::optional<VariantSummary> LookupVariant(const Type& selector_type, std::string_view variant_name) const {
        const Type stripped_selector = StripType(selector_type, *module_, TypeStripMode::kAliasesOnly);
        if (stripped_selector.kind != Type::Kind::kNamed) {
            return std::nullopt;
        }

        const TypeDeclSummary* type_decl = LookupLocalTypeDecl(stripped_selector.name);
        if (type_decl == nullptr) {
            return std::nullopt;
        }

        // Split on the last '.' so both Foo.Bar and mod.Foo.Bar work:
        //   "Foo.Bar"     → qualified_type="Foo",     leaf_name="Bar"
        //   "mod.Foo.Bar" → qualified_type="mod.Foo", leaf_name="Bar"
        const std::size_t separator = variant_name.rfind('.');
        const std::string qualified_type = separator == std::string_view::npos ? stripped_selector.name
                                                                                : std::string(variant_name.substr(0, separator));
        const std::string leaf_name = separator == std::string_view::npos ? std::string(variant_name) : std::string(variant_name.substr(separator + 1));
        if (!qualified_type.empty() && qualified_type != stripped_selector.name) {
            return std::nullopt;
        }

        for (const auto& variant : type_decl->variants) {
            if (variant.name == leaf_name) {
                return InstantiateVariantSummary(variant, *type_decl, stripped_selector);
            }
        }

        return std::nullopt;
    }

    struct VariantDesignatorInfo {
        std::optional<Type> explicit_enum_type;
        std::string enum_name;
        std::string variant_name;
    };

    std::optional<VariantDesignatorInfo> AnalyzeVariantDesignator(const Expr& expr) {
        if (expr.kind == Expr::Kind::kQualifiedName) {
            std::optional<Type> explicit_enum_type;
            if (!expr.type_args.empty()) {
                explicit_enum_type = AnalyzeQualifiedFunctionOrEnum(expr);
            }
            return VariantDesignatorInfo {
                .explicit_enum_type = std::move(explicit_enum_type),
                .enum_name = expr.text,
                .variant_name = expr.secondary_text,
            };
        } else if (expr.kind == Expr::Kind::kField && expr.left != nullptr) {
            if (expr.left->kind == Expr::Kind::kName || expr.left->kind == Expr::Kind::kQualifiedName) {
                std::optional<Type> explicit_enum_type;
                if (!expr.left->type_args.empty()) {
                    explicit_enum_type = AnalyzeExpr(*expr.left);
                }
                return VariantDesignatorInfo {
                    .explicit_enum_type = std::move(explicit_enum_type),
                    .enum_name = CombineQualifiedName(*expr.left),
                    .variant_name = expr.text,
                };
            }
        }

        Report(expr.span, "expected enum variant designator");
        return std::nullopt;
    }

    Type AnalyzeVariantConstructorCall(const Expr& expr,
                                       const Type& callee_type,
                                       const std::vector<Type>& arg_types) {
        if (expr.left == nullptr || expr.left->kind != Expr::Kind::kQualifiedName || callee_type.kind != Type::Kind::kNamed) {
            return UnknownType();
        }

        const auto variant = LookupVariant(callee_type, expr.left->secondary_text);
        if (!variant.has_value()) {
            return UnknownType();
        }

        const std::string callee_name = CombineQualifiedName(*expr.left);
        if (variant->payload_fields.size() != expr.args.size()) {
            Report(expr.span,
                   "call to " + callee_name + " expects " + std::to_string(variant->payload_fields.size()) +
                       " arguments but got " + std::to_string(expr.args.size()));
            return callee_type;
        }

        for (std::size_t index = 0; index < arg_types.size(); ++index) {
            const Type& expected = variant->payload_fields[index].second;
            const Type& actual = arg_types[index];
            if (!IsAssignable(expected, actual, *module_)) {
                Report(expr.args[index]->span,
                       "argument type mismatch for parameter " + std::to_string(index + 1) + " of " + callee_name +
                           ": expected " + FormatType(expected) + ", got " + FormatType(actual));
            }
        }

        return callee_type;
    }

    Type AnalyzeCall(const Expr& expr) {
        if (expr.left == nullptr && expr.type_target == nullptr) {
            return UnknownType();
        }

        Type conversion_target = UnknownType();
        if (expr.type_target != nullptr) {
            ValidateTypeExpr(expr.type_target.get(), CurrentTypeParams(), expr.type_target->span);
            conversion_target = SemanticTypeFromAst(expr.type_target.get(), CurrentTypeParams());
        }

        if (IsPanicBuiltinCall(expr)) {
            std::vector<Type> arg_types;
            arg_types.reserve(expr.args.size());
            for (const auto& arg : expr.args) {
                arg_types.push_back(AnalyzeExpr(*arg));
            }
            return AnalyzePanicBuiltinCall(expr, arg_types);
        }

        if (!IsUnknown(conversion_target)) {
            if (expr.args.size() == 1) {
                ApplyExpectedAggregateType(*expr.args.front(), conversion_target);
            }
            std::vector<Type> arg_types;
            arg_types.reserve(expr.args.size());
            for (const auto& arg : expr.args) {
                arg_types.push_back(AnalyzeExpr(*arg));
            }
            if (expr.args.size() != 1) {
                Report(expr.span, "explicit conversion to " + FormatType(conversion_target) + " expects 1 argument but got " +
                                      std::to_string(expr.args.size()));
                return conversion_target;
            }
            const Type actual = arg_types.front();
            if (expr.bare_type_target_syntax) {
                const Type stripped_target = CanonicalizeBuiltinType(StripType(conversion_target, *module_, TypeStripMode::kAliasesAndDistinct));
                if (!mc::sema::IsIntegerType(stripped_target) || !IsIntegerLikeType(actual, *module_)) {
                    Report(expr.args.front()->span,
                           "explicit integer conversion to " + FormatType(conversion_target) +
                               " requires integer argument but got " + FormatType(actual));
                }
                return stripped_target;
            }
            // Infer missing type parameters (e.g. bare `Slice` → `Slice<T>`) before
            // validating. The error message uses the original written target so the
            // user sees what they wrote, not the inferred form.
            const Type inferred = InferExplicitConversionTarget(conversion_target, actual, *module_);
            if (!IsExplicitlyConvertible(inferred, actual, *module_)) {
                Report(expr.args.front()->span,
                       "explicit conversion to " + FormatType(conversion_target) + " is not allowed from " + FormatType(actual));
            }
            return inferred;
        }

        if (expr.left == nullptr) {
            return UnknownType();
        }

        const Type callee_type = AnalyzeExpr(*expr.left);
        if (IsUnknown(callee_type)) {
            if (expr.left != nullptr && expr.left->kind == Expr::Kind::kName &&
                IsKnownTypeName(expr.left->text, CurrentTypeParams())) {
                Report(expr.span, "explicit conversions require parenthesized type-expression syntax");
            }
            return UnknownType();
        }

        if (expr.left != nullptr && expr.left->kind == Expr::Kind::kQualifiedName && callee_type.kind == Type::Kind::kNamed) {
            if (const auto variant = LookupVariant(callee_type, expr.left->secondary_text);
                variant.has_value() && variant->payload_fields.size() == expr.args.size()) {
                for (std::size_t index = 0; index < expr.args.size(); ++index) {
                    ApplyExpectedAggregateType(*expr.args[index], variant->payload_fields[index].second);
                }
            }
        }

        const Type stripped_callee = ResolveCallableShell(callee_type, *module_);
        if (stripped_callee.kind == Type::Kind::kProcedure) {
            const std::size_t param_count = ProcedureParamCount(stripped_callee);
            if (param_count == expr.args.size()) {
                for (std::size_t index = 0; index < expr.args.size(); ++index) {
                    ApplyExpectedAggregateType(*expr.args[index], stripped_callee.subtypes[index]);
                }
            }
        }

        std::vector<Type> arg_types;
        arg_types.reserve(expr.args.size());
        for (const auto& arg : expr.args) {
            arg_types.push_back(AnalyzeExpr(*arg));
        }

        const Type variant_ctor_type = AnalyzeVariantConstructorCall(expr, callee_type, arg_types);
        if (!IsUnknown(variant_ctor_type)) {
            return variant_ctor_type;
        }

        if (stripped_callee.kind != Type::Kind::kProcedure) {
            Report(expr.left->span, "call target must have procedure type");
            return UnknownType();
        }

        const std::size_t param_count = ProcedureParamCount(stripped_callee);
        const std::string callee_name = CombineQualifiedName(*expr.left);
        const bool has_named_callee = expr.left->kind == Expr::Kind::kName || expr.left->kind == Expr::Kind::kQualifiedName;

        if (param_count != expr.args.size()) {
            const std::string message_prefix = has_named_callee ? "call to " + callee_name : "call target";
            Report(expr.span,
                   message_prefix + " expects " + std::to_string(param_count) + " arguments but got " + std::to_string(expr.args.size()));
            return ProcedureReturnType(stripped_callee);
        }

        for (std::size_t index = 0; index < expr.args.size(); ++index) {
            const Type& actual = arg_types[index];
            const Type& expected = stripped_callee.subtypes[index];
            if (!IsAssignable(expected, actual, *module_)) {
                const std::string callee_label = has_named_callee ? callee_name : "call target";
                Report(expr.args[index]->span,
                       "argument type mismatch for parameter " + std::to_string(index + 1) + " of " + callee_label +
                           ": expected " + FormatType(expected) + ", got " + FormatType(actual));
            }
        }

        return ProcedureReturnType(stripped_callee);
    }

    Type ProcedureReturnType(const Type& procedure_type) const {
        const std::size_t param_count = ProcedureParamCount(procedure_type);
        if (param_count >= procedure_type.subtypes.size()) {
            return VoidType();
        }

        const std::vector<Type> returns(procedure_type.subtypes.begin() + static_cast<std::ptrdiff_t>(param_count),
                                        procedure_type.subtypes.end());
        if (returns.empty()) {
            return VoidType();
        }
        if (returns.size() == 1) {
            return returns.front();
        }
        return TupleType(returns);
    }

    Type AnalyzeExpr(const Expr& expr) {
        const auto record = [&](const Type& type) {
            RecordExprType(expr, type);
            return type;
        };

        switch (expr.kind) {
            case Expr::Kind::kName: {
                const auto binding = LookupValue(expr.text);
                if (binding.has_value()) {
                    if (!expr.type_args.empty()) {
                        Report(expr.span, "type arguments apply only to functions: " + expr.text);
                        return record(UnknownType());
                    }
                    return record(binding->type);
                }
                if (const auto* function = LookupLocalFunctionSignature(expr.text); function != nullptr) {
                    return record(InstantiateFunctionType(*function, expr, expr.text));
                }
                if (const auto* global = LookupLocalGlobalSummary(expr.text); global != nullptr) {
                    if (!expr.type_args.empty()) {
                        Report(expr.span, "type arguments apply only to functions: " + expr.text);
                        return record(UnknownType());
                    }
                    return record(global->type);
                }
                Report(expr.span, "unknown name: " + expr.text);
                return record(UnknownType());
            }
            case Expr::Kind::kQualifiedName:
                if (const auto binding = LookupValue(expr.text); binding.has_value()) {
                    return record(AnalyzeQualifiedBoundValue(expr, *binding));
                }
                if (const auto* global = LookupLocalGlobalSummary(expr.text); global != nullptr) {
                    return record(AnalyzeQualifiedGlobalValue(expr, *global));
                }
                if (const Module* imported_module = FindImportedModule(expr.text); imported_module != nullptr) {
                    return record(AnalyzeQualifiedImportedModuleMember(expr, *imported_module));
                }
                if (expr.text.find('.') != std::string::npos) {
                    return record(AnalyzeFlattenedQualifiedValue(expr));
                }
                return record(AnalyzeQualifiedFunctionOrEnum(expr));
            case Expr::Kind::kLiteral:
                return record(InferLiteralType(expr));
            case Expr::Kind::kUnary: {
                const Type operand = AnalyzeExpr(*expr.left);
                if (expr.text == "!" && !IsUnknown(operand) && !IsBoolType(operand, *module_)) {
                    Report(expr.span, "logical not requires bool operand");
                }
                if (expr.text == "&") {
                    if (!IsAddressableExpr(*expr.left)) {
                        Report(expr.span, "address-of requires an assignable operand");
                    }
                    return record(IsUnknown(operand) ? UnknownType() : PointerType(operand));
                }
                if (expr.text == "*") {
                    const Type deref_operand = StripType(operand, *module_, TypeStripMode::kAliasesAndDistinct);
                    if (!IsUnknown(deref_operand) && deref_operand.kind != Type::Kind::kPointer) {
                        Report(expr.span, "pointer dereference requires pointer operand");
                    }
                    if (deref_operand.kind == Type::Kind::kPointer && !deref_operand.subtypes.empty()) {
                        return record(deref_operand.subtypes.front());
                    }
                    return record(UnknownType());
                }
                return record(expr.text == "!" ? BoolType() : operand);
            }
            case Expr::Kind::kBinary:
            case Expr::Kind::kRange: {
                if (expr.text == "is") {
                    const Type left = AnalyzeExpr(*expr.left);
                    const auto variant = AnalyzeVariantDesignator(*expr.right);
                    if (variant.has_value() && !IsUnknown(left)) {
                        const Type selector_type = StripType(left, *module_, TypeStripMode::kAliasesOnly);
                        if (selector_type.kind != Type::Kind::kNamed) {
                            Report(expr.span,
                                   "variant test requires enum selector type but got " + FormatType(left));
                        } else {
                            if (variant->explicit_enum_type.has_value() &&
                                !IsAssignable(*variant->explicit_enum_type, selector_type, *module_) &&
                                !IsAssignable(selector_type, *variant->explicit_enum_type, *module_)) {
                                Report(expr.span,
                                       "variant test requires selector type " + FormatType(*variant->explicit_enum_type) +
                                           " but got " + FormatType(selector_type));
                            } else if (!variant->enum_name.empty() && variant->enum_name != selector_type.name) {
                                Report(expr.span,
                                       "variant test requires selector type " + variant->enum_name +
                                           " but got " + FormatType(selector_type));
                            }
                            if (!LookupVariant(selector_type, variant->variant_name).has_value()) {
                                Report(expr.right->span,
                                       "type " + FormatType(selector_type) + " has no variant named " + variant->variant_name);
                            }
                        }
                    }
                    return record(BoolType());
                }

                const Type left = AnalyzeExpr(*expr.left);
                const Type right = AnalyzeExpr(*expr.right);
                if (expr.kind == Expr::Kind::kRange) {
                    const Type element = MergeNumericTypes(left, right, *module_);
                    if (IsUnknown(element)) {
                        Report(expr.span, "range bounds must be compatible numeric types");
                    }
                    return record(RangeType(element));
                }
                if (expr.text == "&&" || expr.text == "||") {
                    if (!IsUnknown(left) && !IsBoolType(left, *module_)) {
                        Report(expr.left->span, "logical operator requires bool operands");
                    }
                    if (!IsUnknown(right) && !IsBoolType(right, *module_)) {
                        Report(expr.right->span, "logical operator requires bool operands");
                    }
                    return record(BoolType());
                }
                if (expr.text == "==" || expr.text == "!=" || expr.text == "<" || expr.text == ">" || expr.text == "<=" ||
                    expr.text == ">=") {
                    if (IsUnknown(MergeNumericTypes(left, right, *module_)) && !IsAssignable(left, right, *module_) &&
                        !IsAssignable(right, left, *module_)) {
                        Report(expr.span, "comparison requires compatible operand types");
                    }
                    return record(BoolType());
                }
                if (expr.text == "<<" || expr.text == ">>") {
                    if (!IsUnknown(left) && !IsIntegerLikeType(left, *module_)) {
                        Report(expr.left->span, "shift operator requires integer left operand");
                    }
                    if (!IsUnknown(right) && !IsIntegerLikeType(right, *module_)) {
                        Report(expr.right->span, "shift operator requires integer right operand");
                    }
                    return record(left);
                }
                if (expr.text == "&" || expr.text == "|" || expr.text == "^") {
                    if (!IsUnknown(left) && !IsIntegerLikeType(left, *module_)) {
                        Report(expr.left->span, "bitwise operator requires integer operands");
                    }
                    if (!IsUnknown(right) && !IsIntegerLikeType(right, *module_)) {
                        Report(expr.right->span, "bitwise operator requires integer operands");
                    }
                    const Type merged = MergeNumericTypes(left, right, *module_);
                    if (IsUnknown(merged) || (!IsUnknown(merged) && !IsIntegerLikeType(merged, *module_))) {
                        if (!IsUnknown(left) && !IsUnknown(right)) {
                            Report(expr.span, "bitwise operator requires compatible integer operand types");
                        }
                        return record(UnknownType());
                    }
                    return record(merged);
                }
                if (!IsNumericType(left, *module_) || !IsNumericType(right, *module_)) {
                    if (!IsUnknown(left) && !IsUnknown(right)) {
                        Report(expr.span, "operator " + expr.text + " requires numeric operands");
                    }
                    return record(UnknownType());
                }
                const Type merged = MergeNumericTypes(left, right, *module_);
                if (IsUnknown(merged)) {
                    Report(expr.span, "operator " + expr.text + " requires compatible numeric operand types");
                }
                return record(merged);
            }
            case Expr::Kind::kCall:
                return record(AnalyzeCall(expr));
            case Expr::Kind::kField: {
                const Type base = AnalyzeExpr(*expr.left);
                return record(AnalyzeFieldSelection(base, expr.text, expr.span));
            }
            case Expr::Kind::kDerefField: {
                const Type base = StripType(AnalyzeExpr(*expr.left), *module_, TypeStripMode::kAliasesOnly);
                const Type pointee = base.kind == Type::Kind::kPointer && !base.subtypes.empty() ? base.subtypes.front() : UnknownType();
                return record(pointee);
            }
            case Expr::Kind::kIndex: {
                const Type base = StripType(AnalyzeExpr(*expr.left), *module_, TypeStripMode::kAliasesOnly);
                (void)AnalyzeExpr(*expr.right);
                if (base.kind == Type::Kind::kArray && !base.subtypes.empty()) {
                    return record(base.subtypes.front());
                }
                if (base.kind == Type::Kind::kNamed && (base.name == "Slice" || base.name == "Buffer") && !base.subtypes.empty()) {
                    return record(base.subtypes.front());
                }
                return record(UnknownType());
            }
            case Expr::Kind::kSlice:
            {
                const Type base = StripType(AnalyzeExpr(*expr.left), *module_, TypeStripMode::kAliasesOnly);
                if (expr.right != nullptr) {
                    (void)AnalyzeExpr(*expr.right);
                }
                if (expr.extra != nullptr) {
                    (void)AnalyzeExpr(*expr.extra);
                }
                if (base.kind == Type::Kind::kArray && !base.subtypes.empty()) {
                    Type slice = NamedType("Slice");
                    slice.subtypes.push_back(base.subtypes.front());
                    return record(slice);
                }
                if (base.kind == Type::Kind::kNamed && base.name == "Slice") {
                    return record(base);
                }
                if (base.kind == Type::Kind::kNamed && base.name == "Buffer") {
                    Type slice = NamedType("Slice");
                    slice.subtypes = base.subtypes;
                    return record(slice);
                }
                if (base.kind == Type::Kind::kString) {
                    return record(StringType());
                }
                return record(UnknownType());
            }
            case Expr::Kind::kAggregateInit: {
                Type aggregate_type = ResolveAggregateInitType(expr);
                if (IsUnknown(aggregate_type)) {
                    for (const auto& field_init : expr.field_inits) {
                        if (field_init.value != nullptr) {
                            (void)AnalyzeExpr(*field_init.value);
                        }
                    }
                    Report(expr.span, "aggregate initializer requires an explicit type or an expected aggregate context");
                    return record(UnknownType());
                }

                ApplyNestedAggregateTypeHints(const_cast<Expr&>(expr), aggregate_type);

                const Type resolved_aggregate = StripType(aggregate_type, *module_, TypeStripMode::kAliasesOnly);
                if (resolved_aggregate.kind == Type::Kind::kArray) {
                    if (resolved_aggregate.subtypes.empty()) {
                        for (const auto& field_init : expr.field_inits) {
                            if (field_init.value != nullptr) {
                                (void)AnalyzeExpr(*field_init.value);
                            }
                        }
                        return record(resolved_aggregate);
                    }

                    const Type& element_type = resolved_aggregate.subtypes.front();
                    const auto length = mc::support::ParseArrayLength(resolved_aggregate.metadata);
                    if (length.has_value() && expr.field_inits.size() != *length) {
                        Report(expr.span,
                               "array aggregate initializer for type " + FormatType(resolved_aggregate) + " expects " +
                                   std::to_string(*length) + " elements but got " + std::to_string(expr.field_inits.size()));
                    }

                    for (const auto& field_init : expr.field_inits) {
                        if (field_init.value == nullptr) {
                            continue;
                        }
                        const Type value_type = AnalyzeExpr(*field_init.value);
                        if (field_init.has_name) {
                            Report(field_init.span,
                                   "array aggregate initializers do not support named elements for type " +
                                       FormatType(resolved_aggregate));
                            continue;
                        }
                        if (!IsAssignable(element_type, value_type, *module_)) {
                            Report(field_init.span,
                                   "array aggregate element type mismatch: expected " + FormatType(element_type) + ", got " +
                                       FormatType(value_type));
                        }
                    }
                    return record(resolved_aggregate);
                }

                const auto field_source = ResolveAggregateFieldTypes(resolved_aggregate);
                if (!field_source.empty()) {
                    std::unordered_map<std::string, Type> fields;
                    for (const auto& field : field_source) {
                        fields[field.first] = field.second;
                    }
                    for (std::size_t index = 0; index < expr.field_inits.size(); ++index) {
                        const auto& field_init = expr.field_inits[index];
                        const Type value_type = AnalyzeExpr(*field_init.value);
                        if (field_init.has_name) {
                            const auto found = fields.find(field_init.name);
                            if (found == fields.end()) {
                                Report(field_init.span,
                                       "type " + FormatType(resolved_aggregate) + " has no field named " + field_init.name);
                                continue;
                            }
                            if (!IsAssignable(found->second, value_type, *module_)) {
                                Report(field_init.span,
                                       "aggregate field type mismatch for " + field_init.name + ": expected " + FormatType(found->second) +
                                           ", got " + FormatType(value_type));
                            }
                            continue;
                        }
                        if (index >= field_source.size()) {
                            Report(field_init.span,
                                   "too many aggregate initializer values for type " + FormatType(resolved_aggregate));
                            continue;
                        }
                        if (!IsAssignable(field_source[index].second, value_type, *module_)) {
                            Report(field_init.span,
                                   "aggregate field type mismatch for " + field_source[index].first + ": expected " +
                                       FormatType(field_source[index].second) + ", got " + FormatType(value_type));
                        }
                    }
                } else {
                    for (const auto& field_init : expr.field_inits) {
                        (void)AnalyzeExpr(*field_init.value);
                    }
                }
                return record(resolved_aggregate);
            }
            case Expr::Kind::kRecordUpdate: {
                if (expr.left == nullptr) {
                    for (const auto& field_init : expr.field_inits) {
                        if (field_init.value != nullptr) {
                            (void)AnalyzeExpr(*field_init.value);
                        }
                    }
                    Report(expr.span, "record update requires a base value");
                    return record(UnknownType());
                }

                const Type base_type = AnalyzeExpr(*expr.left);
                const Type normalized_base = NormalizeAggregateExpectedType(base_type);
                const auto field_source = ResolveAggregateFieldTypes(base_type);
                if (field_source.empty()) {
                    for (const auto& field_init : expr.field_inits) {
                        if (field_init.value != nullptr) {
                            (void)AnalyzeExpr(*field_init.value);
                        }
                    }
                    if (!IsUnknown(normalized_base)) {
                        Report(expr.span, "record update requires a record-typed base value");
                    }
                    return record(UnknownType());
                }

                std::unordered_map<std::string, Type> fields;
                for (const auto& field : field_source) {
                    fields[field.first] = field.second;
                }

                std::unordered_set<std::string> updated_fields;
                for (const auto& field_init : expr.field_inits) {
                    if (field_init.value == nullptr) {
                        continue;
                    }
                    if (!field_init.has_name) {
                        (void)AnalyzeExpr(*field_init.value);
                        Report(field_init.span, "record update requires named fields");
                        continue;
                    }
                    if (!updated_fields.insert(field_init.name).second) {
                        (void)AnalyzeExpr(*field_init.value);
                        Report(field_init.span, "duplicate record update field: " + field_init.name);
                        continue;
                    }

                    const auto found = fields.find(field_init.name);
                    if (found == fields.end()) {
                        (void)AnalyzeExpr(*field_init.value);
                        Report(field_init.span, "type " + FormatType(normalized_base) + " has no field named " + field_init.name);
                        continue;
                    }

                    ApplyExpectedAggregateType(*field_init.value, found->second);
                    const Type value_type = AnalyzeExpr(*field_init.value);
                    if (!IsAssignable(found->second, value_type, *module_)) {
                        Report(field_init.span,
                               "record update field type mismatch for " + field_init.name + ": expected " +
                                   FormatType(found->second) + ", got " + FormatType(value_type));
                    }
                }
                return record(base_type);
            }
            case Expr::Kind::kParen:
                return record(AnalyzeExpr(*expr.left));
        }

        return record(UnknownType());
    }

    void CheckAssignableNameTarget(std::string_view name, const mc::support::SourceSpan& span, const Type& value_type) {
        const auto binding = LookupValue(std::string(name));
        if (!binding.has_value()) {
            Report(span, "assignment target is not declared: " + std::string(name));
            return;
        }
        if (!binding->is_mutable) {
            Report(span, "assignment target is readonly: " + std::string(name));
            return;
        }
        if (!IsAssignable(binding->type, value_type, *module_)) {
            Report(span,
                   "assignment type mismatch for " + std::string(name) + ": expected " + FormatType(binding->type) + ", got " +
                       FormatType(value_type));
        }
    }

    void CheckAssignableTarget(const Expr& expr, const Type& value_type) {
        if (expr.kind == Expr::Kind::kName) {
            CheckAssignableNameTarget(expr.text, expr.span, value_type);
            return;
        }

        (void)AnalyzeExpr(expr);
    }

    void CheckStmtList(const std::vector<std::unique_ptr<Stmt>>& statements, bool create_scope) {
        if (create_scope) {
            PushScope();
        }
        for (const auto& stmt : statements) {
            CheckStmt(*stmt);
        }
        if (create_scope) {
            PopScope();
        }
    }

    void CheckStmt(const Stmt& stmt) {
        switch (stmt.kind) {
            case Stmt::Kind::kBlock:
                CheckStmtList(stmt.statements, true);
                return;
            case Stmt::Kind::kBinding:
            case Stmt::Kind::kVar:
            case Stmt::Kind::kConst:
                CheckBindingLike(stmt, stmt.kind != Stmt::Kind::kConst);
                return;
            case Stmt::Kind::kBindingOrAssign:
                CheckBindingOrAssign(stmt);
                return;
            case Stmt::Kind::kAssign:
                CheckAssign(stmt);
                return;
            case Stmt::Kind::kExpr:
                if (!stmt.exprs.empty()) {
                    (void)AnalyzeExpr(*stmt.exprs.front());
                }
                return;
            case Stmt::Kind::kIf:
                if (!stmt.exprs.empty()) {
                    const Type condition = AnalyzeExpr(*stmt.exprs.front());
                    if (!IsUnknown(condition) && !IsBoolType(condition, *module_)) {
                        Report(stmt.exprs.front()->span, "if condition must have type bool");
                    }
                }
                if (stmt.then_branch != nullptr) {
                    CheckStmt(*stmt.then_branch);
                }
                if (stmt.else_branch != nullptr) {
                    CheckStmt(*stmt.else_branch);
                }
                return;
            case Stmt::Kind::kSwitch:
                CheckSwitch(stmt);
                return;
            case Stmt::Kind::kWhile:
            case Stmt::Kind::kForCondition:
                CheckConditionLoop(stmt);
                return;
            case Stmt::Kind::kForIn:
                CheckForIn(stmt);
                return;
            case Stmt::Kind::kForEach:
            case Stmt::Kind::kForEachIndex:
                CheckForEach(stmt);
                return;
            case Stmt::Kind::kForRange:
                CheckForRange(stmt);
                return;
            case Stmt::Kind::kReturn:
                CheckReturn(stmt);
                return;
            case Stmt::Kind::kBreak:
                if (loop_depth_ == 0) {
                    Report(stmt.span, "break used outside of a loop");
                }
                return;
            case Stmt::Kind::kContinue:
                if (loop_depth_ == 0) {
                    Report(stmt.span, "continue used outside of a loop");
                }
                return;
            case Stmt::Kind::kDefer:
                if (stmt.exprs.empty() || stmt.exprs.front()->kind != Expr::Kind::kCall) {
                    Report(stmt.span, "defer requires a call expression");
                    return;
                }
                (void)AnalyzeExpr(*stmt.exprs.front());
                return;
        }
    }

    std::optional<std::vector<ExprTypeValue>> AnalyzeExprValuesForArity(const std::vector<std::unique_ptr<Expr>>& exprs,
                                                                        std::size_t expected_count,
                                                                        const mc::support::SourceSpan& report_span,
                                                                        std::string_view count_error) {
        auto analyze_direct = [&]() {
            std::vector<ExprTypeValue> values;
            values.reserve(exprs.size());
            for (const auto& expr : exprs) {
                values.push_back({
                    .type = AnalyzeExpr(*expr),
                    .span = expr->span,
                });
            }
            return values;
        };

        if (exprs.size() == expected_count) {
            return analyze_direct();
        }

        std::vector<ExprTypeValue> flattened;
        flattened.reserve(expected_count);
        for (const auto& expr : exprs) {
            const Type type = AnalyzeExpr(*expr);
            if (type.kind == Type::Kind::kTuple) {
                for (const auto& element_type : type.subtypes) {
                    flattened.push_back({
                        .type = element_type,
                        .span = expr->span,
                    });
                }
            } else {
                flattened.push_back({
                    .type = type,
                    .span = expr->span,
                });
            }
        }

        if (flattened.size() != expected_count) {
            Report(report_span, std::string(count_error));
            return std::nullopt;
        }

        return flattened;
    }

    void CheckBindingLike(const Stmt& stmt, bool is_mutable) {
        ValidateTypeExpr(stmt.type_ann.get(), current_function_ != nullptr ? current_function_->type_params : std::vector<std::string> {}, stmt.span);
        const Type declared_type = SemanticTypeFromAst(stmt.type_ann.get(),
                                   current_function_ != nullptr ? current_function_->type_params
                                                : std::vector<std::string> {});

        if (stmt.has_initializer && stmt.exprs.size() == stmt.pattern.names.size() && !IsUnknown(declared_type)) {
            for (const auto& expr : stmt.exprs) {
                ApplyExpectedAggregateType(*expr, declared_type);
            }
        }

        std::vector<Type> value_types;
        value_types.reserve(stmt.pattern.names.size());
        if (stmt.has_initializer) {
            const auto values = AnalyzeExprValuesForArity(stmt.exprs,
                                                          stmt.pattern.names.size(),
                                                          stmt.span,
                                                          "binding requires one initializer per bound name");
            if (!values.has_value()) {
                return;
            }
            for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
                const Type& value_type = (*values)[index].type;
                if (!IsUnknown(declared_type) && !IsAssignable(declared_type, value_type, *module_)) {
                    Report((*values)[index].span,
                           "binding type mismatch for " + stmt.pattern.names[index] + ": expected " +
                               FormatType(declared_type) + ", got " + FormatType(value_type));
                }
                value_types.push_back(value_type);
            }
        } else {
            for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
                value_types.push_back(declared_type);
            }
        }

        for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
            BindValue(stmt.pattern.names[index], IsUnknown(declared_type) ? value_types[index] : declared_type, is_mutable, stmt.span);
        }
    }

    void CheckBindingOrAssign(const Stmt& stmt) {
        std::vector<BindingOrAssignResolution> resolutions;
        resolutions.reserve(stmt.pattern.names.size());
        for (const auto& name : stmt.pattern.names) {
            resolutions.push_back(LookupValue(name).has_value() ? BindingOrAssignResolution::kAssign : BindingOrAssignResolution::kBind);
        }

        if (stmt.exprs.size() == stmt.pattern.names.size()) {
            for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
                if (resolutions[index] != BindingOrAssignResolution::kAssign) {
                    continue;
                }
                const auto binding = LookupValue(stmt.pattern.names[index]);
                if (binding.has_value()) {
                    ApplyExpectedAggregateType(*stmt.exprs[index], binding->type);
                }
            }
        }

        const auto values = AnalyzeExprValuesForArity(stmt.exprs,
                                                      stmt.pattern.names.size(),
                                                      stmt.span,
                                                      "binding-or-assignment requires one value per name");
        if (!values.has_value()) {
            return;
        }

        if (HasDuplicateBindingOrAssignBindNames(stmt, resolutions)) {
            return;
        }

        RecordBindingOrAssignFact(stmt, resolutions);

        for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
            if (resolutions[index] == BindingOrAssignResolution::kAssign) {
                CheckAssignableNameTarget(stmt.pattern.names[index], stmt.span, (*values)[index].type);
                continue;
            }
            BindValue(stmt.pattern.names[index], (*values)[index].type, true, stmt.span);
        }
    }

    void CheckAssign(const Stmt& stmt) {
        if (stmt.assign_values.size() == stmt.assign_targets.size()) {
            for (std::size_t index = 0; index < stmt.assign_targets.size(); ++index) {
                ApplyExpectedAggregateType(*stmt.assign_values[index], AnalyzeExpr(*stmt.assign_targets[index]));
            }
        }

        const auto values = AnalyzeExprValuesForArity(stmt.assign_values,
                                                      stmt.assign_targets.size(),
                                                      stmt.span,
                                                      "assignment requires one value per target");
        if (!values.has_value()) {
            return;
        }
        for (std::size_t index = 0; index < stmt.assign_targets.size(); ++index) {
            CheckAssignableTarget(*stmt.assign_targets[index], (*values)[index].type);
        }
    }

    void CheckSwitch(const Stmt& stmt) {
        const Type selector = stmt.exprs.empty() ? UnknownType() : AnalyzeExpr(*stmt.exprs.front());
        for (const auto& switch_case : stmt.switch_cases) {
            PushScope();
            if (switch_case.pattern.kind == mc::ast::CasePattern::Kind::kExpr && switch_case.pattern.expr != nullptr) {
                const Type pattern_type = AnalyzeExpr(*switch_case.pattern.expr);
                if (!IsUnknown(selector) && !IsUnknown(pattern_type) && !IsAssignable(selector, pattern_type, *module_) &&
                    !IsAssignable(pattern_type, selector, *module_)) {
                    Report(switch_case.pattern.span, "switch case pattern type does not match selector type");
                }
            } else if (switch_case.pattern.kind == mc::ast::CasePattern::Kind::kVariant) {
                const auto variant = LookupVariant(selector, switch_case.pattern.variant_name);
                if (!variant.has_value()) {
                    Report(switch_case.pattern.span, "unknown switch case variant: " + switch_case.pattern.variant_name);
                } else {
                    if (switch_case.pattern.bindings.size() != variant->payload_fields.size()) {
                        Report(switch_case.pattern.span,
                               "switch case variant binding count mismatch for " + switch_case.pattern.variant_name + ": expected " +
                                   std::to_string(variant->payload_fields.size()) + ", got " + std::to_string(switch_case.pattern.bindings.size()));
                    }
                    const std::size_t bind_count = std::min(switch_case.pattern.bindings.size(), variant->payload_fields.size());
                    for (std::size_t index = 0; index < bind_count; ++index) {
                        BindValue(switch_case.pattern.bindings[index], variant->payload_fields[index].second, false, switch_case.pattern.span);
                    }
                }
            }
            CheckStmtList(switch_case.statements, false);
            PopScope();
        }
        if (stmt.has_default_case) {
            CheckStmtList(stmt.default_case.statements, true);
        }
    }

    void CheckConditionLoop(const Stmt& stmt) {
        if (!stmt.exprs.empty()) {
            const Type condition = AnalyzeExpr(*stmt.exprs.front());
            if (!IsUnknown(condition) && !IsBoolType(condition, *module_)) {
                Report(stmt.exprs.front()->span, "loop condition must have type bool");
            }
        }
        ++loop_depth_;
        if (stmt.then_branch != nullptr) {
            CheckStmt(*stmt.then_branch);
        }
        --loop_depth_;
    }

    void CheckForIn(const Stmt& stmt) {
        if (stmt.exprs.empty()) {
            return;
        }

        const Type iterable = AnalyzeExpr(*stmt.exprs.front());
        const bool is_range = iterable.kind == Type::Kind::kRange;
        RecordForInFact(stmt, is_range ? ForInResolution::kForRange : ForInResolution::kForEach);

        Type element_type = UnknownType();
        if (is_range && !iterable.subtypes.empty()) {
            element_type = iterable.subtypes.front();
        }
        if (!is_range && iterable.kind == Type::Kind::kArray && !iterable.subtypes.empty()) {
            element_type = iterable.subtypes.front();
        }
        if (!is_range && iterable.kind == Type::Kind::kNamed && (iterable.name == "Slice" || iterable.name == "Buffer") &&
            !iterable.subtypes.empty()) {
            element_type = iterable.subtypes.front();
        }

        ++loop_depth_;
        PushScope();
        BindValue(stmt.loop_name, element_type, true, stmt.span);
        if (stmt.then_branch != nullptr) {
            CheckStmt(*stmt.then_branch);
        }
        PopScope();
        --loop_depth_;
    }

    void CheckForEach(const Stmt& stmt) {
        if (stmt.exprs.empty()) {
            return;
        }
        const Type iterable = AnalyzeExpr(*stmt.exprs.front());
        Type element_type = UnknownType();
        if (iterable.kind == Type::Kind::kArray && !iterable.subtypes.empty()) {
            element_type = iterable.subtypes.front();
        }
        if (iterable.kind == Type::Kind::kNamed && (iterable.name == "Slice" || iterable.name == "Buffer") && !iterable.subtypes.empty()) {
            element_type = iterable.subtypes.front();
        }

        ++loop_depth_;
        PushScope();
        if (stmt.kind == Stmt::Kind::kForEachIndex) {
            BindValue(stmt.loop_name, NamedType("usize"), true, stmt.span);
            BindValue(stmt.loop_second_name, element_type, true, stmt.span);
        } else {
            BindValue(stmt.loop_name, element_type, true, stmt.span);
        }
        if (stmt.then_branch != nullptr) {
            CheckStmt(*stmt.then_branch);
        }
        PopScope();
        --loop_depth_;
    }

    void CheckForRange(const Stmt& stmt) {
        if (stmt.exprs.empty()) {
            return;
        }
        const Type range_type = AnalyzeExpr(*stmt.exprs.front());
        Type element_type = UnknownType();
        if (range_type.kind == Type::Kind::kRange && !range_type.subtypes.empty()) {
            element_type = range_type.subtypes.front();
        }
        ++loop_depth_;
        PushScope();
        BindValue(stmt.loop_name, element_type, true, stmt.span);
        if (stmt.then_branch != nullptr) {
            CheckStmt(*stmt.then_branch);
        }
        PopScope();
        --loop_depth_;
    }

    void CheckReturn(const Stmt& stmt) {
        if (current_function_ == nullptr) {
            return;
        }
        if (stmt.exprs.size() == current_function_->return_types.size()) {
            for (std::size_t index = 0; index < stmt.exprs.size(); ++index) {
                ApplyExpectedAggregateType(*stmt.exprs[index], current_function_->return_types[index]);
            }
        }
        const auto values = AnalyzeExprValuesForArity(stmt.exprs,
                                                      current_function_->return_types.size(),
                                                      stmt.span,
                                                      "return value count does not match function signature");
        if (!values.has_value()) {
            return;
        }
        for (std::size_t index = 0; index < current_function_->return_types.size(); ++index) {
            const Type& actual = (*values)[index].type;
            if (!IsAssignable(current_function_->return_types[index], actual, *module_)) {
                Report((*values)[index].span,
                       "return type mismatch: expected " + FormatType(current_function_->return_types[index]) + ", got " +
                           FormatType(actual));
            }
        }
    }

    const ast::SourceFile& source_file_;
    std::filesystem::path file_path_;
    std::optional<std::string> current_package_identity_;
    const ImportedModules* imported_modules_;
    support::DiagnosticSink& diagnostics_;
    std::vector<std::filesystem::path> module_part_paths_;
    std::size_t module_part_line_stride_ = 0;
    Module* module_ = nullptr;
    std::unordered_map<std::string, Decl::Kind> value_symbols_;
    std::unordered_map<std::string, Decl::Kind> type_symbols_;
    std::unordered_map<std::string, ValueBinding> global_symbols_;
    std::unordered_map<std::string, const FunctionSignature*> checker_function_lookup_;
    std::unordered_map<std::string, const TypeDeclSummary*> checker_type_decl_lookup_;
    std::unordered_map<std::string, const GlobalSummary*> checker_global_lookup_;
    std::unordered_map<std::string, ConstValue> const_eval_cache_;
    std::unordered_set<std::string> private_value_names_;
    std::unordered_set<std::string> private_type_names_;
    std::vector<std::unordered_map<std::string, ValueBinding>> scopes_;
    const FunctionSignature* current_function_ = nullptr;
    int loop_depth_ = 0;
};

std::unique_ptr<ast::SourceFile> ParseFile(const std::filesystem::path& path, support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        diagnostics.Report({
            .file_path = path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "unable to read source file",
        });
        return nullptr;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const auto lexed = mc::lex::Lex(buffer.str(), path.generic_string(), diagnostics);
    auto parsed = mc::parse::Parse(lexed, path, diagnostics);
    if (!parsed.ok) {
        return nullptr;
    }
    return std::move(parsed.source_file);
}

CheckResult CheckProgramInternal(const ast::SourceFile& source_file,
                                 const std::filesystem::path& file_path,
                                 const CheckOptions& options,
                                 support::DiagnosticSink& diagnostics,
                                 std::unordered_map<std::filesystem::path, Module>& visible_cache,
                                 std::unordered_map<std::filesystem::path, VisitState>& visit_state) {
    const std::filesystem::path normalized_path = std::filesystem::absolute(file_path).lexically_normal();
    const auto cached = visible_cache.find(normalized_path);
    if (cached != visible_cache.end()) {
        return {
            .module = std::make_unique<Module>(cached->second),
            .ok = !diagnostics.HasErrors(),
        };
    }

    if (source_file.module_kind == ast::SourceFile::ModuleKind::kInternal && !options.current_package_identity.has_value()) {
        diagnostics.Report({
            .file_path = normalized_path,
            .span = source_file.span,
            .severity = DiagnosticSeverity::kError,
            .message = "direct source mode does not support internal module roots",
        });
        return {
            .module = std::make_unique<Module>(),
            .ok = false,
        };
    }

    const auto state = visit_state.find(normalized_path);
    if (state != visit_state.end() && state->second == VisitState::kVisiting) {
        diagnostics.Report({
            .file_path = normalized_path,
            .span = source_file.span,
            .severity = DiagnosticSeverity::kError,
            .message = "import cycle detected involving module: " + normalized_path.filename().replace_extension().string(),
        });
        return {
            .module = std::make_unique<Module>(),
            .ok = false,
        };
    }

    visit_state[normalized_path] = VisitState::kVisiting;

    ImportedModules imported_modules;
    if (options.imported_modules != nullptr) {
        for (const auto& import_decl : source_file.imports) {
            const auto found = options.imported_modules->find(import_decl.module_name);
            if (found == options.imported_modules->end()) {
                continue;
            }
            imported_modules[import_decl.module_name] = RewriteImportedModuleSurfaceTypes(found->second, import_decl.module_name);
        }
    } else {
        // Bootstrap direct-source checking still owns the fallback filesystem
        // import resolution path. Project-mode callers can bypass it entirely
        // by supplying options.imported_modules with already-checked surfaces.
        for (const auto& import_decl : source_file.imports) {
            const auto resolved_import = ResolveImportPathForCheck(normalized_path,
                                                                   import_decl.module_name,
                                                                   options.import_roots,
                                                                   options.current_package_identity,
                                                                   options.package_identity_for_source,
                                                                   diagnostics,
                                                                   import_decl.span);
            if (!resolved_import.has_value()) {
                continue;
            }

            const auto import_state = visit_state.find(resolved_import->path);
            if (import_state != visit_state.end() && import_state->second == VisitState::kVisiting) {
                diagnostics.Report({
                    .file_path = normalized_path,
                    .span = import_decl.span,
                    .severity = DiagnosticSeverity::kError,
                    .message = "import cycle detected involving module: " + import_decl.module_name,
                });
                continue;
            }

            Module visible_module;
            const auto found_visible = visible_cache.find(resolved_import->path);
            if (found_visible != visible_cache.end()) {
                visible_module = found_visible->second;
            } else {
                auto parsed_import = ParseFile(resolved_import->path, diagnostics);
                if (parsed_import == nullptr) {
                    continue;
                }
                CheckOptions child_options = options;
                child_options.current_package_identity = resolved_import->package_identity;
                const auto checked_import = CheckProgramInternal(*parsed_import,
                                                                 resolved_import->path,
                                                                 child_options,
                                                                 diagnostics,
                                                                 visible_cache,
                                                                 visit_state);
                if (checked_import.module != nullptr) {
                    visible_module = BuildImportVisibleModuleSurface(*checked_import.module, *parsed_import);
                }
            }

            imported_modules[import_decl.module_name] = RewriteImportedModuleSurfaceTypes(visible_module, import_decl.module_name);
        }
    }

    const ImportedModules* imported_modules_ptr = imported_modules.empty() ? nullptr : &imported_modules;
    auto checked = Checker(source_file, normalized_path, options.current_package_identity, imported_modules_ptr, diagnostics,
                           options.module_part_paths, options.module_part_line_stride).Run();
    Module visible_module = checked.module != nullptr ? BuildImportVisibleModuleSurface(*checked.module, source_file) : Module {};
    visible_cache[normalized_path] = visible_module;
    visit_state[normalized_path] = VisitState::kDone;
    return checked;
}

}  // namespace

CheckResult CheckProgram(const ast::SourceFile& source_file,
                         const std::filesystem::path& file_path,
                         const CheckOptions& options,
                         support::DiagnosticSink& diagnostics) {
    std::unordered_map<std::filesystem::path, Module> visible_cache;
    std::unordered_map<std::filesystem::path, VisitState> visit_state;
    return CheckProgramInternal(source_file, file_path, options, diagnostics, visible_cache, visit_state);
}

CheckResult CheckProgram(const ast::SourceFile& source_file,
                         const std::filesystem::path& file_path,
                         support::DiagnosticSink& diagnostics) {
    return CheckProgram(source_file, file_path, CheckOptions {}, diagnostics);
}

CheckResult CheckSourceFile(const ast::SourceFile& source_file,
                            const std::filesystem::path& file_path,
                            support::DiagnosticSink& diagnostics) {
    return Checker(source_file, file_path, std::nullopt, nullptr, diagnostics).Run();
}

const FunctionSignature* FindFunctionSignature(const Module& module, std::string_view name) {
    for (const auto& function : module.functions) {
        if (function.name == name) {
            return &function;
        }
    }
    return nullptr;
}

const TypeDeclSummary* FindTypeDecl(const Module& module, std::string_view name) {
    for (const auto& type_decl : module.type_decls) {
        if (type_decl.name == name) {
            return &type_decl;
        }
    }
    return nullptr;
}

const GlobalSummary* FindGlobalSummary(const Module& module, std::string_view name) {
    for (const auto& global : module.globals) {
        for (const auto& global_name : global.names) {
            if (global_name == name) {
                return &global;
            }
        }
    }
    return nullptr;
}

const Type* FindExprType(const Module& module, const ast::Expr& expr) {
    const auto found = module.expr_types.find(expr.span);
    if (found != module.expr_types.end()) {
        return &found->second;
    }
    return nullptr;
}

const BindingOrAssignFact* FindBindingOrAssignFact(const Module& module, const ast::Stmt& stmt) {
    const auto found = module.binding_or_assign_facts.find(stmt.span);
    if (found != module.binding_or_assign_facts.end()) {
        return &found->second;
    }
    return nullptr;
}

const ForInFact* FindForInFact(const Module& module, const ast::Stmt& stmt) {
    const auto found = module.for_in_facts.find(stmt.span);
    if (found != module.for_in_facts.end()) {
        return &found->second;
    }
    return nullptr;
}

void BuildModuleLookupMaps(Module& module) {
    module.function_lookup.clear();
    module.function_lookup.reserve(module.functions.size());
    for (std::size_t i = 0; i < module.functions.size(); ++i) {
        module.function_lookup.emplace(module.functions[i].name, i);
    }
    module.type_decl_lookup.clear();
    module.type_decl_lookup.reserve(module.type_decls.size());
    for (std::size_t i = 0; i < module.type_decls.size(); ++i) {
        module.type_decl_lookup.emplace(module.type_decls[i].name, i);
    }
}

std::string DumpModule(const Module& module) {
    std::ostringstream stream;
    WriteLine(stream, 0, "SemanticModule");
    for (const auto& import_name : module.imports) {
        WriteLine(stream, 1, "Import name=" + import_name);
    }
    for (const auto& type_decl : module.type_decls) {
        WriteLine(stream, 1, "TypeDecl name=" + type_decl.name + " kind=" + std::string(ToString(type_decl.kind)));
        if (!type_decl.type_params.empty()) {
            std::ostringstream params;
            params << "typeParams=[";
            for (std::size_t index = 0; index < type_decl.type_params.size(); ++index) {
                if (index > 0) {
                    params << ", ";
                }
                params << type_decl.type_params[index];
            }
            params << ']';
            WriteLine(stream, 2, params.str());
        }
        if (!type_decl.attributes.empty()) {
            std::ostringstream attrs;
            attrs << "attributes=[";
            for (std::size_t index = 0; index < type_decl.attributes.size(); ++index) {
                if (index > 0) {
                    attrs << ", ";
                }
                attrs << type_decl.attributes[index].name;
                if (!type_decl.attributes[index].args.empty()) {
                    attrs << '(';
                    for (std::size_t arg_index = 0; arg_index < type_decl.attributes[index].args.size(); ++arg_index) {
                        if (arg_index > 0) {
                            attrs << ", ";
                        }
                        attrs << type_decl.attributes[index].args[arg_index].value;
                    }
                    attrs << ')';
                }
            }
            attrs << ']';
            WriteLine(stream, 2, attrs.str());
        }
        for (const auto& field : type_decl.fields) {
            WriteLine(stream, 2, "Field name=" + field.first + " type=" + FormatType(field.second));
        }
        for (const auto& variant : type_decl.variants) {
            WriteLine(stream, 2, "Variant name=" + variant.name);
            for (const auto& payload_field : variant.payload_fields) {
                WriteLine(stream, 3, "PayloadField name=" + payload_field.first + " type=" + FormatType(payload_field.second));
            }
        }
        if (!IsUnknown(type_decl.aliased_type)) {
            WriteLine(stream, 2, "AliasedType=" + FormatType(type_decl.aliased_type));
        }
        if (type_decl.layout.valid) {
            std::ostringstream layout;
            layout << "Layout size=" << type_decl.layout.size << " align=" << type_decl.layout.alignment << " offsets=[";
            for (std::size_t index = 0; index < type_decl.layout.field_offsets.size(); ++index) {
                if (index > 0) {
                    layout << ", ";
                }
                layout << type_decl.layout.field_offsets[index];
            }
            layout << ']';
            WriteLine(stream, 2, layout.str());
        }
    }
    for (const auto& global : module.globals) {
        std::ostringstream line;
        line << (global.is_const ? "Const" : "Var") << " names=[";
        for (std::size_t index = 0; index < global.names.size(); ++index) {
            if (index > 0) {
                line << ", ";
            }
            line << global.names[index];
        }
        line << "] type=" << FormatType(global.type);
        bool has_any_value = false;
        for (std::size_t index = 0; index < global.names.size(); ++index) {
            const bool has_value = index < global.constant_values.size() && global.constant_values[index].has_value();
            const bool zero_init = index < global.zero_initialized_values.size() && global.zero_initialized_values[index];
            if (has_value || zero_init) {
                has_any_value = true;
                break;
            }
        }
        if (has_any_value) {
            line << " values=[";
            for (std::size_t index = 0; index < global.names.size(); ++index) {
                if (index > 0) {
                    line << ", ";
                }
                if (index < global.zero_initialized_values.size() && global.zero_initialized_values[index]) {
                    line << "zero";
                } else if (index < global.constant_values.size() && global.constant_values[index].has_value()) {
                    line << RenderConstValue(*global.constant_values[index]);
                } else {
                    line << "?";
                }
            }
            line << ']';
        }
        WriteLine(stream, 1, line.str());
    }
    for (const auto& function : module.functions) {
        std::ostringstream line;
        line << "Function name=" << function.name;
        if (function.is_extern) {
            line << " extern=" << function.extern_abi;
        }
        line << " params=[";
        for (std::size_t index = 0; index < function.param_types.size(); ++index) {
            if (index > 0) {
                line << ", ";
            }
            line << FormatType(function.param_types[index]);
        }
        line << "] returns=[";
        for (std::size_t index = 0; index < function.return_types.size(); ++index) {
            if (index > 0) {
                line << ", ";
            }
            line << FormatType(function.return_types[index]);
        }
        line << ']';
        bool has_any_noalias = false;
        for (bool value : function.param_is_noalias) {
            if (value) {
                has_any_noalias = true;
                break;
            }
        }
        if (has_any_noalias) {
            line << " paramAttrs=[";
            for (std::size_t index = 0; index < function.param_is_noalias.size(); ++index) {
                if (index > 0) {
                    line << ", ";
                }
                line << (function.param_is_noalias[index] ? "noalias" : "-");
            }
            line << ']';
        }
        WriteLine(stream, 1, line.str());
    }
    return stream.str();
}

// Evaluate a simple array-length expression using pre-computed constant values
// stored in the module's global summaries.  Handles integer literals and plain
// name references to module-scoped const declarations.  Returns nullopt for
// any expression it cannot reduce to a non-negative integer.
static std::optional<std::size_t> EvalArrayLengthFromModule(
    const ast::Expr* length_expr, const Module& module) {
    if (length_expr == nullptr) {
        return std::nullopt;
    }
    if (length_expr->kind == ast::Expr::Kind::kLiteral) {
        return mc::support::ParseArrayLength(length_expr->text);
    }
    if (length_expr->kind == ast::Expr::Kind::kName) {
        const GlobalSummary* g = FindGlobalSummary(module, length_expr->text);
        if (g == nullptr || !g->is_const) {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < g->names.size(); ++i) {
            if (g->names[i] == length_expr->text &&
                i < g->constant_values.size() &&
                g->constant_values[i].has_value() &&
                g->constant_values[i]->kind == ConstValue::Kind::kInteger &&
                g->constant_values[i]->integer_value >= 0) {
                return static_cast<std::size_t>(g->constant_values[i]->integer_value);
            }
        }
    }
    return std::nullopt;
}

Type ResolveTypeFromAst(const ast::TypeExpr* type_expr, const Module& module) {
    if (type_expr == nullptr ||
        type_expr->kind != ast::TypeExpr::Kind::kArray) {
        return TypeFromAst(type_expr);
    }
    // Recursively resolve the element type first.
    Type inner = ResolveTypeFromAst(type_expr->inner.get(), module);
    // Get base type from TypeFromAst to pick up the length text produced by
    // RenderExprInline (which is file-local to type.cpp).
    Type base = TypeFromAst(type_expr);
    // If the metadata is not already a resolved integer, try to evaluate it
    // from the module's const globals.
    if (!mc::support::ParseArrayLength(base.metadata).has_value() &&
        type_expr->length_expr != nullptr) {
        const auto len = EvalArrayLengthFromModule(
            type_expr->length_expr.get(), module);
        if (len.has_value()) {
            base.metadata = std::to_string(*len);
        }
    }
    // Replace the inner type with the recursively resolved version.
    if (!base.subtypes.empty()) {
        base.subtypes[0] = std::move(inner);
    }
    return base;
}

}  // namespace mc::sema