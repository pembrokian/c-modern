#ifndef C_MODERN_COMPILER_PARSE_PARSER_INTERNAL_H_
#define C_MODERN_COMPILER_PARSE_PARSER_INTERNAL_H_

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "compiler/parse/parser.h"

namespace mc::parse {

constexpr std::size_t kMaxTypeExprDepth = 256;
constexpr std::size_t kMaxUnaryExprDepth = 256;

using mc::ast::Attribute;
using mc::ast::AttributeArg;
using mc::ast::CasePattern;
using mc::ast::Decl;
using mc::ast::DefaultCase;
using mc::ast::EnumVariantDecl;
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
    // The parser borrows tokens from the lexer result and reports directly into
    // the caller-owned diagnostic sink. Both must outlive the Parser.
    Parser(const mc::lex::LexResult& lexed_module,
           const std::filesystem::path& file_path,
           mc::support::DiagnosticSink& diagnostics);

    ParseResult Run();

  private:
    bool AtEnd() const;
    const Token& Current() const;
    const Token& Previous() const;
    const Token& Peek(std::size_t offset) const;
    bool Check(TokenKind kind) const;
    bool Match(TokenKind kind);
    const Token& Advance();
    const Token* Consume(TokenKind kind, const char* message);
    void ReportError(const Token& token, const std::string& message);

    void SkipNewlines();
    bool RequireSourceSeparator(const char* context);
    void SynchronizeTopLevel();
    // ParseIdentifier snapshots Current().lexeme before Advance() invalidates
    // the current-token view for callers that need the identifier text.
    std::optional<std::string> ParseIdentifier(const char* message);

