#include "compiler/lex/lexer.h"

#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace mc::lex {
namespace {

using mc::support::Diagnostic;
using mc::support::DiagnosticSeverity;
using mc::support::SourcePosition;
using mc::support::SourceSpan;

const std::unordered_map<std::string_view, TokenKind> kKeywords = {
    {"import", TokenKind::kImport},     {"export", TokenKind::kExport},   {"func", TokenKind::kFunc},
    {"struct", TokenKind::kStruct},     {"enum", TokenKind::kEnum},       {"distinct", TokenKind::kDistinct},
    {"type", TokenKind::kType},         {"extern", TokenKind::kExtern},   {"if", TokenKind::kIf},
    {"else", TokenKind::kElse},         {"switch", TokenKind::kSwitch},   {"case", TokenKind::kCase},
    {"default", TokenKind::kDefault},   {"for", TokenKind::kFor},         {"while", TokenKind::kWhile},
    {"in", TokenKind::kIn},             {"is", TokenKind::kIs},           {"with", TokenKind::kWith},
    {"return", TokenKind::kReturn},
    {"break", TokenKind::kBreak},
    {"continue", TokenKind::kContinue}, {"defer", TokenKind::kDefer},     {"var", TokenKind::kVar},
    {"const", TokenKind::kConst},       {"nil", TokenKind::kNil},         {"true", TokenKind::kTrue},
    {"false", TokenKind::kFalse},
};

int DigitValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

std::optional<std::int64_t> ParseIntegerLiteralValue(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    int base = 10;
    if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty()) {
        return std::nullopt;
    }

    std::uint64_t value = 0;
    bool saw_digit = false;
    for (const char ch : text) {
        if (ch == '_') {
            continue;
        }
        const int digit = DigitValue(ch);
        if (digit < 0 || digit >= base) {
            return std::nullopt;
        }
        saw_digit = true;
        if (value > (static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - static_cast<std::uint64_t>(digit)) /
                        static_cast<std::uint64_t>(base)) {
            return std::nullopt;
        }
        value = (value * static_cast<std::uint64_t>(base)) + static_cast<std::uint64_t>(digit);
    }
    if (!saw_digit) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(value);
}

std::optional<double> ParseFloatLiteralValue(std::string_view text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (const char ch : text) {
        if (ch != '_') {
            normalized.push_back(ch);
        }
    }
    std::istringstream stream {normalized};
    double value = 0.0;
    stream >> value;
    if (!stream || !stream.eof()) {
        return std::nullopt;
    }
    return value;
}

class Lexer {
  public:
    Lexer(std::string_view source_text,
                    std::string file_path,
          support::DiagnosticSink& diagnostics)
                : source_text_(source_text), file_path_(std::move(file_path)), diagnostics_(diagnostics) {}

    LexResult Run() {
        LexResult result;

        while (!AtEnd()) {
            SkipTrivia();
            if (AtEnd()) {
                break;
            }

            if (ConsumeNewline(result.tokens)) {
                continue;
            }

            const auto start = CurrentPosition();
            const char current = Peek();

            if (IsIdentifierStart(current)) {
                result.tokens.push_back(LexIdentifier());
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(current)) != 0) {
                result.tokens.push_back(LexNumber(result.ok));
                continue;
            }

            if (current == '"') {
                result.tokens.push_back(LexString(result.ok));
                continue;
            }

            if (const auto punctuation = LexPunctuation(start); punctuation.has_value()) {
                result.tokens.push_back(*punctuation);
                continue;
            }

            Advance();
            result.ok = false;
            diagnostics_.Report({
                .file_path = file_path_,
                .span = {start, CurrentPosition()},
                .severity = DiagnosticSeverity::kError,
                .message = "unexpected character",
            });
        }

