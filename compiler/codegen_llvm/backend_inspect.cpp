#include "compiler/codegen_llvm/backend_internal.h"

#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "compiler/support/assert.h"

namespace mc::codegen_llvm {

namespace {

constexpr std::string_view kInspectSurface = "backend_ir_text";

bool LowerInstruction(const mir::Instruction& instruction,
                      std::size_t block_index,
                      std::size_t instruction_index,
                      const std::filesystem::path& source_path,
                      support::DiagnosticSink& diagnostics,
                      FunctionLoweringState& state,
                      BackendBlock& backend_block) {
    const mir::Function& function = *state.function;
    const mir::BasicBlock& block = function.blocks[block_index];

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
                                      "const in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = const " + FormatTypeInfo(type_info) + " " +
                                                 instruction.op);
            return true;
        }

        case mir::Instruction::Kind::kLocalAddr: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "local_addr in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            if (instruction.target_kind == mir::Instruction::TargetKind::kGlobal) {
                if (instruction.target_name.empty()) {
                    ReportBackendError(source_path,
                                       "local_addr global target is missing target_name metadata in function '" + function.name +
                                           "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }
                backend_block.instructions.push_back(backend_name + " = local_addr " + FormatTypeInfo(type_info) + " " +
                                                     BackendGlobalName(instruction.target_name));
                return true;
            }
            const BackendLocal* local = nullptr;
            if (!ResolveLocal(state, instruction.target, function, block, source_path, diagnostics, local)) {
                return false;
            }
            backend_block.instructions.push_back(backend_name + " = local_addr " + FormatTypeInfo(type_info) + " " +
                                                 local->backend_name);
            return true;
        }

