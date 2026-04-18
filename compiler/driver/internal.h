#ifndef C_MODERN_COMPILER_DRIVER_INTERNAL_H_
#define C_MODERN_COMPILER_DRIVER_INTERNAL_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <sys/types.h>

#include "compiler/ast/ast.h"
#include "compiler/codegen_llvm/backend.h"
#include "compiler/driver/project.h"
#include "compiler/mir/mir.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"
#include "compiler/support/dump_paths.h"

namespace mc::driver {

struct CommandOptions {
    std::filesystem::path source_path;
    std::optional<std::filesystem::path> project_path;
    std::optional<std::string> target_name;
    std::optional<std::string> mode_override;
    std::optional<std::string> env_override;
    std::filesystem::path build_dir = "build/debug";
    bool build_dir_explicit = false;
    std::vector<std::filesystem::path> import_roots;
    std::vector<std::string> run_arguments;
    bool emit_dump_paths = false;
    bool dump_ast = false;
    bool dump_mir = false;
    bool dump_backend = false;
};

enum class InvocationKind {
    kDirectSource,
    kProjectTarget,
};

struct BuildUnit {
    std::string module_name;
    std::string artifact_key;
    std::filesystem::path source_path;
    mc::parse::ParseResult parse_result;
    mc::sema::CheckResult sema_result;
    mc::mir::LowerResult mir_result;
    std::filesystem::path llvm_ir_path;
    std::filesystem::path object_path;
    std::string interface_hash;
    bool reused_object = false;
};

struct ImportedInterfaceData {
    std::unordered_map<std::string, sema::Module> modules;
    std::vector<std::pair<std::string, std::string>> interface_hashes;
};

struct DirectSourceBuildResult {
    std::filesystem::path source_path;
    support::DumpTargets dump_targets;
    support::BuildArtifactTargets build_targets;
    std::filesystem::path executable_path;
};

struct ModuleBuildState {
    std::string target_identity;
    std::string package_identity;
    std::string mode;
    std::string env;
    std::string source_hash;
    std::string interface_hash;
    bool wrap_hosted_main = false;
    std::vector<std::pair<std::string, std::string>> imported_interface_hashes;
};

struct CompileImport {
    std::string module_name;
    std::string artifact_key;
    std::string package_identity;
    std::filesystem::path source_path;
    std::vector<std::filesystem::path> source_paths;
    ast::SourceFile::ModuleKind module_kind = ast::SourceFile::ModuleKind::kOrdinary;
};

struct CompileNode {
    std::string module_name;
    std::string artifact_key;
    std::string package_identity;
    std::filesystem::path source_path;
    std::vector<std::filesystem::path> source_paths;
    mc::parse::ParseResult parse_result;
    std::vector<CompileImport> imports;
};

struct CompileGraph {
    std::filesystem::path entry_source_path;
    std::vector<std::filesystem::path> import_roots;
    std::filesystem::path build_dir;
    std::string mode;
    std::string env;
    bool wrap_entry_main = true;
    bool namespace_entry_module = false;
    mc::codegen_llvm::TargetConfig target_config;
    std::vector<CompileNode> nodes;
};

struct TargetBuildGraph {
    ProjectFile project;
    ProjectTarget target;
    CompileGraph compile_graph;
    std::vector<TargetBuildGraph> linked_targets;
};

struct ProjectBuildResult {
    std::vector<BuildUnit> units;
    std::optional<std::filesystem::path> executable_path;
    std::optional<std::filesystem::path> static_library_path;
    std::vector<std::filesystem::path> link_library_paths;
};

struct CapturedCommandResult {
    bool exited = false;
    int exit_code = -1;
    bool signaled = false;
    int signal_number = -1;
    bool timed_out = false;
    std::string output;
};

enum class WaitForChildResult {
    kExited,
    kWaitError,
    kTimedOut,
};

constexpr int kModuleBuildStateFormatVersion = 2;

std::optional<std::filesystem::path> DiscoverHostedRuntimeSupportSource(const std::filesystem::path& source_path);
std::optional<std::filesystem::path> DiscoverRuntimeSupportSource(std::string_view env,
                                                                  std::string_view startup,
                                                                  const std::filesystem::path& source_path);
std::vector<std::filesystem::path> ComputeEffectiveImportRoots(const std::filesystem::path& source_path,
                                                               const std::vector<std::filesystem::path>& import_roots);
std::optional<std::string> ReadSourceText(const std::filesystem::path& path,
                                          support::DiagnosticSink& diagnostics);
std::optional<mc::parse::ParseResult> ParseSourcePath(const std::filesystem::path& path,
                                                      support::DiagnosticSink& diagnostics);

std::filesystem::path DefaultProjectPath();
std::string BootstrapTargetIdentity();
bool IsSupportedMode(std::string_view mode);
bool IsSupportedEnv(std::string_view env);
bool IsExecutableTargetKind(std::string_view kind);
bool IsStaticLibraryTargetKind(std::string_view kind);
bool IsPathWithinRoot(const std::filesystem::path& path,
                      const std::filesystem::path& root);
std::optional<std::filesystem::path> DiscoverRepositoryRoot(const std::filesystem::path& start_path);

std::string HexU64(std::uint64_t value);
std::string HashText(std::string_view text);
std::filesystem::path ComputeModuleStatePath(const std::filesystem::path& source_path,
                                             const std::filesystem::path& build_dir);
std::filesystem::path ComputeLogicalModuleStatePath(std::string_view artifact_key,
                                                    const std::filesystem::path& build_dir);
std::filesystem::path ComputeRuntimeObjectPath(const std::filesystem::path& entry_source_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view env,
                                               std::string_view startup);
std::filesystem::path ComputeLogicalRuntimeObjectPath(std::string_view artifact_key,
                                                      const std::filesystem::path& build_dir,
                                                      std::string_view env,
                                                      std::string_view startup);

std::string FormatCommandForDisplay(const std::vector<std::string>& args);
WaitForChildResult WaitForChildProcess(pid_t pid,
                                       const std::optional<int>& timeout_ms,
                                       int& status);
int RunExecutableCommand(const std::filesystem::path& executable_path,
                         const std::vector<std::string>& arguments,
                         const std::optional<int>& timeout_ms = std::nullopt);
CapturedCommandResult RunExecutableCapture(const std::filesystem::path& executable_path,
                                          const std::vector<std::string>& arguments,
                                          const std::filesystem::path& output_path,
                                          const std::optional<int>& timeout_ms = std::nullopt);

std::optional<ModuleBuildState> LoadModuleBuildState(const std::filesystem::path& path,
                                                     support::DiagnosticSink& diagnostics);
bool WriteModuleBuildState(const std::filesystem::path& path,
                           const ModuleBuildState& state,
                           support::DiagnosticSink& diagnostics);
bool ShouldReuseModuleObject(const ModuleBuildState& state,
                             const ModuleBuildState& current,
                             const std::filesystem::path& object_path,
                             const std::filesystem::path& mci_path);

std::optional<ProjectFile> LoadSelectedProject(const CommandOptions& options,
                                               support::DiagnosticSink& diagnostics);
bool SupportsBootstrapTarget(const ProjectTarget& target,
                             const ProjectFile& project,
                             support::DiagnosticSink& diagnostics);

std::optional<CompileGraph> DiscoverModuleGraph(const std::filesystem::path& entry_source_path,
                                                const std::vector<std::filesystem::path>& import_roots,
                                                const std::function<std::optional<std::string>(const std::filesystem::path&)>& package_identity_for_source,
                                                const std::function<std::vector<CompileImport>(std::string_view)>& known_module_sources,
                                                const std::unordered_set<std::string>& external_module_paths,
                                                const std::filesystem::path& build_dir,
                                                const mc::codegen_llvm::TargetConfig& target_config,
                                                std::string mode,
                                                std::string env,
                                                bool wrap_entry_main,
                                                bool namespace_entry_module,
                                                support::DiagnosticSink& diagnostics);
void AssertCompileGraphTopologicalOrder(const CompileGraph& graph);

std::optional<TargetBuildGraph> BuildResolvedTargetGraph(const ProjectFile& project,
                                                         const ProjectTarget& target,
                                                         const std::vector<std::filesystem::path>& cli_import_roots,
                                                         support::DiagnosticSink& diagnostics,
                                                         std::unordered_set<std::string>& visiting_targets);
std::optional<TargetBuildGraph> BuildTargetGraph(const CommandOptions& options,
                                                 support::DiagnosticSink& diagnostics);
std::optional<std::vector<BuildUnit>> CompileModuleGraph(CompileGraph& graph,
                                                         support::DiagnosticSink& diagnostics,
                                                         bool emit_objects);
bool PrepareLinkedTargetInterfaces(std::vector<TargetBuildGraph>& linked_targets,
                                   support::DiagnosticSink& diagnostics);
std::optional<BuildUnit> CompileToMir(const CommandOptions& options,
                                      support::DiagnosticSink& diagnostics,
                                      bool include_imports_for_build);
std::optional<ProjectBuildResult> BuildProjectTarget(TargetBuildGraph& graph,
                                                     support::DiagnosticSink& diagnostics);
bool WriteTextArtifact(const std::filesystem::path& path,
                       std::string_view contents,
                       std::string_view description,
                       support::DiagnosticSink& diagnostics);
std::optional<DirectSourceBuildResult> BuildDirectSource(const CommandOptions& options,
                                                         support::DiagnosticSink& diagnostics);
std::string MergeRenderedDiagnostics(std::string_view primary,
                                     std::string_view secondary);
int RunTestCommand(const CommandOptions& options);

}  // namespace mc::driver

#endif  // C_MODERN_COMPILER_DRIVER_INTERNAL_H_