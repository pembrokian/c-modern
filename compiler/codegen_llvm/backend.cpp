#include "compiler/codegen_llvm/backend.h"

#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace mc::codegen_llvm {
namespace {

constexpr std::string_view kBootstrapTriple = "arm64-apple-darwin25.4.0";
constexpr std::string_view kBootstrapTargetFamily = "arm64-apple-darwin";
constexpr std::string_view kBootstrapObjectFormat = "macho";
constexpr std::string_view kInspectSurface = "backend_ir_text";

struct FunctionLoweringState {
    std::size_t function_index = 0;
    const mir::Module* module = nullptr;
    const mir::Function* function = nullptr;
    std::unordered_map<std::string, BackendLocal> locals;
    std::unordered_map<std::string, std::string> blocks;
    std::unordered_map<std::string, std::string> values;
    std::unordered_map<std::string, BackendTypeInfo> value_types;
};

std::string_view ToString(mir::Instruction::Kind kind) {
    switch (kind) {
        case mir::Instruction::Kind::kConst:
            return "const";
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
        case mir::Instruction::Kind::kCall:
            return "call";
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
        case mir::Instruction::Kind::kVariantMatch:
            return "variant_match";
        case mir::Instruction::Kind::kVariantExtract:
            return "variant_extract";
    }

    return "instruction";
}

std::string_view ToString(mir::Instruction::TargetKind kind) {
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

    return "target";
}

std::string_view ToString(mir::Instruction::ArithmeticSemantics semantics) {
    switch (semantics) {
        case mir::Instruction::ArithmeticSemantics::kNone:
            return "none";
        case mir::Instruction::ArithmeticSemantics::kWrap:
            return "wrap";
    }

    return "none";
}

std::string_view ToString(mir::Terminator::Kind kind) {
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

    return "terminator";
}

void WriteIndent(std::ostringstream& stream, int indent) {
    for (int count = 0; count < indent; ++count) {
        stream << "  ";
    }
}

void WriteLine(std::ostringstream& stream, int indent, const std::string& text) {
    WriteIndent(stream, indent);
    stream << text << '\n';
}

void ReportBackendError(const std::filesystem::path& source_path,
                        const std::string& message,
                        support::DiagnosticSink& diagnostics) {
    diagnostics.Report({
        .file_path = source_path,
        .span = support::kDefaultSourceSpan,
        .severity = support::DiagnosticSeverity::kError,
        .message = message,
    });
}

bool IsBootstrapTarget(const TargetConfig& target) {
    return target.hosted && target.triple == kBootstrapTriple && target.target_family == kBootstrapTargetFamily &&
           target.object_format == kBootstrapObjectFormat;
}

const mir::TypeDecl* FindTypeDecl(const mir::Module& module,
                                  std::string_view name) {
    for (const auto& type_decl : module.type_decls) {
        if (type_decl.name == name) {
            return &type_decl;
        }
    }
    return nullptr;
}

std::optional<std::size_t> ParseArrayLength(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::size_t value = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = (value * 10) + static_cast<std::size_t>(ch - '0');
    }
    return value;
}