        case mir::Instruction::Kind::kLoadLocal: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            const BackendLocal* local = nullptr;
            if (!ResolveLocal(state, instruction.target, function, block, source_path, diagnostics, local)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, local->lowered_type);
            backend_block.instructions.push_back(backend_name + " = load " + FormatTypeInfo(local->lowered_type) +
                                                 " " + local->backend_name);
            return true;
        }

        case mir::Instruction::Kind::kStoreLocal: {
            const BackendLocal* local = nullptr;
            if (!ResolveLocal(state, instruction.target, function, block, source_path, diagnostics, local)) {
                return false;
            }
            std::string operand;
            if (!ResolveSingleOperand(state,
                                      instruction,
                                      "LLVM bootstrap backend",
                                      "store_local",
                                      function,
                                      block,
                                      source_path,
                                      diagnostics,
                                      operand)) {
                return false;
            }
            backend_block.instructions.push_back("store " + sema::FormatType(local->type) + " " + operand + " -> " +
                                                 local->backend_name + " as " + FormatTypeInfo(local->lowered_type));
            return true;
        }

        case mir::Instruction::Kind::kUnary: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::string operand;
            if (!ResolveSingleOperand(state,
                                      instruction,
                                      "LLVM bootstrap backend",
                                      "unary",
                                      function,
                                      block,
                                      source_path,
                                      diagnostics,
                                      operand)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "unary in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = unary " + instruction.op + " " +
                                                 FormatTypeInfo(type_info) + " " + operand);
            return true;
        }

        case mir::Instruction::Kind::kBinary: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::string lhs;
            std::string rhs;
            if (!ResolveTwoOperands(state,
                                    instruction,
                                    "LLVM bootstrap backend",
                                    "binary",
                                    function,
                                    block,
                                    source_path,
                                    diagnostics,
                                    lhs,
                                    rhs)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "binary in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);

            std::ostringstream line;
            line << backend_name << " = binary ";
            if (instruction.arithmetic_semantics != mir::Instruction::ArithmeticSemantics::kNone) {
                line << ToString(instruction.arithmetic_semantics) << ' ';
            }
            line << instruction.op << ' ' << FormatTypeInfo(type_info) << ' ' << lhs << ", " << rhs;
            backend_block.instructions.push_back(line.str());
            return true;
        }

        case mir::Instruction::Kind::kSymbolRef: {
            const bool function_or_global = instruction.target_kind == mir::Instruction::TargetKind::kFunction ||
                                            instruction.target_kind == mir::Instruction::TargetKind::kGlobal;
            const bool enum_constant = instruction.target_kind == mir::Instruction::TargetKind::kOther &&
                                       instruction.type.kind == sema::Type::Kind::kNamed &&
                                       !instruction.target_name.empty();
            if ((!function_or_global && !enum_constant) || instruction.target_name.empty()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend only admits function and global symbol_ref values in the current bootstrap slice; function '" +
                                       function.name + "' block '" + block.label + "' uses target_kind='" +
                                       std::string(ToString(instruction.target_kind)) + "'",
                                   diagnostics);
                return false;
            }
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "symbol_ref in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            if (enum_constant) {
                const mir::TypeDecl* type_decl = FindTypeDecl(*state.module, instruction.type.name);
                if (type_decl == nullptr || type_decl->kind != mir::TypeDecl::Kind::kEnum) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap backend could not resolve enum symbol_ref type '" +
                                           sema::FormatType(instruction.type) + "' in function '" + function.name +
                                           "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }
                std::size_t variant_index = 0;
                const mir::VariantDecl* variant_decl = FindVariantDecl(*type_decl, instruction.target_name, &variant_index);
                if (variant_decl == nullptr || !variant_decl->payload_fields.empty()) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap backend only supports payload-free enum symbol_ref values in function '" +
                                           function.name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }
                backend_block.instructions.push_back(backend_name + " = enum_const " + FormatTypeInfo(type_info) + " " +
                                                     instruction.target_name + " tag=" + std::to_string(variant_index));
                return true;
            }
            backend_block.instructions.push_back(backend_name + " = symbol_ref " + FormatTypeInfo(type_info) + " " +
                                                 (instruction.target_kind == mir::Instruction::TargetKind::kFunction
                                                      ? BackendFunctionName(instruction.target_name)
                                                      : BackendGlobalName(instruction.target_name)));
            return true;
        }

        case mir::Instruction::Kind::kCall: {
            if (instruction.target_kind != mir::Instruction::TargetKind::kFunction || instruction.target_name.empty()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend only admits direct function calls in the current bootstrap slice; function '" +
                                       function.name + "' block '" + block.label + "' is not a direct call",
                                   diagnostics);
                return false;
            }
            std::vector<std::string> operands;
            if (!ResolveOperands(state,
                                 instruction,
                                 function,
                                 block,
                                 source_path,
                                 diagnostics,
                                 operands)) {
                return false;
            }

            std::ostringstream line;
            if (instruction.result.empty()) {
                line << "call void " << BackendFunctionName(instruction.target_name) << '(' << JoinOperands(operands)
                     << ')';
            } else {
                BackendTypeInfo type_info;
                if (!LowerInstructionType(*state.module,
                                          instruction.type,
                                          source_path,
                                          diagnostics,
                                          "call result in function '" + function.name + "' block '" + block.label + "'",
                                          type_info)) {
                    return false;
                }
                const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
                RecordLoweredValue(state, instruction.result, backend_name, type_info);
                line << backend_name << " = call " << FormatTypeInfo(type_info) << ' '
                     << BackendFunctionName(instruction.target_name) << '(' << JoinOperands(operands) << ')';
            }
            backend_block.instructions.push_back(line.str());
            return true;
        }

        case mir::Instruction::Kind::kBoundsCheck: {
            std::vector<std::string> operands;
            if (!ResolveOperands(state,
                                 instruction,
                                 function,
                                 block,
                                 source_path,
                                 diagnostics,
                                 operands)) {
                return false;
            }
            backend_block.instructions.push_back("check.bounds " + instruction.op + " " + JoinOperands(operands));
            return true;
        }

        case mir::Instruction::Kind::kDivCheck: {
            std::string lhs;
            std::string rhs;
            BackendTypeInfo lhs_type;
            BackendTypeInfo rhs_type;
            if (!ResolveTwoTypedOperands(state,
                                         instruction,
                                         "LLVM bootstrap backend",
                                         "div_check",
                                         function,
                                         block,
                                         source_path,
                                         diagnostics,
                                         lhs,
                                         lhs_type,
                                         rhs,
                                         rhs_type)) {
                return false;
            }
            backend_block.instructions.push_back("check.div " + instruction.op + " lhs=" + lhs + " rhs=" + rhs +
                                                 " type=" + FormatTypeInfo(lhs_type));
            return true;
        }

        case mir::Instruction::Kind::kShiftCheck: {
            std::string value;
            std::string count;
            BackendTypeInfo value_type;
            BackendTypeInfo count_type;
            if (!ResolveTwoTypedOperands(state,
                                         instruction,
                                         "LLVM bootstrap backend",
                                         "shift_check",
                                         function,
                                         block,
                                         source_path,
                                         diagnostics,
                                         value,
                                         value_type,
                                         count,
                                         count_type)) {
                return false;
            }
            backend_block.instructions.push_back("check.shift " + instruction.op + " value=" + value + " count=" + count +
                                                 " type=" + FormatTypeInfo(value_type));
            return true;
        }

        case mir::Instruction::Kind::kConvert: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::string operand;
            if (!ResolveSingleOperand(state,
                                      instruction,
                                      "LLVM bootstrap backend",
                                      "convert",
                                      function,
                                      block,
                                      source_path,
                                      diagnostics,
                                      operand)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "convert in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = convert " + FormatTypeInfo(type_info) + " " + operand);
            return true;
        }

        case mir::Instruction::Kind::kConvertNumeric: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::string operand;
            if (!ResolveSingleOperand(state,
                                      instruction,
                                      "LLVM bootstrap backend",
                                      "convert_numeric",
                                      function,
                                      block,
                                      source_path,
                                      diagnostics,
                                      operand)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "convert_numeric in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = convert_numeric " + FormatTypeInfo(type_info) + " " + operand);
            return true;
        }

        case mir::Instruction::Kind::kConvertDistinct: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::string operand;
            if (!ResolveSingleOperand(state,
                                      instruction,
                                      "LLVM bootstrap backend",
                                      "convert_distinct",
                                      function,
                                      block,
                                      source_path,
                                      diagnostics,
                                      operand)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "convert_distinct in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = convert_distinct " + FormatTypeInfo(type_info) +
                                                 " " + operand);
            return true;
        }

        case mir::Instruction::Kind::kPointerToInt: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::string operand;
            if (!ResolveSingleOperand(state,
                                      instruction,
                                      "LLVM bootstrap backend",
                                      "pointer_to_int",
                                      function,
                                      block,
                                      source_path,
                                      diagnostics,
                                      operand)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "pointer_to_int in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = ptrtoint " + FormatTypeInfo(type_info) + " " +
                                                 operand);
            return true;
        }

        case mir::Instruction::Kind::kIntToPointer: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::string operand;
            if (!ResolveSingleOperand(state,
                                      instruction,
                                      "LLVM bootstrap backend",
                                      "int_to_pointer",
                                      function,
                                      block,
                                      source_path,
                                      diagnostics,
                                      operand)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "int_to_pointer in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = inttoptr " + FormatTypeInfo(type_info) + " " +
                                                 operand);
            return true;
        }

        case mir::Instruction::Kind::kStoreTarget: {
            std::vector<std::string> operands;
            if (!ResolveOperands(state,
                                 instruction,
                                 function,
                                 block,
                                 source_path,
                                 diagnostics,
                                 operands)) {
                return false;
            }
            std::ostringstream rendered;
            for (std::size_t index = 0; index < operands.size(); ++index) {
                if (index > 0) {
                    rendered << ", ";
                }
                rendered << operands[index];
            }
            backend_block.instructions.push_back("store_target " + instruction.target_display + " kind=" +
                                                 std::string(ToString(instruction.target_kind)) + " operands=[" + rendered.str() + "]");
            return true;
        }

        case mir::Instruction::Kind::kArrayToSlice:
        case mir::Instruction::Kind::kBufferToSlice:
        case mir::Instruction::Kind::kBufferNew:
        case mir::Instruction::Kind::kArenaNew:
        case mir::Instruction::Kind::kSliceFromBuffer:
        case mir::Instruction::Kind::kField:
        case mir::Instruction::Kind::kIndex:
        case mir::Instruction::Kind::kSlice:
        case mir::Instruction::Kind::kAggregateInit:
        case mir::Instruction::Kind::kVariantInit:
        case mir::Instruction::Kind::kVariantMatch:
        case mir::Instruction::Kind::kVariantExtract: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::vector<std::string> operands;
            if (!ResolveOperands(state,
                                 instruction,
                                 function,
                                 block,
                                 source_path,
                                 diagnostics,
                                 operands)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      std::string(ToString(instruction.kind)) + " in function '" + function.name +
                                          "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);

            std::ostringstream line;
            line << backend_name << " = " << ToString(instruction.kind) << " " << FormatTypeInfo(type_info);
            if (!instruction.target_display.empty()) {
                line << " target_display=" << instruction.target_display;
            } else if (!instruction.target.empty()) {
                line << " target=" << instruction.target;
            }
            if (!instruction.field_names.empty()) {
                line << " fields=[";
                for (std::size_t index = 0; index < instruction.field_names.size(); ++index) {
                    if (index > 0) {
                        line << ", ";
                    }
                    line << instruction.field_names[index];
                }
                line << "]";
            }
            if (!operands.empty()) {
                line << " operands=[" << JoinOperands(operands) << "]";
            }
            backend_block.instructions.push_back(line.str());
            return true;
        }

        case mir::Instruction::Kind::kBufferFree: {
            std::vector<std::string> operands;
            if (!ResolveOperands(state,
                                 instruction,
                                 function,
                                 block,
                                 source_path,
                                 diagnostics,
                                 operands)) {
                return false;
            }
            std::ostringstream line;
            line << ToString(instruction.kind);
            if (!operands.empty()) {
                line << " operands=[" << JoinOperands(operands) << ']';
            }
            backend_block.instructions.push_back(line.str());
            return true;
        }

        case mir::Instruction::Kind::kVolatileLoad: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::string ptr;
            if (!ResolveSingleOperand(state,
                                      instruction,
                                      "LLVM bootstrap backend",
                                      "volatile_load",
                                      function,
                                      block,
                                      source_path,
                                      diagnostics,
                                      ptr)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "volatile_load in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = volatile_load " + FormatTypeInfo(type_info) + " " + ptr);
            return true;
        }

        case mir::Instruction::Kind::kVolatileStore: {
            std::string ptr;
            BackendTypeInfo ptr_type;
            std::string value;
            BackendTypeInfo value_type;
            if (!ResolveTwoTypedOperands(state,
                                         instruction,
                                         "LLVM bootstrap backend",
                                         "volatile_store",
                                         function,
                                         block,
                                         source_path,
                                         diagnostics,
                                         ptr,
                                         ptr_type,
                                         value,
                                         value_type)) {
                return false;
            }
            backend_block.instructions.push_back("volatile_store " + FormatTypeInfo(value_type) + " " + value + " -> " + ptr);
            return true;
        }

        case mir::Instruction::Kind::kAtomicLoad: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            if (instruction.operands.empty()) {
                return false;
            }
            std::string ptr;
            BackendTypeInfo ptr_type;
            if (!ResolveTypedValue(state,
                                   instruction.operands[0],
                                   function,
                                   block,
                                   source_path,
                                   diagnostics,
                                   ptr,
                                   ptr_type)) {
                return false;
            }
            const auto order = LLVMAtomicOrderKeyword(instruction.atomic_order);
            if (!order.has_value()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires constant MemoryOrder metadata for 'atomic_load' in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "atomic_load in function '" + function.name + "' block '" + block.label + "'",
                                      type_info)) {
                return false;
            }
            if (!IsAtomicScalarType(type_info)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend only supports scalar atomic_load element types in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = atomic_load order=" + instruction.atomic_order + " " +
                                                 FormatTypeInfo(type_info) + " " + ptr);
            return true;
        }

        case mir::Instruction::Kind::kAtomicStore: {
            if (instruction.operands.size() < 2) {
                return false;
            }
            std::string ptr;
            BackendTypeInfo ptr_type;
            std::string value;
            BackendTypeInfo value_type;
            if (!ResolveTypedValue(state,
                                   instruction.operands[0],
                                   function,
                                   block,
                                   source_path,
                                   diagnostics,
                                   ptr,
                                   ptr_type) ||
                !ResolveTypedValue(state,
                                   instruction.operands[1],
                                   function,
                                   block,
                                   source_path,
                                   diagnostics,
                                   value,
                                   value_type)) {
                return false;
            }
            const auto order = LLVMAtomicOrderKeyword(instruction.atomic_order);
            if (!order.has_value()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires constant MemoryOrder metadata for 'atomic_store' in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            if (!IsAtomicScalarType(value_type)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend only supports scalar atomic_store element types in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            backend_block.instructions.push_back("atomic_store order=" + instruction.atomic_order + " " +
                                                 FormatTypeInfo(value_type) + " " + value + " -> " + ptr);
            return true;
        }

        case mir::Instruction::Kind::kAtomicExchange:
        case mir::Instruction::Kind::kAtomicFetchAdd: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            if (instruction.operands.size() < 2) {
                return false;
            }
            std::string ptr;
            BackendTypeInfo ptr_type;
            std::string value;
            BackendTypeInfo value_type;
            if (!ResolveTypedValue(state,
                                   instruction.operands[0],
                                   function,
                                   block,
                                   source_path,
                                   diagnostics,
                                   ptr,
                                   ptr_type) ||
                !ResolveTypedValue(state,
                                   instruction.operands[1],
                                   function,
                                   block,
                                   source_path,
                                   diagnostics,
                                   value,
                                   value_type)) {
                return false;
            }
            const auto order = LLVMAtomicOrderKeyword(instruction.atomic_order);
            if (!order.has_value()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires constant MemoryOrder metadata for '" +
                                       std::string(ToString(instruction.kind)) + "' in function '" + function.name +
                                       "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            if (!IsAtomicScalarType(value_type)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend only supports scalar " + std::string(ToString(instruction.kind)) +
                                       " element types in function '" + function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            const std::string backend_name = BackendTempName(state.function_index, block_index, instruction_index);
            RecordLoweredValue(state, instruction.result, backend_name, value_type);
            backend_block.instructions.push_back(backend_name + " = " + std::string(ToString(instruction.kind)) + " order=" +
                                                 instruction.atomic_order + " " + FormatTypeInfo(value_type) + " " + ptr + ", " + value);
            return true;
        }

        case mir::Instruction::Kind::kAtomicCompareExchange:
            ReportBackendError(source_path,
                               "LLVM bootstrap backend does not yet support MIR instruction 'atomic_compare_exchange' in Stage 3; function '" +
                                   function.name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
    }

    MC_UNREACHABLE("unhandled mir::Instruction::Kind in debug backend lowering");
}

bool LowerTerminator(const mir::BasicBlock& block,
                     const std::filesystem::path& source_path,
                     support::DiagnosticSink& diagnostics,
                     const FunctionLoweringState& state,
                     BackendBlock& backend_block) {
    const mir::Function& function = *state.function;

    switch (block.terminator.kind) {
        case mir::Terminator::Kind::kReturn: {
            if (block.terminator.values.empty()) {
                if (!function.return_types.empty()) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap backend requires return values for non-void function '" +
                                           function.name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }
                backend_block.terminator = "ret void";
                return true;
            }

            if (block.terminator.values.size() != function.return_types.size()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend return value count does not match function signature in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            std::vector<std::string> values;
            values.reserve(block.terminator.values.size());
            for (const auto& return_value : block.terminator.values) {
                std::string value;
                if (!ResolveValue(state,
                                  return_value,
                                  function,
                                  block,
                                  source_path,
                                  diagnostics,
                                  value)) {
                    return false;
                }
                values.push_back(std::move(value));
            }

            const auto lowered_return_type = LowerFunctionReturnType(*state.module, function.return_types);
            if (!lowered_return_type.has_value()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend could not lower return type for function '" + function.name + "' block '" +
                                       block.label + "'",
                                   diagnostics);
                return false;
            }

            if (function.return_types.size() == 1) {
                backend_block.terminator = "ret " + FormatTypeInfo(*lowered_return_type) + " " + values.front();
            } else {
                backend_block.terminator = "ret " + FormatTypeInfo(*lowered_return_type) + " (" + JoinOperands(values) + ")";
            }
            return true;
        }

        case mir::Terminator::Kind::kBranch: {
            std::string target;
            if (!ResolveBlock(state, block.terminator.true_target, function, block, source_path, diagnostics, target)) {
                return false;
            }
            backend_block.terminator = "br " + target;
            return true;
        }

        case mir::Terminator::Kind::kCondBranch: {
            if (block.terminator.values.size() != 1) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires cond_branch to use exactly one condition value in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            std::string condition;
            std::string true_target;
            std::string false_target;
            if (!ResolveValue(state,
                              block.terminator.values.front(),
                              function,
                              block,
                              source_path,
                              diagnostics,
                              condition) ||
                !ResolveBlock(state, block.terminator.true_target, function, block, source_path, diagnostics, true_target) ||
                !ResolveBlock(state, block.terminator.false_target, function, block, source_path, diagnostics, false_target)) {
                return false;
            }

            backend_block.terminator = "condbr " + condition + " true=" + true_target + " false=" + false_target;
            return true;
        }

        case mir::Terminator::Kind::kNone:
            ReportBackendError(source_path,
                               "LLVM bootstrap backend requires explicit MIR terminators; function '" + function.name +
                                   "' block '" + block.label + "' ends with terminator '" +
                                   std::string(ToString(block.terminator.kind)) + "'",
                               diagnostics);
            return false;
    }

    return false;
}

