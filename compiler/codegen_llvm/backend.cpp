#include "compiler/codegen_llvm/backend.h"
#include "compiler/codegen_llvm/backend_internal.h"

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
#include <cstdlib>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

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

bool ValidateBootstrapTargetImpl(const TargetConfig& target,
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

// Forward declaration — defined below once TypeSupportsErasedGenericEmission is available.
bool FunctionSupportsErasedGenericEmission(const mir::Module& module, const mir::Function& function);

bool ValidateExecutableBackendCapabilitiesImpl(const mir::Module& module,
                                              const TargetConfig& target,
                                              const std::filesystem::path& source_path,
                                              support::DiagnosticSink& diagnostics) {
    if (!IsBootstrapTarget(target)) {
        return true;
    }

    for (const auto& function : module.functions) {
        // Non-extern generic functions whose type parameters appear in ABI-significant
        // positions cannot be emitted by the bootstrap backend (monomorphization is not
        // yet supported).  Catch this early so the caller gets a compiler diagnostic
        // instead of a silent definition omission followed by a linker error.
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

bool IsProcedureSourceType(std::string_view source_name);

std::string LLVMTypeName(const BackendTypeInfo& type_info);

std::string FunctionBlockContext(std::string_view operation,
                                 std::string_view function_name,
                                 const mir::BasicBlock& block);

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

const mir::Function* FindFunction(const mir::Module& module,
                                  std::string_view name) {
    for (const auto& function : module.functions) {
        if (function.name == name) {
            return &function;
        }
    }
    return nullptr;
}

const mir::Local* FindMirLocal(const mir::Function& function,
                               std::string_view name) {
    for (const auto& local : function.locals) {
        if (local.name == name) {
            return &local;
        }
    }
    return nullptr;
}

const mir::GlobalDecl* FindMirGlobal(const mir::Module& module,
                                     std::string_view name) {
    for (const auto& global : module.globals) {
        for (const auto& global_name : global.names) {
            if (global_name == name) {
                return &global;
            }
        }
    }
    return nullptr;
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

bool FormatExecutableCallArguments(const mir::Instruction& instruction,
                                   const ExecutableCallSignature& call_signature,
                                   const mir::BasicBlock& block,
                                   const std::filesystem::path& source_path,
                                   support::DiagnosticSink& diagnostics,
                                   ExecutableFunctionState& state,
                                   std::string& args_text) {
    const std::size_t argument_start = 1;
    const std::size_t argument_count = instruction.operands.size() - argument_start;
    if (argument_count != call_signature.types.param_types.size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission expected " + std::to_string(call_signature.types.param_types.size()) +
                               " call arguments for " + call_signature.call_label + " but saw " + std::to_string(argument_count) +
                               " in function '" + state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    std::vector<BackendTypeInfo> param_types;
    if (!executable_support::LowerExecutableParamTypes(*state.module,
                                   call_signature.types,
                                   source_path,
                                   diagnostics,
                                   [&](std::size_t) {
                                       return FunctionBlockContext("call argument", state.function->name, block);
                                   },
                                   param_types)) {
        return false;
    }

    std::ostringstream args;
    for (std::size_t index = 0; index < argument_count; ++index) {
        ExecutableValue value;
        if (!ResolveExecutableValue(state,
                                    instruction.operands[argument_start + index],
                                    block,
                                    source_path,
                                    diagnostics,
                                    value)) {
            return false;
        }

        if (index > 0) {
            args << ", ";
        }
        args << LLVMTypeName(param_types[index]) << " " << value.text;
    }

    args_text = args.str();
    return true;
}

bool ResolveExecutableCallSignature(const mir::Instruction& instruction,
                                    const std::unordered_map<std::string, std::string>& function_symbols,
                                    const mir::BasicBlock& block,
                                    const std::filesystem::path& source_path,
                                    support::DiagnosticSink& diagnostics,
                                    ExecutableFunctionState& state,
                                    ExecutableCallSignature& signature) {
    if (instruction.operands.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires a callee operand for call in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    ExecutableValue callee_value;
    if (!ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, callee_value)) {
        return false;
    }

    const bool indirect_call = instruction.target_name.empty() || instruction.target_kind != mir::Instruction::TargetKind::kFunction;
    if (indirect_call) {
        if (!IsProcedureSourceType(callee_value.type.source_name)) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission requires procedure-typed callee for indirect call in function '" +
                                   state.function->name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        signature.callee_text = callee_value.text;
        signature.call_label = "indirect call";
        return executable_support::NormalizeProcedureTypeSignature(
            callee_value.source_type.has_value() ? *callee_value.source_type : sema::UnknownType(),
            source_path,
            diagnostics,
            "indirect call in function '" + state.function->name + "' block '" + block.label + "'",
            signature.types);
    }

    const mir::Function* callee = FindFunction(*state.module, instruction.target_name);
    if (callee == nullptr) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission references unknown direct callee '" +
                               instruction.target_name + "' in function '" + state.function->name + "' block '" +
                               block.label + "'",
                           diagnostics);
        return false;
    }

    const auto function_it = function_symbols.find(callee->name);
    if (function_it == function_symbols.end()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission has no symbol mapping for direct callee '" +
                               instruction.target_name + "' in function '" + state.function->name + "' block '" +
                               block.label + "'",
                           diagnostics);
        return false;
    }

    signature.callee_text = function_it->second;
    signature.call_label = "call to '" + instruction.target_name + "'";
    signature.types = executable_support::NormalizeFunctionSignature(*callee).types;
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

sema::Type InstantiateAliasedType(const mir::TypeDecl& type_decl, const sema::Type& instantiated_type) {
    sema::Type aliased_type = type_decl.aliased_type;
    if (!type_decl.type_params.empty()) {
        aliased_type = sema::SubstituteTypeParams(std::move(aliased_type), type_decl.type_params, instantiated_type.subtypes);
    }
    return aliased_type;
}

std::vector<std::pair<std::string, sema::Type>> InstantiateFields(const mir::TypeDecl& type_decl,
                                                                  const sema::Type& instantiated_type) {
    std::vector<std::pair<std::string, sema::Type>> fields = type_decl.fields;
    if (type_decl.type_params.empty()) {
        return fields;
    }
    for (auto& field : fields) {
        field.second = sema::SubstituteTypeParams(std::move(field.second), type_decl.type_params, instantiated_type.subtypes);
    }
    return fields;
}

std::string LLVMTypeName(const BackendTypeInfo& type_info) {
    return type_info.backend_name;
}

std::string RenderLLVMParameter(const BackendTypeInfo& type_info,
                                const mir::Local& param,
                                bool include_name) {
    std::ostringstream stream;
    stream << type_info.backend_name;
    if (param.is_noalias) {
        stream << " noalias";
    }
    if (include_name) {
        stream << " " << LLVMParamName(param.name);
    }
    return stream.str();
}

std::string LLVMZeroValue(const BackendTypeInfo& type_info) {
    if (type_info.backend_name == "float") {
        return "0.0";
    }
    if (type_info.backend_name == "double") {
        return "0.0";
    }
    if (type_info.backend_name == "ptr") {
        return "null";
    }
    if (type_info.backend_name == "i1") {
        return "false";
    }
    if (!type_info.backend_name.empty() && (type_info.backend_name.front() == '{' || type_info.backend_name.front() == '[')) {
        return "zeroinitializer";
    }
    return "0";
}

bool IsAggregateType(const BackendTypeInfo& type_info) {
    return !type_info.backend_name.empty() &&
           (type_info.backend_name.front() == '{' || type_info.backend_name.front() == '[' || type_info.backend_name.front() == '<');
}

bool IsSignedSourceType(std::string_view source_name) {
    return source_name == "i8" || source_name == "i16" || source_name == "i32" || source_name == "i64" ||
           source_name == "isize" || source_name == "int_literal";
}

bool IsUnsignedSourceType(std::string_view source_name) {
    return source_name == "u8" || source_name == "u16" || source_name == "u32" || source_name == "u64" ||
           source_name == "usize" || source_name == "uintptr";
}

std::size_t IntegerBitWidth(const BackendTypeInfo& type_info) {
    if (type_info.backend_name.size() < 2 || type_info.backend_name.front() != 'i') {
        return 0;
    }
    const std::string_view digits = std::string_view(type_info.backend_name).substr(1);
    if (!std::all_of(digits.begin(), digits.end(), ::isdigit)) {
        return 0;
    }
    return static_cast<std::size_t>(std::stoul(std::string(digits)));
}

std::optional<std::size_t> FindFieldIndex(const mir::Module& module,
                                          const sema::Type& base_type,
                                          std::string_view field_name) {
    const auto builtin_fields = sema::BuiltinAggregateFields(base_type);
    if (!builtin_fields.empty()) {
        for (std::size_t index = 0; index < builtin_fields.size(); ++index) {
            if (builtin_fields[index].first == field_name) {
                return index;
            }
        }
    }
    const sema::Type lowered_base = sema::CanonicalizeBuiltinType(base_type);
    if (lowered_base.kind == sema::Type::Kind::kNamed) {
        if (const auto* type_decl = FindTypeDecl(module, lowered_base.name)) {
            const auto fields = InstantiateFields(*type_decl, lowered_base);
            for (std::size_t index = 0; index < fields.size(); ++index) {
                if (fields[index].first == field_name) {
                    return index;
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<sema::Type> FindFieldType(const mir::Module& module,
                                        const sema::Type& base_type,
                                        std::string_view field_name) {
    const auto builtin_fields = sema::BuiltinAggregateFields(base_type);
    for (const auto& field : builtin_fields) {
        if (field.first == field_name) {
            return field.second;
        }
    }
    const sema::Type canonical_base = sema::CanonicalizeBuiltinType(base_type);
    if (canonical_base.kind == sema::Type::Kind::kNamed) {
        if (const auto* type_decl = FindTypeDecl(module, canonical_base.name)) {
            const auto fields = InstantiateFields(*type_decl, canonical_base);
            for (const auto& field : fields) {
                if (field.first == field_name) {
                    return field.second;
                }
            }
        }
    }
    return std::nullopt;
}

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

sema::Type StripMirAliasOrDistinct(const mir::Module& module, sema::Type type) {
    std::unordered_set<std::string> visited;
    while (type.kind == sema::Type::Kind::kNamed) {
        const mir::TypeDecl* type_decl = FindTypeDecl(module, type.name);
        if (type_decl == nullptr) {
            break;
        }
        if (type_decl->kind != mir::TypeDecl::Kind::kDistinct && type_decl->kind != mir::TypeDecl::Kind::kAlias) {
            break;
        }
        if (!visited.insert(type.name).second) {
            break;
        }
        type = InstantiateAliasedType(*type_decl, type);
    }
    return type;
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

std::string LLVMStructInsertBase(const BackendTypeInfo& type_info) {
    return IsAggregateType(type_info) ? "zeroinitializer" : LLVMZeroValue(type_info);
}

bool IsFloatType(const BackendTypeInfo& type_info) {
    return type_info.backend_name == "float" || type_info.backend_name == "double";
}

bool IsProcedureSourceType(std::string_view source_name) {
    return source_name.rfind("proc(", 0) == 0;
}

std::optional<std::string_view> UnsupportedExecutableInstructionName(mir::Instruction::Kind kind) {
    switch (kind) {
        case mir::Instruction::Kind::kAtomicCompareExchange:
            return "atomic_compare_exchange";
        default:
            return std::nullopt;
    }
}

std::string DecodeStringLiteral(std::string_view literal) {
    if (literal.size() >= 2 && literal.front() == '"' && literal.back() == '"') {
        literal.remove_prefix(1);
        literal.remove_suffix(1);
    }

    std::string decoded;
    decoded.reserve(literal.size());
    for (std::size_t index = 0; index < literal.size(); ++index) {
        const char ch = literal[index];
        if (ch != '\\' || index + 1 >= literal.size()) {
            decoded.push_back(ch);
            continue;
        }

        const char escaped = literal[++index];
        switch (escaped) {
            case 'n':
                decoded.push_back('\n');
                break;
            case 'r':
                decoded.push_back('\r');
                break;
            case 't':
                decoded.push_back('\t');
                break;
            case '\\':
                decoded.push_back('\\');
                break;
            case '"':
                decoded.push_back('"');
                break;
            case '0':
                decoded.push_back('\0');
                break;
            default:
                decoded.push_back(escaped);
                break;
        }
    }
    return decoded;
}

std::string EncodeLLVMStringBytes(std::string_view decoded) {
    std::ostringstream encoded;
    for (const unsigned char ch : decoded) {
        if (ch >= 32 && ch <= 126 && ch != '\\' && ch != '"') {
            encoded << static_cast<char>(ch);
            continue;
        }

        encoded << '\\';
        constexpr char kHex[] = "0123456789ABCDEF";
        encoded << kHex[(ch >> 4) & 0xF] << kHex[ch & 0xF];
    }
    encoded << "\\00";
    return encoded.str();
}

std::string EscapeLLVMQuotedString(std::string_view text) {
    std::ostringstream encoded;
    for (const unsigned char ch : text) {
        if (ch >= 32 && ch <= 126 && ch != '\\' && ch != '"') {
            encoded << static_cast<char>(ch);
            continue;
        }

        encoded << '\\';
        constexpr char kHex[] = "0123456789ABCDEF";
        encoded << kHex[(ch >> 4) & 0xF] << kHex[ch & 0xF];
    }
    return encoded.str();
}

std::string FormatLLVMLiteral(const BackendTypeInfo& type_info,
                              std::string_view value) {
    if (type_info.backend_name == "i1") {
        if (value == "true" || value == "1") {
            return "true";
        }
        return "false";
    }

    if (type_info.backend_name == "ptr" && (value == "nil" || value == "null")) {
        return "null";
    }

    return std::string(value);
}

bool RenderLLVMEnumConstValue(const mir::Module& module,
                              const sema::Type& enum_type,
                              const sema::ConstValue& value,
                              const std::filesystem::path& source_path,
                              support::DiagnosticSink& diagnostics,
                              std::string& rendered) {
    if (value.kind != sema::ConstValue::Kind::kEnum || enum_type.kind != sema::Type::Kind::kNamed) {
        return false;
    }

    sema::Type rendered_enum_type = value.enum_type;
    if (rendered_enum_type != enum_type && rendered_enum_type.kind == sema::Type::Kind::kNamed &&
        rendered_enum_type.subtypes == enum_type.subtypes && VariantLeafName(rendered_enum_type.name) == VariantLeafName(enum_type.name)) {
        rendered_enum_type.name = enum_type.name;
    }

    if (rendered_enum_type != enum_type) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend received mismatched enum constant type " +
                               sema::FormatType(value.enum_type) + " for " + sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }

    const auto* type_decl = FindTypeDecl(module, enum_type.name);
    if (type_decl == nullptr || type_decl->kind != mir::TypeDecl::Kind::kEnum) {
        return false;
    }

    if (value.variant_tag < 0 ||
        value.variant_tag >= static_cast<std::int64_t>(type_decl->variants.size())) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend received enum constant tag out of range for type " +
                               sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }

    const auto variant_index = static_cast<std::size_t>(value.variant_tag);
    const auto& variant = type_decl->variants[variant_index];
    if (variant.name != value.variant_name) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend received enum constant variant mismatch for type " +
                               sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }
    if (!variant.payload_fields.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend does not yet support payload enum global constants for type " +
                               sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }

    const auto lowered_layout = LowerEnumLayout(module, *type_decl, enum_type);
    if (!lowered_layout.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend cannot lower enum global constant type " +
                               sema::FormatType(enum_type),
                           diagnostics);
        return false;
    }

    std::ostringstream stream;
    stream << "{ i64 " << value.variant_tag;
    if (lowered_layout->payload_size != 0) {
        stream << ", " << lowered_layout->payload_type.backend_name << " zeroinitializer";
    }
    stream << " }";
    rendered = stream.str();
    return true;
}

bool RenderLLVMGlobalConstValue(const mir::Module& module,
                                sema::Type type,
                                const sema::ConstValue& value,
                                const std::filesystem::path& source_path,
                                support::DiagnosticSink& diagnostics,
                                std::string& rendered) {
    type = sema::CanonicalizeBuiltinType(std::move(type));
    if (type.kind == sema::Type::Kind::kConst && !type.subtypes.empty()) {
        return RenderLLVMGlobalConstValue(module, type.subtypes.front(), value, source_path, diagnostics, rendered);
    }

    const sema::Type canonical_type = StripMirAliasOrDistinct(module, std::move(type));
    const auto lowered_type = LowerTypeInfo(module, canonical_type);
    if (!lowered_type.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend cannot lower global constant type " + sema::FormatType(canonical_type),
                           diagnostics);
        return false;
    }

    if (value.kind != sema::ConstValue::Kind::kAggregate) {
        if (RenderLLVMEnumConstValue(module, canonical_type, value, source_path, diagnostics, rendered)) {
            return true;
        }
        rendered = FormatLLVMLiteral(*lowered_type, value.text);
        return true;
    }

    if (canonical_type.kind == sema::Type::Kind::kArray) {
        if (canonical_type.subtypes.empty()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend requires array element type for global constant " +
                                   sema::FormatType(canonical_type),
                               diagnostics);
            return false;
        }

        const auto length = mc::support::ParseArrayLength(canonical_type.metadata);
        if (!length.has_value() || *length != value.elements.size()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend requires exact array constant element count for global type " +
                                   sema::FormatType(canonical_type),
                               diagnostics);
            return false;
        }

        const auto element_type = LowerTypeInfo(module, canonical_type.subtypes.front());
        if (!element_type.has_value()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend cannot lower array element type for global constant " +
                                   sema::FormatType(canonical_type.subtypes.front()),
                               diagnostics);
            return false;
        }

        std::ostringstream stream;
        stream << '[';
        for (std::size_t index = 0; index < value.elements.size(); ++index) {
            std::string element_text;
            if (!RenderLLVMGlobalConstValue(module,
                                            canonical_type.subtypes.front(),
                                            value.elements[index],
                                            source_path,
                                            diagnostics,
                                            element_text)) {
                return false;
            }
            if (index > 0) {
                stream << ", ";
            }
            stream << element_type->backend_name << ' ' << element_text;
        }
        stream << ']';
        rendered = stream.str();
        return true;
    }

    std::vector<std::pair<std::string, sema::Type>> fields = sema::BuiltinAggregateFields(canonical_type);
    if (fields.empty() && canonical_type.kind == sema::Type::Kind::kTuple) {
        fields.reserve(canonical_type.subtypes.size());
        for (std::size_t index = 0; index < canonical_type.subtypes.size(); ++index) {
            fields.push_back({std::to_string(index), canonical_type.subtypes[index]});
        }
    }
    if (fields.empty() && canonical_type.kind == sema::Type::Kind::kNamed) {
        if (const auto* type_decl = FindTypeDecl(module, canonical_type.name); type_decl != nullptr) {
            fields = InstantiateFields(*type_decl, canonical_type);
        }
    }
    if (fields.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend cannot render structural global constant for type " +
                               sema::FormatType(canonical_type),
                           diagnostics);
        return false;
    }
    if (fields.size() != value.elements.size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend requires one constant element per field for global type " +
                               sema::FormatType(canonical_type),
                           diagnostics);
        return false;
    }

    std::ostringstream stream;
    stream << '{';
    for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto field_type = LowerTypeInfo(module, fields[index].second);
        if (!field_type.has_value()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend cannot lower field type " + sema::FormatType(fields[index].second) +
                                   " for global constant " + sema::FormatType(canonical_type),
                               diagnostics);
            return false;
        }

        std::string field_text;
        if (!RenderLLVMGlobalConstValue(module, fields[index].second, value.elements[index], source_path, diagnostics, field_text)) {
            return false;
        }

        if (index > 0) {
            stream << ", ";
        }
        stream << field_type->backend_name << ' ' << field_text;
    }
    stream << '}';
    rendered = stream.str();
    return true;
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

