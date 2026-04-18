#include "compiler/codegen_llvm/backend_internal.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

#include "compiler/sema/type_utils.h"

namespace mc::codegen_llvm {
namespace {

std::vector<const mir::Local*> ParameterLocals(const mir::Function& function) {
    std::vector<const mir::Local*> params;
    params.reserve(function.locals.size());
    for (const auto& local : function.locals) {
        if (local.is_parameter) {
            params.push_back(&local);
        }
    }
    return params;
}

}  // namespace

const mir::Function* FindFunction(const mir::Module& module,
                                  std::string_view name) {
    for (const auto& function : module.functions) {
        if (function.name == name) {
            return &function;
        }
    }
    return nullptr;
}

std::string RenderLLVMParameter(const BackendTypeInfo& type_info,
                                const mir::Local& param,
                                bool include_name) {
    std::ostringstream stream;
    stream << type_info.backend_name;
    if (param.is_noalias) {
        stream << " noalias";
    }
    if (include_name) {
        stream << " " << LLVMParamName(param.name);
    }
    return stream.str();
}

std::string DecodeStringLiteral(std::string_view literal) {
    if (literal.size() >= 2 && literal.front() == '"' && literal.back() == '"') {
        literal.remove_prefix(1);
        literal.remove_suffix(1);
    }

    std::string decoded;
    decoded.reserve(literal.size());
    for (std::size_t index = 0; index < literal.size(); ++index) {
        const char ch = literal[index];
        if (ch != '\\' || index + 1 >= literal.size()) {
            decoded.push_back(ch);
            continue;
        }

        const char escaped = literal[++index];
        switch (escaped) {
            case 'n':
                decoded.push_back('\n');
                break;
            case 'r':
                decoded.push_back('\r');
                break;
            case 't':
                decoded.push_back('\t');
                break;
            case '\\':
                decoded.push_back('\\');
                break;
            case '"':
                decoded.push_back('"');
                break;
            case '0':
                decoded.push_back('\0');
                break;
            default:
                decoded.push_back(escaped);
                break;
        }
    }
    return decoded;
}

std::string EncodeLLVMStringBytes(std::string_view decoded) {
    std::ostringstream encoded;
    for (const unsigned char ch : decoded) {
        if (ch >= 32 && ch <= 126 && ch != '\\' && ch != '"') {
            encoded << static_cast<char>(ch);
            continue;
        }

        encoded << '\\';
        constexpr char kHex[] = "0123456789ABCDEF";
        encoded << kHex[(ch >> 4) & 0xF] << kHex[ch & 0xF];
    }
    encoded << "\\00";
    return encoded.str();
}

std::string EscapeLLVMQuotedString(std::string_view text) {
    std::ostringstream encoded;
    for (const unsigned char ch : text) {
        if (ch >= 32 && ch <= 126 && ch != '\\' && ch != '"') {
            encoded << static_cast<char>(ch);
            continue;
        }

        encoded << '\\';
        constexpr char kHex[] = "0123456789ABCDEF";
        encoded << kHex[(ch >> 4) & 0xF] << kHex[ch & 0xF];
    }
    return encoded.str();
}

std::string FormatLLVMLiteral(const BackendTypeInfo& type_info,
                              std::string_view value) {
    if (type_info.backend_name == "i1") {
        if (value == "true" || value == "1") {
            return "true";
        }
        return "false";
    }

    if (type_info.backend_name == "ptr" && (value == "nil" || value == "null")) {
        return "null";
    }

    return std::string(value);
}

namespace {

std::string LLVMZeroValue(const BackendTypeInfo& type_info) {
    if (type_info.backend_name == "float" || type_info.backend_name == "double") {
        return "0.0";
    }
    if (type_info.backend_name == "ptr") {
        return "null";
    }
    if (type_info.backend_name == "i1") {
        return "false";
    }
    if (!type_info.backend_name.empty() && (type_info.backend_name.front() == '{' || type_info.backend_name.front() == '[')) {
        return "zeroinitializer";
    }
    return "0";
}

bool RenderLLVMEnumConstValue(const mir::Module& module,
                              const sema::Type& enum_type,
                              const sema::ConstValue& value,
                              const std::filesystem::path& source_path,
                              support::DiagnosticSink& diagnostics,
                              std::string& rendered) {
    if (value.kind != sema::ConstValue::Kind::kEnum || enum_type.kind != sema::Type::Kind::kNamed) {
        return false;
    }

    sema::Type rendered_enum_type = value.enum_type;
    if (rendered_enum_type != enum_type && rendered_enum_type.kind == sema::Type::Kind::kNamed &&
        rendered_enum_type.subtypes == enum_type.subtypes && mir::VariantLeafName(rendered_enum_type.name) == mir::VariantLeafName(enum_type.name)) {
        rendered_enum_type.name = enum_type.name;
    }

    if (rendered_enum_type != enum_type) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend received mismatched enum constant type " +
                               sema::FormatType(value.enum_type) + " for " + sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }

