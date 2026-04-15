#include "compiler/sema/type.h"

#include <sstream>

#include "compiler/support/diagnostics.h"

namespace mc::sema {
namespace {

bool IsStringBuiltinName(std::string_view name) {
    return name == "str" || name == "string" || name == "cstr";
}

std::string RenderExprInline(const ast::Expr& expr);

std::string RenderTypeInline(const ast::TypeExpr& type) {
    switch (type.kind) {
        case ast::TypeExpr::Kind::kNamed: {
            if (type.type_args.empty()) {
                return type.name;
            }
            std::ostringstream stream;
            stream << type.name << '<';
            for (std::size_t index = 0; index < type.type_args.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << RenderTypeInline(*type.type_args[index]);
            }
            stream << '>';
            return stream.str();
        }
        case ast::TypeExpr::Kind::kPointer:
            return "*" + (type.inner != nullptr ? RenderTypeInline(*type.inner) : std::string("<?>"));
        case ast::TypeExpr::Kind::kConst:
            return "const " + (type.inner != nullptr ? RenderTypeInline(*type.inner) : std::string("<?>"));
        case ast::TypeExpr::Kind::kArray:
            return "[" + (type.length_expr != nullptr ? RenderExprInline(*type.length_expr) : std::string("?")) + "]" +
                   (type.inner != nullptr ? RenderTypeInline(*type.inner) : std::string("<?>"));
        case ast::TypeExpr::Kind::kParen:
            return "(" + (type.inner != nullptr ? RenderTypeInline(*type.inner) : std::string("<?>")) + ")";
        case ast::TypeExpr::Kind::kProcedure: {
            std::ostringstream stream;
            stream << "func(";
            for (std::size_t index = 0; index < type.params.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << RenderTypeInline(*type.params[index]);
            }
            stream << ')';
            if (type.returns.size() == 1) {
                stream << ' ' << RenderTypeInline(*type.returns.front());
            } else if (!type.returns.empty()) {
                stream << " (";
                for (std::size_t index = 0; index < type.returns.size(); ++index) {
                    if (index > 0) {
                        stream << ", ";
                    }
                    stream << RenderTypeInline(*type.returns[index]);
                }
                stream << ')';
            }
            return stream.str();
        }
    }

    return "<?>";
}

std::string RenderExprInline(const ast::Expr& expr) {
    switch (expr.kind) {
        case ast::Expr::Kind::kName:
        case ast::Expr::Kind::kQualifiedName: {
            const std::string base = expr.kind == ast::Expr::Kind::kQualifiedName ? expr.text + "." + expr.secondary_text : expr.text;
            if (expr.type_args.empty()) {
                return base;
            }
            std::ostringstream stream;
            stream << base << '<';
            for (std::size_t index = 0; index < expr.type_args.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << RenderTypeInline(*expr.type_args[index]);
            }
            stream << '>';
            return stream.str();
        }
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
            stream << (expr.type_target != nullptr ? RenderTypeInline(*expr.type_target)
                                                   : (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")))
                   << '(';
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
            return expr.type_target != nullptr ? RenderTypeInline(*expr.type_target)
                                               : (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>"));
        case ast::Expr::Kind::kParen:
            return "(" + (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + ")";
    }

    return "<?>";
}

}  // namespace

bool operator==(const Type& left, const Type& right) {
    const Type canonical_left = CanonicalizeBuiltinType(left);
    const Type canonical_right = CanonicalizeBuiltinType(right);
    return canonical_left.kind == canonical_right.kind && canonical_left.name == canonical_right.name &&
           canonical_left.metadata == canonical_right.metadata && canonical_left.subtypes == canonical_right.subtypes;
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

Type ProcedureType(std::vector<Type> params, std::vector<Type> returns) {
    Type type {
        .kind = Type::Kind::kProcedure,
        .metadata = std::to_string(params.size()),
    };
    type.subtypes = std::move(params);
    type.subtypes.insert(type.subtypes.end(), std::make_move_iterator(returns.begin()), std::make_move_iterator(returns.end()));
    return type;
}

Type NamedOrBuiltinType(std::string_view name) {
    if (name == "bool") {
        return BoolType();
    }
    if (IsStringBuiltinName(name)) {
        return StringType();
    }
    return NamedType(std::string(name));
}

Type CanonicalizeBuiltinType(Type type) {
    for (auto& subtype : type.subtypes) {
        subtype = CanonicalizeBuiltinType(std::move(subtype));
    }

    if (type.kind != Type::Kind::kNamed) {
        return type;
    }
    if (type.name == "bool" && type.subtypes.empty()) {
        return BoolType();
    }
    if (IsStringBuiltinName(type.name) && type.subtypes.empty()) {
        return StringType();
    }
    return type;
}

std::vector<std::pair<std::string, Type>> BuiltinAggregateFields(const Type& raw_type) {
    const Type type = CanonicalizeBuiltinType(raw_type);
    std::vector<std::pair<std::string, Type>> fields;
    if (type.kind == Type::Kind::kTuple) {
        fields.reserve(type.subtypes.size());
        for (std::size_t index = 0; index < type.subtypes.size(); ++index) {
            fields.push_back({std::to_string(index), type.subtypes[index]});
        }
        return fields;
    }
    if (type.kind == Type::Kind::kString) {
        fields.push_back({"ptr", PointerType(NamedType("u8"))});
        fields.push_back({"len", NamedType("usize")});
        return fields;
    }
    if (type.kind != Type::Kind::kNamed) {
        return fields;
    }
    if (type.name == "Slice") {
        const Type element_type = type.subtypes.empty() ? UnknownType() : type.subtypes.front();
        fields.push_back({"ptr", PointerType(element_type)});
        fields.push_back({"len", NamedType("usize")});
        return fields;
    }
    if (type.name == "Buffer") {
        const Type element_type = type.subtypes.empty() ? UnknownType() : type.subtypes.front();
        fields.push_back({"ptr", PointerType(element_type)});
        fields.push_back({"len", NamedType("usize")});
        fields.push_back({"cap", NamedType("usize")});
        fields.push_back({"alloc", PointerType(NamedType("Allocator"))});
        return fields;
    }
    return fields;
}

std::unique_ptr<ast::TypeExpr> TypeToAst(const Type& raw_type) {
    const Type type = CanonicalizeBuiltinType(raw_type);
    auto expr = std::make_unique<ast::TypeExpr>();
    switch (type.kind) {
        case Type::Kind::kBool:
            expr->kind = ast::TypeExpr::Kind::kNamed;
            expr->name = "bool";
            return expr;
        case Type::Kind::kString:
            expr->kind = ast::TypeExpr::Kind::kNamed;
            expr->name = "string";
            return expr;
        case Type::Kind::kNamed:
            expr->kind = ast::TypeExpr::Kind::kNamed;
            expr->name = type.name;
            expr->type_args.reserve(type.subtypes.size());
            for (const auto& subtype : type.subtypes) {
                auto type_arg = TypeToAst(subtype);
                if (type_arg == nullptr) {
                    return nullptr;
                }
                expr->type_args.push_back(std::move(type_arg));
            }
            return expr;
        case Type::Kind::kPointer:
            if (type.subtypes.empty()) {
                return nullptr;
            }
            expr->kind = ast::TypeExpr::Kind::kPointer;
            expr->inner = TypeToAst(type.subtypes.front());
            return expr->inner != nullptr ? std::move(expr) : nullptr;
        case Type::Kind::kConst:
            if (type.subtypes.empty()) {
                return nullptr;
            }
            expr->kind = ast::TypeExpr::Kind::kConst;
            expr->inner = TypeToAst(type.subtypes.front());
            return expr->inner != nullptr ? std::move(expr) : nullptr;
        case Type::Kind::kArray:
            if (type.subtypes.empty()) {
                return nullptr;
            }
            expr->kind = ast::TypeExpr::Kind::kArray;
            expr->inner = TypeToAst(type.subtypes.front());
            if (expr->inner == nullptr) {
                return nullptr;
            }
            expr->length_expr = std::make_unique<ast::Expr>();
            expr->length_expr->kind = ast::Expr::Kind::kLiteral;
            expr->length_expr->text = type.metadata;
            expr->length_expr->secondary_text = "int_lit";
            if (const auto length = mc::support::ParseArrayLength(type.metadata); length.has_value()) {
                expr->length_expr->integer_literal_value = static_cast<std::int64_t>(*length);
            }
            return expr;
        default:
            return nullptr;
    }
}

Type TypeFromAst(const ast::TypeExpr* type_expr) {
    if (type_expr == nullptr) {
        return UnknownType();
    }

    switch (type_expr->kind) {
        case ast::TypeExpr::Kind::kNamed: {
            Type type = NamedOrBuiltinType(type_expr->name);
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
        case ast::TypeExpr::Kind::kProcedure: {
            std::vector<Type> params;
            params.reserve(type_expr->params.size());
            for (const auto& param : type_expr->params) {
                params.push_back(TypeFromAst(param.get()));
            }
            std::vector<Type> returns;
            returns.reserve(type_expr->returns.size());
            for (const auto& ret : type_expr->returns) {
                returns.push_back(TypeFromAst(ret.get()));
            }
            return ProcedureType(std::move(params), std::move(returns));
        }
    }

    return UnknownType();
}

std::string FormatType(const Type& type) {
    const Type canonical = CanonicalizeBuiltinType(type);
    switch (canonical.kind) {
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
            if (canonical.subtypes.empty()) {
                return canonical.name;
            }

            std::ostringstream stream;
            stream << canonical.name << '<';
            for (std::size_t index = 0; index < canonical.subtypes.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << FormatType(canonical.subtypes[index]);
            }
            stream << '>';
            return stream.str();
        }
        case Type::Kind::kPointer:
            return "*" + FormatType(canonical.subtypes.front());
        case Type::Kind::kConst:
            return "const " + FormatType(canonical.subtypes.front());
        case Type::Kind::kArray:
            return "[" + canonical.metadata + "]" + FormatType(canonical.subtypes.front());
        case Type::Kind::kTuple: {
            std::ostringstream stream;
            stream << "tuple<";
            for (std::size_t index = 0; index < canonical.subtypes.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << FormatType(canonical.subtypes[index]);
            }
            stream << '>';
            return stream.str();
        }
        case Type::Kind::kRange:
            return "range<" + FormatType(canonical.subtypes.front()) + ">";
        case Type::Kind::kProcedure: {
            std::size_t param_count = 0;
            if (!canonical.metadata.empty()) {
                param_count = mc::support::ParseArrayLength(canonical.metadata).value_or(0);
            }
            std::ostringstream stream;
            stream << "proc(";
            for (std::size_t index = 0; index < param_count && index < canonical.subtypes.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << FormatType(canonical.subtypes[index]);
            }
            stream << ")";
            const std::size_t return_count = canonical.subtypes.size() >= param_count ? canonical.subtypes.size() - param_count : 0;
            if (return_count == 0) {
                stream << "->void";
            } else if (return_count == 1) {
                stream << "->" << FormatType(canonical.subtypes[param_count]);
            } else {
                stream << "->tuple<";
                for (std::size_t index = 0; index < return_count; ++index) {
                    if (index > 0) {
                        stream << ", ";
                    }
                    stream << FormatType(canonical.subtypes[param_count + index]);
                }
                stream << '>';
            }
            return stream.str();
        }
    }

    return "unknown";
}

bool IsUnknown(const Type& type) {
    return type.kind == Type::Kind::kUnknown;
}

}  // namespace mc::sema