#pragma once

#include "compiler/support/dump_paths.h"

#include <filesystem>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mc::test_support {
struct CommandOutcome;
}

namespace mc::tool_tests {

struct FreestandingKernelCommonPaths {
    std::filesystem::path project_path;
    std::filesystem::path main_source_path;
    std::filesystem::path roadmap_path;
    std::filesystem::path kernel_readme_path;
    std::filesystem::path repo_map_path;
    std::filesystem::path freestanding_readme_path;
    std::filesystem::path decision_log_path;
    std::filesystem::path backlog_path;
};

struct FreestandingKernelRunArtifacts {
    std::filesystem::path build_dir;
    mc::support::BuildArtifactTargets build_targets;
    mc::support::DumpTargets dump_targets;
    std::string run_output;
};

struct FreestandingKernelPhaseCheck {
    std::string_view label;
    std::string_view expected_run_lines_file;
    std::span<const std::string_view> required_object_files;
    std::span<const std::string_view> mir_selectors;
    std::string_view expected_mir_contract_file;
    std::string_view mir_contract;
};

struct FreestandingKernelRuntimePhaseDescriptor {
    int phase = 0;
    int shard = 0;
    std::string project;
    std::string target;
    std::string output_stem;
    std::string build_context;
    std::string run_context;
    std::string mir_contract;
    std::string label;
    std::string expected_run_lines_file;
    std::vector<std::string> required_object_file_storage;
    std::vector<std::string_view> required_object_files;
    std::vector<std::string> mir_selector_storage;
    std::vector<std::string_view> mir_selectors;
    std::string expected_mir_contract_file;

    void RefreshViews();
    FreestandingKernelPhaseCheck View() const;
};

struct FreestandingKernelTextAuditDescriptor {
    std::string label;
    std::string source_file;
    std::string context;
    std::vector<std::string> expected_contains;
};

struct FreestandingKernelArtifactAuditDescriptor {
    std::filesystem::path descriptor_dir;
    std::string label;
    std::string kind;
    std::string project_name;
    std::string output_stem;
    std::string build_context;
    std::string run_context;
    std::string expected_run_lines_file;
    std::string source_file;
    std::string main_source_file;
    std::string driver_support_source_file;
    std::string manual_support_source_file;
    int expected_exit_code = 0;
};

FreestandingKernelCommonPaths MakeFreestandingKernelCommonPaths(const std::filesystem::path& source_root);

std::filesystem::path ResolveFreestandingKernelGoldenPath(const std::filesystem::path& source_root,
                                                          std::string_view file_name);

std::vector<FreestandingKernelRuntimePhaseDescriptor> LoadFreestandingKernelRuntimePhaseDescriptors(
    const std::filesystem::path& source_root,
    int shard);

std::vector<FreestandingKernelRuntimePhaseDescriptor> LoadAllFreestandingKernelRuntimePhaseDescriptors(
    const std::filesystem::path& source_root);

std::vector<FreestandingKernelTextAuditDescriptor> LoadFreestandingKernelTextAuditDescriptors(
    const std::filesystem::path& source_root,
    std::string_view relative_root,
    std::string_view descriptor_file_name);

std::vector<FreestandingKernelArtifactAuditDescriptor> LoadFreestandingKernelArtifactAuditDescriptors(
    const std::filesystem::path& source_root);

bool FreestandingKernelCaseNameMatchesExpectedRunFile(std::string_view expected_run_lines_file,
                                                      std::string_view case_name);

const FreestandingKernelRuntimePhaseDescriptor& RequireFreestandingKernelRuntimePhaseDescriptor(
    const std::vector<FreestandingKernelRuntimePhaseDescriptor>& phase_checks,
    std::string_view expected_run_lines_file,
    std::string_view context_label);

const FreestandingKernelRuntimePhaseDescriptor* FindFreestandingKernelRuntimePhaseDescriptorForCase(
    std::span<const FreestandingKernelRuntimePhaseDescriptor> phase_checks,
    std::string_view case_name);

void RunFreestandingKernelRuntimePhaseCase(const std::filesystem::path& source_root,
                                           const std::filesystem::path& binary_root,
                                           const std::filesystem::path& mc_path,
                                           int shard,
                                           std::string_view case_name,
                                           std::string_view context_label);

void RunFreestandingKernelRuntimePhaseCase(const std::filesystem::path& source_root,
                                           const std::filesystem::path& binary_root,
                                           const std::filesystem::path& mc_path,
                                           std::string_view case_name,
                                           std::string_view context_label);

std::filesystem::path ResolveCanopusRoadmapPath(const std::filesystem::path& source_root,
                                                int phase_number);

void MaybeCleanBuildDir(const std::filesystem::path& build_dir);

FreestandingKernelRunArtifacts BuildAndRunFreestandingKernelTarget(const std::filesystem::path& mc_path,
                                                                  const std::filesystem::path& project_path,
                                                                  const std::filesystem::path& main_source_path,
                                                                  const std::filesystem::path& build_dir,
                                                                  std::string_view output_stem,
                                                                  const std::string& build_context,
                                                                  const std::string& run_context);

void CompileBootstrapCObjectAndExpectSuccess(const std::filesystem::path& source_path,
                                             const std::filesystem::path& object_path,
                                             const std::filesystem::path& output_path,
                                             const std::string& context);

void LinkBootstrapObjectsAndExpectSuccess(const std::vector<std::filesystem::path>& object_paths,
                                          const std::filesystem::path& executable_path,
                                          const std::filesystem::path& output_path,
                                          const std::string& context);

std::vector<std::filesystem::path> CollectBootstrapObjectFiles(const std::filesystem::path& object_dir);

std::string LinkBootstrapObjectsAndRunExpectExitCode(const std::vector<std::filesystem::path>& object_paths,
                                                     const std::filesystem::path& executable_path,
                                                     const std::filesystem::path& link_output_path,
                                                     const std::filesystem::path& run_output_path,
                                                     int expected_exit_code,
                                                     const std::string& link_context,
                                                     const std::string& run_context);

std::filesystem::path ResolvePlanDocPath(const std::filesystem::path& source_root,
                                         std::string_view file_name);

std::filesystem::path WriteBasicProject(const std::filesystem::path& root,
                                        std::string_view helper_source,
                                        std::string_view main_source);

std::filesystem::path WriteTestProject(const std::filesystem::path& root,
                                       std::string_view main_source);

std::string RunProjectTestAndExpectSuccess(const std::filesystem::path& mc_path,
                                           const std::filesystem::path& project_path,
                                           const std::filesystem::path& build_dir,
                                           std::string_view output_name,
                                           const std::string& context);

void BuildProjectTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                        const std::filesystem::path& project_path,
                                        const std::filesystem::path& build_dir,
                                        std::string_view target_name,
                                        std::string_view output_name,
                                        const std::string& context);

