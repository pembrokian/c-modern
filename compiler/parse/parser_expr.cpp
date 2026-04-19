#include "compiler/parse/parser_internal.h"

#include <string_view>
#include <utility>

namespace mc::parse {
namespace {

bool IsBuiltinIntegerTypeName(std::string_view name) {
    return name == "i8" || name == "i16" || name == "i32" || name == "i64" || name == "isize" || name == "u8" ||
           name == "u16" || name == "u32" || name == "u64" || name == "usize" || name == "uintptr";
}

}  // namespace

std::unique_ptr<Expr> Parser::ParseExpr(bool allow_aggregate_init) {
    const AggInitGuard guard(*this, allow_aggregate_init);
    return ParseRangeExpr();
}

std::unique_ptr<Expr> Parser::ParseRangeExpr() {
    auto expr = ParseLogicalOrExpr();
    while (Match(TokenKind::kRange)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseLogicalOrExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    if (expr->kind == Expr::Kind::kBinary && expr->text == "..") {
        expr->kind = Expr::Kind::kRange;
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseLogicalOrExpr() {
    auto expr = ParseLogicalAndExpr();
    while (Match(TokenKind::kPipePipe)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseLogicalAndExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseLogicalAndExpr() {
    auto expr = ParseBitwiseOrExpr();
    while (Match(TokenKind::kAmpAmp)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseBitwiseOrExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseBitwiseOrExpr() {
    auto expr = ParseBitwiseXorExpr();
    while (Match(TokenKind::kPipe)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseBitwiseXorExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseBitwiseXorExpr() {
    auto expr = ParseBitwiseAndExpr();
    while (Match(TokenKind::kCaret)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseBitwiseAndExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseBitwiseAndExpr() {
    auto expr = ParseIsExpr();
    while (Match(TokenKind::kAmp)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseIsExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseIsExpr() {
    auto expr = ParseEqualityExpr();
    if (Match(TokenKind::kIs)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseEqualityExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseEqualityExpr() {
    auto expr = ParseRelationalExpr();
    while (Match(TokenKind::kEqEq) || Match(TokenKind::kBangEq)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseRelationalExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseRelationalExpr() {
    auto expr = ParseShiftExpr();
    while (Match(TokenKind::kLt) || Match(TokenKind::kLtEq) || Match(TokenKind::kGt) || Match(TokenKind::kGtEq)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseShiftExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseShiftExpr() {
    auto expr = ParseAdditiveExpr();
    while (Match(TokenKind::kLtLt) || Match(TokenKind::kGtGt)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseAdditiveExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseAdditiveExpr() {
    auto expr = ParseMultiplicativeExpr();
    while (Match(TokenKind::kPlus) || Match(TokenKind::kMinus)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseMultiplicativeExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParseMultiplicativeExpr() {
    auto expr = ParseUnaryExpr();
    while (Match(TokenKind::kStar) || Match(TokenKind::kSlash) || Match(TokenKind::kPercent)) {
        const auto op = Previous();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::kBinary;
        node->span.begin = expr->span.begin;
        node->text = std::string(mc::lex::ToString(op.kind));
        node->left = std::move(expr);
        node->right = ParseUnaryExpr();
        node->span.end = node->right->span.end;
        expr = std::move(node);
    }
    return expr;
}

std::optional<std::size_t> Parser::SkipArrayLengthLookahead(std::size_t offset) const {
    int bracket_depth = 1;
    int paren_depth = 0;
    while (true) {
        const TokenKind kind = Peek(offset).kind;
        if (kind == TokenKind::kEof) {
            return std::nullopt;
        }
        if (kind == TokenKind::kLBracket) {
            ++bracket_depth;
        } else if (kind == TokenKind::kRBracket) {
            --bracket_depth;
            if (bracket_depth == 0 && paren_depth == 0) {
                return offset + 1;
            }
        } else if (kind == TokenKind::kLParen) {
            ++paren_depth;
        } else if (kind == TokenKind::kRParen && paren_depth > 0) {
            --paren_depth;
        }
        ++offset;
    }
}

std::optional<std::size_t> Parser::SkipTypeExprLookahead(std::size_t offset, bool allow_bare_identifier) const {
    return SkipTypeExprLookahead(offset, allow_bare_identifier, 0);
}

std::optional<std::size_t> Parser::SkipTypeArgListLookahead(std::size_t offset, std::size_t depth) const {
    if (Peek(offset).kind != TokenKind::kLt) {
        return std::nullopt;
    }

    std::size_t next = offset + 1;
    if (Peek(next).kind == TokenKind::kGt) {
        return next + 1;
    }

    while (true) {
        if (depth >= kMaxTypeExprDepth) {
            return std::nullopt;
        }
        const auto type_arg_end = SkipTypeExprLookahead(next, true, depth + 1);
        if (!type_arg_end.has_value()) {
            return std::nullopt;
        }
        next = *type_arg_end;
        if (Peek(next).kind == TokenKind::kComma) {
            ++next;
            continue;
        }
        if (Peek(next).kind != TokenKind::kGt) {
            return std::nullopt;
        }
        return next + 1;
    }
}

std::optional<std::size_t> Parser::SkipTypeExprLookahead(std::size_t offset,
                                                         bool allow_bare_identifier,
                                                         std::size_t depth) const {
    std::size_t nesting = depth;
    while (Peek(offset).kind == TokenKind::kConst || Peek(offset).kind == TokenKind::kStar) {
        if (nesting >= kMaxTypeExprDepth) {
            return std::nullopt;
        }
        ++nesting;
        ++offset;
    }

    switch (Peek(offset).kind) {
        case TokenKind::kLBracket: {
            if (nesting >= kMaxTypeExprDepth) {
                return std::nullopt;
            }
            const auto after_length = SkipArrayLengthLookahead(offset + 1);
            if (!after_length.has_value()) {
                return std::nullopt;
            }
            return SkipTypeExprLookahead(*after_length, true, nesting + 1);
        }
        case TokenKind::kLParen: {
            if (nesting >= kMaxTypeExprDepth) {
                return std::nullopt;
            }
            const auto inner = SkipTypeExprLookahead(offset + 1, true, nesting + 1);
            if (!inner.has_value() || Peek(*inner).kind != TokenKind::kRParen) {
                return std::nullopt;
            }
            return *inner + 1;
        }
        case TokenKind::kFunc: {
            if (nesting >= kMaxTypeExprDepth || Peek(offset + 1).kind != TokenKind::kLParen) {
                return std::nullopt;
            }

            std::size_t next = offset + 2;
            if (Peek(next).kind != TokenKind::kRParen) {
                while (true) {
                    const auto param_end = SkipTypeExprLookahead(next, true, nesting + 1);
                    if (!param_end.has_value()) {
                        return std::nullopt;
                    }
                    next = *param_end;
                    if (Peek(next).kind == TokenKind::kComma) {
                        ++next;
                        continue;
                    }
                    if (Peek(next).kind != TokenKind::kRParen) {
                        return std::nullopt;
                    }
                    break;
                }
            }
            ++next;

            if (!IsTypeExprStart(Peek(next).kind)) {
                return next;
            }
            if (Peek(next).kind == TokenKind::kLParen) {
                ++next;
                if (Peek(next).kind != TokenKind::kRParen) {
                    while (true) {
                        const auto ret_end = SkipTypeExprLookahead(next, true, nesting + 1);
                        if (!ret_end.has_value()) {
                            return std::nullopt;
                        }
                        next = *ret_end;
                        if (Peek(next).kind == TokenKind::kComma) {
                            ++next;
                            continue;
                        }
                        if (Peek(next).kind != TokenKind::kRParen) {
                            return std::nullopt;
                        }
                        break;
                    }
                }
                return next + 1;
            }
            return SkipTypeExprLookahead(next, true, nesting + 1);
        }
        case TokenKind::kIdentifier: {
            std::size_t next = offset + 1;
            while (Peek(next).kind == TokenKind::kDot && Peek(next + 1).kind == TokenKind::kIdentifier) {
                next += 2;
            }
            if (Peek(next).kind == TokenKind::kLt) {
                return SkipTypeArgListLookahead(next, nesting);
            }
            return allow_bare_identifier ? std::optional<std::size_t>(next) : std::nullopt;
        }
        default:
            return std::nullopt;
    }
}

bool Parser::LooksLikeTypeCallExpr() const {
    if (!Check(TokenKind::kLParen)) {
        return false;
    }
    const auto inner_type_end = SkipTypeExprLookahead(1, true);
    return inner_type_end.has_value() && Peek(*inner_type_end).kind == TokenKind::kRParen && Peek(*inner_type_end + 1).kind == TokenKind::kLParen;
}

bool Parser::LooksLikeBareIntegerConversionExpr() const {
    return Check(TokenKind::kIdentifier) && IsBuiltinIntegerTypeName(Current().lexeme) && Peek(1).kind == TokenKind::kLParen;
}

bool Parser::LooksLikeTypeAggregateInitExpr() const {
    if (!IsTypeExprStart(Current().kind)) {
        return false;
    }
    const auto type_end = SkipTypeExprLookahead(0, true);
    return type_end.has_value() && Peek(*type_end).kind == TokenKind::kLBrace;
}

bool Parser::LooksLikePostfixTypeArgs() const {
    if (!Check(TokenKind::kLt)) {
        return false;
    }

    const auto after_type_args = SkipTypeArgListLookahead(0, 0);
    return after_type_args.has_value() &&
           (Peek(*after_type_args).kind == TokenKind::kLParen || Peek(*after_type_args).kind == TokenKind::kLBrace ||
            Peek(*after_type_args).kind == TokenKind::kDot);
}

std::unique_ptr<Expr> Parser::ParseTypeCallExpr() {
    auto expr = std::make_unique<Expr>();
    expr->kind = Expr::Kind::kCall;
    expr->span.begin = Current().span.begin;
    Consume(TokenKind::kLParen, "expected '(' before conversion target type");
    expr->type_target = ParseTypeExpr();
    Consume(TokenKind::kRParen, "expected ')' after conversion target type");
    Consume(TokenKind::kLParen, "expected '(' after conversion target type");
    SkipNewlines();
    if (!Check(TokenKind::kRParen)) {
        expr->args = ParseExprListUntil(TokenKind::kRParen);
    }
    Consume(TokenKind::kRParen, "expected ')' after call arguments");
    expr->span.end = Previous().span.end;
    return expr;
}

std::unique_ptr<Expr> Parser::ParseBareIntegerConversionExpr() {
    auto expr = std::make_unique<Expr>();
    expr->kind = Expr::Kind::kCall;
    expr->bare_type_target_syntax = true;
    expr->span.begin = Current().span.begin;

    auto type = std::make_unique<TypeExpr>();
    type->kind = TypeExpr::Kind::kNamed;
    type->span = Current().span;
    type->name = Current().lexeme;
    expr->type_target = std::move(type);

    Advance();
    Consume(TokenKind::kLParen, "expected '(' after conversion target type");
    SkipNewlines();
    if (!Check(TokenKind::kRParen)) {
        expr->args = ParseExprListUntil(TokenKind::kRParen);
    }
    Consume(TokenKind::kRParen, "expected ')' after call arguments");
    expr->span.end = Previous().span.end;
    return expr;
}

std::unique_ptr<Expr> Parser::ParseTypeAggregateInitExpr() {
    auto expr = std::make_unique<Expr>();
    expr->kind = Expr::Kind::kAggregateInit;
    expr->span.begin = Current().span.begin;
    expr->type_target = ParseTypeExpr();
    Consume(TokenKind::kLBrace, "expected '{' after aggregate initializer type");
    SkipNewlines();
    if (!Check(TokenKind::kRBrace)) {
        expr->field_inits = ParseFieldInitListUntil(TokenKind::kRBrace);
    }
    Consume(TokenKind::kRBrace, "expected '}' after aggregate initializer");
    expr->span.end = Previous().span.end;
    return expr;
}

std::unique_ptr<Expr> Parser::ParseBraceAggregateInitExpr() {
    auto expr = std::make_unique<Expr>();
    expr->kind = Expr::Kind::kAggregateInit;
    expr->span.begin = Current().span.begin;
    Consume(TokenKind::kLBrace, "expected '{' to start aggregate initializer");
    SkipNewlines();
    if (!Check(TokenKind::kRBrace)) {
        expr->field_inits = ParseFieldInitListUntil(TokenKind::kRBrace);
    }
    Consume(TokenKind::kRBrace, "expected '}' after aggregate initializer");
    expr->span.end = Previous().span.end;
    return expr;
}

std::unique_ptr<Expr> Parser::ParseEmptyCollectionExpr() {
    auto expr = std::make_unique<Expr>();
    expr->kind = Expr::Kind::kEmptyCollection;
    expr->span.begin = Current().span.begin;
    Consume(TokenKind::kLBracket, "expected '[' to start empty collection literal");
    Consume(TokenKind::kRBracket, "expected ']' after '[' in empty collection literal");
    expr->span.end = Previous().span.end;
    return expr;
}

std::vector<std::unique_ptr<Expr>> Parser::ParseExprListUntil(TokenKind terminator) {
    std::vector<std::unique_ptr<Expr>> result;
    result.push_back(ParseExpr());
    SkipNewlines();
    while (Match(TokenKind::kComma)) {
        SkipNewlines();
        if (Check(terminator)) {
            break;
        }
        result.push_back(ParseExpr());
        SkipNewlines();
    }
    return result;
}

std::unique_ptr<Expr> Parser::ParseUnaryExpr() {
    if (LooksLikeBareIntegerConversionExpr()) {
        return ParseBareIntegerConversionExpr();
    }
    if (LooksLikeTypeCallExpr()) {
        return ParseTypeCallExpr();
    }
    if (allow_aggregate_init_ && LooksLikeTypeAggregateInitExpr()) {
        return ParseTypeAggregateInitExpr();
    }
    std::vector<Token> operators;
    while (IsPrefixUnaryOperator(Current().kind)) {
        const auto op = Advance();
        if (operators.size() >= kMaxUnaryExprDepth) {
            ReportError(op, "unary expression nesting exceeds parser limit");
            while (IsPrefixUnaryOperator(Current().kind)) {
                Advance();
            }
            break;
        }
        operators.push_back(op);
    }

    auto expr = ParsePostfixExpr();
    for (std::size_t index = operators.size(); index > 0; --index) {
        auto unary = std::make_unique<Expr>();
        unary->kind = Expr::Kind::kUnary;
        unary->span.begin = operators[index - 1].span.begin;
        unary->text = std::string(mc::lex::ToString(operators[index - 1].kind));
        unary->left = std::move(expr);
        unary->span.end = unary->left->span.end;
        expr = std::move(unary);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::ParsePostfixExpr() {
    auto expr = ParsePrimaryExpr();
    while (true) {
        if (Match(TokenKind::kDot)) {
            std::optional<std::string> member;
            if (Check(TokenKind::kIdentifier) || Check(TokenKind::kIntLiteral)) {
                member = Current().lexeme;
                Advance();
            } else {
                ReportError(Current(), "expected field name after '.'");
            }
            auto node = std::make_unique<Expr>();
            node->span.begin = expr->span.begin;
            if ((expr->kind == Expr::Kind::kName || expr->kind == Expr::Kind::kQualifiedName) &&
                Previous().kind == TokenKind::kIdentifier) {
                node->kind = Expr::Kind::kQualifiedName;
                node->text = expr->kind == Expr::Kind::kQualifiedName ? expr->text + "." + expr->secondary_text : expr->text;
                node->secondary_text = member.value_or("");
                node->type_args = std::move(expr->type_args);
                node->span.end = Previous().span.end;
                expr = std::move(node);
                continue;
            }
            node->kind = Expr::Kind::kField;
            node->text = member.value_or("");
            node->left = std::move(expr);
            node->span.end = Previous().span.end;
            expr = std::move(node);
            continue;
        }

        if (Match(TokenKind::kDotDeref)) {
            auto node = std::make_unique<Expr>();
            node->kind = Expr::Kind::kDerefField;
            node->span.begin = expr->span.begin;
            node->left = std::move(expr);
            node->span.end = Previous().span.end;
            expr = std::move(node);
            continue;
        }

        if (Match(TokenKind::kLBracket)) {
            auto node = std::make_unique<Expr>();
            node->span.begin = expr->span.begin;
            node->left = std::move(expr);
            if (Match(TokenKind::kColon)) {
                node->kind = Expr::Kind::kSlice;
                if (!Check(TokenKind::kRBracket)) {
                    node->extra = ParseExpr();
                }
                Consume(TokenKind::kRBracket, "expected ']' after slice expression");
            } else {
                auto first = ParseExpr();
                if (Match(TokenKind::kColon)) {
                    node->kind = Expr::Kind::kSlice;
                    node->right = std::move(first);
                    if (!Check(TokenKind::kRBracket)) {
                        node->extra = ParseExpr();
                    }
                    Consume(TokenKind::kRBracket, "expected ']' after slice expression");
                } else {
                    node->kind = Expr::Kind::kIndex;
                    node->right = std::move(first);
                    Consume(TokenKind::kRBracket, "expected ']' after index expression");
                }
            }
            node->span.end = Previous().span.end;
            expr = std::move(node);
            continue;
        }

        if (expr->type_args.empty() && LooksLikePostfixTypeArgs()) {
            expr->type_args = ParseCallTypeArgs();
            continue;
        }

        if (Match(TokenKind::kLParen)) {
            auto node = std::make_unique<Expr>();
            node->kind = Expr::Kind::kCall;
            node->span.begin = expr->span.begin;
            node->left = std::move(expr);
            SkipNewlines();
            if (!Check(TokenKind::kRParen)) {
                node->args = ParseExprListUntil(TokenKind::kRParen);
            }
            Consume(TokenKind::kRParen, "expected ')' after call arguments");
            node->span.end = Previous().span.end;
            expr = std::move(node);
            continue;
        }

        if (allow_aggregate_init_ && Check(TokenKind::kLBrace) && IsAggregateInitBase(*expr)) {
            Advance();
            auto node = std::make_unique<Expr>();
            node->kind = Expr::Kind::kAggregateInit;
            node->span.begin = expr->span.begin;
            node->left = std::move(expr);
            SkipNewlines();
            if (!Check(TokenKind::kRBrace)) {
                node->field_inits = ParseFieldInitListUntil(TokenKind::kRBrace);
            }
            Consume(TokenKind::kRBrace, "expected '}' after aggregate initializer");
            node->span.end = Previous().span.end;
            expr = std::move(node);
            continue;
        }

        if (Match(TokenKind::kWith)) {
            auto node = std::make_unique<Expr>();
            node->kind = Expr::Kind::kRecordUpdate;
            node->span.begin = expr->span.begin;
            node->left = std::move(expr);
            Consume(TokenKind::kLBrace, "expected '{' after 'with'");
            SkipNewlines();
            if (!Check(TokenKind::kRBrace)) {
                node->field_inits = ParseFieldInitListUntil(TokenKind::kRBrace, true);
            }
            Consume(TokenKind::kRBrace, "expected '}' after record update");
            node->span.end = Previous().span.end;
            expr = std::move(node);
            continue;
        }

        break;
    }
    return expr;
}

std::vector<FieldInit> Parser::ParseFieldInitListUntil(TokenKind terminator, bool allow_dotted_name) {
    std::vector<FieldInit> result;
    result.push_back(ParseFieldInit(terminator, allow_dotted_name));
    SkipNewlines();
    while (Match(TokenKind::kComma)) {
        SkipNewlines();
        if (Check(terminator)) {
            break;
        }
        result.push_back(ParseFieldInit(terminator, allow_dotted_name));
        SkipNewlines();
    }
    return result;
}

FieldInit Parser::ParseFieldInit(TokenKind terminator, bool allow_dotted_name) {
    FieldInit init;
    init.span.begin = Current().span.begin;
    if (Check(TokenKind::kIdentifier) && Peek(1).kind == TokenKind::kColon) {
        init.has_name = true;
        init.name = Current().lexeme;
        init.field_path.push_back(init.name);
        Advance();
        Advance();
        if (Check(TokenKind::kComma) || Check(terminator)) {
            auto value = std::make_unique<Expr>();
            value->kind = Expr::Kind::kName;
            value->span.begin = init.span.begin;
            value->span.end = Previous().span.end;
            value->text = init.name;
            init.value = std::move(value);
            init.span.end = init.value->span.end;
            return init;
        }
    } else if (allow_dotted_name && Check(TokenKind::kIdentifier)) {
        std::vector<std::string> path;
        path.push_back(Current().lexeme);
        std::size_t offset = 1;
        while (Peek(offset).kind == TokenKind::kDot && Peek(offset + 1).kind == TokenKind::kIdentifier) {
            path.push_back(Peek(offset + 1).lexeme);
            offset += 2;
        }
        if (Peek(offset).kind == TokenKind::kColon && path.size() > 1) {
            init.has_name = true;
            init.field_path = path;
            init.name.clear();
            for (std::size_t index = 0; index < path.size(); ++index) {
                if (index > 0) {
                    init.name += ".";
                }
                init.name += path[index];
            }
            Advance();
            while (Match(TokenKind::kDot)) {
                if (Check(TokenKind::kIdentifier)) {
                    Advance();
                    continue;
                }
                ReportError(Current(), "expected field name after '.' in record update path");
                break;
            }
            Advance();
            if (Check(TokenKind::kComma) || Check(terminator)) {
                ReportError(Previous(), "record update shorthand requires a simple field name");
                auto value = std::make_unique<Expr>();
                value->kind = Expr::Kind::kName;
                value->span.begin = init.span.begin;
                value->span.end = Previous().span.end;
                value->text = path.back();
                init.value = std::move(value);
                init.span.end = init.value->span.end;
                return init;
            }
        }
    }
    init.value = ParseExpr();
    init.span.end = init.value->span.end;
    return init;
}

std::unique_ptr<Expr> Parser::ParseAssignmentTarget() {
    const auto start = Current().span.begin;
    auto target = NormalizeAssignmentTarget(ParsePostfixExpr());
    if (!IsAssignmentTarget(*target)) {
        diagnostics_.Report({
            .file_path = file_path_,
            .span = {start, target->span.end},
            .severity = DiagnosticSeverity::kError,
            .message = "expected assignment target",
        });
        ok_ = false;
    }
    return target;
}

std::unique_ptr<Expr> Parser::ParsePrimaryExpr() {
    if (Check(TokenKind::kLBracket) && Peek(1).kind == TokenKind::kRBracket) {
        return ParseEmptyCollectionExpr();
    }

    if (allow_aggregate_init_ && Check(TokenKind::kLBrace)) {
        return ParseBraceAggregateInitExpr();
    }

    if (IsLiteral(Current().kind)) {
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::Kind::kLiteral;
        expr->span = Current().span;
        expr->text = Current().lexeme;
        expr->secondary_text = std::string(mc::lex::ToString(Current().kind));
        expr->integer_literal_value = Current().integer_value;
        expr->float_literal_value = Current().float_value;
        Advance();
        return expr;
    }

    if (Match(TokenKind::kIdentifier)) {
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::Kind::kName;
        expr->span = Previous().span;
        expr->text = Previous().lexeme;
        return expr;
    }

    if (Match(TokenKind::kLParen)) {
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::Kind::kParen;
        expr->span.begin = Previous().span.begin;
        expr->left = ParseExpr();
        const auto* close = Consume(TokenKind::kRParen, "expected ')' after expression");
        expr->span.end = close != nullptr ? close->span.end : Previous().span.end;
        return expr;
    }

    const auto error_token = Current();
    ReportError(Current(), "expected expression");
    if (!AtEnd()) {
        Advance();
    }
    auto expr = std::make_unique<Expr>();
    expr->kind = Expr::Kind::kLiteral;
    expr->span = error_token.span;
    expr->text = "<error>";
    return expr;
}

}  // namespace mc::parse