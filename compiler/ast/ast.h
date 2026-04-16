#ifndef C_MODERN_COMPILER_AST_AST_H_
#define C_MODERN_COMPILER_AST_AST_H_

#include <cstdint>
#include <memory>
#include <optional>
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
        kProcedure,
    };

    Kind kind = Kind::kNamed;
    std::string name;
    std::vector<std::unique_ptr<TypeExpr>> type_args;
    std::unique_ptr<TypeExpr> inner;
    std::unique_ptr<Expr> length_expr;
    std::vector<std::unique_ptr<TypeExpr>> params;
    std::vector<std::unique_ptr<TypeExpr>> returns;
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
    std::vector<std::string> field_path;
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

// Expr — bootstrap AST expression node.
//
// Design: a single fat struct covers all expression kinds so that the tree can
// be allocated with ordinary unique_ptr and traversed without virtual dispatch.
// This is deliberate for the bootstrap phase; a discriminated-union redesign
// is out of scope until the full type system is stable.
//
// Field usage by Kind:
//   kName            : text = identifier name
//   kQualifiedName   : text = module name, secondary_text = member name
//   kLiteral         : text = literal token text, secondary_text = literal token kind name,
//                      integer_literal_value/float_literal_value = decoded numeric payload when present
//   kUnary           : text = operator, left = operand
//   kBinary          : text = operator, left = lhs, right = rhs
//   kRange           : left = begin, right = end (nullptr means open end)
//   kCall            : left = callee, args = argument list, type_args = explicit type args
//   kField           : left = base, text = field name
//   kDerefField      : left = base
//   kIndex           : left = base, right = index
//   kSlice           : left = base, right = begin, extra = end
//   kAggregateInit   : left = named base or type_target = explicit/inferred type, field_inits = fields
//   kRecordUpdate    : left = base value expression, field_inits = replaced fields
//   kParen           : left = inner expression
//
// Optional pointer fields: nullptr means the field is absent for that Kind.
//   left, right, extra, type_target are all optional.
//
// Conversion syntax flag:
//   bare_type_target_syntax = true when a CallExpr came from the narrow
//   builtin-integer target form `u8(expr)` admitted in Phase 194. This lets
//   sema keep that source form narrower than the older parenthesized explicit
//   conversion surface.
//
// secondary_text meanings by Kind:
//   kQualifiedName   : member name after the dot
//   kLiteral         : lexer token kind name (for example "string_lit" or "int_lit")
//   (all other Kinds leave secondary_text empty)
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
        kRecordUpdate,
        kParen,
    };

    Kind kind = Kind::kName;
    std::string text;
    std::string secondary_text;
    std::optional<std::int64_t> integer_literal_value;
    std::optional<double> float_literal_value;
    std::vector<std::unique_ptr<TypeExpr>> type_args;
    std::unique_ptr<TypeExpr> type_target;
    bool bare_type_target_syntax = false;
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
        kVar,
        kConst,
        kAssign,
        kExpr,
        kIf,
        kSwitch,
        kWhile,
        kForCondition,
        // kForIn: parser-deferred loop classification.
        // The parser knows this is `for name in expr`, but sema must decide
        // whether `expr` is a semantic range or an ordinary iterable.
        kForIn,
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
    enum class ModuleKind {
        kOrdinary,
        kInternal,
    };

    ModuleKind module_kind = ModuleKind::kOrdinary;
    std::vector<ImportDecl> imports;
    std::vector<Decl> decls;
};

std::string DumpSourceFile(const SourceFile& source_file);

}  // namespace mc::ast

#endif  // C_MODERN_COMPILER_AST_AST_H_