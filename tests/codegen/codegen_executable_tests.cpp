#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <sys/wait.h>

#include "compiler/support/dump_paths.h"

namespace {

struct CommandOutcome {
    bool exited = false;
    int exit_code = -1;
    bool signaled = false;
    int signal_number = -1;
};

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

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input) {
        Fail("unable to read file: " + path.generic_string());
    }
    return buffer.str();
}

CommandOutcome RunCommand(const std::vector<std::string>& args,
                          const std::string& context) {
    const std::string command = JoinCommand(args);
    const int raw_status = std::system(command.c_str());
#ifdef WIFEXITED
    if (WIFEXITED(raw_status)) {
        return {
            .exited = true,
            .exit_code = WEXITSTATUS(raw_status),
        };
    }
#endif
#ifdef WIFSIGNALED
    if (WIFSIGNALED(raw_status)) {
        return {
            .signaled = true,
            .signal_number = WTERMSIG(raw_status),
        };
    }
#endif
    Fail(context + ": command failed to execute cleanly: " + command + " (raw_status=" +
         std::to_string(raw_status) + ")");
    return {};
}

std::pair<CommandOutcome, std::string> RunCommandCapture(const std::vector<std::string>& args,
                                                         const std::filesystem::path& output_path,
                                                         const std::string& context) {
    const std::string command = JoinCommand(args) + " >" + QuoteShellArg(output_path.generic_string()) + " 2>&1";
    const int raw_status = std::system(command.c_str());
    const std::string output = ReadFile(output_path);
#ifdef WIFEXITED
    if (WIFEXITED(raw_status)) {
        return {{
                    .exited = true,
                    .exit_code = WEXITSTATUS(raw_status),
                },
                output};
    }
#endif
#ifdef WIFSIGNALED
    if (WIFSIGNALED(raw_status)) {
        return {{
                    .signaled = true,
                    .signal_number = WTERMSIG(raw_status),
                },
                output};
    }
#endif
    Fail(context + ": command failed to execute cleanly: " + command + " (raw_status=" +
         std::to_string(raw_status) + ")");
    return {};
}

void ExpectCommandSuccess(const std::vector<std::string>& args,
                          const std::string& context) {
    const CommandOutcome outcome = RunCommand(args, context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + ": command returned non-zero exit code " + std::to_string(outcome.exit_code));
    }
}

void ExpectExecutableExit(const std::filesystem::path& executable,
                          const std::vector<std::string>& args,
                          int expected_exit_code,
                          const std::string& context) {
    std::vector<std::string> command;
    command.push_back(executable.generic_string());
    command.insert(command.end(), args.begin(), args.end());
    const CommandOutcome outcome = RunCommand(command, context);
    if (!outcome.exited || outcome.exit_code != expected_exit_code) {
        Fail(context + ": expected exit code " + std::to_string(expected_exit_code) + ", got " +
             std::to_string(outcome.exit_code));
    }
}

void ExpectExecutableFailure(const std::filesystem::path& executable,
                             const std::vector<std::string>& args,
                             const std::string& context) {
    std::vector<std::string> command;
    command.push_back(executable.generic_string());
    command.insert(command.end(), args.begin(), args.end());
    const CommandOutcome outcome = RunCommand(command, context);
    if ((!outcome.exited && !outcome.signaled) || (outcome.exited && outcome.exit_code == 0)) {
        Fail(context + ": expected non-zero exit code from checked runtime failure");
    }
}

