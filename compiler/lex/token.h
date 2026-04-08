#ifndef C_MODERN_COMPILER_LEX_TOKEN_H_
#define C_MODERN_COMPILER_LEX_TOKEN_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "compiler/support/source_span.h"

namespace mc::lex {

enum class TokenKind {
    kNewline,
    kIdentifier,
    kImport,
    kExport,
    kFunc,
    kStruct,
    kEnum,
    kDistinct,
    kType,
    kExtern,
    kIf,
    kElse,
    kSwitch,
    kCase,
    kDefault,
    kFor,
    kWhile,
    kIn,
    kIs,
    kReturn,
    kBreak,
    kContinue,
    kDefer,
    kVar,
    kConst,
    kNil,
    kTrue,
    kFalse,
    kIntLiteral,
    kFloatLiteral,
    kStringLiteral,
    kLBrace,
    kRBrace,
    kLParen,
    kRParen,
    kLBracket,
    kRBracket,
    kComma,
    kColon,
    kDot,
    kAt,
    kDotDeref,
    kRange,
    kAssign,
    kEqEq,
    kBangEq,
    kLt,
    kLtEq,
    kLtLt,
    kGt,
    kGtEq,
    kGtGt,
    kPlus,
    kMinus,
    kStar,
    kSlash,
    kPercent,
    kAmp,
    kPipe,
    kCaret,
    kBang,
    kAmpAmp,
    kPipePipe,
    kArrow,
    kEof,
};

struct Token {
    TokenKind kind = TokenKind::kEof;
    std::string lexeme;
    std::optional<std::int64_t> integer_value;
    std::optional<double> float_value;
    support::SourceSpan span {};
};

std::string_view ToString(TokenKind kind);

}  // namespace mc::lex

#endif  // C_MODERN_COMPILER_LEX_TOKEN_H_
