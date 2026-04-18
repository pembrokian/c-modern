#include "compiler/codegen_llvm/backend_internal.h"

namespace mc::codegen_llvm {
namespace {

std::vector<const mir::Local*> ParameterLocals(const mir::Function& function) {
    std::vector<const mir::Local*> params;
    params.reserve(function.locals.size());
    for (const auto& local : function.locals) {
        if (local.is_parameter) {
            params.push_back(&local);
        }
    }
    return params;
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

}  // namespace

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

namespace executable_support {

ExecutableFunctionSignature NormalizeFunctionSignature(const mir::Function& function) {
    ExecutableFunctionSignature signature;
    signature.params = ParameterLocals(function);
    signature.types.param_types.reserve(signature.params.size());
    for (const mir::Local* param : signature.params) {
        signature.types.param_types.push_back(param->type);
    }
    signature.types.return_types = function.return_types;
    return signature;
}

bool NormalizeProcedureTypeSignature(const sema::Type& procedure_type,
                                     const std::filesystem::path& source_path,
                                     support::DiagnosticSink& diagnostics,
                                     std::string_view context,
                                     ExecutableSignature& signature) {
    if (procedure_type.kind != sema::Type::Kind::kProcedure) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires checked procedure signature metadata for " +
                               std::string(context),
                           diagnostics);
        return false;
    }

    const auto parsed_param_count = mc::support::ParseArrayLength(procedure_type.metadata);
    if (!parsed_param_count.has_value() || *parsed_param_count > procedure_type.subtypes.size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission encountered malformed procedure signature metadata for " +
                               std::string(context),
                           diagnostics);
        return false;
    }

    signature.param_types.assign(procedure_type.subtypes.begin(),
                                 procedure_type.subtypes.begin() + static_cast<std::ptrdiff_t>(*parsed_param_count));
    signature.return_types.assign(procedure_type.subtypes.begin() + static_cast<std::ptrdiff_t>(*parsed_param_count),
                                  procedure_type.subtypes.end());
    return true;
}

sema::Type SignatureResultType(const ExecutableSignature& signature) {
    if (signature.return_types.empty()) {
        return sema::VoidType();
    }
    if (signature.return_types.size() == 1) {
        return signature.return_types.front();
    }
    return sema::TupleType(signature.return_types);
}

bool LowerExecutableParamTypes(const mir::Module& module,
                               const ExecutableSignature& signature,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               const std::function<std::string(std::size_t)>& describe_param,
                               std::vector<BackendTypeInfo>& param_types) {
    param_types.clear();
    param_types.reserve(signature.param_types.size());
    for (std::size_t index = 0; index < signature.param_types.size(); ++index) {
        BackendTypeInfo param_type;
        if (!LowerInstructionType(module,
                                  signature.param_types[index],
                                  source_path,
                                  diagnostics,
                                  describe_param(index),
                                  param_type)) {
            return false;
        }
        param_types.push_back(std::move(param_type));
    }
    return true;
}

bool LowerExecutableReturnType(const mir::Module& module,
                               const ExecutableSignature& signature,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               std::string_view context,
                               std::optional<BackendTypeInfo>& return_type) {
    return_type.reset();
    if (signature.return_types.empty()) {
        return true;
    }

    const auto lowered_return_type = LowerFunctionReturnType(module, signature.return_types);
    if (!lowered_return_type.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend could not lower " + std::string(context),
                           diagnostics);
        return false;
    }

    return_type = *lowered_return_type;
    return true;
}

void RenderExecutableParameterList(const ExecutableFunctionSignature& function_signature,
                                   const std::vector<BackendTypeInfo>& param_types,
                                   bool include_names,
                                   std::ostringstream& stream) {
    for (std::size_t index = 0; index < function_signature.params.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << RenderLLVMParameter(param_types[index], *function_signature.params[index], include_names);
    }
}

