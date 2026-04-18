#include "compiler/codegen_llvm/backend_internal.h"

#include <unordered_set>

namespace mc::codegen_llvm {
namespace {

std::string_view LeafTypeName(std::string_view name) {
    const std::size_t separator = name.rfind('.');
    if (separator == std::string_view::npos) {
        return name;
    }
    return name.substr(separator + 1);
}

bool IsNamedTypeFamily(const sema::Type& type, std::string_view family_name) {
    return type.kind == sema::Type::Kind::kNamed && LeafTypeName(type.name) == family_name;
}

std::optional<sema::Type> PointerPointeeType(const sema::Type& type) {
    if (type.kind != sema::Type::Kind::kPointer || type.subtypes.empty()) {
        return std::nullopt;
    }
    return type.subtypes.front();
}

std::optional<sema::Type> AtomicElementType(const mir::Module& module, const sema::Type& pointer_type) {
    const auto pointee = PointerPointeeType(StripMirAliasOrDistinct(module, pointer_type));
    if (!pointee.has_value()) {
        return std::nullopt;
    }
    const sema::Type stripped_pointee = StripMirAliasOrDistinct(module, *pointee);
    if (stripped_pointee.kind != sema::Type::Kind::kNamed || !IsNamedTypeFamily(stripped_pointee, "Atomic") ||
        stripped_pointee.subtypes.empty()) {
        return std::nullopt;
    }
    return stripped_pointee.subtypes.front();
}

std::optional<sema::Type> FindFunctionValueType(const mir::Module& module,
                                                const mir::Function& function,
                                                std::string_view value_name) {
    for (const auto& local : function.locals) {
        if (local.name == value_name) {
            return local.type;
        }
    }
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (instruction.result == value_name) {
                return instruction.type;
            }
        }
    }
    for (const auto& global : module.globals) {
        for (const auto& global_name : global.names) {
            if (global_name == value_name) {
                return global.type;
            }
        }
    }
    return std::nullopt;
}

bool ResolveAtomicElementBackendType(const mir::Module& module,
                                     const mir::Function& function,
                                     const mir::Instruction& instruction,
                                     const std::filesystem::path& source_path,
                                     support::DiagnosticSink& diagnostics,
                                     std::string_view context,
                                     BackendTypeInfo& element_type) {
    if (instruction.operands.empty()) {
        return false;
    }
    const auto ptr_type = FindFunctionValueType(module, function, instruction.operands.front());
    if (!ptr_type.has_value()) {
        ReportBackendError(source_path,
                           std::string("LLVM bootstrap executable emission could not resolve atomic pointer type for '") +
                               std::string(context) + "' in function '" + function.name + "'",
                           diagnostics);
        return false;
    }
    const auto atomic_element_type = AtomicElementType(module, *ptr_type);
    if (!atomic_element_type.has_value()) {
        ReportBackendError(source_path,
                           std::string("LLVM bootstrap executable emission requires *Atomic<T> pointer for '") +
                               std::string(context) + "' in function '" + function.name + "'",
                           diagnostics);
        return false;
    }
    return LowerInstructionType(module,
                                *atomic_element_type,
                                source_path,
                                diagnostics,
                                std::string(context) + " atomic element",
                                element_type);
}

bool AtomicOrderAllowedForInstruction(mir::Instruction::Kind kind,
                                      std::string_view order_name) {
    const std::string leaf = VariantLeafName(order_name);
    switch (kind) {
        case mir::Instruction::Kind::kAtomicLoad:
            return leaf == "Relaxed" || leaf == "Acquire" || leaf == "SeqCst";
        case mir::Instruction::Kind::kAtomicStore:
            return leaf == "Relaxed" || leaf == "Release" || leaf == "SeqCst";
        case mir::Instruction::Kind::kAtomicExchange:
        case mir::Instruction::Kind::kAtomicFetchAdd:
            return leaf == "Relaxed" || leaf == "Acquire" || leaf == "Release" || leaf == "AcqRel" ||
                   leaf == "SeqCst";
        default:
            return false;
    }
}

