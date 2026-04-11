#ifndef C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_H_
#define C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_H_

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
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
    std::vector<std::string> host_tool_prefix;
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
    bool is_noalias = false;
    bool is_mutable = true;
};

struct BackendBlock {
    std::string source_label;
    std::string backend_label;
    std::vector<std::string> instructions;
    std::string terminator;
};

struct BackendGlobal {
    bool is_const = false;
    bool is_extern = false;
    std::string source_name;
    std::string backend_name;
    sema::Type type;
    BackendTypeInfo lowered_type;
    std::vector<std::string> initializers;
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
    std::vector<BackendGlobal> globals;
    std::vector<BackendFunction> functions;
};

struct LowerResult {
    std::unique_ptr<BackendModule> module;
    bool ok = false;
};

struct BuildArtifacts {
    std::filesystem::path llvm_ir_path;
    std::filesystem::path object_path;
    std::filesystem::path executable_path;
};

struct ObjectArtifacts {
    std::filesystem::path llvm_ir_path;
    std::filesystem::path object_path;
};

struct ObjectBuildOptions {
    TargetConfig target;
    ObjectArtifacts artifacts;
    bool wrap_hosted_main = false;
};

struct ObjectBuildResult {
    ObjectArtifacts artifacts;
    bool ok = false;
};

struct LinkOptions {
    TargetConfig target;
    std::vector<std::filesystem::path> object_paths;
    std::vector<std::filesystem::path> extra_input_paths;
    std::vector<std::filesystem::path> library_paths;
    std::optional<std::filesystem::path> runtime_source_path;
    std::filesystem::path runtime_object_path;
    std::filesystem::path executable_path;
};

struct LinkResult {
    std::filesystem::path runtime_object_path;
    std::filesystem::path executable_path;
    bool ok = false;
};

struct ArchiveOptions {
    TargetConfig target;
    std::vector<std::filesystem::path> object_paths;
    std::filesystem::path archive_path;
};

struct ArchiveResult {
    std::filesystem::path archive_path;
    bool ok = false;
};

struct BuildOptions {
    TargetConfig target;
    std::optional<std::filesystem::path> runtime_source_path;
    BuildArtifacts artifacts;
};

struct BuildResult {
    BuildArtifacts artifacts;
    bool ok = false;
};

TargetConfig BootstrapTargetConfig();

LowerResult LowerModule(const mir::Module& module,
                        const std::filesystem::path& source_path,
                        const LowerOptions& options,
                        support::DiagnosticSink& diagnostics);

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

std::string DumpModule(const BackendModule& module);

}  // namespace mc::codegen_llvm

#endif  // C_MODERN_COMPILER_CODEGEN_LLVM_BACKEND_H_
