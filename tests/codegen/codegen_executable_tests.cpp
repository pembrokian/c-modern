#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <sys/wait.h>

#include "compiler/support/dump_paths.h"

namespace {

void Fail(const std::string& message) {
    std::cerr << "test failure: " << message << '\n';
    std::exit(1);
}

std::string QuoteShellArg(std::string_view argument) {
    std::string quoted;
    quoted.reserve(argument.size() + 2);
    quoted.push_back('\'');
    for (const char ch : argument) {
        if (ch == '\'') {
            quoted += "'\\''";
            continue;
        }
        quoted.push_back(ch);
    }
    quoted.push_back('\'');
    return quoted;
}

std::string JoinCommand(const std::vector<std::string>& args) {
    std::string command;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (index > 0) {
            command.push_back(' ');
        }
        command += QuoteShellArg(args[index]);
    }
    return command;
}

int RunCommand(const std::vector<std::string>& args,
               const std::string& context) {
    const std::string command = JoinCommand(args);
    const int raw_status = std::system(command.c_str());
#ifdef WIFEXITED
    if (WIFEXITED(raw_status)) {
        return WEXITSTATUS(raw_status);
    }
#endif
    Fail(context + ": command failed to execute cleanly: " + command);
    return raw_status;
}

void ExpectCommandSuccess(const std::vector<std::string>& args,
                          const std::string& context) {
    const int exit_code = RunCommand(args, context);
    if (exit_code != 0) {
        Fail(context + ": command returned non-zero exit code " + std::to_string(exit_code));
    }
}

void ExpectExecutableExit(const std::filesystem::path& executable,
                          const std::vector<std::string>& args,
                          int expected_exit_code,
                          const std::string& context) {
    std::vector<std::string> command;
    command.push_back(executable.generic_string());
    command.insert(command.end(), args.begin(), args.end());
    const int exit_code = RunCommand(command, context);
    if (exit_code != expected_exit_code) {
        Fail(context + ": expected exit code " + std::to_string(expected_exit_code) + ", got " +
             std::to_string(exit_code));
    }
}

void WriteFile(const std::filesystem::path& path,
               std::string_view contents) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << contents;
    if (!output) {
        Fail("unable to write test source: " + path.generic_string());
    }
}

void RunBuiltFixture(const std::filesystem::path& mc_path,
                     const std::filesystem::path& source_path,
                     const std::filesystem::path& build_dir,
                     int expected_exit_code,
                     const std::vector<std::string>& run_args,
                     bool dump_backend = false) {
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    std::vector<std::string> build_command = {
        mc_path.generic_string(),
        "build",
        source_path.generic_string(),
        "--build-dir",
        build_dir.generic_string(),
    };
    if (dump_backend) {
        build_command.push_back("--dump-backend");
    }

    ExpectCommandSuccess(build_command, "mc build " + source_path.generic_string());

    const auto artifacts = mc::support::ComputeBuildArtifactTargets(source_path, build_dir);
    if (!std::filesystem::exists(artifacts.llvm_ir)) {
        Fail("expected LLVM IR artifact: " + artifacts.llvm_ir.generic_string());
    }
    if (!std::filesystem::exists(artifacts.object)) {
        Fail("expected object artifact: " + artifacts.object.generic_string());
    }
    if (!std::filesystem::exists(artifacts.executable)) {
        Fail("expected executable artifact: " + artifacts.executable.generic_string());
    }

    ExpectExecutableExit(artifacts.executable,
                         run_args,
                         expected_exit_code,
                         "run built executable " + artifacts.executable.generic_string());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        Fail("expected source root and binary root arguments");
    }

    const std::filesystem::path source_root = argv[1];
    const std::filesystem::path binary_root = argv[2];
    const std::filesystem::path mc_path = binary_root / "mc";
    const std::filesystem::path work_root = binary_root / "stage5_exec_tests";

    RunBuiltFixture(mc_path,
                    source_root / "tests/codegen/smoke_return_zero.mc",
                    work_root / "smoke_return_zero",
                    0,
                    {},
                    true);

    RunBuiltFixture(mc_path,
                    source_root / "tests/codegen/direct_call_add.mc",
                    work_root / "direct_call_add",
                    3,
                    {});

    const std::filesystem::path hosted_args_source = work_root / "hosted_main_args.mc";
    WriteFile(hosted_args_source,
              "func main(args: Slice<cstr>) i32 {\n"
              "    return 0\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    hosted_args_source,
                    work_root / "hosted_main_args_build",
                    0,
                    {"alpha", "beta"});

    return 0;
}