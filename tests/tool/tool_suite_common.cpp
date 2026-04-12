#include "tests/tool/tool_suite_common.h"

#include <cstdlib>
#include <sstream>
#include <vector>

#include "tests/support/process_utils.h"

namespace mc::tool_tests {

using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;

namespace {

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

}  // namespace

FreestandingKernelCommonPaths MakeFreestandingKernelCommonPaths(const std::filesystem::path& source_root) {
    return {
        .project_path = source_root / "kernel" / "build.toml",
        .main_source_path = source_root / "kernel" / "src" / "main.mc",
        .roadmap_path = source_root / "docs" / "plan" / "admin" / "canopus_post_phase109_speculative_roadmap.txt",
        .kernel_readme_path = source_root / "kernel" / "README.md",
        .repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md",
        .freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md",
        .decision_log_path = source_root / "docs" / "plan" / "decision_log.txt",
        .backlog_path = source_root / "docs" / "plan" / "backlog.txt",
    };
}

std::filesystem::path ResolveCanopusRoadmapPath(const std::filesystem::path& source_root,
                                                int phase_number) {
    const std::filesystem::path admin_root = source_root / "docs" / "plan" / "admin";
    if (phase_number <= 123) {
        return admin_root / "specuative_roadmap_post_phase109.txt";
    }
    if (phase_number <= 137) {
        return admin_root / "specuative_roadmap_post_phase123.txt";
    }
    if (phase_number <= 145) {
        return admin_root / "canopus_post_phase109_speculative_roadmap.txt";
    }
    return admin_root / "speculative_roadmap_post_phase145.txt";
}

void MaybeCleanBuildDir(const std::filesystem::path& build_dir) {
    const char* force_clean_env = std::getenv("MC_TEST_FORCE_CLEAN_BUILDS");
    if (force_clean_env == nullptr || std::string_view(force_clean_env) == "0") {
        return;
    }
    std::filesystem::remove_all(build_dir);
}

std::filesystem::path ResolvePlanDocPath(const std::filesystem::path& source_root,
                                         std::string_view file_name) {
    const std::filesystem::path plan_root = source_root / "docs" / "plan";
    const std::filesystem::path active_path = plan_root / "active" / std::string(file_name);
    if (std::filesystem::exists(active_path)) {
        return active_path;
    }

    const std::filesystem::path root_path = plan_root / std::string(file_name);
    if (std::filesystem::exists(root_path)) {
        return root_path;
    }

    const std::filesystem::path archive_path = plan_root / "archive" / std::string(file_name);
    if (std::filesystem::exists(archive_path)) {
        return archive_path;
    }

    return active_path;
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
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should pass:\n" + output);
    }
    return output;
}

void BuildProjectTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                        const std::filesystem::path& project_path,
                                        const std::filesystem::path& build_dir,
                                        std::string_view target_name,
                                        std::string_view output_name,
                                        const std::string& context) {
    std::vector<std::string> args {
        mc_path.generic_string(),
        "build",
        "--project",
        project_path.generic_string(),
    };
    if (!target_name.empty()) {
        args.push_back("--target");
        args.push_back(std::string(target_name));
    }
    args.push_back("--build-dir");
    args.push_back(build_dir.generic_string());

    const auto [outcome, output] = RunCommandCapture(args,
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should pass:\n" + output);
    }
}

std::string BuildProjectTargetAndCapture(const std::filesystem::path& mc_path,
                                        const std::filesystem::path& project_path,
                                        const std::filesystem::path& build_dir,
                                        std::string_view target_name,
                                        std::string_view output_name,
                                        const std::string& context) {
    std::vector<std::string> args {
        mc_path.generic_string(),
        "build",
        "--project",
        project_path.generic_string(),
    };
    if (!target_name.empty()) {
        args.push_back("--target");
        args.push_back(std::string(target_name));
    }
    args.push_back("--build-dir");
    args.push_back(build_dir.generic_string());

    const auto [outcome, output] = RunCommandCapture(args,
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should pass:\n" + output);
    }
    return output;
}

