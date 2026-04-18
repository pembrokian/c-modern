#include "compiler/parse/parser_internal.h"

#include <utility>

namespace mc::parse {

ImportDecl Parser::ParseImportDecl(mc::support::SourcePosition start) {
    ImportDecl decl;
    decl.span.begin = start;
    const auto name = ParseIdentifier("expected module name after import");
    if (name.has_value()) {
        decl.module_name = *name;
    }
    decl.span.end = Previous().span.end;
    return decl;
}

std::vector<Attribute> Parser::ParseAttributes() {
    std::vector<Attribute> attributes;
    while (Match(TokenKind::kAt)) {
        Attribute attribute;
        attribute.span.begin = Previous().span.begin;
        const auto name = ParseIdentifier("expected attribute name after '@'");
        if (name.has_value()) {
            attribute.name = *name;
        }

        if (Match(TokenKind::kLParen)) {
            if (!Check(TokenKind::kRParen)) {
                do {
                    AttributeArg arg;
                    arg.span.begin = Current().span.begin;
                    if (Check(TokenKind::kIdentifier)) {
                        arg.is_identifier = true;
                        arg.value = Current().lexeme;
                        Advance();
                    } else if (IsLiteral(Current().kind)) {
                        arg.value = Current().lexeme;
                        Advance();
                    } else {
                        ReportError(Current(), "expected attribute argument");
                        break;
                    }
                    arg.span.end = Previous().span.end;
                    attribute.args.push_back(std::move(arg));
                } while (Match(TokenKind::kComma));
            }
            Consume(TokenKind::kRParen, "expected ')' after attribute arguments");
        }

        attribute.span.end = Previous().span.end;
        attributes.push_back(std::move(attribute));
    }
    return attributes;
}

std::optional<std::vector<std::string>> Parser::ParseTypeParams() {
    if (!Match(TokenKind::kLt)) {
        return std::nullopt;
    }

    std::vector<std::string> names;
    if (!Check(TokenKind::kGt)) {
        do {
            const auto name = ParseIdentifier("expected type parameter name");
            if (name.has_value()) {
                names.push_back(*name);
            }
        } while (Match(TokenKind::kComma));
    }

    Consume(TokenKind::kGt, "expected '>' after type parameters");
    return names;
}

std::vector<std::unique_ptr<TypeExpr>> Parser::ParseCallTypeArgs() {
    std::vector<std::unique_ptr<TypeExpr>> args;
    Consume(TokenKind::kLt, "expected '<' before call type arguments");
    if (!Check(TokenKind::kGt)) {
        do {
            args.push_back(ParseTypeExpr());
        } while (Match(TokenKind::kComma));
    }
    Consume(TokenKind::kGt, "expected '>' after call type arguments");
    return args;
}

std::optional<Decl> Parser::ParseTopLevelDecl() {
    auto attributes = ParseAttributes();
    if (!attributes.empty()) {
        SkipNewlines();
    }
    if (Match(TokenKind::kExport)) {
        ReportError(Previous(), "export blocks are no longer supported; top-level declarations are public by default and @private hides them");
        if (Match(TokenKind::kLBrace)) {
            int depth = 1;
            while (!AtEnd() && depth > 0) {
                if (Match(TokenKind::kLBrace)) {
                    ++depth;
                    continue;
                }
                if (Match(TokenKind::kRBrace)) {
                    --depth;
                    continue;
                }
                Advance();
            }
        }
        return std::nullopt;
    }
    if (Match(TokenKind::kExtern)) {
        return ParseExternFuncDecl(std::move(attributes), Previous().span.begin);
    }
    if (Match(TokenKind::kFunc)) {
        return ParseFuncDecl(std::move(attributes), Previous().span.begin);
    }
    if (Match(TokenKind::kStruct)) {
        return ParseStructDecl(std::move(attributes), Previous().span.begin);
    }
    if (Match(TokenKind::kEnum)) {
        return ParseEnumDecl(std::move(attributes), Previous().span.begin);
    }
    if (Match(TokenKind::kDistinct)) {
        return ParseDistinctDecl(std::move(attributes), Previous().span.begin);
    }
    if (Match(TokenKind::kType)) {
        return ParseTypeAliasDecl(std::move(attributes), Previous().span.begin);
    }
    if (Match(TokenKind::kVar)) {
        return ParseBindingDecl(std::move(attributes), Decl::Kind::kVar, Previous().span.begin, true);
    }
    if (Match(TokenKind::kConst)) {
        return ParseBindingDecl(std::move(attributes), Decl::Kind::kConst, Previous().span.begin, false);
    }

    ReportError(Current(), "expected top-level declaration");
    return std::nullopt;
}

std::optional<Decl> Parser::ParseExternFuncDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start) {
    Decl decl;
    decl.kind = Decl::Kind::kExternFunc;
    decl.attributes = std::move(attributes);
    decl.span.begin = start;

    Consume(TokenKind::kLParen, "expected '(' after extern");
    const auto abi = ParseIdentifier("expected extern ABI name");
    if (abi.has_value()) {
        decl.extern_abi = *abi;
    }
    Consume(TokenKind::kRParen, "expected ')' after extern ABI");
    Consume(TokenKind::kFunc, "expected 'func' after extern modifier");
    auto parsed = ParseFuncLikeTail(std::move(decl), false);
    if (parsed.has_value()) {
        parsed->span.end = Previous().span.end;
    }
    return parsed;
}

