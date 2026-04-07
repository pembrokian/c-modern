#ifndef C_MODERN_COMPILER_PARSE_PARSER_H_
#define C_MODERN_COMPILER_PARSE_PARSER_H_

#include <filesystem>
#include <memory>

#include "compiler/ast/ast.h"
#include "compiler/lex/lexer.h"
#include "compiler/support/diagnostics.h"

namespace mc::parse {

struct ParseResult {
    std::unique_ptr<ast::SourceFile> source_file;
    bool ok = false;
};

ParseResult Parse(const mc::lex::LexResult& lexed_module,
                  const std::filesystem::path& file_path,
                  mc::support::DiagnosticSink& diagnostics);

}  // namespace mc::parse

#endif  // C_MODERN_COMPILER_PARSE_PARSER_H_
