#include "compiler/codegen_llvm/backend_internal.h"

#include <algorithm>
#include <cctype>

namespace mc::codegen_llvm {
namespace {

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

}  // namespace

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

}  // namespace mc::codegen_llvm