std::optional<Decl> Parser::ParseFuncDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start) {
    Decl decl;
    decl.kind = Decl::Kind::kFunc;
    decl.attributes = std::move(attributes);
    decl.span.begin = start;
    return ParseFuncLikeTail(std::move(decl), true);
}

std::optional<Decl> Parser::ParseFuncLikeTail(Decl decl, bool expect_body) {
    const auto name = ParseIdentifier("expected function name");
    if (name.has_value()) {
        decl.name = *name;
    }

    if (const auto type_params = ParseTypeParams(); type_params.has_value()) {
        decl.type_params = std::move(*type_params);
    }

    Consume(TokenKind::kLParen, "expected '(' after function name");
    if (!Check(TokenKind::kRParen)) {
        do {
            decl.params.push_back(ParseParameter());
        } while (Match(TokenKind::kComma));
    }
    Consume(TokenKind::kRParen, "expected ')' after parameter list");

    if (!Check(TokenKind::kLBrace) && !Check(TokenKind::kNewline) && !Check(TokenKind::kEof)) {
        for (auto& type : ParseReturnTypes()) {
            decl.return_types.push_back(std::move(type));
        }
    }

    if (expect_body) {
        auto body = ParseBlockStmt();
        if (body != nullptr) {
            decl.body = std::move(body);
            decl.span.end = decl.body->span.end;
        }
    } else {
        decl.span.end = Previous().span.end;
    }
    return decl;
}

ParamDecl Parser::ParseParameter() {
    ParamDecl param;
    param.span.begin = Current().span.begin;
    param.attributes = ParseAttributes();
    const auto name = ParseIdentifier("expected parameter name");
    if (name.has_value()) {
        param.name = *name;
    }
    Consume(TokenKind::kColon, "expected ':' after parameter name");
    param.type = ParseTypeExpr();
    param.span.end = param.type != nullptr ? param.type->span.end : Previous().span.end;
    return param;
}

std::vector<std::unique_ptr<TypeExpr>> Parser::ParseReturnTypes() {
    std::vector<std::unique_ptr<TypeExpr>> result;
    if (Match(TokenKind::kLParen)) {
        if (!Check(TokenKind::kRParen)) {
            do {
                result.push_back(ParseTypeExpr());
            } while (Match(TokenKind::kComma));
        }
        Consume(TokenKind::kRParen, "expected ')' after return types");
        return result;
    }

    result.push_back(ParseTypeExpr());
    return result;
}

bool Parser::IsTypeExprStart(TokenKind kind) const {
    return kind == TokenKind::kConst || kind == TokenKind::kStar || kind == TokenKind::kLBracket ||
           kind == TokenKind::kLParen || kind == TokenKind::kIdentifier || kind == TokenKind::kFunc;
}

std::vector<std::unique_ptr<TypeExpr>> Parser::ParseProcedureParamTypes(std::size_t depth) {
    std::vector<std::unique_ptr<TypeExpr>> params;
    Consume(TokenKind::kLParen, "expected '(' after 'func' in procedure type");
    if (!Check(TokenKind::kRParen)) {
        do {
            params.push_back(ParseTypeExpr(depth + 1));
        } while (Match(TokenKind::kComma));
    }
    Consume(TokenKind::kRParen, "expected ')' after procedure parameter types");
    return params;
}

std::vector<std::unique_ptr<TypeExpr>> Parser::ParseProcedureReturnTypes(std::size_t depth) {
    std::vector<std::unique_ptr<TypeExpr>> returns;
    if (!IsTypeExprStart(Current().kind)) {
        return returns;
    }
    if (Match(TokenKind::kLParen)) {
        if (!Check(TokenKind::kRParen)) {
            do {
                returns.push_back(ParseTypeExpr(depth + 1));
            } while (Match(TokenKind::kComma));
        }
        Consume(TokenKind::kRParen, "expected ')' after procedure return types");
        return returns;
    }

    returns.push_back(ParseTypeExpr(depth + 1));
    return returns;
}