    const auto* type_decl = FindTypeDecl(module, enum_type.name);
    if (type_decl == nullptr || type_decl->kind != mir::TypeDecl::Kind::kEnum) {
        return false;
    }

    if (value.variant_tag < 0 ||
        value.variant_tag >= static_cast<std::int64_t>(type_decl->variants.size())) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend received enum constant tag out of range for type " +
                               sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }

    const auto variant_index = static_cast<std::size_t>(value.variant_tag);
    const auto& variant = type_decl->variants[variant_index];
    if (variant.name != value.variant_name) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend received enum constant variant mismatch for type " +
                               sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }
    if (!variant.payload_fields.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend does not yet support payload enum global constants for type " +
                               sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }

    const auto lowered_layout = LowerEnumLayout(module, *type_decl, enum_type);
    if (!lowered_layout.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend cannot lower enum global constant type " +
                               sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }

    std::ostringstream stream;
    stream << "{ i64 " << value.variant_tag;
    if (lowered_layout->payload_size != 0) {
        stream << ", " << lowered_layout->payload_type.backend_name << " zeroinitializer";
    }
    stream << " }";
    rendered = stream.str();
    return true;
}

}  // namespace

std::string LLVMStructInsertBase(const BackendTypeInfo& type_info) {
    return IsAggregateType(type_info) ? "zeroinitializer" : LLVMZeroValue(type_info);
}

