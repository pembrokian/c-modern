#ifndef C_MODERN_COMPILER_SEMA_CHECK_INTERNAL_H_
#define C_MODERN_COMPILER_SEMA_CHECK_INTERNAL_H_

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "compiler/sema/check.h"

namespace mc::sema {

using ImportedModules = std::unordered_map<std::string, Module>;

struct ValueBinding {
    Type type;
    bool is_mutable = true;
};

class TopLevelSummaryBuilder {
  public:
    struct Context {
        const ast::SourceFile& source_file;
        const ImportedModules* imported_modules;
        Module& module;
        std::unordered_map<std::string, ast::Decl::Kind>& value_symbols;
        std::unordered_map<std::string, ast::Decl::Kind>& type_symbols;
        std::unordered_map<std::string, ValueBinding>& global_symbols;
        std::function<void(const mc::support::SourceSpan&, const std::string&)> report;
        std::function<const Module*(std::string_view)> find_imported_module;
        std::function<FunctionSignature(const ast::Decl&)> build_function_signature;
        std::function<TypeDeclSummary(const ast::Decl&)> build_type_decl_summary;
        std::function<Type(const ast::TypeExpr*, const std::vector<std::string>&)> semantic_type_from_ast;
    };

    explicit TopLevelSummaryBuilder(const Context& context) : context_(context) {}

    void SeedImportedSymbols();
    void CollectTopLevelDecls();

  private:
    Context context_;
};

class ConstExprEvaluator {
  public:
    struct Context {
        mc::support::SourceSpan source_span;
        const Module& module;
        std::unordered_map<std::string, ConstValue>& const_eval_cache;
        std::function<void(const mc::support::SourceSpan&, const std::string&)> report;
        std::function<std::optional<std::pair<const ast::Decl*, std::size_t>>(std::string_view, ast::Decl::Kind)>
            find_top_level_binding_decl;
        std::function<const Module*(std::string_view)> find_imported_module;
        std::function<void(const ast::TypeExpr*, const std::vector<std::string>&, const mc::support::SourceSpan&)>
            validate_type_expr;
        std::function<std::vector<std::string>()> current_type_params;
        std::function<Type(const ast::TypeExpr*, const std::vector<std::string>&)> semantic_type_from_ast;
        std::function<Type(const ast::Expr&)> resolve_aggregate_init_type;
        std::function<void(ast::Expr&, const Type&)> apply_nested_aggregate_type_hints;
        std::function<std::optional<ConstValue>(Type, const ConstValue&)> convert_const_value;
        std::function<const TypeDeclSummary*(std::string_view)> lookup_local_type_decl;
        std::function<const TypeDeclSummary*(const Type&)> lookup_struct_type;
        std::function<std::string(const ast::Expr&)> combine_qualified_name;
    };

    explicit ConstExprEvaluator(const Context& context) : context_(context) {}

    std::optional<ConstValue> EvaluateConstExpr(const ast::Expr& expr,
                                                bool report_errors,
                                                std::unordered_set<std::string>& active_names);
    std::optional<std::size_t> EvaluateArrayLength(const ast::Expr* expr, bool report_errors);

  private:
    struct EnumConstDesignator {
        Type enum_type;
        std::string variant_name;
        std::vector<std::pair<std::string, Type>> payload_fields;
        std::size_t variant_index = 0;
    };

    std::optional<ConstValue> EvaluateTopLevelConst(std::string_view name,
                                                    bool report_errors,
                                                    std::unordered_set<std::string>& active_names);
    std::optional<ConstValue> EvaluateConstBinaryExpr(const ast::Expr& expr,
                                                      bool report_errors,
                                                      std::unordered_set<std::string>& active_names);
    std::optional<EnumConstDesignator> ResolveEnumConstDesignator(const ast::Expr& expr, bool report_errors);
    std::optional<ConstValue> EvaluateEnumConst(const ast::Expr& designator,
                                                const std::vector<std::unique_ptr<ast::Expr>>* payload_args,
                                                bool report_errors,
                                                std::unordered_set<std::string>& active_names);

    Context context_;
};

}  // namespace mc::sema

#endif  // C_MODERN_COMPILER_SEMA_CHECK_INTERNAL_H_