void ExpectExecutableOutput(const std::filesystem::path& executable,
                            const std::vector<std::string>& args,
                            int expected_exit_code,
                            std::string_view expected_output,
                            const std::filesystem::path& output_path,
                            const std::string& context) {
    std::vector<std::string> command;
    command.push_back(executable.generic_string());
    command.insert(command.end(), args.begin(), args.end());
    const auto [outcome, output] = RunCommandCapture(command, output_path, context);
    if (!outcome.exited || outcome.exit_code != expected_exit_code) {
        Fail(context + ": expected exit code " + std::to_string(expected_exit_code) + ", got " +
             std::to_string(outcome.exit_code));
    }
    if (output != expected_output) {
        Fail(context + ": expected output '" + std::string(expected_output) + "', got '" + output + "'");
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

void RunBuiltFailureFixture(const std::filesystem::path& mc_path,
                            const std::filesystem::path& source_path,
                            const std::filesystem::path& build_dir,
                            const std::vector<std::string>& run_args,
                            const std::vector<std::string>& required_ir_snippets) {
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    ExpectCommandSuccess({mc_path.generic_string(),
                          "build",
                          source_path.generic_string(),
                          "--build-dir",
                          build_dir.generic_string()},
                         "mc build " + source_path.generic_string());

    const auto artifacts = mc::support::ComputeBuildArtifactTargets(source_path, build_dir);
    if (!std::filesystem::exists(artifacts.llvm_ir) ||
        !std::filesystem::exists(artifacts.object) ||
        !std::filesystem::exists(artifacts.executable)) {
        Fail("expected build artifacts for failure fixture: " + source_path.generic_string());
    }

    const std::string llvm_ir = ReadFile(artifacts.llvm_ir);
    for (const auto& snippet : required_ir_snippets) {
        if (llvm_ir.find(snippet) == std::string::npos) {
            Fail("expected LLVM IR snippet '" + snippet + "' in " + artifacts.llvm_ir.generic_string());
        }
    }

    ExpectExecutableFailure(artifacts.executable,
                            run_args,
                            "run built executable " + artifacts.executable.generic_string());
}

void RunBuildFailureFixture(const std::filesystem::path& mc_path,
                           const std::filesystem::path& source_path,
                           const std::filesystem::path& build_dir,
                           const std::string& required_output_snippet) {
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const std::filesystem::path output_path = build_dir / "build_failure_output.txt";
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      source_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     output_path,
                                                     "mc build " + source_path.generic_string());

    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("expected build failure for fixture: " + source_path.generic_string());
    }
    if (output.find(required_output_snippet) == std::string::npos) {
        Fail("expected build failure output snippet '" + required_output_snippet + "' in " + output_path.generic_string());
    }
}

void RunBuiltOutputFixture(const std::filesystem::path& mc_path,
                           const std::filesystem::path& source_path,
                           const std::filesystem::path& build_dir,
                           const std::vector<std::string>& run_args,
                           int expected_exit_code,
                           std::string_view expected_output) {
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    ExpectCommandSuccess({mc_path.generic_string(),
                          "build",
                          source_path.generic_string(),
                          "--build-dir",
                          build_dir.generic_string()},
                         "mc build " + source_path.generic_string());

    const auto artifacts = mc::support::ComputeBuildArtifactTargets(source_path, build_dir);
    ExpectExecutableOutput(artifacts.executable,
                           run_args,
                           expected_exit_code,
                           expected_output,
                           build_dir / "captured_output.txt",
                           "run built executable " + artifacts.executable.generic_string());
}