std::string BuildProjectTargetAndCapture(const std::filesystem::path& mc_path,
                                         const std::filesystem::path& project_path,
                                         const std::filesystem::path& build_dir,
                                         std::string_view target_name,
                                         std::string_view output_name,
                                         const std::string& context);

std::string BuildProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context);

std::string CheckProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context);

std::string CheckProjectTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context);

std::string RunProjectTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                             const std::filesystem::path& project_path,
                                             const std::filesystem::path& build_dir,
                                             std::string_view target_name,
                                             const std::filesystem::path& sample_path,
                                             std::string_view output_name,
                                             const std::string& context);

std::string RunProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                             const std::filesystem::path& project_path,
                                             const std::filesystem::path& build_dir,
                                             std::string_view target_name,
                                             std::string_view output_name,
                                             const std::string& context);

std::string RunProjectTestTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                                 const std::filesystem::path& project_path,
                                                 const std::filesystem::path& build_dir,
                                                 std::string_view target_name,
                                                 std::string_view output_name,
                                                 const std::string& context);

void ExpectExitCodeAtLeast(const mc::test_support::CommandOutcome& outcome,
                          int minimum_exit_code,
                          std::string_view output,
                          const std::string& context);

void ExpectMirFirstMatchProjection(std::string_view mir_text,
                                   std::initializer_list<std::string_view> selectors,
                                   std::string_view expected_projection,
                                   const std::string& context);

void ExpectMirFirstMatchProjectionSpan(std::string_view mir_text,
                                       std::span<const std::string_view> selectors,
                                       std::string_view expected_projection,
                                       const std::string& context);

void ExpectTextContainsLines(std::string_view actual_text,
                             std::string_view expected_lines,
                             const std::string& context);

void ExpectTextContainsLinesFile(std::string_view actual_text,
                                 const std::filesystem::path& expected_lines_path,
                                 const std::string& context);

void ExpectMirFirstMatchProjectionFile(std::string_view mir_text,
                                       std::initializer_list<std::string_view> selectors,
                                       const std::filesystem::path& expected_projection_path,
                                       const std::string& context);

void ExpectMirFirstMatchProjectionFileSpan(std::string_view mir_text,
                                           std::span<const std::string_view> selectors,
                                           const std::filesystem::path& expected_projection_path,
                                           const std::string& context);

void ExpectFreestandingKernelPhaseFromArtifacts(const std::filesystem::path& source_root,
                                                const std::filesystem::path& object_dir,
                                                std::string_view run_output,
                                                std::string_view mir_text,
                                                const FreestandingKernelPhaseCheck& phase_check);

void RunWorkflowToolSuite(const std::filesystem::path& source_root,
                          const std::filesystem::path& binary_root,
                          const std::filesystem::path& mc_path);

void RunBuildStateToolSuite(const std::filesystem::path& binary_root,
                            const std::filesystem::path& mc_path);

void RunRealProjectToolSuite(const std::filesystem::path& source_root,
                             const std::filesystem::path& binary_root,
                             const std::filesystem::path& mc_path);

void RunRealProjectToolSuiteCase(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path,
                                 std::string_view case_name);

void RunFreestandingBootstrapToolSuite(const std::filesystem::path& source_root,
                                       const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path);

void RunFreestandingKernelToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path);

void RunFreestandingKernelSyntheticSuite(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path);

void RunFreestandingKernelMetadataSuite(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path);

void RunFreestandingKernelArtifactSuite(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path);

void RunFreestandingKernelSyntheticSuiteCase(const std::filesystem::path& source_root,
                                             const std::filesystem::path& binary_root,
                                             const std::filesystem::path& mc_path,
                                             std::string_view case_name);

void RunFreestandingSystemToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path);

void RunFreestandingToolSuite(const std::filesystem::path& source_root,
                              const std::filesystem::path& binary_root,
                              const std::filesystem::path& mc_path);

}  // namespace mc::tool_tests