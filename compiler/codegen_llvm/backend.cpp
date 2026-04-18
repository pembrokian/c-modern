#include "compiler/codegen_llvm/backend.h"
#include "compiler/codegen_llvm/backend_internal.h"

#include <cstdlib>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>

// Bootstrap LLVM backend — two-pass code generation architecture.
//
// This file contains two independent passes over the MIR module:
//
//   1. Debug / inspect path (LowerFunction, BackendModule, BackendBlock)
//      Produces a human-readable pseudo-IR text analogous to mir::DumpModule.
//      Activated by --inspect-surface=backend_ir_text.  Does NOT emit real
//      LLVM IR; it is a diagnostic aid only.
//
//   2. Real LLVM IR emission path (RenderExecutableFunction,
//      RenderExecutableInstruction, EmitXxx helpers)
//      Produces textual LLVM IR that is piped to `llc` and then `clang`.
//      This is the path taken by `mc build`.
//
// Both paths must be updated when a new mir::Instruction::Kind is added.
// The debug path calls LowerInstruction for every instruction; the real path
// calls RenderExecutableInstruction.  Forgetting to handle a new kind in
// either path will produce a "not handled in backend" diagnostic at runtime,
// not a compile-time error — keep this in mind during MIR extension work.
//
// EmitArenaNewInstruction assumes the Arena struct layout:
//   { ptr i8*, i64 cap, i64 used, ptr Allocator* }
// The allocation alignment is rounded up to the pointer size of the target.
// Changing the Arena struct in the runtime requires updating this function.

#include "compiler/sema/type_utils.h"
#include "compiler/support/assert.h"
#include "compiler/support/target.h"

namespace mc::codegen_llvm {
namespace {

using mc::kBootstrapTriple;
using mc::kBootstrapTargetFamily;
constexpr std::string_view kBootstrapObjectFormat = "macho";
constexpr std::string_view kInspectSurface = "backend_ir_text";

std::optional<std::string_view> UnsupportedExecutableInstructionName(mir::Instruction::Kind kind);

bool IsBootstrapTarget(const TargetConfig& target) {
    return target.triple == kBootstrapTriple && target.target_family == kBootstrapTargetFamily &&
           target.object_format == kBootstrapObjectFormat;
}

// Forward declaration — defined below once TypeSupportsErasedGenericEmission is available.
bool FunctionSupportsErasedGenericEmission(const mir::Module& module, const mir::Function& function);

bool RenderExecutableInstruction(const mir::Instruction& instruction,
                                 std::size_t function_index,
                                 std::size_t block_index,
                                 std::size_t instruction_index,
                                 const std::unordered_map<std::string, std::string>& function_symbols,
                                 const std::filesystem::path& source_path,
                                 support::DiagnosticSink& diagnostics,
                                 ExecutableFunctionState& state,
                                 std::vector<std::string>& output_lines);

bool RenderExecutableTerminator(const mir::BasicBlock& block,
                                const std::filesystem::path& source_path,
                                support::DiagnosticSink& diagnostics,
                                const ExecutableFunctionState& state,
                                std::vector<std::string>& output_lines);

bool CollectExecutableGlobals(const mir::Module& module,
                             const std::filesystem::path& source_path,
                             support::DiagnosticSink& diagnostics,
                             ExecutableGlobals& globals) {
    globals.clear();
    for (const auto& global : module.globals) {
        BackendTypeInfo global_type;
        if (!LowerInstructionType(module,
                                  global.type,
                                  source_path,
                                  diagnostics,
                                  "global for executable emission",
                                  global_type)) {
            return false;
        }
        for (const auto& name : global.names) {
            globals.emplace(name,
                            ExecutableGlobalInfo{
                                .backend_name = LLVMGlobalName(name),
                                .type = global.type,
                                .lowered_type = global_type,
                                .is_const = global.is_const,
                                .is_extern = global.is_extern,
                                .initializers = global.initializers,
                            });
        }
    }
    return true;
}

bool RenderExecutableBlocks(const mir::Function& function,
                            std::size_t function_index,
                            const std::unordered_map<std::string, std::string>& function_symbols,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics,
                            ExecutableFunctionState& state,
                            std::ostringstream& stream) {
    for (std::size_t block_index = 0; block_index < function.blocks.size(); ++block_index) {
        const auto& block = function.blocks[block_index];
        stream << LLVMBlockLabel(function_index, block_index, block.label) << ":\n";

        std::vector<std::string> lines;
        for (std::size_t instruction_index = 0; instruction_index < block.instructions.size(); ++instruction_index) {
            if (!RenderExecutableInstruction(block.instructions[instruction_index],
                                             function_index,
                                             block_index,
                                             instruction_index,
                                             function_symbols,
                                             source_path,
                                             diagnostics,
                                             state,
                                             lines)) {
                if (!diagnostics.HasErrors()) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission failed while rendering MIR instruction '" +
                                           std::string(ToString(block.instructions[instruction_index].kind)) +
                                           "' in function '" + function.name + "' block '" + block.label + "'",
                                       diagnostics);
                }
                return false;
            }
        }
        if (!RenderExecutableTerminator(block, source_path, diagnostics, state, lines)) {
            if (!diagnostics.HasErrors()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission failed while rendering terminator in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
            }
            return false;
        }

        for (const auto& line : lines) {
            stream << "  " << line << "\n";
        }
        stream << "\n";
    }
    return true;
}