void RunBuiltProjectFixture(const std::filesystem::path& mc_path,
                            const std::filesystem::path& project_path,
                            const std::filesystem::path& root_source_path,
                            const std::filesystem::path& build_dir,
                            int expected_exit_code,
                            const std::vector<std::string>& run_args) {
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    ExpectCommandSuccess({mc_path.generic_string(),
                          "build",
                          "--project",
                          project_path.generic_string(),
                          "--build-dir",
                          build_dir.generic_string()},
                         "mc build --project " + project_path.generic_string());

    const auto artifacts = mc::support::ComputeBuildArtifactTargets(root_source_path, build_dir);
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

    const std::filesystem::path global_source = work_root / "global_counter.mc";
    WriteFile(global_source,
              "var counter: i32 = 7\n"
              "\n"
              "func main() i32 {\n"
              "    counter = counter + 5\n"
              "    return counter\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    global_source,
                    work_root / "global_counter_build",
                    12,
                    {});

    const std::filesystem::path aggregate_source = work_root / "aggregate_pair.mc";
    WriteFile(aggregate_source,
              "struct Pair {\n"
              "    left: i32,\n"
              "    right: i32,\n"
              "}\n"
              "\n"
              "func make_pair(left: i32, right: i32) Pair {\n"
              "    return Pair{ left: left, right: right }\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    pair: Pair = make_pair(6, 9)\n"
              "    return pair.left + pair.right\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    aggregate_source,
                    work_root / "aggregate_pair_build",
                    15,
                    {});

    const std::filesystem::path padded_struct_source = work_root / "padded_header.mc";
    WriteFile(padded_struct_source,
              "struct Header {\n"
              "    tag: u8,\n"
              "    value: i32,\n"
              "    tail: u16,\n"
              "}\n"
              "\n"
              "func make_header() Header {\n"
              "    return Header{ tag: 1, value: 9, tail: 2 }\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    header: Header = make_header()\n"
              "    if header.tag != 1 {\n"
              "        return 1\n"
              "    }\n"
              "    if header.value != 9 {\n"
              "        return 2\n"
              "    }\n"
              "    if header.tail != 2 {\n"
              "        return 3\n"
              "    }\n"
              "    return 0\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    padded_struct_source,
                    work_root / "padded_header_build",
                    0,
                    {});

    const std::filesystem::path packed_struct_source = work_root / "packed_header.mc";
    WriteFile(packed_struct_source,
              "@packed\n"
              "struct PackedHeader {\n"
              "    tag: u8,\n"
              "    value: i32,\n"
              "    tail: u16,\n"
              "}\n"
              "\n"
              "func make_header() PackedHeader {\n"
              "    return PackedHeader{ tag: 1, value: 9, tail: 2 }\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    header: PackedHeader = make_header()\n"
              "    if header.tag != 1 {\n"
              "        return 1\n"
              "    }\n"
              "    if header.value != 9 {\n"
              "        return 2\n"
              "    }\n"
              "    if header.tail != 2 {\n"
              "        return 3\n"
              "    }\n"
              "    return 0\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    packed_struct_source,
                    work_root / "packed_header_build",
                    0,
                    {});

    const std::filesystem::path distinct_source = work_root / "distinct_roundtrip.mc";
    WriteFile(distinct_source,
              "distinct UserId = i32\n"
              "\n"
              "func promote(raw: i32) UserId {\n"
              "    return (UserId)(raw)\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    value: UserId = promote(11)\n"
              "    return (i32)(value)\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    distinct_source,
                    work_root / "distinct_roundtrip_build",
                    11,
                    {});

    const std::filesystem::path checked_math_source = work_root / "checked_math_ok.mc";
    WriteFile(checked_math_source,
              "func main() i32 {\n"
              "    value: i32 = 12\n"
              "    denom: i32 = 3\n"
              "    count: i32 = 1\n"
              "    return (value / denom) << count\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    checked_math_source,
                    work_root / "checked_math_ok_build",
                    8,
                    {});

    const std::filesystem::path checked_unsigned_div_source = work_root / "checked_unsigned_div_ok.mc";
    WriteFile(checked_unsigned_div_source,
              "func divide(value: u32, denom: u32) u32 {\n"
              "    return value / denom\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    if divide(9, 3) == 3 {\n"
              "        return 0\n"
              "    }\n"
              "    return 1\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    checked_unsigned_div_source,
                    work_root / "checked_unsigned_div_ok_build",
                    0,
                    {});

    const std::filesystem::path checked_remainder_i16_source = work_root / "checked_remainder_i16_ok.mc";
    WriteFile(checked_remainder_i16_source,
              "func remainder(value: i16, denom: i16) i16 {\n"
              "    return value % denom\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    if remainder(17, 5) == 2 {\n"
              "        return 0\n"
              "    }\n"
              "    return 1\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    checked_remainder_i16_source,
                    work_root / "checked_remainder_i16_ok_build",
                    0,
                    {});

    const std::filesystem::path checked_shift_i64_source = work_root / "checked_shift_i64_ok.mc";
    WriteFile(checked_shift_i64_source,
              "func shift(value: i64, count: i64) i64 {\n"
              "    return value >> count\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    if shift(32, 3) == 4 {\n"
              "        return 0\n"
              "    }\n"
              "    return 1\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    checked_shift_i64_source,
                    work_root / "checked_shift_i64_ok_build",
                    0,
                    {});

    const std::filesystem::path checked_shift_i8_source = work_root / "checked_shift_i8_ok.mc";
    WriteFile(checked_shift_i8_source,
              "func shift(value: i8, count: i8) i8 {\n"
              "    return value << count\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    if shift(3, 1) == 6 {\n"
              "        return 0\n"
              "    }\n"
              "    return 1\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    checked_shift_i8_source,
                    work_root / "checked_shift_i8_ok_build",
                    0,
                    {});

    const std::filesystem::path checked_shift_i16_source = work_root / "checked_shift_i16_ok.mc";
    WriteFile(checked_shift_i16_source,
              "func shift(value: i16, count: i16) i16 {\n"
              "    return value << count\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    if shift(2, 3) == 16 {\n"
              "        return 0\n"
              "    }\n"
              "    return 1\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    checked_shift_i16_source,
                    work_root / "checked_shift_i16_ok_build",
                    0,
                    {});

    const std::filesystem::path div_zero_source = work_root / "checked_div_zero.mc";
    WriteFile(div_zero_source,
              "func main() i32 {\n"
              "    value: i32 = 12\n"
              "    denom: i32 = 0\n"
              "    return value / denom\n"
              "}\n");
    RunBuiltFailureFixture(mc_path,
                           div_zero_source,
                           work_root / "checked_div_zero_build",
                           {},
                           {"call void @__mc_check_div_i32"});

    const std::filesystem::path div_zero_i64_source = work_root / "checked_div_zero_i64.mc";
    WriteFile(div_zero_i64_source,
              "func explode(value: i64, denom: i64) i64 {\n"
              "    return value / denom\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    doomed: i64 = explode(12, 0)\n"
              "    return 0\n"
              "}\n");
    RunBuiltFailureFixture(mc_path,
                           div_zero_i64_source,
                           work_root / "checked_div_zero_i64_build",
                           {},
                           {"call void @__mc_check_div_i64"});

    const std::filesystem::path div_zero_u8_source = work_root / "checked_div_zero_u8.mc";
    WriteFile(div_zero_u8_source,
              "func explode(value: u8, denom: u8) u8 {\n"
              "    return value / denom\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    doomed: u8 = explode(12, 0)\n"
              "    return 0\n"
              "}\n");
    RunBuiltFailureFixture(mc_path,
                           div_zero_u8_source,
                           work_root / "checked_div_zero_u8_build",
                           {},
                           {"call void @__mc_check_div_i8"});

    const std::filesystem::path shift_oob_source = work_root / "checked_shift_oob.mc";
    WriteFile(shift_oob_source,
              "func main() i32 {\n"
              "    value: i32 = 1\n"
              "    count: i32 = 32\n"
              "    return value << count\n"
              "}\n");
    RunBuiltFailureFixture(mc_path,
                           shift_oob_source,
                           work_root / "checked_shift_oob_build",
                           {},
                           {"call void @__mc_check_shift_32"});

    const std::filesystem::path shift_oob_u16_source = work_root / "checked_shift_oob_u16.mc";
    WriteFile(shift_oob_u16_source,
              "func explode(value: u16, count: u16) u16 {\n"
              "    return value << count\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    doomed: u16 = explode(1, 16)\n"
              "    return 0\n"
              "}\n");
    RunBuiltFailureFixture(mc_path,
                           shift_oob_u16_source,
                           work_root / "checked_shift_oob_u16_build",
                           {},
                           {"call void @__mc_check_shift_16"});

    const std::filesystem::path shift_oob_u8_source = work_root / "checked_shift_oob_u8.mc";
    WriteFile(shift_oob_u8_source,
              "func explode(value: u8, count: u8) u8 {\n"
              "    return value << count\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    doomed: u8 = explode(1, 8)\n"
              "    return 0\n"
              "}\n");
    RunBuiltFailureFixture(mc_path,
                           shift_oob_u8_source,
                           work_root / "checked_shift_oob_u8_build",
                           {},
                           {"call void @__mc_check_shift_8"});

    const std::filesystem::path low_level_supported_source = work_root / "low_level_supported.mc";
    WriteFile(low_level_supported_source,
              "enum MemoryOrder {\n"
              "    Relaxed,\n"
              "    Acquire,\n"
              "    Release,\n"
              "}\n"
              "\n"
              "struct Atomic<T> {}\n"
              "\n"
              "func volatile_load(ptr: *i32) i32 {\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func volatile_store(ptr: *i32, value: i32) {\n"
              "}\n"
              "\n"
              "func atomic_load(ptr: *Atomic<i32>, order: MemoryOrder) i32 {\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func atomic_store(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) {\n"
              "}\n"
              "\n"
              "func atomic_exchange(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) i32 {\n"
              "    return value\n"
              "}\n"
              "\n"
              "func atomic_fetch_add(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) i32 {\n"
              "    return value\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    raw: i32 = 4\n"
              "    stored: i32 = 9\n"
              "    exchange_value: i32 = 5\n"
              "    add_value: i32 = 2\n"
              "    volatile_store(&raw, stored)\n"
              "    loaded: i32 = volatile_load(&raw)\n"
              "    atom: *Atomic<i32> = (*Atomic<i32>)((uintptr)(&raw))\n"
              "    prior: i32 = atomic_exchange(atom, exchange_value, MemoryOrder.Acquire)\n"
              "    added: i32 = atomic_fetch_add(atom, add_value, MemoryOrder.Release)\n"
              "    latest: i32 = atomic_load(atom, MemoryOrder.Acquire)\n"
              "    atomic_store(atom, prior, MemoryOrder.Release)\n"
              "    return loaded + prior + added + latest + raw\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    low_level_supported_source,
                    work_root / "low_level_supported_build",
                    39,
                    {});

    const std::filesystem::path low_level_dynamic_order_source = work_root / "low_level_dynamic_order_fail.mc";
    WriteFile(low_level_dynamic_order_source,
              "enum MemoryOrder {\n"
              "    Relaxed,\n"
              "    Acquire,\n"
              "    Release,\n"
              "}\n"
              "\n"
              "struct Atomic<T> {}\n"
              "\n"
              "func atomic_load(ptr: *Atomic<i32>, order: MemoryOrder) i32 {\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func read(atom: *Atomic<i32>, order: MemoryOrder) i32 {\n"
              "    return atomic_load(atom, order)\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    raw: i32 = 0\n"
              "    atom: *Atomic<i32> = (*Atomic<i32>)((uintptr)(&raw))\n"
              "    return read(atom, MemoryOrder.Acquire)\n"
              "}\n");
    RunBuildFailureFixture(mc_path,
                           low_level_dynamic_order_source,
                           work_root / "low_level_dynamic_order_fail_build",
                           "requires constant MemoryOrder metadata for 'atomic_load'");

    const std::filesystem::path bounds_index_source = work_root / "bounds_index_oob.mc";
    WriteFile(bounds_index_source,
              "func main(args: Slice<cstr>) i32 {\n"
              "    value: cstr = args[1]\n"
              "    return 0\n"
              "}\n");
    RunBuiltFailureFixture(mc_path,
                           bounds_index_source,
                           work_root / "bounds_index_oob_build",
                           {},
                           {"call void @__mc_check_bounds_index"});

    const std::filesystem::path bounds_slice_source = work_root / "bounds_slice_oob.mc";
    WriteFile(bounds_slice_source,
              "func main(args: Slice<cstr>) i32 {\n"
              "    window: Slice<cstr> = args[0:2]\n"
              "    return 0\n"
              "}\n");
    RunBuiltFailureFixture(mc_path,
                           bounds_slice_source,
                           work_root / "bounds_slice_oob_build",
                           {},
                           {"call void @__mc_check_bounds_slice"});

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

    RunBuiltOutputFixture(mc_path,
                          source_root / "tests/stdlib/hello_stdout.mc",
                          work_root / "phase6_hello_stdout_build",
                          {},
                          0,
                          "hello, stdlib\n");

    RunBuiltOutputFixture(mc_path,
                          source_root / "tests/stdlib/write_stdout.mc",
                          work_root / "phase6_write_stdout_build",
                          {},
                          0,
                          "phase6 write");

    RunBuiltOutputFixture(mc_path,
                          source_root / "tests/stdlib/echo_arg.mc",
                          work_root / "phase6_echo_arg_build",
                          {"borrowed-arg"},
                          0,
                          "borrowed-arg\n");

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/allocator_buffer_len.mc",
                    work_root / "phase6_allocator_buffer_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/string_byte_len.mc",
                    work_root / "phase6_string_byte_len_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/utf8_byte_len.mc",
                    work_root / "phase6_utf8_byte_len_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/utf8_valid.mc",
                    work_root / "phase6_utf8_valid_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/utf8_invalid_bytes.mc",
                    work_root / "phase6_utf8_invalid_bytes_build",
                    0,
                    {});

    const std::filesystem::path phase6_file_input = work_root / "phase6_file_input.txt";
    WriteFile(phase6_file_input, "phase6\n");

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/file_size.mc",
                    work_root / "phase6_file_size_build",
                    0,
                    {phase6_file_input.generic_string()});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/file_read_len.mc",
                    work_root / "phase6_file_read_len_build",
                    7,
                    {phase6_file_input.generic_string()});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/file_read_default_len.mc",
                    work_root / "phase6_file_read_default_len_build",
                    0,
                    {phase6_file_input.generic_string()});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/slice_from_buffer_len.mc",
                    work_root / "phase6_slice_from_buffer_len_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/typed_buffer_roundtrip.mc",
                    work_root / "phase6_typed_buffer_roundtrip_build",
                    0,
                    {});

    RunBuiltOutputFixture(mc_path,
                          source_root / "examples/canonical/hello_stdout.mc",
                          work_root / "phase8_hello_stdout_build",
                          {},
                          0,
                          "hello, phase8\n");

    const std::filesystem::path phase8_file_input = work_root / "phase8_file_input.txt";
    WriteFile(phase8_file_input, "phase8\n");

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/read_entire_file.mc",
                    work_root / "phase8_read_entire_file_build",
                    7,
                    {phase8_file_input.generic_string()});

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/scoped_temp_buffer.mc",
                    work_root / "phase8_scoped_temp_buffer_build",
                    6,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/c_abi_boundary.mc",
                    work_root / "phase8_c_abi_boundary_build",
                    0,
                    {"phase8"});

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/config_parser.mc",
                    work_root / "phase8_config_parser_build",
                    0,
                    {});

    const std::filesystem::path imported_project_root = work_root / "imported_globals_project";
    WriteFile(imported_project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase11a-imported-globals\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(imported_project_root / "src/helper.mc",
              "export { Pair, make_pair, LIMIT, counter }\n"
              "\n"
              "struct Pair {\n"
              "    left: i32,\n"
              "    right: i32,\n"
              "}\n"
              "\n"
              "const LIMIT: i32 = 9\n"
              "var counter: i32 = 4\n"
              "\n"
              "func make_pair() Pair {\n"
              "    return Pair{ left: LIMIT, right: 5 }\n"
              "}\n");
    WriteFile(imported_project_root / "src/main.mc",
              "import helper\n"
              "\n"
              "func main() i32 {\n"
              "    pair: helper.Pair = helper.make_pair()\n"
              "    helper.counter = helper.counter + pair.right\n"
              "    return pair.left + helper.counter + helper.LIMIT\n"
              "}\n");
    RunBuiltProjectFixture(mc_path,
                           imported_project_root / "build.toml",
                           imported_project_root / "src/main.mc",
                           work_root / "imported_globals_project_build",
                           27,
                           {});

    const std::filesystem::path imported_layout_project_root = work_root / "imported_layout_project";
    WriteFile(imported_layout_project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase11c-imported-layout\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(imported_layout_project_root / "src/helper.mc",
              "export { PackedHeader, make_packed, PaddedHeader, make_padded }\n"
              "\n"
              "@packed\n"
              "struct PackedHeader {\n"
              "    tag: u8,\n"
              "    value: i32,\n"
              "    tail: u8,\n"
              "}\n"
              "\n"
              "struct PaddedHeader {\n"
              "    tag: u8,\n"
              "    value: i32,\n"
              "    tail: u8,\n"
              "}\n"
              "\n"
              "func make_packed() PackedHeader {\n"
              "    return PackedHeader{ tag: 2, value: 9, tail: 1 }\n"
              "}\n"
              "\n"
              "func make_padded() PaddedHeader {\n"
              "    return PaddedHeader{ tag: 3, value: 11, tail: 4 }\n"
              "}\n");
    WriteFile(imported_layout_project_root / "src/main.mc",
              "import helper\n"
              "\n"
              "func main() i32 {\n"
              "    packed: helper.PackedHeader = helper.make_packed()\n"
              "    padded: helper.PaddedHeader = helper.make_padded()\n"
              "    return (i32)(packed.tag) + packed.value + (i32)(packed.tail) + (i32)(padded.tag) + padded.value + (i32)(padded.tail)\n"
              "}\n");
    RunBuiltProjectFixture(mc_path,
                           imported_layout_project_root / "build.toml",
                           imported_layout_project_root / "src/main.mc",
                           work_root / "imported_layout_project_build",
                           30,
                           {});

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/arena_ast_build.mc",
                    work_root / "phase8_arena_ast_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/recursive_evaluator.mc",
                    work_root / "phase8_recursive_evaluator_build",
                    0,
                    {});

    const std::filesystem::path hosted_bad_return_source = work_root / "hosted_main_bad_return.mc";
    WriteFile(hosted_bad_return_source,
              "func main() u64 {\n"
              "    return 0\n"
              "}\n");
    RunBuildFailureFixture(mc_path,
                           hosted_bad_return_source,
                           work_root / "hosted_main_bad_return_build",
                           "hosted entry only supports void or i32 returns from 'main'");

    const std::filesystem::path buffer_slice_source = work_root / "buffer_to_slice_supported.mc";
    WriteFile(buffer_slice_source,
              "struct Buffer<T> {\n"
              "    ptr: *T,\n"
              "    len: usize,\n"
              "    cap: usize,\n"
              "    alloc: *u8,\n"
              "}\n"
              "\n"
              "func view(values: Buffer<i32>) Slice<i32> {\n"
              "    return (Slice<i32>)(values)\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    return 0\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    buffer_slice_source,
                    work_root / "buffer_to_slice_supported_build",
                    0,
                    {});

    return 0;
}