#include "compiler/parse/parser.h"

#include <optional>
#include <utility>

namespace mc::parse {
namespace {

using mc::ast::Attribute;
using mc::ast::AttributeArg;
using mc::ast::CasePattern;
using mc::ast::Decl;
using mc::ast::DefaultCase;
using mc::ast::EnumVariantDecl;
using mc::ast::ExportBlock;
using mc::ast::Expr;
using mc::ast::FieldDecl;
using mc::ast::FieldInit;
using mc::ast::ImportDecl;
using mc::ast::NamePattern;
using mc::ast::ParamDecl;
using mc::ast::SourceFile;
using mc::ast::Stmt;
using mc::ast::SwitchCase;
using mc::ast::TypeExpr;
using mc::lex::Token;
using mc::lex::TokenKind;
using mc::support::DiagnosticSeverity;

class Parser {
  public:
    Parser(const mc::lex::LexResult& lexed_module,
           const std::filesystem::path& file_path,
           support::DiagnosticSink& diagnostics)
        : tokens_(lexed_module.tokens), file_path_(file_path), diagnostics_(diagnostics), ok_(lexed_module.ok) {}

    ParseResult Run() {
        auto source_file = std::make_unique<SourceFile>();
        source_file->span.begin = Current().span.begin;

        SkipNewlines();
        if (Match(TokenKind::kExport)) {
            source_file->has_export_block = true;
            source_file->export_block = ParseExportBlock(Previous().span.begin);
            RequireSourceSeparator("export block");
        }

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

  private:
    bool AtEnd() const {
        return Current().kind == TokenKind::kEof;
    }

    const Token& Current() const {
        return tokens_[index_];
    }

    const Token& Previous() const {
        return tokens_[index_ - 1];
    }

    const Token& Peek(std::size_t offset) const {
        const auto target = index_ + offset;
        return tokens_[target < tokens_.size() ? target : tokens_.size() - 1];
    }

    bool Check(TokenKind kind) const {
        return Current().kind == kind;
    }

    bool Match(TokenKind kind) {
        if (!Check(kind)) {
            return false;
        }
        Advance();
        return true;
    }

    const Token& Advance() {
        if (!AtEnd()) {
            ++index_;
        }
        return Previous();
    }

    const Token* Consume(TokenKind kind, const char* message) {
        if (Match(kind)) {
            return &Previous();
        }
        ReportError(Current(), message);
        return nullptr;
    }

    void ReportError(const Token& token, const std::string& message) {
        diagnostics_.Report({
            .file_path = file_path_,
            .span = token.span,
            .severity = DiagnosticSeverity::kError,
            .message = message,
        });
        ok_ = false;
    }

    void SkipNewlines() {
        while (Match(TokenKind::kNewline)) {
        }
    }

    bool RequireSourceSeparator(const char* context) {
        if (AtEnd() || Check(TokenKind::kNewline)) {
            SkipNewlines();
            return true;
        }

        ReportError(Current(), std::string("expected newline after ") + context);
        return false;
    }

    void SynchronizeTopLevel() {
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

    std::optional<std::string> ParseIdentifier(const char* message) {
        if (!Check(TokenKind::kIdentifier)) {
            ReportError(Current(), message);
            return std::nullopt;
        }
        const auto text = Current().lexeme;
        Advance();
        return text;
    }

    ExportBlock ParseExportBlock(mc::support::SourcePosition start) {
        ExportBlock block;
        block.span.begin = start;
        Consume(TokenKind::kLBrace, "expected '{' after export");
        if (!Check(TokenKind::kRBrace)) {
            do {
                const auto name = ParseIdentifier("expected export name");
                if (name.has_value()) {
                    block.names.push_back(*name);
                }
            } while (Match(TokenKind::kComma));
        }
        const auto* close = Consume(TokenKind::kRBrace, "expected '}' after export block");
        block.span.end = close != nullptr ? close->span.end : Current().span.end;
        return block;
    }

    ImportDecl ParseImportDecl(mc::support::SourcePosition start) {
        ImportDecl decl;
        decl.span.begin = start;
        const auto name = ParseIdentifier("expected module name after import");
        if (name.has_value()) {
            decl.module_name = *name;
        }
        decl.span.end = Previous().span.end;
        return decl;
    }

    std::vector<Attribute> ParseAttributes() {
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

    std::optional<std::vector<std::string>> ParseTypeParams() {
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

    std::optional<Decl> ParseTopLevelDecl() {
        auto attributes = ParseAttributes();
        if (!attributes.empty()) {
            SkipNewlines();
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
            return ParseDistinctDecl(Previous().span.begin);
        }
        if (Match(TokenKind::kType)) {
            return ParseTypeAliasDecl(Previous().span.begin);
        }
        if (Match(TokenKind::kVar)) {
            return ParseBindingDecl(Decl::Kind::kVar, Previous().span.begin, true);
        }
        if (Match(TokenKind::kConst)) {
            return ParseBindingDecl(Decl::Kind::kConst, Previous().span.begin, false);
        }

        ReportError(Current(), "expected top-level declaration");
        return std::nullopt;
    }

    std::optional<Decl> ParseExternFuncDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start) {
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

    std::optional<Decl> ParseFuncDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start) {
        Decl decl;
        decl.kind = Decl::Kind::kFunc;
        decl.attributes = std::move(attributes);
        decl.span.begin = start;
        return ParseFuncLikeTail(std::move(decl), true);
    }

    std::optional<Decl> ParseFuncLikeTail(Decl decl, bool expect_body) {
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

    ParamDecl ParseParameter() {
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

    std::vector<std::unique_ptr<TypeExpr>> ParseReturnTypes() {
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

    std::optional<Decl> ParseStructDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start) {
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

    std::optional<Decl> ParseEnumDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start) {
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

    std::optional<Decl> ParseDistinctDecl(mc::support::SourcePosition start) {
        Decl decl;
        decl.kind = Decl::Kind::kDistinct;
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

    std::optional<Decl> ParseTypeAliasDecl(mc::support::SourcePosition start) {
        Decl decl;
        decl.kind = Decl::Kind::kTypeAlias;
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

    std::optional<Decl> ParseBindingDecl(Decl::Kind kind, mc::support::SourcePosition start, bool allow_storage_without_initializer) {
        Decl decl;
        decl.kind = kind;
        decl.span.begin = start;
        decl.pattern = ParseBindingPattern();

        if (Match(TokenKind::kColon)) {
            decl.type_ann = ParseTypeExpr();
        }

        if (Match(TokenKind::kAssign)) {
            decl.has_initializer = true;
            decl.values = ParseExprList();
        } else if (!allow_storage_without_initializer || decl.type_ann == nullptr) {
            ReportError(Current(), kind == Decl::Kind::kConst ? "const declaration requires initializer" : "var declaration requires either a type or initializer");
        }

        decl.span.end = Previous().span.end;
        return decl;
    }

    FieldDecl ParseFieldDecl() {
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

    EnumVariantDecl ParseEnumVariantDecl() {
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

    FieldDecl ParseVariantField() {
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

    NamePattern ParseBindingPattern() {
        NamePattern pattern;
        pattern.span.begin = Current().span.begin;
        const auto first = ParseIdentifier("expected binding name");
        if (first.has_value()) {
            pattern.names.push_back(*first);
        }
        while (Match(TokenKind::kComma)) {
            const auto next = ParseIdentifier("expected binding name after ','");
            if (next.has_value()) {
                pattern.names.push_back(*next);
            }
        }
        pattern.span.end = Previous().span.end;
        return pattern;
    }

    std::unique_ptr<TypeExpr> ParseTypeExpr() {
        if (Match(TokenKind::kConst)) {
            auto type = std::make_unique<TypeExpr>();
            type->kind = TypeExpr::Kind::kConst;
            type->span.begin = Previous().span.begin;
            type->inner = ParseTypeExpr();
            type->span.end = type->inner != nullptr ? type->inner->span.end : Previous().span.end;
            return type;
        }

        if (Match(TokenKind::kStar)) {
            auto type = std::make_unique<TypeExpr>();
            type->kind = TypeExpr::Kind::kPointer;
            type->span.begin = Previous().span.begin;
            type->inner = ParseTypeExpr();
            type->span.end = type->inner != nullptr ? type->inner->span.end : Previous().span.end;
            return type;
        }

        if (Match(TokenKind::kLBracket)) {
            auto type = std::make_unique<TypeExpr>();
            type->kind = TypeExpr::Kind::kArray;
            type->span.begin = Previous().span.begin;
            type->length_expr = ParseExpr();
            Consume(TokenKind::kRBracket, "expected ']' after array length");
            type->inner = ParseTypeExpr();
            type->span.end = type->inner != nullptr ? type->inner->span.end : Previous().span.end;
            return type;
        }

        if (Match(TokenKind::kLParen)) {
            auto type = std::make_unique<TypeExpr>();
            type->kind = TypeExpr::Kind::kParen;
            type->span.begin = Previous().span.begin;
            type->inner = ParseTypeExpr();
            const auto* close = Consume(TokenKind::kRParen, "expected ')' after parenthesized type");
            type->span.end = close != nullptr ? close->span.end : Previous().span.end;
            return type;
        }

        if (!Check(TokenKind::kIdentifier)) {
            ReportError(Current(), "expected type expression");
            return nullptr;
        }

        auto type = std::make_unique<TypeExpr>();
        type->kind = TypeExpr::Kind::kNamed;
        type->span.begin = Current().span.begin;
        type->name = Current().lexeme;
        Advance();

        if (Match(TokenKind::kLt)) {
            if (!Check(TokenKind::kGt)) {
                do {
                    type->type_args.push_back(ParseTypeExpr());
                } while (Match(TokenKind::kComma));
            }
            Consume(TokenKind::kGt, "expected '>' after type arguments");
        }

        type->span.end = Previous().span.end;
        return type;
    }

    std::unique_ptr<Stmt> ParseBlockStmt() {
        if (!Match(TokenKind::kLBrace)) {
            ReportError(Current(), "expected '{' to start block");
            return nullptr;
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
            SkipStatementSeparator();
        }

        const auto* close = Consume(TokenKind::kRBrace, "expected '}' to close block");
        block->span.end = close != nullptr ? close->span.end : Previous().span.end;
        return block;
    }

    std::unique_ptr<Stmt> ParseStatement() {
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

    std::unique_ptr<Stmt> ParseIfStmt(mc::support::SourcePosition start) {
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

    std::unique_ptr<Stmt> ParseSwitchStmt(mc::support::SourcePosition start) {
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
            SkipStatementSeparator();
        }
        const auto* close = Consume(TokenKind::kRBrace, "expected '}' after switch body");
        stmt->span.end = close != nullptr ? close->span.end : Previous().span.end;
        return stmt;
    }

    SwitchCase ParseSwitchCase(mc::support::SourcePosition start) {
        SwitchCase case_node;
        case_node.span.begin = start;
        case_node.pattern = ParseCasePattern();
        Consume(TokenKind::kColon, "expected ':' after case pattern");
        if (!Check(TokenKind::kCase) && !Check(TokenKind::kDefault) && !Check(TokenKind::kRBrace) && !Check(TokenKind::kNewline)) {
            case_node.statements.push_back(ParseStatement());
        }
        SkipStatementSeparator();
        while (!Check(TokenKind::kCase) && !Check(TokenKind::kDefault) && !Check(TokenKind::kRBrace) && !AtEnd()) {
            case_node.statements.push_back(ParseStatement());
            SkipStatementSeparator();
        }
        case_node.span.end = Previous().span.end;
        return case_node;
    }

    DefaultCase ParseDefaultCase(mc::support::SourcePosition start) {
        DefaultCase case_node;
        case_node.span.begin = start;
        Consume(TokenKind::kColon, "expected ':' after default");
        if (!Check(TokenKind::kCase) && !Check(TokenKind::kDefault) && !Check(TokenKind::kRBrace) && !Check(TokenKind::kNewline)) {
            case_node.statements.push_back(ParseStatement());
        }
        SkipStatementSeparator();
        while (!Check(TokenKind::kCase) && !Check(TokenKind::kDefault) && !Check(TokenKind::kRBrace) && !AtEnd()) {
            case_node.statements.push_back(ParseStatement());
            SkipStatementSeparator();
        }
        case_node.span.end = Previous().span.end;
        return case_node;
    }

    CasePattern ParseCasePattern() {
        CasePattern pattern;
        pattern.span.begin = Current().span.begin;
        if (Check(TokenKind::kIdentifier) && Peek(1).kind == TokenKind::kDot && Peek(2).kind == TokenKind::kIdentifier) {
            pattern.kind = CasePattern::Kind::kVariant;
            pattern.variant_name = Current().lexeme + std::string(".") + Peek(2).lexeme;
            Advance();
            Advance();
            Advance();
            if (Match(TokenKind::kLParen)) {
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
            pattern.span.end = Previous().span.end;
            return pattern;
        }

        if (Check(TokenKind::kIdentifier) && Peek(1).kind == TokenKind::kLParen) {
            pattern.kind = CasePattern::Kind::kVariant;
            pattern.variant_name = Current().lexeme;
            Advance();
            Advance();
            if (!Check(TokenKind::kRParen)) {
                do {
                    const auto binding = ParseIdentifier("expected pattern binding name");
                    if (binding.has_value()) {
                        pattern.bindings.push_back(*binding);
                    }
                } while (Match(TokenKind::kComma));
            }
            Consume(TokenKind::kRParen, "expected ')' after pattern bindings");
            pattern.span.end = Previous().span.end;
            return pattern;
        }

        pattern.kind = CasePattern::Kind::kExpr;
        pattern.expr = ParseExpr();
        pattern.span.end = pattern.expr != nullptr ? pattern.expr->span.end : Previous().span.end;
        return pattern;
    }

    std::unique_ptr<Stmt> ParseForStmt(mc::support::SourcePosition start) {
        if (Check(TokenKind::kIdentifier) && Peek(1).kind == TokenKind::kIn) {
            std::string name = Current().lexeme;
            Advance();
            Advance();
            auto iterable = ParseExpr(false);
            auto body = ParseBlockStmt();
            auto stmt = std::make_unique<Stmt>();
            stmt->kind = iterable != nullptr && iterable->kind == Expr::Kind::kRange ? Stmt::Kind::kForRange : Stmt::Kind::kForEach;
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

    std::unique_ptr<Stmt> ParseWhileStmt(mc::support::SourcePosition start) {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = Stmt::Kind::kWhile;
        stmt->span.begin = start;
        stmt->exprs.push_back(ParseExpr(false));
        stmt->then_branch = ParseBlockStmt();
        stmt->span.end = stmt->then_branch != nullptr ? stmt->then_branch->span.end : Current().span.end;
        return stmt;
    }

    std::unique_ptr<Stmt> ParseReturnStmt(mc::support::SourcePosition start) {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = Stmt::Kind::kReturn;
        stmt->span.begin = start;
        if (!IsStatementTerminator(Current().kind)) {
            stmt->exprs = ParseExprList();
        }
        stmt->span.end = Previous().span.end;
        return stmt;
    }

    std::unique_ptr<Stmt> ParseDeferStmt(mc::support::SourcePosition start) {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = Stmt::Kind::kDefer;
        stmt->span.begin = start;
        stmt->exprs.push_back(ParseExpr());
        stmt->span.end = stmt->exprs.back()->span.end;
        return stmt;
    }

    std::unique_ptr<Stmt> ParseBindingStmt(Stmt::Kind kind, mc::support::SourcePosition start, bool allow_storage_without_initializer) {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = kind;
        stmt->span.begin = start;
        stmt->pattern = ParseBindingPattern();
        if (Match(TokenKind::kColon)) {
            stmt->type_ann = ParseTypeExpr();
        }
        if (Match(TokenKind::kAssign)) {
            stmt->has_initializer = true;
            stmt->exprs = ParseExprList();
        } else if (!allow_storage_without_initializer || stmt->type_ann == nullptr) {
            ReportError(Current(), kind == Stmt::Kind::kConst ? "const statement requires initializer" : "binding requires either a type or initializer");
        }
        stmt->span.end = Previous().span.end;
        return stmt;
    }

    std::unique_ptr<Stmt> ParseAssignOrExprStmt() {
        const auto start = Current().span.begin;
        auto first = ParseExpr();
        if (Match(TokenKind::kAssign)) {
            if (IsBareNameExpr(*first)) {
                auto stmt = std::make_unique<Stmt>();
                stmt->kind = Stmt::Kind::kBindingOrAssign;
                stmt->span.begin = start;
                stmt->has_initializer = true;
                stmt->pattern.names.push_back(first->text);
                stmt->pattern.span = first->span;
                stmt->exprs = ParseExprList();
                stmt->span.end = Previous().span.end;
                return stmt;
            }

            auto stmt = std::make_unique<Stmt>();
            stmt->kind = Stmt::Kind::kAssign;
            stmt->span.begin = start;
            stmt->assign_targets.push_back(std::move(first));
            stmt->assign_values = ParseExprList();
            stmt->span.end = Previous().span.end;
            return stmt;
        }

        if (IsBareNameExpr(*first) && LooksLikeBareNameListAssignment()) {
            auto stmt = std::make_unique<Stmt>();
            stmt->kind = Stmt::Kind::kBindingOrAssign;
            stmt->span.begin = start;
            stmt->has_initializer = true;
            stmt->pattern.names.push_back(first->text);
            stmt->pattern.span.begin = first->span.begin;
            stmt->pattern.span.end = first->span.end;

            while (Match(TokenKind::kComma)) {
                const auto name = ParseIdentifier("expected identifier after ',' in binding-or-assignment");
                if (name.has_value()) {
                    stmt->pattern.names.push_back(*name);
                    stmt->pattern.span.end = Previous().span.end;
                }
            }

            Consume(TokenKind::kAssign, "expected '=' after identifier list");
            stmt->exprs = ParseExprList();
            stmt->span.end = Previous().span.end;
            return stmt;
        }

        if (Check(TokenKind::kComma)) {
            auto stmt = std::make_unique<Stmt>();
            stmt->kind = Stmt::Kind::kAssign;
            stmt->span.begin = start;

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

    std::vector<std::unique_ptr<Expr>> ParseExprList() {
        std::vector<std::unique_ptr<Expr>> result;
        result.push_back(ParseExpr());
        while (Match(TokenKind::kComma)) {
            result.push_back(ParseExpr());
        }
        return result;
    }

    std::unique_ptr<Expr> ParseExpr(bool allow_aggregate_init = true) {
        const bool previous = allow_aggregate_init_;
        allow_aggregate_init_ = allow_aggregate_init;
        auto expr = ParseRangeExpr();
        allow_aggregate_init_ = previous;
        return expr;
    }

    std::unique_ptr<Expr> ParseRangeExpr() {
        auto expr = ParseBinaryChain(&Parser::ParseLogicalOrExpr, {TokenKind::kRange});
        if (expr->kind == Expr::Kind::kBinary && expr->text == "..") {
            expr->kind = Expr::Kind::kRange;
        }
        return expr;
    }

    std::unique_ptr<Expr> ParseLogicalOrExpr() {
        return ParseBinaryChain(&Parser::ParseLogicalAndExpr, {TokenKind::kPipePipe});
    }

    std::unique_ptr<Expr> ParseLogicalAndExpr() {
        return ParseBinaryChain(&Parser::ParseEqualityExpr, {TokenKind::kAmpAmp});
    }

    std::unique_ptr<Expr> ParseEqualityExpr() {
        return ParseBinaryChain(&Parser::ParseRelationalExpr, {TokenKind::kEqEq, TokenKind::kBangEq});
    }

    std::unique_ptr<Expr> ParseRelationalExpr() {
        return ParseBinaryChain(&Parser::ParseShiftExpr, {TokenKind::kLt, TokenKind::kLtEq, TokenKind::kGt, TokenKind::kGtEq});
    }

    std::unique_ptr<Expr> ParseShiftExpr() {
        return ParseBinaryChain(&Parser::ParseAdditiveExpr, {TokenKind::kLtLt, TokenKind::kGtGt});
    }

    std::unique_ptr<Expr> ParseAdditiveExpr() {
        return ParseBinaryChain(&Parser::ParseMultiplicativeExpr, {TokenKind::kPlus, TokenKind::kMinus});
    }

    std::unique_ptr<Expr> ParseMultiplicativeExpr() {
        return ParseBinaryChain(&Parser::ParseUnaryExpr, {TokenKind::kStar, TokenKind::kSlash, TokenKind::kPercent});
    }

    std::unique_ptr<Expr> ParseBinaryChain(std::unique_ptr<Expr> (Parser::*parse_operand)(),
                                           std::initializer_list<TokenKind> operators) {
        auto expr = (this->*parse_operand)();
        while (MatchesAny(operators)) {
            const auto op = Previous();
            auto node = std::make_unique<Expr>();
            node->kind = Expr::Kind::kBinary;
            node->span.begin = expr->span.begin;
            node->text = std::string(mc::lex::ToString(op.kind));
            node->left = std::move(expr);
            node->right = (this->*parse_operand)();
            node->span.end = node->right->span.end;
            expr = std::move(node);
        }
        return expr;
    }

    bool MatchesAny(std::initializer_list<TokenKind> kinds) {
        for (const auto kind : kinds) {
            if (Match(kind)) {
                return true;
            }
        }
        return false;
    }

    std::optional<std::size_t> SkipArrayLengthLookahead(std::size_t offset) const {
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

    std::optional<std::size_t> SkipTypeExprLookahead(std::size_t offset, bool allow_bare_identifier) const {
        switch (Peek(offset).kind) {
            case TokenKind::kConst:
                return SkipTypeExprLookahead(offset + 1, true);
            case TokenKind::kStar:
                return SkipTypeExprLookahead(offset + 1, true);
            case TokenKind::kLBracket: {
                const auto after_length = SkipArrayLengthLookahead(offset + 1);
                if (!after_length.has_value()) {
                    return std::nullopt;
                }
                return SkipTypeExprLookahead(*after_length, true);
            }
            case TokenKind::kLParen: {
                const auto inner = SkipTypeExprLookahead(offset + 1, true);
                if (!inner.has_value() || Peek(*inner).kind != TokenKind::kRParen) {
                    return std::nullopt;
                }
                return *inner + 1;
            }
            case TokenKind::kIdentifier: {
                std::size_t next = offset + 1;
                if (Peek(next).kind == TokenKind::kLt) {
                    ++next;
                    if (Peek(next).kind != TokenKind::kGt) {
                        while (true) {
                            const auto type_arg_end = SkipTypeExprLookahead(next, true);
                            if (!type_arg_end.has_value()) {
                                return std::nullopt;
                            }
                            next = *type_arg_end;
                            if (Peek(next).kind == TokenKind::kComma) {
                                ++next;
                                continue;
                            }
                            break;
                        }
                    }
                    if (Peek(next).kind != TokenKind::kGt) {
                        return std::nullopt;
                    }
                    return next + 1;
                }
                return allow_bare_identifier ? std::optional<std::size_t>(next) : std::nullopt;
            }
            default:
                return std::nullopt;
        }
    }

    bool LooksLikeTypeCallExpr() const {
        if (!Check(TokenKind::kLParen)) {
            return false;
        }
        const auto inner_type_end = SkipTypeExprLookahead(1, true);
        return inner_type_end.has_value() && Peek(*inner_type_end).kind == TokenKind::kRParen && Peek(*inner_type_end + 1).kind == TokenKind::kLParen;
    }

    std::unique_ptr<Expr> ParseTypeCallExpr() {
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::Kind::kCall;
        expr->span.begin = Current().span.begin;
        Consume(TokenKind::kLParen, "expected '(' before conversion target type");
        expr->type_target = ParseTypeExpr();
        Consume(TokenKind::kRParen, "expected ')' after conversion target type");
        Consume(TokenKind::kLParen, "expected '(' after conversion target type");
        if (!Check(TokenKind::kRParen)) {
            do {
                expr->args.push_back(ParseExpr());
            } while (Match(TokenKind::kComma));
        }
        Consume(TokenKind::kRParen, "expected ')' after call arguments");
        expr->span.end = Previous().span.end;
        return expr;
    }

    std::unique_ptr<Expr> ParseUnaryExpr() {
        if (LooksLikeTypeCallExpr()) {
            return ParseTypeCallExpr();
        }
        if (MatchesAny({TokenKind::kBang, TokenKind::kMinus, TokenKind::kAmp, TokenKind::kStar})) {
            const auto op = Previous();
            auto expr = std::make_unique<Expr>();
            expr->kind = Expr::Kind::kUnary;
            expr->span.begin = op.span.begin;
            expr->text = std::string(mc::lex::ToString(op.kind));
            expr->left = ParseUnaryExpr();
            expr->span.end = expr->left->span.end;
            return expr;
        }
        return ParsePostfixExpr();
    }

    std::unique_ptr<Expr> ParsePostfixExpr() {
        auto expr = ParsePrimaryExpr();
        while (true) {
            if (Match(TokenKind::kDot)) {
                const auto member = ParseIdentifier("expected field name after '.'");
                auto node = std::make_unique<Expr>();
                node->span.begin = expr->span.begin;
                if (expr->kind == Expr::Kind::kName) {
                    node->kind = Expr::Kind::kQualifiedName;
                    node->text = expr->text;
                    node->secondary_text = member.value_or("");
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

            if (Match(TokenKind::kLParen)) {
                auto node = std::make_unique<Expr>();
                node->kind = Expr::Kind::kCall;
                node->span.begin = expr->span.begin;
                node->left = std::move(expr);
                if (!Check(TokenKind::kRParen)) {
                    do {
                        node->args.push_back(ParseExpr());
                    } while (Match(TokenKind::kComma));
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
                if (!Check(TokenKind::kRBrace)) {
                    do {
                        node->field_inits.push_back(ParseFieldInit());
                    } while (Match(TokenKind::kComma));
                }
                Consume(TokenKind::kRBrace, "expected '}' after aggregate initializer");
                node->span.end = Previous().span.end;
                expr = std::move(node);
                continue;
            }

            break;
        }
        return expr;
    }

    FieldInit ParseFieldInit() {
        FieldInit init;
        init.span.begin = Current().span.begin;
        if (Check(TokenKind::kIdentifier) && Peek(1).kind == TokenKind::kColon) {
            init.has_name = true;
            init.name = Current().lexeme;
            Advance();
            Advance();
        }
        init.value = ParseExpr();
        init.span.end = init.value->span.end;
        return init;
    }

    std::unique_ptr<Expr> ParseAssignmentTarget() {
        const auto start = Current().span.begin;
        auto target = ParsePostfixExpr();
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

    std::unique_ptr<Expr> ParsePrimaryExpr() {
        if (IsLiteral(Current().kind)) {
            auto expr = std::make_unique<Expr>();
            expr->kind = Expr::Kind::kLiteral;
            expr->span = Current().span;
            expr->text = Current().lexeme;
            expr->secondary_text = std::string(mc::lex::ToString(Current().kind));
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

        ReportError(Current(), "expected expression");
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::Kind::kLiteral;
        expr->span = Current().span;
        expr->text = "<error>";
        return expr;
    }

    bool LooksLikeBindingStmt() const {
        if (Current().kind != TokenKind::kIdentifier) {
            return false;
        }

        std::size_t offset = 1;
        while (Peek(offset).kind == TokenKind::kComma && Peek(offset + 1).kind == TokenKind::kIdentifier) {
            offset += 2;
        }

        return Peek(offset).kind == TokenKind::kColon;
    }

    void SkipStatementSeparator() {
        if (Match(TokenKind::kNewline)) {
            SkipNewlines();
            return;
        }
        if (Check(TokenKind::kRBrace) || Check(TokenKind::kCase) || Check(TokenKind::kDefault)) {
            return;
        }
        ReportError(Current(), "expected newline between statements");
    }

    void ConsumeMemberSeparator() {
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

    void SynchronizeStatement() {
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

    bool IsLiteral(TokenKind kind) const {
        return kind == TokenKind::kIntLiteral || kind == TokenKind::kFloatLiteral || kind == TokenKind::kStringLiteral ||
               kind == TokenKind::kTrue || kind == TokenKind::kFalse || kind == TokenKind::kNil;
    }

    bool IsAggregateInitBase(const Expr& expr) const {
        return expr.kind == Expr::Kind::kName || expr.kind == Expr::Kind::kQualifiedName;
    }

    bool IsBareNameExpr(const Expr& expr) const {
        return expr.kind == Expr::Kind::kName;
    }

    bool LooksLikeBareNameListAssignment() const {
        if (!Check(TokenKind::kComma)) {
            return false;
        }

        std::size_t offset = 0;
        while (Peek(offset).kind == TokenKind::kComma && Peek(offset + 1).kind == TokenKind::kIdentifier) {
            offset += 2;
        }

        return Peek(offset).kind == TokenKind::kAssign;
    }

    bool IsAssignmentTarget(const Expr& expr) const {
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

    bool IsStatementTerminator(TokenKind kind) const {
        return kind == TokenKind::kNewline || kind == TokenKind::kRBrace || kind == TokenKind::kEof;
    }

    std::unique_ptr<Stmt> MakeLeafStmt(Stmt::Kind kind,
                                       mc::support::SourcePosition begin,
                                       mc::support::SourcePosition end) {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = kind;
        stmt->span = {begin, end};
        return stmt;
    }

    const std::vector<Token>& tokens_;
    std::filesystem::path file_path_;
    support::DiagnosticSink& diagnostics_;
    std::size_t index_ = 0;
    bool ok_ = true;
    bool allow_aggregate_init_ = true;
};

}  // namespace

ParseResult Parse(const mc::lex::LexResult& lexed_module,
                  const std::filesystem::path& file_path,
                  support::DiagnosticSink& diagnostics) {
    return Parser(lexed_module, file_path, diagnostics).Run();
}

}  // namespace mc::parse