bool ResolveExecutableLocal(const ExecutableFunctionState& state,
                            const std::string& local_name,
                            const mir::BasicBlock& block,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics,
                            std::string& local_slot) {
    const auto it = state.local_slots.find(local_name);
    if (it != state.local_slots.end()) {
        local_slot = it->second;
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap executable emission references unknown local '" + local_name +
                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
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

std::string PreferredOperandType(const ExecutableValue& lhs,
                                 const ExecutableValue& rhs) {
    if (!IsProcedureSourceType(lhs.type.source_name) && lhs.type.source_name != "int_literal") {
        return lhs.type.backend_name;
    }
    if (!IsProcedureSourceType(rhs.type.source_name) && rhs.type.source_name != "int_literal") {
        return rhs.type.backend_name;
    }
    return lhs.type.backend_name;
}

std::string_view StripConstSourceName(std::string_view source_name) {
    constexpr std::string_view kConstPrefix = "const ";
    while (source_name.rfind(kConstPrefix, 0) == 0) {
        source_name.remove_prefix(kConstPrefix.size());
    }
    return source_name;
}

std::string_view NamedSourceDeclName(std::string_view source_name) {
    source_name = StripConstSourceName(source_name);
    const std::size_t generic_start = source_name.find('<');
    if (generic_start != std::string_view::npos) {
        source_name = source_name.substr(0, generic_start);
    }
    return source_name;
}

std::optional<std::size_t> ParseEnumPayloadSize(const BackendTypeInfo& type_info) {
    if (type_info.backend_name == "{i64}") {
        return 0;
    }

    constexpr std::string_view kPrefix = "{i64, ";
    if (type_info.backend_name.rfind(kPrefix, 0) != 0 || type_info.backend_name.size() <= kPrefix.size() + 1 ||
        type_info.backend_name.back() != '}') {
        return std::nullopt;
    }

    const std::string_view payload_type = std::string_view(type_info.backend_name).substr(
        kPrefix.size(), type_info.backend_name.size() - kPrefix.size() - 1);
    return ParseBackendArrayLength(payload_type);
}

struct EnumCompareInfo {
    sema::Type source_type;
    const mir::TypeDecl* type_decl = nullptr;
    EnumBackendLayout layout;
    std::size_t payload_size = 0;
    std::vector<std::vector<sema::Type>> variant_field_types;
};

std::optional<EnumCompareInfo> ResolveEnumCompareInfo(const mir::Module& module,
                                                      const ExecutableValue& value) {
    if (!value.source_type.has_value()) {
        return std::nullopt;
    }

    sema::Type source_type = StripMirAliasOrDistinct(module, *value.source_type);
    while (source_type.kind == sema::Type::Kind::kConst && !source_type.subtypes.empty()) {
        source_type = StripMirAliasOrDistinct(module, source_type.subtypes.front());
    }

    if (source_type.kind != sema::Type::Kind::kNamed) {
        return std::nullopt;
    }

    const mir::TypeDecl* type_decl = FindTypeDecl(module, source_type.name);
    if (type_decl == nullptr || type_decl->kind != mir::TypeDecl::Kind::kEnum) {
        return std::nullopt;
    }

    const auto lowered_layout = LowerEnumLayout(module, *type_decl, source_type);
    if (!lowered_layout.has_value()) {
        return std::nullopt;
    }

    const auto payload_size = ParseEnumPayloadSize(value.type);
    if (!payload_size.has_value()) {
        return std::nullopt;
    }

    std::vector<std::vector<sema::Type>> variant_field_types;
    variant_field_types.reserve(type_decl->variants.size());
    for (const auto& raw_variant : type_decl->variants) {
        mir::VariantDecl variant = raw_variant;
        if (!type_decl->type_params.empty()) {
            for (auto& field : variant.payload_fields) {
                field.second = sema::SubstituteTypeParams(std::move(field.second), type_decl->type_params, source_type.subtypes);
            }
        }

        std::vector<sema::Type> field_types;
        field_types.reserve(variant.payload_fields.size());
        for (const auto& field : variant.payload_fields) {
            field_types.push_back(field.second);
        }
        variant_field_types.push_back(std::move(field_types));
    }

    return EnumCompareInfo{
        .source_type = std::move(source_type),
        .type_decl = type_decl,
        .layout = *lowered_layout,
        .payload_size = *payload_size,
        .variant_field_types = std::move(variant_field_types),
    };
}

std::string EmitEnumPayloadFieldPointer(const std::string& enum_slot,
                                       const EnumBackendLayout& layout,
                                       std::size_t payload_offset,
                                       std::size_t function_index,
                                       std::size_t block_index,
                                       std::size_t instruction_index,
                                       std::string_view suffix,
                                       std::vector<std::string>& output_lines);

std::string EmitEnumPayloadChunkLoad(const std::string& aggregate_slot,
                                     const ExecutableValue& value,
                                     std::size_t payload_size,
                                     std::size_t payload_offset,
                                     std::size_t chunk_size,
                                     std::size_t function_index,
                                     std::size_t block_index,
                                     std::size_t instruction_index,
                                     std::string_view suffix,
                                     std::vector<std::string>& output_lines) {
    const std::string payload_ptr = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix) + ".payload";
    output_lines.push_back(payload_ptr + " = getelementptr inbounds " + value.type.backend_name + ", ptr " + aggregate_slot + ", i64 0, i32 1");

    const std::string byte_ptr = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix) + ".byte";
    output_lines.push_back(byte_ptr + " = getelementptr inbounds [" + std::to_string(payload_size) + " x i8], ptr " + payload_ptr + ", i64 0, i64 " +
                           std::to_string(payload_offset));

    const std::string chunk_type = "i" + std::to_string(chunk_size * 8);
    const std::string chunk_value = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix) + ".load";
    output_lines.push_back(chunk_value + " = load " + chunk_type + ", ptr " + byte_ptr + ", align 1");
    return chunk_value;
}

bool EmitScalarComparison(std::string_view op,
                          const ExecutableValue& lhs,
                          const ExecutableValue& rhs,
                          std::size_t function_index,
                          std::size_t block_index,
                          std::size_t instruction_index,
                          std::string_view suffix,
                          std::vector<std::string>& output_lines,
                          std::string& result) {
    const std::string temp = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix);
    if (IsFloatType(lhs.type)) {
        const char* predicate = nullptr;
        if (op == "==") {
            predicate = "oeq";
        } else if (op == "!=") {
            predicate = "one";
        } else if (op == "<") {
            predicate = "olt";
        } else if (op == "<=") {
            predicate = "ole";
        } else if (op == ">") {
            predicate = "ogt";
        } else if (op == ">=") {
            predicate = "oge";
        }
        if (predicate == nullptr) {
            return false;
        }
        output_lines.push_back(temp + " = fcmp " + std::string(predicate) + " " + lhs.type.backend_name + " " + lhs.text + ", " + rhs.text);
        result = temp;
        return true;
    }

    if (lhs.type.backend_name == "ptr" && op != "==" && op != "!=") {
        return false;
    }

    const bool is_unsigned = lhs.type.backend_name == "i1" || IsUnsignedSourceType(lhs.type.source_name) || lhs.type.backend_name == "ptr";
    const char* predicate = nullptr;
    if (op == "==") {
        predicate = "eq";
    } else if (op == "!=") {
        predicate = "ne";
    } else if (op == "<") {
        predicate = is_unsigned ? "ult" : "slt";
    } else if (op == "<=") {
        predicate = is_unsigned ? "ule" : "sle";
    } else if (op == ">") {
        predicate = is_unsigned ? "ugt" : "sgt";
    } else if (op == ">=") {
        predicate = is_unsigned ? "uge" : "sge";
    }
    if (predicate == nullptr) {
        return false;
    }

    output_lines.push_back(temp + " = icmp " + std::string(predicate) + " " + lhs.type.backend_name + " " + lhs.text + ", " + rhs.text);
    result = temp;
    return true;
}

