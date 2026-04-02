#include "compiler/sema/check.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "compiler/lex/lexer.h"
#include "compiler/parse/parser.h"

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

enum class VisitState {
    kVisiting,
    kDone,
};

using ImportedModules = std::unordered_map<std::string, Module>;

bool IsIntegerTypeName(std::string_view name) {
    static const std::unordered_set<std::string_view> names = {
        "i8", "i16", "i32", "i64", "isize", "u8", "u16", "u32", "u64", "usize", "uintptr",
    };
    return names.contains(name);
}

bool IsFloatTypeName(std::string_view name) {
    return name == "f32" || name == "f64";
}

bool IsBuiltinNamedType(std::string_view name) {
    return IsIntegerTypeName(name) || IsFloatTypeName(name) || name == "bool" || name == "string" || name == "str" ||
           name == "cstr" || name == "void" || name == "Slice" || name == "Buffer" || name == "Result";
}

std::string QualifyImportedName(std::string_view module_name, std::string_view name) {
    return std::string(module_name) + "." + std::string(name);
}

Type RewriteImportedTypeNames(Type type,
                              std::string_view module_name,
                              const std::unordered_set<std::string>& local_type_names,
                              const std::vector<std::string>& type_params = {}) {
    for (auto& subtype : type.subtypes) {
        subtype = RewriteImportedTypeNames(std::move(subtype), module_name, local_type_names, type_params);
    }

    if (type.kind != Type::Kind::kNamed || type.name.empty() || type.name.find('.') != std::string::npos) {
        return type;
    }
    if (IsBuiltinNamedType(type.name) || std::find(type_params.begin(), type_params.end(), type.name) != type_params.end()) {
        return type;
    }
    if (local_type_names.contains(type.name)) {
        type.name = QualifyImportedName(module_name, type.name);
    }
    return type;
}

Type SubstituteTypeParams(Type type, const std::vector<std::string>& type_params, const std::vector<Type>& type_args) {
    for (auto& subtype : type.subtypes) {
        subtype = SubstituteTypeParams(std::move(subtype), type_params, type_args);
    }

    if (type.kind != Type::Kind::kNamed) {
        return type;
    }

    for (std::size_t index = 0; index < type_params.size() && index < type_args.size(); ++index) {
        if (type.name == type_params[index]) {
            return type_args[index];
        }
    }

    return type;
}

TypeDeclSummary RewriteImportedTypeDecl(const TypeDeclSummary& type_decl,
                                        std::string_view module_name,
                                        const std::unordered_set<std::string>& local_type_names) {
    TypeDeclSummary qualified = type_decl;
    qualified.name = type_decl.name.find('.') == std::string::npos ? QualifyImportedName(module_name, type_decl.name) : type_decl.name;
    for (auto& field : qualified.fields) {
        field.second = RewriteImportedTypeNames(std::move(field.second), module_name, local_type_names, qualified.type_params);
    }
    for (auto& variant : qualified.variants) {
        for (auto& payload_field : variant.payload_fields) {
            payload_field.second = RewriteImportedTypeNames(std::move(payload_field.second), module_name, local_type_names, qualified.type_params);
        }
    }
    qualified.aliased_type = RewriteImportedTypeNames(std::move(qualified.aliased_type), module_name, local_type_names, qualified.type_params);
    return qualified;
}

