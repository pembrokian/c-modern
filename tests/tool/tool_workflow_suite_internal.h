#pragma once

#include <filesystem>

namespace mc::tool_tests {

void RunWorkflowHelpSuite(const std::filesystem::path& binary_root,
                          const std::filesystem::path& mc_path);

void RunWorkflowTestCommandSuite(const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);

void RunWorkflowProjectValidationSuite(const std::filesystem::path& source_root,
                                       const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path);

void RunWorkflowMultifileModuleSuite(const std::filesystem::path& binary_root,
                                     const std::filesystem::path& mc_path);

void RunWorkflowKernelResetLaneSuite(const std::filesystem::path& source_root,
                                     const std::filesystem::path& binary_root,
                                     const std::filesystem::path& mc_path);

void RunWorkflowKernelResetLaneFullSuite(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path);

}  // namespace mc::tool_tests
