#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>

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

FreestandingKernelCommonPaths MakeFreestandingKernelCommonPaths(const std::filesystem::path& source_root);

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

void ExpectMirFirstMatchProjectionFile(std::string_view mir_text,
                                       std::initializer_list<std::string_view> selectors,
                                       const std::filesystem::path& expected_projection_path,
                                       const std::string& context);

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

void RunFreestandingKernelToolSuiteShard(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path,
                                         int shard);

void RunFreestandingKernelToolSuiteCase(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path,
                                        std::string_view case_name);

void RunFreestandingKernelToolSuiteShard1(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path);

void RunFreestandingKernelToolSuiteShard2(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path);

void RunFreestandingKernelToolSuiteShard3(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path);

void RunFreestandingKernelToolSuiteShard4(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path);

void RunFreestandingKernelToolSuiteShard5(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path);

void RunFreestandingSystemToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path);

void RunFreestandingToolSuite(const std::filesystem::path& source_root,
                              const std::filesystem::path& binary_root,
                              const std::filesystem::path& mc_path);

}  // namespace mc::tool_tests