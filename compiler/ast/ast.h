#ifndef C_MODERN_COMPILER_AST_AST_H_
#define C_MODERN_COMPILER_AST_AST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "compiler/support/source_span.h"

namespace mc::ast {

struct Expr;
struct Stmt;
struct TypeExpr;

struct Node {
    support::SourceSpan span {};
};

struct AttributeArg : Node {
    bool is_identifier = false;
    std::string value;
};

struct Attribute : Node {
    std::string name;
    std::vector<AttributeArg> args;
};

struct ExportBlock : Node {
    std::vector<std::string> names;
};

struct ImportDecl : Node {
    std::string module_name;
};

struct TypeExpr : Node {
    enum class Kind {
        kNamed,
        kPointer,
        kConst,
        kArray,
        kParen,
    };

    Kind kind = Kind::kNamed;
    std::string name;
    std::vector<std::unique_ptr<TypeExpr>> type_args;
    std::unique_ptr<TypeExpr> inner;
    std::unique_ptr<Expr> length_expr;
};

struct NamePattern : Node {
    std::vector<std::string> names;
};

struct FieldDecl : Node {
    std::string name;
    std::unique_ptr<TypeExpr> type;
};

struct EnumVariantDecl : Node {
    std::string name;
    std::vector<FieldDecl> payload_fields;
};

struct ParamDecl : Node {
    std::vector<Attribute> attributes;
    std::string name;
    std::unique_ptr<TypeExpr> type;
};

struct FieldInit : Node {
    bool has_name = false;
    std::string name;
    std::unique_ptr<Expr> value;
};

struct CasePattern : Node {
    enum class Kind {
        kExpr,
        kVariant,
    };

    Kind kind = Kind::kExpr;
    std::unique_ptr<Expr> expr;
    std::string variant_name;
    std::vector<std::string> bindings;
};

struct SwitchCase : Node {
    CasePattern pattern;
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct DefaultCase : Node {
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct Expr : Node {
    enum class Kind {
        kName,
        kQualifiedName,
        kLiteral,
        kUnary,
        kBinary,
        kRange,
        kCall,
        kField,
        kDerefField,
        kIndex,
        kSlice,
        kAggregateInit,
        kParen,
    };

    Kind kind = Kind::kName;
    std::string text;
    std::string secondary_text;
    std::vector<std::unique_ptr<TypeExpr>> type_args;
    std::unique_ptr<TypeExpr> type_target;
    std::vector<std::unique_ptr<Expr>> args;
    std::vector<FieldInit> field_inits;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    std::unique_ptr<Expr> extra;
};

struct Stmt : Node {
    enum class Kind {
        kBlock,
        kBinding,
        kBindingOrAssign,
        kVar,
        kConst,
        kAssign,
        kExpr,
        kIf,
        kSwitch,
        kWhile,
        kForCondition,
        kForEach,
        kForEachIndex,
        kForRange,
        kReturn,
        kBreak,
        kContinue,
        kDefer,
    };

    Kind kind = Kind::kExpr;
    std::vector<std::unique_ptr<Stmt>> statements;
    NamePattern pattern;
    std::unique_ptr<TypeExpr> type_ann;
    std::vector<std::unique_ptr<Expr>> exprs;
    std::vector<std::unique_ptr<Expr>> assign_targets;
    std::vector<std::unique_ptr<Expr>> assign_values;
    bool has_initializer = false;
    std::vector<SwitchCase> switch_cases;
    bool has_default_case = false;
    DefaultCase default_case;
    std::string loop_name;
    std::string loop_second_name;
    std::unique_ptr<Stmt> then_branch;
    std::unique_ptr<Stmt> else_branch;
};

struct Decl : Node {
    enum class Kind {
        kFunc,
        kExternFunc,
        kStruct,
        kEnum,
        kDistinct,
        kTypeAlias,
        kConst,
        kVar,
    };

    Kind kind = Kind::kFunc;
    std::vector<Attribute> attributes;
    std::string name;
    std::string extern_abi;
    std::vector<std::string> type_params;
    std::vector<ParamDecl> params;
    std::vector<std::unique_ptr<TypeExpr>> return_types;
    std::unique_ptr<Stmt> body;
    std::vector<FieldDecl> fields;
    std::vector<EnumVariantDecl> variants;
    NamePattern pattern;
    std::unique_ptr<TypeExpr> type_ann;
    std::vector<std::unique_ptr<Expr>> values;
    bool has_initializer = false;
    std::unique_ptr<TypeExpr> aliased_type;
};

struct SourceFile : Node {
    bool has_export_block = false;
    ExportBlock export_block;
    std::vector<ImportDecl> imports;
    std::vector<Decl> decls;
};

std::string DumpSourceFile(const SourceFile& source_file);

}  // namespace mc::ast

#endif  // C_MODERN_COMPILER_AST_AST_H_