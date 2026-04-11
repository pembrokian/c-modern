#ifndef C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_INTERNAL_H_
#define C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_INTERNAL_H_

#include <cstddef>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "compiler/codegen_llvm/backend.h"
#include "compiler/support/assert.h"

namespace mc::codegen_llvm {

bool ValidateBootstrapTarget(const TargetConfig& target,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics);

bool ValidateExecutableBackendCapabilities(const mir::Module& module,
                                          const TargetConfig& target,
                                          const std::filesystem::path& source_path,
                                          support::DiagnosticSink& diagnostics);

void ReportBackendError(const std::filesystem::path& source_path,
                        const std::string& message,
                        support::DiagnosticSink& diagnostics);

const mir::TypeDecl* FindTypeDecl(const mir::Module& module,
                                  std::string_view name);

std::string VariantLeafName(std::string_view variant_name);

const mir::VariantDecl* FindVariantDecl(const mir::TypeDecl& type_decl,
                                        std::string_view variant_name,
                                        std::size_t* variant_index = nullptr);

struct EnumBackendLayout {
    BackendTypeInfo aggregate_type;
    BackendTypeInfo payload_type;
    std::size_t payload_size = 0;
    std::size_t payload_alignment = 1;
    std::vector<std::vector<BackendTypeInfo>> variant_field_types;
    std::vector<std::vector<std::size_t>> variant_payload_offsets;
};

std::optional<std::size_t> ParseBackendArrayLength(std::string_view text);

std::optional<BackendTypeInfo> LowerTypeInfo(const mir::Module& module,
                                             const sema::Type& type);

std::optional<BackendTypeInfo> LowerAggregateFieldStorageType(const mir::Module& module,
                                                              const sema::Type& aggregate_type,
                                                              std::string_view field_name);

std::optional<EnumBackendLayout> LowerEnumLayout(const mir::Module& module,
                                                 const mir::TypeDecl& type_decl,
                                                 const sema::Type& original_type);

std::string FormatTypeInfo(const BackendTypeInfo& type_info);

bool LowerInstructionType(const mir::Module& module,
                          const sema::Type& type,
                          const std::filesystem::path& source_path,
                          support::DiagnosticSink& diagnostics,
                          const std::string& context,
                          BackendTypeInfo& type_info);

std::string BackendFunctionName(const std::string& source_name);
std::string BackendGlobalName(const std::string& source_name);
std::string BackendLocalName(const std::string& source_name);
std::string BackendBlockName(std::size_t function_index,
                             std::size_t block_index,
                             const std::string& source_name);
std::string BackendTempName(std::size_t function_index,
                            std::size_t block_index,
                            std::size_t instruction_index);

std::string LLVMFunctionSymbol(std::string_view source_name,
                               bool wrap_hosted_main);
std::string LLVMBlockLabel(std::size_t function_index,
                           std::size_t block_index,
                           const std::string& source_label);
std::string LLVMLocalSlotName(std::string_view source_name);
std::string LLVMParamName(std::string_view source_name);
std::string LLVMTempName(std::size_t function_index,
                         std::size_t block_index,
                         std::size_t instruction_index);
std::string LLVMGlobalName(std::string_view source_name);

std::string FormatReturnTypes(const std::vector<BackendTypeInfo>& types);

std::optional<BackendTypeInfo> LowerFunctionReturnType(const mir::Module& module,
                                                       const std::vector<sema::Type>& return_types);

LowerResult LowerInspectModule(const mir::Module& module,
                               const std::filesystem::path& source_path,
                               const TargetConfig& target,
                               support::DiagnosticSink& diagnostics);

bool RenderLlvmModule(const mir::Module& module,
                      const TargetConfig& target,
                      const std::filesystem::path& source_path,
                      bool wrap_hosted_main,
                      support::DiagnosticSink& diagnostics,
                      std::string& llvm_ir);

struct FunctionLoweringState {
    std::size_t function_index = 0;
    const mir::Module* module = nullptr;
    const mir::Function* function = nullptr;
    std::unordered_map<std::string, BackendLocal> locals;
    std::unordered_map<std::string, std::string> blocks;
    std::unordered_map<std::string, std::string> values;
    std::unordered_map<std::string, BackendTypeInfo> value_types;
};

inline std::string_view ToString(mir::Instruction::Kind kind) {
    switch (kind) {
        case mir::Instruction::Kind::kConst:
            return "const";
        case mir::Instruction::Kind::kLocalAddr:
            return "local_addr";
        case mir::Instruction::Kind::kArenaNew:
            return "arena_new";
        case mir::Instruction::Kind::kLoadLocal:
            return "load_local";
        case mir::Instruction::Kind::kStoreLocal:
            return "store_local";
        case mir::Instruction::Kind::kStoreTarget:
            return "store_target";
        case mir::Instruction::Kind::kSymbolRef:
            return "symbol_ref";
        case mir::Instruction::Kind::kBoundsCheck:
            return "bounds_check";
        case mir::Instruction::Kind::kDivCheck:
            return "div_check";
        case mir::Instruction::Kind::kShiftCheck:
            return "shift_check";
        case mir::Instruction::Kind::kUnary:
            return "unary";
        case mir::Instruction::Kind::kBinary:
            return "binary";
        case mir::Instruction::Kind::kConvert:
            return "convert";
        case mir::Instruction::Kind::kConvertNumeric:
            return "convert_numeric";
        case mir::Instruction::Kind::kConvertDistinct:
            return "convert_distinct";
        case mir::Instruction::Kind::kPointerToInt:
            return "pointer_to_int";
        case mir::Instruction::Kind::kIntToPointer:
            return "int_to_pointer";
        case mir::Instruction::Kind::kArrayToSlice:
            return "array_to_slice";
        case mir::Instruction::Kind::kBufferToSlice:
            return "buffer_to_slice";
        case mir::Instruction::Kind::kBufferNew:
            return "buffer_new";
        case mir::Instruction::Kind::kBufferFree:
            return "buffer_free";
        case mir::Instruction::Kind::kSliceFromBuffer:
            return "slice_from_buffer";
        case mir::Instruction::Kind::kCall:
            return "call";
        case mir::Instruction::Kind::kMemoryBarrier:
            return "memory_barrier";
        case mir::Instruction::Kind::kVolatileLoad:
            return "volatile_load";
        case mir::Instruction::Kind::kVolatileStore:
            return "volatile_store";
        case mir::Instruction::Kind::kAtomicLoad:
            return "atomic_load";
        case mir::Instruction::Kind::kAtomicStore:
            return "atomic_store";
        case mir::Instruction::Kind::kAtomicExchange:
            return "atomic_exchange";
        case mir::Instruction::Kind::kAtomicCompareExchange:
            return "atomic_compare_exchange";
        case mir::Instruction::Kind::kAtomicFetchAdd:
            return "atomic_fetch_add";
        case mir::Instruction::Kind::kField:
            return "field";
        case mir::Instruction::Kind::kIndex:
            return "index";
        case mir::Instruction::Kind::kSlice:
            return "slice";
        case mir::Instruction::Kind::kAggregateInit:
            return "aggregate_init";
        case mir::Instruction::Kind::kVariantInit:
            return "variant_init";
        case mir::Instruction::Kind::kVariantMatch:
            return "variant_match";
        case mir::Instruction::Kind::kVariantExtract:
            return "variant_extract";
    }

    MC_UNREACHABLE("unhandled mir::Instruction::Kind in codegen_llvm::ToString");
}

inline std::string_view ToString(mir::Instruction::TargetKind kind) {
    switch (kind) {
        case mir::Instruction::TargetKind::kNone:
            return "none";
        case mir::Instruction::TargetKind::kFunction:
            return "function";
        case mir::Instruction::TargetKind::kGlobal:
            return "global";
        case mir::Instruction::TargetKind::kField:
            return "field";
        case mir::Instruction::TargetKind::kDerefField:
            return "deref_field";
        case mir::Instruction::TargetKind::kIndex:
            return "index";
        case mir::Instruction::TargetKind::kOther:
            return "other";
    }

    MC_UNREACHABLE("unhandled mir::Instruction::TargetKind in codegen_llvm::ToString");
}

inline std::string_view ToString(mir::Instruction::ArithmeticSemantics semantics) {
    switch (semantics) {
        case mir::Instruction::ArithmeticSemantics::kNone:
            return "none";
        case mir::Instruction::ArithmeticSemantics::kWrap:
            return "wrap";
    }

    MC_UNREACHABLE("unhandled mir::Instruction::ArithmeticSemantics in codegen_llvm::ToString");
}

inline std::string_view ToString(mir::Terminator::Kind kind) {
    switch (kind) {
        case mir::Terminator::Kind::kNone:
            return "none";
        case mir::Terminator::Kind::kReturn:
            return "return";
        case mir::Terminator::Kind::kBranch:
            return "branch";
        case mir::Terminator::Kind::kCondBranch:
            return "cond_branch";
    }

    MC_UNREACHABLE("unhandled mir::Terminator::Kind in codegen_llvm::ToString");
}

inline bool IsIntegerType(const BackendTypeInfo& type_info) {
    return !type_info.backend_name.empty() && type_info.backend_name[0] == 'i';
}

inline bool IsAtomicScalarType(const BackendTypeInfo& type_info) {
    return type_info.backend_name == "ptr" || IsIntegerType(type_info);
}

inline std::optional<std::string> LLVMAtomicOrderKeyword(std::string_view order_name) {
    const std::string leaf = VariantLeafName(order_name);
    if (leaf == "Relaxed") {
        return "monotonic";
    }
    if (leaf == "Acquire") {
        return "acquire";
    }
    if (leaf == "Release") {
        return "release";
    }
    if (leaf == "AcqRel") {
        return "acq_rel";
    }
    if (leaf == "SeqCst") {
        return "seq_cst";
    }
    return std::nullopt;
}

inline std::string JoinOperands(const std::vector<std::string>& operands) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < operands.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << operands[index];
    }
    return stream.str();
}