Module RewriteImportedModuleSurfaceTypesInternal(const Module& module, std::string_view module_name) {
    Module rewritten = module;
    std::unordered_set<std::string> local_type_names;
    for (const auto& type_decl : module.type_decls) {
        local_type_names.insert(type_decl.name);
    }

    for (auto& type_decl : rewritten.type_decls) {
        type_decl = RewriteImportedTypeDecl(type_decl, module_name, local_type_names);
    }
    for (auto& global : rewritten.globals) {
        global.type = RewriteImportedTypeNames(std::move(global.type), module_name, local_type_names);
    }
    for (auto& function : rewritten.functions) {
        for (auto& param_type : function.param_types) {
            param_type = RewriteImportedTypeNames(std::move(param_type), module_name, local_type_names, function.type_params);
        }
        for (auto& return_type : function.return_types) {
            return_type = RewriteImportedTypeNames(std::move(return_type), module_name, local_type_names, function.type_params);
        }
    }
    return rewritten;
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

std::size_t AlignTo(std::size_t value, std::size_t alignment) {
    if (alignment == 0) {
        return value;
    }
    const std::size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
}

bool IsNumericType(const Type& type) {
    if (type.kind == Type::Kind::kIntLiteral || type.kind == Type::Kind::kFloatLiteral) {
        return true;
    }
    if (type.kind != Type::Kind::kNamed) {
        return false;
    }
    return IsIntegerTypeName(type.name) || IsFloatTypeName(type.name);
}

bool IsIntegerLikeType(const Type& type) {
    if (type.kind == Type::Kind::kIntLiteral) {
        return true;
    }
    return type.kind == Type::Kind::kNamed && IsIntegerTypeName(type.name);
}

bool IsBoolType(const Type& type) {
    return type.kind == Type::Kind::kBool || (type.kind == Type::Kind::kNamed && type.name == "bool");
}

bool IsPointerLikeType(const Type& type) {
    return type.kind == Type::Kind::kPointer;
}

bool IsUintPtrType(const Type& type) {
    return type.kind == Type::Kind::kNamed && type.name == "uintptr";
}

bool IsAddressableExpr(const Expr& expr) {
    switch (expr.kind) {
        case Expr::Kind::kName:
        case Expr::Kind::kQualifiedName:
        case Expr::Kind::kField:
        case Expr::Kind::kDerefField:
        case Expr::Kind::kIndex:
            return true;
        case Expr::Kind::kParen:
            return expr.left != nullptr && IsAddressableExpr(*expr.left);
        default:
            return false;
    }
}

bool IsAssignable(const Type& expected, const Type& actual);
bool IsExplicitlyConvertible(const Type& expected, const Type& actual, const Module& module);

Type StripAliasOrDistinct(Type type, const Module& module) {
    std::unordered_set<std::string> visited;
    while (type.kind == Type::Kind::kNamed) {
        if (!visited.insert(type.name).second) {
            break;
        }

        const TypeDeclSummary* type_decl = FindTypeDecl(module, type.name);
        if (type_decl == nullptr) {
            break;
        }
        if (type_decl->kind != Decl::Kind::kDistinct && type_decl->kind != Decl::Kind::kTypeAlias) {
            break;
        }

        type = type_decl->aliased_type;
    }

    return type;
}

Type InferExplicitConversionTarget(Type target, const Type& actual, const Module& module) {
    if (target.kind == Type::Kind::kNamed && target.name == "Slice" && target.subtypes.empty()) {
        if (actual.kind == Type::Kind::kArray && !actual.subtypes.empty()) {
            target.subtypes.push_back(actual.subtypes.front());
            return target;
        }
        if (actual.kind == Type::Kind::kNamed && (actual.name == "Slice" || actual.name == "Buffer") && !actual.subtypes.empty()) {
            target.subtypes = actual.subtypes;
            return target;
        }
    }

    const Type stripped_target = StripAliasOrDistinct(target, module);
    if (stripped_target.kind == Type::Kind::kNamed && stripped_target.name == "Slice" && stripped_target.subtypes.empty()) {
        Type inferred = stripped_target;
        if (actual.kind == Type::Kind::kArray && !actual.subtypes.empty()) {
            inferred.subtypes.push_back(actual.subtypes.front());
            return inferred;
        }
        if (actual.kind == Type::Kind::kNamed && (actual.name == "Slice" || actual.name == "Buffer") && !actual.subtypes.empty()) {
            inferred.subtypes = actual.subtypes;
            return inferred;
        }
    }

    return target;
}

bool IsExplicitlyConvertible(const Type& expected, const Type& actual, const Module& module) {
    if (IsAssignable(expected, actual)) {
        return true;
    }

    if (expected.kind == Type::Kind::kNamed && expected.name == "Slice" && !expected.subtypes.empty()) {
        if (actual.kind == Type::Kind::kArray && !actual.subtypes.empty()) {
            return IsAssignable(expected.subtypes.front(), actual.subtypes.front());
        }
        if (actual.kind == Type::Kind::kNamed && (actual.name == "Slice" || actual.name == "Buffer") && !actual.subtypes.empty()) {
            return IsAssignable(expected.subtypes.front(), actual.subtypes.front());
        }
    }

    const Type stripped_expected = StripAliasOrDistinct(expected, module);
    const Type stripped_actual = StripAliasOrDistinct(actual, module);
    if (IsAssignable(stripped_expected, stripped_actual)) {
        return true;
    }

    if (stripped_expected.kind == Type::Kind::kNamed && stripped_expected.name == "Slice" && !stripped_expected.subtypes.empty()) {
        if (stripped_actual.kind == Type::Kind::kArray && !stripped_actual.subtypes.empty()) {
            return IsAssignable(stripped_expected.subtypes.front(), stripped_actual.subtypes.front());
        }
        if (stripped_actual.kind == Type::Kind::kNamed && (stripped_actual.name == "Slice" || stripped_actual.name == "Buffer") &&
            !stripped_actual.subtypes.empty()) {
            return IsAssignable(stripped_expected.subtypes.front(), stripped_actual.subtypes.front());
        }
    }

    if ((IsPointerLikeType(stripped_expected) && IsUintPtrType(stripped_actual)) ||
        (IsUintPtrType(stripped_expected) && IsPointerLikeType(stripped_actual))) {
        return true;
    }

    return IsNumericType(stripped_expected) && IsNumericType(stripped_actual);
}

Type InferLiteralType(const Expr& expr) {
    if (expr.text == "true" || expr.text == "false") {
        return BoolType();
    }
    if (expr.text == "nil") {
        return NilType();
    }
    if (!expr.text.empty() && expr.text.front() == '"') {
        return StringType();
    }
    if (expr.text.find_first_of(".eE") != std::string::npos) {
        return FloatLiteralType();
    }
    return IntLiteralType();
}

Type MergeNumericTypes(const Type& left, const Type& right) {
    if (IsUnknown(left) || IsUnknown(right)) {
        return UnknownType();
    }
    if (left == right) {
        return left;
    }
    if (left.kind == Type::Kind::kNamed && right.kind == Type::Kind::kIntLiteral && IsIntegerTypeName(left.name)) {
        return left;
    }
    if (right.kind == Type::Kind::kNamed && left.kind == Type::Kind::kIntLiteral && IsIntegerTypeName(right.name)) {
        return right;
    }
    if (left.kind == Type::Kind::kNamed && right.kind == Type::Kind::kFloatLiteral && IsFloatTypeName(left.name)) {
        return left;
    }
    if (right.kind == Type::Kind::kNamed && left.kind == Type::Kind::kFloatLiteral && IsFloatTypeName(right.name)) {
        return right;
    }
    if (left.kind == Type::Kind::kFloatLiteral && right.kind == Type::Kind::kIntLiteral) {
        return left;
    }
    if (right.kind == Type::Kind::kFloatLiteral && left.kind == Type::Kind::kIntLiteral) {
        return right;
    }
    return UnknownType();
}

bool IsAssignable(const Type& expected, const Type& actual) {
    const Type canonical_expected = CanonicalizeBuiltinType(expected);
    const Type canonical_actual = CanonicalizeBuiltinType(actual);
    if (IsUnknown(canonical_expected) || IsUnknown(canonical_actual)) {
        return true;
    }
    if (canonical_expected == canonical_actual) {
        return true;
    }
    if (canonical_expected.kind == Type::Kind::kNamed && canonical_actual.kind == Type::Kind::kIntLiteral &&
        IsIntegerTypeName(canonical_expected.name)) {
        return true;
    }
    if (canonical_expected.kind == Type::Kind::kNamed && canonical_actual.kind == Type::Kind::kFloatLiteral &&
        IsFloatTypeName(canonical_expected.name)) {
        return true;
    }
    if (IsPointerLikeType(canonical_expected) && canonical_actual.kind == Type::Kind::kNil) {
        return true;
    }
    return false;
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

bool SameSpan(const mc::support::SourceSpan& left, const mc::support::SourceSpan& right) {
    return left.begin.line == right.begin.line && left.begin.column == right.begin.column && left.end.line == right.end.line &&
           left.end.column == right.end.column;
}

std::string SpanKey(const mc::support::SourceSpan& span) {
        return std::to_string(span.begin.line) + ":" + std::to_string(span.begin.column) + "-" + std::to_string(span.end.line) + ":" +
                     std::to_string(span.end.column);
}

class Checker {
  public:
    Checker(const ast::SourceFile& source_file,
            const std::filesystem::path& file_path,
            const ImportedModules* imported_modules,
            support::DiagnosticSink& diagnostics)
        : source_file_(source_file), file_path_(file_path), imported_modules_(imported_modules), diagnostics_(diagnostics) {}

    CheckResult Run() {
        auto module = std::make_unique<Module>();
        module_ = module.get();

        for (const auto& import_decl : source_file_.imports) {
            module_->imports.push_back(import_decl.module_name);
        }

        // Checker phase ordering matters: imported symbols must be seeded before top-level
        // declaration collection, and function/type lookup tables are incomplete until
        // CollectTopLevelDecls has finished.
        SeedImportedSymbols();
        CollectTopLevelDecls();
        ValidateExportBlock();
        ValidateTypeDecls();
        ValidateGlobals();
        ValidateFunctions();

        return {
            .module = std::move(module),
            .ok = !diagnostics_.HasErrors(),
        };
    }

  private:
    void Report(const mc::support::SourceSpan& span, const std::string& message) {
        diagnostics_.Report({
            .file_path = file_path_,
            .span = span,
            .severity = DiagnosticSeverity::kError,
            .message = message,
        });
    }

    void SeedImportedSymbols() {
        if (imported_modules_ == nullptr) {
            return;
        }

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
                    global.type = TypeFromAst(decl.type_ann.get());
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

        for (const auto& function : module_->functions) {
            function_map_[function.name] = &function;
        }
        for (const auto& type_decl : module_->type_decls) {
            type_map_[type_decl.name] = &type_decl;
        }
    }

    void ValidateExportBlock() {
        if (!source_file_.has_export_block) {
            return;
        }

        std::unordered_set<std::string> seen;
        for (const auto& name : source_file_.export_block.names) {
            if (!seen.insert(name).second) {
                Report(source_file_.export_block.span, "duplicate export name: " + name);
                continue;
            }
            if (!value_symbols_.contains(name) && !type_symbols_.contains(name)) {
                Report(source_file_.export_block.span, "export name is not declared in module: " + name);
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
            signature.param_types.push_back(TypeFromAst(param.type.get()));
        }
        for (const auto& return_type : decl.return_types) {
            signature.return_types.push_back(TypeFromAst(return_type.get()));
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
            summary.fields.emplace_back(field.name, TypeFromAst(field.type.get()));
        }
        for (const auto& variant : decl.variants) {
            VariantSummary variant_summary;
            variant_summary.name = variant.name;
            for (const auto& field : variant.payload_fields) {
                variant_summary.payload_fields.emplace_back(field.name, TypeFromAst(field.type.get()));
            }
            summary.variants.push_back(std::move(variant_summary));
        }
        summary.aliased_type = TypeFromAst(decl.aliased_type.get());
        return summary;
    }

    bool IsKnownTypeName(const std::string& name, const std::vector<std::string>& type_params) const {
        return IsBuiltinNamedType(name) || type_map_.contains(name) ||
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
            type_args.push_back(TypeFromAst(type_arg.get()));
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
        }

        ComputeTypeLayouts();
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

    LayoutInfo ComputeTypeLayout(const Type& type, const mc::support::SourceSpan& span, std::unordered_set<std::string>& active_types) {
        switch (type.kind) {
            case Type::Kind::kBool:
                return {.valid = true, .size = 1, .alignment = 1};
            case Type::Kind::kString:
                return {.valid = true, .size = 16, .alignment = 8};
            case Type::Kind::kPointer:
                return {.valid = true, .size = 8, .alignment = 8};
            case Type::Kind::kProcedure:
                return {.valid = true, .size = 8, .alignment = 8};
            case Type::Kind::kConst:
                return type.subtypes.empty() ? LayoutInfo {} : ComputeTypeLayout(type.subtypes.front(), span, active_types);
            case Type::Kind::kArray: {
                if (type.subtypes.empty()) {
                    return {};
                }
                const LayoutInfo element_layout = ComputeTypeLayout(type.subtypes.front(), span, active_types);
                if (!element_layout.valid) {
                    return {};
                }
                const auto length = mc::support::ParseArrayLength(type.metadata);
                if (!length.has_value()) {
                    Report(span, "array layout requires an integer constant length, got " + type.metadata);
                    return {};
                }
                return {
                    .valid = true,
                    .size = element_layout.size * *length,
                    .alignment = element_layout.alignment,
                };
            }
            case Type::Kind::kNamed:
                break;
            case Type::Kind::kUnknown:
            case Type::Kind::kVoid:
            case Type::Kind::kNil:
            case Type::Kind::kIntLiteral:
            case Type::Kind::kFloatLiteral:
            case Type::Kind::kTuple:
            case Type::Kind::kRange:
                return {};
        }

        if (type.name == "i8" || type.name == "u8") {
            return {.valid = true, .size = 1, .alignment = 1};
        }
        if (type.name == "i16" || type.name == "u16") {
            return {.valid = true, .size = 2, .alignment = 2};
        }
        if (type.name == "i32" || type.name == "u32" || type.name == "f32") {
            return {.valid = true, .size = 4, .alignment = 4};
        }
        if (type.name == "i64" || type.name == "u64" || type.name == "isize" || type.name == "usize" || type.name == "uintptr" ||
            type.name == "f64") {
            return {.valid = true, .size = 8, .alignment = 8};
        }
        if (type.name == "Slice") {
            return {.valid = true, .size = 16, .alignment = 8};
        }
        if (type.name == "Buffer") {
            return {.valid = true, .size = 32, .alignment = 8};
        }

        TypeDeclSummary* type_decl = FindMutableTypeDecl(type.name);
        if (type_decl == nullptr) {
            Report(span, "layout is not available for type " + FormatType(type));
            return {};
        }

        if (type_decl->kind == Decl::Kind::kDistinct || type_decl->kind == Decl::Kind::kTypeAlias) {
            return ComputeTypeLayout(type_decl->aliased_type, span, active_types);
        }

        if (type_decl->kind != Decl::Kind::kStruct) {
            Report(span, "layout is not available for type " + type_decl->name + " in bootstrap sema");
            return {};
        }

        if (type_decl->layout.valid) {
            return type_decl->layout;
        }

        if (!active_types.insert(type_decl->name).second) {
            Report(span, "type layout cycle requires indirection: " + type_decl->name);
            return {};
        }

        LayoutInfo layout;
        layout.valid = true;
        layout.alignment = type_decl->is_packed ? 1 : 0;
        std::size_t size = 0;
        for (const auto& field : type_decl->fields) {
            const LayoutInfo field_layout = ComputeTypeLayout(field.second, span, active_types);
            if (!field_layout.valid) {
                active_types.erase(type_decl->name);
                return {};
            }
            const std::size_t field_alignment = type_decl->is_packed ? 1 : field_layout.alignment;
            size = AlignTo(size, field_alignment);
            layout.field_offsets.push_back(size);
            size += field_layout.size;
            layout.alignment = std::max(layout.alignment, field_alignment);
        }
        layout.size = AlignTo(size, layout.alignment);

        active_types.erase(type_decl->name);
        type_decl->layout = layout;
        return layout;
    }

    void ComputeTypeLayouts() {
        std::unordered_set<std::string> active_types;
        for (auto& type_decl : module_->type_decls) {
            if (type_decl.kind != Decl::Kind::kStruct) {
                continue;
            }
            if (type_decl.layout.valid) {
                continue;
            }
            (void)ComputeTypeLayout(NamedType(type_decl.name), source_file_.span, active_types);
            active_types.clear();
        }
    }

    void ValidateGlobals() {
        for (const auto& decl : source_file_.decls) {
            if (decl.kind != Decl::Kind::kConst && decl.kind != Decl::Kind::kVar) {
                continue;
            }

            ValidateTypeExpr(decl.type_ann.get(), {}, decl.span);
            for (const auto& value : decl.values) {
                (void)AnalyzeExpr(*value);
            }
        }
    }

    void ValidateFunctions() {
        for (const auto& decl : source_file_.decls) {
            if (decl.kind != Decl::Kind::kFunc) {
                continue;
            }

            const FunctionSignature* signature = FindFunctionSignature(*module_, decl.name);
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
            PopScope();
            current_function_ = nullptr;
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
        const std::string key = SpanKey(expr.span);
        const auto found = module_->expr_type_indices.find(key);
        if (found != module_->expr_type_indices.end()) {
            module_->expr_types[found->second].type = type;
            return;
        }
        module_->expr_types.push_back({
            .span = expr.span,
            .type = type,
        });
        module_->expr_type_indices[key] = module_->expr_types.size() - 1;
    }

    void RecordBindingOrAssignFact(const Stmt& stmt, std::vector<BindingOrAssignResolution> resolutions) {
        const std::string key = SpanKey(stmt.span);
        const auto found = module_->binding_or_assign_indices.find(key);
        if (found != module_->binding_or_assign_indices.end()) {
            module_->binding_or_assign_facts[found->second].resolutions = std::move(resolutions);
            return;
        }
        module_->binding_or_assign_facts.push_back({
            .span = stmt.span,
            .resolutions = std::move(resolutions),
        });
        module_->binding_or_assign_indices[key] = module_->binding_or_assign_facts.size() - 1;
    }

    const TypeDeclSummary* LookupStructType(const Type& type) const {
        if (type.kind != Type::Kind::kNamed) {
            return nullptr;
        }
        return FindTypeDecl(*module_, type.name);
    }

    const VariantSummary* LookupVariant(const Type& selector_type, std::string_view variant_name) const {
        if (selector_type.kind != Type::Kind::kNamed) {
            return nullptr;
        }

        const TypeDeclSummary* type_decl = FindTypeDecl(*module_, selector_type.name);
        if (type_decl == nullptr) {
            return nullptr;
        }

        const std::size_t separator = variant_name.find('.');
        const std::string qualified_type = separator == std::string_view::npos ? selector_type.name : std::string(variant_name.substr(0, separator));
        const std::string leaf_name = separator == std::string_view::npos ? std::string(variant_name) : std::string(variant_name.substr(separator + 1));
        if (!qualified_type.empty() && qualified_type != selector_type.name) {
            return nullptr;
        }

        for (const auto& variant : type_decl->variants) {
            if (variant.name == leaf_name) {
                return &variant;
            }
        }

        return nullptr;
    }

    Type AnalyzeVariantConstructorCall(const Expr& expr,
                                       const Type& callee_type,
                                       const std::vector<Type>& arg_types) {
        if (expr.left == nullptr || expr.left->kind != Expr::Kind::kQualifiedName || callee_type.kind != Type::Kind::kNamed) {
            return UnknownType();
        }

        const VariantSummary* variant = LookupVariant(callee_type, expr.left->secondary_text);
        if (variant == nullptr) {
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
            if (!IsAssignable(expected, actual)) {
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
            conversion_target = TypeFromAst(expr.type_target.get());
        }

        std::vector<Type> arg_types;
        arg_types.reserve(expr.args.size());
        for (const auto& arg : expr.args) {
            arg_types.push_back(AnalyzeExpr(*arg));
        }

        if (!IsUnknown(conversion_target)) {
            if (expr.args.size() != 1) {
                Report(expr.span, "explicit conversion to " + FormatType(conversion_target) + " expects 1 argument but got " +
                                      std::to_string(expr.args.size()));
                return conversion_target;
            }
            const Type actual = arg_types.front();
            conversion_target = InferExplicitConversionTarget(conversion_target, actual, *module_);
            if (!IsExplicitlyConvertible(conversion_target, actual, *module_)) {
                Report(expr.args.front()->span,
                       "explicit conversion to " + FormatType(conversion_target) + " is not allowed from " + FormatType(actual));
            }
            return conversion_target;
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

        const Type variant_ctor_type = AnalyzeVariantConstructorCall(expr, callee_type, arg_types);
        if (!IsUnknown(variant_ctor_type)) {
            return variant_ctor_type;
        }

        const Type stripped_callee = StripAliasOrDistinct(callee_type, *module_);
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
            if (param_count >= stripped_callee.subtypes.size()) {
                return VoidType();
            }
            const std::vector<Type> returns(stripped_callee.subtypes.begin() + static_cast<std::ptrdiff_t>(param_count),
                                            stripped_callee.subtypes.end());
            if (returns.empty()) {
                return VoidType();
            }
            if (returns.size() == 1) {
                return returns.front();
            }
            return TupleType(returns);
        }

        for (std::size_t index = 0; index < expr.args.size(); ++index) {
            const Type& actual = arg_types[index];
            const Type& expected = stripped_callee.subtypes[index];
            if (!IsAssignable(expected, actual)) {
                const std::string callee_label = has_named_callee ? callee_name : "call target";
                Report(expr.args[index]->span,
                       "argument type mismatch for parameter " + std::to_string(index + 1) + " of " + callee_label +
                           ": expected " + FormatType(expected) + ", got " + FormatType(actual));
            }
        }

        if (param_count >= stripped_callee.subtypes.size()) {
            return VoidType();
        }

        const std::vector<Type> returns(stripped_callee.subtypes.begin() + static_cast<std::ptrdiff_t>(param_count),
                                        stripped_callee.subtypes.end());
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
                if (const auto* function = FindFunctionSignature(*module_, expr.text); function != nullptr) {
                    return record(InstantiateFunctionType(*function, expr, expr.text));
                }
                if (const auto* global = FindGlobalSummary(*module_, expr.text); global != nullptr) {
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
                    if (!expr.type_args.empty()) {
                        Report(expr.span, "type arguments apply only to functions: " + CombineQualifiedName(expr));
                        return record(UnknownType());
                    }
                    const Type base = binding->type;
                    if (base.kind == Type::Kind::kNamed && (base.name == "Slice" || base.name == "Buffer")) {
                        if ((expr.secondary_text == "ptr") && !base.subtypes.empty()) {
                            return record(PointerType(base.subtypes.front()));
                        }
                        if (expr.secondary_text == "len" || (base.name == "Buffer" && expr.secondary_text == "cap")) {
                            return record(NamedType("usize"));
                        }
                        if (base.name == "Buffer" && expr.secondary_text == "alloc") {
                            return record(PointerType(NamedType("Allocator")));
                        }
                    }
                    if ((base.kind == Type::Kind::kString || (base.kind == Type::Kind::kNamed && (base.name == "str" || base.name == "string"))) &&
                        (expr.secondary_text == "ptr" || expr.secondary_text == "len")) {
                        if (expr.secondary_text == "ptr") {
                            return record(PointerType(NamedType("u8")));
                        }
                        return record(NamedType("usize"));
                    }
                    const TypeDeclSummary* type_decl = LookupStructType(base);
                    if (type_decl != nullptr) {
                        for (const auto& field : type_decl->fields) {
                            if (field.first == expr.secondary_text) {
                                return record(field.second);
                            }
                        }
                        Report(expr.span, "type " + type_decl->name + " has no field named " + expr.secondary_text);
                    }
                    return record(UnknownType());
                }
                if (const auto* global = FindGlobalSummary(*module_, expr.text); global != nullptr) {
                    if (!expr.type_args.empty()) {
                        Report(expr.span, "type arguments apply only to functions: " + CombineQualifiedName(expr));
                        return record(UnknownType());
                    }
                    const Type base = global->type;
                    if (base.kind == Type::Kind::kNamed && (base.name == "Slice" || base.name == "Buffer")) {
                        if ((expr.secondary_text == "ptr") && !base.subtypes.empty()) {
                            return record(PointerType(base.subtypes.front()));
                        }
                        if (expr.secondary_text == "len" || (base.name == "Buffer" && expr.secondary_text == "cap")) {
                            return record(NamedType("usize"));
                        }
                        if (base.name == "Buffer" && expr.secondary_text == "alloc") {
                            return record(PointerType(NamedType("Allocator")));
                        }
                    }
                }
                if (const Module* imported_module = FindImportedModule(expr.text); imported_module != nullptr) {
                    if (const auto* function = FindFunctionSignature(*imported_module, expr.secondary_text); function != nullptr) {
                        return record(InstantiateFunctionType(*function, expr, CombineQualifiedName(expr)));
                    }
                    if (const auto* global = FindGlobalSummary(*imported_module, expr.secondary_text); global != nullptr) {
                        if (!expr.type_args.empty()) {
                            Report(expr.span, "type arguments apply only to functions: " + CombineQualifiedName(expr));
                            return record(UnknownType());
                        }
                        return record(global->type);
                    }
                    Report(expr.span, "module " + expr.text + " has no exported member named " + expr.secondary_text);
                    return record(UnknownType());
                }
                if (const auto* function = FindFunctionSignature(*module_, expr.text + "." + expr.secondary_text); function != nullptr) {
                    return record(InstantiateFunctionType(*function, expr, CombineQualifiedName(expr)));
                }
                if (!expr.type_args.empty()) {
                    Report(expr.span, "type arguments apply only to functions: " + CombineQualifiedName(expr));
                    return record(UnknownType());
                }
                if (const auto* type_decl = FindTypeDecl(*module_, expr.text);
                    type_decl != nullptr && type_decl->kind == Decl::Kind::kEnum) {
                    for (const auto& variant : type_decl->variants) {
                        if (variant.name == expr.secondary_text) {
                            return record(NamedType(expr.text));
                        }
                    }
                }
                return record(UnknownType());
            case Expr::Kind::kLiteral:
                return record(InferLiteralType(expr));
            case Expr::Kind::kUnary: {
                const Type operand = AnalyzeExpr(*expr.left);
                if (expr.text == "!" && !IsUnknown(operand) && !IsBoolType(operand)) {
                    Report(expr.span, "logical not requires bool operand");
                }
                if (expr.text == "&") {
                    if (!IsAddressableExpr(*expr.left)) {
                        Report(expr.span, "address-of requires an assignable operand");
                    }
                    return record(IsUnknown(operand) ? UnknownType() : PointerType(operand));
                }
                if (expr.text == "*") {
                    const Type deref_operand = StripAliasOrDistinct(operand, *module_);
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
                const Type left = AnalyzeExpr(*expr.left);
                const Type right = AnalyzeExpr(*expr.right);
                if (expr.kind == Expr::Kind::kRange) {
                    const Type element = MergeNumericTypes(left, right);
                    if (IsUnknown(element)) {
                        Report(expr.span, "range bounds must be compatible numeric types");
                    }
                    return record(RangeType(element));
                }
                if (expr.text == "&&" || expr.text == "||") {
                    if (!IsUnknown(left) && !IsBoolType(left)) {
                        Report(expr.left->span, "logical operator requires bool operands");
                    }
                    if (!IsUnknown(right) && !IsBoolType(right)) {
                        Report(expr.right->span, "logical operator requires bool operands");
                    }
                    return record(BoolType());
                }
                if (expr.text == "==" || expr.text == "!=" || expr.text == "<" || expr.text == ">" || expr.text == "<=" ||
                    expr.text == ">=") {
                    if (IsUnknown(MergeNumericTypes(left, right)) && !IsAssignable(left, right) && !IsAssignable(right, left)) {
                        Report(expr.span, "comparison requires compatible operand types");
                    }
                    return record(BoolType());
                }
                if (expr.text == "<<" || expr.text == ">>") {
                    if (!IsUnknown(left) && !IsIntegerLikeType(left)) {
                        Report(expr.left->span, "shift operator requires integer left operand");
                    }
                    if (!IsUnknown(right) && !IsIntegerLikeType(right)) {
                        Report(expr.right->span, "shift operator requires integer right operand");
                    }
                    return record(left);
                }
                if (!IsNumericType(left) || !IsNumericType(right)) {
                    if (!IsUnknown(left) && !IsUnknown(right)) {
                        Report(expr.span, "operator " + expr.text + " requires numeric operands");
                    }
                    return record(UnknownType());
                }
                const Type merged = MergeNumericTypes(left, right);
                if (IsUnknown(merged)) {
                    Report(expr.span, "operator " + expr.text + " requires compatible numeric operand types");
                }
                return record(merged);
            }
            case Expr::Kind::kCall:
                return record(AnalyzeCall(expr));
            case Expr::Kind::kField:
            case Expr::Kind::kDerefField: {
                const Type base = AnalyzeExpr(*expr.left);
                if (base.kind == Type::Kind::kNamed && (base.name == "Slice" || base.name == "Buffer")) {
                    if (expr.text == "ptr" && !base.subtypes.empty()) {
                        return record(PointerType(base.subtypes.front()));
                    }
                    if (expr.text == "len" || (base.name == "Buffer" && expr.text == "cap")) {
                        return record(NamedType("usize"));
                    }
                    if (base.name == "Buffer" && expr.text == "alloc") {
                        return record(PointerType(NamedType("Allocator")));
                    }
                }
                if ((base.kind == Type::Kind::kString || (base.kind == Type::Kind::kNamed && (base.name == "str" || base.name == "string"))) &&
                    (expr.text == "ptr" || expr.text == "len")) {
                    if (expr.text == "ptr") {
                        return record(PointerType(NamedType("u8")));
                    }
                    return record(NamedType("usize"));
                }
                const TypeDeclSummary* type_decl = LookupStructType(base);
                if (type_decl != nullptr) {
                    for (const auto& field : type_decl->fields) {
                        if (field.first == expr.text) {
                            return record(field.second);
                        }
                    }
                    Report(expr.span, "type " + type_decl->name + " has no field named " + expr.text);
                }
                return record(UnknownType());
            }
            case Expr::Kind::kIndex: {
                const Type base = AnalyzeExpr(*expr.left);
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
                const Type base = AnalyzeExpr(*expr.left);
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
                Type aggregate_type = UnknownType();
                if (expr.left != nullptr && (expr.left->kind == Expr::Kind::kName || expr.left->kind == Expr::Kind::kQualifiedName)) {
                    aggregate_type = NamedOrBuiltinType(CombineQualifiedName(*expr.left));
                    for (const auto& type_arg : expr.left->type_args) {
                        ValidateTypeExpr(type_arg.get(), CurrentTypeParams(), type_arg->span);
                        aggregate_type.subtypes.push_back(TypeFromAst(type_arg.get()));
                    }
                }
                const TypeDeclSummary* type_decl = LookupStructType(aggregate_type);
                const auto builtin_fields = BuiltinAggregateFields(aggregate_type);
                if (type_decl != nullptr || !builtin_fields.empty()) {
                    std::unordered_map<std::string, Type> fields;
                    const auto& field_source = type_decl != nullptr ? type_decl->fields : builtin_fields;
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
                                       "type " + FormatType(aggregate_type) + " has no field named " + field_init.name);
                                continue;
                            }
                            if (!IsAssignable(found->second, value_type)) {
                                Report(field_init.span,
                                       "aggregate field type mismatch for " + field_init.name + ": expected " + FormatType(found->second) +
                                           ", got " + FormatType(value_type));
                            }
                            continue;
                        }
                        if (index >= field_source.size()) {
                            Report(field_init.span, "too many aggregate initializer values for type " + FormatType(aggregate_type));
                            continue;
                        }
                        if (!IsAssignable(field_source[index].second, value_type)) {
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
                return record(aggregate_type);
            }
            case Expr::Kind::kParen:
                return record(AnalyzeExpr(*expr.left));
        }

        return record(UnknownType());
    }

    void CheckAssignableTarget(const Expr& expr, const Type& value_type) {
        if (expr.kind == Expr::Kind::kName) {
            const auto binding = LookupValue(expr.text);
            if (!binding.has_value()) {
                Report(expr.span, "assignment target is not declared: " + expr.text);
                return;
            }
            if (!binding->is_mutable) {
                Report(expr.span, "assignment target is readonly: " + expr.text);
                return;
            }
            if (!IsAssignable(binding->type, value_type)) {
                Report(expr.span, "assignment type mismatch for " + expr.text + ": expected " + FormatType(binding->type) + ", got " +
                                      FormatType(value_type));
            }
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
                    if (!IsUnknown(condition) && !IsBoolType(condition)) {
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
                    Report(stmt.span, "defer requires a call expression in the bootstrap semantic checker");
                    return;
                }
                (void)AnalyzeExpr(*stmt.exprs.front());
                return;
        }
    }

    void CheckBindingLike(const Stmt& stmt, bool is_mutable) {
        ValidateTypeExpr(stmt.type_ann.get(), current_function_ != nullptr ? current_function_->type_params : std::vector<std::string> {}, stmt.span);
        const Type declared_type = TypeFromAst(stmt.type_ann.get());
        if (stmt.has_initializer && stmt.pattern.names.size() != stmt.exprs.size()) {
            Report(stmt.span, "binding requires one initializer per bound name");
            return;
        }

        std::vector<Type> value_types;
        value_types.reserve(stmt.pattern.names.size());
        for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
            Type value_type = declared_type;
            if (index < stmt.exprs.size()) {
                value_type = AnalyzeExpr(*stmt.exprs[index]);
                if (!IsUnknown(declared_type) && !IsAssignable(declared_type, value_type)) {
                    Report(stmt.exprs[index]->span, "binding type mismatch for " + stmt.pattern.names[index] + ": expected " +
                                                   FormatType(declared_type) + ", got " + FormatType(value_type));
                }
            }
            value_types.push_back(value_type);
        }

        for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
            BindValue(stmt.pattern.names[index], IsUnknown(declared_type) ? value_types[index] : declared_type, is_mutable, stmt.span);
        }
    }

    void CheckBindingOrAssign(const Stmt& stmt) {
        if (stmt.pattern.names.size() != stmt.exprs.size()) {
            Report(stmt.span, "binding-or-assignment requires one value per name");
            return;
        }

        std::vector<Type> value_types;
        value_types.reserve(stmt.exprs.size());
        for (const auto& expr : stmt.exprs) {
            value_types.push_back(AnalyzeExpr(*expr));
        }

        std::vector<BindingOrAssignResolution> resolutions;
        resolutions.reserve(stmt.pattern.names.size());
        for (const auto& name : stmt.pattern.names) {
            resolutions.push_back(LookupValue(name).has_value() ? BindingOrAssignResolution::kAssign : BindingOrAssignResolution::kBind);
        }
        RecordBindingOrAssignFact(stmt, resolutions);

        for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
            if (resolutions[index] == BindingOrAssignResolution::kAssign) {
                Expr target;
                target.kind = Expr::Kind::kName;
                target.text = stmt.pattern.names[index];
                target.span = stmt.span;
                CheckAssignableTarget(target, value_types[index]);
                continue;
            }
            BindValue(stmt.pattern.names[index], value_types[index], true, stmt.span);
        }
    }

    void CheckAssign(const Stmt& stmt) {
        if (stmt.assign_targets.size() != stmt.assign_values.size()) {
            Report(stmt.span, "assignment requires one value per target");
            return;
        }
        for (std::size_t index = 0; index < stmt.assign_targets.size(); ++index) {
            const Type value_type = AnalyzeExpr(*stmt.assign_values[index]);
            CheckAssignableTarget(*stmt.assign_targets[index], value_type);
        }
    }

    void CheckSwitch(const Stmt& stmt) {
        const Type selector = stmt.exprs.empty() ? UnknownType() : AnalyzeExpr(*stmt.exprs.front());
        for (const auto& switch_case : stmt.switch_cases) {
            PushScope();
            if (switch_case.pattern.kind == mc::ast::CasePattern::Kind::kExpr && switch_case.pattern.expr != nullptr) {
                const Type pattern_type = AnalyzeExpr(*switch_case.pattern.expr);
                if (!IsUnknown(selector) && !IsUnknown(pattern_type) && !IsAssignable(selector, pattern_type) && !IsAssignable(pattern_type, selector)) {
                    Report(switch_case.pattern.span, "switch case pattern type does not match selector type");
                }
            } else if (switch_case.pattern.kind == mc::ast::CasePattern::Kind::kVariant) {
                const VariantSummary* variant = LookupVariant(selector, switch_case.pattern.variant_name);
                if (variant == nullptr) {
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
            if (!IsUnknown(condition) && !IsBoolType(condition)) {
                Report(stmt.exprs.front()->span, "loop condition must have type bool");
            }
        }
        ++loop_depth_;
        if (stmt.then_branch != nullptr) {
            CheckStmt(*stmt.then_branch);
        }
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
        if (stmt.exprs.size() != current_function_->return_types.size()) {
            Report(stmt.span, "return value count does not match function signature");
            return;
        }
        for (std::size_t index = 0; index < stmt.exprs.size(); ++index) {
            const Type actual = AnalyzeExpr(*stmt.exprs[index]);
            if (!IsAssignable(current_function_->return_types[index], actual)) {
                Report(stmt.exprs[index]->span,
                       "return type mismatch: expected " + FormatType(current_function_->return_types[index]) + ", got " +
                           FormatType(actual));
            }
        }
    }

    const ast::SourceFile& source_file_;
    std::filesystem::path file_path_;
    const ImportedModules* imported_modules_;
    support::DiagnosticSink& diagnostics_;
    Module* module_ = nullptr;
    std::unordered_map<std::string, Decl::Kind> value_symbols_;
    std::unordered_map<std::string, Decl::Kind> type_symbols_;
    std::unordered_map<std::string, ValueBinding> global_symbols_;
    std::unordered_map<std::string, const FunctionSignature*> function_map_;
    std::unordered_map<std::string, const TypeDeclSummary*> type_map_;
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

Module BuildExportedModuleSurfaceInternal(const Module& module, const ast::SourceFile& source_file) {
    Module exported;
    exported.imports = module.imports;
    if (!source_file.has_export_block) {
        return exported;
    }

    std::unordered_set<std::string> exported_names(source_file.export_block.names.begin(), source_file.export_block.names.end());
    for (const auto& function : module.functions) {
        if (exported_names.contains(function.name)) {
            exported.functions.push_back(function);
        }
    }
    for (const auto& type_decl : module.type_decls) {
        if (exported_names.contains(type_decl.name)) {
            exported.type_decls.push_back(type_decl);
        }
    }
    for (const auto& global : module.globals) {
        GlobalSummary filtered = global;
        filtered.names.clear();
        for (const auto& name : global.names) {
            if (exported_names.contains(name)) {
                filtered.names.push_back(name);
            }
        }
        if (!filtered.names.empty()) {
            exported.globals.push_back(std::move(filtered));
        }
    }
    return exported;
}

CheckResult CheckProgramInternal(const ast::SourceFile& source_file,
                                 const std::filesystem::path& file_path,
                                 const CheckOptions& options,
                                 support::DiagnosticSink& diagnostics,
                                 std::unordered_map<std::filesystem::path, Module>& exported_cache,
                                 std::unordered_map<std::filesystem::path, VisitState>& visit_state) {
    const std::filesystem::path normalized_path = std::filesystem::absolute(file_path).lexically_normal();
    const auto cached = exported_cache.find(normalized_path);
    if (cached != exported_cache.end()) {
        return {
            .module = std::make_unique<Module>(cached->second),
            .ok = !diagnostics.HasErrors(),
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
            imported_modules[import_decl.module_name] = RewriteImportedModuleSurfaceTypesInternal(found->second, import_decl.module_name);
        }
    } else {
        for (const auto& import_decl : source_file.imports) {
            const auto import_path = mc::support::ResolveImportPath(normalized_path,
                                        import_decl.module_name,
                                        options.import_roots,
                                        diagnostics,
                                        import_decl.span);
            if (!import_path.has_value()) {
                continue;
            }

            const auto import_state = visit_state.find(*import_path);
            if (import_state != visit_state.end() && import_state->second == VisitState::kVisiting) {
                diagnostics.Report({
                    .file_path = normalized_path,
                    .span = import_decl.span,
                    .severity = DiagnosticSeverity::kError,
                    .message = "import cycle detected involving module: " + import_decl.module_name,
                });
                continue;
            }

            Module exported_module;
            const auto found_exported = exported_cache.find(*import_path);
            if (found_exported != exported_cache.end()) {
                exported_module = found_exported->second;
            } else {
                auto parsed_import = ParseFile(*import_path, diagnostics);
                if (parsed_import == nullptr) {
                    continue;
                }
                const auto checked_import = CheckProgramInternal(*parsed_import, *import_path, options, diagnostics, exported_cache, visit_state);
                if (checked_import.module != nullptr) {
                    exported_module = BuildExportedModuleSurfaceInternal(*checked_import.module, *parsed_import);
                }
            }

            imported_modules[import_decl.module_name] = RewriteImportedModuleSurfaceTypesInternal(exported_module, import_decl.module_name);
        }
    }

    const ImportedModules* imported_modules_ptr = imported_modules.empty() ? nullptr : &imported_modules;
    auto checked = Checker(source_file, normalized_path, imported_modules_ptr, diagnostics).Run();
    Module exported_module = checked.module != nullptr ? BuildExportedModuleSurfaceInternal(*checked.module, source_file) : Module {};
    exported_cache[normalized_path] = exported_module;
    visit_state[normalized_path] = VisitState::kDone;
    return checked;
}

}  // namespace

Module BuildExportedModuleSurface(const Module& module,
                                  const ast::SourceFile& source_file) {
    return BuildExportedModuleSurfaceInternal(module, source_file);
}

Module RewriteImportedModuleSurfaceTypes(const Module& module,
                                         std::string_view module_name) {
    return RewriteImportedModuleSurfaceTypesInternal(module, module_name);
}

CheckResult CheckProgram(const ast::SourceFile& source_file,
                         const std::filesystem::path& file_path,
                         const CheckOptions& options,
                         support::DiagnosticSink& diagnostics) {
    std::unordered_map<std::filesystem::path, Module> exported_cache;
    std::unordered_map<std::filesystem::path, VisitState> visit_state;
    return CheckProgramInternal(source_file, file_path, options, diagnostics, exported_cache, visit_state);
}

CheckResult CheckProgram(const ast::SourceFile& source_file,
                         const std::filesystem::path& file_path,
                         support::DiagnosticSink& diagnostics) {
    return CheckProgram(source_file, file_path, CheckOptions {}, diagnostics);
}

CheckResult CheckSourceFile(const ast::SourceFile& source_file,
                            const std::filesystem::path& file_path,
                            support::DiagnosticSink& diagnostics) {
    return Checker(source_file, file_path, nullptr, diagnostics).Run();
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
    const auto found = module.expr_type_indices.find(SpanKey(expr.span));
    if (found != module.expr_type_indices.end() && found->second < module.expr_types.size()) {
        return &module.expr_types[found->second].type;
    }
    return nullptr;
}

const BindingOrAssignFact* FindBindingOrAssignFact(const Module& module, const ast::Stmt& stmt) {
    const auto found = module.binding_or_assign_indices.find(SpanKey(stmt.span));
    if (found != module.binding_or_assign_indices.end() && found->second < module.binding_or_assign_facts.size()) {
        return &module.binding_or_assign_facts[found->second];
    }
    return nullptr;
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
        WriteLine(stream, 1, line.str());
    }
    return stream.str();
}

}  // namespace mc::sema