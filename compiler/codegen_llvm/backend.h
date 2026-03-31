#ifndef C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_H_
#define C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_H_

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "compiler/mir/mir.h"
#include "compiler/support/diagnostics.h"

namespace mc::codegen_llvm {

struct TargetConfig {
    std::string triple;
    std::string target_family;
    std::string object_format;
    bool hosted = true;
};

struct LowerOptions {
    TargetConfig target;
};

struct BackendModule {
    TargetConfig target;
    std::vector<std::string> functions;
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

}  // namespace mc::codegen_llvm

#endif  // C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_H_