bool TypeSupportsErasedGenericEmission(const mir::Module& module,
                                       const sema::Type& type) {
    return sema::IsUnknown(type) || type.kind == sema::Type::Kind::kVoid || LowerTypeInfo(module, type).has_value();
}

bool FunctionSupportsErasedGenericEmission(const mir::Module& module,
                                           const mir::Function& function) {
    for (const auto& local : function.locals) {
        if (!TypeSupportsErasedGenericEmission(module, local.type)) {
            return false;
        }
    }
    for (const auto& return_type : function.return_types) {
        if (!TypeSupportsErasedGenericEmission(module, return_type)) {
            return false;
        }
    }
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (!TypeSupportsErasedGenericEmission(module, instruction.type) ||
                !TypeSupportsErasedGenericEmission(module, instruction.target_base_type)) {
                return false;
            }
            for (const auto& aux_type : instruction.target_aux_types) {
                if (!TypeSupportsErasedGenericEmission(module, aux_type)) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool ShouldEmitFunctionForExecutable(const mir::Module& module,
                                     const mir::Function& function) {
    return function.type_params.empty() || FunctionSupportsErasedGenericEmission(module, function);
}

std::optional<std::string_view> UnsupportedExecutableInstructionName(mir::Instruction::Kind kind) {
    switch (kind) {
        case mir::Instruction::Kind::kAtomicCompareExchange:
            return "atomic_compare_exchange";
        default:
            return std::nullopt;
    }
}

bool ResolveExecutableBlock(const ExecutableFunctionState& state,
                            const std::string& block_name,
                            const mir::BasicBlock& block,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics,
                            std::string& label) {
    const auto it = state.block_labels.find(block_name);
    if (it != state.block_labels.end()) {
        label = it->second;
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap executable emission references unknown branch target '" + block_name +
                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

std::string DivCheckHelperName(const BackendTypeInfo& type_info) {
    return "@__mc_check_div_" + type_info.backend_name;
}

std::string ShiftCheckHelperName(const BackendTypeInfo& type_info) {
    return "@__mc_check_shift_" + std::to_string(IntegerBitWidth(type_info));
}

void EmitDivCheckHelperDefinition(std::ostringstream& stream,
                                  std::string_view backend_name) {
    stream << "define private void @__mc_check_div_" << backend_name << "(" << backend_name << " %rhs) {\n";
    stream << "entry:\n";
    stream << "  %is.zero = icmp eq " << backend_name << " %rhs, 0\n";
    stream << "  br i1 %is.zero, label %trap, label %ok\n";
    stream << "trap:\n";
    stream << "  call void @__mc_trap()\n";
    stream << "  unreachable\n";
    stream << "ok:\n";
    stream << "  ret void\n";
    stream << "}\n\n";
}

void EmitShiftCheckHelperDefinition(std::ostringstream& stream,
                                    std::size_t bit_width) {
    stream << "define private void @__mc_check_shift_" << bit_width << "(i64 %count) {\n";
    stream << "entry:\n";
    stream << "  %bad = icmp uge i64 %count, " << bit_width << "\n";
    stream << "  br i1 %bad, label %trap, label %ok\n";
    stream << "trap:\n";
    stream << "  call void @__mc_trap()\n";
    stream << "  unreachable\n";
    stream << "ok:\n";
    stream << "  ret void\n";
    stream << "}\n\n";
}

struct ExecutableRuntimeRequirements {
    bool needs_malloc = false;
    bool needs_free = false;
    bool needs_panic = false;
};

ExecutableRuntimeRequirements CollectExecutableRuntimeRequirements(const mir::Module& module) {
    ExecutableRuntimeRequirements requirements;
    for (const auto& function : module.functions) {
        for (const auto& block : function.blocks) {
            for (const auto& instruction : block.instructions) {
                if (instruction.kind == mir::Instruction::Kind::kBufferNew) {
                    requirements.needs_malloc = true;
                }
                if (instruction.kind == mir::Instruction::Kind::kBufferFree) {
                    requirements.needs_free = true;
                }
            }
            if (block.terminator.kind == mir::Terminator::Kind::kPanic) {
                requirements.needs_panic = true;
            }
        }
    }
    return requirements;
}

bool RenderExecutableInstruction(const mir::Instruction& instruction,
                                 std::size_t function_index,
                                 std::size_t block_index,
                                 std::size_t instruction_index,
                                 const std::unordered_map<std::string, std::string>& function_symbols,
                                 const std::filesystem::path& source_path,
                                 support::DiagnosticSink& diagnostics,
                                 ExecutableFunctionState& state,
                                 std::vector<std::string>& output_lines) {
    const mir::BasicBlock& block = state.function->blocks[block_index];
    ExecutableEmissionContext emission {
        .function_index = function_index,
        .block_index = block_index,
        .instruction_index = instruction_index,
        .block = block,
        .source_path = source_path,
        .diagnostics = diagnostics,
        .state = state,
        .output_lines = output_lines,
    };

    if (const auto unsupported = UnsupportedExecutableInstructionName(instruction.kind); unsupported.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission does not yet support MIR instruction '" + std::string(*unsupported) +
                               "' in function '" + state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    switch (instruction.kind) {
        case mir::Instruction::Kind::kConst: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      ExecutableFunctionBlockContext("const", state, block),
                                      type_info)) {
                return false;
            }
            const auto string_constant = state.string_constants.find(instruction.result);
            if (string_constant != state.string_constants.end()) {
                const std::string data_temp = LLVMTempName(function_index, block_index, instruction_index) + ".str.ptr";
                const std::string init_temp = LLVMTempName(function_index, block_index, instruction_index) + ".str.init";
                const std::string value_temp = LLVMTempName(function_index, block_index, instruction_index);
                output_lines.push_back(data_temp + " = getelementptr inbounds " + string_constant->second.array_type + ", ptr " +
                                       string_constant->second.global_name + ", i64 0, i64 0");
                output_lines.push_back(init_temp + " = insertvalue {ptr, i64} zeroinitializer, ptr " + data_temp + ", 0");
                output_lines.push_back(value_temp + " = insertvalue {ptr, i64} " + init_temp + ", i64 " +
                                       std::to_string(string_constant->second.length) + ", 1");
                RecordExecutableValue(state, instruction.result, value_temp, type_info, std::nullopt, instruction.type);
                return true;
            }
            RecordExecutableValue(state, instruction.result, FormatLLVMLiteral(type_info, instruction.op), type_info, std::nullopt, instruction.type);
            return true;
        }

        case mir::Instruction::Kind::kLocalAddr: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kLoadLocal: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kStoreLocal: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kBoundsCheck: {
            return EmitBoundsCheckCall(instruction, emission);
        }

        case mir::Instruction::Kind::kDivCheck: {
            return EmitDivCheckCall(instruction, emission);
        }

        case mir::Instruction::Kind::kShiftCheck: {
            return EmitShiftCheckCall(instruction, emission);
        }

        case mir::Instruction::Kind::kUnary: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }

            ExecutableValue operand;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "unary",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
                return false;
            }

            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      ExecutableFunctionBlockContext("unary", state, block),
                                      type_info)) {
                return false;
            }

            const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
            if (instruction.op == "*") {
                if (operand.type.backend_name != "ptr") {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission requires pointer operand for unary '*' in function '" +
                                           state.function->name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }
                output_lines.push_back(temp + " = load " + type_info.backend_name + ", ptr " + operand.text + ", align " +
                                       std::to_string(type_info.alignment));
            } else if (instruction.op == "-") {
                if (IsFloatType(type_info)) {
                    output_lines.push_back(temp + " = fsub " + type_info.backend_name + " 0.0, " + operand.text);
                } else {
                    output_lines.push_back(temp + " = sub " + type_info.backend_name + " 0, " + operand.text);
                }
            } else if (instruction.op == "!") {
                output_lines.push_back(temp + " = xor i1 " + operand.text + ", true");
            } else if (instruction.op == "~") {
                output_lines.push_back(temp + " = xor " + type_info.backend_name + " " + operand.text + ", -1");
            } else {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission does not support unary operator '" + instruction.op +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            RecordExecutableValue(state, instruction.result, temp, type_info, std::nullopt, instruction.type);
            return true;
        }

        case mir::Instruction::Kind::kBinary: {
            return EmitBinaryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kSymbolRef: {
            return EmitSymbolRefInstruction(instruction, emission, function_symbols);
        }

        case mir::Instruction::Kind::kCall: {
            return EmitCallInstruction(instruction, emission, function_symbols);
        }

        case mir::Instruction::Kind::kStoreTarget: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kConvertDistinct: {
            return EmitConvertDistinctInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kConvert: {
            return EmitConvertInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kConvertNumeric: {
            return EmitConvertNumericInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kPointerToInt: {
            return EmitPointerToIntInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kIntToPointer: {
            return EmitIntToPointerInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kVolatileLoad: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            ExecutableValue ptr;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "volatile_load",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, ptr)) {
                return false;
            }
            if (ptr.type.backend_name != "ptr") {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires pointer operand for volatile_load in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            BackendTypeInfo result_type;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      ExecutableFunctionBlockContext("volatile_load", state, block),
                                      result_type)) {
                return false;
            }
            if (!IsAtomicScalarType(result_type)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission only supports scalar volatile_load element types in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
            output_lines.push_back(temp + " = load volatile " + result_type.backend_name + ", ptr " + ptr.text + ", align " +
                                   std::to_string(result_type.alignment));
            RecordExecutableValue(state, instruction.result, temp, result_type);
            return true;
        }

        case mir::Instruction::Kind::kMemoryBarrier: {
            if (!RequireOperandCount(instruction,
                                     0,
                                     "LLVM bootstrap executable emission",
                                     "memory_barrier",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics)) {
                return false;
            }
            output_lines.push_back("fence seq_cst");
            return true;
        }

        case mir::Instruction::Kind::kVolatileStore: {
            ExecutableValue ptr;
            ExecutableValue value;
            if (!RequireOperandCount(instruction,
                                     2,
                                     "LLVM bootstrap executable emission",
                                     "volatile_store",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableOperands(instruction, block, source_path, diagnostics, state, ptr, value)) {
                return false;
            }
            if (ptr.type.backend_name != "ptr") {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires pointer operand for volatile_store in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            if (!IsAtomicScalarType(value.type)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission only supports scalar volatile_store element types in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            output_lines.push_back("store volatile " + value.type.backend_name + " " + value.text + ", ptr " + ptr.text +
                                   ", align " + std::to_string(value.type.alignment));
            return true;
        }

        case mir::Instruction::Kind::kAtomicLoad:
        case mir::Instruction::Kind::kAtomicStore:
        case mir::Instruction::Kind::kAtomicExchange:
        case mir::Instruction::Kind::kAtomicFetchAdd:
            return executable_atomic::RenderAtomicInstruction(instruction, emission);

        case mir::Instruction::Kind::kAtomicCompareExchange:
            MC_UNREACHABLE("unsupported executable instruction should be rejected before lowering");

        case mir::Instruction::Kind::kArrayToSlice: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kBufferToSlice: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kBufferNew: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kArenaNew: {
            return EmitArenaNewInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kBufferFree: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kSliceFromBuffer:
            ReportBackendError(source_path,
                               "slice_from_buffer should lower before executable emission in function '" + state.function->name +
                                   "' block '" + block.label + "'",
                               diagnostics);
            return false;

        case mir::Instruction::Kind::kField: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kIndex: {
            return EmitMemoryInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kSlice: {
            return EmitSliceInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kAggregateInit: {
            return EmitAggregateInitInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kVariantInit: {
            return EmitVariantInitInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kVariantMatch: {
            return EmitVariantMatchInstruction(instruction, emission);
        }

        case mir::Instruction::Kind::kVariantExtract: {
            return EmitVariantExtractInstruction(instruction, emission);
        }
    }

    MC_UNREACHABLE("unhandled mir::Instruction::Kind in executable backend lowering");
}

bool RenderExecutableTerminator(const mir::BasicBlock& block,
                                const std::filesystem::path& source_path,
                                support::DiagnosticSink& diagnostics,
                                const ExecutableFunctionState& state,
                                std::vector<std::string>& output_lines) {
    switch (block.terminator.kind) {
        case mir::Terminator::Kind::kReturn: {
            if (block.terminator.values.size() != state.signature.types.return_types.size()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission return value count does not match function signature in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            if (block.terminator.values.empty()) {
                output_lines.push_back("ret void");
                return true;
            }

            std::optional<BackendTypeInfo> return_type;
            if (!executable_support::LowerExecutableReturnType(*state.module,
                                           state.signature.types,
                                           source_path,
                                           diagnostics,
                                           "return type for function '" + state.function->name + "' block '" + block.label + "'",
                                           return_type)) {
                return false;
            }

            if (state.signature.types.return_types.size() == 1) {
                ExecutableValue value;
                if (!ResolveExecutableValue(state,
                                            block.terminator.values.front(),
                                            block,
                                            source_path,
                                            diagnostics,
                                            value)) {
                    return false;
                }
                output_lines.push_back("ret " + return_type->backend_name + " " + value.text);
                return true;
            }

            std::string current_value = LLVMStructInsertBase(*return_type);
            for (std::size_t index = 0; index < block.terminator.values.size(); ++index) {
                ExecutableValue value;
                if (!ResolveExecutableValue(state,
                                            block.terminator.values[index],
                                            block,
                                            source_path,
                                            diagnostics,
                                            value)) {
                    return false;
                }

                BackendTypeInfo field_type;
                if (!LowerInstructionType(*state.module,
                                          state.signature.types.return_types[index],
                                          source_path,
                                          diagnostics,
                                          "return field in executable emission for function '" + state.function->name + "' block '" +
                                              block.label + "'",
                                          field_type)) {
                    return false;
                }

                const std::string next_value = "%ret." + block.label + "." + std::to_string(index);
                output_lines.push_back(next_value + " = insertvalue " + return_type->backend_name + " " + current_value + ", " +
                                       field_type.backend_name + " " + value.text + ", " + std::to_string(index));
                current_value = next_value;
            }

            output_lines.push_back("ret " + return_type->backend_name + " " + current_value);
            return true;
        }

        case mir::Terminator::Kind::kPanic: {
            if (block.terminator.values.size() != 1) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires panic terminators to use exactly one fault code value in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            ExecutableValue code;
            if (!ResolveExecutableValue(state,
                                        block.terminator.values.front(),
                                        block,
                                        source_path,
                                        diagnostics,
                                        code)) {
                return false;
            }

            if (code.type.backend_name == "i64") {
                output_lines.push_back("call void @__mc_panic(i64 " + code.text + ")");
            } else {
                const std::string widened_code = "%panic.code." + block.label;
                output_lines.push_back(widened_code + " = zext " + code.type.backend_name + " " + code.text + " to i64");
                output_lines.push_back("call void @__mc_panic(i64 " + widened_code + ")");
            }
            output_lines.push_back("unreachable");
            return true;
        }

        case mir::Terminator::Kind::kBranch: {
            std::string label;
            if (!ResolveExecutableBlock(state, block.terminator.true_target, block, source_path, diagnostics, label)) {
                return false;
            }
            output_lines.push_back("br label %" + label);
            return true;
        }

        case mir::Terminator::Kind::kCondBranch: {
            if (block.terminator.values.size() != 1) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires cond_branch to use exactly one condition value in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            ExecutableValue condition;
            if (!ResolveExecutableValue(state,
                                        block.terminator.values.front(),
                                        block,
                                        source_path,
                                        diagnostics,
                                        condition)) {
                return false;
            }
            if (condition.type.backend_name != "i1") {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires i1 branch conditions in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            std::string true_label;
            std::string false_label;
            if (!ResolveExecutableBlock(state, block.terminator.true_target, block, source_path, diagnostics, true_label) ||
                !ResolveExecutableBlock(state, block.terminator.false_target, block, source_path, diagnostics, false_label)) {
                return false;
            }

            output_lines.push_back("br i1 " + condition.text + ", label %" + true_label + ", label %" + false_label);
            return true;
        }

        case mir::Terminator::Kind::kNone:
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission requires explicit MIR terminators in function '" +
                                   state.function->name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
    }

    return false;
}

bool RenderExecutableFunction(const mir::Module& module,
                              const mir::Function& function,
                              std::size_t function_index,
                              bool wrap_hosted_main,
                              const std::unordered_map<std::string, std::string>& function_symbols,
                              const ExecutableGlobals& globals,
                              const std::filesystem::path& source_path,
                              support::DiagnosticSink& diagnostics,
                              std::ostringstream& stream) {
    const ExecutableFunctionSignature function_signature = executable_support::NormalizeFunctionSignature(function);
    std::vector<BackendTypeInfo> param_types;
    if (!executable_support::LowerExecutableParamTypes(module,
                                   function_signature.types,
                                   source_path,
                                   diagnostics,
                                   [&](std::size_t index) {
                                       return "parameter '" + function_signature.params[index]->name +
                                              "' for executable emission in function '" + function.name + "'";
                                   },
                                   param_types)) {
        return false;
    }

    std::unordered_map<std::string, ExecutableStringConstant> string_constants;
    if (!executable_support::CollectExecutableStringConstants(module,
                                          function,
                                          function_index,
                                          source_path,
                                          diagnostics,
                                          stream,
                                          string_constants)) {
        return false;
    }

    std::optional<BackendTypeInfo> return_type;
    if (!executable_support::LowerExecutableReturnType(module,
                                   function_signature.types,
                                   source_path,
                                   diagnostics,
                                   "return type for executable emission in function '" + function.name + "'",
                                   return_type)) {
        return false;
    }

    stream << "define " << (return_type.has_value() ? return_type->backend_name : std::string("void")) << " "
           << LLVMFunctionSymbol(function.name, wrap_hosted_main) << "(";
    executable_support::RenderExecutableParameterList(function_signature, param_types, true, stream);
    stream << ") {\n";
    stream << "entry.alloca:\n";

    std::vector<BackendTypeInfo> local_types;
    if (!executable_support::LowerExecutableLocalTypes(module,
                                   function,
                                   source_path,
                                   diagnostics,
                                   local_types)) {
        return false;
    }

    ExecutableFunctionState state = executable_support::InitializeExecutableFunctionState(
        module,
        function,
        function_signature,
        function_index,
        globals,
        std::move(string_constants));

    executable_support::RenderExecutableEntryPrologue(function, function_signature, local_types, param_types, stream);
    if (function.blocks.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires at least one basic block in function '" +
                               function.name + "'",
                           diagnostics);
        return false;
    }
    stream << "  br label %" << LLVMBlockLabel(function_index, 0, function.blocks.front().label) << "\n\n";

    if (!RenderExecutableBlocks(function,
                                function_index,
                                function_symbols,
                                source_path,
                                diagnostics,
                                state,
                                stream)) {
        return false;
    }

    stream << "}\n\n";
    return true;
}

bool IsCanonicalHostedMainParam(const mir::Local& local) {
    return local.type.kind == sema::Type::Kind::kNamed && local.type.name == "Slice" && local.type.subtypes.size() == 1 &&
           local.type.subtypes.front().kind == sema::Type::Kind::kString;
}

bool RenderExternFunctionDeclaration(const mir::Module& module,
                                     const mir::Function& function,
                                     bool wrap_hosted_main,
                                     const std::filesystem::path& source_path,
                                     support::DiagnosticSink& diagnostics,
                                     std::ostringstream& stream) {
    if (!function.is_extern) {
        return true;
    }
    if (!ShouldEmitFunctionForExecutable(module, function)) {
        return true;
    }
    if (!function.extern_abi.empty() && function.extern_abi != "c") {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission only supports extern(c) functions; function '" +
                               function.name + "' uses abi '" + function.extern_abi + "'",
                           diagnostics);
        return false;
    }

    const ExecutableFunctionSignature function_signature = executable_support::NormalizeFunctionSignature(function);
    std::vector<BackendTypeInfo> param_types;
    if (!executable_support::LowerExecutableParamTypes(module,
                                   function_signature.types,
                                   source_path,
                                   diagnostics,
                                   [&](std::size_t index) {
                                       return "extern parameter '" + function_signature.params[index]->name + "' for '" + function.name + "'";
                                   },
                                   param_types)) {
        return false;
    }

    std::string return_backend_name = "void";
    std::optional<BackendTypeInfo> return_type;
    if (!executable_support::LowerExecutableReturnType(module,
                                   function_signature.types,
                                   source_path,
                                   diagnostics,
                                   "extern return type for '" + function.name + "'",
                                   return_type)) {
        return false;
    }
    if (return_type.has_value()) {
        return_backend_name = return_type->backend_name;
    }

    stream << "declare " << return_backend_name << " " << LLVMFunctionSymbol(function.name, wrap_hosted_main) << "(";
    executable_support::RenderExecutableParameterList(function_signature, param_types, false, stream);
    stream << ")\n";
    return true;
}

bool RenderHostedEntryWrapper(const mir::Module& module,
                              const mir::Function& main_function,
                              bool wrap_hosted_main,
                              const std::filesystem::path& source_path,
                              support::DiagnosticSink& diagnostics,
                              std::ostringstream& stream) {
    if (!wrap_hosted_main) {
        return true;
    }

    const ExecutableFunctionSignature main_signature = executable_support::NormalizeFunctionSignature(main_function);
    if (main_signature.params.size() > 1) {
        ReportBackendError(source_path,
                           "LLVM bootstrap hosted entry only supports 'main' with zero parameters or one Slice<cstr>-shaped parameter",
                           diagnostics);
        return false;
    }
    if (main_signature.params.size() == 1 && !IsCanonicalHostedMainParam(*main_signature.params.front())) {
        ReportBackendError(source_path,
                           "LLVM bootstrap hosted entry only supports 'main(args: Slice<cstr>)' in the current executable slice",
                           diagnostics);
        return false;
    }
    if (main_signature.types.return_types.size() > 1) {
        ReportBackendError(source_path,
                           "LLVM bootstrap hosted entry only supports void or i32 returns from 'main'",
                           diagnostics);
        return false;
    }

    std::optional<BackendTypeInfo> return_type;
    if (!executable_support::LowerExecutableReturnType(module,
                                   main_signature.types,
                                   source_path,
                                   diagnostics,
                                   "hosted entry wrapper return type for 'main'",
                                   return_type)) {
        return false;
    }

    const bool returns_i32 = return_type.has_value();
    if (return_type.has_value() && return_type->backend_name != "i32") {
            ReportBackendError(source_path,
                               "LLVM bootstrap hosted entry only supports void or i32 returns from 'main'",
                               diagnostics);
            return false;
    }

    stream << "define i32 @__mc_hosted_entry({ptr, i64} %args) {\n";
    stream << "entry:\n";
    if (main_signature.params.empty()) {
        if (returns_i32) {
            stream << "  %main.result = call i32 " << LLVMFunctionSymbol(main_function.name, true) << "()\n";
            stream << "  ret i32 %main.result\n";
        } else {
            stream << "  call void " << LLVMFunctionSymbol(main_function.name, true) << "()\n";
            stream << "  ret i32 0\n";
        }
    } else {
        if (returns_i32) {
            stream << "  %main.result = call i32 " << LLVMFunctionSymbol(main_function.name, true) << "({ptr, i64} %args)\n";
            stream << "  ret i32 %main.result\n";
        } else {
            stream << "  call void " << LLVMFunctionSymbol(main_function.name, true) << "({ptr, i64} %args)\n";
            stream << "  ret i32 0\n";
        }
    }
    stream << "}\n\n";
    return true;
}

bool RenderLlvmModuleImpl(const mir::Module& module,
                          const TargetConfig& target,
                          const std::filesystem::path& source_path,
                          bool wrap_hosted_main,
                          support::DiagnosticSink& diagnostics,
                          std::string& llvm_ir) {
    if (wrap_hosted_main && !target.hosted) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission cannot synthesize a hosted entry wrapper for a freestanding target",
                           diagnostics);
        return false;
    }

    const mir::Function* main_function = FindFunction(module, "main");
    if (wrap_hosted_main && main_function == nullptr) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires a root 'main' function",
                           diagnostics);
        return false;
    }

    std::unordered_map<std::string, std::string> function_symbols;
    function_symbols.reserve(module.functions.size());
    for (const auto& function : module.functions) {
        if (!ShouldEmitFunctionForExecutable(module, function)) {
            continue;
        }
        function_symbols.emplace(function.name, LLVMFunctionSymbol(function.name, wrap_hosted_main));
    }

    CheckedHelperRequirements checked_helpers;
    if (!CollectCheckedHelperRequirements(module, source_path, diagnostics, checked_helpers)) {
        return false;
    }

    ExecutableGlobals globals;
    if (!CollectExecutableGlobals(module, source_path, diagnostics, globals)) {
        return false;
    }

    const ExecutableRuntimeRequirements runtime_requirements = CollectExecutableRuntimeRequirements(module);

    std::ostringstream stream;
    stream << "source_filename = \"" << EscapeLLVMQuotedString(source_path.filename().generic_string()) << "\"\n";
    stream << "target triple = \"" << target.triple << "\"\n\n";
    if (target.hosted) {
        stream << "declare void @exit(i32)\n";
    }
    if (runtime_requirements.needs_malloc) {
        stream << "declare ptr @malloc(i64)\n";
    }
    if (runtime_requirements.needs_free) {
        stream << "declare void @free(ptr)\n";
    }
    if (target.hosted || runtime_requirements.needs_malloc || runtime_requirements.needs_free) {
        stream << "\n";
    }
    stream << "define private void @__mc_trap() {\n";
    stream << "entry:\n";
    if (target.hosted) {
        stream << "  call void @exit(i32 134)\n";
    }
    stream << "  unreachable\n";
    stream << "}\n\n";
    if (runtime_requirements.needs_panic) {
        stream << "define private void @__mc_panic(i64 %code) {\n";
        stream << "entry:\n";
        if (target.hosted) {
            stream << "  %exit.code = trunc i64 %code to i32\n";
            stream << "  call void @exit(i32 %exit.code)\n";
        }
        stream << "  call void @__mc_trap()\n";
        stream << "  unreachable\n";
        stream << "}\n\n";
    }
    stream << "define private void @__mc_check_bounds_index(i64 %index, i64 %len) {\n";
    stream << "entry:\n";
    stream << "  %neg = icmp slt i64 %index, 0\n";
    stream << "  br i1 %neg, label %trap, label %check\n";
    stream << "check:\n";
    stream << "  %oob = icmp uge i64 %index, %len\n";
    stream << "  br i1 %oob, label %trap, label %ok\n";
    stream << "trap:\n";
    stream << "  call void @__mc_trap()\n";
    stream << "  unreachable\n";
    stream << "ok:\n";
    stream << "  ret void\n";
    stream << "}\n\n";
    stream << "define private void @__mc_check_bounds_slice(i64 %lower, i64 %upper, i64 %len) {\n";
    stream << "entry:\n";
    stream << "  %lower.neg = icmp slt i64 %lower, 0\n";
    stream << "  br i1 %lower.neg, label %trap, label %check.upper.neg\n";
    stream << "check.upper.neg:\n";
    stream << "  %upper.neg = icmp slt i64 %upper, 0\n";
    stream << "  br i1 %upper.neg, label %trap, label %check.order\n";
    stream << "check.order:\n";
    stream << "  %bad.order = icmp ugt i64 %lower, %upper\n";
    stream << "  br i1 %bad.order, label %trap, label %check.len\n";
    stream << "check.len:\n";
    stream << "  %bad.len = icmp ugt i64 %upper, %len\n";
    stream << "  br i1 %bad.len, label %trap, label %ok\n";
    stream << "trap:\n";
    stream << "  call void @__mc_trap()\n";
    stream << "  unreachable\n";
    stream << "ok:\n";
    stream << "  ret void\n";
    stream << "}\n\n";
    for (const auto& backend_name : checked_helpers.div_backend_names) {
        EmitDivCheckHelperDefinition(stream, backend_name);
    }
    for (const auto bit_width : checked_helpers.shift_widths) {
        EmitShiftCheckHelperDefinition(stream, bit_width);
    }

    bool emitted_extern_declaration = false;
    for (const auto& function : module.functions) {
        if (!function.is_extern || !ShouldEmitFunctionForExecutable(module, function)) {
            continue;
        }
        if (!RenderExternFunctionDeclaration(module, function, wrap_hosted_main, source_path, diagnostics, stream)) {
            return false;
        }
        emitted_extern_declaration = true;
    }
    if (emitted_extern_declaration) {
        stream << "\n";
    }

    for (const auto& global : module.globals) {
        BackendTypeInfo global_type;
        if (!LowerInstructionType(module,
                                  global.type,
                                  source_path,
                                  diagnostics,
                                  "global executable emission",
                                  global_type)) {
            return false;
        }
        for (std::size_t index = 0; index < global.names.size(); ++index) {
            const auto& name = global.names[index];
            if (global.is_extern) {
                stream << LLVMGlobalName(name) << " = ";
                stream << "external " << (global.is_const ? "constant " : "global ")
                       << global_type.backend_name << ", align " << global_type.alignment << "\n";
                continue;
            }

            std::string init_value = LLVMStructInsertBase(global_type);
            if (index < global.constant_values.size() && global.constant_values[index].has_value()) {
                const sema::Type canonical_type = sema::CanonicalizeBuiltinType(mir::StripMirAliasOrDistinct(module, global.type));
                if (canonical_type.kind == sema::Type::Kind::kString &&
                    global.constant_values[index]->kind == sema::ConstValue::Kind::kString) {
                    std::ostringstream prelude;
                    if (!RenderLLVMGlobalStringConstValue(module,
                                                          global.type,
                                                          *global.constant_values[index],
                                                          LLVMGlobalName(name) + ".bytes",
                                                          source_path,
                                                          diagnostics,
                                                          prelude,
                                                          init_value)) {
                        return false;
                    }
                    stream << prelude.str();
                } else if (!RenderLLVMGlobalConstValue(module,
                                                       global.type,
                                                       *global.constant_values[index],
                                                       wrap_hosted_main,
                                                       source_path,
                                                       diagnostics,
                                                       init_value)) {
                    return false;
                }
            } else if (index < global.initializers.size()) {
                init_value = FormatLLVMLiteral(global_type, global.initializers[index]);
            }
            stream << LLVMGlobalName(name) << " = ";
            stream << (global.is_const ? "constant " : "global ")
                   << global_type.backend_name << " " << init_value << ", align " << global_type.alignment << "\n";
        }
    }
    if (!module.globals.empty()) {
        stream << "\n";
    }

    for (std::size_t function_index = 0; function_index < module.functions.size(); ++function_index) {
        if (module.functions[function_index].is_extern || !ShouldEmitFunctionForExecutable(module, module.functions[function_index])) {
            continue;
        }
        if (!RenderExecutableFunction(module,
                                      module.functions[function_index],
                                      function_index,
                                      wrap_hosted_main,
                                      function_symbols,
                                      globals,
                                      source_path,
                                      diagnostics,
                                      stream)) {
            return false;
        }
    }

    if (wrap_hosted_main && !RenderHostedEntryWrapper(module, *main_function, wrap_hosted_main, source_path, diagnostics, stream)) {
        return false;
    }

    llvm_ir = stream.str();
    return true;
}

}  // namespace

bool ValidateBootstrapTarget(const TargetConfig& target,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics) {
    if (IsBootstrapTarget(target)) {
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap backend only supports bootstrap 'arm64-apple-darwin' targets in Stage 3; got triple='" +
                           target.triple + "' target_family='" + target.target_family + "'",
                       diagnostics);
    return false;
}

bool ValidateExecutableBackendCapabilities(const mir::Module& module,
                                          const TargetConfig& target,
                                          const std::filesystem::path& source_path,
                                          support::DiagnosticSink& diagnostics) {
    if (!IsBootstrapTarget(target)) {
        return true;
    }

    for (const auto& function : module.functions) {
        if (!function.is_extern && !function.type_params.empty() &&
            !FunctionSupportsErasedGenericEmission(module, function)) {
            ReportBackendError(source_path,
                               "generic function '" + function.name +
                                   "' uses type parameters in ABI-incompatible positions; "
                                   "monomorphized generic functions are not yet supported by the bootstrap backend",
                               diagnostics);
            return false;
        }

        for (const auto& block : function.blocks) {
            for (const auto& instruction : block.instructions) {
                const auto unsupported = UnsupportedExecutableInstructionName(instruction.kind);
                if (!unsupported.has_value()) {
                    continue;
                }

                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission does not yet support MIR instruction '" + std::string(*unsupported) +
                                       "' before LLVM IR emission in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
        }
    }

    return true;
}

bool RenderLlvmModule(const mir::Module& module,
                      const TargetConfig& target,
                      const std::filesystem::path& source_path,
                      bool wrap_hosted_main,
                      support::DiagnosticSink& diagnostics,
                      std::string& llvm_ir) {
    return RenderLlvmModuleImpl(module, target, source_path, wrap_hosted_main, diagnostics, llvm_ir);
}

TargetConfig BootstrapTargetConfig() {
    return {
        .triple = std::string(kBootstrapTriple),
        .target_family = std::string(kBootstrapTargetFamily),
        .object_format = std::string(kBootstrapObjectFormat),
        .host_tool_prefix = {"xcrun"},
        .hosted = true,
    };
}

LowerResult LowerModule(const mir::Module& module,
                        const std::filesystem::path& source_path,
                        const LowerOptions& options,
                        support::DiagnosticSink& diagnostics) {
    if (!ValidateBootstrapTarget(options.target, source_path, diagnostics)) {
        return {};
    }

    return LowerInspectModule(module, source_path, options.target, diagnostics);
}

ObjectBuildResult BuildObjectFile(const mir::Module& module,
                                  const std::filesystem::path& source_path,
                                  const ObjectBuildOptions& options,
                                  support::DiagnosticSink& diagnostics);

LinkResult LinkExecutable(const std::filesystem::path& source_path,
                          const LinkOptions& options,
                          support::DiagnosticSink& diagnostics);

ArchiveResult ArchiveStaticLibrary(const std::filesystem::path& source_path,
                                   const ArchiveOptions& options,
                                   support::DiagnosticSink& diagnostics);

BuildResult BuildExecutable(const mir::Module& module,
                            const std::filesystem::path& source_path,
                            const BuildOptions& options,
                            support::DiagnosticSink& diagnostics);

std::string FunctionBlockContext(std::string_view operation,
                                 std::string_view function_name,
                                 const mir::BasicBlock& block) {
    return std::string(operation) + " in function '" + std::string(function_name) + "' block '" + block.label + "'";
}

std::string ExecutableFunctionBlockContext(std::string_view operation,
                                           const ExecutableFunctionState& state,
                                           const mir::BasicBlock& block) {
    return std::string(operation) + " in executable emission for function '" + state.function->name + "' block '" +
           block.label + "'";
}

bool RequireOperandRange(const mir::Instruction& instruction,
                         std::size_t minimum_count,
                         std::size_t maximum_count,
                         std::string_view backend_name,
                         std::string_view requirement,
                         std::string_view function_name,
                         const mir::BasicBlock& block,
                         const std::filesystem::path& source_path,
                         support::DiagnosticSink& diagnostics) {
    if (instruction.operands.size() >= minimum_count && instruction.operands.size() <= maximum_count) {
        return true;
    }

    ReportBackendError(source_path,
                       std::string(backend_name) + " requires " + std::string(requirement) + " in function '" +
                           std::string(function_name) + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

std::string EmitSliceLengthValue(std::size_t function_index,
                                 std::size_t block_index,
                                 std::size_t instruction_index,
                                 std::string_view suffix,
                                 std::string_view lower_i64,
                                 const std::optional<std::string>& upper_i64,
                                 std::string_view fallback_upper_bound,
                                 std::vector<std::string>& output_lines) {
    const std::string len_value = LLVMTempName(function_index, block_index, instruction_index) + "." +
                                  std::string(suffix);
    if (upper_i64.has_value()) {
        output_lines.push_back(len_value + " = sub i64 " + *upper_i64 + ", " + std::string(lower_i64));
    } else {
        output_lines.push_back(len_value + " = sub i64 " + std::string(fallback_upper_bound) + ", " +
                               std::string(lower_i64));
    }
    return len_value;
}

std::string EmitAggregateStackSlot(const ExecutableValue& value,
                                   std::size_t function_index,
                                   std::size_t block_index,
                                   std::size_t instruction_index,
                                   std::string_view suffix,
                                   std::vector<std::string>& output_lines) {
    const std::string slot_name = LLVMTempName(function_index, block_index, instruction_index) + "." +
                                  std::string(suffix);
    output_lines.push_back(slot_name + " = alloca " + value.type.backend_name + ", align " +
                           std::to_string(value.type.alignment));
    output_lines.push_back("store " + value.type.backend_name + " " + value.text + ", ptr " + slot_name + ", align " +
                           std::to_string(value.type.alignment));
    return slot_name;
}

}  // namespace mc::codegen_llvm