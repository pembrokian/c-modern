#include "compiler/codegen_llvm/backend_internal.h"

namespace mc::codegen_llvm {
namespace {

bool FormatExecutableCallArguments(const mir::Instruction& instruction,
                                   const ExecutableCallSignature& call_signature,
                                   const ExecutableEmissionContext& context,
                                   std::string& args_text) {
    const std::size_t argument_start = 1;
    const std::size_t argument_count = instruction.operands.size() - argument_start;
    if (argument_count != call_signature.types.param_types.size()) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission expected " + std::to_string(call_signature.types.param_types.size()) +
                               " call arguments for " + call_signature.call_label + " but saw " + std::to_string(argument_count) +
                               " in function '" + context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    std::vector<BackendTypeInfo> param_types;
    if (!executable_support::LowerExecutableParamTypes(*context.state.module,
                                                       call_signature.types,
                                                       context.source_path,
                                                       context.diagnostics,
                                                       [&](std::size_t) {
                                                           return FunctionBlockContext("call argument", context.state.function->name, context.block);
                                                       },
                                                       param_types)) {
        return false;
    }

    std::ostringstream rendered_args;
    for (std::size_t index = 0; index < argument_count; ++index) {
        ExecutableValue value;
        if (!ResolveExecutableValue(context.state,
                                    instruction.operands[argument_start + index],
                                    context.block,
                                    context.source_path,
                                    context.diagnostics,
                                    value)) {
            return false;
        }
        if (index > 0) {
            rendered_args << ", ";
        }
        rendered_args << param_types[index].backend_name << " " << value.text;
    }

    args_text = rendered_args.str();
    return true;
}

bool ResolveExecutableCallSignature(const mir::Instruction& instruction,
                                    const std::unordered_map<std::string, std::string>& function_symbols,
                                    const ExecutableEmissionContext& context,
                                    ExecutableCallSignature& signature) {
    if (instruction.operands.empty()) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission requires a callee operand for call in function '" +
                               context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    ExecutableValue callee_value;
    if (!ResolveExecutableValue(context.state,
                                instruction.operands.front(),
                                context.block,
                                context.source_path,
                                context.diagnostics,
                                callee_value)) {
        return false;
    }

    const bool indirect_call = instruction.target_name.empty() || instruction.target_kind != mir::Instruction::TargetKind::kFunction;
    if (indirect_call) {
        if (!IsProcedureSourceType(callee_value.type.source_name)) {
            ReportBackendError(context.source_path,
                               "LLVM bootstrap executable emission requires procedure-typed callee for indirect call in function '" +
                                   context.state.function->name + "' block '" + context.block.label + "'",
                               context.diagnostics);
            return false;
        }
        signature.callee_text = callee_value.text;
        signature.call_label = "indirect call";
        return executable_support::NormalizeProcedureTypeSignature(
            callee_value.source_type.has_value() ? *callee_value.source_type : sema::UnknownType(),
            context.source_path,
            context.diagnostics,
            "indirect call in function '" + context.state.function->name + "' block '" + context.block.label + "'",
            signature.types);
    }

    const mir::Function* callee = FindFunction(*context.state.module, instruction.target_name);
    if (callee == nullptr) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission references unknown direct callee '" +
                               instruction.target_name + "' in function '" + context.state.function->name + "' block '" +
                               context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    const auto function_it = function_symbols.find(callee->name);
    if (function_it == function_symbols.end()) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission has no symbol mapping for direct callee '" +
                               instruction.target_name + "' in function '" + context.state.function->name + "' block '" +
                               context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    signature.callee_text = function_it->second;
    signature.call_label = "call to '" + instruction.target_name + "'";
    signature.types = executable_support::NormalizeFunctionSignature(*callee).types;
    return true;
}

}  // namespace

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

bool IsProcedureSourceType(std::string_view source_name) {
    return source_name.rfind("proc(", 0) == 0;
}

bool EmitCallInstruction(const mir::Instruction& instruction,
                         const ExecutableEmissionContext& context,
                         const std::unordered_map<std::string, std::string>& function_symbols) {
    if ((instruction.target_name == "hal.memory_barrier" || instruction.target_name == "memory_barrier") &&
        instruction.operands.empty() && instruction.result.empty()) {
        context.output_lines.push_back("fence seq_cst");
        return true;
    }

    ExecutableCallSignature call_signature;
    if (!ResolveExecutableCallSignature(instruction,
                                        function_symbols,
                                        context,
                                        call_signature)) {
        return false;
    }

    std::string args_text;
    if (!FormatExecutableCallArguments(instruction,
                                       call_signature,
                                       context,
                                       args_text)) {
        return false;
    }

    const sema::Type result_source_type = executable_support::SignatureResultType(call_signature.types);
    std::optional<BackendTypeInfo> lowered_return_type;
    if (!executable_support::LowerExecutableReturnType(*context.state.module,
                                                       call_signature.types,
                                                       context.source_path,
                                                       context.diagnostics,
                                                       "call result type for '" +
                                                           std::string(instruction.target_name.empty() ? instruction.target_display : instruction.target_name) +
                                                           "' in function '" + context.state.function->name + "' block '" + context.block.label + "'",
                                                       lowered_return_type)) {
        return false;
    }

    if (instruction.result.empty()) {
        if (result_source_type.kind == sema::Type::Kind::kVoid) {
            context.output_lines.push_back("call void " + call_signature.callee_text + "(" + args_text + ")");
            return true;
        }

        context.output_lines.push_back("call " + lowered_return_type->backend_name + " " + call_signature.callee_text + "(" + args_text + ")");
        return true;
    }

    const std::string temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index);
    context.output_lines.push_back(temp + " = call " + lowered_return_type->backend_name + " " + call_signature.callee_text + "(" +
                                   args_text + ")");
    RecordExecutableValue(context.state, instruction.result, temp, *lowered_return_type, std::nullopt, instruction.type);
    return true;
}

}  // namespace mc::codegen_llvm