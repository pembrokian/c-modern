#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "compiler/support/dump_paths.h"

namespace {

struct CommandOutcome {
    bool exited = false;
    int exit_code = -1;
    bool signaled = false;
    int signal_number = -1;
};

struct BackgroundProcess {
    pid_t pid = -1;
    std::filesystem::path output_path;
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

void WriteFile(const std::filesystem::path& path,
               std::string_view contents) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << contents;
    if (!output) {
        Fail("unable to write file: " + path.generic_string());
    }
}

std::pair<CommandOutcome, std::string> RunCommandCapture(const std::vector<std::string>& args,
                                                         const std::filesystem::path& output_path,
                                                         const std::string& context) {
    std::filesystem::create_directories(output_path.parent_path());
    const std::string command = JoinCommand(args) + " >" + QuoteShellArg(output_path.generic_string()) + " 2>&1";
    const int raw_status = std::system(command.c_str());
    const std::string output = ReadFile(output_path);
#ifdef WIFEXITED
    if (WIFEXITED(raw_status)) {
        return {{.exited = true, .exit_code = WEXITSTATUS(raw_status)}, output};
    }
#endif
    Fail(context + ": command failed to execute cleanly: " + command);
    return {};
}

void ExpectOutputContains(std::string_view output,
                          std::string_view needle,
                          const std::string& context) {
    if (output.find(needle) == std::string::npos) {
        Fail(context + ": expected output to contain '" + std::string(needle) + "', got '" + std::string(output) + "'");
    }
}

void SleepForTimestampTick() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
}

std::filesystem::file_time_type RequireWriteTime(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        Fail("expected file to exist: " + path.generic_string());
    }
    return std::filesystem::last_write_time(path);
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

void CloseFd(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

BackgroundProcess SpawnBackgroundCommand(const std::vector<std::string>& args,
                                         const std::filesystem::path& output_path,
                                         const std::string& context) {
    if (args.empty()) {
        Fail(context + ": expected command arguments");
    }

    std::filesystem::create_directories(output_path.parent_path());
    pid_t pid = fork();
    if (pid < 0) {
        Fail(context + ": fork failed");
    }

    if (pid == 0) {
        const int output_fd = open(output_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (output_fd < 0) {
            _exit(127);
        }
        if (dup2(output_fd, STDOUT_FILENO) < 0 || dup2(output_fd, STDERR_FILENO) < 0) {
            close(output_fd);
            _exit(127);
        }
        close(output_fd);

        std::vector<std::string> argv_storage = args;
        std::vector<char*> argv_ptrs;
        argv_ptrs.reserve(argv_storage.size() + 1);
        for (std::string& value : argv_storage) {
            argv_ptrs.push_back(value.data());
        }
        argv_ptrs.push_back(nullptr);
        execv(argv_storage[0].c_str(), argv_ptrs.data());
        _exit(127);
    }

    return {.pid = pid, .output_path = output_path};
}

CommandOutcome WaitForProcessExit(pid_t pid,
                                  int timeout_ms,
                                  const std::string& context) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        int status = 0;
        const pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
#ifdef WIFEXITED
            if (WIFEXITED(status)) {
                return {.exited = true, .exit_code = WEXITSTATUS(status)};
            }
#endif
#ifdef WIFSIGNALED
            if (WIFSIGNALED(status)) {
                return {.signaled = true, .signal_number = WTERMSIG(status)};
            }
#endif
            Fail(context + ": child exited with unknown status");
        }
        if (result < 0) {
            Fail(context + ": waitpid failed");
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
            Fail(context + ": timed out waiting for child process");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ExpectBackgroundProcessSuccess(const BackgroundProcess& process,
                                    int timeout_ms,
                                    const std::string& context) {
    const CommandOutcome outcome = WaitForProcessExit(process.pid, timeout_ms, context);
    if (!outcome.exited || outcome.exit_code != 0) {
        const std::string output = ReadFile(process.output_path);
        Fail(context + ": child exited with code " + std::to_string(outcome.exit_code) +
             ", output='" + output + "'");
    }
}

uint16_t ReserveLoopbackPort() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        Fail("unable to create loopback probe socket");
    }

    const int enabled = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        const int saved_errno = errno;
        CloseFd(fd);
        Fail("unable to reserve loopback port: errno=" + std::to_string(saved_errno));
    }

    sockaddr_in bound_addr {};
    socklen_t bound_len = sizeof(bound_addr);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&bound_addr), &bound_len) != 0) {
        const int saved_errno = errno;
        CloseFd(fd);
        Fail("unable to read reserved loopback port: errno=" + std::to_string(saved_errno));
    }

    const uint16_t port = ntohs(bound_addr.sin_port);
    CloseFd(fd);
    return port;
}

int ConnectLoopbackWithRetry(uint16_t port,
                             int timeout_ms,
                             const std::string& context) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        const int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            Fail(context + ": unable to create client socket");
        }

        timeval timeout {};
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0) {
            return fd;
        }

        CloseFd(fd);
        if (std::chrono::steady_clock::now() >= deadline) {
            const int saved_errno = errno;
            Fail(context + ": unable to connect to loopback server: errno=" + std::to_string(saved_errno));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void WriteAllToSocket(int fd,
                      std::string_view data,
                      const std::string& context) {
    size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written = send(fd, data.data() + offset, data.size() - offset, 0);
        if (written <= 0) {
            const int saved_errno = errno;
            Fail(context + ": send failed: errno=" + std::to_string(saved_errno));
        }
        offset += static_cast<size_t>(written);
    }
}

