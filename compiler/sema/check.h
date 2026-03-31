#ifndef C_MODERN_COMPILER_SEMA_CHECK_H_
#define C_MODERN_COMPILER_SEMA_CHECK_H_

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "compiler/ast/ast.h"
#include "compiler/sema/type.h"
#include "compiler/support/diagnostics.h"

namespace mc::sema {

struct FunctionSignature {
    std::string name;
    bool is_extern = false;
    std::string extern_abi;
    std::vector<std::string> type_params;
    std::vector<Type> param_types;
    std::vector<Type> return_types;
};

struct LayoutInfo {
    bool valid = false;
    std::size_t size = 0;
    std::size_t alignment = 0;
    std::vector<std::size_t> field_offsets;
};

struct VariantSummary {
    std::string name;
    std::vector<std::pair<std::string, Type>> payload_fields;
};

struct TypeDeclSummary {
    ast::Decl::Kind kind = ast::Decl::Kind::kStruct;
    std::string name;
    std::vector<std::string> type_params;
    std::vector<ast::Attribute> attributes;
    std::vector<std::pair<std::string, Type>> fields;
    std::vector<VariantSummary> variants;
    Type aliased_type;
    bool is_packed = false;
    bool is_abi_c = false;
    LayoutInfo layout;
};

struct GlobalSummary {
    bool is_const = false;
    std::vector<std::string> names;
    Type type;
};

struct ExprTypeFact {
    support::SourceSpan span {};
    Type type;
};

struct Module {
    std::vector<std::string> imports;
    std::vector<TypeDeclSummary> type_decls;
    std::vector<GlobalSummary> globals;
    std::vector<FunctionSignature> functions;
    std::vector<ExprTypeFact> expr_types;
};

struct CheckResult {
    std::unique_ptr<Module> module;
    bool ok = false;
};

struct CheckOptions {
    std::vector<std::filesystem::path> import_roots;
};

CheckResult CheckProgram(const ast::SourceFile& source_file,
                         const std::filesystem::path& file_path,
                         const CheckOptions& options,
                         support::DiagnosticSink& diagnostics);

CheckResult CheckProgram(const ast::SourceFile& source_file,
                         const std::filesystem::path& file_path,
                         support::DiagnosticSink& diagnostics);

CheckResult CheckSourceFile(const ast::SourceFile& source_file,
                            const std::filesystem::path& file_path,
                            support::DiagnosticSink& diagnostics);

const FunctionSignature* FindFunctionSignature(const Module& module, std::string_view name);
const TypeDeclSummary* FindTypeDecl(const Module& module, std::string_view name);
const GlobalSummary* FindGlobalSummary(const Module& module, std::string_view name);
const Type* FindExprType(const Module& module, const ast::Expr& expr);
std::string DumpModule(const Module& module);

}  // namespace mc::sema

#endif  // C_MODERN_COMPILER_SEMA_CHECK_H_