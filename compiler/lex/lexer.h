#ifndef C_MODERN_COMPILER_LEX_LEXER_H_
#define C_MODERN_COMPILER_LEX_LEXER_H_

#include <string_view>
#include <vector>

#include "compiler/lex/token.h"

namespace mc::lex {

struct LexResult {
    std::vector<TokenKind> tokens;
    bool complete = false;
};

LexResult LexForBootstrap(std::string_view source_text);

}  // namespace mc::lex

#endif  // C_MODERN_COMPILER_LEX_LEXER_H_