std::string ReadExactFromSocket(int fd,
                                size_t expected_size,
                                const std::string& context) {
    std::string data(expected_size, '\0');
    size_t offset = 0;
    while (offset < expected_size) {
        const ssize_t read_size = recv(fd, data.data() + offset, expected_size - offset, 0);
        if (read_size <= 0) {
            const int saved_errno = errno;
            Fail(context + ": recv failed: errno=" + std::to_string(saved_errno));
        }
        offset += static_cast<size_t>(read_size);
    }
    return data;
}

void TestHelpMentionsRun(const std::filesystem::path& binary_root,
                         const std::filesystem::path& mc_path) {
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(), "--help"},
                                                     binary_root / "phase7_help_output.txt",
                                                     "phase7 help");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("mc --help should succeed");
    }
    ExpectOutputContains(output, "mc run", "help text should mention mc run");
    ExpectOutputContains(output, "mc test", "help text should mention mc test");
    ExpectOutputContains(output, "--mode <name>", "help text should mention mode overrides");
}

void TestProjectTestCommandSucceeds(const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase7_test_success_project";
    const std::filesystem::path project_path = WriteTestProject(
        project_root,
        "func main() i32 {\n"
        "    return 0\n"
        "}\n");
    WriteFile(project_root / "tests/alpha_test.mc",
              "export { test_alpha_pass }\n"
              "\n"
              "func test_alpha_pass() *i32 {\n"
              "    return nil\n"
              "}\n");
    WriteFile(project_root / "tests/beta_test.mc",
              "export { test_beta_pass }\n"
              "\n"
              "func test_beta_pass() *i32 {\n"
              "    return nil\n"
              "}\n");
    WriteFile(project_root / "tests/compiler_manifest.txt",
              "check-pass regressions/check_ok.mc\n"
              "check-fail regressions/check_fail.mc regressions/check_fail.errors.txt\n"
              "run-output regressions/run_ok.mc 5 regressions/run_ok.stdout.txt\n"
              "mir regressions/mir_ok.mc regressions/mir_ok.mir.txt\n");
    WriteFile(project_root / "tests/regressions/check_ok.mc",
              "func helper(value: i32) i32 {\n"
              "    return value\n"
              "}\n");
    WriteFile(project_root / "tests/regressions/check_fail.mc",
              "import missing\n"
              "\n"
              "func main() i32 {\n"
              "    return 0\n"
              "}\n");
    WriteFile(project_root / "tests/regressions/check_fail.errors.txt",
              "unable to resolve import module: missing\n");
    WriteFile(project_root / "tests/regressions/run_ok.mc",
              "import io\n"
              "\n"
              "func main() i32 {\n"
              "    status: i32 = io.write_line(\"phase7 run output\")\n"
              "    if status != 0 {\n"
              "        return status\n"
              "    }\n"
              "    return 5\n"
              "}\n");
    WriteFile(project_root / "tests/regressions/run_ok.stdout.txt",
              "phase7 run output\n");
    WriteFile(project_root / "tests/regressions/mir_ok.mc",
              "distinct UserId = i32\n"
              "\n"
              "func promote(raw: i32) UserId {\n"
              "    return (UserId)(raw)\n"
              "}\n");
    WriteFile(project_root / "tests/regressions/mir_ok.mir.txt",
              "Module\n"
              "  TypeDecl kind=distinct name=UserId\n"
              "    AliasedType=i32\n"
              "  Function name=promote returns=[UserId]\n"
              "    Local name=raw type=i32 param readonly\n"
              "    Block label=entry0\n"
              "      load_local %v0:i32 target=raw\n"
              "      convert_distinct %v1:UserId target=UserId operands=[%v0]\n"
              "      terminator return values=[%v1]\n");

    const std::filesystem::path build_dir = binary_root / "phase7_test_success_build";
    std::filesystem::remove_all(build_dir);
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     build_dir / "phase7_test_success_output.txt",
                                                     "phase7 test success command");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("mc test success project should pass:\n" + output);
    }

    ExpectOutputContains(output, "testing target app", "mc test should announce the target under test");
    ExpectOutputContains(output, "PASS alpha_test.test_alpha_pass", "ordinary tests should report passing cases");
    ExpectOutputContains(output, "PASS beta_test.test_beta_pass", "ordinary tests should be discovered deterministically");
    ExpectOutputContains(output, "2 tests, 2 passed, 0 failed", "ordinary test summary should be printed");
    ExpectOutputContains(output,
                         "PASS check-pass " + (project_root / "tests/regressions/check_ok.mc").generic_string(),
                         "compiler regression check-pass should run");
    ExpectOutputContains(output,
                         "PASS check-fail " + (project_root / "tests/regressions/check_fail.mc").generic_string(),
                         "compiler regression check-fail should run");
    ExpectOutputContains(output,
                         "PASS run-output " + (project_root / "tests/regressions/run_ok.mc").generic_string(),
                         "compiler regression run-output should run");
    ExpectOutputContains(output,
                         "PASS mir " + (project_root / "tests/regressions/mir_ok.mc").generic_string(),
                         "compiler regression MIR fixture should run");
}