bool EmitOrderedEnumFieldComparison(const mir::Module& module,
                                    const sema::Type& field_type,
                                    const BackendTypeInfo& backend_type,
                                    const std::string& lhs_text,
                                    const std::string& rhs_text,
                                    std::size_t function_index,
                                    std::size_t block_index,
                                    std::size_t instruction_index,
                                    std::string_view suffix,
                                    const std::filesystem::path& source_path,
                                    support::DiagnosticSink& diagnostics,
                                    std::vector<std::string>& output_lines,
                                    std::string& eq_result,
                                    std::string& lt_result,
                                    std::string& gt_result) {
    const sema::Type stripped_type = StripMirAliasOrDistinct(module, field_type);
    ExecutableValue lhs_value{
        .text = lhs_text,
        .type = backend_type,
        .source_type = stripped_type,
    };
    ExecutableValue rhs_value{
        .text = rhs_text,
        .type = backend_type,
        .source_type = stripped_type,
    };

    if (const auto enum_info = ResolveEnumCompareInfo(module, lhs_value); enum_info.has_value()) {
        const auto emit_enum_ordering_components = [&](const ExecutableValue& enum_lhs,
                                                       const ExecutableValue& enum_rhs,
                                                       const EnumCompareInfo& nested_info,
                                                       std::string_view nested_suffix,
                                                       std::string& nested_eq,
                                                       std::string& nested_lt,
                                                       std::string& nested_gt,
                                                       const auto& self) -> bool {
            const std::string nested_base = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(nested_suffix);
            const std::string nested_lhs_tag = nested_base + ".lhs.tag";
            const std::string nested_rhs_tag = nested_base + ".rhs.tag";
            const std::string nested_tag_eq = nested_base + ".tag.eq";
            const std::string nested_tag_lt = nested_base + ".tag.lt";
            const std::string nested_tag_gt = nested_base + ".tag.gt";
            output_lines.push_back(nested_lhs_tag + " = extractvalue " + enum_lhs.type.backend_name + " " + enum_lhs.text + ", 0");
            output_lines.push_back(nested_rhs_tag + " = extractvalue " + enum_rhs.type.backend_name + " " + enum_rhs.text + ", 0");
            output_lines.push_back(nested_tag_eq + " = icmp eq i64 " + nested_lhs_tag + ", " + nested_rhs_tag);
            output_lines.push_back(nested_tag_lt + " = icmp slt i64 " + nested_lhs_tag + ", " + nested_rhs_tag);
            output_lines.push_back(nested_tag_gt + " = icmp sgt i64 " + nested_lhs_tag + ", " + nested_rhs_tag);

            if (nested_info.payload_size == 0) {
                nested_eq = nested_tag_eq;
                nested_lt = nested_tag_lt;
                nested_gt = nested_tag_gt;
                return true;
            }

            const std::string nested_lhs_slot = EmitAggregateStackSlot(enum_lhs,
                                                                       function_index,
                                                                       block_index,
                                                                       instruction_index,
                                                                       std::string(nested_suffix) + ".lhs.slot",
                                                                       output_lines);
            const std::string nested_rhs_slot = EmitAggregateStackSlot(enum_rhs,
                                                                       function_index,
                                                                       block_index,
                                                                       instruction_index,
                                                                       std::string(nested_suffix) + ".rhs.slot",
                                                                       output_lines);

            std::string same_variant_eq = "false";
            std::string same_variant_lt = "false";
            std::string same_variant_gt = "false";
            for (std::size_t variant_index = nested_info.variant_field_types.size(); variant_index > 0; --variant_index) {
                const std::size_t actual_index = variant_index - 1;
                std::string variant_eq = "true";
                std::string variant_lt = "false";
                std::string variant_gt = "false";
                for (std::size_t field_index = nested_info.variant_field_types[actual_index].size(); field_index > 0; --field_index) {
                    const std::size_t actual_field_index = field_index - 1;
                    const std::string field_suffix_base = std::string(nested_suffix) + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index);
                    const std::string nested_lhs_field_slot = EmitEnumPayloadFieldPointer(nested_lhs_slot,
                                                                                          nested_info.layout,
                                                                                          nested_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                                          function_index,
                                                                                          block_index,
                                                                                          instruction_index,
                                                                                          field_suffix_base + ".lhs.ptr",
                                                                                          output_lines);
                    const std::string nested_rhs_field_slot = EmitEnumPayloadFieldPointer(nested_rhs_slot,
                                                                                          nested_info.layout,
                                                                                          nested_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                                          function_index,
                                                                                          block_index,
                                                                                          instruction_index,
                                                                                          field_suffix_base + ".rhs.ptr",
                                                                                          output_lines);
                    const BackendTypeInfo& nested_field_backend_type = nested_info.layout.variant_field_types[actual_index][actual_field_index];
                    const std::string nested_lhs_field = nested_base + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".lhs";
                    const std::string nested_rhs_field = nested_base + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".rhs";
                    output_lines.push_back(nested_lhs_field + " = load " + nested_field_backend_type.backend_name + ", ptr " + nested_lhs_field_slot + ", align " +
                                           std::to_string(nested_field_backend_type.alignment));
                    output_lines.push_back(nested_rhs_field + " = load " + nested_field_backend_type.backend_name + ", ptr " + nested_rhs_field_slot + ", align " +
                                           std::to_string(nested_field_backend_type.alignment));

                    ExecutableValue nested_lhs_field_value{
                        .text = nested_lhs_field,
                        .type = nested_field_backend_type,
                        .source_type = nested_info.variant_field_types[actual_index][actual_field_index],
                    };
                    ExecutableValue nested_rhs_field_value{
                        .text = nested_rhs_field,
                        .type = nested_field_backend_type,
                        .source_type = nested_info.variant_field_types[actual_index][actual_field_index],
                    };

                    std::string field_eq;
                    std::string field_lt;
                    std::string field_gt;
                    if (const auto deeper_enum = ResolveEnumCompareInfo(module, nested_lhs_field_value); deeper_enum.has_value()) {
                        if (!self(nested_lhs_field_value,
                                  nested_rhs_field_value,
                                  *deeper_enum,
                                  field_suffix_base,
                                  field_eq,
                                  field_lt,
                                  field_gt,
                                  self)) {
                            return false;
                        }
                    } else if (!EmitScalarComparison("==", nested_lhs_field_value, nested_rhs_field_value, function_index, block_index, instruction_index, field_suffix_base + ".eq", output_lines, field_eq) ||
                               !EmitScalarComparison("<", nested_lhs_field_value, nested_rhs_field_value, function_index, block_index, instruction_index, field_suffix_base + ".lt", output_lines, field_lt) ||
                               !EmitScalarComparison(">", nested_lhs_field_value, nested_rhs_field_value, function_index, block_index, instruction_index, field_suffix_base + ".gt", output_lines, field_gt)) {
                        ReportBackendError(source_path,
                                           "LLVM bootstrap executable emission does not support payload-bearing enum ordering for field type '" +
                                               sema::FormatType(nested_info.variant_field_types[actual_index][actual_field_index]) + "'",
                                           diagnostics);
                        return false;
                    }

                    const std::string next_variant_eq = nested_base + ".variant." + std::to_string(actual_index) + ".eq." + std::to_string(actual_field_index);
                    const std::string next_variant_lt = nested_base + ".variant." + std::to_string(actual_index) + ".lt." + std::to_string(actual_field_index);
                    const std::string next_variant_gt = nested_base + ".variant." + std::to_string(actual_index) + ".gt." + std::to_string(actual_field_index);
                    output_lines.push_back(next_variant_eq + " = and i1 " + field_eq + ", " + variant_eq);
                    output_lines.push_back(next_variant_lt + " = select i1 " + field_eq + ", i1 " + variant_lt + ", i1 " + field_lt);
                    output_lines.push_back(next_variant_gt + " = select i1 " + field_eq + ", i1 " + variant_gt + ", i1 " + field_gt);
                    variant_eq = next_variant_eq;
                    variant_lt = next_variant_lt;
                    variant_gt = next_variant_gt;
                }

                const std::string variant_match = nested_base + ".variant." + std::to_string(actual_index) + ".match";
                const std::string next_same_eq = nested_base + ".same.eq." + std::to_string(actual_index);
                const std::string next_same_lt = nested_base + ".same.lt." + std::to_string(actual_index);
                const std::string next_same_gt = nested_base + ".same.gt." + std::to_string(actual_index);
                output_lines.push_back(variant_match + " = icmp eq i64 " + nested_lhs_tag + ", " + std::to_string(actual_index));
                output_lines.push_back(next_same_eq + " = select i1 " + variant_match + ", i1 " + variant_eq + ", i1 " + same_variant_eq);
                output_lines.push_back(next_same_lt + " = select i1 " + variant_match + ", i1 " + variant_lt + ", i1 " + same_variant_lt);
                output_lines.push_back(next_same_gt + " = select i1 " + variant_match + ", i1 " + variant_gt + ", i1 " + same_variant_gt);
                same_variant_eq = next_same_eq;
                same_variant_lt = next_same_lt;
                same_variant_gt = next_same_gt;
            }

            nested_eq = nested_base + ".eq";
            nested_lt = nested_base + ".lt";
            nested_gt = nested_base + ".gt";
            output_lines.push_back(nested_eq + " = select i1 " + nested_tag_eq + ", i1 " + same_variant_eq + ", i1 false");
            output_lines.push_back(nested_lt + " = select i1 " + nested_tag_eq + ", i1 " + same_variant_lt + ", i1 " + nested_tag_lt);
            output_lines.push_back(nested_gt + " = select i1 " + nested_tag_eq + ", i1 " + same_variant_gt + ", i1 " + nested_tag_gt);
            return true;
        };

        return emit_enum_ordering_components(lhs_value,
                                             rhs_value,
                                             *enum_info,
                                             suffix,
                                             eq_result,
                                             lt_result,
                                             gt_result,
                                             emit_enum_ordering_components);
    }

    if (!EmitScalarComparison("==", lhs_value, rhs_value, function_index, block_index, instruction_index, std::string(suffix) + ".eq", output_lines, eq_result) ||
        !EmitScalarComparison("<", lhs_value, rhs_value, function_index, block_index, instruction_index, std::string(suffix) + ".lt", output_lines, lt_result) ||
        !EmitScalarComparison(">", lhs_value, rhs_value, function_index, block_index, instruction_index, std::string(suffix) + ".gt", output_lines, gt_result)) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission does not support payload-bearing enum ordering for field type '" +
                               sema::FormatType(field_type) + "'",
                           diagnostics);
        return false;
    }

    return true;
}

bool EmitEnumPayloadEqualityComparison(const mir::Module& module,
                                       const EnumCompareInfo& compare_info,
                                       const std::string& lhs_slot,
                                       const std::string& rhs_slot,
                                       const std::string& lhs_tag,
                                       std::size_t function_index,
                                       std::size_t block_index,
                                       std::size_t instruction_index,
                                       const std::filesystem::path& source_path,
                                       support::DiagnosticSink& diagnostics,
                                       std::vector<std::string>& output_lines,
                                       std::string& payload_eq_result) {
    const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
    std::string same_tag_eq = "true";
    for (std::size_t variant_index = compare_info.variant_field_types.size(); variant_index > 0; --variant_index) {
        const std::size_t actual_index = variant_index - 1;
        std::string variant_eq = "true";
        for (std::size_t field_index = compare_info.variant_field_types[actual_index].size(); field_index > 0; --field_index) {
            const std::size_t actual_field_index = field_index - 1;
            const std::string field_suffix = "enum.equal.variant." + std::to_string(actual_index) + ".field." +
                                             std::to_string(actual_field_index);
            const std::string lhs_field_slot = EmitEnumPayloadFieldPointer(lhs_slot,
                                                                           compare_info.layout,
                                                                           compare_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                           function_index,
                                                                           block_index,
                                                                           instruction_index,
                                                                           field_suffix + ".lhs.ptr",
                                                                           output_lines);
            const std::string rhs_field_slot = EmitEnumPayloadFieldPointer(rhs_slot,
                                                                           compare_info.layout,
                                                                           compare_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                           function_index,
                                                                           block_index,
                                                                           instruction_index,
                                                                           field_suffix + ".rhs.ptr",
                                                                           output_lines);
            const BackendTypeInfo& field_backend_type = compare_info.layout.variant_field_types[actual_index][actual_field_index];
            const std::string lhs_field = temp + ".variant." + std::to_string(actual_index) + ".field." +
                                          std::to_string(actual_field_index) + ".lhs";
            const std::string rhs_field = temp + ".variant." + std::to_string(actual_index) + ".field." +
                                          std::to_string(actual_field_index) + ".rhs";
            output_lines.push_back(lhs_field + " = load " + field_backend_type.backend_name + ", ptr " + lhs_field_slot + ", align " +
                                   std::to_string(field_backend_type.alignment));
            output_lines.push_back(rhs_field + " = load " + field_backend_type.backend_name + ", ptr " + rhs_field_slot + ", align " +
                                   std::to_string(field_backend_type.alignment));

            std::string field_eq;
            std::string field_lt;
            std::string field_gt;
            if (!EmitOrderedEnumFieldComparison(module,
                                                compare_info.variant_field_types[actual_index][actual_field_index],
                                                field_backend_type,
                                                lhs_field,
                                                rhs_field,
                                                function_index,
                                                block_index,
                                                instruction_index,
                                                field_suffix,
                                                source_path,
                                                diagnostics,
                                                output_lines,
                                                field_eq,
                                                field_lt,
                                                field_gt)) {
                return false;
            }

            const std::string next_variant_eq = temp + ".variant." + std::to_string(actual_index) + ".eq." +
                                                std::to_string(actual_field_index);
            output_lines.push_back(next_variant_eq + " = and i1 " + variant_eq + ", " + field_eq);
            variant_eq = next_variant_eq;
        }

        const std::string variant_match = temp + ".variant." + std::to_string(actual_index) + ".match";
        const std::string next_same_tag = temp + ".same_tag.eq." + std::to_string(actual_index);
        output_lines.push_back(variant_match + " = icmp eq i64 " + lhs_tag + ", " + std::to_string(actual_index));
        output_lines.push_back(next_same_tag + " = select i1 " + variant_match + ", i1 " + variant_eq + ", i1 " + same_tag_eq);
        same_tag_eq = next_same_tag;
    }

    payload_eq_result = same_tag_eq;
    return true;
}

