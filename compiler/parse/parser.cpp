#include "compiler/parse/parser_internal.h"

#include <cassert>
#include <utility>

namespace mc::parse {

Parser::Parser(const mc::lex::LexResult& lexed_module,
               const std::filesystem::path& file_path,
               mc::support::DiagnosticSink& diagnostics)
    : tokens_(lexed_module.tokens), file_path_(file_path), diagnostics_(diagnostics), ok_(lexed_module.ok) {}

ParseResult Parser::Run() {
    auto source_file = std::make_unique<SourceFile>();
    source_file->span.begin = Current().span.begin;
    if (file_path_.filename() == "internal.mc") {
        source_file->module_kind = SourceFile::ModuleKind::kInternal;
    }

    SkipNewlines();

    while (Match(TokenKind::kImport)) {
        source_file->imports.push_back(ParseImportDecl(Previous().span.begin));
        RequireSourceSeparator("import declaration");
    }

    SkipNewlines();
    while (!AtEnd()) {
        auto decl = ParseTopLevelDecl();
        if (decl.has_value()) {
            source_file->decls.push_back(std::move(*decl));
        } else {
            SynchronizeTopLevel();
        }

        SkipNewlines();
    }

    source_file->span.end = Current().span.end;
    return {
        .source_file = std::move(source_file),
        .ok = ok_ && !diagnostics_.HasErrors(),
    };
}

bool Parser::AtEnd() const {
    return Current().kind == TokenKind::kEof;
}

const Token& Parser::Current() const {
    return tokens_[index_];
}

const Token& Parser::Previous() const {
    assert(index_ > 0 && "Previous() called before any Advance()");
    return tokens_[index_ - 1];
}

const Token& Parser::Peek(std::size_t offset) const {
    const auto target = index_ + offset;
    return tokens_[target < tokens_.size() ? target : tokens_.size() - 1];
}

bool Parser::Check(TokenKind kind) const {
    return Current().kind == kind;
}

bool Parser::Match(TokenKind kind) {
    if (!Check(kind)) {
        return false;
    }
    Advance();
    return true;
}

const Token& Parser::Advance() {
    if (!AtEnd()) {
        ++index_;
    }
    return Previous();
}

const Token* Parser::Consume(TokenKind kind, const char* message) {
    if (Match(kind)) {
        return &Previous();
    }
    ReportError(Current(), message);
    return nullptr;
}

void Parser::ReportError(const Token& token, const std::string& message) {
    diagnostics_.Report({
        .file_path = file_path_,
        .span = token.span,
        .severity = DiagnosticSeverity::kError,
        .message = message,
    });
    ok_ = false;
}

void Parser::SkipNewlines() {
    while (Match(TokenKind::kNewline)) {
    }
}

bool Parser::RequireSourceSeparator(const char* context) {
    if (AtEnd() || Check(TokenKind::kNewline)) {
        SkipNewlines();
        return true;
    }

    ReportError(Current(), std::string("expected newline after ") + context);
    return false;
}

void Parser::SynchronizeTopLevel() {
    while (!AtEnd()) {
        if (Match(TokenKind::kNewline)) {
            SkipNewlines();
            return;
        }

        if (Check(TokenKind::kFunc) || Check(TokenKind::kStruct) || Check(TokenKind::kEnum) || Check(TokenKind::kDistinct) ||
            Check(TokenKind::kType) || Check(TokenKind::kVar) || Check(TokenKind::kConst) || Check(TokenKind::kExtern) ||
            Check(TokenKind::kAt)) {
            return;
        }

        Advance();
    }
}

std::optional<std::string> Parser::ParseIdentifier(const char* message) {
    if (!Check(TokenKind::kIdentifier)) {
        ReportError(Current(), message);
        return std::nullopt;
    }
    const auto text = Current().lexeme;
    Advance();
    return text;
}

bool Parser::LooksLikeBindingStmt() const {
    if (Current().kind != TokenKind::kIdentifier) {
        return false;
    }

    std::size_t offset = 1;
    while (Peek(offset).kind == TokenKind::kComma && Peek(offset + 1).kind == TokenKind::kIdentifier) {
        offset += 2;
    }

    return Peek(offset).kind == TokenKind::kColon;
}

void Parser::SkipStatementSeparator() {
    if (Match(TokenKind::kNewline)) {
        SkipNewlines();
        return;
    }
    if (Check(TokenKind::kRBrace) || Check(TokenKind::kCase) || Check(TokenKind::kDefault)) {
        return;
    }
    ReportError(Current(), "expected newline between statements");
}

void Parser::ConsumeMemberSeparator() {
    if (Match(TokenKind::kComma)) {
        SkipNewlines();
        return;
    }
    if (Match(TokenKind::kNewline)) {
        SkipNewlines();
        return;
    }
    if (Check(TokenKind::kRBrace)) {
        return;
    }
    ReportError(Current(), "expected member separator");
}

void Parser::SynchronizeStatement() {
    while (!AtEnd()) {
        if (Match(TokenKind::kNewline)) {
            SkipNewlines();
            return;
        }
        if (Check(TokenKind::kRBrace) || Check(TokenKind::kCase) || Check(TokenKind::kDefault)) {
            return;
        }
        Advance();
    }
}

bool Parser::IsLiteral(TokenKind kind) const {
    return kind == TokenKind::kIntLiteral || kind == TokenKind::kFloatLiteral || kind == TokenKind::kStringLiteral ||
           kind == TokenKind::kTrue || kind == TokenKind::kFalse || kind == TokenKind::kNil;
}

bool Parser::IsAggregateInitBase(const Expr& expr) const {
    return expr.kind == Expr::Kind::kName || expr.kind == Expr::Kind::kQualifiedName;
}

bool Parser::IsBareNameExpr(const Expr& expr) const {
    return expr.kind == Expr::Kind::kName;
}

bool Parser::LooksLikeBareNameListAssignment() const {
    if (!Check(TokenKind::kComma)) {
        return false;
    }

    std::size_t offset = 0;
    while (Peek(offset).kind == TokenKind::kComma && Peek(offset + 1).kind == TokenKind::kIdentifier) {
        offset += 2;
    }

    return Peek(offset).kind == TokenKind::kAssign;
}

Parser::ParsedBindingTail Parser::ParseBindingTail(bool allow_storage_without_initializer,
                                                   const char* missing_initializer_message) {
    ParsedBindingTail binding;
    binding.pattern = ParseBindingPattern();

    if (Match(TokenKind::kColon)) {
        binding.type_ann = ParseTypeExpr();
    }

    if (Match(TokenKind::kAssign)) {
        binding.has_initializer = true;
        binding.initializers = ParseExprList();
    } else if (!allow_storage_without_initializer || binding.type_ann == nullptr) {
        ReportError(Current(), missing_initializer_message);
    }

    binding.end = Previous().span.end;
    return binding;
}

bool Parser::IsAssignmentTarget(const Expr& expr) const {
    switch (expr.kind) {
        case Expr::Kind::kName:
        case Expr::Kind::kQualifiedName:
        case Expr::Kind::kField:
        case Expr::Kind::kDerefField:
        case Expr::Kind::kIndex:
            return true;
        case Expr::Kind::kParen:
            return expr.left != nullptr && IsAssignmentTarget(*expr.left);
        default:
            return false;
    }
}

bool Parser::IsCaseBoundary() const {
    return Check(TokenKind::kCase) || Check(TokenKind::kDefault) || Check(TokenKind::kRBrace);
}

bool Parser::IsPrefixUnaryOperator(TokenKind kind) const {
    return kind == TokenKind::kBang || kind == TokenKind::kMinus || kind == TokenKind::kAmp || kind == TokenKind::kStar;
}

bool Parser::IsStatementTerminator(TokenKind kind) const {
    return kind == TokenKind::kNewline || kind == TokenKind::kRBrace || kind == TokenKind::kEof;
}

std::unique_ptr<Stmt> Parser::MakeLeafStmt(Stmt::Kind kind,
                                           mc::support::SourcePosition begin,
                                           mc::support::SourcePosition end) {
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = kind;
    stmt->span = {begin, end};
    return stmt;
}

Parser::AggInitGuard::AggInitGuard(Parser& parser, bool allow)
    : parser_(parser), saved_(parser.allow_aggregate_init_) {
    parser_.allow_aggregate_init_ = allow;
}

Parser::AggInitGuard::~AggInitGuard() {
    parser_.allow_aggregate_init_ = saved_;
}

ParseResult Parse(const mc::lex::LexResult& lexed_module,
                  const std::filesystem::path& file_path,
                  mc::support::DiagnosticSink& diagnostics) {
    return Parser(lexed_module, file_path, diagnostics).Run();
}

}  // namespace mc::parse
