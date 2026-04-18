#include "compiler/codegen_llvm/backend_internal.h"

namespace mc::codegen_llvm {
namespace {

bool EmitSymbolRefInstruction(const mir::Instruction& instruction,
                              const ExecutableEmissionContext& context,
                              const std::unordered_map<std::string, std::string>& function_symbols) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;

    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    const bool function_or_global = instruction.target_kind == mir::Instruction::TargetKind::kFunction ||
                                    instruction.target_kind == mir::Instruction::TargetKind::kGlobal;
    const bool enum_constant = instruction.target_kind == mir::Instruction::TargetKind::kOther &&
                               instruction.type.kind == sema::Type::Kind::kNamed &&
                               !instruction.target_name.empty();
    if ((!function_or_global && !enum_constant) || instruction.target_name.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission only supports function and global symbol_ref values in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    BackendTypeInfo type_info;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("symbol_ref", state, block),
                              type_info)) {
        return false;
    }

    if (enum_constant) {
        const mir::TypeDecl* type_decl = FindTypeDecl(*state.module, instruction.type.name);
        if (type_decl == nullptr || type_decl->kind != mir::TypeDecl::Kind::kEnum) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission could not resolve enum symbol_ref type '" +
                                   sema::FormatType(instruction.type) + "' in function '" + state.function->name +
                                   "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }

        const auto lowered_layout = LowerEnumLayout(*state.module, *type_decl, instruction.type);
        if (!lowered_layout.has_value()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission could not lower enum symbol_ref type '" +
                                   sema::FormatType(instruction.type) + "' in function '" + state.function->name +
                                   "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }

        std::size_t variant_index = 0;
        const mir::VariantDecl* variant_decl = FindVariantDecl(*type_decl, instruction.target_name, &variant_index);
        if (variant_decl == nullptr || !variant_decl->payload_fields.empty()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission only supports payload-free enum symbol_ref values in function '" +
                                   state.function->name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }

        const std::string value_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index);
        output_lines.push_back(value_temp + " = insertvalue " + lowered_layout->aggregate_type.backend_name +
                               " zeroinitializer, i64 " + std::to_string(variant_index) + ", 0");
        RecordExecutableValue(state, instruction.result, value_temp, lowered_layout->aggregate_type, std::nullopt, instruction.type);
        return true;
    }

    if (instruction.target_kind == mir::Instruction::TargetKind::kFunction) {
        const auto function_it = function_symbols.find(instruction.target_name);
        if (function_it == function_symbols.end()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission references unknown function symbol '" +
                                   instruction.target_name + "' in function '" + state.function->name + "' block '" +
                                   block.label + "'",
                               diagnostics);
            return false;
        }

        RecordExecutableValue(state, instruction.result, function_it->second, type_info, std::nullopt, instruction.type);
        return true;
    }

    const auto global_it = state.globals->find(instruction.target_name);
    if (global_it == state.globals->end()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission references unknown global symbol '" +
                               instruction.target_name + "' in function '" + state.function->name + "' block '" +
                               block.label + "'",
                           diagnostics);
        return false;
    }

    const std::string temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index);
    output_lines.push_back(temp + " = load " + type_info.backend_name + ", ptr " + global_it->second.backend_name +
                           ", align " + std::to_string(type_info.alignment));
    RecordExecutableValue(state, instruction.result, temp, type_info, std::nullopt, instruction.type);
    return true;
}

bool EmitConvertDistinctInstruction(const mir::Instruction& instruction,
                                    const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;

    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue operand;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "convert_distinct",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("convert_distinct", state, block),
                              result_type)) {
        return false;
    }

    RecordExecutableValue(state, instruction.result, operand.text, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitConvertInstruction(const mir::Instruction& instruction,
                            const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;

    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue operand;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "convert",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("convert", state, block),
                              result_type)) {
        return false;
    }

    if (operand.type.backend_name != result_type.backend_name) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission only supports representation-preserving convert in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    RecordExecutableValue(state, instruction.result, operand.text, result_type);
    return true;
}

bool EmitConvertNumericInstruction(const mir::Instruction& instruction,
                                   const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;

    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue operand;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "convert_numeric",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("convert_numeric", state, block),
                              result_type)) {
        return false;
    }

    std::string converted;
    if (!EmitNumericConversion(operand,
                               result_type,
                               context.function_index,
                               context.block_index,
                               context.instruction_index,
                               output_lines,
                               converted)) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission could not lower numeric conversion in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    RecordExecutableValue(state, instruction.result, converted, result_type);
    return true;
}

}  // namespace

bool EmitValueInstruction(const mir::Instruction& instruction,
                          const ExecutableEmissionContext& context,
                          const std::unordered_map<std::string, std::string>& function_symbols) {
    switch (instruction.kind) {
        case mir::Instruction::Kind::kSymbolRef:
            return EmitSymbolRefInstruction(instruction, context, function_symbols);
        case mir::Instruction::Kind::kConvertDistinct:
            return EmitConvertDistinctInstruction(instruction, context);
        case mir::Instruction::Kind::kConvert:
            return EmitConvertInstruction(instruction, context);
        case mir::Instruction::Kind::kConvertNumeric:
            return EmitConvertNumericInstruction(instruction, context);
        default:
            MC_UNREACHABLE("unexpected non-value instruction in executable value lowering");
    }
}

}  // namespace mc::codegen_llvm