void EmitEnumCompare(const mir::Instruction& instruction,
                     std::size_t function_index,
                     std::size_t block_index,
                     std::size_t instruction_index,
                     std::string_view compare_predicate,
                     const ExecutableValue& lhs,
                     const ExecutableValue& rhs,
                     const EnumCompareInfo& compare_info,
                     const std::filesystem::path& source_path,
                     const mir::BasicBlock& block,
                     support::DiagnosticSink& diagnostics,
                     const BackendTypeInfo& result_type,
                     ExecutableFunctionState& state,
                     std::vector<std::string>& output_lines,
                     bool& emitted) {
    emitted = false;
    const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
    const std::string lhs_tag = temp + ".lhs.tag";
    const std::string rhs_tag = temp + ".rhs.tag";
    output_lines.push_back(lhs_tag + " = extractvalue " + lhs.type.backend_name + " " + lhs.text + ", 0");
    output_lines.push_back(rhs_tag + " = extractvalue " + rhs.type.backend_name + " " + rhs.text + ", 0");

    if (compare_info.payload_size == 0) {
        output_lines.push_back(temp + " = icmp " + std::string(compare_predicate) + " i64 " + lhs_tag + ", " + rhs_tag);
        RecordExecutableValue(state, instruction.result, temp, result_type);
        emitted = true;
        return;
    }

    const std::string tag_eq = temp + ".tag.eq";
    output_lines.push_back(tag_eq + " = icmp eq i64 " + lhs_tag + ", " + rhs_tag);

    const std::string lhs_slot = EmitAggregateStackSlot(lhs,
                                                        function_index,
                                                        block_index,
                                                        instruction_index,
                                                        "enum.compare.lhs.slot",
                                                        output_lines);
    const std::string rhs_slot = EmitAggregateStackSlot(rhs,
                                                        function_index,
                                                        block_index,
                                                        instruction_index,
                                                        "enum.compare.rhs.slot",
                                                        output_lines);

    if (instruction.op == "==" || instruction.op == "!=") {
        std::string payload_eq;
        if (!EmitEnumPayloadEqualityComparison(*state.module,
                                               compare_info,
                                               lhs_slot,
                                               rhs_slot,
                                               lhs_tag,
                                               function_index,
                                               block_index,
                                               instruction_index,
                                               source_path,
                                               diagnostics,
                                               output_lines,
                                               payload_eq)) {
            return;
        }

        if (instruction.op == "==") {
            output_lines.push_back(temp + " = and i1 " + tag_eq + ", " + payload_eq);
        } else {
            const std::string tag_ne = temp + ".tag.ne";
            const std::string payload_ne = temp + ".payload.ne";
            output_lines.push_back(tag_ne + " = icmp ne i64 " + lhs_tag + ", " + rhs_tag);
            output_lines.push_back(payload_ne + " = xor i1 " + payload_eq + ", true");
            output_lines.push_back(temp + " = or i1 " + tag_ne + ", " + payload_ne);
        }
        RecordExecutableValue(state, instruction.result, temp, result_type);
        emitted = true;
        return;
    }

    const std::string tag_order = temp + ".tag.order";
    output_lines.push_back(tag_order + " = icmp " + std::string(compare_predicate) + " i64 " + lhs_tag + ", " + rhs_tag);

    const std::string default_same_tag = instruction.op == "<=" || instruction.op == ">=" ? "true" : "false";
    std::string same_tag_result = default_same_tag;
    for (std::size_t variant_index = compare_info.variant_field_types.size(); variant_index > 0; --variant_index) {
        const std::size_t actual_index = variant_index - 1;
        std::string variant_result = default_same_tag;
        for (std::size_t field_index = compare_info.variant_field_types[actual_index].size(); field_index > 0; --field_index) {
            const std::size_t actual_field_index = field_index - 1;
            const std::string field_suffix = "enum.order.variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index);
            const std::string lhs_field_slot = EmitEnumPayloadFieldPointer(lhs_slot,
                                                                           compare_info.layout,
                                                                           compare_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                           function_index,
                                                                           block_index,
                                                                           instruction_index,
                                                                           field_suffix + ".lhs.ptr",
                                                                           output_lines);
            const std::string rhs_field_slot = EmitEnumPayloadFieldPointer(rhs_slot,
                                                                           compare_info.layout,
                                                                           compare_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                           function_index,
                                                                           block_index,
                                                                           instruction_index,
                                                                           field_suffix + ".rhs.ptr",
                                                                           output_lines);
            const BackendTypeInfo& field_backend_type = compare_info.layout.variant_field_types[actual_index][actual_field_index];
            const std::string lhs_field = temp + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".lhs";
            const std::string rhs_field = temp + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".rhs";
            output_lines.push_back(lhs_field + " = load " + field_backend_type.backend_name + ", ptr " + lhs_field_slot + ", align " +
                                   std::to_string(field_backend_type.alignment));
            output_lines.push_back(rhs_field + " = load " + field_backend_type.backend_name + ", ptr " + rhs_field_slot + ", align " +
                                   std::to_string(field_backend_type.alignment));

            std::string field_eq;
            std::string field_lt;
            std::string field_gt;
            if (!EmitOrderedEnumFieldComparison(*state.module,
                                                compare_info.variant_field_types[actual_index][actual_field_index],
                                                field_backend_type,
                                                lhs_field,
                                                rhs_field,
                                                function_index,
                                                block_index,
                                                instruction_index,
                                                field_suffix,
                                                source_path,
                                                diagnostics,
                                                output_lines,
                                                field_eq,
                                                field_lt,
                                                field_gt)) {
                return;
            }

            const std::string selected_order = instruction.op == "<" || instruction.op == "<=" ? field_lt : field_gt;
            const std::string next_variant_result = temp + ".variant." + std::to_string(actual_index) + ".step." + std::to_string(actual_field_index);
            output_lines.push_back(next_variant_result + " = select i1 " + field_eq + ", i1 " + variant_result + ", i1 " + selected_order);
            variant_result = next_variant_result;
        }

        const std::string variant_match = temp + ".variant." + std::to_string(actual_index) + ".match";
        const std::string next_same_tag = temp + ".same_tag." + std::to_string(actual_index);
        output_lines.push_back(variant_match + " = icmp eq i64 " + lhs_tag + ", " + std::to_string(actual_index));
        output_lines.push_back(next_same_tag + " = select i1 " + variant_match + ", i1 " + variant_result + ", i1 " + same_tag_result);
        same_tag_result = next_same_tag;
    }

    output_lines.push_back(temp + " = select i1 " + tag_eq + ", i1 " + same_tag_result + ", i1 " + tag_order);
    RecordExecutableValue(state, instruction.result, temp, result_type);
    emitted = true;
}

struct BinaryOpcodeEntry {
    std::string_view op;
    const char* float_opcode;
    const char* unsigned_opcode;
    const char* signed_opcode;
};

constexpr BinaryOpcodeEntry kBinaryOpcodeTable[] = {
    {"+", "fadd", "add", "add"},
    {"-", "fsub", "sub", "sub"},
    {"*", "fmul", "mul", "mul"},
    {"/", "fdiv", "udiv", "sdiv"},
    {"%", "frem", "urem", "srem"},
    {"<<", nullptr, "shl", "shl"},
    {">>", nullptr, "lshr", "ashr"},
    {"&", nullptr, "and", "and"},
    {"|", nullptr, "or", "or"},
    {"^", nullptr, "xor", "xor"},
};

struct ComparePredicateEntry {
    std::string_view op;
    const char* float_predicate;
    const char* unsigned_predicate;
    const char* signed_predicate;
};

constexpr ComparePredicateEntry kComparePredicateTable[] = {
    {"==", "oeq", "eq", "eq"},
    {"!=", "one", "ne", "ne"},
    {"<", "olt", "ult", "slt"},
    {"<=", "ole", "ule", "sle"},
    {">", "ogt", "ugt", "sgt"},
    {">=", "oge", "uge", "sge"},
};

std::string BinaryOpcodeForLLVM(const mir::Instruction& instruction,
                                const ExecutableValue& lhs,
                                const BackendTypeInfo& result_type) {
    const bool is_float = IsFloatType(result_type);
    const bool is_unsigned = IsUnsignedSourceType(lhs.type.source_name);

    for (const auto& entry : kBinaryOpcodeTable) {
        if (instruction.op != entry.op) {
            continue;
        }
        if (is_float) {
            return entry.float_opcode == nullptr ? std::string() : std::string(entry.float_opcode);
        }
        return std::string(is_unsigned ? entry.unsigned_opcode : entry.signed_opcode);
    }
    return {};
}

std::string ComparePredicateForLLVM(const mir::Instruction& instruction,
                                    const ExecutableValue& lhs) {
    const bool is_float = IsFloatType(lhs.type);
    const bool is_unsigned = IsUnsignedSourceType(lhs.type.source_name);

    for (const auto& entry : kComparePredicateTable) {
        if (instruction.op != entry.op) {
            continue;
        }
        if (is_float) {
            return std::string(entry.float_predicate);
        }
        return std::string(is_unsigned ? entry.unsigned_predicate : entry.signed_predicate);
    }
    return {};
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

struct CheckedHelperRequirements {
    std::set<std::string> div_backend_names;
    std::set<std::size_t> shift_widths;
};

struct ExecutableRuntimeRequirements {
    bool needs_malloc = false;
    bool needs_free = false;
    bool needs_panic = false;
};

void SeedFunctionValueTypes(const mir::Function& function,
                            std::unordered_map<std::string, sema::Type>& value_types) {
    value_types.clear();
    value_types.reserve(function.locals.size());
    for (const auto& local : function.locals) {
        value_types.emplace(local.name, local.type);
    }
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (!instruction.result.empty()) {
                value_types.insert_or_assign(instruction.result, instruction.type);
            }
        }
    }
}

const sema::Type* LookupCheckedHelperValueType(const mir::Function& function,
                                               const mir::BasicBlock& block,
                                               const std::unordered_map<std::string, sema::Type>& value_types,
                                               std::string_view value_name,
                                               std::string_view check_name,
                                               const std::filesystem::path& source_path,
                                               support::DiagnosticSink& diagnostics) {
    const auto it = value_types.find(std::string(value_name));
    if (it != value_types.end()) {
        return &it->second;
    }
    ReportBackendError(source_path,
                       "LLVM bootstrap executable emission could not resolve " + std::string(check_name) +
                           " operand type for '" + std::string(value_name) + "' in function '" + function.name +
                           "' block '" + block.label + "'",
                       diagnostics);
    return nullptr;
}

bool AddDivCheckRequirement(const mir::Module& module,
                            const mir::Function& function,
                            const mir::BasicBlock& block,
                            const mir::Instruction& instruction,
                            const std::unordered_map<std::string, sema::Type>& value_types,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics,
                            CheckedHelperRequirements& requirements) {
    if (instruction.operands.size() != 2) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires div_check to use exactly two operands in function '" +
                               function.name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }
    const sema::Type* rhs_type = LookupCheckedHelperValueType(function,
                                                              block,
                                                              value_types,
                                                              instruction.operands[1],
                                                              "div_check",
                                                              source_path,
                                                              diagnostics);
    if (rhs_type == nullptr) {
        return false;
    }
    BackendTypeInfo lowered_type;
    if (!LowerInstructionType(module,
                              *rhs_type,
                              source_path,
                              diagnostics,
                              "checked-helper analysis for div_check in function '" + function.name + "' block '" +
                                  block.label + "'",
                              lowered_type)) {
        return false;
    }
    requirements.div_backend_names.insert(lowered_type.backend_name);
    return true;
}

bool AddShiftCheckRequirement(const mir::Module& module,
                              const mir::Function& function,
                              const mir::BasicBlock& block,
                              const mir::Instruction& instruction,
                              const std::unordered_map<std::string, sema::Type>& value_types,
                              const std::filesystem::path& source_path,
                              support::DiagnosticSink& diagnostics,
                              CheckedHelperRequirements& requirements) {
    if (instruction.operands.size() != 2) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires shift_check to use exactly two operands in function '" +
                               function.name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }
    const sema::Type* value_type = LookupCheckedHelperValueType(function,
                                                                block,
                                                                value_types,
                                                                instruction.operands[0],
                                                                "shift_check",
                                                                source_path,
                                                                diagnostics);
    if (value_type == nullptr) {
        return false;
    }
    BackendTypeInfo lowered_type;
    if (!LowerInstructionType(module,
                              *value_type,
                              source_path,
                              diagnostics,
                              "checked-helper analysis for shift_check in function '" + function.name + "' block '" +
                                  block.label + "'",
                              lowered_type)) {
        return false;
    }
    const std::size_t bit_width = IntegerBitWidth(lowered_type);
    if (bit_width == 0) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires integer shift_check values in function '" +
                               function.name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }
    requirements.shift_widths.insert(bit_width);
    return true;
}

