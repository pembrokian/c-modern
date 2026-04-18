#include "compiler/parse/parser_internal.h"

#include <utility>

namespace mc::parse {

std::unique_ptr<Stmt> Parser::ParseBlockStmt() {
    if (!Match(TokenKind::kLBrace)) {
        ReportError(Current(), "expected '{' to start block");
        auto empty = std::make_unique<Stmt>();
        empty->kind = Stmt::Kind::kBlock;
        empty->span = Current().span;
        return empty;
    }

    auto block = std::make_unique<Stmt>();
    block->kind = Stmt::Kind::kBlock;
    block->span.begin = Previous().span.begin;
    SkipNewlines();

    while (!Check(TokenKind::kRBrace) && !AtEnd()) {
        if (auto statement = ParseStatement(); statement != nullptr) {
            block->statements.push_back(std::move(statement));
        } else {
            SynchronizeStatement();
        }
        RequireStatementSeparatorOrSync();
    }

    const auto* close = Consume(TokenKind::kRBrace, "expected '}' to close block");
    block->span.end = close != nullptr ? close->span.end : Previous().span.end;
    return block;
}

std::unique_ptr<Stmt> Parser::ParseStatement() {
    if (Check(TokenKind::kLBrace)) {
        return ParseBlockStmt();
    }
    if (Match(TokenKind::kIf)) {
        return ParseIfStmt(Previous().span.begin);
    }
    if (Match(TokenKind::kSwitch)) {
        return ParseSwitchStmt(Previous().span.begin);
    }
    if (Match(TokenKind::kFor)) {
        return ParseForStmt(Previous().span.begin);
    }
    if (Match(TokenKind::kWhile)) {
        return ParseWhileStmt(Previous().span.begin);
    }
    if (Match(TokenKind::kReturn)) {
        return ParseReturnStmt(Previous().span.begin);
    }
    if (Match(TokenKind::kBreak)) {
        return MakeLeafStmt(Stmt::Kind::kBreak, Previous().span.begin, Previous().span.end);
    }
    if (Match(TokenKind::kContinue)) {
        return MakeLeafStmt(Stmt::Kind::kContinue, Previous().span.begin, Previous().span.end);
    }
    if (Match(TokenKind::kDefer)) {
        return ParseDeferStmt(Previous().span.begin);
    }
    if (Match(TokenKind::kVar)) {
        return ParseBindingStmt(Stmt::Kind::kVar, Previous().span.begin, true);
    }
    if (Match(TokenKind::kConst)) {
        return ParseBindingStmt(Stmt::Kind::kConst, Previous().span.begin, false);
    }

    if (LooksLikeBindingStmt()) {
        return ParseBindingStmt(Stmt::Kind::kBinding, Current().span.begin, true);
    }

    return ParseAssignOrExprStmt();
}

std::unique_ptr<Stmt> Parser::ParseIfStmt(mc::support::SourcePosition start) {
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = Stmt::Kind::kIf;
    stmt->span.begin = start;
    stmt->exprs.push_back(ParseExpr(false));
    stmt->then_branch = ParseBlockStmt();
    if (Match(TokenKind::kElse)) {
        if (Check(TokenKind::kIf)) {
            Advance();
            stmt->else_branch = ParseIfStmt(Previous().span.begin);
        } else {
            stmt->else_branch = ParseBlockStmt();
        }
    }
    stmt->span.end = stmt->else_branch != nullptr
                         ? stmt->else_branch->span.end
                         : (stmt->then_branch != nullptr ? stmt->then_branch->span.end : Current().span.end);
    return stmt;
}

std::unique_ptr<Stmt> Parser::ParseSwitchStmt(mc::support::SourcePosition start) {
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = Stmt::Kind::kSwitch;
    stmt->span.begin = start;
    stmt->exprs.push_back(ParseExpr(false));
    Consume(TokenKind::kLBrace, "expected '{' after switch expression");
    SkipNewlines();
    while (!Check(TokenKind::kRBrace) && !AtEnd()) {
        if (Match(TokenKind::kCase)) {
            stmt->switch_cases.push_back(ParseSwitchCase(Previous().span.begin));
        } else if (Match(TokenKind::kDefault)) {
            stmt->has_default_case = true;
            stmt->default_case = ParseDefaultCase(Previous().span.begin);
        } else {
            ReportError(Current(), "expected case or default in switch");
            SynchronizeStatement();
        }
        RequireStatementSeparatorOrSync();
    }
    const auto* close = Consume(TokenKind::kRBrace, "expected '}' after switch body");
    stmt->span.end = close != nullptr ? close->span.end : Previous().span.end;
    return stmt;
}

SwitchCase Parser::ParseSwitchCase(mc::support::SourcePosition start) {
    SwitchCase case_node;
    case_node.span.begin = start;
    case_node.pattern = ParseCasePattern();
    Consume(TokenKind::kColon, "expected ':' after case pattern");
    case_node.statements = ParseCaseBodyStatements();
    case_node.span.end = Previous().span.end;
    return case_node;
}

DefaultCase Parser::ParseDefaultCase(mc::support::SourcePosition start) {
    DefaultCase case_node;
    case_node.span.begin = start;
    Consume(TokenKind::kColon, "expected ':' after default");
    case_node.statements = ParseCaseBodyStatements();
    case_node.span.end = Previous().span.end;
    return case_node;
}

std::vector<std::unique_ptr<Stmt>> Parser::ParseCaseBodyStatements() {
    std::vector<std::unique_ptr<Stmt>> statements;
    if (!IsCaseBoundary() && !Check(TokenKind::kNewline) && !AtEnd()) {
        statements.push_back(ParseStatement());
    }
    RequireStatementSeparatorOrSync();
    while (!IsCaseBoundary() && !AtEnd()) {
        statements.push_back(ParseStatement());
        RequireStatementSeparatorOrSync();
    }
    return statements;
}

bool Parser::LooksLikeVariantCasePattern() const {
    if (!Check(TokenKind::kIdentifier)) {
        return false;
    }
    if (Peek(1).kind == TokenKind::kLParen) {
        return true;
    }
    if (Peek(1).kind != TokenKind::kDot || Peek(2).kind != TokenKind::kIdentifier) {
        return false;
    }
    if (Peek(3).kind == TokenKind::kLParen) {
        return true;
    }
    return Peek(3).kind == TokenKind::kDot && Peek(4).kind == TokenKind::kIdentifier;
}

std::string Parser::ParseCaseVariantName() {
    std::string name;
    if (!Check(TokenKind::kIdentifier)) {
        ReportError(Current(), "expected variant name");
        return name;
    }

    name = Current().lexeme;
    Advance();
    if (!Match(TokenKind::kDot)) {
        return name;
    }

    const auto second = ParseIdentifier("expected variant name after '.'");
    if (!second.has_value()) {
        return name;
    }
    name += "." + *second;

    if (!Match(TokenKind::kDot)) {
        return name;
    }

    const auto third = ParseIdentifier("expected variant name after '.'");
    if (third.has_value()) {
        name += "." + *third;
    }
    return name;
}

void Parser::ParseCasePatternBindings(CasePattern& pattern) {
    if (!Match(TokenKind::kLParen)) {
        return;
    }

    if (!Check(TokenKind::kRParen)) {
        do {
            const auto binding = ParseIdentifier("expected pattern binding name");
            if (binding.has_value()) {
                pattern.bindings.push_back(*binding);
            }
        } while (Match(TokenKind::kComma));
    }
    Consume(TokenKind::kRParen, "expected ')' after pattern bindings");
}

CasePattern Parser::ParseCasePattern() {
    CasePattern pattern;
    pattern.span.begin = Current().span.begin;
    if (LooksLikeVariantCasePattern()) {
        pattern.kind = CasePattern::Kind::kVariant;
        pattern.variant_name = ParseCaseVariantName();
        ParseCasePatternBindings(pattern);
        pattern.span.end = Previous().span.end;
        return pattern;
    }

    pattern.kind = CasePattern::Kind::kExpr;
    pattern.expr = ParseExpr();
    pattern.span.end = pattern.expr != nullptr ? pattern.expr->span.end : Previous().span.end;
    return pattern;
}

std::unique_ptr<Stmt> Parser::ParseForStmt(mc::support::SourcePosition start) {
    if (Check(TokenKind::kIdentifier) && Peek(1).kind == TokenKind::kIn) {
        std::string name = Current().lexeme;
        Advance();
        Advance();
        auto iterable = ParseExpr(false);
        auto body = ParseBlockStmt();
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = Stmt::Kind::kForIn;
        stmt->span.begin = start;
        stmt->loop_name = std::move(name);
        stmt->exprs.push_back(std::move(iterable));
        stmt->then_branch = std::move(body);
        stmt->span.end = stmt->then_branch != nullptr ? stmt->then_branch->span.end : Current().span.end;
        return stmt;
    }

    if (Check(TokenKind::kIdentifier) && Peek(1).kind == TokenKind::kComma && Peek(2).kind == TokenKind::kIdentifier &&
        Peek(3).kind == TokenKind::kIn) {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = Stmt::Kind::kForEachIndex;
        stmt->span.begin = start;
        stmt->loop_name = Current().lexeme;
        Advance();
        Advance();
        stmt->loop_second_name = Current().lexeme;
        Advance();
        Advance();
        stmt->exprs.push_back(ParseExpr(false));
        stmt->then_branch = ParseBlockStmt();
        stmt->span.end = stmt->then_branch != nullptr ? stmt->then_branch->span.end : Current().span.end;
        return stmt;
    }

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = Stmt::Kind::kForCondition;
    stmt->span.begin = start;
    stmt->exprs.push_back(ParseExpr(false));
    stmt->then_branch = ParseBlockStmt();
    stmt->span.end = stmt->then_branch != nullptr ? stmt->then_branch->span.end : Current().span.end;
    return stmt;
}

std::unique_ptr<Stmt> Parser::ParseWhileStmt(mc::support::SourcePosition start) {
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = Stmt::Kind::kWhile;
    stmt->span.begin = start;
    stmt->exprs.push_back(ParseExpr(false));
    stmt->then_branch = ParseBlockStmt();
    stmt->span.end = stmt->then_branch != nullptr ? stmt->then_branch->span.end : Current().span.end;
    return stmt;
}

std::unique_ptr<Stmt> Parser::ParseReturnStmt(mc::support::SourcePosition start) {
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = Stmt::Kind::kReturn;
    stmt->span.begin = start;
    if (!IsStatementTerminator(Current().kind)) {
        stmt->exprs = ParseExprList();
    }
    stmt->span.end = Previous().span.end;
    return stmt;
}

std::unique_ptr<Stmt> Parser::ParseDeferStmt(mc::support::SourcePosition start) {
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = Stmt::Kind::kDefer;
    stmt->span.begin = start;
    stmt->exprs.push_back(ParseExpr());
    stmt->span.end = stmt->exprs.back()->span.end;
    return stmt;
}

std::unique_ptr<Stmt> Parser::ParseBindingStmt(Stmt::Kind kind,
                                               mc::support::SourcePosition start,
                                               bool allow_storage_without_initializer) {
    auto stmt = std::make_unique<Stmt>();
    stmt->kind = kind;
    stmt->span.begin = start;
    auto binding = ParseBindingTail(allow_storage_without_initializer,
                                    kind == Stmt::Kind::kBinding,
                                    true,
                                    kind == Stmt::Kind::kConst ? "const statement requires initializer"
                                                               : "binding requires either a type or initializer");
    stmt->pattern = std::move(binding.pattern);
    stmt->type_ann = std::move(binding.type_ann);
    stmt->has_initializer = binding.has_initializer;
    stmt->exprs = std::move(binding.initializers);
    stmt->span.end = binding.end;
    return stmt;
}

std::unique_ptr<Stmt> Parser::ParseAssignOrExprStmt() {
    const auto start = Current().span.begin;
    auto first = ParseExpr();
    if (Match(TokenKind::kAssign)) {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = Stmt::Kind::kAssign;
        stmt->span.begin = start;
        stmt->assign_targets.push_back(NormalizeAssignmentTarget(std::move(first)));
        stmt->assign_values = ParseExprList();
        stmt->span.end = Previous().span.end;
        return stmt;
    }

    if (Check(TokenKind::kComma)) {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = Stmt::Kind::kAssign;
        stmt->span.begin = start;

        first = NormalizeAssignmentTarget(std::move(first));
        if (!IsAssignmentTarget(*first)) {
            diagnostics_.Report({
                .file_path = file_path_,
                .span = first->span,
                .severity = DiagnosticSeverity::kError,
                .message = "expected assignment target before ','",
            });
            ok_ = false;
        }

        stmt->assign_targets.push_back(std::move(first));
        while (Match(TokenKind::kComma)) {
            stmt->assign_targets.push_back(ParseAssignmentTarget());
        }

        Consume(TokenKind::kAssign, "expected '=' after assignment targets");
        stmt->assign_values = ParseExprList();
        stmt->span.end = Previous().span.end;
        return stmt;
    }

    auto stmt = std::make_unique<Stmt>();
    stmt->kind = Stmt::Kind::kExpr;
    stmt->span.begin = start;
    stmt->exprs.push_back(std::move(first));
    stmt->span.end = stmt->exprs.back()->span.end;
    return stmt;
}

std::vector<std::unique_ptr<Expr>> Parser::ParseExprList() {
    std::vector<std::unique_ptr<Expr>> result;
    result.push_back(ParseExpr());
    while (Match(TokenKind::kComma)) {
        result.push_back(ParseExpr());
    }
    return result;
}

}  // namespace mc::parse