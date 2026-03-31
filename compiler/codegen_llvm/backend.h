#ifndef C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_H_
#define C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_H_

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "compiler/mir/mir.h"
#include "compiler/sema/type.h"
#include "compiler/support/diagnostics.h"

namespace mc::codegen_llvm {

struct BackendTypeInfo {
    std::string source_name;
    std::string backend_name;
    std::size_t size = 0;
    std::size_t alignment = 0;
};

struct TargetConfig {
    std::string triple;
    std::string target_family;
    std::string object_format;
    bool hosted = true;
};

struct LowerOptions {
    TargetConfig target;
};

struct BackendLocal {
    std::string source_name;
    std::string backend_name;
    sema::Type type;
    BackendTypeInfo lowered_type;
    bool is_parameter = false;
    bool is_mutable = true;
};

struct BackendBlock {
    std::string source_label;
    std::string backend_label;
    std::vector<std::string> instructions;
    std::string terminator;
};

struct BackendFunction {
    std::string source_name;
    std::string backend_name;
    std::vector<sema::Type> return_types;
    std::vector<BackendTypeInfo> backend_return_types;
    std::vector<BackendLocal> locals;
    std::vector<BackendBlock> blocks;
};

struct BackendModule {
    TargetConfig target;
    std::string inspect_surface;
    std::vector<BackendFunction> functions;
};

struct LowerResult {
    std::unique_ptr<BackendModule> module;
    bool ok = false;
};

TargetConfig BootstrapTargetConfig();

LowerResult LowerModule(const mir::Module& module,
                        const std::filesystem::path& source_path,
                        const LowerOptions& options,
                        support::DiagnosticSink& diagnostics);

std::string DumpModule(const BackendModule& module);

}  // namespace mc::codegen_llvm

#endif  // C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_H_