bool CollectCheckedHelperRequirements(const mir::Module& module,
                                      const std::filesystem::path& source_path,
                                      support::DiagnosticSink& diagnostics,
                                      CheckedHelperRequirements& requirements) {
    std::unordered_map<std::string, sema::Type> value_types;
    for (const auto& function : module.functions) {
        SeedFunctionValueTypes(function, value_types);
        for (const auto& block : function.blocks) {
            for (const auto& instruction : block.instructions) {
                switch (instruction.kind) {
                    case mir::Instruction::Kind::kDivCheck:
                        if (!AddDivCheckRequirement(module,
                                                    function,
                                                    block,
                                                    instruction,
                                                    value_types,
                                                    source_path,
                                                    diagnostics,
                                                    requirements)) {
                            return false;
                        }
                        break;
                    case mir::Instruction::Kind::kShiftCheck:
                        if (!AddShiftCheckRequirement(module,
                                                      function,
                                                      block,
                                                      instruction,
                                                      value_types,
                                                      source_path,
                                                      diagnostics,
                                                      requirements)) {
                            return false;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
    return true;
}

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

bool EmitBoundsCheckCall(const mir::Instruction& instruction,
                         std::size_t function_index,
                         std::size_t block_index,
                         std::size_t instruction_index,
                         const mir::BasicBlock& block,
                         const std::filesystem::path& source_path,
                         support::DiagnosticSink& diagnostics,
                         ExecutableFunctionState& state,
                         std::vector<std::string>& output_lines) {
    if (!RequireOperandRange(instruction,
                             2,
                             3,
                             "LLVM bootstrap executable emission",
                             "bounds_check to use two or three operands",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics)) {
        return false;
    }

    std::vector<std::string> operands_i64;
    operands_i64.reserve(instruction.operands.size());
    for (std::size_t index = 0; index < instruction.operands.size(); ++index) {
        ExecutableValue operand;
        if (!ResolveExecutableValue(state, instruction.operands[index], block, source_path, diagnostics, operand)) {
            return false;
        }
        std::string extended;
        if (!ExtendIntegerToI64(operand,
                                function_index,
                                block_index,
                                instruction_index + index,
                                "bounds",
                                output_lines,
                                extended)) {
            return false;
        }
        operands_i64.push_back(std::move(extended));
    }

    if (instruction.op == "index") {
        output_lines.push_back("call void @__mc_check_bounds_index(i64 " + operands_i64[0] + ", i64 " + operands_i64[1] + ")");
        return true;
    }
    if (instruction.op == "slice") {
        if (operands_i64.size() == 2) {
            output_lines.push_back("call void @__mc_check_bounds_index(i64 " + operands_i64[0] + ", i64 " + operands_i64[1] + ")");
            return true;
        }
        output_lines.push_back("call void @__mc_check_bounds_slice(i64 " + operands_i64[0] + ", i64 " + operands_i64[1] + ", i64 " + operands_i64[2] + ")");
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap executable emission does not recognize bounds_check mode '" + instruction.op +
                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

bool EmitDivCheckCall(const mir::Instruction& instruction,
                      std::size_t function_index,
                      std::size_t block_index,
                      std::size_t instruction_index,
                      const mir::BasicBlock& block,
                      const std::filesystem::path& source_path,
                      support::DiagnosticSink& diagnostics,
                      ExecutableFunctionState& state,
                      std::vector<std::string>& output_lines) {
    ExecutableValue lhs;
    ExecutableValue rhs;
    if (!RequireOperandCount(instruction,
                             2,
                             "LLVM bootstrap executable emission",
                             "div_check",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, lhs) ||
        !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, rhs)) {
        return false;
    }
    output_lines.push_back("call void " + DivCheckHelperName(rhs.type) + "(" + rhs.type.backend_name + " " + rhs.text + ")");
    return true;
}

bool EmitShiftCheckCall(const mir::Instruction& instruction,
                        std::size_t function_index,
                        std::size_t block_index,
                        std::size_t instruction_index,
                        const mir::BasicBlock& block,
                        const std::filesystem::path& source_path,
                        support::DiagnosticSink& diagnostics,
                        ExecutableFunctionState& state,
                        std::vector<std::string>& output_lines) {
    ExecutableValue value;
    ExecutableValue count;
    if (!RequireOperandCount(instruction,
                             2,
                             "LLVM bootstrap executable emission",
                             "shift_check",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, value) ||
        !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, count)) {
        return false;
    }

    std::string count_i64;
    if (!ExtendIntegerToI64(count,
                            function_index,
                            block_index,
                            instruction_index,
                            "shift",
                            output_lines,
                            count_i64)) {
        return false;
    }
    output_lines.push_back("call void " + ShiftCheckHelperName(value.type) + "(i64 " + count_i64 + ")");
    return true;
}

bool EmitBinaryInstruction(const mir::Instruction& instruction,
                           std::size_t function_index,
                           std::size_t block_index,
                           std::size_t instruction_index,
                           const mir::BasicBlock& block,
                           const std::filesystem::path& source_path,
                           support::DiagnosticSink& diagnostics,
                           ExecutableFunctionState& state,
                           std::vector<std::string>& output_lines) {
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue lhs;
    ExecutableValue rhs;
    if (!RequireOperandCount(instruction,
                             2,
                             "LLVM bootstrap executable emission",
                             "binary",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, lhs) ||
        !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, rhs)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("binary", state, block),
                              result_type)) {
        return false;
    }

    const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
    const std::string compare_predicate = ComparePredicateForLLVM(instruction, lhs);
    if (!compare_predicate.empty()) {
        const auto lhs_enum = ResolveEnumCompareInfo(*state.module, lhs);
        const auto rhs_enum = ResolveEnumCompareInfo(*state.module, rhs);
        if (lhs_enum.has_value() && rhs_enum.has_value()) {
            bool emitted = false;
            EmitEnumCompare(instruction,
                            function_index,
                            block_index,
                            instruction_index,
                            compare_predicate,
                            lhs,
                            rhs,
                            *lhs_enum,
                            source_path,
                            block,
                            diagnostics,
                            result_type,
                            state,
                            output_lines,
                            emitted);
            return emitted;
        }
        const std::string operand_type = PreferredOperandType(lhs, rhs);
        const std::string compare_opcode = IsFloatType(lhs.type) ? "fcmp" : "icmp";
        output_lines.push_back(temp + " = " + compare_opcode + " " + compare_predicate + " " + operand_type +
                               " " + lhs.text + ", " + rhs.text);
        RecordExecutableValue(state, instruction.result, temp, result_type);
        return true;
    }

    const std::string opcode = BinaryOpcodeForLLVM(instruction, lhs, result_type);
    if (opcode.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission does not support binary operator '" + instruction.op +
                               "' in function '" + state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    output_lines.push_back(temp + " = " + opcode + " " + LLVMTypeName(result_type) + " " + lhs.text + ", " +
                           rhs.text);
    RecordExecutableValue(state, instruction.result, temp, result_type);
    return true;
}

bool EmitAggregateInitInstruction(const mir::Instruction& instruction,
                                  std::size_t function_index,
                                  std::size_t block_index,
                                  std::size_t instruction_index,
                                  const mir::BasicBlock& block,
                                  const std::filesystem::path& source_path,
                                  support::DiagnosticSink& diagnostics,
                                  ExecutableFunctionState& state,
                                  std::vector<std::string>& output_lines) {
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    BackendTypeInfo aggregate_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("aggregate_init", state.function->name, block),
                              aggregate_type)) {
        return false;
    }

    std::vector<BackendTypeInfo> field_types;
    field_types.reserve(instruction.operands.size());
    const auto builtin_fields = sema::BuiltinAggregateFields(instruction.type);
    const bool is_array_aggregate = instruction.type.kind == sema::Type::Kind::kArray;
    if (instruction.type.kind == sema::Type::Kind::kNamed || !builtin_fields.empty()) {
        const mir::TypeDecl* type_decl = FindTypeDecl(*state.module, instruction.type.name);
        const auto field_count = type_decl != nullptr ? type_decl->fields.size() : builtin_fields.size();
        if ((type_decl != nullptr && type_decl->kind != mir::TypeDecl::Kind::kStruct) || field_count != instruction.operands.size()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission could not resolve aggregate_init field layout for type '" +
                                   sema::FormatType(instruction.type) + "' in function '" + state.function->name +
                                   "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        const auto& fields = type_decl != nullptr ? type_decl->fields : builtin_fields;
        for (const auto& field : fields) {
            const auto lowered_field = LowerAggregateFieldStorageType(*state.module, instruction.type, field.first);
            if (!lowered_field.has_value()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not lower aggregate_init storage for field '" + field.first +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            field_types.push_back(*lowered_field);
        }
    } else if (is_array_aggregate) {
        if (instruction.type.subtypes.empty()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission requires an array element type for aggregate_init in function '" +
                                   state.function->name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        const auto length = mc::support::ParseArrayLength(instruction.type.metadata);
        if (!length.has_value() || *length != instruction.operands.size()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission requires exact array aggregate_init element count in function '" +
                                   state.function->name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        for (const auto& field_name : instruction.field_names) {
            if (field_name != "_") {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires positional array aggregate_init elements in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
        }
        const auto element_type = LowerTypeInfo(*state.module, instruction.type.subtypes.front());
        if (!element_type.has_value()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission could not lower aggregate_init array element type '" +
                                   sema::FormatType(instruction.type.subtypes.front()) + "' in function '" + state.function->name +
                                   "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        field_types.assign(instruction.operands.size(), *element_type);
    } else {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission only supports named-struct aggregate_init in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    std::string current_value = LLVMStructInsertBase(aggregate_type);
    for (std::size_t index = 0; index < instruction.operands.size(); ++index) {
        ExecutableValue operand;
        if (!ResolveExecutableValue(state, instruction.operands[index], block, source_path, diagnostics, operand)) {
            return false;
        }

        std::string coerced_operand = operand.text;
        if ((operand.type.backend_name == "i1" && field_types[index].backend_name == "i8") ||
            (operand.type.backend_name == "i8" && field_types[index].backend_name == "i1")) {
            if (!EmitValueRepresentationCoercion(operand,
                                                 field_types[index],
                                                 function_index,
                                                 block_index,
                                                 instruction_index,
                                                 "agg_coerce" + std::to_string(index),
                                                 output_lines,
                                                 coerced_operand)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not coerce aggregate_init operand " +
                                       std::to_string(index) + " in function '" + state.function->name + "' block '" +
                                       block.label + "'",
                                   diagnostics);
                return false;
            }
        }

        const std::string next_value = index + 1 == instruction.operands.size()
                                           ? LLVMTempName(function_index, block_index, instruction_index)
                                           : LLVMTempName(function_index, block_index, instruction_index) + ".agg" + std::to_string(index);
        output_lines.push_back(next_value + " = insertvalue " + aggregate_type.backend_name + " " + current_value + ", " +
                               field_types[index].backend_name + " " + coerced_operand + ", " + std::to_string(index));
        current_value = next_value;
    }

    RecordExecutableValue(state, instruction.result, current_value, aggregate_type, std::nullopt, instruction.type);
    return true;
}

bool ResolveEnumInstructionLayout(const mir::Instruction& instruction,
                                  const mir::Module& module,
                                  const std::filesystem::path& source_path,
                                  support::DiagnosticSink& diagnostics,
                                  const std::string& context,
                                  EnumBackendLayout& layout,
                                  std::size_t& variant_index) {
    const sema::Type enum_type = instruction.kind == mir::Instruction::Kind::kVariantInit ? instruction.type : instruction.target_base_type;
    if (enum_type.kind != sema::Type::Kind::kNamed) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires named enum metadata for " + context,
                           diagnostics);
        return false;
    }

    const mir::TypeDecl* type_decl = FindTypeDecl(module, enum_type.name);
    if (type_decl == nullptr || type_decl->kind != mir::TypeDecl::Kind::kEnum) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission could not resolve enum layout for type '" +
                               sema::FormatType(enum_type) + "' in " + context,
                           diagnostics);
        return false;
    }

    const auto lowered_layout = LowerEnumLayout(module, *type_decl, enum_type);
    if (!lowered_layout.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission could not lower enum layout for type '" +
                               sema::FormatType(enum_type) + "' in " + context,
                           diagnostics);
        return false;
    }
    layout = *lowered_layout;

    const mir::VariantDecl* variant_decl =
        FindVariantDecl(*type_decl, instruction.target_name.empty() ? instruction.target : instruction.target_name, &variant_index);
    if (variant_decl == nullptr) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission references unknown enum variant '" +
                               (instruction.target_name.empty() ? instruction.target : instruction.target_name) +
                               "' in " + context,
                           diagnostics);
        return false;
    }
    if (variant_index >= layout.variant_field_types.size() || variant_index >= layout.variant_payload_offsets.size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission produced inconsistent enum layout metadata for type '" +
                               sema::FormatType(enum_type) + "' in " + context,
                           diagnostics);
        return false;
    }
    return true;
}

std::string EmitEnumPayloadFieldPointer(const std::string& enum_slot,
                                       const EnumBackendLayout& layout,
                                       std::size_t payload_offset,
                                       std::size_t function_index,
                                       std::size_t block_index,
                                       std::size_t instruction_index,
                                       std::string_view suffix,
                                       std::vector<std::string>& output_lines) {
    const std::string payload_slot = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix) + ".payload";
    output_lines.push_back(payload_slot + " = getelementptr inbounds " + layout.aggregate_type.backend_name + ", ptr " + enum_slot + ", i64 0, i32 1");
    if (payload_offset == 0) {
        return payload_slot;
    }

    const std::string field_slot = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix) + ".field";
    output_lines.push_back(field_slot + " = getelementptr inbounds " + layout.payload_type.backend_name + ", ptr " + payload_slot + ", i64 0, i64 " +
                           std::to_string(payload_offset));
    return field_slot;
}

bool EmitVariantInitInstruction(const mir::Instruction& instruction,
                               std::size_t function_index,
                               std::size_t block_index,
                               std::size_t instruction_index,
                               const mir::BasicBlock& block,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               ExecutableFunctionState& state,
                               std::vector<std::string>& output_lines) {
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }
    EnumBackendLayout layout;
    std::size_t variant_index = 0;
    if (!ResolveEnumInstructionLayout(instruction,
                                      *state.module,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("variant_init", state.function->name, block),
                                      layout,
                                      variant_index)) {
        return false;
    }
    if (instruction.operands.size() != layout.variant_field_types[variant_index].size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission found enum payload operand mismatch in " +
                               FunctionBlockContext("variant_init", state.function->name, block),
                           diagnostics);
        return false;
    }

    const std::string enum_slot = LLVMTempName(function_index, block_index, instruction_index) + ".variant.slot";
    output_lines.push_back(enum_slot + " = alloca " + layout.aggregate_type.backend_name + ", align " +
                           std::to_string(layout.aggregate_type.alignment));
    output_lines.push_back("store " + layout.aggregate_type.backend_name + " zeroinitializer, ptr " + enum_slot + ", align " +
                           std::to_string(layout.aggregate_type.alignment));

    const std::string tag_slot = LLVMTempName(function_index, block_index, instruction_index) + ".variant.tag";
    output_lines.push_back(tag_slot + " = getelementptr inbounds " + layout.aggregate_type.backend_name + ", ptr " + enum_slot + ", i64 0, i32 0");
    output_lines.push_back("store i64 " + std::to_string(variant_index) + ", ptr " + tag_slot + ", align 8");

    for (std::size_t index = 0; index < instruction.operands.size(); ++index) {
        ExecutableValue operand;
        if (!ResolveExecutableValue(state, instruction.operands[index], block, source_path, diagnostics, operand)) {
            return false;
        }

        const std::string field_slot = EmitEnumPayloadFieldPointer(enum_slot,
                                                                   layout,
                                                                   layout.variant_payload_offsets[variant_index][index],
                                                                   function_index,
                                                                   block_index,
                                                                   instruction_index,
                                                                   "variant_init." + std::to_string(index),
                                                                   output_lines);
        output_lines.push_back("store " + layout.variant_field_types[variant_index][index].backend_name + " " + operand.text + ", ptr " +
                               field_slot + ", align " +
                               std::to_string(layout.variant_field_types[variant_index][index].alignment));
    }

    const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
    output_lines.push_back(result_temp + " = load " + layout.aggregate_type.backend_name + ", ptr " + enum_slot + ", align " +
                           std::to_string(layout.aggregate_type.alignment));
    RecordExecutableValue(state, instruction.result, result_temp, layout.aggregate_type, std::nullopt, instruction.type);
    return true;
}