        result.tokens.push_back({.kind = TokenKind::kEof, .lexeme = "", .span = {CurrentPosition(), CurrentPosition()}});
        return result;
    }

  private:
    bool AtEnd() const {
        return index_ >= source_text_.size();
    }

    char Peek() const {
        return AtEnd() ? '\0' : source_text_[index_];
    }

    char PeekNext() const {
        return index_ + 1 >= source_text_.size() ? '\0' : source_text_[index_ + 1];
    }

    char Advance() {
        const char current = source_text_[index_++];
        if (current == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        return current;
    }

    SourcePosition CurrentPosition() const {
        return {.line = line_, .column = column_};
    }

    static bool IsIdentifierStart(char value) {
        return std::isalpha(static_cast<unsigned char>(value)) != 0 || value == '_';
    }

    static bool IsIdentifierContinue(char value) {
        return std::isalnum(static_cast<unsigned char>(value)) != 0 || value == '_';
    }

    static bool IsDecimalDigit(char value) {
        return std::isdigit(static_cast<unsigned char>(value)) != 0;
    }

    static bool IsHexDigit(char value) {
        return std::isxdigit(static_cast<unsigned char>(value)) != 0;
    }

    void SkipTrivia() {
        while (!AtEnd()) {
            if (Peek() == ' ' || Peek() == '\t' || Peek() == '\r') {
                Advance();
                continue;
            }

            if (Peek() == '/' && PeekNext() == '/') {
                while (!AtEnd() && Peek() != '\n') {
                    Advance();
                }
                continue;
            }

            break;
        }
    }

    bool ConsumeNewline(std::vector<Token>& tokens) {
        if (Peek() != '\n') {
            return false;
        }

        const auto start = CurrentPosition();
        while (!AtEnd() && Peek() == '\n') {
            Advance();
            SkipTrivia();
        }

        tokens.push_back({.kind = TokenKind::kNewline, .lexeme = "\\n", .span = {start, CurrentPosition()}});
        return true;
    }

    Token LexIdentifier() {
        const auto start_position = CurrentPosition();
        const auto start_index = index_;
        while (!AtEnd() && IsIdentifierContinue(Peek())) {
            Advance();
        }

        const auto text = std::string(source_text_.substr(start_index, index_ - start_index));
        const auto keyword = kKeywords.find(text);
        return {
            .kind = keyword == kKeywords.end() ? TokenKind::kIdentifier : keyword->second,
            .lexeme = text,
            .span = {start_position, CurrentPosition()},
        };
    }

    Token LexNumber(bool& ok) {
        const auto start_position = CurrentPosition();
        const auto start_index = index_;
        bool is_float = false;

        if (Peek() == '0' && (PeekNext() == 'x' || PeekNext() == 'X')) {
            Advance();
            Advance();

            ConsumeDigitSequence(ok,
                                 start_position,
                                 &Lexer::IsHexDigit,
                                 true,
                                 "expected hexadecimal digits after 0x",
                                 "numeric separator must appear between digits");
        } else {
            ConsumeDigitSequence(ok,
                                 start_position,
                                 &Lexer::IsDecimalDigit,
                                 true,
                                 "expected digits in numeric literal",
                                 "numeric separator must appear between digits");

            if (!AtEnd() && Peek() == '.' && PeekNext() != '.') {
                is_float = true;
                Advance();
                ConsumeDigitSequence(ok,
                                     start_position,
                                     &Lexer::IsDecimalDigit,
                                     true,
                                     "expected digit after decimal point",
                                     "numeric separator must appear between digits");
            }

            if (!AtEnd() && (Peek() == 'e' || Peek() == 'E')) {
                is_float = true;
                Advance();
                if (!AtEnd() && (Peek() == '+' || Peek() == '-')) {
                    Advance();
                }
                ConsumeDigitSequence(ok,
                                     start_position,
                                     &Lexer::IsDecimalDigit,
                                     false,
                                     "expected digits after exponent marker",
                                     "numeric separator is not allowed in exponent");
            }
        }

        const std::string lexeme = std::string(source_text_.substr(start_index, index_ - start_index));
        Token token {
            .kind = is_float ? TokenKind::kFloatLiteral : TokenKind::kIntLiteral,
            .lexeme = lexeme,
            .span = {start_position, CurrentPosition()},
        };
        if (is_float) {
            token.float_value = ParseFloatLiteralValue(lexeme);
        } else {
            token.integer_value = ParseIntegerLiteralValue(lexeme);
        }
        return token;
    }

    void ConsumeDigitSequence(bool& ok,
                              const SourcePosition& literal_start,
                              bool (*is_digit)(char),
                              bool allow_separator,
                              const char* missing_digits_message,
                              const char* separator_message) {
        bool saw_digit = false;
        bool just_saw_separator = false;

        while (!AtEnd()) {
            const char value = Peek();
            if (is_digit(value)) {
                saw_digit = true;
                just_saw_separator = false;
                Advance();
                continue;
            }

            if (value == '_') {
                const char next = PeekNext();
                const bool separator_is_valid = allow_separator && saw_digit && !just_saw_separator && next != '\0' && is_digit(next);
                if (!separator_is_valid) {
                    ok = false;
                    diagnostics_.Report({
                        .file_path = file_path_,
                        .span = {literal_start, CurrentPosition()},
                        .severity = DiagnosticSeverity::kError,
                        .message = separator_message,
                    });
                    Advance();
                    continue;
                }

                just_saw_separator = true;
                Advance();
                continue;
            }

            break;
        }

        if (!saw_digit) {
            ok = false;
            diagnostics_.Report({
                .file_path = file_path_,
                .span = {literal_start, CurrentPosition()},
                .severity = DiagnosticSeverity::kError,
                .message = missing_digits_message,
            });
        }
    }

    Token LexString(bool& ok) {
        const auto start_position = CurrentPosition();
        const auto start_index = index_;
        Advance();

        bool terminated = false;
        while (!AtEnd()) {
            const char value = Advance();
            if (value == '"') {
                terminated = true;
                break;
            }

            if (value == '\\') {
                if (AtEnd()) {
                    break;
                }

                const char escape = Advance();
                switch (escape) {
                    case '"':
                    case '\\':
                    case 'n':
                    case 'r':
                    case 't':
                    case '0':
                        break;
                    default:
                        ok = false;
                        diagnostics_.Report({
                            .file_path = file_path_,
                            .span = {start_position, CurrentPosition()},
                            .severity = DiagnosticSeverity::kError,
                            .message = "unsupported string escape",
                        });
                        break;
                }
                continue;
            }

            if (value == '\n' || value == '\r') {
                break;
            }
        }

        if (!terminated) {
            ok = false;
            diagnostics_.Report({
                .file_path = file_path_,
                .span = {start_position, CurrentPosition()},
                .severity = DiagnosticSeverity::kError,
                .message = "unterminated string literal",
            });
        }

        return {
            .kind = TokenKind::kStringLiteral,
            .lexeme = std::string(source_text_.substr(start_index, index_ - start_index)),
            .span = {start_position, CurrentPosition()},
        };
    }

    std::optional<Token> LexPunctuation(SourcePosition start) {
        const auto emit = [&](TokenKind kind, std::size_t width) -> Token {
            const auto start_index = index_;
            for (std::size_t count = 0; count < width; ++count) {
                Advance();
            }
            return {
                .kind = kind,
                .lexeme = std::string(source_text_.substr(start_index, width)),
                .span = {start, CurrentPosition()},
            };
        };

        switch (Peek()) {
            case '{':
                return emit(TokenKind::kLBrace, 1);
            case '}':
                return emit(TokenKind::kRBrace, 1);
            case '(':
                return emit(TokenKind::kLParen, 1);
            case ')':
                return emit(TokenKind::kRParen, 1);
            case '[':
                return emit(TokenKind::kLBracket, 1);
            case ']':
                return emit(TokenKind::kRBracket, 1);
            case ',':
                return emit(TokenKind::kComma, 1);
            case ':':
                return emit(TokenKind::kColon, 1);
            case '@':
                return emit(TokenKind::kAt, 1);
            case '.':
                if (PeekNext() == '*') {
                    return emit(TokenKind::kDotDeref, 2);
                }
                if (PeekNext() == '.') {
                    return emit(TokenKind::kRange, 2);
                }
                return emit(TokenKind::kDot, 1);
            case '=':
                if (PeekNext() == '=') {
                    return emit(TokenKind::kEqEq, 2);
                }
                return emit(TokenKind::kAssign, 1);
            case '!':
                if (PeekNext() == '=') {
                    return emit(TokenKind::kBangEq, 2);
                }
                return emit(TokenKind::kBang, 1);
            case '<':
                if (PeekNext() == '<') {
                    return emit(TokenKind::kLtLt, 2);
                }
                if (PeekNext() == '=') {
                    return emit(TokenKind::kLtEq, 2);
                }
                return emit(TokenKind::kLt, 1);
            case '>':
                if (PeekNext() == '>') {
                    return emit(TokenKind::kGtGt, 2);
                }
                if (PeekNext() == '=') {
                    return emit(TokenKind::kGtEq, 2);
                }
                return emit(TokenKind::kGt, 1);
            case '+':
                return emit(TokenKind::kPlus, 1);
            case '-':
                if (PeekNext() == '>') {
                    return emit(TokenKind::kArrow, 2);
                }
                return emit(TokenKind::kMinus, 1);
            case '*':
                return emit(TokenKind::kStar, 1);
            case '/':
                return emit(TokenKind::kSlash, 1);
            case '%':
                return emit(TokenKind::kPercent, 1);
            case '&':
                if (PeekNext() == '&') {
                    return emit(TokenKind::kAmpAmp, 2);
                }
                return emit(TokenKind::kAmp, 1);
            case '|':
                if (PeekNext() == '|') {
                    return emit(TokenKind::kPipePipe, 2);
                }
                return emit(TokenKind::kPipe, 1);
            case '^':
                return emit(TokenKind::kCaret, 1);
            default:
                break;
        }

        return std::nullopt;
    }

    std::string_view source_text_;
    std::string file_path_;
    support::DiagnosticSink& diagnostics_;
    std::size_t index_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

}  // namespace