void TestProjectTestCommandFailsOnOrdinaryFailure(const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase7_test_failure_project";
    const std::filesystem::path project_path = WriteTestProject(
        project_root,
        "func main() i32 {\n"
        "    return 0\n"
        "}\n");
    WriteFile(project_root / "tests/failing_test.mc",
              "export { test_failure }\n"
              "import testing\n"
              "\n"
              "func test_failure() *i32 {\n"
              "    return testing.fail()\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "phase7_test_failure_build";
    std::filesystem::remove_all(build_dir);
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     build_dir / "phase7_test_failure_output.txt",
                                                     "phase7 test failure command");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test failure project should return a non-zero exit status:\n" + output);
    }

    ExpectOutputContains(output, "FAIL failing_test.test_failure", "ordinary test failure should be reported");
    ExpectOutputContains(output, "1 tests, 0 passed, 1 failed", "ordinary test failure summary should be printed");
}

void TestProjectBuildAndMciEmission(const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase7_generated_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "export { answer }\n"
        "\n"
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "phase7_tool_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     build_dir / "build_output.txt",
                                                     "phase7 project build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase7 project build should succeed:\n" + output);
    }

    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    const auto main_mci = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir).mci;
    const auto helper_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/helper.mc", build_dir).object;
    const auto main_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).object;
    const auto artifacts = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    if (!std::filesystem::exists(helper_mci) || !std::filesystem::exists(main_mci) || !std::filesystem::exists(helper_object) ||
        !std::filesystem::exists(main_object) || !std::filesystem::exists(artifacts.executable)) {
        Fail("phase7 project build should emit module objects, executable, and .mci artifacts");
    }

    const std::string helper_mci_text = ReadFile(helper_mci);
    ExpectOutputContains(helper_mci_text, "module\thelper", "helper .mci should record the module identity");
    ExpectOutputContains(helper_mci_text, "function\tanswer", "helper .mci should record exported functions");
    ExpectOutputContains(helper_mci_text, "interface_hash\t", "helper .mci should record an interface hash");

    const auto [run_outcome, run_output] = RunCommandCapture({artifacts.executable.generic_string()},
                                                             build_dir / "run_output.txt",
                                                             "phase7 project executable run");
    if (!run_outcome.exited || run_outcome.exit_code != 7) {
        Fail("phase7 project executable should return 7, got output:\n" + run_output);
    }
}

void TestProjectTestTargetBuildsAndRuns(const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase13_test_kind_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-test-kind\"\n"
              "default = \"unit\"\n"
              "\n"
              "[targets.unit]\n"
              "kind = \"test\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.unit.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.unit.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc",
              "func main() i32 {\n"
              "    return 6\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "phase13_test_kind_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  (project_root / "build.toml").generic_string(),
                                                                  "--build-dir",
                                                                  build_dir.generic_string()},
                                                                 build_dir / "build_output.txt",
                                                                 "phase13 test-kind build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase13 test target build should succeed:\n" + build_output);
    }

    const auto executable = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).executable;
    if (!std::filesystem::exists(executable)) {
        Fail("phase13 test target build should emit an executable artifact");
    }

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              (project_root / "build.toml").generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "run_output.txt",
                                                             "phase13 test-kind run");
    if (!run_outcome.exited || run_outcome.exit_code != 6) {
        Fail("phase13 test target run should return 6, got:\n" + run_output);
    }
}

void TestProjectImportedGlobalMirDeclarations(const std::filesystem::path& binary_root,
                                              const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase7_imported_globals_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "export { LIMIT, counter }\n"
        "\n"
        "const LIMIT: i32 = 9\n"
        "var counter: i32 = 4\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    helper.counter = helper.counter + helper.LIMIT\n"
        "    return helper.counter\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "phase7_imported_globals_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string(),
                                                      "--dump-mir"},
                                                     build_dir / "build_output.txt",
                                                     "phase7 imported global dump-mir build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase7 imported global dump-mir build should succeed:\n" + output);
    }

    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    const auto main_mir = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir).mir;
    const std::string helper_mci_text = ReadFile(helper_mci);
    const std::string main_mir_text = ReadFile(main_mir);
    ExpectOutputContains(helper_mci_text,
                         "global\t0\tcounter\ti32\t",
                         "helper .mci should record exported mutable globals");
    ExpectOutputContains(main_mir_text,
                         "ConstGlobal names=[helper.LIMIT] type=i32 extern",
                         "dependent MIR should record imported const globals as extern globals");
    ExpectOutputContains(main_mir_text,
                         "VarGlobal names=[helper.counter] type=i32 extern",
                         "dependent MIR should record imported mutable globals as extern globals");
    ExpectOutputContains(main_mir_text,
                         "store_target target=helper.counter target_kind=global target_name=helper.counter",
                         "dependent MIR should lower imported mutable global stores as global targets");
}