std::optional<Decl> Parser::ParseStructDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start) {
    Decl decl;
    decl.kind = Decl::Kind::kStruct;
    decl.attributes = std::move(attributes);
    decl.span.begin = start;
    const auto name = ParseIdentifier("expected struct name");
    if (name.has_value()) {
        decl.name = *name;
    }
    if (const auto type_params = ParseTypeParams(); type_params.has_value()) {
        decl.type_params = std::move(*type_params);
    }

    Consume(TokenKind::kLBrace, "expected '{' to start struct body");
    SkipNewlines();
    while (!Check(TokenKind::kRBrace) && !AtEnd()) {
        decl.fields.push_back(ParseFieldDecl());
        ConsumeMemberSeparator();
    }
    Consume(TokenKind::kRBrace, "expected '}' to close struct body");
    decl.span.end = Previous().span.end;
    return decl;
}

std::optional<Decl> Parser::ParseEnumDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start) {
    Decl decl;
    decl.kind = Decl::Kind::kEnum;
    decl.attributes = std::move(attributes);
    decl.span.begin = start;
    const auto name = ParseIdentifier("expected enum name");
    if (name.has_value()) {
        decl.name = *name;
    }
    if (const auto type_params = ParseTypeParams(); type_params.has_value()) {
        decl.type_params = std::move(*type_params);
    }

    Consume(TokenKind::kLBrace, "expected '{' to start enum body");
    SkipNewlines();
    while (!Check(TokenKind::kRBrace) && !AtEnd()) {
        decl.variants.push_back(ParseEnumVariantDecl());
        ConsumeMemberSeparator();
    }
    Consume(TokenKind::kRBrace, "expected '}' to close enum body");
    decl.span.end = Previous().span.end;
    return decl;
}

std::optional<Decl> Parser::ParseDistinctDecl(std::vector<Attribute> attributes,
                                              mc::support::SourcePosition start) {
    Decl decl;
    decl.kind = Decl::Kind::kDistinct;
    decl.attributes = std::move(attributes);
    decl.span.begin = start;
    const auto name = ParseIdentifier("expected distinct type name");
    if (name.has_value()) {
        decl.name = *name;
    }
    Consume(TokenKind::kAssign, "expected '=' in distinct declaration");
    decl.aliased_type = ParseTypeExpr();
    decl.span.end = decl.aliased_type != nullptr ? decl.aliased_type->span.end : Previous().span.end;
    return decl;
}

std::optional<Decl> Parser::ParseTypeAliasDecl(std::vector<Attribute> attributes,
                                               mc::support::SourcePosition start) {
    Decl decl;
    decl.kind = Decl::Kind::kTypeAlias;
    decl.attributes = std::move(attributes);
    decl.span.begin = start;
    const auto name = ParseIdentifier("expected type alias name");
    if (name.has_value()) {
        decl.name = *name;
    }
    if (const auto type_params = ParseTypeParams(); type_params.has_value()) {
        decl.type_params = std::move(*type_params);
    }
    Consume(TokenKind::kAssign, "expected '=' in type alias declaration");
    decl.aliased_type = ParseTypeExpr();
    decl.span.end = decl.aliased_type != nullptr ? decl.aliased_type->span.end : Previous().span.end;
    return decl;
}

std::optional<Decl> Parser::ParseBindingDecl(std::vector<Attribute> attributes,
                                             Decl::Kind kind,
                                             mc::support::SourcePosition start,
                                             bool allow_storage_without_initializer) {
    Decl decl;
    decl.kind = kind;
    decl.attributes = std::move(attributes);
    decl.span.begin = start;
    auto binding = ParseBindingTail(allow_storage_without_initializer,
                                    false,
                                    false,
                                    kind == Decl::Kind::kConst ? "const declaration requires initializer"
                                                               : "var declaration requires either a type or initializer");
    decl.pattern = std::move(binding.pattern);
    decl.type_ann = std::move(binding.type_ann);
    decl.has_initializer = binding.has_initializer;
    decl.values = std::move(binding.initializers);
    decl.span.end = binding.end;
    return decl;
}

FieldDecl Parser::ParseFieldDecl() {
    FieldDecl field;
    field.span.begin = Current().span.begin;
    const auto name = ParseIdentifier("expected field name");
    if (name.has_value()) {
        field.name = *name;
    }
    Consume(TokenKind::kColon, "expected ':' after field name");
    field.type = ParseTypeExpr();
    field.span.end = field.type != nullptr ? field.type->span.end : Previous().span.end;
    return field;
}

EnumVariantDecl Parser::ParseEnumVariantDecl() {
    EnumVariantDecl variant;
    variant.span.begin = Current().span.begin;
    const auto name = ParseIdentifier("expected enum variant name");
    if (name.has_value()) {
        variant.name = *name;
    }
    if (Match(TokenKind::kLParen)) {
        if (!Check(TokenKind::kRParen)) {
            do {
                variant.payload_fields.push_back(ParseVariantField());
            } while (Match(TokenKind::kComma));
        }
        Consume(TokenKind::kRParen, "expected ')' after enum variant payload");
    }
    variant.span.end = Previous().span.end;
    return variant;
}

