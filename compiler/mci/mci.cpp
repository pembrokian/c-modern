#include "compiler/mci/mci.h"

#include "compiler/sema/const_eval.h"
#include "compiler/sema/type_predicates.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

namespace mc::mci {
namespace {

using mc::support::DiagnosticSeverity;

constexpr int kFormatVersion = 7;

std::optional<sema::Type> ParseTypeText(std::string_view text);

bool ParseBoolField(std::string_view text, bool& value);

std::string EscapeText(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '\\':
                escaped += "\\\\";
                break;
            case '\t':
                escaped += "\\t";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case ';':
                escaped += "\\;";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }
    return escaped;
}

std::optional<std::string> UnescapeText(std::string_view text) {
    std::string output;
    output.reserve(text.size());
    bool escape = false;
    for (char ch : text) {
        if (!escape) {
            if (ch == '\\') {
                escape = true;
            } else {
                output.push_back(ch);
            }
            continue;
        }

        switch (ch) {
            case '\\':
                output.push_back('\\');
                break;
            case 't':
                output.push_back('\t');
                break;
            case 'n':
                output.push_back('\n');
                break;
            case ';':
                output.push_back(';');
                break;
            default:
                return std::nullopt;
        }
        escape = false;
    }

    if (escape) {
        return std::nullopt;
    }
    return output;
}

std::string JoinEscaped(const std::vector<std::string>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << ';';
        }
        stream << EscapeText(values[index]);
    }
    return stream.str();
}

void AppendLengthPrefixed(std::string& output,
                          std::string_view value) {
    output += std::to_string(value.size());
    output.push_back(':');
    output.append(value);
}

std::optional<std::size_t> ParseLengthPrefix(std::string_view& text) {
    const std::size_t separator = text.find(':');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const auto parsed = mc::support::ParseArrayLength(text.substr(0, separator));
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    text.remove_prefix(separator + 1);
    return *parsed;
}

std::optional<std::string_view> ConsumeLengthPrefixed(std::string_view& text) {
    const auto length = ParseLengthPrefix(text);
    if (!length.has_value() || *length > text.size()) {
        return std::nullopt;
    }
    const std::string_view value = text.substr(0, *length);
    text.remove_prefix(*length);
    return value;
}

std::string SerializeConstValue(const std::optional<sema::ConstValue>& value) {
    if (!value.has_value()) {
        return {};
    }
    switch (value->kind) {
        case sema::ConstValue::Kind::kBool:
            return std::string("b:") + (value->bool_value ? "1" : "0");
        case sema::ConstValue::Kind::kInteger:
            return "i:" + std::to_string(value->integer_value);
        case sema::ConstValue::Kind::kFloat:
            return "f:" + value->text;
        case sema::ConstValue::Kind::kString:
            return "s:" + value->text;
        case sema::ConstValue::Kind::kNil:
            return "n:";
        case sema::ConstValue::Kind::kEnum: {
            std::string encoded = "e:";
            AppendLengthPrefixed(encoded, sema::FormatType(value->enum_type));
            AppendLengthPrefixed(encoded, value->variant_name);
            encoded += std::to_string(value->variant_tag);
            encoded.push_back(':');
            encoded += std::to_string(value->elements.size());
            encoded.push_back(':');
            for (std::size_t index = 0; index < value->elements.size(); ++index) {
                const std::string_view field_name = index < value->field_names.size() ? std::string_view(value->field_names[index])
                                                                                       : std::string_view {};
                AppendLengthPrefixed(encoded, field_name);
                const std::string child = SerializeConstValue(value->elements[index]);
                AppendLengthPrefixed(encoded, child);
            }
            return encoded;
        }
        case sema::ConstValue::Kind::kAggregate: {
            std::string encoded = "a:" + std::to_string(value->elements.size()) + ':';
            for (std::size_t index = 0; index < value->elements.size(); ++index) {
                const std::string_view field_name = index < value->field_names.size() ? std::string_view(value->field_names[index])
                                                                                       : std::string_view {};
                AppendLengthPrefixed(encoded, field_name);
                const std::string child = SerializeConstValue(value->elements[index]);
                AppendLengthPrefixed(encoded, child);
            }
            return encoded;
        }
    }
    return {};
}

std::string JoinConstValues(const std::vector<std::optional<sema::ConstValue>>& values) {
    std::vector<std::string> encoded;
    encoded.reserve(values.size());
    for (const auto& value : values) {
        encoded.push_back(SerializeConstValue(value));
    }
    return JoinEscaped(encoded);
}

std::string JoinBoolFlags(const std::vector<bool>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << ';';
        }
        stream << (values[index] ? '1' : '0');
    }
    return stream.str();
}

std::optional<std::vector<bool>> ParseBoolFlags(std::string_view text) {
    if (text.empty()) {
        return std::vector<bool> {};
    }

    std::vector<bool> values;
    std::string_view remaining = text;
    while (true) {
        const std::size_t separator = remaining.find(';');
        const std::string_view field = separator == std::string_view::npos ? remaining : remaining.substr(0, separator);
        bool value = false;
        if (!ParseBoolField(field, value)) {
            return std::nullopt;
        }
        values.push_back(value);
        if (separator == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(separator + 1);
    }

    return values;
}

std::string JoinEscapedPaths(const std::vector<std::size_t>& values) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << ';';
        }
        stream << values[index];
    }
    return stream.str();
}