void TestCorruptedInterfaceArtifactFailsBuild(const std::filesystem::path& binary_root,
                                              const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase13_corrupt_mci_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "export { answer }\n"
        "\n"
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "phase13_corrupt_mci_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [initial_outcome, initial_output] = RunCommandCapture({mc_path.generic_string(),
                                                                      "build",
                                                                      "--project",
                                                                      project_path.generic_string(),
                                                                      "--build-dir",
                                                                      build_dir.generic_string()},
                                                                     build_dir / "initial_build_output.txt",
                                                                     "phase13 initial build");
    if (!initial_outcome.exited || initial_outcome.exit_code != 0) {
        Fail("phase13 initial build should succeed:\n" + initial_output);
    }

    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    WriteFile(helper_mci,
              "format\t2\n"
              "target\tarm64-apple-darwin25.4.0\n"
              "module\thelper\n"
              "source\t" + (project_root / "src/helper.mc").generic_string() + "\n"
              "interface_hash\tdeadbeef\n");

    const auto [corrupt_outcome, corrupt_output] = RunCommandCapture({mc_path.generic_string(),
                                                                      "build",
                                                                      "--project",
                                                                      project_path.generic_string(),
                                                                      "--build-dir",
                                                                      build_dir.generic_string()},
                                                                     build_dir / "corrupt_build_output.txt",
                                                                     "phase13 corrupt mci build");
    if (!corrupt_outcome.exited || corrupt_outcome.exit_code == 0) {
        Fail("build with corrupted existing .mci should fail");
    }
    ExpectOutputContains(corrupt_output,
                         "interface artifact hash mismatch",
                         "corrupted .mci should fail with a trust-boundary diagnostic");
}

void TestModuleBuildStateIsVersionedAndDeterministic(const std::filesystem::path& binary_root,
                                                     const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase13_state_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-state\"\n"
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
    WriteFile(project_root / "src/alpha.mc",
              "export { answer_alpha }\n"
              "\n"
              "func answer_alpha() i32 {\n"
              "    return 2\n"
              "}\n");
    WriteFile(project_root / "src/zeta.mc",
              "export { answer_zeta }\n"
              "\n"
              "func answer_zeta() i32 {\n"
              "    return 5\n"
              "}\n");
    WriteFile(project_root / "src/main.mc",
              "import zeta\n"
              "import alpha\n"
              "\n"
              "func main() i32 {\n"
              "    return zeta.answer_zeta() + alpha.answer_alpha()\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "phase13_state_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     build_dir / "state_build_output.txt",
                                                     "phase13 deterministic state build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase13 deterministic state build should succeed:\n" + output);
    }

    const auto state_path = build_dir / "state" /
                            (mc::support::SanitizeArtifactStem(project_root / "src/main.mc") + ".state.txt");
    const std::string state_text = ReadFile(state_path);
    ExpectOutputContains(state_text, "format\t1\n", "module build state should record its format version");

    const std::size_t alpha_hash = state_text.find("import_hash\talpha=");
    const std::size_t zeta_hash = state_text.find("import_hash\tzeta=");
    if (alpha_hash == std::string::npos || zeta_hash == std::string::npos) {
        Fail("module build state should record imported interface hashes for all imports");
    }
    if (!(alpha_hash < zeta_hash)) {
        Fail("module build state should serialize import hashes in deterministic sorted order");
    }
}

void TestForeignInterfaceArtifactFailsBuild(const std::filesystem::path& binary_root,
                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path source_project_root = binary_root / "phase13_foreign_artifact_source_project";
    const std::filesystem::path source_project_path = WriteBasicProject(
        source_project_root,
        "export { answer }\n"
        "\n"
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path source_build_dir = binary_root / "phase13_foreign_artifact_source_build";
    std::filesystem::remove_all(source_build_dir);
    std::filesystem::create_directories(source_build_dir);

    const auto [source_outcome, source_output] = RunCommandCapture({mc_path.generic_string(),
                                                                    "build",
                                                                    "--project",
                                                                    source_project_path.generic_string(),
                                                                    "--build-dir",
                                                                    source_build_dir.generic_string()},
                                                                   source_build_dir / "build_output.txt",
                                                                   "phase13 foreign artifact source build");
    if (!source_outcome.exited || source_outcome.exit_code != 0) {
        Fail("phase13 foreign artifact source build should succeed:\n" + source_output);
    }

    const std::filesystem::path contaminated_project_root = binary_root / "phase13_foreign_artifact_target_project";
    const std::filesystem::path contaminated_project_path = WriteBasicProject(
        contaminated_project_root,
        "export { answer }\n"
        "\n"
        "func answer() i32 {\n"
        "    return 11\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path contaminated_build_dir = binary_root / "phase13_foreign_artifact_target_build";
    std::filesystem::remove_all(contaminated_build_dir);
    std::filesystem::create_directories(contaminated_build_dir / "mci");

    const auto source_helper_mci = mc::support::ComputeDumpTargets(source_project_root / "src/helper.mc", source_build_dir).mci;
    const auto target_helper_mci = mc::support::ComputeDumpTargets(contaminated_project_root / "src/helper.mc", contaminated_build_dir).mci;
    WriteFile(target_helper_mci, ReadFile(source_helper_mci));

    const auto [contaminated_outcome, contaminated_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                "build",
                                                                                "--project",
                                                                                contaminated_project_path.generic_string(),
                                                                                "--build-dir",
                                                                                contaminated_build_dir.generic_string()},
                                                                               contaminated_build_dir / "contaminated_output.txt",
                                                                               "phase13 foreign artifact contaminated build");
    if (!contaminated_outcome.exited || contaminated_outcome.exit_code == 0) {
        Fail("build with foreign .mci contamination should fail");
    }
    ExpectOutputContains(contaminated_output,
                         "interface artifact source path mismatch",
                         "foreign .mci contamination should fail with metadata mismatch");
}

void TestInvalidStateFileForcesRebuild(const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase13_invalid_state_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "export { answer }\n"
        "\n"
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "phase13_invalid_state_build";
    std::filesystem::remove_all(build_dir);

    const auto run_build = [&](const std::string& context) {
        const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                          "build",
                                                          "--project",
                                                          project_path.generic_string(),
                                                          "--build-dir",
                                                          build_dir.generic_string()},
                                                         build_dir / (context + ".txt"),
                                                         context);
        if (!outcome.exited || outcome.exit_code != 0) {
            Fail(context + " should succeed:\n" + output);
        }
    };

    std::filesystem::remove_all(build_dir);
    run_build("phase13_invalid_state_initial_build");

    const auto helper_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/helper.mc", build_dir).object;
    const auto helper_state = build_dir / "state" /
                              (mc::support::SanitizeArtifactStem(project_root / "src/helper.mc") + ".state.txt");
    const auto helper_object_time_1 = RequireWriteTime(helper_object);

    SleepForTimestampTick();
    WriteFile(helper_state,
              "format\t999\n"
              "target\tarm64-apple-darwin25.4.0\n");
    run_build("phase13_invalid_state_rebuild");

    if (!(RequireWriteTime(helper_object) > helper_object_time_1)) {
        Fail("invalid stale state file should force rebuilding the affected module object");
    }
}

