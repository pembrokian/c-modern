#include "compiler/sema/type.h"

#include <sstream>

namespace mc::sema {
namespace {

std::string RenderExprInline(const ast::Expr& expr) {
    switch (expr.kind) {
        case ast::Expr::Kind::kName:
            return expr.text;
        case ast::Expr::Kind::kQualifiedName:
            return expr.text + "." + expr.secondary_text;
        case ast::Expr::Kind::kLiteral:
            return expr.text;
        case ast::Expr::Kind::kUnary:
            return expr.text + (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>"));
        case ast::Expr::Kind::kBinary:
        case ast::Expr::Kind::kRange:
            return (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + " " + expr.text + " " +
                   (expr.right != nullptr ? RenderExprInline(*expr.right) : std::string("<?>"));
        case ast::Expr::Kind::kCall: {
            std::ostringstream stream;
            stream << (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) << '(';
            for (std::size_t index = 0; index < expr.args.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << RenderExprInline(*expr.args[index]);
            }
            stream << ')';
            return stream.str();
        }
        case ast::Expr::Kind::kField:
            return (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + "." + expr.text;
        case ast::Expr::Kind::kDerefField:
            return (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + ".*";
        case ast::Expr::Kind::kIndex:
            return (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + "[" +
                   (expr.right != nullptr ? RenderExprInline(*expr.right) : std::string("<?>")) + "]";
        case ast::Expr::Kind::kSlice: {
            std::ostringstream stream;
            stream << (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) << '[';
            if (expr.right != nullptr) {
                stream << RenderExprInline(*expr.right);
            }
            stream << ':';
            if (expr.extra != nullptr) {
                stream << RenderExprInline(*expr.extra);
            }
            stream << ']';
            return stream.str();
        }
        case ast::Expr::Kind::kAggregateInit:
            return expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>");
        case ast::Expr::Kind::kParen:
            return "(" + (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + ")";
    }

    return "<?>";
}

}  // namespace

bool operator==(const Type& left, const Type& right) {
    return left.kind == right.kind && left.name == right.name && left.metadata == right.metadata && left.subtypes == right.subtypes;
}

bool operator!=(const Type& left, const Type& right) {
    return !(left == right);
}

Type UnknownType() {
    return {.kind = Type::Kind::kUnknown};
}

Type VoidType() {
    return {.kind = Type::Kind::kVoid};
}

Type BoolType() {
    return {.kind = Type::Kind::kBool};
}

Type StringType() {
    return {.kind = Type::Kind::kString};
}

Type NilType() {
    return {.kind = Type::Kind::kNil};
}

Type IntLiteralType() {
    return {.kind = Type::Kind::kIntLiteral};
}

Type FloatLiteralType() {
    return {.kind = Type::Kind::kFloatLiteral};
}

Type NamedType(std::string name) {
    return {
        .kind = Type::Kind::kNamed,
        .name = std::move(name),
    };
}

Type PointerType(Type pointee) {
    Type type {.kind = Type::Kind::kPointer};
    type.subtypes.push_back(std::move(pointee));
    return type;
}

Type ConstType(Type inner) {
    Type type {.kind = Type::Kind::kConst};
    type.subtypes.push_back(std::move(inner));
    return type;
}

Type ArrayType(Type inner, std::string length_text) {
    Type type {
        .kind = Type::Kind::kArray,
        .metadata = std::move(length_text),
    };
    type.subtypes.push_back(std::move(inner));
    return type;
}

Type TupleType(std::vector<Type> elements) {
    Type type {.kind = Type::Kind::kTuple};
    type.subtypes = std::move(elements);
    return type;
}

Type RangeType(Type element) {
    Type type {.kind = Type::Kind::kRange};
    type.subtypes.push_back(std::move(element));
    return type;
}

Type TypeFromAst(const ast::TypeExpr* type_expr) {
    if (type_expr == nullptr) {
        return UnknownType();
    }

    switch (type_expr->kind) {
        case ast::TypeExpr::Kind::kNamed: {
            if (type_expr->name == "bool") {
                return BoolType();
            }
            if (type_expr->name == "str" || type_expr->name == "string" || type_expr->name == "cstr") {
                return StringType();
            }
            Type type = NamedType(type_expr->name);
            type.subtypes.reserve(type_expr->type_args.size());
            for (const auto& type_arg : type_expr->type_args) {
                type.subtypes.push_back(TypeFromAst(type_arg.get()));
            }
            return type;
        }
        case ast::TypeExpr::Kind::kPointer:
            return PointerType(TypeFromAst(type_expr->inner.get()));
        case ast::TypeExpr::Kind::kConst:
            return ConstType(TypeFromAst(type_expr->inner.get()));
        case ast::TypeExpr::Kind::kArray:
            return ArrayType(TypeFromAst(type_expr->inner.get()),
                             type_expr->length_expr != nullptr ? RenderExprInline(*type_expr->length_expr) : std::string("?"));
        case ast::TypeExpr::Kind::kParen:
            return TypeFromAst(type_expr->inner.get());
    }

    return UnknownType();
}

std::string FormatType(const Type& type) {
    switch (type.kind) {
        case Type::Kind::kUnknown:
            return "unknown";
        case Type::Kind::kVoid:
            return "void";
        case Type::Kind::kBool:
            return "bool";
        case Type::Kind::kString:
            return "string";
        case Type::Kind::kNil:
            return "nil";
        case Type::Kind::kIntLiteral:
            return "int_literal";
        case Type::Kind::kFloatLiteral:
            return "float_literal";
        case Type::Kind::kNamed: {
            if (type.subtypes.empty()) {
                return type.name;
            }

            std::ostringstream stream;
            stream << type.name << '<';
            for (std::size_t index = 0; index < type.subtypes.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << FormatType(type.subtypes[index]);
            }
            stream << '>';
            return stream.str();
        }
        case Type::Kind::kPointer:
            return "*" + FormatType(type.subtypes.front());
        case Type::Kind::kConst:
            return "const " + FormatType(type.subtypes.front());
        case Type::Kind::kArray:
            return "[" + type.metadata + "]" + FormatType(type.subtypes.front());
        case Type::Kind::kTuple: {
            std::ostringstream stream;
            stream << "tuple<";
            for (std::size_t index = 0; index < type.subtypes.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << FormatType(type.subtypes[index]);
            }
            stream << '>';
            return stream.str();
        }
        case Type::Kind::kRange:
            return "range<" + FormatType(type.subtypes.front()) + ">";
    }

    return "unknown";
}

bool IsUnknown(const Type& type) {
    return type.kind == Type::Kind::kUnknown;
}

}  // namespace mc::sema