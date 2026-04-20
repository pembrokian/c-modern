#include "tests/tool/tool_suite_common.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <vector>

#include "tests/support/process_utils.h"

namespace mc::tool_tests {

using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;

enum class ProjectCommandExpectation {
    success,
    failure,
};

enum class ProjectCommandOutputMode {
    discard,
    capture,
};

std::string NormalizeProjectionText(std::string_view text) {
    std::string normalized(text);
    while (!normalized.empty() && (normalized.back() == '\n' || normalized.back() == '\r')) {
        normalized.pop_back();
    }
    return normalized;
}

std::vector<std::string> SplitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::istringstream stream{std::string(text)};
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(std::move(line));
    }
    return lines;
}

std::vector<std::string> BuildProjectCommandArgs(const std::filesystem::path& mc_path,
                                                 std::string_view subcommand,
                                                 const std::filesystem::path& project_path,
                                                 const std::filesystem::path& build_dir,
                                                 std::string_view target_name,
                                                 std::optional<std::filesystem::path> sample_path) {
    std::vector<std::string> args{
        mc_path.generic_string(),
        std::string(subcommand),
        "--project",
        project_path.generic_string(),
    };
    if (!target_name.empty()) {
        args.push_back("--target");
        args.push_back(std::string(target_name));
    }
    args.push_back("--build-dir");
    args.push_back(build_dir.generic_string());
    if (sample_path.has_value()) {
        args.push_back("--");
        args.push_back(sample_path->generic_string());
    }
    return args;
}

std::vector<std::string> BuildProjectRunCommandArgs(const std::filesystem::path& mc_path,
                                                    const std::filesystem::path& project_path,
                                                    const std::filesystem::path& build_dir,
                                                    std::initializer_list<std::string> program_args) {
    std::vector<std::string> args = BuildProjectCommandArgs(mc_path,
                                                            "run",
                                                            project_path,
                                                            build_dir,
                                                            {},
                                                            std::nullopt);
    if (program_args.size() != 0) {
        args.push_back("--");
        args.insert(args.end(), program_args.begin(), program_args.end());
    }
    return args;
}

std::string RunProjectCommandAndCheck(const std::filesystem::path& output_path,
                                      const std::vector<std::string>& args,
                                      const std::string& context,
                                      ProjectCommandExpectation expectation,
                                      ProjectCommandOutputMode output_mode) {
    const auto [outcome, output] = RunCommandCapture(args, output_path, context);
    switch (expectation) {
    case ProjectCommandExpectation::success:
        if (!outcome.exited || outcome.exit_code != 0) {
            Fail(context + " should pass:\n" + output);
        }
        break;
    case ProjectCommandExpectation::failure:
        if (!outcome.exited || outcome.exit_code == 0) {
            Fail(context + " should fail:\n" + output);
        }
        break;
    }

    switch (output_mode) {
    case ProjectCommandOutputMode::discard:
        return {};
    case ProjectCommandOutputMode::capture:
        return output;
    }
}

