#ifndef C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_INTERNAL_H_
#define C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_INTERNAL_H_

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "compiler/codegen_llvm/backend.h"
#include "compiler/support/assert.h"

namespace mc::codegen_llvm {

struct ExecutableValue;
struct ExecutableFunctionState;

bool ValidateBootstrapTarget(const TargetConfig& target,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics);

bool ValidateExecutableBackendCapabilities(const mir::Module& module,
                                          const TargetConfig& target,
                                          const std::filesystem::path& source_path,
                                          support::DiagnosticSink& diagnostics);

bool IsProcedureSourceType(std::string_view source_name);

const mir::Function* FindFunction(const mir::Module& module,
                                  std::string_view name);

bool IsSignedSourceType(std::string_view source_name);

bool IsUnsignedSourceType(std::string_view source_name);

bool IsFloatType(const BackendTypeInfo& type_info);

std::size_t IntegerBitWidth(const BackendTypeInfo& type_info);

std::string DecodeStringLiteral(std::string_view literal);

std::string EncodeLLVMStringBytes(std::string_view decoded);

std::string EscapeLLVMQuotedString(std::string_view text);

std::string FormatLLVMLiteral(const BackendTypeInfo& type_info,
                              std::string_view value);

std::string LLVMStructInsertBase(const BackendTypeInfo& type_info);

bool RenderLLVMGlobalConstValue(const mir::Module& module,
                                sema::Type type,
                                const sema::ConstValue& value,
                                bool wrap_hosted_main,
                                const std::filesystem::path& source_path,
                                support::DiagnosticSink& diagnostics,
                                std::string& rendered,
                                std::string_view string_backing_prefix = {},
                                std::ostringstream* prelude = nullptr,
                                std::size_t* string_constant_index = nullptr);

bool RenderLLVMGlobalStringConstValue(const mir::Module& module,
                                      sema::Type type,
                                      const sema::ConstValue& value,
                                      std::string_view backing_global_name,
                                      const std::filesystem::path& source_path,
                                      support::DiagnosticSink& diagnostics,
                                      std::ostringstream& prelude,
                                      std::string& rendered);

std::string FunctionBlockContext(std::string_view operation,
                                 std::string_view function_name,
                                 const mir::BasicBlock& block);

std::string ExecutableFunctionBlockContext(std::string_view operation,
                                           const ExecutableFunctionState& state,
                                           const mir::BasicBlock& block);

bool RequireOperandRange(const mir::Instruction& instruction,
                         std::size_t minimum_count,
                         std::size_t maximum_count,
                         std::string_view backend_name,
                         std::string_view requirement,
                         std::string_view function_name,
                         const mir::BasicBlock& block,
                         const std::filesystem::path& source_path,
                         support::DiagnosticSink& diagnostics);

std::string EmitSliceLengthValue(std::size_t function_index,
                                 std::size_t block_index,
                                 std::size_t instruction_index,
                                 std::string_view suffix,
                                 std::string_view lower_i64,
                                 const std::optional<std::string>& upper_i64,
                                 std::string_view fallback_upper_bound,
                                 std::vector<std::string>& output_lines);

std::string EmitAggregateStackSlot(const ExecutableValue& value,
                                   std::size_t function_index,
                                   std::size_t block_index,
                                   std::size_t instruction_index,
                                   std::string_view suffix,
                                   std::vector<std::string>& output_lines);

void ReportBackendError(const std::filesystem::path& source_path,
                        const std::string& message,
                        support::DiagnosticSink& diagnostics);

const mir::TypeDecl* FindTypeDecl(const mir::Module& module,
                                  std::string_view name);

sema::Type InstantiateAliasedType(const mir::TypeDecl& type_decl,
                                  const sema::Type& instantiated_type);

std::vector<std::pair<std::string, sema::Type>> InstantiateFields(
    const mir::TypeDecl& type_decl,
    const sema::Type& instantiated_type);

bool IsAggregateType(const BackendTypeInfo& type_info);

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

std::string RenderLLVMParameter(const BackendTypeInfo& type_info,
                                const mir::Local& param,
                                bool include_name);

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

struct ExecutableValue {
    std::string text;
    BackendTypeInfo type;
    std::optional<std::string> storage_slot;
    std::optional<sema::Type> source_type;
};

struct ExecutableStringConstant {
    std::string global_name;
    std::string array_type;
    std::size_t length = 0;
};

struct ExecutableGlobalInfo {
    std::string backend_name;
    sema::Type type;
    BackendTypeInfo lowered_type;
    bool is_const = false;
    bool is_extern = false;
    std::vector<std::string> initializers;
};

using ExecutableGlobals = std::unordered_map<std::string, ExecutableGlobalInfo>;

struct CheckedHelperRequirements {
    std::set<std::string> div_backend_names;
    std::set<std::size_t> shift_widths;
};

struct ExecutableSignature {
    std::vector<sema::Type> param_types;
    std::vector<sema::Type> return_types;
};

struct ExecutableFunctionSignature {
    std::vector<const mir::Local*> params;
    ExecutableSignature types;
};

struct ExecutableCallSignature {
    std::string callee_text;
    std::string call_label;
    ExecutableSignature types;
};

struct ExecutableFunctionState {
    const mir::Module* module = nullptr;
    const mir::Function* function = nullptr;
    ExecutableFunctionSignature signature;
    std::unordered_map<std::string, std::string> block_labels;
    std::unordered_map<std::string, std::string> local_slots;
    const ExecutableGlobals* globals = nullptr;
    std::unordered_map<std::string, ExecutableStringConstant> string_constants;
    std::unordered_map<std::string, ExecutableValue> values;
};

struct ExecutableEmissionContext {
    std::size_t function_index = 0;
    std::size_t block_index = 0;
    std::size_t instruction_index = 0;
    const mir::BasicBlock& block;
    const std::filesystem::path& source_path;
    support::DiagnosticSink& diagnostics;
    ExecutableFunctionState& state;
    std::vector<std::string>& output_lines;
};

bool ResolveExecutableValue(const ExecutableFunctionState& state,
                            const std::string& value_name,
                            const mir::BasicBlock& block,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics,
                            ExecutableValue& value);

bool ResolveExecutableOperands(const mir::Instruction& instruction,
                               const mir::BasicBlock& block,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const ExecutableFunctionState& state,
                               ExecutableValue& first);

bool ResolveExecutableOperands(const mir::Instruction& instruction,
                               const mir::BasicBlock& block,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const ExecutableFunctionState& state,
                               ExecutableValue& first,
                               ExecutableValue& second);

bool ResolveExecutableOperands(const mir::Instruction& instruction,
                               const mir::BasicBlock& block,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const ExecutableFunctionState& state,
                               ExecutableValue& first,
                               ExecutableValue& second,
                               ExecutableValue& third);

std::optional<sema::Type> ResolveExecutableFieldBaseType(const mir::Module& module,
                                                         const sema::Type& metadata_base_type,
                                                         const ExecutableValue& base_value);

void RecordExecutableValue(ExecutableFunctionState& state,
                           const std::string& result_name,
                           const std::string& text,
                           const BackendTypeInfo& type,
                           std::optional<std::string> storage_slot = std::nullopt,
                           std::optional<sema::Type> source_type = std::nullopt);

bool EmitValueRepresentationCoercion(const ExecutableValue& value,
                                     const BackendTypeInfo& target_type,
                                     std::size_t function_index,
                                     std::size_t block_index,
                                     std::size_t instruction_index,
                                     std::string_view suffix,
                                     std::vector<std::string>& output_lines,
                                     std::string& coerced);

bool ExtendIntegerToI64(const ExecutableValue& value,
                        std::size_t function_index,
                        std::size_t block_index,
                        std::size_t instruction_index,
                        std::string_view suffix,
                        std::vector<std::string>& output_lines,
                        std::string& extended);

bool EmitNumericConversion(const ExecutableValue& operand,
                           const BackendTypeInfo& result_type,
                           std::size_t function_index,
                           std::size_t block_index,
                           std::size_t instruction_index,
                           std::vector<std::string>& output_lines,
                           std::string& result_text);

bool CollectCheckedHelperRequirements(const mir::Module& module,
                                      const std::filesystem::path& source_path,
                                      support::DiagnosticSink& diagnostics,
                                      CheckedHelperRequirements& requirements);

bool EmitSymbolRefInstruction(const mir::Instruction& instruction,
                              const ExecutableEmissionContext& context,
                              const std::unordered_map<std::string, std::string>& function_symbols);

bool EmitConvertDistinctInstruction(const mir::Instruction& instruction,
                                    const ExecutableEmissionContext& context);

bool EmitConvertInstruction(const mir::Instruction& instruction,
                            const ExecutableEmissionContext& context);

bool EmitConvertNumericInstruction(const mir::Instruction& instruction,
                                   const ExecutableEmissionContext& context);

bool EmitPointerToIntInstruction(const mir::Instruction& instruction,
                                 const ExecutableEmissionContext& context);

bool EmitIntToPointerInstruction(const mir::Instruction& instruction,
                                 const ExecutableEmissionContext& context);

bool EmitCallInstruction(const mir::Instruction& instruction,
                         const ExecutableEmissionContext& context,
                         const std::unordered_map<std::string, std::string>& function_symbols);

bool EmitBinaryInstruction(const mir::Instruction& instruction,
                           const ExecutableEmissionContext& context);

bool EmitBoundsCheckCall(const mir::Instruction& instruction,
                         const ExecutableEmissionContext& context);

bool EmitDivCheckCall(const mir::Instruction& instruction,
                      const ExecutableEmissionContext& context);

bool EmitShiftCheckCall(const mir::Instruction& instruction,
                        const ExecutableEmissionContext& context);

bool EmitMemoryInstruction(const mir::Instruction& instruction,
                           const ExecutableEmissionContext& context);

bool ResolveEnumInstructionLayout(const mir::Instruction& instruction,
                                  const mir::Module& module,
                                  const std::filesystem::path& source_path,
                                  support::DiagnosticSink& diagnostics,
                                  const std::string& context,
                                  EnumBackendLayout& layout,
                                  std::size_t& variant_index);

std::string EmitEnumPayloadFieldPointer(const std::string& enum_slot,
                                        const EnumBackendLayout& layout,
                                        std::size_t payload_offset,
                                        std::size_t function_index,
                                        std::size_t block_index,
                                        std::size_t instruction_index,
                                        std::string_view suffix,
                                        std::vector<std::string>& output_lines);

bool EmitAggregateInitInstruction(const mir::Instruction& instruction,
                                  const ExecutableEmissionContext& context);

bool EmitVariantInitInstruction(const mir::Instruction& instruction,
                                const ExecutableEmissionContext& context);

bool EmitVariantMatchInstruction(const mir::Instruction& instruction,
                                 const ExecutableEmissionContext& context);

bool EmitVariantExtractInstruction(const mir::Instruction& instruction,
                                   const ExecutableEmissionContext& context);

bool EmitArenaNewInstruction(const mir::Instruction& instruction,
                             const ExecutableEmissionContext& context);

bool EmitSliceInstruction(const mir::Instruction& instruction,
                          const ExecutableEmissionContext& context);

namespace executable_support {

ExecutableFunctionSignature NormalizeFunctionSignature(const mir::Function& function);

bool NormalizeProcedureTypeSignature(const sema::Type& procedure_type,
                                     const std::filesystem::path& source_path,
                                     support::DiagnosticSink& diagnostics,
                                     std::string_view context,
                                     ExecutableSignature& signature);

sema::Type SignatureResultType(const ExecutableSignature& signature);

bool LowerExecutableParamTypes(const mir::Module& module,
                               const ExecutableSignature& signature,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const std::function<std::string(std::size_t)>& describe_param,
                               std::vector<BackendTypeInfo>& param_types);

bool LowerExecutableReturnType(const mir::Module& module,
                               const ExecutableSignature& signature,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               std::string_view context,
                               std::optional<BackendTypeInfo>& return_type);

void RenderExecutableParameterList(const ExecutableFunctionSignature& function_signature,
                                   const std::vector<BackendTypeInfo>& param_types,
                                   bool include_names,
                                   std::ostringstream& stream);

bool LowerExecutableLocalTypes(const mir::Module& module,
                               const mir::Function& function,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               std::vector<BackendTypeInfo>& local_types);

void RenderExecutableEntryPrologue(const mir::Function& function,
                                   const ExecutableFunctionSignature& function_signature,
                                   const std::vector<BackendTypeInfo>& local_types,
                                   const std::vector<BackendTypeInfo>& param_types,
                                   std::ostringstream& stream);

ExecutableFunctionState InitializeExecutableFunctionState(
    const mir::Module& module,
    const mir::Function& function,
    const ExecutableFunctionSignature& function_signature,
    std::size_t function_index,
    const ExecutableGlobals& globals,
    std::unordered_map<std::string, ExecutableStringConstant> string_constants);

bool CollectExecutableStringConstants(const mir::Module& module,
                                     const mir::Function& function,
                                     std::size_t function_index,
                                     const std::filesystem::path& source_path,
                                     support::DiagnosticSink& diagnostics,
                                     std::ostringstream& stream,
                                     std::unordered_map<std::string, ExecutableStringConstant>& string_constants);

}  // namespace executable_support

namespace executable_atomic {

bool RenderAtomicInstruction(const mir::Instruction& instruction,
                             const ExecutableEmissionContext& context);

}  // namespace executable_atomic

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
        case mir::Terminator::Kind::kPanic:
            return "panic";
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
    const std::string leaf = mir::VariantLeafName(order_name);
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