bool EmitVariantMatchInstruction(const mir::Instruction& instruction,
                                std::size_t function_index,
                                std::size_t block_index,
                                std::size_t instruction_index,
                                const mir::BasicBlock& block,
                                const std::filesystem::path& source_path,
                                support::DiagnosticSink& diagnostics,
                                ExecutableFunctionState& state,
                                std::vector<std::string>& output_lines) {
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue selector;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "variant_match",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, selector)) {
        return false;
    }
    EnumBackendLayout layout;
    std::size_t variant_index = 0;
    if (!ResolveEnumInstructionLayout(instruction,
                                      *state.module,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("variant_match", state.function->name, block),
                                      layout,
                                      variant_index)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("variant_match", state.function->name, block),
                              result_type)) {
        return false;
    }

    const std::string tag_temp = LLVMTempName(function_index, block_index, instruction_index) + ".tag";
    const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
    output_lines.push_back(tag_temp + " = extractvalue " + layout.aggregate_type.backend_name + " " + selector.text + ", 0");
    output_lines.push_back(result_temp + " = icmp eq i64 " + tag_temp + ", " + std::to_string(variant_index));
    RecordExecutableValue(state, instruction.result, result_temp, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitVariantExtractInstruction(const mir::Instruction& instruction,
                                  std::size_t function_index,
                                  std::size_t block_index,
                                  std::size_t instruction_index,
                                  const mir::BasicBlock& block,
                                  const std::filesystem::path& source_path,
                                  support::DiagnosticSink& diagnostics,
                                  ExecutableFunctionState& state,
                                  std::vector<std::string>& output_lines) {
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue selector;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "variant_extract",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, selector)) {
        return false;
    }
    EnumBackendLayout layout;
    std::size_t variant_index = 0;
    if (!ResolveEnumInstructionLayout(instruction,
                                      *state.module,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("variant_extract", state.function->name, block),
                                      layout,
                                      variant_index)) {
        return false;
    }

    std::size_t payload_index = instruction.target_index >= 0 ? static_cast<std::size_t>(instruction.target_index) : 0;
    if (instruction.target_index < 0 && !instruction.op.empty()) {
        if (!std::all_of(instruction.op.begin(), instruction.op.end(), ::isdigit)) {
            ReportBackendError(source_path,
                               "backend: variant_extract instruction has non-numeric op field: " + instruction.op,
                               diagnostics);
            return false;
        }
        payload_index = static_cast<std::size_t>(std::stoul(instruction.op));
    }
    if (payload_index >= layout.variant_field_types[variant_index].size() ||
        payload_index >= layout.variant_payload_offsets[variant_index].size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission references out-of-range enum payload index in " +
                               FunctionBlockContext("variant_extract", state.function->name, block),
                           diagnostics);
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("variant_extract", state.function->name, block),
                              result_type)) {
        return false;
    }

    const std::string enum_slot = EmitAggregateStackSlot(selector,
                                                         function_index,
                                                         block_index,
                                                         instruction_index,
                                                         "variant_extract.enum",
                                                         output_lines);
    const std::string field_slot = EmitEnumPayloadFieldPointer(enum_slot,
                                                               layout,
                                                               layout.variant_payload_offsets[variant_index][payload_index],
                                                               function_index,
                                                               block_index,
                                                               instruction_index,
                                                               "variant_extract." + std::to_string(payload_index),
                                                               output_lines);
    const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
    output_lines.push_back(result_temp + " = load " + result_type.backend_name + ", ptr " + field_slot + ", align " +
                           std::to_string(result_type.alignment));
    RecordExecutableValue(state, instruction.result, result_temp, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitArenaNewInstruction(const mir::Instruction& instruction,
                            std::size_t function_index,
                            std::size_t block_index,
                            std::size_t instruction_index,
                            const mir::BasicBlock& block,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics,
                            ExecutableFunctionState& state,
                            std::vector<std::string>& output_lines) {
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue arena;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "arena_new",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, arena)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("arena_new", state.function->name, block),
                              result_type)) {
        return false;
    }

    const sema::Type pointee_type = instruction.type.subtypes.empty() ? sema::UnknownType() : instruction.type.subtypes.front();
    BackendTypeInfo element_type;
    if (!LowerInstructionType(*state.module,
                              pointee_type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("arena_new element", state.function->name, block),
                              element_type)) {
        return false;
    }

    const std::string loaded = LLVMTempName(function_index, block_index, instruction_index) + ".arena";
    const std::string data_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".data";
    const std::string cap = LLVMTempName(function_index, block_index, instruction_index) + ".cap";
    const std::string used = LLVMTempName(function_index, block_index, instruction_index) + ".used";
    output_lines.push_back(loaded + " = load {ptr, i64, i64, ptr}, ptr " + arena.text + ", align 8");
    output_lines.push_back(data_ptr + " = extractvalue {ptr, i64, i64, ptr} " + loaded + ", 0");
    output_lines.push_back(cap + " = extractvalue {ptr, i64, i64, ptr} " + loaded + ", 1");
    output_lines.push_back(used + " = extractvalue {ptr, i64, i64, ptr} " + loaded + ", 2");

    std::string aligned_used = used;
    if (element_type.alignment > 1) {
        const std::string sum = LLVMTempName(function_index, block_index, instruction_index) + ".align_sum";
        const std::string aligned = LLVMTempName(function_index, block_index, instruction_index) + ".aligned_used";
        output_lines.push_back(sum + " = add i64 " + used + ", " + std::to_string(element_type.alignment - 1));
        output_lines.push_back(aligned + " = and i64 " + sum + ", " + std::to_string(-static_cast<long long>(element_type.alignment)));
        aligned_used = aligned;
    }

    const std::string next_used = LLVMTempName(function_index, block_index, instruction_index) + ".next_used";
    const std::string fits = LLVMTempName(function_index, block_index, instruction_index) + ".fits";
    const std::string raw_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".raw_ptr";
    const std::string result_ptr = LLVMTempName(function_index, block_index, instruction_index);
    const std::string used_slot = LLVMTempName(function_index, block_index, instruction_index) + ".used_slot";
    const std::string stored_used = LLVMTempName(function_index, block_index, instruction_index) + ".stored_used";
    output_lines.push_back(next_used + " = add i64 " + aligned_used + ", " + std::to_string(element_type.size));
    output_lines.push_back(fits + " = icmp ule i64 " + next_used + ", " + cap);
    output_lines.push_back(raw_ptr + " = getelementptr i8, ptr " + data_ptr + ", i64 " + aligned_used);
    output_lines.push_back(result_ptr + " = select i1 " + fits + ", ptr " + raw_ptr + ", ptr null");
    output_lines.push_back(used_slot + " = getelementptr inbounds {ptr, i64, i64, ptr}, ptr " + arena.text + ", i64 0, i32 2");
    output_lines.push_back(stored_used + " = select i1 " + fits + ", i64 " + next_used + ", i64 " + used);
    output_lines.push_back("store i64 " + stored_used + ", ptr " + used_slot + ", align 8");
    RecordExecutableValue(state, instruction.result, result_ptr, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitCallInstruction(const mir::Instruction& instruction,
                         std::size_t function_index,
                         std::size_t block_index,
                         std::size_t instruction_index,
                         const std::unordered_map<std::string, std::string>& function_symbols,
                         const mir::BasicBlock& block,
                         const std::filesystem::path& source_path,
                         support::DiagnosticSink& diagnostics,
                         ExecutableFunctionState& state,
                         std::vector<std::string>& output_lines) {
    if ((instruction.target_name == "hal.memory_barrier" || instruction.target_name == "memory_barrier") &&
        instruction.operands.empty() && instruction.result.empty()) {
        output_lines.push_back("fence seq_cst");
        return true;
    }

    ExecutableCallSignature call_signature;
    if (!ResolveExecutableCallSignature(instruction,
                                        function_symbols,
                                        block,
                                        source_path,
                                        diagnostics,
                                        state,
                                        call_signature)) {
        return false;
    }

    std::string args_text;
    if (!FormatExecutableCallArguments(instruction,
                                       call_signature,
                                       block,
                                       source_path,
                                       diagnostics,
                                       state,
                                       args_text)) {
        return false;
    }

    const sema::Type result_source_type = executable_support::SignatureResultType(call_signature.types);
    std::optional<BackendTypeInfo> lowered_return_type;
    if (!executable_support::LowerExecutableReturnType(*state.module,
                                   call_signature.types,
                                   source_path,
                                   diagnostics,
                                   "call result type for '" +
                                       std::string(instruction.target_name.empty() ? instruction.target_display : instruction.target_name) +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   lowered_return_type)) {
        return false;
    }

    if (instruction.result.empty()) {
        if (result_source_type.kind == sema::Type::Kind::kVoid) {
            output_lines.push_back("call void " + call_signature.callee_text + "(" + args_text + ")");
            return true;
        }

        output_lines.push_back("call " + LLVMTypeName(*lowered_return_type) + " " + call_signature.callee_text + "(" + args_text + ")");
        return true;
    }

    const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
    output_lines.push_back(temp + " = call " + LLVMTypeName(*lowered_return_type) + " " + call_signature.callee_text + "(" +
                           args_text + ")");
    RecordExecutableValue(state, instruction.result, temp, *lowered_return_type, std::nullopt, instruction.type);
    return true;
}

bool EmitSliceInstruction(const mir::Instruction& instruction,
                          std::size_t function_index,
                          std::size_t block_index,
                          std::size_t instruction_index,
                          const mir::BasicBlock& block,
                          const std::filesystem::path& source_path,
                          support::DiagnosticSink& diagnostics,
                          ExecutableFunctionState& state,
                          std::vector<std::string>& output_lines) {
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }
    if (!RequireOperandRange(instruction,
                             2,
                             3,
                             "LLVM bootstrap executable emission",
                             "slice to use base plus one or two bounds",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics)) {
        return false;
    }

    ExecutableValue base;
    ExecutableValue lower;
    if (!ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, base) ||
        !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, lower)) {
        return false;
    }

    std::optional<ExecutableValue> upper;
    if (instruction.operands.size() == 3) {
        ExecutableValue resolved_upper;
        if (!ResolveExecutableValue(state, instruction.operands[2], block, source_path, diagnostics, resolved_upper)) {
            return false;
        }
        upper = resolved_upper;
    }

    BackendTypeInfo slice_type;
    BackendTypeInfo element_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("slice", state.function->name, block),
                              slice_type)) {
        return false;
    }

    const bool string_slice = instruction.type.kind == sema::Type::Kind::kString ||
                              instruction.type.name == "str" || instruction.type.name == "string" ||
                              instruction.type.name == "cstr";
    if (string_slice) {
        if (!LowerInstructionType(*state.module,
                                  sema::NamedType("u8"),
                                  source_path,
                                  diagnostics,
                                  FunctionBlockContext("slice element", state.function->name, block),
                                  element_type)) {
            return false;
        }
    } else if (instruction.type.subtypes.empty() ||
               !LowerInstructionType(*state.module,
                                     instruction.type.subtypes.front(),
                                     source_path,
                                     diagnostics,
                                     FunctionBlockContext("slice element", state.function->name, block),
                                     element_type)) {
        return false;
    }

    std::string lower_i64;
    if (!ExtendIntegerToI64(lower,
                            function_index,
                            block_index,
                            instruction_index,
                            "slice_lower",
                            output_lines,
                            lower_i64)) {
        return false;
    }

    std::string upper_i64;
    if (upper.has_value() &&
        !ExtendIntegerToI64(*upper,
                            function_index,
                            block_index,
                            instruction_index + 1,
                            "slice_upper",
                            output_lines,
                            upper_i64)) {
        return false;
    }

    const std::string data_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".slice_ptr";
    std::string len_value;
    if (!base.type.backend_name.empty() && base.type.backend_name.front() == '[') {
        const auto array_length = ParseBackendArrayLength(base.type.backend_name);
        if (!array_length.has_value()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission requires statically known array length for slice base in function '" +
                                   state.function->name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        const std::string array_slot = EmitAggregateStackSlot(base,
                                                              function_index,
                                                              block_index,
                                                              instruction_index,
                                                              "slice_array",
                                                              output_lines);
        output_lines.push_back(data_ptr + " = getelementptr inbounds " + base.type.backend_name + ", ptr " + array_slot + ", i64 0, i64 " + lower_i64);
        len_value = EmitSliceLengthValue(function_index,
                                         block_index,
                                         instruction_index,
                                         "slice_len",
                                         lower_i64,
                                         upper.has_value() ? std::optional<std::string>(upper_i64) : std::nullopt,
                                         std::to_string(*array_length),
                                         output_lines);
    } else if (base.type.backend_name == "{ptr, i64}") {
        const std::string base_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".base_ptr";
        const std::string base_len = LLVMTempName(function_index, block_index, instruction_index) + ".base_len";
        output_lines.push_back(base_ptr + " = extractvalue {ptr, i64} " + base.text + ", 0");
        output_lines.push_back(base_len + " = extractvalue {ptr, i64} " + base.text + ", 1");
        output_lines.push_back(data_ptr + " = getelementptr inbounds " + element_type.backend_name + ", ptr " + base_ptr + ", i64 " + lower_i64);
        len_value = EmitSliceLengthValue(function_index,
                                         block_index,
                                         instruction_index,
                                         "slice_len",
                                         lower_i64,
                                         upper.has_value() ? std::optional<std::string>(upper_i64) : std::nullopt,
                                         base_len,
                                         output_lines);
    } else {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission does not support slice base type '" + base.type.source_name +
                               "' in function '" + state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    const std::string init_temp = LLVMTempName(function_index, block_index, instruction_index) + ".slice_init";
    const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
    output_lines.push_back(init_temp + " = insertvalue " + slice_type.backend_name + " zeroinitializer, ptr " + data_ptr + ", 0");
    output_lines.push_back(result_temp + " = insertvalue " + slice_type.backend_name + " " + init_temp + ", i64 " + len_value + ", 1");
    RecordExecutableValue(state, instruction.result, result_temp, slice_type);
    return true;
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
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      ExecutableFunctionBlockContext("local_addr", state, block),
                                      type_info)) {
                return false;
            }
            if (instruction.target_kind == mir::Instruction::TargetKind::kGlobal) {
                if (instruction.target_name.empty()) {
                    ReportBackendError(source_path,
                                       "local_addr global target is missing target_name metadata in function '" + state.function->name +
                                           "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }
                const auto global_it = state.globals->find(instruction.target_name);
                if (global_it == state.globals->end()) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission references unknown global address target '" +
                                           instruction.target_name + "' in function '" + state.function->name + "' block '" +
                                           block.label + "'",
                                       diagnostics);
                    return false;
                }
                RecordExecutableValue(state, instruction.result, global_it->second.backend_name, type_info);
                return true;
            }
            if (instruction.target_kind == mir::Instruction::TargetKind::kField) {
                if (instruction.target_name.empty()) {
                    ReportBackendError(source_path,
                                       "local_addr field target is missing target_name metadata in function '" + state.function->name +
                                           "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }
                if (instruction.target_base_name.empty()) {
                    ReportBackendError(source_path,
                                       "local_addr field target is missing base-name metadata in function '" + state.function->name +
                                           "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }

                std::string base_slot;
                if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kLocal) {
                    if (!ResolveExecutableLocal(state, instruction.target_base_name, block, source_path, diagnostics, base_slot)) {
                        return false;
                    }
                } else if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kGlobal) {
                    const auto global_it = state.globals->find(instruction.target_base_name);
                    if (global_it == state.globals->end()) {
                        ReportBackendError(source_path,
                                           "LLVM bootstrap executable emission references unknown global field-address base '" +
                                               instruction.target_base_name + "' in function '" + state.function->name + "' block '" +
                                               block.label + "'",
                                           diagnostics);
                        return false;
                    }
                    base_slot = global_it->second.backend_name;
                } else {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission currently supports only direct local/global field addresses in function '" +
                                           state.function->name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }

                const auto field_index = FindFieldIndex(*state.module, instruction.target_base_type, instruction.target_name);
                if (!field_index.has_value()) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission could not resolve field-address target '" + instruction.target_name +
                                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }

                BackendTypeInfo base_type;
                if (!LowerInstructionType(*state.module,
                                          instruction.target_base_type,
                                          source_path,
                                          diagnostics,
                                          ExecutableFunctionBlockContext("local_addr field base", state, block),
                                          base_type)) {
                    return false;
                }

                const std::string field_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".field.addr";
                output_lines.push_back(field_ptr + " = getelementptr inbounds " + base_type.backend_name + ", ptr " + base_slot + ", i32 0, i32 " +
                                       std::to_string(*field_index));
                RecordExecutableValue(state, instruction.result, field_ptr, type_info);
                return true;
            }
            if (instruction.target_kind == mir::Instruction::TargetKind::kIndex) {
                ExecutableValue base;
                ExecutableValue index;
                if (!RequireOperandCount(instruction,
                                         2,
                                         "LLVM bootstrap executable emission",
                                         "local_addr index",
                                         state.function->name,
                                         block,
                                         source_path,
                                         diagnostics) ||
                    !ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, base) ||
                    !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, index)) {
                    return false;
                }

                std::string index_i64;
                if (!ExtendIntegerToI64(index,
                                        function_index,
                                        block_index,
                                        instruction_index,
                                        "local_addr_index",
                                        output_lines,
                                        index_i64)) {
                    return false;
                }

                const std::string ptr_temp = LLVMTempName(function_index, block_index, instruction_index) + ".index.addr";
                if (!base.type.backend_name.empty() && base.type.backend_name.front() == '[') {
                    std::string array_ptr;
                    if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kLocal) {
                        const auto local_it = state.local_slots.find(instruction.target_base_name);
                        if (local_it != state.local_slots.end()) {
                            array_ptr = local_it->second;
                        }
                    } else if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kGlobal) {
                        const auto global_it = state.globals->find(instruction.target_base_name);
                        if (global_it != state.globals->end()) {
                            array_ptr = global_it->second.backend_name;
                        }
                    }
                    if (array_ptr.empty()) {
                        array_ptr = EmitAggregateStackSlot(base,
                                                           function_index,
                                                           block_index,
                                                           instruction_index,
                                                           "local_addr_index_array",
                                                           output_lines);
                    }
                    output_lines.push_back(ptr_temp + " = getelementptr inbounds " + base.type.backend_name + ", ptr " + array_ptr + ", i64 0, i64 " +
                                           index_i64);
                } else if (base.type.backend_name == "{ptr, i64}") {
                    BackendTypeInfo element_type;
                    if (!LowerInstructionType(*state.module,
                                              instruction.type.subtypes.front(),
                                              source_path,
                                              diagnostics,
                                              ExecutableFunctionBlockContext("local_addr index element", state, block),
                                              element_type)) {
                        return false;
                    }
                    const std::string data_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".index.data";
                    output_lines.push_back(data_ptr + " = extractvalue {ptr, i64} " + base.text + ", 0");
                    output_lines.push_back(ptr_temp + " = getelementptr inbounds " + element_type.backend_name + ", ptr " + data_ptr + ", i64 " +
                                           index_i64);
                } else {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission does not support local_addr index base type '" + base.type.source_name +
                                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }

                RecordExecutableValue(state, instruction.result, ptr_temp, type_info);
                return true;
            }
            std::string local_slot;
            if (!ResolveExecutableLocal(state, instruction.target, block, source_path, diagnostics, local_slot)) {
                return false;
            }
            RecordExecutableValue(state, instruction.result, local_slot, type_info);
            return true;
        }

        case mir::Instruction::Kind::kLoadLocal: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            std::string local_slot;
            if (!ResolveExecutableLocal(state, instruction.target, block, source_path, diagnostics, local_slot)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      ExecutableFunctionBlockContext("load_local", state, block),
                                      type_info)) {
                return false;
            }

            const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
            output_lines.push_back(temp + " = load " + LLVMTypeName(type_info) + ", ptr " + local_slot + ", align " +
                                   std::to_string(type_info.alignment));
            RecordExecutableValue(state,
                                  instruction.result,
                                  temp,
                                  type_info,
                                  IsAggregateType(type_info) ? std::optional<std::string>(local_slot) : std::nullopt,
                                  instruction.type);
            return true;
        }

        case mir::Instruction::Kind::kStoreLocal: {
            const auto* local = FindMirLocal(*state.function, instruction.target);
            if (local == nullptr) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission references unknown local '" + instruction.target +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            std::string local_slot;
            if (!ResolveExecutableLocal(state, instruction.target, block, source_path, diagnostics, local_slot)) {
                return false;
            }

            ExecutableValue operand;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "store_local",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
                return false;
            }

            BackendTypeInfo type_info;
            if (!LowerInstructionType(*state.module,
                                      local->type,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("store_local destination", state.function->name, block),
                                      type_info)) {
                return false;
            }

            output_lines.push_back("store " + LLVMTypeName(type_info) + " " + operand.text + ", ptr " + local_slot +
                                   ", align " + std::to_string(type_info.alignment));
            return true;
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

            if (instruction.op == "+") {
                RecordExecutableValue(state, instruction.result, operand.text, type_info, std::nullopt, instruction.type);
                return true;
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
                    output_lines.push_back(temp + " = fsub " + LLVMTypeName(type_info) + " 0.0, " + operand.text);
                } else {
                    output_lines.push_back(temp + " = sub " + LLVMTypeName(type_info) + " 0, " + operand.text);
                }
            } else if (instruction.op == "!") {
                output_lines.push_back(temp + " = xor i1 " + operand.text + ", true");
            } else if (instruction.op == "~") {
                output_lines.push_back(temp + " = xor " + LLVMTypeName(type_info) + " " + operand.text + ", -1");
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
            return EmitBinaryInstruction(instruction,
                                         function_index,
                                         block_index,
                                         instruction_index,
                                         block,
                                         source_path,
                                         diagnostics,
                                         state,
                                         output_lines);
        }

        case mir::Instruction::Kind::kSymbolRef: {
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
                const std::string value_temp = LLVMTempName(function_index, block_index, instruction_index);
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
            const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
            output_lines.push_back(temp + " = load " + type_info.backend_name + ", ptr " + global_it->second.backend_name +
                                   ", align " + std::to_string(type_info.alignment));
            RecordExecutableValue(state, instruction.result, temp, type_info, std::nullopt, instruction.type);
            return true;
        }

        case mir::Instruction::Kind::kCall: {
            return EmitCallInstruction(instruction,
                                       function_index,
                                       block_index,
                                       instruction_index,
                                       function_symbols,
                                       block,
                                       source_path,
                                       diagnostics,
                                       state,
                                       output_lines);
        }

        case mir::Instruction::Kind::kConvertDistinct: {
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

        case mir::Instruction::Kind::kPointerToInt:
        case mir::Instruction::Kind::kIntToPointer: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            ExecutableValue operand;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     ToString(instruction.kind),
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
                                      ExecutableFunctionBlockContext(ToString(instruction.kind), state, block),
                                      result_type)) {
                return false;
            }

            const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
            const std::string opcode = instruction.kind == mir::Instruction::Kind::kPointerToInt ? "ptrtoint" : "inttoptr";
            output_lines.push_back(temp + " = " + opcode + " " + operand.type.backend_name + " " + operand.text +
                                   " to " + result_type.backend_name);
            RecordExecutableValue(state, instruction.result, temp, result_type, std::nullopt, instruction.type);
            return true;
        }

        case mir::Instruction::Kind::kBoundsCheck:
            return EmitBoundsCheckCall(instruction,
                                       function_index,
                                       block_index,
                                       instruction_index,
                                       block,
                                       source_path,
                                       diagnostics,
                                       state,
                                       output_lines);

        case mir::Instruction::Kind::kDivCheck:
            return EmitDivCheckCall(instruction,
                                    function_index,
                                    block_index,
                                    instruction_index,
                                    block,
                                    source_path,
                                    diagnostics,
                                    state,
                                    output_lines);

        case mir::Instruction::Kind::kShiftCheck:
            return EmitShiftCheckCall(instruction,
                                      function_index,
                                      block_index,
                                      instruction_index,
                                      block,
                                      source_path,
                                      diagnostics,
                                      state,
                                      output_lines);

        case mir::Instruction::Kind::kStoreTarget: {
            if (instruction.target_kind == mir::Instruction::TargetKind::kGlobal && !instruction.target.empty()) {
                ExecutableValue operand;
                if (!RequireOperandCount(instruction,
                                         1,
                                         "LLVM bootstrap executable emission",
                                         "store_target",
                                         state.function->name,
                                         block,
                                         source_path,
                                         diagnostics) ||
                    !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
                    return false;
                }

                const auto global_it = state.globals->find(instruction.target);
                if (global_it == state.globals->end()) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission references unknown global store target '" + instruction.target +
                                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }

                output_lines.push_back("store " + global_it->second.lowered_type.backend_name + " " + operand.text + ", ptr " +
                                       global_it->second.backend_name + ", align " +
                                       std::to_string(global_it->second.lowered_type.alignment));
                return true;
            }

            if (instruction.target_kind == mir::Instruction::TargetKind::kField && instruction.operands.size() == 2) {
                ExecutableValue stored_value;
                ExecutableValue base;
                if (!ResolveExecutableOperands(instruction, block, source_path, diagnostics, state, stored_value, base)) {
                    return false;
                }

                const auto base_type = ResolveExecutableFieldBaseType(*state.module, instruction.target_base_type, base);
                const auto field_index = base_type.has_value()
                                             ? FindFieldIndex(*state.module, *base_type, instruction.target_name)
                                             : std::nullopt;
                const auto field_storage_type = base_type.has_value()
                                                    ? LowerAggregateFieldStorageType(*state.module, *base_type, instruction.target_name)
                                                    : std::nullopt;
                if (!field_index.has_value()) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission could not resolve store_target field '" + instruction.target_name +
                                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }
                if (!field_storage_type.has_value()) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission could not lower store_target field storage for '" +
                                           instruction.target_name + "' in function '" + state.function->name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }

                if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kNone ||
                    instruction.target_base_name.empty()) {
                    ReportBackendError(source_path,
                                       "LLVM bootstrap executable emission requires a direct field store target in function '" +
                                           state.function->name + "' block '" + block.label + "'",
                                       diagnostics);
                    return false;
                }

                std::string coerced_value = stored_value.text;
                if ((stored_value.type.backend_name == "i1" && field_storage_type->backend_name == "i8") ||
                    (stored_value.type.backend_name == "i8" && field_storage_type->backend_name == "i1")) {
                    if (!EmitValueRepresentationCoercion(stored_value,
                                                         *field_storage_type,
                                                         function_index,
                                                         block_index,
                                                         instruction_index,
                                                         "store_field",
                                                         output_lines,
                                                         coerced_value)) {
                        ReportBackendError(source_path,
                                           "LLVM bootstrap executable emission could not coerce store_target field value for '" +
                                               instruction.target_name + "' in function '" + state.function->name + "' block '" +
                                               block.label + "'",
                                           diagnostics);
                        return false;
                    }
                }

                const std::string updated_value = LLVMTempName(function_index, block_index, instruction_index) + ".field";
                output_lines.push_back(updated_value + " = insertvalue " + base.type.backend_name + " " + base.text + ", " +
                                       field_storage_type->backend_name + " " + coerced_value + ", " +
                                       std::to_string(*field_index));

                if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kLocal) {
                    const auto local_it = state.local_slots.find(instruction.target_base_name);
                    if (local_it != state.local_slots.end()) {
                        output_lines.push_back("store " + base.type.backend_name + " " + updated_value + ", ptr " + local_it->second + ", align " +
                                               std::to_string(base.type.alignment));
                        return true;
                    }
                }

                if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kGlobal) {
                    const auto global_it = state.globals->find(instruction.target_base_name);
                    if (global_it != state.globals->end()) {
                        output_lines.push_back("store " + global_it->second.lowered_type.backend_name + " " + updated_value + ", ptr " +
                                               global_it->second.backend_name + ", align " +
                                               std::to_string(global_it->second.lowered_type.alignment));
                        return true;
                    }
                }

                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission currently supports only direct local/global field store_target effects in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            if (instruction.target_kind == mir::Instruction::TargetKind::kOther && instruction.operands.size() == 2 &&
                instruction.target_base_type.kind == sema::Type::Kind::kPointer) {
                ExecutableValue stored_value;
                ExecutableValue base;
                if (!ResolveExecutableOperands(instruction, block, source_path, diagnostics, state, stored_value, base)) {
                    return false;
                }

                BackendTypeInfo stored_type;
                if (!LowerInstructionType(*state.module,
                                          instruction.type,
                                          source_path,
                                          diagnostics,
                                          ExecutableFunctionBlockContext("store_target", state, block),
                                          stored_type)) {
                    return false;
                }

                output_lines.push_back("store " + stored_type.backend_name + " " + stored_value.text + ", ptr " + base.text +
                                       ", align " + std::to_string(stored_type.alignment));
                return true;
            }

            if (instruction.target_kind != mir::Instruction::TargetKind::kIndex || instruction.operands.size() != 3) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission currently supports only global, direct field, and indexed store_target effects in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            ExecutableValue stored_value;
            ExecutableValue base;
            ExecutableValue index;
            if (!ResolveExecutableOperands(instruction, block, source_path, diagnostics, state, stored_value, base, index)) {
                return false;
            }

            BackendTypeInfo stored_type;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      ExecutableFunctionBlockContext("store_target", state, block),
                                      stored_type)) {
                return false;
            }

            std::string index_i64;
            if (!ExtendIntegerToI64(index,
                                    function_index,
                                    block_index,
                                    instruction_index,
                                    "store_index",
                                    output_lines,
                                    index_i64)) {
                return false;
            }

            const std::string ptr_temp = LLVMTempName(function_index, block_index, instruction_index) + ".ptr";
            if (!base.type.backend_name.empty() && base.type.backend_name.front() == '[') {
                std::string array_ptr;
                if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kLocal) {
                    const auto local_it = state.local_slots.find(instruction.target_base_name);
                    if (local_it != state.local_slots.end()) {
                        array_ptr = local_it->second;
                    }
                } else if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kGlobal) {
                    const auto global_it = state.globals->find(instruction.target_base_name);
                    if (global_it != state.globals->end()) {
                        array_ptr = global_it->second.backend_name;
                    }
                }
                if (array_ptr.empty()) {
                    array_ptr = EmitAggregateStackSlot(base,
                                                       function_index,
                                                       block_index,
                                                       instruction_index,
                                                       "store_array",
                                                       output_lines);
                }
                output_lines.push_back(ptr_temp + " = getelementptr inbounds " + base.type.backend_name + ", ptr " + array_ptr + ", i64 0, i64 " +
                                       index_i64);
            } else if (base.type.backend_name == "{ptr, i64}") {
                const std::string data_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".data";
                output_lines.push_back(data_ptr + " = extractvalue {ptr, i64} " + base.text + ", 0");
                output_lines.push_back(ptr_temp + " = getelementptr inbounds " + stored_type.backend_name + ", ptr " + data_ptr + ", i64 " +
                                       index_i64);
            } else {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission does not support store_target index base type '" + base.type.source_name +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            output_lines.push_back("store " + stored_type.backend_name + " " + stored_value.text + ", ptr " + ptr_temp + ", align " +
                                   std::to_string(stored_type.alignment));
            return true;
        }

        case mir::Instruction::Kind::kConvert: {
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

        case mir::Instruction::Kind::kConvertNumeric: {
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
                                       function_index,
                                       block_index,
                                       instruction_index,
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
            output_lines.push_back(temp + " = load volatile " + LLVMTypeName(result_type) + ", ptr " + ptr.text + ", align " +
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
            output_lines.push_back("store volatile " + LLVMTypeName(value.type) + " " + value.text + ", ptr " + ptr.text +
                                   ", align " + std::to_string(value.type.alignment));
            return true;
        }

        case mir::Instruction::Kind::kAtomicLoad:
        case mir::Instruction::Kind::kAtomicStore:
        case mir::Instruction::Kind::kAtomicExchange:
        case mir::Instruction::Kind::kAtomicFetchAdd:
            return executable_atomic::RenderAtomicInstruction(instruction,
                                                              function_index,
                                                              block_index,
                                                              instruction_index,
                                                              block,
                                                              source_path,
                                                              diagnostics,
                                                              state,
                                                              output_lines);

        case mir::Instruction::Kind::kAtomicCompareExchange:
            MC_UNREACHABLE("unsupported executable instruction should be rejected before lowering");

        case mir::Instruction::Kind::kArrayToSlice: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            ExecutableValue base;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "array_to_slice",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, base)) {
                return false;
            }

            const auto length = ParseBackendArrayLength(base.type.backend_name);
            if (!length.has_value()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires statically known array length for array_to_slice in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            BackendTypeInfo slice_type;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("array_to_slice", state.function->name, block),
                                      slice_type)) {
                return false;
            }

            const std::string array_slot = base.storage_slot.has_value()
                                               ? *base.storage_slot
                                               : EmitAggregateStackSlot(base,
                                                                        function_index,
                                                                        block_index,
                                                                        instruction_index,
                                                                        "array",
                                                                        output_lines);
            const std::string ptr_temp = LLVMTempName(function_index, block_index, instruction_index) + ".ptr";
            const std::string init_temp = LLVMTempName(function_index, block_index, instruction_index) + ".init";
            const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
            output_lines.push_back(ptr_temp + " = getelementptr inbounds " + base.type.backend_name + ", ptr " + array_slot + ", i64 0, i64 0");
            output_lines.push_back(init_temp + " = insertvalue " + slice_type.backend_name + " zeroinitializer, ptr " + ptr_temp + ", 0");
            output_lines.push_back(result_temp + " = insertvalue " + slice_type.backend_name + " " + init_temp + ", i64 " +
                                   std::to_string(*length) + ", 1");
            RecordExecutableValue(state, instruction.result, result_temp, slice_type);
            return true;
        }

        case mir::Instruction::Kind::kBufferToSlice: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            ExecutableValue base;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "buffer_to_slice",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, base)) {
                return false;
            }

            BackendTypeInfo slice_type;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("buffer_to_slice", state.function->name, block),
                                      slice_type)) {
                return false;
            }

            const std::string ptr_temp = LLVMTempName(function_index, block_index, instruction_index) + ".ptr";
            const std::string len_temp = LLVMTempName(function_index, block_index, instruction_index) + ".len";
            const std::string init_temp = LLVMTempName(function_index, block_index, instruction_index) + ".init";
            const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
            output_lines.push_back(ptr_temp + " = extractvalue " + base.type.backend_name + " " + base.text + ", 0");
            output_lines.push_back(len_temp + " = extractvalue " + base.type.backend_name + " " + base.text + ", 1");
            output_lines.push_back(init_temp + " = insertvalue " + slice_type.backend_name + " zeroinitializer, ptr " + ptr_temp + ", 0");
            output_lines.push_back(result_temp + " = insertvalue " + slice_type.backend_name + " " + init_temp + ", i64 " + len_temp + ", 1");
            RecordExecutableValue(state, instruction.result, result_temp, slice_type);
            return true;
        }

        case mir::Instruction::Kind::kBufferNew: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            ExecutableValue alloc;
            ExecutableValue cap;
            if (!RequireOperandCount(instruction,
                                     2,
                                     "LLVM bootstrap executable emission",
                                     "buffer_new",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, alloc) ||
                !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, cap)) {
                return false;
            }

            BackendTypeInfo result_type;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("buffer_new", state.function->name, block),
                                      result_type)) {
                return false;
            }

            const sema::Type buffer_type = instruction.type.subtypes.empty() ? sema::UnknownType() : instruction.type.subtypes.front();
            const auto element_ptr_type = FindFieldType(*state.module, buffer_type, "ptr");
            if (!element_ptr_type.has_value() || element_ptr_type->kind != sema::Type::Kind::kPointer || element_ptr_type->subtypes.empty()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not resolve Buffer element type for buffer_new in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            BackendTypeInfo element_type;
            if (!LowerInstructionType(*state.module,
                                      element_ptr_type->subtypes.front(),
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("buffer_new element", state.function->name, block),
                                      element_type)) {
                return false;
            }

            std::string cap_i64;
            if (!ExtendIntegerToI64(cap, function_index, block_index, instruction_index, "cap", output_lines, cap_i64)) {
                return false;
            }
            const std::string bytes_temp = LLVMTempName(function_index, block_index, instruction_index) + ".bytes";
            const std::string zero_temp = LLVMTempName(function_index, block_index, instruction_index) + ".zero";
            const std::string alloc_bytes = LLVMTempName(function_index, block_index, instruction_index) + ".alloc_bytes";
            const std::string data_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".data_ptr";
            const std::string buffer_size_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".buf_size.ptr";
            const std::string buffer_size = LLVMTempName(function_index, block_index, instruction_index) + ".buf_size";
            const std::string buffer_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".buf_ptr";
            output_lines.push_back(bytes_temp + " = mul i64 " + cap_i64 + ", " + std::to_string(element_type.size));
            output_lines.push_back(zero_temp + " = icmp eq i64 " + bytes_temp + ", 0");
            output_lines.push_back(alloc_bytes + " = select i1 " + zero_temp + ", i64 1, i64 " + bytes_temp);
            output_lines.push_back(data_ptr + " = call ptr @malloc(i64 " + alloc_bytes + ")");
            output_lines.push_back(buffer_size_ptr + " = getelementptr inbounds {ptr, i64, i64, ptr}, ptr null, i64 1");
            output_lines.push_back(buffer_size + " = ptrtoint ptr " + buffer_size_ptr + " to i64");
            output_lines.push_back(buffer_ptr + " = call ptr @malloc(i64 " + buffer_size + ")");

            const std::string ptr_slot = LLVMTempName(function_index, block_index, instruction_index) + ".ptr_slot";
            const std::string len_slot = LLVMTempName(function_index, block_index, instruction_index) + ".len_slot";
            const std::string cap_slot = LLVMTempName(function_index, block_index, instruction_index) + ".cap_slot";
            const std::string alloc_slot = LLVMTempName(function_index, block_index, instruction_index) + ".alloc_slot";
            output_lines.push_back(ptr_slot + " = getelementptr inbounds {ptr, i64, i64, ptr}, ptr " + buffer_ptr + ", i64 0, i32 0");
            output_lines.push_back(len_slot + " = getelementptr inbounds {ptr, i64, i64, ptr}, ptr " + buffer_ptr + ", i64 0, i32 1");
            output_lines.push_back(cap_slot + " = getelementptr inbounds {ptr, i64, i64, ptr}, ptr " + buffer_ptr + ", i64 0, i32 2");
            output_lines.push_back(alloc_slot + " = getelementptr inbounds {ptr, i64, i64, ptr}, ptr " + buffer_ptr + ", i64 0, i32 3");
            output_lines.push_back("store ptr " + data_ptr + ", ptr " + ptr_slot + ", align 8");
            output_lines.push_back("store i64 " + cap_i64 + ", ptr " + len_slot + ", align 8");
            output_lines.push_back("store i64 " + cap_i64 + ", ptr " + cap_slot + ", align 8");
            output_lines.push_back("store ptr " + alloc.text + ", ptr " + alloc_slot + ", align 8");
            RecordExecutableValue(state, instruction.result, buffer_ptr, result_type);
            return true;
        }

        case mir::Instruction::Kind::kArenaNew: {
            return EmitArenaNewInstruction(instruction,
                                           function_index,
                                           block_index,
                                           instruction_index,
                                           block,
                                           source_path,
                                           diagnostics,
                                           state,
                                           output_lines);
        }

        case mir::Instruction::Kind::kBufferFree: {
            ExecutableValue buffer;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "buffer_free",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, buffer)) {
                return false;
            }
            const std::string loaded = LLVMTempName(function_index, block_index, instruction_index) + ".buffer";
            const std::string data_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".data_ptr";
            output_lines.push_back(loaded + " = load {ptr, i64, i64, ptr}, ptr " + buffer.text + ", align 8");
            output_lines.push_back(data_ptr + " = extractvalue {ptr, i64, i64, ptr} " + loaded + ", 0");
            output_lines.push_back("call void @free(ptr " + data_ptr + ")");
            output_lines.push_back("call void @free(ptr " + buffer.text + ")");
            return true;
        }

        case mir::Instruction::Kind::kSliceFromBuffer:
            ReportBackendError(source_path,
                               "slice_from_buffer should lower before executable emission in function '" + state.function->name +
                                   "' block '" + block.label + "'",
                               diagnostics);
            return false;

        case mir::Instruction::Kind::kField: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            ExecutableValue base;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "field",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, base)) {
                return false;
            }

            const std::string field_name = instruction.target_name.empty() ? instruction.target : instruction.target_name;
            const auto field_base_type = ResolveExecutableFieldBaseType(*state.module,
                                                                       instruction.target_base_type,
                                                                       base);
            const auto field_index = field_base_type.has_value()
                                         ? FindFieldIndex(*state.module, *field_base_type, field_name)
                                         : std::nullopt;
            if (!field_index.has_value()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not resolve field '" + field_name + "' in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            BackendTypeInfo field_type;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("field", state.function->name, block),
                                      field_type)) {
                return false;
            }

            const auto storage_type = field_base_type.has_value()
                                          ? LowerAggregateFieldStorageType(*state.module, *field_base_type, field_name)
                                          : std::nullopt;
            if (!storage_type.has_value()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not lower field storage for '" + field_name +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            const std::string extracted = LLVMTempName(function_index, block_index, instruction_index) + ".field";
            output_lines.push_back(extracted + " = extractvalue " + base.type.backend_name + " " + base.text + ", " +
                                   std::to_string(*field_index));

            ExecutableValue extracted_value {
                .text = extracted,
                .type = *storage_type,
            };
            std::string coerced_value;
            if (!EmitValueRepresentationCoercion(extracted_value,
                                                 field_type,
                                                 function_index,
                                                 block_index,
                                                 instruction_index,
                                                 "field_value",
                                                 output_lines,
                                                 coerced_value)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not coerce extracted field '" + field_name +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            RecordExecutableValue(state, instruction.result, coerced_value, field_type, std::nullopt, instruction.type);
            return true;
        }

        case mir::Instruction::Kind::kIndex: {
            if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
                return false;
            }
            ExecutableValue base;
            ExecutableValue index;
            if (!RequireOperandCount(instruction,
                                     2,
                                     "LLVM bootstrap executable emission",
                                     "index",
                                     state.function->name,
                                     block,
                                     source_path,
                                     diagnostics) ||
                !ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, base) ||
                !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, index)) {
                return false;
            }

            BackendTypeInfo result_type;
            if (!LowerInstructionType(*state.module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("index", state.function->name, block),
                                      result_type)) {
                return false;
            }

            std::string index_i64;
            if (!ExtendIntegerToI64(index,
                                    function_index,
                                    block_index,
                                    instruction_index,
                                    "index",
                                    output_lines,
                                    index_i64)) {
                return false;
            }

            const std::string ptr_temp = LLVMTempName(function_index, block_index, instruction_index) + ".ptr";
            if (!base.type.backend_name.empty() && base.type.backend_name.front() == '[') {
                const std::string array_slot = EmitAggregateStackSlot(base,
                                                                      function_index,
                                                                      block_index,
                                                                      instruction_index,
                                                                      "array",
                                                                      output_lines);
                output_lines.push_back(ptr_temp + " = getelementptr inbounds " + base.type.backend_name + ", ptr " + array_slot + ", i64 0, i64 " + index_i64);
            } else if (base.type.backend_name == "{ptr, i64}") {
                const std::string data_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".data";
                output_lines.push_back(data_ptr + " = extractvalue {ptr, i64} " + base.text + ", 0");
                output_lines.push_back(ptr_temp + " = getelementptr inbounds " + result_type.backend_name + ", ptr " + data_ptr + ", i64 " + index_i64);
            } else {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission does not support index base type '" + base.type.source_name +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
            output_lines.push_back(result_temp + " = load " + result_type.backend_name + ", ptr " + ptr_temp + ", align " +
                                   std::to_string(result_type.alignment));
            RecordExecutableValue(state, instruction.result, result_temp, result_type, std::nullopt, instruction.type);
            return true;
        }

        case mir::Instruction::Kind::kSlice: {
            return EmitSliceInstruction(instruction,
                                        function_index,
                                        block_index,
                                        instruction_index,
                                        block,
                                        source_path,
                                        diagnostics,
                                        state,
                                        output_lines);
        }

        case mir::Instruction::Kind::kAggregateInit: {
            return EmitAggregateInitInstruction(instruction,
                                                function_index,
                                                block_index,
                                                instruction_index,
                                                block,
                                                source_path,
                                                diagnostics,
                                                state,
                                                output_lines);
        }

        case mir::Instruction::Kind::kVariantInit: {
            return EmitVariantInitInstruction(instruction,
                                              function_index,
                                              block_index,
                                              instruction_index,
                                              block,
                                              source_path,
                                              diagnostics,
                                              state,
                                              output_lines);
        }

        case mir::Instruction::Kind::kVariantMatch: {
            return EmitVariantMatchInstruction(instruction,
                                               function_index,
                                               block_index,
                                               instruction_index,
                                               block,
                                               source_path,
                                               diagnostics,
                                               state,
                                               output_lines);
        }

        case mir::Instruction::Kind::kVariantExtract: {
            return EmitVariantExtractInstruction(instruction,
                                                 function_index,
                                                 block_index,
                                                 instruction_index,
                                                 block,
                                                 source_path,
                                                 diagnostics,
                                                 state,
                                                 output_lines);
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
            stream << LLVMGlobalName(name) << " = ";
            if (global.is_extern) {
                stream << "external " << (global.is_const ? "constant " : "global ")
                       << global_type.backend_name << ", align " << global_type.alignment << "\n";
                continue;
            }

            std::string init_value = LLVMStructInsertBase(global_type);
            if (index < global.constant_values.size() && global.constant_values[index].has_value()) {
                if (!RenderLLVMGlobalConstValue(module,
                                                global.type,
                                                *global.constant_values[index],
                                                source_path,
                                                diagnostics,
                                                init_value)) {
                    return false;
                }
            } else if (index < global.initializers.size()) {
                init_value = FormatLLVMLiteral(global_type, global.initializers[index]);
            }
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
    return ValidateBootstrapTargetImpl(target, source_path, diagnostics);
}

bool ValidateExecutableBackendCapabilities(const mir::Module& module,
                                          const TargetConfig& target,
                                          const std::filesystem::path& source_path,
                                          support::DiagnosticSink& diagnostics) {
    return ValidateExecutableBackendCapabilitiesImpl(module, target, source_path, diagnostics);
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

}  // namespace mc::codegen_llvm