std::optional<BackendTypeInfo> LowerTypeInfo(const mir::Module& module,
                                             const sema::Type& type) {
    BackendTypeInfo info {
        .source_name = sema::FormatType(type),
    };

    const auto use_backend_type = [&](std::string backend_name,
                                      std::size_t size,
                                      std::size_t alignment) -> std::optional<BackendTypeInfo> {
        info.backend_name = std::move(backend_name);
        info.size = size;
        info.alignment = alignment;
        return info;
    };

    switch (type.kind) {
        case sema::Type::Kind::kBool:
            return use_backend_type("i1", 1, 1);
        case sema::Type::Kind::kIntLiteral:
            return use_backend_type("i64", 8, 8);
        case sema::Type::Kind::kFloatLiteral:
            return use_backend_type("double", 8, 8);
        case sema::Type::Kind::kNamed:
            if (type.name == "bool") {
                return use_backend_type("i1", 1, 1);
            }
            if (type.name == "i8" || type.name == "u8") {
                return use_backend_type("i8", 1, 1);
            }
            if (type.name == "i16" || type.name == "u16") {
                return use_backend_type("i16", 2, 2);
            }
            if (type.name == "i32" || type.name == "u32") {
                return use_backend_type("i32", 4, 4);
            }
            if (type.name == "i64" || type.name == "u64" || type.name == "isize" || type.name == "usize" ||
                type.name == "uintptr") {
                return use_backend_type("i64", 8, 8);
            }
            if (type.name == "f32") {
                return use_backend_type("float", 4, 4);
            }
            if (type.name == "f64") {
                return use_backend_type("double", 8, 8);
            }
            if (type.name == "str" || type.name == "string" || type.name == "cstr") {
                return use_backend_type("{ptr, i64}", 16, 8);
            }
            if (type.name == "Slice") {
                return use_backend_type("{ptr, i64}", 16, 8);
            }
            if (type.name == "Buffer") {
                return use_backend_type("{ptr, i64, i64, ptr}", 32, 8);
            }
            if (const mir::TypeDecl* type_decl = FindTypeDecl(module, type.name)) {
                if (type_decl->kind == mir::TypeDecl::Kind::kAlias || type_decl->kind == mir::TypeDecl::Kind::kDistinct) {
                    auto lowered = LowerTypeInfo(module, type_decl->aliased_type);
                    if (!lowered.has_value()) {
                        return std::nullopt;
                    }
                    lowered->source_name = info.source_name;
                    return lowered;
                }
            }
            return std::nullopt;
        case sema::Type::Kind::kPointer:
            return use_backend_type("ptr", 8, 8);
        case sema::Type::Kind::kConst: {
            if (type.subtypes.empty()) {
                return std::nullopt;
            }
            auto lowered = LowerTypeInfo(module, type.subtypes.front());
            if (!lowered.has_value()) {
                return std::nullopt;
            }
            lowered->source_name = info.source_name;
            return lowered;
        }
        case sema::Type::Kind::kArray: {
            if (type.subtypes.empty()) {
                return std::nullopt;
            }
            auto element = LowerTypeInfo(module, type.subtypes.front());
            const auto length = ParseArrayLength(type.metadata);
            if (!element.has_value() || !length.has_value()) {
                return std::nullopt;
            }
            return use_backend_type("[" + std::to_string(*length) + " x " + element->backend_name + "]",
                                    element->size * *length,
                                    element->alignment);
        }
        case sema::Type::Kind::kProcedure:
            return use_backend_type("ptr", 8, 8);
        case sema::Type::Kind::kString:
            return use_backend_type("{ptr, i64}", 16, 8);
        default:
            return std::nullopt;
    }
}

std::string FormatTypeInfo(const BackendTypeInfo& type_info) {
    std::ostringstream stream;
    stream << type_info.source_name << " [repr=" << type_info.backend_name << ", size=" << type_info.size
           << ", align=" << type_info.alignment << ']';
    return stream.str();
}

bool LowerInstructionType(const mir::Module& module,
                         const sema::Type& type,
                         const std::filesystem::path& source_path,
                         support::DiagnosticSink& diagnostics,
                         const std::string& context,
                         BackendTypeInfo& type_info) {
    auto lowered = LowerTypeInfo(module, type);
    if (lowered.has_value()) {
        type_info = std::move(*lowered);
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap backend cannot map MIR type '" + sema::FormatType(type) +
                           "' to a Darwin arm64 representation in " + context,
                       diagnostics);
    return false;
}

std::string BackendFunctionName(const std::string& source_name) {
    return "@" + source_name;
}

std::string BackendLocalName(const std::string& source_name) {
    return "%local." + source_name;
}