bool RenderLLVMGlobalConstValue(const mir::Module& module,
                                sema::Type type,
                                const sema::ConstValue& value,
                                bool wrap_hosted_main,
                                const std::filesystem::path& source_path,
                                support::DiagnosticSink& diagnostics,
                                std::string& rendered) {
    type = sema::CanonicalizeBuiltinType(std::move(type));
    if (type.kind == sema::Type::Kind::kConst && !type.subtypes.empty()) {
        return RenderLLVMGlobalConstValue(module, type.subtypes.front(), value, wrap_hosted_main, source_path, diagnostics, rendered);
    }

    if (value.kind == sema::ConstValue::Kind::kProcedure) {
        const sema::Type canonical_type = mir::StripMirAliasOrDistinct(module, std::move(type));
        if (canonical_type.kind != sema::Type::Kind::kProcedure) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend requires procedure-typed global constant for named procedure reference",
                               diagnostics);
            return false;
        }
        rendered = LLVMFunctionSymbol(value.procedure_name, wrap_hosted_main);
        return true;
    }

    const sema::Type canonical_type = mir::StripMirAliasOrDistinct(module, std::move(type));
    const auto lowered_type = LowerTypeInfo(module, canonical_type);
    if (!lowered_type.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend cannot lower global constant type " + sema::FormatType(canonical_type),
                           diagnostics);
        return false;
    }

    if (value.kind != sema::ConstValue::Kind::kAggregate) {
        if (RenderLLVMEnumConstValue(module, canonical_type, value, source_path, diagnostics, rendered)) {
            return true;
        }
        rendered = FormatLLVMLiteral(*lowered_type, value.text);
        return true;
    }

    if (canonical_type.kind == sema::Type::Kind::kArray) {
        if (canonical_type.subtypes.empty()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend requires array element type for global constant " +
                                   sema::FormatType(canonical_type),
                               diagnostics);
            return false;
        }

        const auto length = mc::support::ParseArrayLength(canonical_type.metadata);
        if (!length.has_value() || *length != value.elements.size()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend requires exact array constant element count for global type " +
                                   sema::FormatType(canonical_type),
                               diagnostics);
            return false;
        }

        const auto element_type = LowerTypeInfo(module, canonical_type.subtypes.front());
        if (!element_type.has_value()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend cannot lower array element type for global constant " +
                                   sema::FormatType(canonical_type.subtypes.front()),
                               diagnostics);
            return false;
        }

        std::ostringstream stream;
        stream << '[';
        for (std::size_t index = 0; index < value.elements.size(); ++index) {
            std::string element_text;
            if (!RenderLLVMGlobalConstValue(module,
                                            canonical_type.subtypes.front(),
                                            value.elements[index],
                                            wrap_hosted_main,
                                            source_path,
                                            diagnostics,
                                            element_text)) {
                return false;
            }
            if (index > 0) {
                stream << ", ";
            }
            stream << element_type->backend_name << ' ' << element_text;
        }
        stream << ']';
        rendered = stream.str();
        return true;
    }

    std::vector<std::pair<std::string, sema::Type>> fields = sema::BuiltinAggregateFields(canonical_type);
    if (fields.empty() && canonical_type.kind == sema::Type::Kind::kTuple) {
        fields.reserve(canonical_type.subtypes.size());
        for (std::size_t index = 0; index < canonical_type.subtypes.size(); ++index) {
            fields.push_back({std::to_string(index), canonical_type.subtypes[index]});
        }
    }
    if (fields.empty() && canonical_type.kind == sema::Type::Kind::kNamed) {
        if (const auto* type_decl = FindTypeDecl(module, canonical_type.name); type_decl != nullptr) {
            fields = InstantiateFields(*type_decl, canonical_type);
        }
    }
    if (fields.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend cannot render structural global constant for type " +
                               sema::FormatType(canonical_type),
                           diagnostics);
        return false;
    }
    if (fields.size() != value.elements.size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend requires one constant element per field for global type " +
                               sema::FormatType(canonical_type),
                           diagnostics);
        return false;
    }

    std::ostringstream stream;
    stream << '{';
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto field_type = LowerTypeInfo(module, fields[index].second);
        if (!field_type.has_value()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend cannot lower field type " + sema::FormatType(fields[index].second) +
                                   " for global constant " + sema::FormatType(canonical_type),
                               diagnostics);
            return false;
        }

        std::string field_text;
        if (!RenderLLVMGlobalConstValue(module,
                                        fields[index].second,
                                        value.elements[index],
                                        wrap_hosted_main,
                                        source_path,
                                        diagnostics,
                                        field_text)) {
            return false;
        }

        if (index > 0) {
            stream << ", ";
        }
        stream << field_type->backend_name << ' ' << field_text;
    }
    stream << '}';
    rendered = stream.str();
    return true;
}

bool RenderLLVMGlobalStringConstValue(const mir::Module& module,
                                      sema::Type type,
                                      const sema::ConstValue& value,
                                      std::string_view backing_global_name,
                                      const std::filesystem::path& source_path,
                                      support::DiagnosticSink& diagnostics,
                                      std::ostringstream& prelude,
                                      std::string& rendered) {
    const sema::Type canonical_type = sema::CanonicalizeBuiltinType(mir::StripMirAliasOrDistinct(module, std::move(type)));
    if (canonical_type.kind != sema::Type::Kind::kString || value.kind != sema::ConstValue::Kind::kString) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend requires string-typed global constant for dedicated string lowering",
                           diagnostics);
        return false;
    }

    const auto lowered_type = LowerTypeInfo(module, canonical_type);
    if (!lowered_type.has_value() || lowered_type->backend_name != "{ptr, i64}") {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend could not lower hosted string global constant type " +
                               sema::FormatType(canonical_type),
                           diagnostics);
        return false;
    }

    const std::string decoded = DecodeStringLiteral(value.text);
    const std::string array_type = "[" + std::to_string(decoded.size() + 1) + " x i8]";
    prelude << backing_global_name << " = private unnamed_addr constant " << array_type << " c\""
            << EncodeLLVMStringBytes(decoded) << "\", align 1\n";

    std::ostringstream stream;
    stream << "{ptr getelementptr inbounds (" << array_type << ", ptr " << backing_global_name
           << ", i64 0, i64 0), i64 " << decoded.size() << '}';
    rendered = stream.str();
    return true;
}

