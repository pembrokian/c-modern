#ifndef C_MODERN_COMPILER_SEMA_TYPE_PREDICATES_H_
#define C_MODERN_COMPILER_SEMA_TYPE_PREDICATES_H_

#include <string_view>

#include "compiler/sema/type.h"

namespace mc::sema {

inline bool IsIntegerTypeName(std::string_view name) {
    return name == "i8" || name == "i16" || name == "i32" || name == "i64" || name == "isize" || name == "u8" ||
           name == "u16" || name == "u32" || name == "u64" || name == "usize" || name == "uintptr";
}

inline bool IsFloatTypeName(std::string_view name) {
    return name == "f32" || name == "f64";
}

inline bool IsIntegerType(const Type& type) {
    return type.kind == Type::Kind::kNamed && IsIntegerTypeName(type.name);
}

inline bool IsFloatType(const Type& type) {
    return type.kind == Type::Kind::kNamed && IsFloatTypeName(type.name);
}

inline bool IsNumericType(const Type& type) {
    return type.kind == Type::Kind::kIntLiteral || type.kind == Type::Kind::kFloatLiteral || IsIntegerType(type) || IsFloatType(type);
}

inline bool IsIntegerLikeType(const Type& type) {
    return type.kind == Type::Kind::kIntLiteral || IsIntegerType(type);
}

inline bool IsBoolType(const Type& type) {
    return type.kind == Type::Kind::kBool || (type.kind == Type::Kind::kNamed && type.name == "bool");
}

inline bool IsPointerLikeType(const Type& type) {
    return type.kind == Type::Kind::kPointer;
}

inline bool IsUintPtrConvertibleType(const Type& type) {
    return type.kind == Type::Kind::kPointer || type.kind == Type::Kind::kProcedure;
}

inline bool IsUintPtrType(const Type& type) {
    return type.kind == Type::Kind::kNamed && type.name == "uintptr";
}

}  // namespace mc::sema

#endif  // C_MODERN_COMPILER_SEMA_TYPE_PREDICATES_H_