bool LowerExecutableLocalTypes(const mir::Module& module,
                               const mir::Function& function,
                               const std::filesystem::path& source_path,
                               support::DiagnosticSink& diagnostics,
                               std::vector<BackendTypeInfo>& local_types) {
    local_types.clear();
    local_types.reserve(function.locals.size());
    for (const auto& local : function.locals) {
        BackendTypeInfo local_type;
        if (!LowerInstructionType(module,
                                  local.type,
                                  source_path,
                                  diagnostics,
                                  "local '" + local.name + "' for executable emission in function '" + function.name + "'",
                                  local_type)) {
            return false;
        }
        local_types.push_back(std::move(local_type));
    }
    return true;
}

void RenderExecutableEntryPrologue(const mir::Function& function,
                                   const ExecutableFunctionSignature& function_signature,
                                   const std::vector<BackendTypeInfo>& local_types,
                                   const std::vector<BackendTypeInfo>& param_types,
                                   std::ostringstream& stream) {
    for (std::size_t index = 0; index < function.locals.size(); ++index) {
        stream << "  " << LLVMLocalSlotName(function.locals[index].name) << " = alloca " << local_types[index].backend_name
               << ", align " << local_types[index].alignment << "\n";
    }
    for (std::size_t index = 0; index < function_signature.params.size(); ++index) {
        stream << "  store " << param_types[index].backend_name << " " << LLVMParamName(function_signature.params[index]->name)
               << ", ptr " << LLVMLocalSlotName(function_signature.params[index]->name) << ", align "
               << param_types[index].alignment << "\n";
    }
}

ExecutableFunctionState InitializeExecutableFunctionState(
    const mir::Module& module,
    const mir::Function& function,
    const ExecutableFunctionSignature& function_signature,
    std::size_t function_index,
    const ExecutableGlobals& globals,
    std::unordered_map<std::string, ExecutableStringConstant> string_constants) {
    ExecutableFunctionState state;
    state.module = &module;
    state.function = &function;
    state.signature = function_signature;
    state.globals = &globals;
    state.string_constants = std::move(string_constants);
    for (const auto& local : function.locals) {
        state.local_slots.emplace(local.name, LLVMLocalSlotName(local.name));
    }
    for (std::size_t block_index = 0; block_index < function.blocks.size(); ++block_index) {
        state.block_labels.emplace(function.blocks[block_index].label,
                                   LLVMBlockLabel(function_index, block_index, function.blocks[block_index].label));
    }
    return state;
}

bool CollectExecutableStringConstants(const mir::Module& module,
                                     const mir::Function& function,
                                     std::size_t function_index,
                                     const std::filesystem::path& source_path,
                                     support::DiagnosticSink& diagnostics,
                                     std::ostringstream& stream,
                                     std::unordered_map<std::string, ExecutableStringConstant>& string_constants) {
    std::size_t string_constant_index = 0;
    string_constants.clear();
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (instruction.kind != mir::Instruction::Kind::kConst || instruction.result.empty() || instruction.op.size() < 2 ||
                instruction.op.front() != '"') {
                continue;
            }

            BackendTypeInfo string_type;
            if (!LowerInstructionType(module,
                                      instruction.type,
                                      source_path,
                                      diagnostics,
                                      "string literal constant for executable emission in function '" + function.name + "'",
                                      string_type)) {
                return false;
            }
            if (string_type.backend_name != "{ptr, i64}") {
                continue;
            }

            const std::string decoded = DecodeStringLiteral(instruction.op);
            const std::string global_name = "@.mc.str." + std::to_string(function_index) + "." + std::to_string(string_constant_index++);
            const std::string array_type = "[" + std::to_string(decoded.size() + 1) + " x i8]";
            stream << global_name << " = private unnamed_addr constant " << array_type << " c\""
                   << EncodeLLVMStringBytes(decoded) << "\", align 1\n";
            string_constants.emplace(instruction.result,
                                     ExecutableStringConstant {
                                         .global_name = global_name,
                                         .array_type = array_type,
                                         .length = decoded.size(),
                                     });
        }
    }
    if (!string_constants.empty()) {
        stream << "\n";
    }
    return true;
}

}  // namespace executable_support

}  // namespace mc::codegen_llvm
