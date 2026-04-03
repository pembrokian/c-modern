#ifndef C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_INTERNAL_H_
#define C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_INTERNAL_H_

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "compiler/codegen_llvm/backend.h"

namespace mc::codegen_llvm {

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

}  // namespace mc::codegen_llvm

#endif  // C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_INTERNAL_H_