bool IsSignedSourceType(std::string_view source_name) {
    return source_name == "i8" || source_name == "i16" || source_name == "i32" || source_name == "i64" ||
           source_name == "isize" || source_name == "int_literal";
}

bool IsUnsignedSourceType(std::string_view source_name) {
    return source_name == "u8" || source_name == "u16" || source_name == "u32" || source_name == "u64" ||
           source_name == "usize" || source_name == "uintptr";
}

bool IsFloatType(const BackendTypeInfo& type_info) {
    return type_info.backend_name == "float" || type_info.backend_name == "double";
}

std::size_t IntegerBitWidth(const BackendTypeInfo& type_info) {
    if (type_info.backend_name.size() < 2 || type_info.backend_name.front() != 'i') {
        return 0;
    }
    const std::string_view digits = std::string_view(type_info.backend_name).substr(1);
    if (!std::all_of(digits.begin(), digits.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return 0;
    }
    return static_cast<std::size_t>(std::stoul(std::string(digits)));
}

bool ResolveExecutableValue(const ExecutableFunctionState& state,
                            const std::string& value_name,
                            const mir::BasicBlock& block,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics,
                            ExecutableValue& value) {
    const auto it = state.values.find(value_name);
    if (it != state.values.end()) {
        value = it->second;
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap executable emission references unknown MIR value '" + value_name +
                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

bool ResolveExecutableOperands(const mir::Instruction& instruction,
                               const mir::BasicBlock& block,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const ExecutableFunctionState& state,
                               ExecutableValue& first) {
    return !instruction.operands.empty() &&
           ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, first);
}

bool ResolveExecutableOperands(const mir::Instruction& instruction,
                               const mir::BasicBlock& block,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const ExecutableFunctionState& state,
                               ExecutableValue& first,
                               ExecutableValue& second) {
    return instruction.operands.size() >= 2 &&
           ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, first) &&
           ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, second);
}

bool ResolveExecutableOperands(const mir::Instruction& instruction,
                               const mir::BasicBlock& block,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const ExecutableFunctionState& state,
                               ExecutableValue& first,
                               ExecutableValue& second,
                               ExecutableValue& third) {
    return instruction.operands.size() >= 3 &&
           ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, first) &&
           ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, second) &&
           ResolveExecutableValue(state, instruction.operands[2], block, source_path, diagnostics, third);
}

std::optional<sema::Type> ResolveExecutableFieldBaseType(const mir::Module& module,
                                                         const sema::Type& metadata_base_type,
                                                         const ExecutableValue& base_value) {
    if (!sema::IsUnknown(metadata_base_type)) {
        return metadata_base_type;
    }

    if (base_value.type.source_name == "str" || base_value.type.source_name == "string" ||
        base_value.type.source_name == "cstr") {
        return sema::StringType();
    }
    if (base_value.type.source_name == "Slice" || base_value.type.source_name.rfind("Slice<", 0) == 0) {
        return sema::NamedType("Slice");
    }
    if (base_value.type.source_name == "Buffer" || base_value.type.source_name.rfind("Buffer<", 0) == 0) {
        return sema::NamedType("Buffer");
    }

    if (FindTypeDecl(module, base_value.type.source_name) != nullptr) {
        return sema::NamedType(base_value.type.source_name);
    }

    return std::nullopt;
}