std::filesystem::path WriteBasicProject(const std::filesystem::path& root,
                                        std::string_view helper_source,
                                        std::string_view main_source) {
    std::filesystem::remove_all(root);
    WriteFile(root / "build.toml",
              "schema = 1\n"
              "project = \"phase7-generated\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"app\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(root / "src/helper.mc", helper_source);
    WriteFile(root / "src/main.mc", main_source);
    return root / "build.toml";
}

std::filesystem::path WriteTestProject(const std::filesystem::path& root,
                                       std::string_view main_source) {
    std::filesystem::remove_all(root);
    WriteFile(root / "build.toml",
              "schema = 1\n"
              "project = \"phase7-tests\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"app\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.app.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 5000\n");
    WriteFile(root / "src/main.mc", main_source);
    return root / "build.toml";
}

std::string RunProjectTestAndExpectSuccess(const std::filesystem::path& mc_path,
                                           const std::filesystem::path& project_path,
                                           const std::filesystem::path& build_dir,
                                           std::string_view output_name,
                                           const std::string& context) {
    return RunProjectCommandAndCheck(build_dir / std::string(output_name),
                                     BuildProjectCommandArgs(mc_path,
                                                             "test",
                                                             project_path,
                                                             build_dir,
                                                             {},
                                                             std::nullopt),
                                     context,
                                     ProjectCommandExpectation::success,
                                     ProjectCommandOutputMode::capture);
}

std::string RunProjectAndExpectSuccess(const std::filesystem::path& mc_path,
                                       const std::filesystem::path& project_path,
                                       const std::filesystem::path& build_dir,
                                       std::initializer_list<std::string> program_args,
                                       std::string_view output_name,
                                       const std::string& context) {
    return RunProjectCommandAndCheck(build_dir / std::string(output_name),
                                     BuildProjectRunCommandArgs(mc_path,
                                                                project_path,
                                                                build_dir,
                                                                program_args),
                                     context,
                                     ProjectCommandExpectation::success,
                                     ProjectCommandOutputMode::capture);
}

void BuildProjectTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                        const std::filesystem::path& project_path,
                                        const std::filesystem::path& build_dir,
                                        std::string_view target_name,
                                        std::string_view output_name,
                                        const std::string& context) {
    RunProjectCommandAndCheck(build_dir / std::string(output_name),
                              BuildProjectCommandArgs(mc_path,
                                                      "build",
                                                      project_path,
                                                      build_dir,
                                                      target_name,
                                                      std::nullopt),
                              context,
                              ProjectCommandExpectation::success,
                              ProjectCommandOutputMode::discard);
}

std::string BuildProjectTargetAndCapture(const std::filesystem::path& mc_path,
                                         const std::filesystem::path& project_path,
                                         const std::filesystem::path& build_dir,
                                         std::string_view target_name,
                                         std::string_view output_name,
                                         const std::string& context) {
    return RunProjectCommandAndCheck(build_dir / std::string(output_name),
                                     BuildProjectCommandArgs(mc_path,
                                                             "build",
                                                             project_path,
                                                             build_dir,
                                                             target_name,
                                                             std::nullopt),
                                     context,
                                     ProjectCommandExpectation::success,
                                     ProjectCommandOutputMode::capture);
}

std::filesystem::path ParseBuiltProjectExecutablePath(std::string_view build_output,
                                                      std::string_view context) {
    constexpr std::string_view kPrefix = "built target ";
    const size_t line_start = build_output.rfind(kPrefix);
    if (line_start == std::string_view::npos) {
        Fail("expected build output summary for " + std::string(context) + ":\n" +
             std::string(build_output));
    }

    const size_t arrow_pos = build_output.find(" -> ", line_start);
    if (arrow_pos == std::string_view::npos) {
        Fail("expected build output executable path for " + std::string(context) + ":\n" +
             std::string(build_output));
    }

    const size_t path_start = arrow_pos + 4;
    size_t path_end = build_output.find(" (", path_start);
    if (path_end == std::string_view::npos) {
        path_end = build_output.find('\n', path_start);
    }
    if (path_end == std::string_view::npos) {
        path_end = build_output.size();
    }
    if (path_end <= path_start) {
        Fail("expected non-empty executable path for " + std::string(context) + ":\n" +
             std::string(build_output));
    }

    return std::filesystem::path(std::string(build_output.substr(path_start, path_end - path_start)));
}

std::string BuildProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context) {
    return RunProjectCommandAndCheck(build_dir / std::string(output_name),
                                     BuildProjectCommandArgs(mc_path,
                                                             "build",
                                                             project_path,
                                                             build_dir,
                                                             target_name,
                                                             std::nullopt),
                                     context,
                                     ProjectCommandExpectation::failure,
                                     ProjectCommandOutputMode::capture);
}

std::string CheckProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context) {
    return RunProjectCommandAndCheck(build_dir / std::string(output_name),
                                     BuildProjectCommandArgs(mc_path,
                                                             "check",
                                                             project_path,
                                                             build_dir,
                                                             target_name,
                                                             std::nullopt),
                                     context,
                                     ProjectCommandExpectation::failure,
                                     ProjectCommandOutputMode::capture);
}

void ExpectExitCodeAtLeast(const mc::test_support::CommandOutcome& outcome,
                          int minimum_exit_code,
                          std::string_view output,
                          const std::string& context) {
    if (!outcome.exited) {
        std::string message = context + ": process did not exit";
        if (!output.empty()) {
            message += "\n";
            message += output;
        }
        Fail(message);
    }
    if (outcome.exit_code < minimum_exit_code) {
        std::string message = context + ": expected exit code >= " + std::to_string(minimum_exit_code) +
                              ", got " + std::to_string(outcome.exit_code);
        if (!output.empty()) {
            message += "\n";
            message += output;
        }
        Fail(message);
    }
}

std::string CheckProjectTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context) {
    return RunProjectCommandAndCheck(build_dir / std::string(output_name),
                                     BuildProjectCommandArgs(mc_path,
                                                             "check",
                                                             project_path,
                                                             build_dir,
                                                             target_name,
                                                             std::nullopt),
                                     context,
                                     ProjectCommandExpectation::success,
                                     ProjectCommandOutputMode::capture);
}

