#include "tests/tool/tool_suite_common.h"

#include <cstdlib>
#include <sstream>
#include <vector>

#include "compiler/support/target.h"
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

std::filesystem::path ResolveFreestandingKernelGoldenPath(const std::filesystem::path& source_root,
                                                          std::string_view file_name) {
    return source_root / "tests" / "tool" / "freestanding" / "kernel" / std::string(file_name);
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

FreestandingKernelRunArtifacts BuildAndRunFreestandingKernelTarget(const std::filesystem::path& mc_path,
                                                                  const std::filesystem::path& project_path,
                                                                  const std::filesystem::path& main_source_path,
                                                                  const std::filesystem::path& build_dir,
                                                                  std::string_view output_stem,
                                                                  const std::string& build_context,
                                                                  const std::string& run_context) {
    MaybeCleanBuildDir(build_dir);

    const std::string output_stem_text(output_stem);
    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / (output_stem_text + "_build_output.txt"),
                                                                 build_context);
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail(build_context + " should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / (output_stem_text + "_run_output.txt"),
                                                             run_context);
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail(run_context + " should succeed:\n" + run_output);
    }

    return {
        .build_dir = build_dir,
        .build_targets = build_targets,
        .dump_targets = dump_targets,
        .run_output = run_output,
    };
}

void CompileBootstrapCObjectAndExpectSuccess(const std::filesystem::path& source_path,
                                             const std::filesystem::path& object_path,
                                             const std::filesystem::path& output_path,
                                             const std::string& context) {
    const auto [outcome, output] = RunCommandCapture({"xcrun",
                                                      "clang",
                                                      "-target",
                                                      std::string(mc::kBootstrapTriple),
                                                      "-c",
                                                      source_path.generic_string(),
                                                      "-o",
                                                      object_path.generic_string()},
                                                     output_path,
                                                     context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should pass:\n" + output);
    }
}

void LinkBootstrapObjectsAndExpectSuccess(const std::vector<std::filesystem::path>& object_paths,
                                          const std::filesystem::path& executable_path,
                                          const std::filesystem::path& output_path,
                                          const std::string& context) {
    std::vector<std::string> args{"xcrun", "clang", "-target", std::string(mc::kBootstrapTriple)};
    for (const auto& object_path : object_paths) {
        args.push_back(object_path.generic_string());
    }
    args.push_back("-o");
    args.push_back(executable_path.generic_string());

    const auto [outcome, output] = RunCommandCapture(args, output_path, context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should succeed:\n" + output);
    }
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

void ExpectFreestandingKernelPhaseFromArtifacts(const std::filesystem::path& source_root,
                                                const std::filesystem::path& object_dir,
                                                std::string_view run_output,
                                                std::string_view mir_text,
                                                const FreestandingKernelPhaseCheck& phase_check) {
    const std::string label(phase_check.label);
    ExpectTextContainsLinesFile(run_output,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    phase_check.expected_run_lines_file),
                                label + " run");

    for (const auto object_name : phase_check.required_object_files) {
        if (!std::filesystem::exists(object_dir / std::string(object_name))) {
            Fail(label + " missing required object artifact: " + std::string(object_name));
        }
    }

    ExpectMirFirstMatchProjectionFileSpan(mir_text,
                                          phase_check.mir_selectors,
                                          ResolveFreestandingKernelGoldenPath(source_root,
                                                                              phase_check.expected_mir_projection_file),
                                          label + " MIR");
}

}  // namespace mc::tool_tests