void RecordExecutableValue(ExecutableFunctionState& state,
                           const std::string& result_name,
                           const std::string& text,
                           const BackendTypeInfo& type,
                           std::optional<std::string> storage_slot,
                           std::optional<sema::Type> source_type) {
    state.values[result_name] = {
        .text = text,
        .type = type,
        .storage_slot = std::move(storage_slot),
        .source_type = std::move(source_type),
    };
}

bool EmitValueRepresentationCoercion(const ExecutableValue& value,
                                     const BackendTypeInfo& target_type,
                                     std::size_t function_index,
                                     std::size_t block_index,
                                     std::size_t instruction_index,
                                     std::string_view suffix,
                                     std::vector<std::string>& output_lines,
                                     std::string& coerced) {
    if (value.type.backend_name == target_type.backend_name) {
        coerced = value.text;
        return true;
    }

    const std::string temp = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix);
    if (value.type.backend_name == "i1" && target_type.backend_name == "i8") {
        output_lines.push_back(temp + " = zext i1 " + value.text + " to i8");
        coerced = temp;
        return true;
    }
    if (value.type.backend_name == "i8" && target_type.backend_name == "i1") {
        output_lines.push_back(temp + " = icmp ne i8 " + value.text + ", 0");
        coerced = temp;
        return true;
    }

    return false;
}

bool ExtendIntegerToI64(const ExecutableValue& value,
                        std::size_t function_index,
                        std::size_t block_index,
                        std::size_t instruction_index,
                        std::string_view suffix,
                        std::vector<std::string>& output_lines,
                        std::string& extended) {
    if (value.type.backend_name == "i64") {
        extended = value.text;
        return true;
    }

    const std::string temp = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix);
    if (IsSignedSourceType(value.type.source_name)) {
        output_lines.push_back(temp + " = sext " + value.type.backend_name + " " + value.text + " to i64");
    } else {
        output_lines.push_back(temp + " = zext " + value.type.backend_name + " " + value.text + " to i64");
    }
    extended = temp;
    return true;
}

bool EmitNumericConversion(const ExecutableValue& operand,
                           const BackendTypeInfo& result_type,
                           std::size_t function_index,
                           std::size_t block_index,
                           std::size_t instruction_index,
                           std::vector<std::string>& output_lines,
                           std::string& result_text) {
    if (operand.type.backend_name == result_type.backend_name) {
        result_text = operand.text;
        return true;
    }

    const bool source_float = IsFloatType(operand.type);
    const bool target_float = IsFloatType(result_type);
    const std::string temp = LLVMTempName(function_index, block_index, instruction_index);

    if (source_float && target_float) {
        const std::size_t source_bits = operand.type.backend_name == "float" ? 32 : 64;
        const std::size_t target_bits = result_type.backend_name == "float" ? 32 : 64;
        if (source_bits < target_bits) {
            output_lines.push_back(temp + " = fpext " + operand.type.backend_name + " " + operand.text + " to " +
                                   result_type.backend_name);
        } else {
            output_lines.push_back(temp + " = fptrunc " + operand.type.backend_name + " " + operand.text + " to " +
                                   result_type.backend_name);
        }
        result_text = temp;
        return true;
    }

    if (!source_float && !target_float) {
        const std::size_t source_bits = IntegerBitWidth(operand.type);
        const std::size_t target_bits = IntegerBitWidth(result_type);
        if (source_bits == 0 || target_bits == 0) {
            return false;
        }
        if (source_bits == target_bits) {
            result_text = operand.text;
            return true;
        }
        if (source_bits > target_bits) {
            output_lines.push_back(temp + " = trunc " + operand.type.backend_name + " " + operand.text + " to " +
                                   result_type.backend_name);
        } else if (IsSignedSourceType(operand.type.source_name)) {
            output_lines.push_back(temp + " = sext " + operand.type.backend_name + " " + operand.text + " to " +
                                   result_type.backend_name);
        } else {
            output_lines.push_back(temp + " = zext " + operand.type.backend_name + " " + operand.text + " to " +
                                   result_type.backend_name);
        }
        result_text = temp;
        return true;
    }

    if (source_float) {
        const std::string opcode = IsUnsignedSourceType(result_type.source_name) ? "fptoui" : "fptosi";
        output_lines.push_back(temp + " = " + opcode + " " + operand.type.backend_name + " " + operand.text + " to " +
                               result_type.backend_name);
        result_text = temp;
        return true;
    }

    const std::string opcode = IsUnsignedSourceType(operand.type.source_name) ? "uitofp" : "sitofp";
    output_lines.push_back(temp + " = " + opcode + " " + operand.type.backend_name + " " + operand.text + " to " +
                           result_type.backend_name);
    result_text = temp;
    return true;
}