inline const BackendTypeInfo* FindValueType(const FunctionLoweringState& state,
                                            const std::string& value_name) {
    const auto it = state.value_types.find(value_name);
    return it == state.value_types.end() ? nullptr : &it->second;
}

template <typename T>
const T* ResolveNamedEntry(const std::unordered_map<std::string, T>& entries,
                           const std::string& entry_name,
                           const std::filesystem::path& source_path,
                           support::DiagnosticSink& diagnostics,
                           const std::string& error_message) {
    const auto it = entries.find(entry_name);
    if (it != entries.end()) {
        return &it->second;
    }

    ReportBackendError(source_path, error_message, diagnostics);
    return nullptr;
}

inline bool ResolveValue(const FunctionLoweringState& state,
                         const std::string& value_name,
                         const mir::Function& function,
                         const mir::BasicBlock& block,
                         const std::filesystem::path& source_path,
                         support::DiagnosticSink& diagnostics,
                         std::string& resolved) {
    const std::string* value = ResolveNamedEntry(state.values,
                                                 value_name,
                                                 source_path,
                                                 diagnostics,
                                                 "LLVM bootstrap backend references unknown MIR value '" + value_name +
                                                     "' in function '" + function.name + "' block '" + block.label + "'");
    if (value == nullptr) {
        return false;
    }
    resolved = *value;
    return true;
}