void TestIncrementalRebuildBehavior(const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase7_incremental_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "export { answer }\n"
        "\n"
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "phase7_incremental_build";
    std::filesystem::remove_all(build_dir);

    const auto run_build = [&](const std::string& context) {
        const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                          "build",
                                                          "--project",
                                                          project_path.generic_string(),
                                                          "--build-dir",
                                                          build_dir.generic_string()},
                                                         build_dir / (context + ".txt"),
                                                         context);
        if (!outcome.exited || outcome.exit_code != 0) {
            Fail(context + " should succeed:\n" + output);
        }
    };

    run_build("phase7_initial_build");

    const auto helper_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/helper.mc", build_dir).object;
    const auto main_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).object;
    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    const auto executable = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).executable;

    const auto helper_object_time_1 = RequireWriteTime(helper_object);
    const auto main_object_time_1 = RequireWriteTime(main_object);
    const auto helper_mci_time_1 = RequireWriteTime(helper_mci);
    const auto executable_time_1 = RequireWriteTime(executable);

    SleepForTimestampTick();
    run_build("phase7_noop_build");

    if (RequireWriteTime(helper_object) != helper_object_time_1 || RequireWriteTime(main_object) != main_object_time_1 ||
        RequireWriteTime(helper_mci) != helper_mci_time_1 || RequireWriteTime(executable) != executable_time_1) {
        Fail("no-op rebuild should reuse existing helper/main objects, mci, and executable outputs");
    }

    SleepForTimestampTick();
    WriteFile(project_root / "src/helper.mc",
              "export { answer }\n"
              "\n"
              "func answer() i32 {\n"
              "    return 9\n"
              "}\n");
    run_build("phase7_impl_only_build");

    const auto helper_object_time_2 = RequireWriteTime(helper_object);
    const auto main_object_time_2 = RequireWriteTime(main_object);
    const auto helper_mci_time_2 = RequireWriteTime(helper_mci);
    const auto executable_time_2 = RequireWriteTime(executable);
    if (!(helper_object_time_2 > helper_object_time_1)) {
        Fail("implementation-only helper edit should rebuild the helper object");
    }
    if (main_object_time_2 != main_object_time_1) {
        Fail("implementation-only helper edit should not rebuild the dependent main object");
    }
    if (helper_mci_time_2 != helper_mci_time_1) {
        Fail("implementation-only helper edit should preserve the helper interface artifact");
    }
    if (!(executable_time_2 > executable_time_1)) {
        Fail("implementation-only helper edit should relink the final executable");
    }

    const auto [impl_run_outcome, impl_run_output] = RunCommandCapture({executable.generic_string()},
                                                                       build_dir / "phase7_impl_only_run.txt",
                                                                       "phase7 impl-only executable run");
    if (!impl_run_outcome.exited || impl_run_outcome.exit_code != 9) {
        Fail("implementation-only helper edit should change runtime behavior to 9, got:\n" + impl_run_output);
    }

    SleepForTimestampTick();
    WriteFile(project_root / "src/helper.mc",
              "export { answer, extra }\n"
              "\n"
              "func answer() i32 {\n"
              "    return 9\n"
              "}\n"
              "\n"
              "func extra() i32 {\n"
              "    return 1\n"
              "}\n");
    run_build("phase7_interface_change_build");

    if (!(RequireWriteTime(helper_object) > helper_object_time_2)) {
        Fail("interface-changing helper edit should rebuild the helper object");
    }
    if (!(RequireWriteTime(main_object) > main_object_time_2)) {
        Fail("interface-changing helper edit should rebuild the dependent main object");
    }
    if (!(RequireWriteTime(helper_mci) > helper_mci_time_2)) {
        Fail("interface-changing helper edit should rewrite the helper interface artifact");
    }
}