    ImportDecl ParseImportDecl(mc::support::SourcePosition start);
    std::vector<Attribute> ParseAttributes();
    std::optional<std::vector<std::string>> ParseTypeParams();
    std::vector<std::unique_ptr<TypeExpr>> ParseCallTypeArgs();
    std::optional<Decl> ParseTopLevelDecl();
    std::optional<Decl> ParseExternFuncDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start);
    std::optional<Decl> ParseFuncDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start);
    std::optional<Decl> ParseFuncLikeTail(Decl decl, bool expect_body);
    ParamDecl ParseParameter();
    std::vector<std::unique_ptr<TypeExpr>> ParseReturnTypes();
    bool IsTypeExprStart(TokenKind kind) const;
    std::vector<std::unique_ptr<TypeExpr>> ParseProcedureParamTypes(std::size_t depth);
    std::vector<std::unique_ptr<TypeExpr>> ParseProcedureReturnTypes(std::size_t depth);
    std::optional<Decl> ParseStructDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start);
    std::optional<Decl> ParseEnumDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start);
    std::optional<Decl> ParseDistinctDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start);
    std::optional<Decl> ParseTypeAliasDecl(std::vector<Attribute> attributes, mc::support::SourcePosition start);
    std::optional<Decl> ParseBindingDecl(std::vector<Attribute> attributes,
                       Decl::Kind kind,
                       mc::support::SourcePosition start,
                       bool allow_storage_without_initializer);
    FieldDecl ParseFieldDecl();
    EnumVariantDecl ParseEnumVariantDecl();
    FieldDecl ParseVariantField();
    NamePattern ParseBindingPattern();
    std::unique_ptr<TypeExpr> ParseTypeExpr();
    std::unique_ptr<TypeExpr> ParseTypeExpr(std::size_t depth);

    std::unique_ptr<Stmt> ParseBlockStmt();
    std::unique_ptr<Stmt> ParseStatement();
    std::unique_ptr<Stmt> ParseIfStmt(mc::support::SourcePosition start);
    std::unique_ptr<Stmt> ParseSwitchStmt(mc::support::SourcePosition start);
    SwitchCase ParseSwitchCase(mc::support::SourcePosition start);
    DefaultCase ParseDefaultCase(mc::support::SourcePosition start);
    std::vector<std::unique_ptr<Stmt>> ParseCaseBodyStatements();
    bool LooksLikeVariantCasePattern() const;
    std::string ParseCaseVariantName();
    void ParseCasePatternBindings(CasePattern& pattern);
    CasePattern ParseCasePattern();
    std::unique_ptr<Stmt> ParseForStmt(mc::support::SourcePosition start);
    std::unique_ptr<Stmt> ParseWhileStmt(mc::support::SourcePosition start);
    std::unique_ptr<Stmt> ParseReturnStmt(mc::support::SourcePosition start);
    std::unique_ptr<Stmt> ParseDeferStmt(mc::support::SourcePosition start);
    std::unique_ptr<Stmt> ParseBindingStmt(Stmt::Kind kind, mc::support::SourcePosition start, bool allow_storage_without_initializer);
    std::unique_ptr<Stmt> ParseAssignOrExprStmt();
    std::vector<std::unique_ptr<Expr>> ParseExprList();

    std::unique_ptr<Expr> ParseExpr(bool allow_aggregate_init = true);
    std::unique_ptr<Expr> ParseRangeExpr();
    std::unique_ptr<Expr> ParseLogicalOrExpr();
    std::unique_ptr<Expr> ParseLogicalAndExpr();
    std::unique_ptr<Expr> ParseBitwiseOrExpr();
    std::unique_ptr<Expr> ParseBitwiseXorExpr();
    std::unique_ptr<Expr> ParseBitwiseAndExpr();
    std::unique_ptr<Expr> ParseIsExpr();
    std::unique_ptr<Expr> ParseEqualityExpr();
    std::unique_ptr<Expr> ParseRelationalExpr();
    std::unique_ptr<Expr> ParseShiftExpr();
    std::unique_ptr<Expr> ParseAdditiveExpr();
    std::unique_ptr<Expr> ParseMultiplicativeExpr();
    std::optional<std::size_t> SkipArrayLengthLookahead(std::size_t offset) const;
    std::optional<std::size_t> SkipTypeExprLookahead(std::size_t offset, bool allow_bare_identifier) const;
    std::optional<std::size_t> SkipTypeArgListLookahead(std::size_t offset, std::size_t depth) const;
    std::optional<std::size_t> SkipTypeExprLookahead(std::size_t offset,
                                                     bool allow_bare_identifier,
                                                     std::size_t depth) const;
    bool LooksLikeTypeCallExpr() const;
    bool LooksLikeBareIntegerConversionExpr() const;
    bool LooksLikeTypeAggregateInitExpr() const;
    bool LooksLikePostfixTypeArgs() const;
    std::unique_ptr<Expr> ParseBareIntegerConversionExpr();
    std::unique_ptr<Expr> ParseTypeCallExpr();
    std::unique_ptr<Expr> ParseTypeAggregateInitExpr();
    std::unique_ptr<Expr> ParseBraceAggregateInitExpr();
    std::unique_ptr<Expr> ParseUnaryExpr();
    std::unique_ptr<Expr> ParsePostfixExpr();
    FieldInit ParseFieldInit(bool allow_dotted_name = false);
    std::unique_ptr<Expr> ParseAssignmentTarget();
    std::unique_ptr<Expr> ParsePrimaryExpr();

    struct ParsedBindingTail {
      NamePattern pattern;
      std::unique_ptr<TypeExpr> type_ann;
      std::vector<std::unique_ptr<Expr>> initializers;
      bool has_initializer = false;
      mc::support::SourcePosition end {};
    };

    ParsedBindingTail ParseBindingTail(bool allow_storage_without_initializer, const char* missing_initializer_message);

    bool LooksLikeBindingStmt() const;
    void SkipStatementSeparator();
    void RequireStatementSeparatorOrSync();
    void ConsumeMemberSeparator();
    void SynchronizeStatement();
    bool IsLiteral(TokenKind kind) const;
    bool IsAggregateInitBase(const Expr& expr) const;
    bool IsBareNameExpr(const Expr& expr) const;
    bool LooksLikeBareNameListAssignment() const;
    bool IsAssignmentTarget(const Expr& expr) const;
    bool IsCaseBoundary() const;
    bool IsPrefixUnaryOperator(TokenKind kind) const;
    bool IsStatementTerminator(TokenKind kind) const;
    std::unique_ptr<Stmt> MakeLeafStmt(Stmt::Kind kind,
                                       mc::support::SourcePosition begin,
                                       mc::support::SourcePosition end);

    const std::vector<Token>& tokens_;
    std::filesystem::path file_path_;
    mc::support::DiagnosticSink& diagnostics_;
    std::size_t index_ = 0;
    // ok_ carries lexer failure state into parsing and is also cleared on
    // parser-reported errors. The broader lexer/parser result contract is kept
    // intentionally unchanged here.
    bool ok_ = true;
    bool allow_aggregate_init_ = true;

    struct AggInitGuard {
        Parser& parser_;
        bool saved_;

        AggInitGuard(Parser& parser, bool allow);
        ~AggInitGuard();
    };
};

}  // namespace mc::parse

#endif  // C_MODERN_COMPILER_PARSE_PARSER_INTERNAL_H_