std::string RunProjectTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                             const std::filesystem::path& project_path,
                                             const std::filesystem::path& build_dir,
                                             std::string_view target_name,
                                             const std::filesystem::path& sample_path,
                                             std::string_view output_name,
                                             const std::string& context) {
    return RunProjectCommandAndCheck(build_dir / std::string(output_name),
                                     BuildProjectCommandArgs(mc_path,
                                                             "run",
                                                             project_path,
                                                             build_dir,
                                                             target_name,
                                                             sample_path),
                                     context,
                                     ProjectCommandExpectation::success,
                                     ProjectCommandOutputMode::capture);
}

std::string RunProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                             const std::filesystem::path& project_path,
                                             const std::filesystem::path& build_dir,
                                             std::string_view target_name,
                                             std::string_view output_name,
                                             const std::string& context) {
    return RunProjectCommandAndCheck(build_dir / std::string(output_name),
                                     BuildProjectCommandArgs(mc_path,
                                                             "run",
                                                             project_path,
                                                             build_dir,
                                                             target_name,
                                                             std::nullopt),
                                     context,
                                     ProjectCommandExpectation::failure,
                                     ProjectCommandOutputMode::capture);
}

std::string RunProjectTestTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                                 const std::filesystem::path& project_path,
                                                 const std::filesystem::path& build_dir,
                                                 std::string_view target_name,
                                                 std::string_view output_name,
                                                 const std::string& context) {
    return RunProjectCommandAndCheck(build_dir / std::string(output_name),
                                     BuildProjectCommandArgs(mc_path,
                                                             "test",
                                                             project_path,
                                                             build_dir,
                                                             target_name,
                                                             std::nullopt),
                                     context,
                                     ProjectCommandExpectation::success,
                                     ProjectCommandOutputMode::capture);
}

void ExpectMirFirstMatchProjection(std::string_view mir_text,
                                   std::initializer_list<std::string_view> selectors,
                                   std::string_view expected_projection,
                                   const std::string& context) {
    ExpectMirFirstMatchProjectionSpan(mir_text,
                                      std::span<const std::string_view>(selectors.begin(), selectors.size()),
                                      expected_projection,
                                      context);
}

void ExpectMirFirstMatchProjectionSpan(std::string_view mir_text,
                                       std::span<const std::string_view> selectors,
                                       std::string_view expected_projection,
                                       const std::string& context) {
    const auto lines = SplitLines(mir_text);

    std::string actual_projection;
    bool first_line = true;
    for (const auto selector : selectors) {
        bool found = false;
        for (const auto& line : lines) {
            if (line.find(selector) == std::string::npos) {
                continue;
            }
            if (!first_line) {
                actual_projection += '\n';
            }
            actual_projection += line;
            first_line = false;
            found = true;
            break;
        }
        if (!found) {
            Fail(context + " missing MIR projection selector: " + std::string(selector));
        }
    }

    const auto normalized_expected = NormalizeProjectionText(expected_projection);
    const auto normalized_actual = NormalizeProjectionText(actual_projection);
    if (normalized_actual != normalized_expected) {
        Fail(context + " projection mismatch:\nexpected:\n" + normalized_expected + "\nactual:\n" + normalized_actual);
    }
}

void ExpectTextContainsLines(std::string_view actual_text,
                             std::string_view expected_lines,
                             const std::string& context) {
    for (auto expected_line : SplitLines(expected_lines)) {
        if (!expected_line.empty() && expected_line.back() == '\r') {
            expected_line.pop_back();
        }
        if (expected_line.empty()) {
            continue;
        }
        if (actual_text.find(expected_line) == std::string::npos) {
            Fail(context + " missing expected text line: " + expected_line);
        }
    }
}

void ExpectTextContainsLinesFile(std::string_view actual_text,
                                 const std::filesystem::path& expected_lines_path,
                                 const std::string& context) {
    ExpectTextContainsLines(actual_text,
                            ReadFile(expected_lines_path),
                            context + " golden=" + expected_lines_path.generic_string());
}

void ExpectMirFirstMatchProjectionFile(std::string_view mir_text,
                                       std::initializer_list<std::string_view> selectors,
                                       const std::filesystem::path& expected_projection_path,
                                       const std::string& context) {
    ExpectMirFirstMatchProjectionFileSpan(mir_text,
                                          std::span<const std::string_view>(selectors.begin(), selectors.size()),
                                          expected_projection_path,
                                          context);
}

void ExpectMirFirstMatchProjectionFileSpan(std::string_view mir_text,
                                           std::span<const std::string_view> selectors,
                                           const std::filesystem::path& expected_projection_path,
                                           const std::string& context) {
    ExpectMirFirstMatchProjectionSpan(mir_text,
                                      selectors,
                                      ReadFile(expected_projection_path),
                                      context + " golden=" + expected_projection_path.generic_string());
}

}  // namespace mc::tool_tests