inline bool ResolveTypedValue(const FunctionLoweringState& state,
                              const std::string& value_name,
                              const mir::Function& function,
                              const mir::BasicBlock& block,
                              const std::filesystem::path& source_path,
                              support::DiagnosticSink& diagnostics,
                              std::string& resolved,
                              BackendTypeInfo& type_info) {
    if (!ResolveValue(state, value_name, function, block, source_path, diagnostics, resolved)) {
        return false;
    }

    const BackendTypeInfo* lowered_type = FindValueType(state, value_name);
    if (lowered_type != nullptr) {
        type_info = *lowered_type;
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap backend has no lowered type info for MIR value '" + value_name +
                           "' in function '" + function.name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

inline bool ResolveLocal(const FunctionLoweringState& state,
                         const std::string& local_name,
                         const mir::Function& function,
                         const mir::BasicBlock& block,
                         const std::filesystem::path& source_path,
                         support::DiagnosticSink& diagnostics,
                         const BackendLocal*& local) {
    local = ResolveNamedEntry(state.locals,
                              local_name,
                              source_path,
                              diagnostics,
                              "LLVM bootstrap backend references unknown local '" + local_name + "' in function '" +
                                  function.name + "' block '" + block.label + "'");
    return local != nullptr;
}

inline bool ResolveBlock(const FunctionLoweringState& state,
                         const std::string& block_name,
                         const mir::Function& function,
                         const mir::BasicBlock& block,
                         const std::filesystem::path& source_path,
                         support::DiagnosticSink& diagnostics,
                         std::string& resolved) {
    const std::string* value = ResolveNamedEntry(state.blocks,
                                                 block_name,
                                                 source_path,
                                                 diagnostics,
                                                 "LLVM bootstrap backend references unknown branch target '" + block_name +
                                                     "' in function '" + function.name + "' block '" + block.label + "'");
    if (value == nullptr) {
        return false;
    }
    resolved = *value;
    return true;
}

inline bool RequireInstructionResult(const mir::Instruction& instruction,
                                     const mir::BasicBlock& block,
                                     const std::filesystem::path& source_path,
                                     support::DiagnosticSink& diagnostics) {
    if (!instruction.result.empty()) {
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap backend requires a result for MIR instruction '" +
                           std::string(ToString(instruction.kind)) + "' in block '" + block.label + "'",
                       diagnostics);
    return false;
}

inline bool RequireOperandCount(const mir::Instruction& instruction,
                                std::size_t expected_count,
                                std::string_view backend_name,
                                std::string_view operation_name,
                                std::string_view function_name,
                                const mir::BasicBlock& block,
                                const std::filesystem::path& source_path,
                                support::DiagnosticSink& diagnostics) {
    if (instruction.operands.size() == expected_count) {
        return true;
    }

    ReportBackendError(source_path,
                       std::string(backend_name) + " requires " + std::string(operation_name) +
                           " to use exactly " + std::to_string(expected_count) + " operand" +
                           (expected_count == 1 ? "" : "s") + " in function '" + std::string(function_name) +
                           "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

inline void RecordLoweredValue(FunctionLoweringState& state,
                               const std::string& result_name,
                               const std::string& backend_name,
                               const BackendTypeInfo& type_info) {
    state.values[result_name] = backend_name;
    state.value_types[result_name] = type_info;
}

inline bool ResolveSingleOperand(const FunctionLoweringState& state,
                                 const mir::Instruction& instruction,
                                 std::string_view backend_name,
                                 std::string_view operation_name,
                                 const mir::Function& function,
                                 const mir::BasicBlock& block,
                                 const std::filesystem::path& source_path,
                                 support::DiagnosticSink& diagnostics,
                                 std::string& operand) {
    if (!RequireOperandCount(instruction,
                             1,
                             backend_name,
                             operation_name,
                             function.name,
                             block,
                             source_path,
                             diagnostics)) {
        return false;
    }

    return ResolveValue(state,
                        instruction.operands.front(),
                        function,
                        block,
                        source_path,
                        diagnostics,
                        operand);
}

inline bool ResolveTwoOperands(const FunctionLoweringState& state,
                               const mir::Instruction& instruction,
                               std::string_view backend_name,
                               std::string_view operation_name,
                               const mir::Function& function,
                               const mir::BasicBlock& block,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               std::string& lhs,
                               std::string& rhs) {
    if (!RequireOperandCount(instruction,
                             2,
                             backend_name,
                             operation_name,
                             function.name,
                             block,
                             source_path,
                             diagnostics)) {
        return false;
    }

    return ResolveValue(state,
                        instruction.operands[0],
                        function,
                        block,
                        source_path,
                        diagnostics,
                        lhs) &&
           ResolveValue(state,
                        instruction.operands[1],
                        function,
                        block,
                        source_path,
                        diagnostics,
                        rhs);
}

inline bool ResolveTwoTypedOperands(const FunctionLoweringState& state,
                                    const mir::Instruction& instruction,
                                    std::string_view backend_name,
                                    std::string_view operation_name,
                                    const mir::Function& function,
                                    const mir::BasicBlock& block,
                                    const std::filesystem::path& source_path,
                                    support::DiagnosticSink& diagnostics,
                                    std::string& lhs,
                                    BackendTypeInfo& lhs_type,
                                    std::string& rhs,
                                    BackendTypeInfo& rhs_type) {
    if (!RequireOperandCount(instruction,
                             2,
                             backend_name,
                             operation_name,
                             function.name,
                             block,
                             source_path,
                             diagnostics)) {
        return false;
    }

    return ResolveTypedValue(state,
                             instruction.operands[0],
                             function,
                             block,
                             source_path,
                             diagnostics,
                             lhs,
                             lhs_type) &&
           ResolveTypedValue(state,
                             instruction.operands[1],
                             function,
                             block,
                             source_path,
                             diagnostics,
                             rhs,
                             rhs_type);
}

inline bool ResolveOperands(const FunctionLoweringState& state,
                            const mir::Instruction& instruction,
                            const mir::Function& function,
                            const mir::BasicBlock& block,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics,
                            std::vector<std::string>& operands) {
    operands.clear();
    operands.reserve(instruction.operands.size());
    for (const auto& operand_name : instruction.operands) {
        std::string operand;
        if (!ResolveValue(state, operand_name, function, block, source_path, diagnostics, operand)) {
            return false;
        }
        operands.push_back(std::move(operand));
    }
    return true;
}

}  // namespace mc::codegen_llvm

#endif  // C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_INTERNAL_H_