std::string BackendBlockName(std::size_t function_index,
                             std::size_t block_index,
                             const std::string& source_label) {
    return "bb" + std::to_string(function_index) + "_" + std::to_string(block_index) + "." + source_label;
}

std::string BackendTempName(std::size_t function_index,
                            std::size_t block_index,
                            std::size_t instruction_index) {
    return "%t" + std::to_string(function_index) + "_" + std::to_string(block_index) + "_" +
           std::to_string(instruction_index);
}

std::string FormatReturnTypes(const std::vector<BackendTypeInfo>& return_types) {
    std::ostringstream stream;
    stream << '[';
    for (std::size_t index = 0; index < return_types.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << FormatTypeInfo(return_types[index]);
    }
    stream << ']';
    return stream.str();
}

const BackendLocal* FindLocal(const FunctionLoweringState& state,
                              const std::string& local_name) {
    const auto it = state.locals.find(local_name);
    return it == state.locals.end() ? nullptr : &it->second;
}

const std::string* FindValue(const FunctionLoweringState& state,
                             const std::string& value_name) {
    const auto it = state.values.find(value_name);
    return it == state.values.end() ? nullptr : &it->second;
}

const BackendTypeInfo* FindValueType(const FunctionLoweringState& state,
                                     const std::string& value_name) {
    const auto it = state.value_types.find(value_name);
    return it == state.value_types.end() ? nullptr : &it->second;
}

const std::string* FindBlock(const FunctionLoweringState& state,
                             const std::string& block_name) {
    const auto it = state.blocks.find(block_name);
    return it == state.blocks.end() ? nullptr : &it->second;
}

bool RequireInstructionResult(const mir::Instruction& instruction,
                              const mir::BasicBlock& block,
                              const std::filesystem::path& source_path,
                              support::DiagnosticSink& diagnostics) {
    if (!instruction.result.empty()) {
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap backend requires a result for MIR instruction '" +
                           std::string(ToString(instruction.kind)) + "' in function '" +
                           (block.label.empty() ? std::string("<unknown>") : block.label) + "'",
                       diagnostics);
    return false;
}