FieldDecl Parser::ParseVariantField() {
    FieldDecl field;
    field.span.begin = Current().span.begin;
    if (Check(TokenKind::kIdentifier) && Peek(1).kind == TokenKind::kColon) {
        field.name = Current().lexeme;
        Advance();
        Advance();
    }
    field.type = ParseTypeExpr();
    field.span.end = field.type != nullptr ? field.type->span.end : Previous().span.end;
    return field;
}

NamePattern Parser::ParseBindingPattern(bool allow_discard_targets) {
    NamePattern pattern;
    pattern.span.begin = Current().span.begin;
    const auto first = ParseIdentifier("expected binding name");
    if (first.has_value()) {
        if (*first == "_" && !allow_discard_targets) {
            ReportError(Previous(), "discard target '_' is only allowed in local bindings");
        }
        pattern.names.push_back(*first);
        pattern.discards.push_back(allow_discard_targets && *first == "_");
    }
    while (Match(TokenKind::kComma)) {
        const auto next = ParseIdentifier("expected binding name after ','");
        if (next.has_value()) {
            if (*next == "_" && !allow_discard_targets) {
                ReportError(Previous(), "discard target '_' is only allowed in local bindings");
            }
            pattern.names.push_back(*next);
            pattern.discards.push_back(allow_discard_targets && *next == "_");
        }
    }
    pattern.span.end = Previous().span.end;
    return pattern;
}

std::unique_ptr<TypeExpr> Parser::ParseTypeExpr() {
    return ParseTypeExpr(0);
}

std::unique_ptr<TypeExpr> Parser::ParseTypeExpr(std::size_t depth) {
    if (depth >= kMaxTypeExprDepth) {
        ReportError(Current(), "type expression nesting exceeds parser limit");
        return nullptr;
    }

    if (Match(TokenKind::kConst) || Match(TokenKind::kStar)) {
        auto type = std::make_unique<TypeExpr>();
        type->kind = Previous().kind == TokenKind::kConst ? TypeExpr::Kind::kConst : TypeExpr::Kind::kPointer;
        type->span.begin = Previous().span.begin;
        type->inner = ParseTypeExpr(depth + 1);
        type->span.end = type->inner != nullptr ? type->inner->span.end : Previous().span.end;
        return type;
    }

    if (Match(TokenKind::kFunc)) {
        auto type = std::make_unique<TypeExpr>();
        type->kind = TypeExpr::Kind::kProcedure;
        type->span.begin = Previous().span.begin;
        type->params = ParseProcedureParamTypes(depth);
        type->returns = ParseProcedureReturnTypes(depth);
        type->span.end = Previous().span.end;
        return type;
    }

    std::unique_ptr<TypeExpr> type;
    if (Match(TokenKind::kLBracket)) {
        type = std::make_unique<TypeExpr>();
        type->kind = TypeExpr::Kind::kArray;
        type->span.begin = Previous().span.begin;
        type->length_expr = ParseExpr();
        Consume(TokenKind::kRBracket, "expected ']' after array length");
        type->inner = ParseTypeExpr(depth + 1);
        type->span.end = type->inner != nullptr ? type->inner->span.end : Previous().span.end;
    } else if (Match(TokenKind::kLParen)) {
        type = std::make_unique<TypeExpr>();
        type->kind = TypeExpr::Kind::kParen;
        type->span.begin = Previous().span.begin;
        type->inner = ParseTypeExpr(depth + 1);
        const auto* close = Consume(TokenKind::kRParen, "expected ')' after parenthesized type");
        type->span.end = close != nullptr ? close->span.end : Previous().span.end;
    } else {
        if (!Check(TokenKind::kIdentifier)) {
            ReportError(Current(), "expected type expression");
            return nullptr;
        }

        type = std::make_unique<TypeExpr>();
        type->kind = TypeExpr::Kind::kNamed;
        type->span.begin = Current().span.begin;
        type->name = Current().lexeme;
        Advance();

        while (Match(TokenKind::kDot)) {
            const auto member = ParseIdentifier("expected type name after '.'");
            if (!member.has_value()) {
                break;
            }
            type->name += "." + *member;
        }

        if (Match(TokenKind::kLt)) {
            if (!Check(TokenKind::kGt)) {
                do {
                    type->type_args.push_back(ParseTypeExpr(depth + 1));
                } while (Match(TokenKind::kComma));
            }
            Consume(TokenKind::kGt, "expected '>' after type arguments");
        }

        type->span.end = Previous().span.end;
    }

    return type;
}

}  // namespace mc::parse