std::string BuildProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context) {
    std::vector<std::string> args {
        mc_path.generic_string(),
        "build",
        "--project",
        project_path.generic_string(),
    };
    if (!target_name.empty()) {
        args.push_back("--target");
        args.push_back(std::string(target_name));
    }
    args.push_back("--build-dir");
    args.push_back(build_dir.generic_string());

    const auto [outcome, output] = RunCommandCapture(args,
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail(context + " should fail:\n" + output);
    }
    return output;
}

std::string CheckProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context) {
    std::vector<std::string> args {
        mc_path.generic_string(),
        "check",
        "--project",
        project_path.generic_string(),
    };
    if (!target_name.empty()) {
        args.push_back("--target");
        args.push_back(std::string(target_name));
    }
    args.push_back("--build-dir");
    args.push_back(build_dir.generic_string());

    const auto [outcome, output] = RunCommandCapture(args,
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail(context + " should fail:\n" + output);
    }
    return output;
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
    std::vector<std::string> args {
        mc_path.generic_string(),
        "check",
        "--project",
        project_path.generic_string(),
    };
    if (!target_name.empty()) {
        args.push_back("--target");
        args.push_back(std::string(target_name));
    }
    args.push_back("--build-dir");
    args.push_back(build_dir.generic_string());

    const auto [outcome, output] = RunCommandCapture(args,
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should pass:\n" + output);
    }
    return output;
}

std::string RunProjectTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                             const std::filesystem::path& project_path,
                                             const std::filesystem::path& build_dir,
                                             std::string_view target_name,
                                             const std::filesystem::path& sample_path,
                                             std::string_view output_name,
                                             const std::string& context) {
    std::vector<std::string> args {
        mc_path.generic_string(),
        "run",
        "--project",
        project_path.generic_string(),
    };
    if (!target_name.empty()) {
        args.push_back("--target");
        args.push_back(std::string(target_name));
    }
    args.push_back("--build-dir");
    args.push_back(build_dir.generic_string());
    args.push_back("--");
    args.push_back(sample_path.generic_string());

    const auto [outcome, output] = RunCommandCapture(args,
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should pass:\n" + output);
    }
    return output;
}

std::string RunProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                             const std::filesystem::path& project_path,
                                             const std::filesystem::path& build_dir,
                                             std::string_view target_name,
                                             std::string_view output_name,
                                             const std::string& context) {
    std::vector<std::string> args {
        mc_path.generic_string(),
        "run",
        "--project",
        project_path.generic_string(),
    };
    if (!target_name.empty()) {
        args.push_back("--target");
        args.push_back(std::string(target_name));
    }
    args.push_back("--build-dir");
    args.push_back(build_dir.generic_string());

    const auto [outcome, output] = RunCommandCapture(args,
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail(context + " should fail:\n" + output);
    }
    return output;
}

std::string RunProjectTestTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                                 const std::filesystem::path& project_path,
                                                 const std::filesystem::path& build_dir,
                                                 std::string_view target_name,
                                                 std::string_view output_name,
                                                 const std::string& context) {
    std::vector<std::string> args {
        mc_path.generic_string(),
        "test",
        "--project",
        project_path.generic_string(),
    };
    if (!target_name.empty()) {
        args.push_back("--target");
        args.push_back(std::string(target_name));
    }
    args.push_back("--build-dir");
    args.push_back(build_dir.generic_string());

    const auto [outcome, output] = RunCommandCapture(args,
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should pass:\n" + output);
    }
    return output;
}

void ExpectMirFirstMatchProjection(std::string_view mir_text,
                                   std::initializer_list<std::string_view> selectors,
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

void ExpectMirFirstMatchProjectionFile(std::string_view mir_text,
                                       std::initializer_list<std::string_view> selectors,
                                       const std::filesystem::path& expected_projection_path,
                                       const std::string& context) {
    ExpectMirFirstMatchProjection(mir_text,
                                  selectors,
                                  ReadFile(expected_projection_path),
                                  context + " golden=" + expected_projection_path.generic_string());
}

}  // namespace mc::tool_tests