std::optional<std::vector<std::string>> SplitEscaped(std::string_view text) {
    std::vector<std::string> values;
    std::string current;
    bool escape = false;

    for (char ch : text) {
        if (escape) {
            switch (ch) {
                case '\\':
                    current.push_back('\\');
                    break;
                case 't':
                    current.push_back('\t');
                    break;
                case 'n':
                    current.push_back('\n');
                    break;
                case ';':
                    current.push_back(';');
                    break;
                default:
                    return std::nullopt;
            }
            escape = false;
            continue;
        }

        if (ch == '\\') {
            escape = true;
            continue;
        }

        if (ch == ';') {
            values.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    if (escape) {
        return std::nullopt;
    }

    if (!text.empty()) {
        values.push_back(current);
    }
    return values;
}

std::optional<std::int64_t> ParseInt64(std::string_view text) {
    std::int64_t value = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc {} || ptr != end) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> ParseFloat64(std::string_view text) {
    std::istringstream stream {std::string(text)};
    double value = 0.0;
    stream >> value;
    if (!stream || !stream.eof()) {
        return std::nullopt;
    }
    return value;
}

std::optional<sema::ConstValue> ParseConstValue(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    if (text.size() < 2 || text[1] != ':') {
        return std::nullopt;
    }
    sema::ConstValue value;
    switch (text[0]) {
        case 'b':
            if (text.substr(2) != "0" && text.substr(2) != "1") {
                return std::nullopt;
            }
            value.kind = sema::ConstValue::Kind::kBool;
            value.bool_value = text.substr(2) == "1";
            value.text = value.bool_value ? "true" : "false";
            return value;
        case 'i': {
            const auto parsed = ParseInt64(text.substr(2));
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            value.kind = sema::ConstValue::Kind::kInteger;
            value.integer_value = *parsed;
            value.text = std::to_string(*parsed);
            return value;
        }
        case 'f': {
            const auto parsed = ParseFloat64(text.substr(2));
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            value.kind = sema::ConstValue::Kind::kFloat;
            value.float_value = *parsed;
            value.text = std::string(text.substr(2));
            return value;
        }
        case 's':
            value.kind = sema::ConstValue::Kind::kString;
            value.text = std::string(text.substr(2));
            return value;
        case 'n':
            if (text.size() != 2) {
                return std::nullopt;
            }
            value.kind = sema::ConstValue::Kind::kNil;
            value.text = "nil";
            return value;
        case 'e': {
            std::string_view payload = text.substr(2);
            const auto type_text = ConsumeLengthPrefixed(payload);
            const auto variant_name = ConsumeLengthPrefixed(payload);
            const auto variant_tag = ParseLengthPrefix(payload);
            const auto field_count = ParseLengthPrefix(payload);
            if (!type_text.has_value() || !variant_name.has_value() || !variant_tag.has_value() || !field_count.has_value()) {
                return std::nullopt;
            }
            const auto enum_type = ParseTypeText(*type_text);
            if (!enum_type.has_value()) {
                return std::nullopt;
            }
            value.kind = sema::ConstValue::Kind::kEnum;
            value.enum_type = *enum_type;
            value.variant_name = std::string(*variant_name);
            value.variant_tag = static_cast<std::int64_t>(*variant_tag);
            value.field_names.reserve(*field_count);
            value.elements.reserve(*field_count);
            for (std::size_t index = 0; index < *field_count; ++index) {
                const auto field_name = ConsumeLengthPrefixed(payload);
                const auto child_text = ConsumeLengthPrefixed(payload);
                if (!field_name.has_value() || !child_text.has_value()) {
                    return std::nullopt;
                }
                const auto child = ParseConstValue(*child_text);
                if (!child.has_value()) {
                    return std::nullopt;
                }
                value.field_names.push_back(std::string(*field_name));
                value.elements.push_back(*child);
            }
            if (!payload.empty()) {
                return std::nullopt;
            }
            value.text = sema::RenderConstValue(value);
            return value;
        }
        case 'a': {
            std::string_view payload = text.substr(2);
            const auto field_count = ParseLengthPrefix(payload);
            if (!field_count.has_value()) {
                return std::nullopt;
            }
            value.kind = sema::ConstValue::Kind::kAggregate;
            value.field_names.reserve(*field_count);
            value.elements.reserve(*field_count);
            for (std::size_t index = 0; index < *field_count; ++index) {
                const auto field_name = ConsumeLengthPrefixed(payload);
                const auto child_text = ConsumeLengthPrefixed(payload);
                if (!field_name.has_value() || !child_text.has_value()) {
                    return std::nullopt;
                }
                const auto child = ParseConstValue(*child_text);
                if (!child.has_value()) {
                    return std::nullopt;
                }
                value.field_names.push_back(std::string(*field_name));
                value.elements.push_back(*child);
            }
            if (!payload.empty()) {
                return std::nullopt;
            }
            value.text = sema::RenderConstValue(value);
            return value;
        }
        default:
            return std::nullopt;
    }
}

std::optional<std::vector<std::optional<sema::ConstValue>>> ParseConstValues(std::string_view text) {
    const auto values = SplitEscaped(text);
    if (!values.has_value()) {
        return std::nullopt;
    }
    std::vector<std::optional<sema::ConstValue>> parsed;
    parsed.reserve(values->size());
    for (const auto& value : *values) {
        if (value.empty()) {
            parsed.push_back(std::nullopt);
            continue;
        }
        const auto decoded = ParseConstValue(value);
        if (!decoded.has_value()) {
            return std::nullopt;
        }
        parsed.push_back(*decoded);
    }
    return parsed;
}

std::optional<sema::ConstValue> ParseLegacyConstValue(const sema::Type& type, std::string_view text) {
    sema::ConstValue value;
    const sema::Type canonical = sema::CanonicalizeBuiltinType(type);
    if (canonical.kind == sema::Type::Kind::kBool) {
        if (text != "true" && text != "false") {
            return std::nullopt;
        }
        value.kind = sema::ConstValue::Kind::kBool;
        value.bool_value = text == "true";
        value.text = std::string(text);
        return value;
    }
    if (canonical.kind == sema::Type::Kind::kString) {
        value.kind = sema::ConstValue::Kind::kString;
        value.text = std::string(text);
        return value;
    }
    if (canonical.kind == sema::Type::Kind::kNil) {
        if (text != "nil") {
            return std::nullopt;
        }
        value.kind = sema::ConstValue::Kind::kNil;
        value.text = "nil";
        return value;
    }
    if (canonical.kind == sema::Type::Kind::kNamed && sema::IsIntegerTypeName(canonical.name)) {
        const auto parsed = ParseInt64(text);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        value.kind = sema::ConstValue::Kind::kInteger;
        value.integer_value = *parsed;
        value.text = std::to_string(*parsed);
        return value;
    }
    if (canonical.kind == sema::Type::Kind::kNamed && sema::IsFloatTypeName(canonical.name)) {
        const auto parsed = ParseFloat64(text);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        value.kind = sema::ConstValue::Kind::kFloat;
        value.float_value = *parsed;
        value.text = std::string(text);
        return value;
    }
    return std::nullopt;
}

std::optional<std::vector<std::optional<sema::ConstValue>>> ParseLegacyConstValues(const sema::Type& type, std::string_view text) {
    const auto values = SplitEscaped(text);
    if (!values.has_value()) {
        return std::nullopt;
    }
    std::vector<std::optional<sema::ConstValue>> parsed;
    parsed.reserve(values->size());
    for (const auto& value : *values) {
        if (value.empty()) {
            parsed.push_back(std::nullopt);
            continue;
        }
        const auto decoded = ParseLegacyConstValue(type, value);
        if (!decoded.has_value()) {
            return std::nullopt;
        }
        parsed.push_back(*decoded);
    }
    return parsed;
}

std::optional<std::vector<bool>> SplitBoolFlags(std::string_view text) {
    const auto values = SplitEscaped(text);
    if (!values.has_value()) {
        return std::nullopt;
    }

    std::vector<bool> flags;
    flags.reserve(values->size());
    for (const auto& value : *values) {
        bool parsed = false;
        if (!ParseBoolField(value, parsed)) {
            return std::nullopt;
        }
        flags.push_back(parsed);
    }
    return flags;
}

std::optional<std::vector<std::size_t>> SplitOffsets(std::string_view text) {
    std::vector<std::size_t> values;
    if (text.empty()) {
        return values;
    }

    std::istringstream input {std::string(text)};
    std::string part;
    while (std::getline(input, part, ';')) {
        const auto parsed = mc::support::ParseArrayLength(part);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        values.push_back(*parsed);
    }
    return values;
}

std::vector<std::string> SplitFields(std::string_view line) {
    std::vector<std::string> fields;
    std::string current;
    for (char ch : line) {
        if (ch == '\t') {
            fields.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    fields.push_back(current);
    return fields;
}

void ReportMciError(const std::filesystem::path& path,
                    std::size_t line,
                    std::string message,
                    support::DiagnosticSink& diagnostics) {
    diagnostics.Report({
        .file_path = path,
        .span = {{line, 1}, {line, 1}},
        .severity = DiagnosticSeverity::kError,
        .message = std::move(message),
    });
}

std::string TypeKindName(mc::ast::Decl::Kind kind) {
    switch (kind) {
        case mc::ast::Decl::Kind::kStruct:
            return "struct";
        case mc::ast::Decl::Kind::kEnum:
            return "enum";
        case mc::ast::Decl::Kind::kDistinct:
            return "distinct";
        case mc::ast::Decl::Kind::kTypeAlias:
            return "alias";
        case mc::ast::Decl::Kind::kFunc:
        case mc::ast::Decl::Kind::kExternFunc:
        case mc::ast::Decl::Kind::kConst:
        case mc::ast::Decl::Kind::kVar:
            break;
    }
    return "struct";
}

std::optional<mc::ast::Decl::Kind> ParseTypeKind(std::string_view text) {
    if (text == "struct") {
        return mc::ast::Decl::Kind::kStruct;
    }
    if (text == "enum") {
        return mc::ast::Decl::Kind::kEnum;
    }
    if (text == "distinct") {
        return mc::ast::Decl::Kind::kDistinct;
    }
    if (text == "alias") {
        return mc::ast::Decl::Kind::kTypeAlias;
    }
    return std::nullopt;
}

std::string ModuleKindName(mc::ast::SourceFile::ModuleKind kind) {
    switch (kind) {
        case mc::ast::SourceFile::ModuleKind::kOrdinary:
            return "ordinary";
        case mc::ast::SourceFile::ModuleKind::kInternal:
            return "internal";
    }
    return "ordinary";
}

std::optional<mc::ast::SourceFile::ModuleKind> ParseModuleKind(std::string_view text) {
    if (text == "ordinary") {
        return mc::ast::SourceFile::ModuleKind::kOrdinary;
    }
    if (text == "internal") {
        return mc::ast::SourceFile::ModuleKind::kInternal;
    }
    return std::nullopt;
}

class TypeParser {
  public:
    explicit TypeParser(std::string_view text) : text_(text) {}

    std::optional<sema::Type> Parse() {
        auto type = ParseType();
        SkipWhitespace();
        if (!type.has_value() || index_ != text_.size()) {
            return std::nullopt;
        }
        return type;
    }

  private:
    void SkipWhitespace() {
        while (index_ < text_.size() && (text_[index_] == ' ' || text_[index_] == '\t')) {
            ++index_;
        }
    }

    bool Consume(std::string_view token) {
        SkipWhitespace();
        if (text_.substr(index_, token.size()) != token) {
            return false;
        }
        index_ += token.size();
        return true;
    }

    std::optional<std::string> ParseIdentifier() {
        SkipWhitespace();
        std::string name;
        while (index_ < text_.size()) {
            const char ch = text_[index_];
            if (!(std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '.')) {
                break;
            }
            name.push_back(ch);
            ++index_;
        }
        if (name.empty()) {
            return std::nullopt;
        }
        return name;
    }

    std::optional<sema::Type> ParseNamedOrBuiltin() {
        const auto name = ParseIdentifier();
        if (!name.has_value()) {
            return std::nullopt;
        }

        if (*name == "unknown") {
            return sema::UnknownType();
        }
        if (*name == "void") {
            return sema::VoidType();
        }
        if (*name == "bool") {
            return sema::BoolType();
        }
        if (*name == "string") {
            return sema::StringType();
        }
        if (*name == "nil") {
            return sema::NilType();
        }
        if (*name == "int_literal") {
            return sema::IntLiteralType();
        }
        if (*name == "float_literal") {
            return sema::FloatLiteralType();
        }

        if (*name == "tuple" && Consume("<")) {
            std::vector<sema::Type> elements;
            if (!Consume(">")) {
                for (;;) {
                    auto element = ParseType();
                    if (!element.has_value()) {
                        return std::nullopt;
                    }
                    elements.push_back(std::move(*element));
                    if (Consume(">")) {
                        break;
                    }
                    if (!Consume(",")) {
                        return std::nullopt;
                    }
                }
            }
            return sema::TupleType(std::move(elements));
        }

        if (*name == "range" && Consume("<")) {
            auto element = ParseType();
            if (!element.has_value() || !Consume(">")) {
                return std::nullopt;
            }
            return sema::RangeType(std::move(*element));
        }

        sema::Type type = sema::NamedType(*name);
        if (!Consume("<")) {
            return type;
        }

        for (;;) {
            auto arg = ParseType();
            if (!arg.has_value()) {
                return std::nullopt;
            }
            type.subtypes.push_back(std::move(*arg));
            if (Consume(">")) {
                break;
            }
            if (!Consume(",")) {
                return std::nullopt;
            }
        }
        return type;
    }

    std::optional<sema::Type> ParseProcedure() {
        if (!Consume("proc")) {
            return std::nullopt;
        }
        if (!Consume("(")) {
            return std::nullopt;
        }

        std::vector<sema::Type> params;
        if (!Consume(")")) {
            for (;;) {
                auto param = ParseType();
                if (!param.has_value()) {
                    return std::nullopt;
                }
                params.push_back(std::move(*param));
                if (Consume(")")) {
                    break;
                }
                if (!Consume(",")) {
                    return std::nullopt;
                }
            }
        }

        if (!Consume("->")) {
            return std::nullopt;
        }

        auto return_type = ParseType();
        if (!return_type.has_value()) {
            return std::nullopt;
        }

        std::vector<sema::Type> returns;
        if (return_type->kind == sema::Type::Kind::kVoid) {
            returns = {};
        } else if (return_type->kind == sema::Type::Kind::kTuple) {
            returns = std::move(return_type->subtypes);
        } else {
            returns.push_back(std::move(*return_type));
        }
        return sema::ProcedureType(std::move(params), std::move(returns));
    }

    std::optional<sema::Type> ParseType() {
        SkipWhitespace();
        if (Consume("*")) {
            auto inner = ParseType();
            if (!inner.has_value()) {
                return std::nullopt;
            }
            return sema::PointerType(std::move(*inner));
        }
        if (Consume("const")) {
            auto inner = ParseType();
            if (!inner.has_value()) {
                return std::nullopt;
            }
            return sema::ConstType(std::move(*inner));
        }
        if (Consume("[")) {
            const std::size_t length_begin = index_;
            while (index_ < text_.size() && text_[index_] != ']') {
                ++index_;
            }
            if (index_ >= text_.size()) {
                return std::nullopt;
            }
            const std::string length(text_.substr(length_begin, index_ - length_begin));
            ++index_;
            auto inner = ParseType();
            if (!inner.has_value()) {
                return std::nullopt;
            }
            return sema::ArrayType(std::move(*inner), length);
        }
        if (text_.substr(index_, 4) == "proc") {
            return ParseProcedure();
        }
        return ParseNamedOrBuiltin();
    }

    std::string_view text_;
    std::size_t index_ = 0;
};

std::optional<sema::Type> ParseTypeText(std::string_view text) {
    return TypeParser(text).Parse();
}

std::string SerializeArtifactWithoutHashV1(const InterfaceArtifact& artifact) {
    std::ostringstream output;
    output << "format\t" << artifact.format_version << '\n';
    output << "target\t" << EscapeText(artifact.target_identity) << '\n';
    output << "module\t" << EscapeText(artifact.module_name) << '\n';
    output << "source\t" << EscapeText(artifact.source_path.generic_string()) << '\n';
    output << "imports\t" << JoinEscaped(artifact.module.imports) << '\n';

    for (const auto& function : artifact.module.functions) {
        std::vector<std::string> param_types;
        param_types.reserve(function.param_types.size());
        for (const auto& type : function.param_types) {
            param_types.push_back(sema::FormatType(type));
        }

        std::vector<std::string> return_types;
        return_types.reserve(function.return_types.size());
        for (const auto& type : function.return_types) {
            return_types.push_back(sema::FormatType(type));
        }

        output << "function\t" << EscapeText(function.name)
               << '\t' << (function.is_extern ? "1" : "0")
               << '\t' << EscapeText(function.extern_abi)
               << '\t' << JoinEscaped(function.type_params)
               << '\t' << JoinEscaped(param_types)
             << '\t' << JoinEscaped(return_types)
               << '\n';
    }

    for (const auto& type_decl : artifact.module.type_decls) {
        output << "type\t" << TypeKindName(type_decl.kind)
               << '\t' << EscapeText(type_decl.name)
               << '\t' << JoinEscaped(type_decl.type_params)
               << '\t' << (type_decl.is_packed ? "1" : "0")
               << '\t' << (type_decl.is_abi_c ? "1" : "0")
               << '\t' << (type_decl.layout.valid ? "1" : "0")
               << '\t' << type_decl.layout.size
               << '\t' << type_decl.layout.alignment
               << '\t' << JoinEscapedPaths(type_decl.layout.field_offsets)
               << '\t' << EscapeText(sema::FormatType(type_decl.aliased_type))
               << '\n';

        for (const auto& field : type_decl.fields) {
            output << "type_field\t" << EscapeText(type_decl.name)
                   << '\t' << EscapeText(field.first)
                   << '\t' << EscapeText(sema::FormatType(field.second))
                   << '\n';
        }

        for (const auto& variant : type_decl.variants) {
            output << "type_variant\t" << EscapeText(type_decl.name)
                   << '\t' << EscapeText(variant.name)
                   << '\n';

            for (const auto& payload_field : variant.payload_fields) {
                output << "type_variant_field\t" << EscapeText(type_decl.name)
                       << '\t' << EscapeText(variant.name)
                       << '\t' << EscapeText(payload_field.first)
                       << '\t' << EscapeText(sema::FormatType(payload_field.second))
                       << '\n';
            }
        }
    }

    for (const auto& global : artifact.module.globals) {
        output << "global\t" << (global.is_const ? "1" : "0")
               << '\t' << JoinEscaped(global.names)
               << '\t' << EscapeText(sema::FormatType(global.type))
               << '\n';
    }

    return output.str();
}

std::string SerializeArtifactWithoutHash(const InterfaceArtifact& artifact) {
    std::ostringstream output;
    output << "format\t" << artifact.format_version << '\n';
    output << "target\t" << EscapeText(artifact.target_identity) << '\n';
    output << "package\t" << EscapeText(artifact.package_identity) << '\n';
    output << "module\t" << EscapeText(artifact.module_name) << '\n';
    output << "module_kind\t" << ModuleKindName(artifact.module_kind) << '\n';
    output << "source\t" << EscapeText(artifact.source_path.generic_string()) << '\n';
    output << "imports\t" << JoinEscaped(artifact.module.imports) << '\n';

    for (const auto& function : artifact.module.functions) {
        std::vector<std::string> param_types;
        param_types.reserve(function.param_types.size());
        for (const auto& type : function.param_types) {
            param_types.push_back(sema::FormatType(type));
        }

        std::vector<std::string> return_types;
        return_types.reserve(function.return_types.size());
        for (const auto& type : function.return_types) {
            return_types.push_back(sema::FormatType(type));
        }

        output << "function\t" << EscapeText(function.name)
               << '\t' << (function.is_extern ? "1" : "0")
               << '\t' << EscapeText(function.extern_abi)
               << '\t' << JoinEscaped(function.type_params)
               << '\t' << JoinEscaped(param_types)
               << '\t' << JoinEscaped(return_types)
               << '\t' << JoinBoolFlags(function.param_is_noalias)
               << '\n';
    }

    for (const auto& type_decl : artifact.module.type_decls) {
        output << "type\t" << TypeKindName(type_decl.kind)
               << '\t' << EscapeText(type_decl.name)
               << '\t' << JoinEscaped(type_decl.type_params)
               << '\t' << (type_decl.is_packed ? "1" : "0")
               << '\t' << (type_decl.is_abi_c ? "1" : "0")
               << '\t' << (type_decl.layout.valid ? "1" : "0")
               << '\t' << type_decl.layout.size
               << '\t' << type_decl.layout.alignment
               << '\t' << JoinEscapedPaths(type_decl.layout.field_offsets)
               << '\t' << EscapeText(sema::FormatType(type_decl.aliased_type))
               << '\n';

        for (const auto& field : type_decl.fields) {
            output << "type_field\t" << EscapeText(type_decl.name)
                   << '\t' << EscapeText(field.first)
                   << '\t' << EscapeText(sema::FormatType(field.second))
                   << '\n';
        }

        for (const auto& variant : type_decl.variants) {
            output << "type_variant\t" << EscapeText(type_decl.name)
                   << '\t' << EscapeText(variant.name)
                   << '\n';

            for (const auto& payload_field : variant.payload_fields) {
                output << "type_variant_field\t" << EscapeText(type_decl.name)
                       << '\t' << EscapeText(variant.name)
                       << '\t' << EscapeText(payload_field.first)
                       << '\t' << EscapeText(sema::FormatType(payload_field.second))
                       << '\n';
            }
        }
    }

    for (const auto& global : artifact.module.globals) {
        output << "global\t" << (global.is_const ? "1" : "0")
               << '\t' << JoinEscaped(global.names)
               << '\t' << EscapeText(sema::FormatType(global.type))
               << '\t' << JoinConstValues(global.constant_values)
               << '\t' << JoinBoolFlags(global.zero_initialized_values)
               << '\n';
    }

    return output.str();
}

std::string HexU64(std::uint64_t value) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(16, '0');
    for (int index = 15; index >= 0; --index) {
        text[static_cast<std::size_t>(index)] = kDigits[value & 0xfu];
        value >>= 4u;
    }
    return text;
}

bool ParseBoolField(std::string_view text, bool& value) {
    if (text == "0") {
        value = false;
        return true;
    }
    if (text == "1") {
        value = true;
        return true;
    }
    return false;
}

}  // namespace

std::string ComputeInterfaceHash(const InterfaceArtifact& artifact) {
    const std::string contents = SerializeArtifactWithoutHash(artifact);
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char byte : contents) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ull;
    }
    return HexU64(hash);
}