namespace executable_support {

ExecutableFunctionSignature NormalizeFunctionSignature(const mir::Function& function) {
    ExecutableFunctionSignature signature;
    signature.params = ParameterLocals(function);
    signature.types.param_types.reserve(signature.params.size());
    for (const mir::Local* param : signature.params) {
        signature.types.param_types.push_back(param->type);
    }
    signature.types.return_types = function.return_types;
    return signature;
}

bool NormalizeProcedureTypeSignature(const sema::Type& procedure_type,
                                     const std::filesystem::path& source_path,
                                     support::DiagnosticSink& diagnostics,
                                     std::string_view context,
                                     ExecutableSignature& signature) {
    if (procedure_type.kind != sema::Type::Kind::kProcedure) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires checked procedure signature metadata for " +
                               std::string(context),
                           diagnostics);
        return false;
    }

    const auto parsed_param_count = mc::support::ParseArrayLength(procedure_type.metadata);
    if (!parsed_param_count.has_value() || *parsed_param_count > procedure_type.subtypes.size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission encountered malformed procedure signature metadata for " +
                               std::string(context),
                           diagnostics);
        return false;
    }

    signature.param_types.assign(procedure_type.subtypes.begin(),
                                 procedure_type.subtypes.begin() + static_cast<std::ptrdiff_t>(*parsed_param_count));
    signature.return_types.assign(procedure_type.subtypes.begin() + static_cast<std::ptrdiff_t>(*parsed_param_count),
                                  procedure_type.subtypes.end());
    return true;
}

sema::Type SignatureResultType(const ExecutableSignature& signature) {
    if (signature.return_types.empty()) {
        return sema::VoidType();
    }
    if (signature.return_types.size() == 1) {
        return signature.return_types.front();
    }
    return sema::TupleType(signature.return_types);
}

bool LowerExecutableParamTypes(const mir::Module& module,
                               const ExecutableSignature& signature,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const std::function<std::string(std::size_t)>& describe_param,
                               std::vector<BackendTypeInfo>& param_types) {
    param_types.clear();
    param_types.reserve(signature.param_types.size());
    for (std::size_t index = 0; index < signature.param_types.size(); ++index) {
        BackendTypeInfo param_type;
        if (!LowerInstructionType(module,
                                  signature.param_types[index],
                                  source_path,
                                  diagnostics,
                                  describe_param(index),
                                  param_type)) {
            return false;
        }
        param_types.push_back(std::move(param_type));
    }
    return true;
}

bool LowerExecutableReturnType(const mir::Module& module,
                               const ExecutableSignature& signature,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               std::string_view context,
                               std::optional<BackendTypeInfo>& return_type) {
    return_type.reset();
    if (signature.return_types.empty()) {
        return true;
    }

    const auto lowered_return_type = LowerFunctionReturnType(module, signature.return_types);
    if (!lowered_return_type.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend could not lower " + std::string(context),
                           diagnostics);
        return false;
    }

    return_type = *lowered_return_type;
    return true;
}

void RenderExecutableParameterList(const ExecutableFunctionSignature& function_signature,
                                   const std::vector<BackendTypeInfo>& param_types,
                                   bool include_names,
                                   std::ostringstream& stream) {
    for (std::size_t index = 0; index < function_signature.params.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << RenderLLVMParameter(param_types[index], *function_signature.params[index], include_names);
    }
}

