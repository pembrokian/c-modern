#ifndef C_MODERN_COMPILER_LEX_LEXER_H_
#define C_MODERN_COMPILER_LEX_LEXER_H_

#include <string>
#include <string_view>
#include <vector>

#include "compiler/lex/token.h"
#include "compiler/support/diagnostics.h"

namespace mc::lex {

struct LexResult {
    std::vector<Token> tokens;
    bool ok = true;
};

LexResult Lex(std::string_view source_text,
              std::string file_path,
              support::DiagnosticSink& diagnostics);

}  // namespace mc::lex

#endif  // C_MODERN_COMPILER_LEX_LEXER_H_