bool RequireAtomicInstructionResult(const mir::Instruction& instruction,
                                    const mir::BasicBlock& block,
                                    const std::filesystem::path& source_path,
                                    support::DiagnosticSink& diagnostics,
                                    const ExecutableFunctionState& state,
                                    std::string_view op_name) {
    if (!instruction.result.empty()) {
        return true;
    }
    ReportBackendError(source_path,
                       "LLVM bootstrap executable emission requires result for '" + std::string(op_name) +
                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

bool RequireAtomicOperandCount(const mir::Instruction& instruction,
                               std::size_t expected_count,
                               const mir::BasicBlock& block,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const ExecutableFunctionState& state,
                               std::string_view op_name) {
    if (instruction.operands.size() == expected_count) {
        return true;
    }
    ReportBackendError(source_path,
                       "LLVM bootstrap executable emission requires exactly " + std::to_string(expected_count) +
                           " operands for '" + std::string(op_name) + "' in function '" + state.function->name +
                           "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

std::string ExecutableAtomicContext(std::string_view operation,
                                    const ExecutableFunctionState& state,
                                    const mir::BasicBlock& block) {
    return std::string(operation) + " in executable emission for function '" + state.function->name + "' block '" +
           block.label + "'";
}

}  // namespace

namespace executable_atomic {

bool RenderAtomicInstruction(const mir::Instruction& instruction,
                             const ExecutableEmissionContext& context) {
    const std::size_t function_index = context.function_index;
    const std::size_t block_index = context.block_index;
    const std::size_t instruction_index = context.instruction_index;
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;
    switch (instruction.kind) {
        case mir::Instruction::Kind::kAtomicLoad: {
            if (!RequireAtomicInstructionResult(instruction, block, source_path, diagnostics, state, "atomic_load") ||
                !RequireAtomicOperandCount(instruction, 2, block, source_path, diagnostics, state, "atomic_load")) {
                return false;
            }
            ExecutableValue ptr;
            if (!ResolveExecutableOperands(instruction, block, source_path, diagnostics, state, ptr)) {
                return false;
            }
            const auto order = LLVMAtomicOrderKeyword(instruction.atomic_order);
            if (!order.has_value() || !AtomicOrderAllowedForInstruction(instruction.kind, instruction.atomic_order)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires supported constant MemoryOrder metadata for 'atomic_load' in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            if (ptr.type.backend_name != "ptr") {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires pointer operand for atomic_load in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            BackendTypeInfo result_type;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      ExecutableAtomicContext("atomic_load", state, block),
                                      result_type)) {
                return false;
            }
            if (!IsAtomicScalarType(result_type)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission only supports scalar atomic_load element types in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
            output_lines.push_back(temp + " = load atomic " + result_type.backend_name + ", ptr " + ptr.text + " " + *order +
                                   ", align " + std::to_string(result_type.alignment));
            RecordExecutableValue(state, instruction.result, temp, result_type);
            return true;
        }

        case mir::Instruction::Kind::kAtomicStore: {
            if (!RequireAtomicOperandCount(instruction, 3, block, source_path, diagnostics, state, "atomic_store")) {
                return false;
            }
            ExecutableValue ptr;
            ExecutableValue value;
            if (!ResolveExecutableOperands(instruction, block, source_path, diagnostics, state, ptr, value)) {
                return false;
            }
            const auto order = LLVMAtomicOrderKeyword(instruction.atomic_order);
            if (!order.has_value() || !AtomicOrderAllowedForInstruction(instruction.kind, instruction.atomic_order)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires supported constant MemoryOrder metadata for 'atomic_store' in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            if (ptr.type.backend_name != "ptr") {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires pointer operand for atomic_store in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            BackendTypeInfo element_type;
            if (!ResolveAtomicElementBackendType(*state.module,
                                                *state.function,
                                                instruction,
                                                source_path,
                                                diagnostics,
                                                "atomic_store",
                                                element_type)) {
                return false;
            }
            if (!IsAtomicScalarType(element_type)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission only supports scalar atomic_store element types in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            std::string stored_value = value.text;
            if (!EmitNumericConversion(value,
                                       element_type,
                                       function_index,
                                       block_index,
                                       instruction_index,
                                       output_lines,
                                       stored_value)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not coerce atomic_store value to element type in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            output_lines.push_back("store atomic " + element_type.backend_name + " " + stored_value + ", ptr " + ptr.text + " " +
                                   *order + ", align " + std::to_string(element_type.alignment));
            return true;
        }

        case mir::Instruction::Kind::kAtomicExchange:
        case mir::Instruction::Kind::kAtomicFetchAdd: {
            if (!RequireAtomicInstructionResult(instruction,
                                                block,
                                                source_path,
                                                diagnostics,
                                                state,
                                                ToString(instruction.kind)) ||
                !RequireAtomicOperandCount(instruction,
                                           3,
                                           block,
                                           source_path,
                                           diagnostics,
                                           state,
                                           ToString(instruction.kind))) {
                return false;
            }
            ExecutableValue ptr;
            ExecutableValue value;
            if (!ResolveExecutableOperands(instruction, block, source_path, diagnostics, state, ptr, value)) {
                return false;
            }
            const auto order = LLVMAtomicOrderKeyword(instruction.atomic_order);
            if (!order.has_value() || !AtomicOrderAllowedForInstruction(instruction.kind, instruction.atomic_order)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires supported constant MemoryOrder metadata for '" +
                                       std::string(ToString(instruction.kind)) + "' in function '" + state.function->name +
                                       "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            if (ptr.type.backend_name != "ptr") {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires pointer operand for '" + std::string(ToString(instruction.kind)) +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            BackendTypeInfo element_type;
            if (!ResolveAtomicElementBackendType(*state.module,
                                                *state.function,
                                                instruction,
                                                source_path,
                                                diagnostics,
                                                ToString(instruction.kind),
                                                element_type)) {
                return false;
            }
            if (!IsAtomicScalarType(element_type) ||
                (instruction.kind == mir::Instruction::Kind::kAtomicFetchAdd && !IsIntegerType(element_type))) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission only supports scalar atomic element types for '" +
                                       std::string(ToString(instruction.kind)) + "' in function '" + state.function->name +
                                       "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            BackendTypeInfo result_type;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      ExecutableAtomicContext(ToString(instruction.kind), state, block),
                                      result_type)) {
                return false;
            }
            const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
            const std::string op_name = instruction.kind == mir::Instruction::Kind::kAtomicExchange ? "xchg" : "add";
            std::string atomic_value = value.text;
            if (!EmitNumericConversion(value,
                                       element_type,
                                       function_index,
                                       block_index,
                                       instruction_index,
                                       output_lines,
                                       atomic_value)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not coerce atomic value to element type for '" +
                                       std::string(ToString(instruction.kind)) + "' in function '" + state.function->name +
                                       "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            output_lines.push_back(temp + " = atomicrmw " + op_name + " ptr " + ptr.text + ", " + element_type.backend_name + " " +
                                   atomic_value + " " + *order + ", align " + std::to_string(element_type.alignment));
            RecordExecutableValue(state, instruction.result, temp, result_type);
            return true;
        }

        default:
            return false;
    }
}

}  // namespace executable_atomic

}  // namespace mc::codegen_llvm
