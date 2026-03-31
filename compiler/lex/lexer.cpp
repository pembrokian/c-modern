#include "compiler/lex/lexer.h"

namespace mc::lex {

LexResult LexForBootstrap(std::string_view source_text) {
    LexResult result;

    if (!source_text.empty()) {
        result.tokens.push_back(TokenKind::kIdentifier);
    }

    result.tokens.push_back(TokenKind::kEndOfFile);
    result.complete = true;
    return result;
}

}  // namespace mc::lex
