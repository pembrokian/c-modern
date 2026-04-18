#include "compiler/codegen_llvm/backend_internal.h"

namespace mc::codegen_llvm {
namespace {

const mir::Function* FindFunction(const mir::Module& module,
                                  std::string_view name) {
    for (const auto& function : module.functions) {
        if (function.name == name) {
            return &function;
        }
    }
    return nullptr;
}

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