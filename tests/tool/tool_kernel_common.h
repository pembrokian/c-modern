#pragma once

#include "compiler/support/dump_paths.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
    std::string_view run_contract;
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
    std::string run_contract;
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

void ExpectFreestandingKernelPhaseFromArtifacts(const std::filesystem::path& source_root,
                                                const std::filesystem::path& object_dir,
                                                std::string_view run_output,
                                                std::string_view mir_text,
                                                const FreestandingKernelPhaseCheck& phase_check);

}  // namespace mc::tool_tests
