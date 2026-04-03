#include "compiler/codegen_llvm/backend_internal.h"

#include <cctype>
#include <sstream>

namespace mc::codegen_llvm {

namespace {

bool IsSafeLLVMIdentifierChar(unsigned char ch) {
    return std::isalnum(ch) || ch == '.' || ch == '_' || ch == '$' || ch == '-';
}

std::string SanitizeLLVMIdentifier(std::string_view source_name) {
    std::ostringstream sanitized;
    for (const unsigned char ch : source_name) {
        if (IsSafeLLVMIdentifierChar(ch)) {
            sanitized << static_cast<char>(ch);
            continue;
        }

        constexpr char kHex[] = "0123456789ABCDEF";
        sanitized << '_'
                  << kHex[(ch >> 4) & 0xF]
                  << kHex[ch & 0xF]
                  << '_';
    }
    return sanitized.str();
}

}  // namespace

std::string BackendFunctionName(const std::string& source_name) {
    return "@" + source_name;
}

std::string BackendGlobalName(const std::string& source_name) {
    return "@global." + source_name;
}

std::string BackendLocalName(const std::string& source_name) {
    return "%local." + source_name;
}

std::string BackendBlockName(std::size_t function_index,
                             std::size_t block_index,
                             const std::string& source_name) {
    return "bb" + std::to_string(function_index) + "_" + std::to_string(block_index) + "." + source_name;
}

std::string BackendTempName(std::size_t function_index,
                            std::size_t block_index,
                            std::size_t instruction_index) {
    // These names are stable only for the current single-pass lowering path.
    // Future MIR rewrites that reorder or delete instructions must not depend
    // on the encoded indices remaining stable across transformations.
    return "%t" + std::to_string(function_index) + "_" + std::to_string(block_index) + "_" +
           std::to_string(instruction_index);
}

std::string LLVMFunctionSymbol(std::string_view source_name,
                               bool wrap_hosted_main) {
    if (wrap_hosted_main && source_name == "main") {
        return "@__mc_user_main";
    }
    return "@" + SanitizeLLVMIdentifier(source_name);
}

std::string LLVMBlockLabel(std::size_t function_index,
                           std::size_t block_index,
                           const std::string& source_label) {
    return "bb" + std::to_string(function_index) + "_" + std::to_string(block_index) + "." + source_label;
}

std::string LLVMLocalSlotName(std::string_view source_name) {
    return "%local." + SanitizeLLVMIdentifier(source_name);
}

std::string LLVMParamName(std::string_view source_name) {
    return "%arg." + SanitizeLLVMIdentifier(source_name);
}

std::string LLVMTempName(std::size_t function_index,
                         std::size_t block_index,
                         std::size_t instruction_index) {
    return "%t" + std::to_string(function_index) + "_" + std::to_string(block_index) + "_" +
           std::to_string(instruction_index);
}

std::string LLVMGlobalName(std::string_view source_name) {
    return "@global." + SanitizeLLVMIdentifier(source_name);
}

}  // namespace mc::codegen_llvm