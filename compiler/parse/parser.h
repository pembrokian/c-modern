#ifndef C_MODERN_COMPILER_PARSE_PARSER_H_
#define C_MODERN_COMPILER_PARSE_PARSER_H_

#include <string>

#include "compiler/lex/lexer.h"

namespace mc::parse {

struct ParseResult {
    bool ok = false;
    std::string stage;
};

ParseResult ParseForBootstrap(const mc::lex::LexResult& lexed_module);

}  // namespace mc::parse

#endif  // C_MODERN_COMPILER_PARSE_PARSER_H_