std::string_view ToString(TokenKind kind) {
    switch (kind) {
        case TokenKind::kNewline:
            return "newline";
        case TokenKind::kIdentifier:
            return "identifier";
        case TokenKind::kImport:
            return "import";
        case TokenKind::kExport:
            return "export";
        case TokenKind::kFunc:
            return "func";
        case TokenKind::kStruct:
            return "struct";
        case TokenKind::kEnum:
            return "enum";
        case TokenKind::kDistinct:
            return "distinct";
        case TokenKind::kType:
            return "type";
        case TokenKind::kExtern:
            return "extern";
        case TokenKind::kIf:
            return "if";
        case TokenKind::kElse:
            return "else";
        case TokenKind::kSwitch:
            return "switch";
        case TokenKind::kCase:
            return "case";
        case TokenKind::kDefault:
            return "default";
        case TokenKind::kFor:
            return "for";
        case TokenKind::kWhile:
            return "while";
        case TokenKind::kIn:
            return "in";
        case TokenKind::kIs:
            return "is";
        case TokenKind::kWith:
            return "with";
        case TokenKind::kReturn:
            return "return";
        case TokenKind::kBreak:
            return "break";
        case TokenKind::kContinue:
            return "continue";
        case TokenKind::kDefer:
            return "defer";
        case TokenKind::kVar:
            return "var";
        case TokenKind::kConst:
            return "const";
        case TokenKind::kNil:
            return "nil";
        case TokenKind::kTrue:
            return "true";
        case TokenKind::kFalse:
            return "false";
        case TokenKind::kIntLiteral:
            return "int_lit";
        case TokenKind::kFloatLiteral:
            return "float_lit";
        case TokenKind::kStringLiteral:
            return "string_lit";
        case TokenKind::kLBrace:
            return "{";
        case TokenKind::kRBrace:
            return "}";
        case TokenKind::kLParen:
            return "(";
        case TokenKind::kRParen:
            return ")";
        case TokenKind::kLBracket:
            return "[";
        case TokenKind::kRBracket:
            return "]";
        case TokenKind::kComma:
            return ",";
        case TokenKind::kColon:
            return ":";
        case TokenKind::kDot:
            return ".";
        case TokenKind::kAt:
            return "@";
        case TokenKind::kDotDeref:
            return ".*";
        case TokenKind::kRange:
            return "..";
        case TokenKind::kAssign:
            return "=";
        case TokenKind::kEqEq:
            return "==";
        case TokenKind::kBangEq:
            return "!=";
        case TokenKind::kLt:
            return "<";
        case TokenKind::kLtEq:
            return "<=";
        case TokenKind::kLtLt:
            return "<<";
        case TokenKind::kGt:
            return ">";
        case TokenKind::kGtEq:
            return ">=";
        case TokenKind::kGtGt:
            return ">>";
        case TokenKind::kPlus:
            return "+";
        case TokenKind::kMinus:
            return "-";
        case TokenKind::kStar:
            return "*";
        case TokenKind::kSlash:
            return "/";
        case TokenKind::kPercent:
            return "%";
        case TokenKind::kAmp:
            return "&";
        case TokenKind::kPipe:
            return "|";
        case TokenKind::kCaret:
            return "^";
        case TokenKind::kBang:
            return "!";
        case TokenKind::kAmpAmp:
            return "&&";
        case TokenKind::kPipePipe:
            return "||";
        case TokenKind::kArrow:
            return "->";
        case TokenKind::kEof:
            return "eof";
    }

    return "unknown";
}

LexResult Lex(std::string_view source_text,
              std::string file_path,
              support::DiagnosticSink& diagnostics) {
    return Lexer(source_text, std::move(file_path), diagnostics).Run();
}

}  // namespace mc::lex