void TestRunExitCodeAndArgs(const std::filesystem::path& binary_root,
                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase7_run_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "export { answer }\n"
        "\n"
        "func answer() i32 {\n"
        "    return 1\n"
        "}\n",
        "func main(args: Slice<cstr>) i32 {\n"
        "    if args.len == 3 {\n"
        "        return 9\n"
        "    }\n"
        "    return 4\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "phase7_run_build";
    std::filesystem::remove_all(build_dir);
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "run",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string(),
                                                      "--",
                                                      "alpha",
                                                      "beta"},
                                                     build_dir / "phase7_run_output.txt",
                                                     "phase7 run command");
    if (!outcome.exited || outcome.exit_code != 9) {
        Fail("mc run should return the executable exit code 9, got output:\n" + output);
    }
}

void TestMissingDefaultTargetFails(const std::filesystem::path& binary_root,
                                   const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase7_no_default_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase7-no-default\"\n"
              "\n"
              "[targets.first]\n"
              "kind = \"exe\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.first.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.first.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.second]\n"
              "kind = \"exe\"\n"
              "root = \"src/alt.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.second.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.second.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "src/alt.mc", "func main() i32 { return 0 }\n");

    const auto [missing_default_outcome, missing_default_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                      "build",
                                                                                      "--project",
                                                                                      (project_root / "build.toml").generic_string()},
                                                                                     binary_root / "phase7_missing_default_output.txt",
                                                                                     "phase7 missing default target");
    if (!missing_default_outcome.exited || missing_default_outcome.exit_code == 0) {
        Fail("project without default target should fail when no --target is provided");
    }
    ExpectOutputContains(missing_default_output,
                         "project file does not declare a default target; pass --target <name>",
                         "missing default target diagnostic");

    const std::filesystem::path explicit_target_build_dir = binary_root / "phase7_explicit_target_build";
    std::filesystem::remove_all(explicit_target_build_dir);

    const auto [explicit_target_outcome, explicit_target_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                      "build",
                                                                                      "--project",
                                                                                      (project_root / "build.toml").generic_string(),
                                                                                      "--target",
                                                                                      "second",
                                                                                      "--build-dir",
                                                                                      explicit_target_build_dir.generic_string()},
                                                                                     binary_root / "phase7_explicit_target_output.txt",
                                                                                     "phase7 explicit target build");
    if (!explicit_target_outcome.exited || explicit_target_outcome.exit_code != 0) {
        Fail("project build with explicit --target should succeed:\n" + explicit_target_output);
    }
}

void TestProjectMissingImportRootFails(const std::filesystem::path& source_root,
                                       const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests/tool/phase7_missing_root_project";
    const std::filesystem::path output_path = binary_root / "phase7_missing_root_output.txt";
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "check",
                                                      "--project",
                                                      (fixture_root / "build.toml").generic_string()},
                                                     output_path,
                                                     "phase7 missing root check");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("phase7 missing import root check should fail");
    }
    ExpectOutputContains(output, "unable to resolve import module: hidden", "missing import root diagnostic");
}

void TestProjectAmbiguousImportFails(const std::filesystem::path& source_root,
                                     const std::filesystem::path& binary_root,
                                     const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests/tool/phase7_ambiguous_project";
    const std::filesystem::path output_path = binary_root / "phase7_ambiguous_output.txt";
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "check",
                                                      "--project",
                                                      (fixture_root / "build.toml").generic_string()},
                                                     output_path,
                                                     "phase7 ambiguous import check");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("phase7 ambiguous import check should fail");
    }
    ExpectOutputContains(output, "ambiguous import module 'helper' matched multiple roots", "ambiguous import diagnostic");
}

void TestDuplicateModuleRootFailsEarly(const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase13_duplicate_root_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-duplicate-root\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\", \"./src\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string()},
                                                     binary_root / "phase13_duplicate_root_output.txt",
                                                     "phase13 duplicate module root build");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("project with duplicate module roots should fail during project validation");
    }
    ExpectOutputContains(output,
                         "target 'app' declares duplicate module search path:",
                         "duplicate module roots should produce a strict project diagnostic");
}

void TestProjectTestTimeoutFailsDeterministically(const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase13_test_timeout_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-test-timeout\"\n"
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
              "startup = \"default\"\n"
              "\n"
              "[targets.app.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 100\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "tests/hang_test.mc",
              "export { test_hang }\n"
              "\n"
              "func test_hang() *i32 {\n"
              "    while true {\n"
              "    }\n"
              "    return nil\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "phase13_test_timeout_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     build_dir / "timeout_test_output.txt",
                                                     "phase13 timeout test command");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test with a hanging ordinary test should fail");
    }
    ExpectOutputContains(output,
                         "TIMEOUT ordinary tests for target app after 100 ms",
                         "ordinary test timeout should be reported deterministically");
}

