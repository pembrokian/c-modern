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

std::size_t AlignTo(std::size_t value, std::size_t alignment) {
    if (alignment == 0) {
        return value;
    }
    const std::size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
}

std::optional<std::size_t> ParseArrayLength(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::size_t value = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = (value * 10) + static_cast<std::size_t>(ch - '0');
    }
    return value;
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
    if (type.kind != Type::Kind::kNamed) {
        return type;
    }
    const TypeDeclSummary* type_decl = FindTypeDecl(module, type.name);
    if (type_decl == nullptr) {
        return type;
    }
    if (type_decl->kind != Decl::Kind::kDistinct && type_decl->kind != Decl::Kind::kTypeAlias) {
        return type;
    }
    return type_decl->aliased_type;
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
    if ((stripped_expected != expected || stripped_actual != actual) && IsExplicitlyConvertible(stripped_expected, stripped_actual, module)) {
        return true;
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
    if (IsUnknown(expected) || IsUnknown(actual)) {
        return true;
    }
    if (expected == actual) {
        return true;
    }
    if (expected.kind == Type::Kind::kNamed && actual.kind == Type::Kind::kIntLiteral && IsIntegerTypeName(expected.name)) {
        return true;
    }
    if (expected.kind == Type::Kind::kNamed && actual.kind == Type::Kind::kFloatLiteral && IsFloatTypeName(expected.name)) {
        return true;
    }
    if (IsPointerLikeType(expected) && actual.kind == Type::Kind::kNil) {
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

class Checker {
  public:
    Checker(const ast::SourceFile& source_file,
            const std::filesystem::path& file_path,
                        const Module* imported_module,
            support::DiagnosticSink& diagnostics)
                : source_file_(source_file), file_path_(file_path), imported_module_(imported_module), diagnostics_(diagnostics) {}

    CheckResult Run() {
        auto module = std::make_unique<Module>();
        module_ = module.get();

        for (const auto& import_decl : source_file_.imports) {
            module_->imports.push_back(import_decl.module_name);
        }

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
        if (imported_module_ == nullptr) {
            return;
        }

        for (const auto& type_decl : imported_module_->type_decls) {
            if (!type_symbols_.emplace(type_decl.name, type_decl.kind).second) {
                Report(source_file_.span, "duplicate imported type symbol: " + type_decl.name);
                continue;
            }
            module_->type_decls.push_back(type_decl);
        }

        for (const auto& function : imported_module_->functions) {
            if (!value_symbols_.emplace(function.name, function.is_extern ? Decl::Kind::kExternFunc : Decl::Kind::kFunc).second) {
                Report(source_file_.span, "duplicate imported value symbol: " + function.name);
                continue;
            }
            module_->functions.push_back(function);
        }

        for (const auto& global : imported_module_->globals) {
            GlobalSummary imported_global = global;
            imported_global.names.clear();
            for (const auto& name : global.names) {
                if (!value_symbols_.emplace(name, global.is_const ? Decl::Kind::kConst : Decl::Kind::kVar).second) {
                    Report(source_file_.span, "duplicate imported value symbol: " + name);
                    continue;
                }
                imported_global.names.push_back(name);
                global_symbols_[name] = {
                    .type = global.type,
                    .is_mutable = !global.is_const,
                };
            }
            if (!imported_global.names.empty()) {
                module_->globals.push_back(std::move(imported_global));
            }
        }
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
                const auto length = ParseArrayLength(type.metadata);
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
        layout.alignment = type_decl->is_packed ? 1 : 1;
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
                (void)AnalyzeExpr(*value, false);
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
        for (auto& fact : module_->expr_types) {
            if (SameSpan(fact.span, expr.span)) {
                fact.type = type;
                return;
            }
        }
        module_->expr_types.push_back({
            .span = expr.span,
            .type = type,
        });
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

    Type AnalyzeCall(const Expr& expr) {
        if (expr.left == nullptr && expr.type_target == nullptr) {
            return UnknownType();
        }

        const FunctionSignature* signature = nullptr;
        Type conversion_target = UnknownType();
        if (expr.type_target != nullptr) {
            ValidateTypeExpr(expr.type_target.get(), current_function_ != nullptr ? current_function_->type_params : std::vector<std::string> {}, expr.type_target->span);
            conversion_target = TypeFromAst(expr.type_target.get());
        } else if (expr.left != nullptr && expr.left->kind == Expr::Kind::kName) {
            signature = FindFunctionSignature(*module_, expr.left->text);
        }

        for (const auto& arg : expr.args) {
            (void)AnalyzeExpr(*arg, false);
        }

        if (!IsUnknown(conversion_target)) {
            if (expr.args.size() != 1) {
                Report(expr.span, "explicit conversion to " + FormatType(conversion_target) + " expects 1 argument but got " +
                                      std::to_string(expr.args.size()));
                return conversion_target;
            }
            const Type actual = AnalyzeExpr(*expr.args.front(), false);
            conversion_target = InferExplicitConversionTarget(conversion_target, actual, *module_);
            if (!IsExplicitlyConvertible(conversion_target, actual, *module_)) {
                Report(expr.args.front()->span,
                       "explicit conversion to " + FormatType(conversion_target) + " is not allowed from " + FormatType(actual));
            }
            return conversion_target;
        }

        if (signature == nullptr) {
            if (expr.left != nullptr && expr.left->kind == Expr::Kind::kName &&
                IsKnownTypeName(expr.left->text, current_function_ != nullptr ? current_function_->type_params : std::vector<std::string> {})) {
                Report(expr.span, "explicit conversions require parenthesized type-expression syntax");
            }
            return UnknownType();
        }

        if (signature->param_types.size() != expr.args.size()) {
            Report(expr.span, "call to " + signature->name + " expects " + std::to_string(signature->param_types.size()) +
                                  " arguments but got " + std::to_string(expr.args.size()));
            return signature->return_types.empty() ? VoidType() : signature->return_types.front();
        }

        for (std::size_t index = 0; index < expr.args.size(); ++index) {
            const Type actual = AnalyzeExpr(*expr.args[index], false);
            if (!IsAssignable(signature->param_types[index], actual)) {
                Report(expr.args[index]->span,
                       "argument type mismatch for parameter " + std::to_string(index + 1) + " of " + signature->name +
                           ": expected " + FormatType(signature->param_types[index]) + ", got " + FormatType(actual));
            }
        }

        if (signature->return_types.empty()) {
            return VoidType();
        }
        if (signature->return_types.size() == 1) {
            return signature->return_types.front();
        }
        return TupleType(signature->return_types);
    }

    Type AnalyzeExpr(const Expr& expr, bool allow_unresolved_name) {
        const auto record = [&](const Type& type) {
            RecordExprType(expr, type);
            return type;
        };

        switch (expr.kind) {
            case Expr::Kind::kName: {
                const auto binding = LookupValue(expr.text);
                if (binding.has_value()) {
                    return record(binding->type);
                }
                if (const auto* function = FindFunctionSignature(*module_, expr.text); function != nullptr) {
                    return record(ProcedureType(function->param_types, function->return_types));
                }
                if (const auto* global = FindGlobalSummary(*module_, expr.text); global != nullptr) {
                    return record(global->type);
                }
                if (!allow_unresolved_name) {
                    Report(expr.span, "unknown name: " + expr.text);
                }
                return record(UnknownType());
            }
            case Expr::Kind::kQualifiedName:
                if (const auto binding = LookupValue(expr.text); binding.has_value()) {
                    const Type base = binding->type;
                    if (base.kind == Type::Kind::kNamed && (base.name == "Slice" || base.name == "Buffer") && expr.secondary_text == "len") {
                        return record(NamedType("usize"));
                    }
                    if ((base.kind == Type::Kind::kString || (base.kind == Type::Kind::kNamed && (base.name == "str" || base.name == "string"))) &&
                        expr.secondary_text == "len") {
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
                    const Type base = global->type;
                    if (base.kind == Type::Kind::kNamed && (base.name == "Slice" || base.name == "Buffer") && expr.secondary_text == "len") {
                        return record(NamedType("usize"));
                    }
                }
                if (const auto* function = FindFunctionSignature(*module_, expr.text + "." + expr.secondary_text); function != nullptr) {
                    return record(ProcedureType(function->param_types, function->return_types));
                }
                return record(UnknownType());
            case Expr::Kind::kLiteral:
                return record(InferLiteralType(expr));
            case Expr::Kind::kUnary: {
                const Type operand = AnalyzeExpr(*expr.left, false);
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
                const Type left = AnalyzeExpr(*expr.left, false);
                const Type right = AnalyzeExpr(*expr.right, false);
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
                const Type base = AnalyzeExpr(*expr.left, false);
                if (base.kind == Type::Kind::kNamed && (base.name == "Slice" || base.name == "Buffer") && expr.text == "len") {
                    return record(NamedType("usize"));
                }
                if ((base.kind == Type::Kind::kString || (base.kind == Type::Kind::kNamed && (base.name == "str" || base.name == "string"))) &&
                    expr.text == "len") {
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
                const Type base = AnalyzeExpr(*expr.left, false);
                (void)AnalyzeExpr(*expr.right, false);
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
                const Type base = AnalyzeExpr(*expr.left, false);
                if (expr.right != nullptr) {
                    (void)AnalyzeExpr(*expr.right, false);
                }
                if (expr.extra != nullptr) {
                    (void)AnalyzeExpr(*expr.extra, false);
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
                if (expr.left != nullptr && expr.left->kind == Expr::Kind::kName) {
                    aggregate_type = NamedType(expr.left->text);
                }
                const TypeDeclSummary* type_decl = LookupStructType(aggregate_type);
                if (type_decl != nullptr) {
                    std::unordered_map<std::string, Type> fields;
                    for (const auto& field : type_decl->fields) {
                        fields[field.first] = field.second;
                    }
                    for (std::size_t index = 0; index < expr.field_inits.size(); ++index) {
                        const auto& field_init = expr.field_inits[index];
                        const Type value_type = AnalyzeExpr(*field_init.value, false);
                        if (field_init.has_name) {
                            const auto found = fields.find(field_init.name);
                            if (found == fields.end()) {
                                Report(field_init.span, "type " + type_decl->name + " has no field named " + field_init.name);
                                continue;
                            }
                            if (!IsAssignable(found->second, value_type)) {
                                Report(field_init.span,
                                       "aggregate field type mismatch for " + field_init.name + ": expected " + FormatType(found->second) +
                                           ", got " + FormatType(value_type));
                            }
                            continue;
                        }
                        if (index >= type_decl->fields.size()) {
                            Report(field_init.span, "too many aggregate initializer values for type " + type_decl->name);
                            continue;
                        }
                        if (!IsAssignable(type_decl->fields[index].second, value_type)) {
                            Report(field_init.span,
                                   "aggregate field type mismatch for " + type_decl->fields[index].first + ": expected " +
                                       FormatType(type_decl->fields[index].second) + ", got " + FormatType(value_type));
                        }
                    }
                } else {
                    for (const auto& field_init : expr.field_inits) {
                        (void)AnalyzeExpr(*field_init.value, false);
                    }
                }
                return record(aggregate_type);
            }
            case Expr::Kind::kParen:
                return record(AnalyzeExpr(*expr.left, false));
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

        (void)AnalyzeExpr(expr, false);
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
                    (void)AnalyzeExpr(*stmt.exprs.front(), false);
                }
                return;
            case Stmt::Kind::kIf:
                if (!stmt.exprs.empty()) {
                    const Type condition = AnalyzeExpr(*stmt.exprs.front(), false);
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
                (void)AnalyzeExpr(*stmt.exprs.front(), false);
                return;
        }
    }

    void CheckBindingLike(const Stmt& stmt, bool is_mutable) {
        ValidateTypeExpr(stmt.type_ann.get(), current_function_ != nullptr ? current_function_->type_params : std::vector<std::string> {}, stmt.span);
        const Type declared_type = TypeFromAst(stmt.type_ann.get());
        for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
            Type value_type = declared_type;
            if (index < stmt.exprs.size()) {
                value_type = AnalyzeExpr(*stmt.exprs[index], false);
                if (!IsUnknown(declared_type) && !IsAssignable(declared_type, value_type)) {
                    Report(stmt.exprs[index]->span, "binding type mismatch for " + stmt.pattern.names[index] + ": expected " +
                                                   FormatType(declared_type) + ", got " + FormatType(value_type));
                }
            }
            BindValue(stmt.pattern.names[index], IsUnknown(declared_type) ? value_type : declared_type, is_mutable, stmt.span);
        }
    }

    void CheckBindingOrAssign(const Stmt& stmt) {
        bool any_existing = false;
        bool any_missing = false;
        for (const auto& name : stmt.pattern.names) {
            if (LookupValue(name).has_value()) {
                any_existing = true;
            } else {
                any_missing = true;
            }
        }
        if (any_existing && any_missing) {
            Report(stmt.span, "cannot mix existing assignments and new bindings in one binding-or-assignment statement");
            return;
        }
        if (any_existing) {
            for (std::size_t index = 0; index < stmt.pattern.names.size() && index < stmt.exprs.size(); ++index) {
                Expr target;
                target.kind = Expr::Kind::kName;
                target.text = stmt.pattern.names[index];
                target.span = stmt.span;
                CheckAssignableTarget(target, AnalyzeExpr(*stmt.exprs[index], false));
            }
            return;
        }
        CheckBindingLike(stmt, true);
    }

    void CheckAssign(const Stmt& stmt) {
        if (stmt.assign_targets.size() != stmt.assign_values.size()) {
            Report(stmt.span, "assignment requires one value per target");
            return;
        }
        for (std::size_t index = 0; index < stmt.assign_targets.size(); ++index) {
            const Type value_type = AnalyzeExpr(*stmt.assign_values[index], false);
            CheckAssignableTarget(*stmt.assign_targets[index], value_type);
        }
    }

    void CheckSwitch(const Stmt& stmt) {
        const Type selector = stmt.exprs.empty() ? UnknownType() : AnalyzeExpr(*stmt.exprs.front(), false);
        for (const auto& switch_case : stmt.switch_cases) {
            PushScope();
            if (switch_case.pattern.kind == mc::ast::CasePattern::Kind::kExpr && switch_case.pattern.expr != nullptr) {
                const Type pattern_type = AnalyzeExpr(*switch_case.pattern.expr, false);
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
            const Type condition = AnalyzeExpr(*stmt.exprs.front(), false);
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
        const Type iterable = AnalyzeExpr(*stmt.exprs.front(), false);
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
        const Type range_type = AnalyzeExpr(*stmt.exprs.front(), false);
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
            const Type actual = AnalyzeExpr(*stmt.exprs[index], false);
            if (!IsAssignable(current_function_->return_types[index], actual)) {
                Report(stmt.exprs[index]->span,
                       "return type mismatch: expected " + FormatType(current_function_->return_types[index]) + ", got " +
                           FormatType(actual));
            }
        }
    }

    const ast::SourceFile& source_file_;
    std::filesystem::path file_path_;
    const Module* imported_module_;
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

std::optional<std::filesystem::path> ResolveImportPath(const std::filesystem::path& importer_path,
                                                       std::string_view module_name,
                                                       const CheckOptions& options,
                                                       support::DiagnosticSink& diagnostics,
                                                       const mc::support::SourceSpan& span) {
    std::vector<std::filesystem::path> search_roots;
    search_roots.push_back(importer_path.parent_path());
    for (const auto& root : options.import_roots) {
        search_roots.push_back(root);
    }

    for (const auto& root : search_roots) {
        const std::filesystem::path candidate =
            std::filesystem::absolute(root / (std::string(module_name) + ".mc")).lexically_normal();
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    diagnostics.Report({
        .file_path = importer_path,
        .span = span,
        .severity = DiagnosticSeverity::kError,
        .message = "unable to resolve import module: " + std::string(module_name),
    });
    return std::nullopt;
}

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

Module BuildExportedModule(const Module& module, const ast::SourceFile& source_file) {
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

    Module imported_symbols;
    for (const auto& import_decl : source_file.imports) {
        const auto import_path = ResolveImportPath(normalized_path, import_decl.module_name, options, diagnostics, import_decl.span);
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
                exported_module = *checked_import.module;
            }
        }

        imported_symbols.functions.insert(imported_symbols.functions.end(), exported_module.functions.begin(), exported_module.functions.end());
        imported_symbols.type_decls.insert(imported_symbols.type_decls.end(), exported_module.type_decls.begin(), exported_module.type_decls.end());
        imported_symbols.globals.insert(imported_symbols.globals.end(), exported_module.globals.begin(), exported_module.globals.end());
    }

    auto checked = Checker(source_file, normalized_path, &imported_symbols, diagnostics).Run();
    Module exported_module = checked.module != nullptr ? BuildExportedModule(*checked.module, source_file) : Module {};
    exported_cache[normalized_path] = exported_module;
    visit_state[normalized_path] = VisitState::kDone;
    return checked;
}

}  // namespace

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
    for (const auto& fact : module.expr_types) {
        if (SameSpan(fact.span, expr.span)) {
            return &fact.type;
        }
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