bool WriteInterfaceArtifact(const std::filesystem::path& path,
                            const InterfaceArtifact& artifact,
                            support::DiagnosticSink& diagnostics) {
    InterfaceArtifact materialized = artifact;
    materialized.format_version = kFormatVersion;
    materialized.interface_hash = ComputeInterfaceHash(materialized);

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        diagnostics.Report({
            .file_path = path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "unable to write interface artifact",
        });
        return false;
    }

    output << SerializeArtifactWithoutHash(materialized);
    output << "interface_hash\t" << materialized.interface_hash << '\n';
    return static_cast<bool>(output);
}

std::optional<InterfaceArtifact> LoadInterfaceArtifact(const std::filesystem::path& path,
                                                       support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        diagnostics.Report({
            .file_path = path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "unable to read interface artifact",
        });
        return std::nullopt;
    }

    InterfaceArtifact artifact;
    artifact.format_version = kFormatVersion;
    bool saw_package_identity = false;
    bool saw_module_kind = false;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }

        const auto fields = SplitFields(line);
        if (fields.empty()) {
            continue;
        }

        if (fields[0] == "format") {
            if (fields.size() != 2 || !mc::support::ParseArrayLength(fields[1]).has_value()) {
                ReportMciError(path, line_number, "invalid format field in interface artifact", diagnostics);
                return std::nullopt;
            }
            artifact.format_version = static_cast<int>(*mc::support::ParseArrayLength(fields[1]));
            continue;
        }
        if (fields[0] == "target") {
            if (fields.size() != 2) {
                ReportMciError(path, line_number, "invalid target field in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto value = UnescapeText(fields[1]);
            if (!value.has_value()) {
                ReportMciError(path, line_number, "invalid escaped target field in interface artifact", diagnostics);
                return std::nullopt;
            }
            artifact.target_identity = *value;
            continue;
        }
        if (fields[0] == "package") {
            if (fields.size() != 2) {
                ReportMciError(path, line_number, "invalid package field in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto value = UnescapeText(fields[1]);
            if (!value.has_value()) {
                ReportMciError(path, line_number, "invalid escaped package field in interface artifact", diagnostics);
                return std::nullopt;
            }
            artifact.package_identity = *value;
            saw_package_identity = true;
            continue;
        }
        if (fields[0] == "module") {
            if (fields.size() != 2) {
                ReportMciError(path, line_number, "invalid module field in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto value = UnescapeText(fields[1]);
            if (!value.has_value()) {
                ReportMciError(path, line_number, "invalid escaped module field in interface artifact", diagnostics);
                return std::nullopt;
            }
            artifact.module_name = *value;
            continue;
        }
        if (fields[0] == "module_kind") {
            if (fields.size() != 2) {
                ReportMciError(path, line_number, "invalid module_kind field in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto parsed = ParseModuleKind(fields[1]);
            if (!parsed.has_value()) {
                ReportMciError(path, line_number, "invalid module_kind value in interface artifact", diagnostics);
                return std::nullopt;
            }
            artifact.module_kind = *parsed;
            saw_module_kind = true;
            continue;
        }
        if (fields[0] == "source") {
            if (fields.size() != 2) {
                ReportMciError(path, line_number, "invalid source field in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto value = UnescapeText(fields[1]);
            if (!value.has_value()) {
                ReportMciError(path, line_number, "invalid escaped source field in interface artifact", diagnostics);
                return std::nullopt;
            }
            artifact.source_path = *value;
            continue;
        }
        if (fields[0] == "imports") {
            if (fields.size() != 2) {
                ReportMciError(path, line_number, "invalid imports field in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto values = SplitEscaped(fields[1]);
            if (!values.has_value()) {
                ReportMciError(path, line_number, "invalid imports list in interface artifact", diagnostics);
                return std::nullopt;
            }
            artifact.module.imports = std::move(*values);
            continue;
        }
        if (fields[0] == "function") {
            if (fields.size() != 7 && fields.size() != 8) {
                ReportMciError(path, line_number, "invalid function record in interface artifact", diagnostics);
                return std::nullopt;
            }

            sema::FunctionSignature function;
            const auto name = UnescapeText(fields[1]);
            const auto abi = UnescapeText(fields[3]);
            const auto type_params = SplitEscaped(fields[4]);
            const auto params = SplitEscaped(fields[5]);
            const auto returns = SplitEscaped(fields[6]);
            const auto noalias_flags = fields.size() == 8 ? SplitBoolFlags(fields[7]) : std::optional<std::vector<bool>> {std::vector<bool> {}};
            if (!name.has_value() || !abi.has_value() || !type_params.has_value() || !params.has_value() || !returns.has_value() ||
                !noalias_flags.has_value() || !ParseBoolField(fields[2], function.is_extern)) {
                ReportMciError(path, line_number, "invalid function payload in interface artifact", diagnostics);
                return std::nullopt;
            }
            function.name = *name;
            function.extern_abi = *abi;
            function.type_params = *type_params;
            for (const auto& text : *params) {
                const auto type = ParseTypeText(text);
                if (!type.has_value()) {
                    ReportMciError(path, line_number, "invalid function parameter type in interface artifact", diagnostics);
                    return std::nullopt;
                }
                function.param_types.push_back(*type);
            }
            for (const auto& text : *returns) {
                const auto type = ParseTypeText(text);
                if (!type.has_value()) {
                    ReportMciError(path, line_number, "invalid function return type in interface artifact", diagnostics);
                    return std::nullopt;
                }
                function.return_types.push_back(*type);
            }
            if (!noalias_flags->empty() && noalias_flags->size() != function.param_types.size()) {
                ReportMciError(path, line_number, "function noalias flag count does not match parameter count", diagnostics);
                return std::nullopt;
            }
            if (noalias_flags->empty()) {
                function.param_is_noalias.assign(function.param_types.size(), false);
            } else {
                function.param_is_noalias = *noalias_flags;
            }
            artifact.module.functions.push_back(std::move(function));
            continue;
        }
        if (fields[0] == "type") {
            if (fields.size() != 11) {
                ReportMciError(path, line_number, "invalid type record in interface artifact", diagnostics);
                return std::nullopt;
            }

            sema::TypeDeclSummary type_decl;
            const auto kind = ParseTypeKind(fields[1]);
            const auto name = UnescapeText(fields[2]);
            const auto type_params = SplitEscaped(fields[3]);
            const auto offsets = SplitOffsets(fields[9]);
            const auto aliased_type_text = UnescapeText(fields[10]);
            if (!kind.has_value() || !name.has_value() || !type_params.has_value() || !offsets.has_value() || !aliased_type_text.has_value() ||
                !ParseBoolField(fields[4], type_decl.is_packed) || !ParseBoolField(fields[5], type_decl.is_abi_c) ||
                !ParseBoolField(fields[6], type_decl.layout.valid)) {
                ReportMciError(path, line_number, "invalid type payload in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto size = mc::support::ParseArrayLength(fields[7]);
            const auto alignment = mc::support::ParseArrayLength(fields[8]);
            const auto aliased_type = ParseTypeText(*aliased_type_text);
            if (!size.has_value() || !alignment.has_value() || !aliased_type.has_value()) {
                ReportMciError(path, line_number, "invalid type layout or alias in interface artifact", diagnostics);
                return std::nullopt;
            }

            type_decl.kind = *kind;
            type_decl.name = *name;
            type_decl.type_params = *type_params;
            type_decl.layout.size = *size;
            type_decl.layout.alignment = *alignment;
            type_decl.layout.field_offsets = *offsets;
            type_decl.aliased_type = *aliased_type;
            artifact.module.type_decls.push_back(std::move(type_decl));
            continue;
        }
        if (fields[0] == "type_field") {
            if (fields.size() != 4) {
                ReportMciError(path, line_number, "invalid type_field record in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto type_name = UnescapeText(fields[1]);
            const auto field_name = UnescapeText(fields[2]);
            const auto field_type_text = UnescapeText(fields[3]);
            const auto field_type = field_type_text.has_value() ? ParseTypeText(*field_type_text) : std::nullopt;
            if (!type_name.has_value() || !field_name.has_value() || !field_type.has_value()) {
                ReportMciError(path, line_number, "invalid type field payload in interface artifact", diagnostics);
                return std::nullopt;
            }
            auto it = std::find_if(artifact.module.type_decls.begin(), artifact.module.type_decls.end(), [&](const sema::TypeDeclSummary& decl) {
                return decl.name == *type_name;
            });
            if (it == artifact.module.type_decls.end()) {
                ReportMciError(path, line_number, "type_field references unknown type in interface artifact", diagnostics);
                return std::nullopt;
            }
            it->fields.push_back({*field_name, *field_type});
            continue;
        }
        if (fields[0] == "type_variant") {
            if (fields.size() != 3) {
                ReportMciError(path, line_number, "invalid type_variant record in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto type_name = UnescapeText(fields[1]);
            const auto variant_name = UnescapeText(fields[2]);
            if (!type_name.has_value() || !variant_name.has_value()) {
                ReportMciError(path, line_number, "invalid type variant payload in interface artifact", diagnostics);
                return std::nullopt;
            }
            auto it = std::find_if(artifact.module.type_decls.begin(), artifact.module.type_decls.end(), [&](const sema::TypeDeclSummary& decl) {
                return decl.name == *type_name;
            });
            if (it == artifact.module.type_decls.end()) {
                ReportMciError(path, line_number, "type_variant references unknown type in interface artifact", diagnostics);
                return std::nullopt;
            }
            it->variants.push_back({.name = *variant_name});
            continue;
        }
        if (fields[0] == "type_variant_field") {
            if (fields.size() != 5) {
                ReportMciError(path, line_number, "invalid type_variant_field record in interface artifact", diagnostics);
                return std::nullopt;
            }
            const auto type_name = UnescapeText(fields[1]);
            const auto variant_name = UnescapeText(fields[2]);
            const auto field_name = UnescapeText(fields[3]);
            const auto field_type_text = UnescapeText(fields[4]);
            const auto field_type = field_type_text.has_value() ? ParseTypeText(*field_type_text) : std::nullopt;
            if (!type_name.has_value() || !variant_name.has_value() || !field_name.has_value() || !field_type.has_value()) {
                ReportMciError(path, line_number, "invalid type variant field payload in interface artifact", diagnostics);
                return std::nullopt;
            }
            auto type_it = std::find_if(artifact.module.type_decls.begin(), artifact.module.type_decls.end(), [&](const sema::TypeDeclSummary& decl) {
                return decl.name == *type_name;
            });
            if (type_it == artifact.module.type_decls.end()) {
                ReportMciError(path, line_number, "type_variant_field references unknown type in interface artifact", diagnostics);
                return std::nullopt;
            }
            auto variant_it = std::find_if(type_it->variants.begin(), type_it->variants.end(), [&](const sema::VariantSummary& variant) {
                return variant.name == *variant_name;
            });
            if (variant_it == type_it->variants.end()) {
                ReportMciError(path, line_number, "type_variant_field references unknown variant in interface artifact", diagnostics);
                return std::nullopt;
            }
            variant_it->payload_fields.push_back({*field_name, *field_type});
            continue;
        }
        if (fields[0] == "global") {
            if ((artifact.format_version >= 4 && fields.size() != 6) ||
                (artifact.format_version < 4 && fields.size() != 4 && fields.size() != 5)) {
                ReportMciError(path, line_number, "invalid global record in interface artifact", diagnostics);
                return std::nullopt;
            }
            sema::GlobalSummary global;
            const auto names = SplitEscaped(fields[2]);
            const auto type_text = UnescapeText(fields[3]);
            const auto type = type_text.has_value() ? ParseTypeText(*type_text) : std::nullopt;
            const auto constant_values = fields.size() == 6
                                             ? ParseConstValues(fields[4])
                                             : (fields.size() == 5
                                                    ? (artifact.format_version == 2 ? ParseLegacyConstValues(*type, fields[4])
                                                                                    : ParseConstValues(fields[4]))
                                                    : std::optional<std::vector<std::optional<sema::ConstValue>>> {
                                                          std::vector<std::optional<sema::ConstValue>> {}
                                                      });
            const auto zero_initialized_values = fields.size() == 6 ? ParseBoolFlags(fields[5])
                                                                    : std::optional<std::vector<bool>> {std::vector<bool> {}};
            if (!names.has_value() || !type.has_value() || !constant_values.has_value() || !zero_initialized_values.has_value() ||
                !ParseBoolField(fields[1], global.is_const)) {
                ReportMciError(path, line_number, "invalid global payload in interface artifact", diagnostics);
                return std::nullopt;
            }
            global.names = *names;
            global.type = *type;
            if (!constant_values->empty() && constant_values->size() != global.names.size()) {
                ReportMciError(path, line_number, "global constant value count does not match bound name count", diagnostics);
                return std::nullopt;
            }
            if (!zero_initialized_values->empty() && zero_initialized_values->size() != global.names.size()) {
                ReportMciError(path, line_number, "global zero-init flag count does not match bound name count", diagnostics);
                return std::nullopt;
            }
            global.constant_values = *constant_values;
            global.zero_initialized_values = *zero_initialized_values;
            artifact.module.globals.push_back(std::move(global));
            continue;
        }
        if (fields[0] == "interface_hash") {
            if (fields.size() != 2) {
                ReportMciError(path, line_number, "invalid interface_hash record in interface artifact", diagnostics);
                return std::nullopt;
            }
            artifact.interface_hash = fields[1];
            continue;
        }

        ReportMciError(path, line_number, "unknown record in interface artifact: " + fields[0], diagnostics);
        return std::nullopt;
    }

    if (artifact.format_version != kFormatVersion) {
        diagnostics.Report({
            .file_path = path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "unsupported interface artifact format version",
        });
        return std::nullopt;
    }

    if (!saw_package_identity || artifact.package_identity.empty()) {
        diagnostics.Report({
            .file_path = path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "interface artifact is missing package identity",
        });
        return std::nullopt;
    }

    if (!saw_module_kind) {
        diagnostics.Report({
            .file_path = path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "interface artifact is missing module kind",
        });
        return std::nullopt;
    }

    if (artifact.interface_hash.empty()) {
        diagnostics.Report({
            .file_path = path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "interface artifact is missing interface_hash",
        });
        return std::nullopt;
    }

    const std::string computed_hash = ComputeInterfaceHash(artifact);
    if (artifact.interface_hash != computed_hash) {
        diagnostics.Report({
            .file_path = path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "interface artifact hash mismatch",
        });
        return std::nullopt;
    }

    return artifact;
}

bool ValidateInterfaceArtifactMetadata(const InterfaceArtifact& artifact,
                                       const std::filesystem::path& artifact_path,
                                       std::string_view expected_target_identity,
                                       std::string_view expected_package_identity,
                                       std::string_view expected_module_name,
                                       ast::SourceFile::ModuleKind expected_module_kind,
                                       const std::filesystem::path& expected_source_path,
                                       support::DiagnosticSink& diagnostics) {
    auto report_error = [&](const std::string& message) {
        diagnostics.Report({
            .file_path = artifact_path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = message,
        });
        return false;
    };

    if (artifact.target_identity.empty()) {
        return report_error("interface artifact is missing target identity");
    }
    if (artifact.module_name.empty()) {
        return report_error("interface artifact is missing module name");
    }
    if (artifact.package_identity.empty()) {
        return report_error("interface artifact is missing package identity");
    }
    if (artifact.source_path.empty()) {
        return report_error("interface artifact is missing source path");
    }
    if (!expected_target_identity.empty() && artifact.target_identity != expected_target_identity) {
        return report_error("interface artifact target mismatch");
    }
    if (!expected_package_identity.empty() && artifact.package_identity != expected_package_identity) {
        return report_error("interface artifact package identity mismatch");
    }
    if (!expected_module_name.empty() && artifact.module_name != expected_module_name) {
        return report_error("interface artifact module mismatch");
    }
    if (artifact.module_kind != expected_module_kind) {
        return report_error("interface artifact module kind mismatch");
    }
    if (!expected_source_path.empty()) {
        const auto actual_source = artifact.source_path.lexically_normal();
        const auto expected_source = expected_source_path.lexically_normal();
        if (actual_source != expected_source) {
            return report_error("interface artifact source path mismatch");
        }
    }

    return true;
}

}  // namespace mc::mci