void TestDuplicateTargetRootsFailEarly(const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase13_duplicate_target_root_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-duplicate-target-root\"\n"
              "default = \"first\"\n"
              "\n"
              "[targets.first]\n"
              "kind = \"exe\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.first.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.first.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.second]\n"
              "kind = \"exe\"\n"
              "root = \"./src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.second.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.second.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string()},
                                                     binary_root / "phase13_duplicate_target_root_output.txt",
                                                     "phase13 duplicate target roots build");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("project with duplicate target roots should fail during target validation");
    }
    ExpectOutputContains(output,
                         "declare the same root module:",
                         "duplicate target roots should produce a deterministic graph diagnostic");
}

void TestRealGrepLiteProject(const std::filesystem::path& source_root,
                             const std::filesystem::path& binary_root,
                             const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/grep_lite/build.toml";
    const std::filesystem::path sample_path = source_root / "examples/real/grep_lite/tests/sample.txt";
    const std::filesystem::path build_dir = binary_root / "phase8_grep_lite_build";
    std::filesystem::remove_all(build_dir);

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string(),
                                                              "--",
                                                              "alpha",
                                                              sample_path.generic_string()},
                                                             build_dir / "phase8_grep_lite_run_output.txt",
                                                             "phase8 grep-lite run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase8 grep-lite run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output,
                         "alpha\nalphabet\n",
                         "phase8 grep-lite run should print matching lines only");
    if (run_output.find("beta\n") != std::string::npos || run_output.find("omega\n") != std::string::npos) {
        Fail("phase8 grep-lite run should not print non-matching lines, got:\n" + run_output);
    }

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "phase8_grep_lite_test_output.txt",
                                                               "phase8 grep-lite test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase8 grep-lite tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS contains_match_test.test_contains_match",
                         "phase8 grep-lite ordinary tests should include contains-match coverage");
    ExpectOutputContains(test_output,
                         "PASS count_matches_test.test_count_matches",
                         "phase8 grep-lite ordinary tests should include line-count coverage");
    ExpectOutputContains(test_output,
                         "3 tests, 3 passed, 0 failed",
                         "phase8 grep-lite test summary should be deterministic");
}

void TestRealFileWalkerProject(const std::filesystem::path& source_root,
                               const std::filesystem::path& binary_root,
                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/file_walker/build.toml";
    const std::filesystem::path sample_root = source_root / "examples/real/file_walker/tests/sample_tree";
    const std::filesystem::path build_dir = binary_root / "phase8_file_walker_build";
    std::filesystem::remove_all(build_dir);

    const std::string alpha_path = (sample_root / "alpha.txt").generic_string() + "\n";
    const std::string beta_path = (sample_root / "nested/beta.txt").generic_string() + "\n";
    const std::string gamma_path = (sample_root / "nested/deeper/gamma.txt").generic_string() + "\n";
    const std::string directory_line = (sample_root / "nested").generic_string() + "\n";

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string(),
                                                              "--",
                                                              sample_root.generic_string()},
                                                             build_dir / "phase8_file_walker_run_output.txt",
                                                             "phase8 file walker run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase8 file walker run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output, alpha_path, "phase8 file walker should print top-level files");
    ExpectOutputContains(run_output, beta_path, "phase8 file walker should recurse into nested directories");
    ExpectOutputContains(run_output, gamma_path, "phase8 file walker should recurse through deeper directories");
    if (run_output.find(directory_line) != std::string::npos) {
        Fail("phase8 file walker should print files only, got:\n" + run_output);
    }

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "phase8_file_walker_test_output.txt",
                                                               "phase8 file walker test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase8 file walker tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS entry_name_test.test_entry_name",
                         "phase8 file walker ordinary tests should include entry-name coverage");
    ExpectOutputContains(test_output,
                         "PASS path_join_test.test_path_join",
                         "phase8 file walker ordinary tests should include path-join coverage");
    ExpectOutputContains(test_output,
                         "3 tests, 3 passed, 0 failed",
                         "phase8 file walker test summary should be deterministic");
}

void TestRealHashToolProject(const std::filesystem::path& source_root,
                             const std::filesystem::path& binary_root,
                             const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/hash_tool/build.toml";
    const std::filesystem::path sample_path = source_root / "examples/real/hash_tool/tests/sample.txt";
    const std::filesystem::path build_dir = binary_root / "phase8_hash_tool_build";
    std::filesystem::remove_all(build_dir);

    const std::string expected_line =
        "d17af4fb11e13fbf  " + sample_path.generic_string() + "\n";

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string(),
                                                              "--",
                                                              sample_path.generic_string()},
                                                             build_dir / "phase8_hash_tool_run_output.txt",
                                                             "phase8 hash tool run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase8 hash tool run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output,
                         expected_line,
                         "phase8 hash tool should print the deterministic hash line");

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "phase8_hash_tool_test_output.txt",
                                                               "phase8 hash tool test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase8 hash tool tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS hash_bytes_test.test_hash_bytes",
                         "phase8 hash tool ordinary tests should include byte-hash coverage");
    ExpectOutputContains(test_output,
                         "PASS hex_u64_test.test_hex_u64",
                         "phase8 hash tool ordinary tests should include hex encoding coverage");
    ExpectOutputContains(test_output,
                         "3 tests, 3 passed, 0 failed",
                         "phase8 hash tool test summary should be deterministic");
}