bool LowerExecutableLocalTypes(const mir::Module& module,
                               const mir::Function& function,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               std::vector<BackendTypeInfo>& local_types) {
    local_types.clear();
    local_types.reserve(function.locals.size());
    for (const auto& local : function.locals) {
        BackendTypeInfo local_type;
        if (!LowerInstructionType(module,
                                  local.type,
                                  source_path,
                                  diagnostics,
                                  "local '" + local.name + "' for executable emission in function '" + function.name + "'",
                                  local_type)) {
            return false;
        }
        local_types.push_back(std::move(local_type));
    }
    return true;
}

void RenderExecutableEntryPrologue(const mir::Function& function,
                                   const ExecutableFunctionSignature& function_signature,
                                   const std::vector<BackendTypeInfo>& local_types,
                                   const std::vector<BackendTypeInfo>& param_types,
                                   std::ostringstream& stream) {
    for (std::size_t index = 0; index < function.locals.size(); ++index) {
        stream << "  " << LLVMLocalSlotName(function.locals[index].name) << " = alloca " << local_types[index].backend_name
               << ", align " << local_types[index].alignment << "\n";
    }
    for (std::size_t index = 0; index < function_signature.params.size(); ++index) {
        stream << "  store " << param_types[index].backend_name << " " << LLVMParamName(function_signature.params[index]->name)
               << ", ptr " << LLVMLocalSlotName(function_signature.params[index]->name) << ", align "
               << param_types[index].alignment << "\n";
    }
}

ExecutableFunctionState InitializeExecutableFunctionState(
    const mir::Module& module,
    const mir::Function& function,
    const ExecutableFunctionSignature& function_signature,
    std::size_t function_index,
    const ExecutableGlobals& globals,
    std::unordered_map<std::string, ExecutableStringConstant> string_constants) {
    ExecutableFunctionState state;
    state.module = &module;
    state.function = &function;
    state.signature = function_signature;
    state.globals = &globals;
    state.string_constants = std::move(string_constants);
    for (const auto& local : function.locals) {
        state.local_slots.emplace(local.name, LLVMLocalSlotName(local.name));
    }
    for (std::size_t block_index = 0; block_index < function.blocks.size(); ++block_index) {
        state.block_labels.emplace(function.blocks[block_index].label,
                                   LLVMBlockLabel(function_index, block_index, function.blocks[block_index].label));
    }
    return state;
}

bool CollectExecutableStringConstants(const mir::Module& module,
                                     const mir::Function& function,
                                     std::size_t function_index,
                                     const std::filesystem::path& source_path,
                                     support::DiagnosticSink& diagnostics,
                                     std::ostringstream& stream,
                                     std::unordered_map<std::string, ExecutableStringConstant>& string_constants) {
    std::size_t string_constant_index = 0;
    string_constants.clear();
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (instruction.kind != mir::Instruction::Kind::kConst || instruction.result.empty() || instruction.op.size() < 2 ||
                instruction.op.front() != '"') {
                continue;
            }

            BackendTypeInfo string_type;
            if (!LowerInstructionType(module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "string literal constant for executable emission in function '" + function.name + "'",
                                      string_type)) {
                return false;
            }
            if (string_type.backend_name != "{ptr, i64}") {
                continue;
            }

            const std::string decoded = DecodeStringLiteral(instruction.op);
            const std::string global_name = "@.mc.str." + std::to_string(function_index) + "." + std::to_string(string_constant_index++);
            const std::string array_type = "[" + std::to_string(decoded.size() + 1) + " x i8]";
            stream << global_name << " = private unnamed_addr constant " << array_type << " c\""
                   << EncodeLLVMStringBytes(decoded) << "\", align 1\n";
            string_constants.emplace(instruction.result,
                                     ExecutableStringConstant {
                                         .global_name = global_name,
                                         .array_type = array_type,
                                         .length = decoded.size(),
                                     });
        }
    }
    if (!string_constants.empty()) {
        stream << "\n";
    }
    return true;
}

}  // namespace executable_support

}  // namespace mc::codegen_llvm
