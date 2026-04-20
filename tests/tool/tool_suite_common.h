#pragma once

#include <filesystem>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

namespace mc::test_support {
struct CommandOutcome;
}

namespace mc::tool_tests {

std::filesystem::path WriteBasicProject(
    const std::filesystem::path& root,
    std::string_view helper_source,
    std::string_view main_source);

std::filesystem::path WriteTestProject(
    const std::filesystem::path& root,
    std::string_view main_source);

std::string RunProjectTestAndExpectSuccess(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::string_view output_name,
    const std::string& context);

std::string RunProjectAndExpectSuccess(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::initializer_list<std::string> program_args,
    std::string_view output_name,
    const std::string& context);

void BuildProjectTargetAndExpectSuccess(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::string_view target_name,
    std::string_view output_name,
    const std::string& context);

std::string BuildProjectTargetAndCapture(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::string_view target_name,
    std::string_view output_name,
    const std::string& context);

std::filesystem::path ParseBuiltProjectExecutablePath(
    std::string_view build_output,
    std::string_view context);

std::string BuildProjectTargetAndExpectFailure(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::string_view target_name,
    std::string_view output_name,
    const std::string& context);

std::string CheckProjectTargetAndExpectFailure(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::string_view target_name,
    std::string_view output_name,
    const std::string& context);

std::string CheckProjectTargetAndExpectSuccess(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::string_view target_name,
    std::string_view output_name,
    const std::string& context);

std::string RunProjectTargetAndExpectSuccess(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::string_view target_name,
    const std::filesystem::path& sample_path,
    std::string_view output_name,
    const std::string& context);

std::string RunProjectTargetAndExpectFailure(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::string_view target_name,
    std::string_view output_name,
    const std::string& context);

std::string RunProjectTestTargetAndExpectSuccess(
    const std::filesystem::path& mc_path,
    const std::filesystem::path& project_path,
    const std::filesystem::path& build_dir,
    std::string_view target_name,
    std::string_view output_name,
    const std::string& context);

void ExpectExitCodeAtLeast(
    const mc::test_support::CommandOutcome& outcome,
    int minimum_exit_code,
    std::string_view output,
    const std::string& context);

void ExpectMirFirstMatchProjection(
    std::string_view mir_text,
    std::initializer_list<std::string_view> selectors,
    std::string_view expected_projection,
    const std::string& context);

void ExpectMirFirstMatchProjectionSpan(
    std::string_view mir_text,
    std::span<const std::string_view> selectors,
    std::string_view expected_projection,
    const std::string& context);

void ExpectTextContainsLines(
    std::string_view actual_text,
    std::string_view expected_lines,
    const std::string& context);

void ExpectTextContainsLinesFile(
    std::string_view actual_text,
    const std::filesystem::path& expected_lines_path,
    const std::string& context);

void ExpectMirFirstMatchProjectionFile(
    std::string_view mir_text,
    std::initializer_list<std::string_view> selectors,
    const std::filesystem::path& expected_projection_path,
    const std::string& context);

void ExpectMirFirstMatchProjectionFileSpan(
    std::string_view mir_text,
    std::span<const std::string_view> selectors,
    const std::filesystem::path& expected_projection_path,
    const std::string& context);

void RunWorkflowToolSuite(
    const std::filesystem::path& source_root,
    const std::filesystem::path& binary_root,
    const std::filesystem::path& mc_path);

void RunWorkflowToolSuiteCase(
    const std::filesystem::path& source_root,
    const std::filesystem::path& binary_root,
    const std::filesystem::path& mc_path,
    std::string_view case_name);

void RunBuildStateToolSuite(
    const std::filesystem::path& binary_root,
    const std::filesystem::path& mc_path);

void RunRealProjectToolSuite(
    const std::filesystem::path& source_root,
    const std::filesystem::path& binary_root,
    const std::filesystem::path& mc_path);

void RunRealProjectToolSuiteCase(
    const std::filesystem::path& source_root,
    const std::filesystem::path& binary_root,
    const std::filesystem::path& mc_path,
    std::string_view case_name);

}  // namespace mc::tool_tests