void TestRealArenaExprToolProject(const std::filesystem::path& source_root,
                                  const std::filesystem::path& binary_root,
                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/arena_expr/build.toml";
    const std::filesystem::path sample_path = source_root / "examples/real/arena_expr/tests/sample.expr";
    const std::filesystem::path build_dir = binary_root / "phase8_arena_expr_build";
    std::filesystem::remove_all(build_dir);

    const std::string expected_line = "((1+2)+(3+4))\n";

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string(),
                                                              "--",
                                                              sample_path.generic_string()},
                                                             build_dir / "phase8_arena_expr_run_output.txt",
                                                             "phase8 arena expr run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase8 arena expr run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output,
                         expected_line,
                         "phase8 arena expr should print the deterministic normalized form");

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "phase8_arena_expr_test_output.txt",
                                                               "phase8 arena expr test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase8 arena expr tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS normalize_text_test.test_normalize_text",
                         "phase8 arena expr ordinary tests should include normalization coverage");
    ExpectOutputContains(test_output,
                         "PASS parse_tree_test.test_parse_tree",
                         "phase8 arena expr ordinary tests should include tree-shape coverage");
    ExpectOutputContains(test_output,
                         "3 tests, 3 passed, 0 failed",
                         "phase8 arena expr test summary should be deterministic");
}

void TestRealEventedEchoProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/evented_echo/build.toml";
    const std::filesystem::path build_dir = binary_root / "phase13_evented_echo_build";
    std::filesystem::remove_all(build_dir);

    const uint16_t port = ReserveLoopbackPort();
    const BackgroundProcess run_process = SpawnBackgroundCommand({mc_path.generic_string(),
                                                                  "run",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--",
                                                                  std::to_string(port)},
                                                                 build_dir / "phase13_evented_echo_run_output.txt",
                                                                 "phase13 evented echo run");

    const std::string request = "phase13-echo";
    const int client_fd = ConnectLoopbackWithRetry(port, 3000, "connect to phase13 evented echo server");
    WriteAllToSocket(client_fd, request, "write request to phase13 evented echo server");
    const std::string reply = ReadExactFromSocket(client_fd,
                                                  request.size(),
                                                  "read reply from phase13 evented echo server");
    CloseFd(client_fd);
    if (reply != request) {
        Fail("phase13 evented echo server returned unexpected payload: '" + reply + "'");
    }

    ExpectBackgroundProcessSuccess(run_process, 3000, "wait for phase13 evented echo run");

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "phase13_evented_echo_test_output.txt",
                                                               "phase13 evented echo test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase13 evented echo tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "testing target echo",
                         "phase13 evented echo test should announce the target under test");
    ExpectOutputContains(test_output,
                         "PASS parse_valid_port_test.test_parse_valid_port",
                         "phase13 evented echo ordinary tests should include valid-port coverage");
    ExpectOutputContains(test_output,
                         "PASS parse_zero_port_test.test_parse_zero_port",
                         "phase13 evented echo ordinary tests should include zero-port coverage");
    ExpectOutputContains(test_output,
                         "PASS parse_invalid_port_test.test_parse_invalid_port",
                         "phase13 evented echo ordinary tests should include invalid-port coverage");
    ExpectOutputContains(test_output,
                         "3 tests, 3 passed, 0 failed",
                         "phase13 evented echo test summary should be deterministic");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        Fail("expected source root and binary root arguments");
    }

    const std::filesystem::path source_root = argv[1];
    const std::filesystem::path binary_root = argv[2];
    const std::filesystem::path mc_path = binary_root / "mc";

    TestHelpMentionsRun(binary_root, mc_path);
    TestProjectBuildAndMciEmission(binary_root, mc_path);
    TestProjectTestTargetBuildsAndRuns(binary_root, mc_path);
    TestProjectImportedGlobalMirDeclarations(binary_root, mc_path);
    TestCorruptedInterfaceArtifactFailsBuild(binary_root, mc_path);
    TestModuleBuildStateIsVersionedAndDeterministic(binary_root, mc_path);
    TestForeignInterfaceArtifactFailsBuild(binary_root, mc_path);
    TestInvalidStateFileForcesRebuild(binary_root, mc_path);
    TestIncrementalRebuildBehavior(binary_root, mc_path);
    TestRunExitCodeAndArgs(binary_root, mc_path);
    TestProjectTestCommandSucceeds(binary_root, mc_path);
    TestProjectTestCommandFailsOnOrdinaryFailure(binary_root, mc_path);
    TestMissingDefaultTargetFails(binary_root, mc_path);
    TestProjectMissingImportRootFails(source_root, binary_root, mc_path);
    TestProjectAmbiguousImportFails(source_root, binary_root, mc_path);
    TestDuplicateModuleRootFailsEarly(binary_root, mc_path);
    TestProjectTestTimeoutFailsDeterministically(binary_root, mc_path);
    TestDuplicateTargetRootsFailEarly(binary_root, mc_path);
    TestRealGrepLiteProject(source_root, binary_root, mc_path);
    TestRealFileWalkerProject(source_root, binary_root, mc_path);
    TestRealHashToolProject(source_root, binary_root, mc_path);
    TestRealArenaExprToolProject(source_root, binary_root, mc_path);
    TestRealEventedEchoProject(source_root, binary_root, mc_path);
    return 0;
}
