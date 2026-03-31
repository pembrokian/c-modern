#include "compiler/ast/ast.h"

#include <sstream>
#include <string_view>

namespace mc::ast {
namespace {

using Indent = int;

std::string_view ToString(TypeExpr::Kind kind) {
    switch (kind) {
        case TypeExpr::Kind::kNamed:
            return "NamedType";
        case TypeExpr::Kind::kPointer:
            return "PointerType";
        case TypeExpr::Kind::kConst:
            return "ConstType";
        case TypeExpr::Kind::kArray:
            return "ArrayType";
        case TypeExpr::Kind::kParen:
            return "ParenType";
    }

    return "Type";
}

std::string_view ToString(Expr::Kind kind) {
    switch (kind) {
        case Expr::Kind::kName:
            return "NameExpr";
        case Expr::Kind::kQualifiedName:
            return "QualifiedNameExpr";
        case Expr::Kind::kLiteral:
            return "LiteralExpr";
        case Expr::Kind::kUnary:
            return "UnaryExpr";
        case Expr::Kind::kBinary:
            return "BinaryExpr";
        case Expr::Kind::kRange:
            return "RangeExpr";
        case Expr::Kind::kCall:
            return "CallExpr";
        case Expr::Kind::kField:
            return "FieldExpr";
        case Expr::Kind::kDerefField:
            return "DerefFieldExpr";
        case Expr::Kind::kIndex:
            return "IndexExpr";
        case Expr::Kind::kSlice:
            return "SliceExpr";
        case Expr::Kind::kAggregateInit:
            return "AggregateInitExpr";
        case Expr::Kind::kParen:
            return "ParenExpr";
    }

    return "Expr";
}

std::string_view ToString(Stmt::Kind kind) {
    switch (kind) {
        case Stmt::Kind::kBlock:
            return "BlockStmt";
        case Stmt::Kind::kBinding:
            return "BindingStmt";
        case Stmt::Kind::kBindingOrAssign:
            return "BindingOrAssignStmt";
        case Stmt::Kind::kVar:
            return "VarStmt";
        case Stmt::Kind::kConst:
            return "ConstStmt";
        case Stmt::Kind::kAssign:
            return "AssignStmt";
        case Stmt::Kind::kExpr:
            return "ExprStmt";
        case Stmt::Kind::kIf:
            return "IfStmt";
        case Stmt::Kind::kSwitch:
            return "SwitchStmt";
        case Stmt::Kind::kWhile:
            return "WhileStmt";
        case Stmt::Kind::kForCondition:
            return "ForConditionStmt";
        case Stmt::Kind::kForEach:
            return "ForEachStmt";
        case Stmt::Kind::kForEachIndex:
            return "ForEachIndexStmt";
        case Stmt::Kind::kForRange:
            return "ForRangeStmt";
        case Stmt::Kind::kReturn:
            return "ReturnStmt";
        case Stmt::Kind::kBreak:
            return "BreakStmt";
        case Stmt::Kind::kContinue:
            return "ContinueStmt";
        case Stmt::Kind::kDefer:
            return "DeferStmt";
    }

    return "Stmt";
}

std::string_view ToString(Decl::Kind kind) {
    switch (kind) {
        case Decl::Kind::kFunc:
            return "FuncDecl";
        case Decl::Kind::kExternFunc:
            return "ExternFuncDecl";
        case Decl::Kind::kStruct:
            return "StructDecl";
        case Decl::Kind::kEnum:
            return "EnumDecl";
        case Decl::Kind::kDistinct:
            return "DistinctDecl";
        case Decl::Kind::kTypeAlias:
            return "TypeAliasDecl";
        case Decl::Kind::kConst:
            return "ConstDecl";
        case Decl::Kind::kVar:
            return "VarDecl";
    }

    return "Decl";
}

void WriteIndent(std::ostringstream& stream, Indent indent) {
    for (Indent count = 0; count < indent; ++count) {
        stream << "  ";
    }
}

void WriteLine(std::ostringstream& stream, Indent indent, std::string_view text) {
    WriteIndent(stream, indent);
    stream << text << '\n';
}

void DumpType(const TypeExpr& type, std::ostringstream& stream, Indent indent);
void DumpExpr(const Expr& expr, std::ostringstream& stream, Indent indent);
void DumpStmt(const Stmt& stmt, std::ostringstream& stream, Indent indent);

void DumpAttributes(const std::vector<Attribute>& attributes, std::ostringstream& stream, Indent indent) {
    for (const auto& attribute : attributes) {
        WriteIndent(stream, indent);
        stream << "Attribute name=" << attribute.name;
        if (!attribute.args.empty()) {
            stream << " args=[";
            for (std::size_t index = 0; index < attribute.args.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << (attribute.args[index].is_identifier ? "ident:" : "literal:") << attribute.args[index].value;
            }
            stream << "]";
        }
        stream << '\n';
    }
}

void DumpPattern(const NamePattern& pattern, std::ostringstream& stream, Indent indent) {
    WriteIndent(stream, indent);
    stream << "NamePattern names=[";
    for (std::size_t index = 0; index < pattern.names.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << pattern.names[index];
    }
    stream << "]\n";
}

void DumpField(const FieldDecl& field, std::ostringstream& stream, Indent indent) {
    WriteIndent(stream, indent);
    stream << "FieldDecl name=" << field.name << '\n';
    DumpType(*field.type, stream, indent + 1);
}

void DumpType(const TypeExpr& type, std::ostringstream& stream, Indent indent) {
    WriteIndent(stream, indent);
    stream << ToString(type.kind);
    if (!type.name.empty()) {
        stream << " name=" << type.name;
    }
    stream << '\n';

    if (type.length_expr != nullptr) {
        WriteLine(stream, indent + 1, "length:");
        DumpExpr(*type.length_expr, stream, indent + 2);
    }

    if (!type.type_args.empty()) {
        WriteLine(stream, indent + 1, "typeArgs:");
        for (const auto& arg : type.type_args) {
            DumpType(*arg, stream, indent + 2);
        }
    }

    if (type.inner != nullptr) {
        WriteLine(stream, indent + 1, "inner:");
        DumpType(*type.inner, stream, indent + 2);
    }
}

void DumpFieldInit(const FieldInit& init, std::ostringstream& stream, Indent indent) {
    WriteIndent(stream, indent);
    stream << "FieldInit";
    if (init.has_name) {
        stream << " name=" << init.name;
    }
    stream << '\n';
    DumpExpr(*init.value, stream, indent + 1);
}

void DumpExpr(const Expr& expr, std::ostringstream& stream, Indent indent) {
    WriteIndent(stream, indent);
    stream << ToString(expr.kind);
    if (!expr.text.empty()) {
        stream << " text=" << expr.text;
    }
    if (!expr.secondary_text.empty()) {
        stream << " extra=" << expr.secondary_text;
    }
    stream << '\n';

    if (expr.left != nullptr) {
        WriteLine(stream, indent + 1, "left:");
        DumpExpr(*expr.left, stream, indent + 2);
    }

    if (expr.right != nullptr) {
        WriteLine(stream, indent + 1, "right:");
        DumpExpr(*expr.right, stream, indent + 2);
    }

    if (expr.extra != nullptr) {
        WriteLine(stream, indent + 1, "extra:");
        DumpExpr(*expr.extra, stream, indent + 2);
    }

    if (expr.type_target != nullptr) {
        WriteLine(stream, indent + 1, "typeTarget:");
        DumpType(*expr.type_target, stream, indent + 2);
    }

    if (!expr.args.empty()) {
        WriteLine(stream, indent + 1, "args:");
        for (const auto& arg : expr.args) {
            DumpExpr(*arg, stream, indent + 2);
        }
    }

    if (!expr.field_inits.empty()) {
        WriteLine(stream, indent + 1, "inits:");
        for (const auto& init : expr.field_inits) {
            DumpFieldInit(init, stream, indent + 2);
        }
    }
}

void DumpCasePattern(const CasePattern& pattern, std::ostringstream& stream, Indent indent) {
    WriteIndent(stream, indent);
    stream << (pattern.kind == CasePattern::Kind::kExpr ? "ExprCasePattern" : "VariantCasePattern");
    if (!pattern.variant_name.empty()) {
        stream << " name=" << pattern.variant_name;
    }
    stream << '\n';

    if (pattern.expr != nullptr) {
        DumpExpr(*pattern.expr, stream, indent + 1);
    }

    if (!pattern.bindings.empty()) {
        WriteIndent(stream, indent + 1);
        stream << "bindings=[";
        for (std::size_t index = 0; index < pattern.bindings.size(); ++index) {
            if (index > 0) {
                stream << ", ";
            }
            stream << pattern.bindings[index];
        }
        stream << "]\n";
    }
}

void DumpStmt(const Stmt& stmt, std::ostringstream& stream, Indent indent) {
    WriteIndent(stream, indent);
    stream << ToString(stmt.kind);
    if (!stmt.loop_name.empty()) {
        stream << " name=" << stmt.loop_name;
    }
    if (!stmt.loop_second_name.empty()) {
        stream << " second=" << stmt.loop_second_name;
    }
    if (stmt.has_initializer) {
        stream << " hasInitializer=true";
    }
    stream << '\n';

    if (!stmt.pattern.names.empty()) {
        DumpPattern(stmt.pattern, stream, indent + 1);
    }

    if (stmt.type_ann != nullptr) {
        WriteLine(stream, indent + 1, "type:");
        DumpType(*stmt.type_ann, stream, indent + 2);
    }

    if (!stmt.exprs.empty()) {
        WriteLine(stream, indent + 1, "exprs:");
        for (const auto& expr : stmt.exprs) {
            DumpExpr(*expr, stream, indent + 2);
        }
    }

    if (!stmt.assign_targets.empty()) {
        WriteLine(stream, indent + 1, "targets:");
        for (const auto& expr : stmt.assign_targets) {
            DumpExpr(*expr, stream, indent + 2);
        }
    }

    if (!stmt.assign_values.empty()) {
        WriteLine(stream, indent + 1, "values:");
        for (const auto& expr : stmt.assign_values) {
            DumpExpr(*expr, stream, indent + 2);
        }
    }

    if (!stmt.statements.empty()) {
        WriteLine(stream, indent + 1, "statements:");
        for (const auto& child : stmt.statements) {
            DumpStmt(*child, stream, indent + 2);
        }
    }

    if (stmt.then_branch != nullptr) {
        WriteLine(stream, indent + 1, "then:");
        DumpStmt(*stmt.then_branch, stream, indent + 2);
    }

    if (stmt.else_branch != nullptr) {
        WriteLine(stream, indent + 1, "else:");
        DumpStmt(*stmt.else_branch, stream, indent + 2);
    }

    if (!stmt.switch_cases.empty()) {
        WriteLine(stream, indent + 1, "cases:");
        for (const auto& case_node : stmt.switch_cases) {
            WriteLine(stream, indent + 2, "SwitchCase:");
            DumpCasePattern(case_node.pattern, stream, indent + 3);
            for (const auto& item : case_node.statements) {
                DumpStmt(*item, stream, indent + 3);
            }
        }
    }

    if (stmt.has_default_case) {
        WriteLine(stream, indent + 1, "default:");
        for (const auto& item : stmt.default_case.statements) {
            DumpStmt(*item, stream, indent + 2);
        }
    }
}

void DumpDecl(const Decl& decl, std::ostringstream& stream, Indent indent) {
    WriteIndent(stream, indent);
    stream << ToString(decl.kind);
    if (!decl.name.empty()) {
        stream << " name=" << decl.name;
    }
    if (!decl.extern_abi.empty()) {
        stream << " abi=" << decl.extern_abi;
    }
    if (decl.has_initializer) {
        stream << " hasInitializer=true";
    }
    stream << '\n';

    DumpAttributes(decl.attributes, stream, indent + 1);

    if (!decl.type_params.empty()) {
        WriteIndent(stream, indent + 1);
        stream << "typeParams=[";
        for (std::size_t index = 0; index < decl.type_params.size(); ++index) {
            if (index > 0) {
                stream << ", ";
            }
            stream << decl.type_params[index];
        }
        stream << "]\n";
    }

    for (const auto& param : decl.params) {
        WriteIndent(stream, indent + 1);
        stream << "ParamDecl name=" << param.name << '\n';
        DumpAttributes(param.attributes, stream, indent + 2);
        DumpType(*param.type, stream, indent + 2);
    }

    if (!decl.return_types.empty()) {
        WriteLine(stream, indent + 1, "returns:");
        for (const auto& type : decl.return_types) {
            DumpType(*type, stream, indent + 2);
        }
    }

    if (decl.body != nullptr) {
        WriteLine(stream, indent + 1, "body:");
        DumpStmt(*decl.body, stream, indent + 2);
    }

    if (!decl.fields.empty()) {
        WriteLine(stream, indent + 1, "fields:");
        for (const auto& field : decl.fields) {
            DumpField(field, stream, indent + 2);
        }
    }

    if (!decl.variants.empty()) {
        WriteLine(stream, indent + 1, "variants:");
        for (const auto& variant : decl.variants) {
            WriteIndent(stream, indent + 2);
            stream << "EnumVariantDecl name=" << variant.name << '\n';
            for (const auto& field : variant.payload_fields) {
                DumpField(field, stream, indent + 3);
            }
        }
    }

    if (!decl.pattern.names.empty()) {
        DumpPattern(decl.pattern, stream, indent + 1);
    }

    if (decl.type_ann != nullptr) {
        WriteLine(stream, indent + 1, "type:");
        DumpType(*decl.type_ann, stream, indent + 2);
    }

    if (decl.aliased_type != nullptr) {
        WriteLine(stream, indent + 1, "aliasedType:");
        DumpType(*decl.aliased_type, stream, indent + 2);
    }

    if (!decl.values.empty()) {
        WriteLine(stream, indent + 1, "values:");
        for (const auto& value : decl.values) {
            DumpExpr(*value, stream, indent + 2);
        }
    }
}

}  // namespace

std::string DumpSourceFile(const SourceFile& source_file) {
    std::ostringstream stream;
    WriteLine(stream, 0, "SourceFile");

    if (source_file.has_export_block) {
        WriteIndent(stream, 1);
        stream << "ExportBlock names=[";
        for (std::size_t index = 0; index < source_file.export_block.names.size(); ++index) {
            if (index > 0) {
                stream << ", ";
            }
            stream << source_file.export_block.names[index];
        }
        stream << "]\n";
    }

    for (const auto& import_decl : source_file.imports) {
        WriteIndent(stream, 1);
        stream << "ImportDecl moduleName=" << import_decl.module_name << '\n';
    }

    for (const auto& decl : source_file.decls) {
        DumpDecl(decl, stream, 1);
    }

    return stream.str();
}

}  // namespace mc::ast