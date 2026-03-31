#include "compiler/parse/parser.h"

namespace mc::parse {

ParseResult ParseForBootstrap(const mc::lex::LexResult& lexed_module) {
    ParseResult result;
    result.ok = lexed_module.complete && !lexed_module.tokens.empty();
    result.stage = "phase1-bootstrap";
    return result;
}

}  // namespace mc::parse