bool LowerFunction(const mir::Module& module,
                   const mir::Function& function,
                   std::size_t function_index,
                   const std::filesystem::path& source_path,
                   support::DiagnosticSink& diagnostics,
                   BackendModule& backend_module) {
    if (function.is_extern || !function.type_params.empty()) {
        return true;
    }

    BackendFunction backend_function;
    backend_function.source_name = function.name;
    backend_function.backend_name = BackendFunctionName(function.name);
    backend_function.return_types = function.return_types;
    backend_function.backend_return_types.reserve(function.return_types.size());
    backend_function.locals.reserve(function.locals.size());
    backend_function.blocks.reserve(function.blocks.size());

    for (const auto& return_type : function.return_types) {
        BackendTypeInfo lowered_return_type;
        if (!LowerInstructionType(module,
                                  return_type,
                                  source_path,
                                  diagnostics,
                                  "function return types for '" + function.name + "'",
                                  lowered_return_type)) {
            return false;
        }
        backend_function.backend_return_types.push_back(std::move(lowered_return_type));
    }

    FunctionLoweringState state;
    state.function_index = function_index;
    state.module = &module;
    state.function = &function;

    for (const auto& local : function.locals) {
        BackendTypeInfo lowered_type;
        if (!LowerInstructionType(module,
                                  local.type,
                                  source_path,
                                  diagnostics,
                                  "local '" + local.name + "' in function '" + function.name + "'",
                                  lowered_type)) {
            return false;
        }
        BackendLocal backend_local {
            .source_name = local.name,
            .backend_name = BackendLocalName(local.name),
            .type = local.type,
            .lowered_type = lowered_type,
            .is_parameter = local.is_parameter,
            .is_mutable = local.is_mutable,
        };
        state.locals.emplace(local.name, backend_local);
        backend_function.locals.push_back(std::move(backend_local));
    }

    for (std::size_t block_index = 0; block_index < function.blocks.size(); ++block_index) {
        state.blocks.emplace(function.blocks[block_index].label,
                             BackendBlockName(function_index, block_index, function.blocks[block_index].label));
    }

    for (std::size_t block_index = 0; block_index < function.blocks.size(); ++block_index) {
        const auto& block = function.blocks[block_index];
        BackendBlock backend_block;
        backend_block.source_label = block.label;
        backend_block.backend_label = BackendBlockName(function_index, block_index, block.label);
        backend_block.instructions.reserve(block.instructions.size());

        for (std::size_t instruction_index = 0; instruction_index < block.instructions.size(); ++instruction_index) {
            if (!LowerInstruction(block.instructions[instruction_index],
                                  block_index,
                                  instruction_index,
                                  source_path,
                                  diagnostics,
                                  state,
                                  backend_block)) {
                return false;
            }
        }

        if (!LowerTerminator(block, source_path, diagnostics, state, backend_block)) {
            return false;
        }

        backend_function.blocks.push_back(std::move(backend_block));
    }

    backend_module.functions.push_back(std::move(backend_function));
    return true;
}

