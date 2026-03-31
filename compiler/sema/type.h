#ifndef C_MODERN_COMPILER_SEMA_TYPE_H_
#define C_MODERN_COMPILER_SEMA_TYPE_H_

#include <string>
#include <vector>

#include "compiler/ast/ast.h"

namespace mc::sema {

struct Type {
    enum class Kind {
        kUnknown,
        kVoid,
        kBool,
        kString,
        kNil,
        kIntLiteral,
        kFloatLiteral,
        kNamed,
        kPointer,
        kConst,
        kArray,
        kTuple,
        kRange,
    };

    Kind kind = Kind::kUnknown;
    std::string name;
    std::string metadata;
    std::vector<Type> subtypes;
};

bool operator==(const Type& left, const Type& right);
bool operator!=(const Type& left, const Type& right);

Type UnknownType();
Type VoidType();
Type BoolType();
Type StringType();
Type NilType();
Type IntLiteralType();
Type FloatLiteralType();
Type NamedType(std::string name);
Type PointerType(Type pointee);
Type ConstType(Type inner);
Type ArrayType(Type inner, std::string length_text);
Type TupleType(std::vector<Type> elements);
Type RangeType(Type element);

Type TypeFromAst(const ast::TypeExpr* type_expr);
std::string FormatType(const Type& type);
bool IsUnknown(const Type& type);

}  // namespace mc::sema

#endif  // C_MODERN_COMPILER_SEMA_TYPE_H_