#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace mc::tool_tests {

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

void RunWorkflowToolSuite(const std::filesystem::path& source_root,
                          const std::filesystem::path& binary_root,
                          const std::filesystem::path& mc_path);

void RunBuildStateToolSuite(const std::filesystem::path& binary_root,
                            const std::filesystem::path& mc_path);

void RunRealProjectToolSuite(const std::filesystem::path& source_root,
                             const std::filesystem::path& binary_root,
                             const std::filesystem::path& mc_path);

void RunFreestandingBootstrapToolSuite(const std::filesystem::path& source_root,
                                       const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path);

void RunFreestandingKernelToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path);

void RunFreestandingSystemToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path);

void RunFreestandingToolSuite(const std::filesystem::path& source_root,
                              const std::filesystem::path& binary_root,
                              const std::filesystem::path& mc_path);

}  // namespace mc::tool_tests