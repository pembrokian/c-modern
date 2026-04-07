#ifndef C_MODERN_COMPILER_SEMA_CHECK_H_
#define C_MODERN_COMPILER_SEMA_CHECK_H_

#include <filesystem>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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
    std::vector<bool> param_is_noalias;
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

struct ConstValue {
    enum class Kind {
        kBool,
        kInteger,
        kFloat,
        kString,
        kNil,
        kAggregate,
    };

    Kind kind = Kind::kInteger;
    std::int64_t integer_value = 0;
    double float_value = 0.0;
    bool bool_value = false;
    std::string text;
    std::vector<std::string> field_names;
    std::vector<ConstValue> elements;
};

struct GlobalSummary {
    bool is_const = false;
    std::vector<std::string> names;
    Type type;
    std::vector<std::optional<ConstValue>> constant_values;
    std::vector<bool> zero_initialized_values;
};

struct ExprTypeFact {
    support::SourceSpan span {};
    Type type;
};

enum class BindingOrAssignResolution {
    kBind,
    kAssign,
};

struct BindingOrAssignFact {
    support::SourceSpan span {};
    std::vector<BindingOrAssignResolution> resolutions;
};

enum class ForInResolution {
    kForEach,
    kForRange,
};

struct ForInFact {
    support::SourceSpan span {};
    ForInResolution resolution = ForInResolution::kForEach;
};

struct SourceSpanHash {
    std::size_t operator()(const support::SourceSpan& span) const noexcept {
        std::size_t hash = 1469598103934665603ull;
        const auto mix = [&](std::size_t value) {
            hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
        };
        mix(span.begin.line);
        mix(span.begin.column);
        mix(span.end.line);
        mix(span.end.column);
        return hash;
    }
};

struct SourceSpanEqual {
    bool operator()(const support::SourceSpan& left, const support::SourceSpan& right) const noexcept {
        return left.begin.line == right.begin.line && left.begin.column == right.begin.column && left.end.line == right.end.line &&
               left.end.column == right.end.column;
    }
};

struct Module {
    ast::SourceFile::ModuleKind module_kind = ast::SourceFile::ModuleKind::kOrdinary;
    std::string package_identity;
    std::vector<std::string> imports;
    std::vector<TypeDeclSummary> type_decls;
    std::vector<GlobalSummary> globals;
    std::vector<FunctionSignature> functions;
    std::unordered_set<std::string> hidden_value_names;
    std::unordered_set<std::string> hidden_type_names;
    std::unordered_map<support::SourceSpan, Type, SourceSpanHash, SourceSpanEqual> expr_types;
    std::unordered_map<support::SourceSpan, BindingOrAssignFact, SourceSpanHash, SourceSpanEqual> binding_or_assign_facts;
    std::unordered_map<support::SourceSpan, ForInFact, SourceSpanHash, SourceSpanEqual> for_in_facts;
    // Optional name -> index caches for functions and type declarations.
    // BuildModuleLookupMaps() refreshes them after bulk population, but
    // semantic lookups remain correct even if callers mutate the vectors
    // without rebuilding the caches immediately.
    std::unordered_map<std::string, std::size_t> function_lookup;
    std::unordered_map<std::string, std::size_t> type_decl_lookup;
};

struct CheckResult {
    std::unique_ptr<Module> module;
    bool ok = false;
};

struct CheckOptions {
    std::vector<std::filesystem::path> import_roots;
    const std::unordered_map<std::string, Module>* imported_modules = nullptr;
    std::optional<std::string> current_package_identity;
    std::function<std::optional<std::string>(const std::filesystem::path&)> package_identity_for_source;
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

Module BuildImportVisibleModuleSurface(const Module& module,
                                       const ast::SourceFile& source_file);

Module RewriteImportedModuleSurfaceTypes(const Module& module,
                                         std::string_view module_name);

Type SubstituteTypeParams(Type type,
                          const std::vector<std::string>& type_params,
                          const std::vector<Type>& type_args);

const FunctionSignature* FindFunctionSignature(const Module& module, std::string_view name);
const TypeDeclSummary* FindTypeDecl(const Module& module, std::string_view name);
const GlobalSummary* FindGlobalSummary(const Module& module, std::string_view name);
const Type* FindExprType(const Module& module, const ast::Expr& expr);
const BindingOrAssignFact* FindBindingOrAssignFact(const Module& module, const ast::Stmt& stmt);
const ForInFact* FindForInFact(const Module& module, const ast::Stmt& stmt);
std::string DumpModule(const Module& module);
// Rebuild the auxiliary name -> index lookup caches (function_lookup,
// type_decl_lookup) from the current contents of Module::functions and
// Module::type_decls.
void BuildModuleLookupMaps(Module& module);

}  // namespace mc::sema

#endif  // C_MODERN_COMPILER_SEMA_CHECK_H_