bool ResolveValue(const FunctionLoweringState& state,
                  const std::string& value_name,
                  const mir::Function& function,
                  const mir::BasicBlock& block,
                  const std::filesystem::path& source_path,
                  support::DiagnosticSink& diagnostics,
                  std::string& resolved) {
    const std::string* value = FindValue(state, value_name);
    if (value != nullptr) {
        resolved = *value;
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap backend references unknown MIR value '" + value_name + "' in function '" +
                           function.name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

bool ResolveTypedValue(const FunctionLoweringState& state,
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

bool ResolveLocal(const FunctionLoweringState& state,
                  const std::string& local_name,
                  const mir::Function& function,
                  const mir::BasicBlock& block,
                  const std::filesystem::path& source_path,
                  support::DiagnosticSink& diagnostics,
                  const BackendLocal*& local) {
    local = FindLocal(state, local_name);
    if (local != nullptr) {
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap backend references unknown local '" + local_name + "' in function '" +
                           function.name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

bool ResolveBlock(const FunctionLoweringState& state,
                  const std::string& block_name,
                  const mir::Function& function,
                  const mir::BasicBlock& block,
                  const std::filesystem::path& source_path,
                  support::DiagnosticSink& diagnostics,
                  std::string& resolved) {
    const std::string* value = FindBlock(state, block_name);
    if (value != nullptr) {
        resolved = *value;
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap backend references unknown branch target '" + block_name + "' in function '" +
                           function.name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

std::string JoinOperands(const std::vector<std::string>& operands) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < operands.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << operands[index];
    }
    return stream.str();
}

bool LowerInstruction(const mir::Instruction& instruction,
                      std::size_t block_index,
                      std::size_t instruction_index,
                      const std::filesystem::path& source_path,
                      support::DiagnosticSink& diagnostics,
                      FunctionLoweringState& state,
                      BackendBlock& backend_block) {
    const mir::Function& function = *state.function;
    const mir::BasicBlock& block = function.blocks[block_index];

    const auto record_value = [&](const std::string& backend_name,
                                  const BackendTypeInfo& type_info) {
        state.values[instruction.result] = backend_name;
        state.value_types[instruction.result] = type_info;
    };

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
            record_value(backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = const " + FormatTypeInfo(type_info) + " " +
                                                 instruction.op);
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
            record_value(backend_name, local->lowered_type);
            backend_block.instructions.push_back(backend_name + " = load " + FormatTypeInfo(local->lowered_type) +
                                                 " " + local->backend_name);
            return true;
        }

        case mir::Instruction::Kind::kStoreLocal: {
            if (instruction.operands.size() != 1) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires store_local to use exactly one operand in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            const BackendLocal* local = nullptr;
            if (!ResolveLocal(state, instruction.target, function, block, source_path, diagnostics, local)) {
                return false;
            }
            std::string operand;
            if (!ResolveValue(state, instruction.operands.front(), function, block, source_path, diagnostics, operand)) {
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
            if (instruction.operands.size() != 1) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires unary to use exactly one operand in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            std::string operand;
            if (!ResolveValue(state, instruction.operands.front(), function, block, source_path, diagnostics, operand)) {
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
            record_value(backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = unary " + instruction.op + " " +
                                                 FormatTypeInfo(type_info) + " " + operand);
            return true;
        }

        case mir::Instruction::Kind::kBinary: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            if (instruction.operands.size() != 2) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires binary to use exactly two operands in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            std::vector<std::string> operands;
            operands.reserve(2);
            for (const auto& operand_name : instruction.operands) {
                std::string operand;
                if (!ResolveValue(state, operand_name, function, block, source_path, diagnostics, operand)) {
                    return false;
                }
                operands.push_back(std::move(operand));
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
            record_value(backend_name, type_info);

            std::ostringstream line;
            line << backend_name << " = binary ";
            if (instruction.arithmetic_semantics != mir::Instruction::ArithmeticSemantics::kNone) {
                line << ToString(instruction.arithmetic_semantics) << ' ';
            }
            line << instruction.op << ' ' << FormatTypeInfo(type_info) << ' ' << operands[0] << ", "
                 << operands[1];
            backend_block.instructions.push_back(line.str());
            return true;
        }

        case mir::Instruction::Kind::kSymbolRef: {
            if (instruction.target_kind != mir::Instruction::TargetKind::kFunction || instruction.target_name.empty()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend only admits function symbol_ref values in the current bootstrap slice; function '" +
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
            record_value(backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = symbol_ref " + FormatTypeInfo(type_info) + " " +
                                                 BackendFunctionName(instruction.target_name));
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
            operands.reserve(instruction.operands.size());
            for (const auto& operand_name : instruction.operands) {
                std::string operand;
                if (!ResolveValue(state, operand_name, function, block, source_path, diagnostics, operand)) {
                    return false;
                }
                operands.push_back(std::move(operand));
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
                record_value(backend_name, type_info);
                line << backend_name << " = call " << FormatTypeInfo(type_info) << ' '
                     << BackendFunctionName(instruction.target_name) << '(' << JoinOperands(operands) << ')';
            }
            backend_block.instructions.push_back(line.str());
            return true;
        }

        case mir::Instruction::Kind::kBoundsCheck: {
            std::vector<std::string> operands;
            operands.reserve(instruction.operands.size());
            for (const auto& operand_name : instruction.operands) {
                std::string operand;
                if (!ResolveValue(state, operand_name, function, block, source_path, diagnostics, operand)) {
                    return false;
                }
                operands.push_back(std::move(operand));
            }
            backend_block.instructions.push_back("check.bounds " + instruction.op + " " + JoinOperands(operands));
            return true;
        }

        case mir::Instruction::Kind::kDivCheck: {
            if (instruction.operands.size() != 2) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires div_check to use two operands in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            std::string lhs;
            std::string rhs;
            BackendTypeInfo lhs_type;
            BackendTypeInfo rhs_type;
            if (!ResolveTypedValue(state, instruction.operands[0], function, block, source_path, diagnostics, lhs, lhs_type) ||
                !ResolveTypedValue(state, instruction.operands[1], function, block, source_path, diagnostics, rhs, rhs_type)) {
                return false;
            }
            backend_block.instructions.push_back("check.div " + instruction.op + " lhs=" + lhs + " rhs=" + rhs +
                                                 " type=" + FormatTypeInfo(lhs_type));
            return true;
        }

        case mir::Instruction::Kind::kShiftCheck: {
            if (instruction.operands.size() != 2) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires shift_check to use two operands in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            std::string value;
            std::string count;
            BackendTypeInfo value_type;
            BackendTypeInfo count_type;
            if (!ResolveTypedValue(state, instruction.operands[0], function, block, source_path, diagnostics, value, value_type) ||
                !ResolveTypedValue(state, instruction.operands[1], function, block, source_path, diagnostics, count, count_type)) {
                return false;
            }
            backend_block.instructions.push_back("check.shift " + instruction.op + " value=" + value + " count=" + count +
                                                 " type=" + FormatTypeInfo(value_type));
            return true;
        }

        case mir::Instruction::Kind::kConvertDistinct: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            if (instruction.operands.size() != 1) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires convert_distinct to use exactly one operand in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            std::string operand;
            if (!ResolveValue(state, instruction.operands.front(), function, block, source_path, diagnostics, operand)) {
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
            record_value(backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = convert_distinct " + FormatTypeInfo(type_info) +
                                                 " " + operand);
            return true;
        }

        case mir::Instruction::Kind::kPointerToInt: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            if (instruction.operands.size() != 1) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires pointer_to_int to use exactly one operand in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            std::string operand;
            if (!ResolveValue(state, instruction.operands.front(), function, block, source_path, diagnostics, operand)) {
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
            record_value(backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = ptrtoint " + FormatTypeInfo(type_info) + " " +
                                                 operand);
            return true;
        }

        case mir::Instruction::Kind::kIntToPointer: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            if (instruction.operands.size() != 1) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend requires int_to_pointer to use exactly one operand in function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            std::string operand;
            if (!ResolveValue(state, instruction.operands.front(), function, block, source_path, diagnostics, operand)) {
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
            record_value(backend_name, type_info);
            backend_block.instructions.push_back(backend_name + " = inttoptr " + FormatTypeInfo(type_info) + " " +
                                                 operand);
            return true;
        }

        case mir::Instruction::Kind::kStoreTarget:
        case mir::Instruction::Kind::kConvert:
        case mir::Instruction::Kind::kConvertNumeric:
        case mir::Instruction::Kind::kArrayToSlice:
        case mir::Instruction::Kind::kBufferToSlice:
        case mir::Instruction::Kind::kVolatileLoad:
        case mir::Instruction::Kind::kVolatileStore:
        case mir::Instruction::Kind::kAtomicLoad:
        case mir::Instruction::Kind::kAtomicStore:
        case mir::Instruction::Kind::kAtomicExchange:
        case mir::Instruction::Kind::kAtomicCompareExchange:
        case mir::Instruction::Kind::kAtomicFetchAdd:
        case mir::Instruction::Kind::kField:
        case mir::Instruction::Kind::kIndex:
        case mir::Instruction::Kind::kSlice:
        case mir::Instruction::Kind::kAggregateInit:
        case mir::Instruction::Kind::kVariantMatch:
        case mir::Instruction::Kind::kVariantExtract:
            ReportBackendError(source_path,
                               "LLVM bootstrap backend does not support MIR instruction '" +
                                   std::string(ToString(instruction.kind)) + "' in Stage 3; function '" +
                                   function.name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
    }

    return false;
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

            if (block.terminator.values.size() != 1 || function.return_types.size() != 1) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap backend only supports single-value returns in the current bootstrap slice; function '" +
                                       function.name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            std::string value;
            if (!ResolveValue(state,
                              block.terminator.values.front(),
                              function,
                              block,
                              source_path,
                              diagnostics,
                              value)) {
                return false;
            }

            const auto* return_type = function.return_types.empty() ? nullptr : &function.return_types.front();
            BackendTypeInfo lowered_return_type;
            if (return_type != nullptr &&
                !LowerInstructionType(*state.module,
                                      *return_type,
                                      source_path,
                                      diagnostics,
                                      "return in function '" + function.name + "' block '" + block.label + "'",
                                      lowered_return_type)) {
                return false;
            }

            backend_block.terminator = "ret " + FormatTypeInfo(lowered_return_type) + " " + value;
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
    if (function.is_extern) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend does not lower extern functions in the current bootstrap slice; function '" +
                               function.name + "'",
                           diagnostics);
        return false;
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

}  // namespace

TargetConfig BootstrapTargetConfig() {
    return {
        .triple = std::string(kBootstrapTriple),
        .target_family = std::string(kBootstrapTargetFamily),
        .object_format = std::string(kBootstrapObjectFormat),
        .hosted = true,
    };
}

LowerResult LowerModule(const mir::Module& module,
                        const std::filesystem::path& source_path,
                        const LowerOptions& options,
                        support::DiagnosticSink& diagnostics) {
    if (!IsBootstrapTarget(options.target)) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend only supports hosted 'arm64-apple-darwin' in Stage 3; got triple='" +
                               options.target.triple + "' target_family='" + options.target.target_family + "'",
                           diagnostics);
        return {};
    }

    if (!module.globals.empty()) {
        std::ostringstream names;
        for (std::size_t index = 0; index < module.globals.front().names.size(); ++index) {
            if (index > 0) {
                names << ", ";
            }
            names << module.globals.front().names[index];
        }
        ReportBackendError(source_path,
                               "LLVM bootstrap backend does not lower globals in the current bootstrap slice; first global names=[" +
                               names.str() + "]",
                           diagnostics);
        return {};
    }

    auto backend_module = std::make_unique<BackendModule>();
    backend_module->target = options.target;
    backend_module->inspect_surface = std::string(kInspectSurface);
    backend_module->functions.reserve(module.functions.size());

    for (std::size_t function_index = 0; function_index < module.functions.size(); ++function_index) {
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

std::string DumpModule(const BackendModule& module) {
    std::ostringstream stream;
    stream << "BackendModule surface=" << module.inspect_surface << " triple=" << module.target.triple
           << " target_family=" << module.target.target_family << " object_format=" << module.target.object_format
           << " hosted=" << (module.target.hosted ? "true" : "false") << '\n';

    for (const auto& function : module.functions) {
        std::ostringstream header;
        header << "Function source=" << function.source_name << " backend=" << function.backend_name;
        if (!function.backend_return_types.empty()) {
            header << " returns=" << FormatReturnTypes(function.backend_return_types);
        }
        WriteLine(stream, 1, header.str());

        for (const auto& local : function.locals) {
            std::ostringstream local_line;
            local_line << "Local source=" << local.source_name << " backend=" << local.backend_name
                       << " type=" << FormatTypeInfo(local.lowered_type);
            if (local.is_parameter) {
                local_line << " param";
            }
            if (!local.is_mutable) {
                local_line << " readonly";
            }
            WriteLine(stream, 2, local_line.str());
        }

        for (const auto& block : function.blocks) {
            WriteLine(stream, 2, "Block source=" + block.source_label + " backend=" + block.backend_label);
            for (const auto& instruction : block.instructions) {
                WriteLine(stream, 3, instruction);
            }
            WriteLine(stream, 3, block.terminator);
        }
    }

    return stream.str();
}

}  // namespace mc::codegen_llvm