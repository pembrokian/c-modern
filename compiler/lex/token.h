#ifndef C_MODERN_COMPILER_LEX_TOKEN_H_
#define C_MODERN_COMPILER_LEX_TOKEN_H_

#include <string_view>

namespace mc::lex {

enum class TokenKind {
    kIdentifier,
    kKeyword,
    kIntegerLiteral,
    kStringLiteral,
    kPunctuation,
    kAttribute,
    kEndOfFile,
};

inline std::string_view ToString(TokenKind kind) {
    switch (kind) {
        case TokenKind::kIdentifier:
            return "identifier";
        case TokenKind::kKeyword:
            return "keyword";
        case TokenKind::kIntegerLiteral:
            return "integer_literal";
        case TokenKind::kStringLiteral:
            return "string_literal";
        case TokenKind::kPunctuation:
            return "punctuation";
        case TokenKind::kAttribute:
            return "attribute";
        case TokenKind::kEndOfFile:
            return "eof";
    }

    return "unknown";
}

}  // namespace mc::lex

#endif  // C_MODERN_COMPILER_LEX_TOKEN_H_