bool LowerGlobal(const mir::Module& module,
                 const mir::GlobalDecl& global,
                 const std::filesystem::path& source_path,
                 support::DiagnosticSink& diagnostics,
                 BackendModule& backend_module) {
    if (global.names.empty()) {
        ReportBackendError(source_path, "LLVM bootstrap backend requires each global declaration to name at least one global", diagnostics);
        return false;
    }

    BackendTypeInfo lowered_type;
    if (!LowerInstructionType(module,
                              global.type,
                              source_path,
                              diagnostics,
                              "global declaration",
                              lowered_type)) {
        return false;
    }

    for (const auto& name : global.names) {
        backend_module.globals.push_back({
            .is_const = global.is_const,
            .is_extern = global.is_extern,
            .source_name = name,
            .backend_name = BackendGlobalName(name),
            .type = global.type,
            .lowered_type = lowered_type,
            .initializers = global.initializers,
        });
    }
    return true;
}

}  // namespace

LowerResult LowerInspectModule(const mir::Module& module,
                               const std::filesystem::path& source_path,
                               const TargetConfig& target,
                               support::DiagnosticSink& diagnostics) {
    auto backend_module = std::make_unique<BackendModule>();
    backend_module->target = target;
    backend_module->inspect_surface = std::string(kInspectSurface);
    backend_module->globals.reserve(module.globals.size());
    backend_module->functions.reserve(module.functions.size());

    for (const auto& global : module.globals) {
        if (!LowerGlobal(module, global, source_path, diagnostics, *backend_module)) {
            return {};
        }
    }

    for (std::size_t function_index = 0; function_index < module.functions.size(); ++function_index) {
        if (!module.functions[function_index].type_params.empty()) {
            continue;
        }
        if (!LowerFunction(module,
                           module.functions[function_index],
                           function_index,
                           source_path,
                           diagnostics,
                           *backend_module)) {
            return {};
        }
    }

    return {
        .module = std::move(backend_module),
        .ok = true,
    };
}

}  // namespace mc::codegen_llvm