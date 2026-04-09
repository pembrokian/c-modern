#include "tests/tool/tool_suite_common.h"

#include <vector>

#include "tests/support/process_utils.h"

namespace mc::tool_tests {

using mc::test_support::Fail;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;

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

}  // namespace mc::tool_tests
