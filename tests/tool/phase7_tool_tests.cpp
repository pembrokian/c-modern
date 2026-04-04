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

void CopyDirectoryTree(const std::filesystem::path& source,
                       const std::filesystem::path& destination) {
    std::filesystem::remove_all(destination);
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive);
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

std::string BuildPartialWriteResponse(size_t size) {
    static constexpr std::string_view kPattern = "phase16-partial-write-state|";

    std::string payload;
    payload.resize(size);
    for (size_t index = 0; index < size; ++index) {
        payload[index] = kPattern[index % kPattern.size()];
    }
    return payload;
}

void ExerciseEventedEchoRoundTrip(const std::filesystem::path& mc_path,
                                  const std::filesystem::path& project_path,
                                  const std::filesystem::path& build_dir,
                                  uint16_t port,
                                  std::string_view output_name,
                                  const std::string& context) {
    const BackgroundProcess run_process = SpawnBackgroundCommand({mc_path.generic_string(),
                                                                  "run",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--",
                                                                  std::to_string(port)},
                                                                 build_dir / std::string(output_name),
                                                                 context);

    const std::string request = "phase13-echo";
    const int client_fd = ConnectLoopbackWithRetry(port, 3000, "connect to " + context);
    WriteAllToSocket(client_fd, request, "write request to " + context);
    const std::string reply = ReadExactFromSocket(client_fd,
                                                  request.size(),
                                                  "read reply from " + context);
    CloseFd(client_fd);
    if (reply != request) {
        Fail(context + ": server returned unexpected payload: '" + reply + "'");
    }

    ExpectBackgroundProcessSuccess(run_process, 3000, "wait for " + context);
}

void ExercisePartialWriteRoundTrip(const std::filesystem::path& mc_path,
                                   const std::filesystem::path& project_path,
                                   const std::filesystem::path& build_dir,
                                   uint16_t port,
                                   std::string_view output_name,
                                   const std::string& context) {
    const BackgroundProcess run_process = SpawnBackgroundCommand({mc_path.generic_string(),
                                                                  "run",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--",
                                                                  std::to_string(port)},
                                                                 build_dir / std::string(output_name),
                                                                 context);

    const int client_fd = ConnectLoopbackWithRetry(port, 3000, "connect to " + context);
    WriteAllToSocket(client_fd, "push", "write request to " + context);
    const std::string reply = ReadExactFromSocket(client_fd,
                                                  1536,
                                                  "read reply from " + context);
    if (reply != BuildPartialWriteResponse(1536)) {
        CloseFd(client_fd);
        Fail(context + ": server returned unexpected partial-write payload");
    }

    WriteAllToSocket(client_fd, "!", "write ack to " + context);
    CloseFd(client_fd);

    ExpectBackgroundProcessSuccess(run_process, 3000, "wait for " + context);
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

void ExpectReviewBoardRunOutput(std::string_view output,
                                std::string_view expected_line,
                                const std::string& context_prefix) {
    ExpectOutputContains(output,
                         expected_line,
                         context_prefix + ": should print the deterministic target result");
}

void ExpectReviewBoardTestOutput(std::string_view output,
                                 std::string_view target_name,
                                 const std::string& context_prefix) {
    const std::string target_text(target_name);
    ExpectOutputContains(output,
                         "testing target " + target_text,
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target " + target_text + ": 3 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS audit_pause_test.test_audit_pause",
                         context_prefix + ": should include audit pause coverage");
    ExpectOutputContains(output,
                         "PASS focus_threshold_test.test_focus_threshold",
                         context_prefix + ": should include focus-threshold coverage");
    ExpectOutputContains(output,
                         "PASS review_scan_test.test_review_scan",
                         context_prefix + ": should include shared scanner coverage");
    ExpectOutputContains(output,
                         "3 tests, 3 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target " + target_text + " (3 cases)",
                         context_prefix + ": should print the ordinary-test verdict");
    ExpectOutputContains(output,
                         "PASS target " + target_text,
                         context_prefix + ": should print the target verdict");
}

void ExpectEventedEchoTestOutput(std::string_view output,
                                 const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target echo",
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target echo: 3 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS parse_valid_port_test.test_parse_valid_port",
                         context_prefix + ": should include valid-port coverage");
    ExpectOutputContains(output,
                         "PASS parse_zero_port_test.test_parse_zero_port",
                         context_prefix + ": should include zero-port coverage");
    ExpectOutputContains(output,
                         "PASS parse_invalid_port_test.test_parse_invalid_port",
                         context_prefix + ": should include invalid-port coverage");
    ExpectOutputContains(output,
                         "3 tests, 3 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target echo (3 cases)",
                         context_prefix + ": should print the ordinary test verdict");
    ExpectOutputContains(output,
                         "PASS target echo",
                         context_prefix + ": should print the target verdict");
}

void ExpectPartialWriteTestOutput(std::string_view output,
                                  const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target partial-write",
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target partial-write: 2 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS fill_response_pattern_test.test_fill_response_pattern",
                         context_prefix + ": should include response-pattern coverage");
    ExpectOutputContains(output,
                         "PASS write_chunk_len_test.test_write_chunk_len",
                         context_prefix + ": should include write-window coverage");
    ExpectOutputContains(output,
                         "2 tests, 2 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target partial-write (2 cases)",
                         context_prefix + ": should print the ordinary test verdict");
    ExpectOutputContains(output,
                         "PASS target partial-write",
                         context_prefix + ": should print the target verdict");
}

std::string RunWorkerQueueProjectAndExpectSuccess(const std::filesystem::path& mc_path,
                                                  const std::filesystem::path& project_path,
                                                  const std::filesystem::path& build_dir,
                                                  std::string_view output_name,
                                                  const std::string& context) {
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "run",
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

void ExpectWorkerQueueRunOutput(std::string_view output,
                                const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "worker-queue-ok\n",
                         context_prefix + ": should print the deterministic success line");
}

void ExpectWorkerQueueTestOutput(std::string_view output,
                                 const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target worker-queue",
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target worker-queue: 2 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS expected_sum_test.test_expected_sum",
                         context_prefix + ": should include expected-sum coverage");
    ExpectOutputContains(output,
                         "PASS next_slot_test.test_next_slot",
                         context_prefix + ": should include slot-wrap coverage");
    ExpectOutputContains(output,
                         "2 tests, 2 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target worker-queue (2 cases)",
                         context_prefix + ": should print the ordinary test verdict");
    ExpectOutputContains(output,
                         "PASS target worker-queue",
                         context_prefix + ": should print the target verdict");
}

void ExerciseProjectRunTestRerunSequence(const std::filesystem::path& mc_path,
                                         const std::filesystem::path& project_path,
                                         const std::filesystem::path& build_dir,
                                         const std::function<void(uint16_t, std::string_view, const std::string&)>& run_round_trip,
                                         const std::function<void(std::string_view, const std::string&)>& verify_test_output,
                                         std::string_view first_run_output_name,
                                         std::string_view test_output_name,
                                         std::string_view second_run_output_name,
                                         const std::string& context_prefix) {
    run_round_trip(ReserveLoopbackPort(), first_run_output_name, context_prefix + " run before tests");
    const std::string test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                   project_path,
                                                                   build_dir,
                                                                   test_output_name,
                                                                   context_prefix + " test after run");
    verify_test_output(test_output, context_prefix + " test after run");
    run_round_trip(ReserveLoopbackPort(), second_run_output_name, context_prefix + " rerun after tests");
}

void ExerciseProjectTestRunRerunSequence(const std::filesystem::path& mc_path,
                                         const std::filesystem::path& project_path,
                                         const std::filesystem::path& build_dir,
                                         const std::function<void(uint16_t, std::string_view, const std::string&)>& run_round_trip,
                                         const std::function<void(std::string_view, const std::string&)>& verify_test_output,
                                         std::string_view first_test_output_name,
                                         std::string_view run_output_name,
                                         std::string_view second_test_output_name,
                                         const std::string& context_prefix) {
    const std::string first_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                         project_path,
                                                                         build_dir,
                                                                         first_test_output_name,
                                                                         context_prefix + " initial test");
    verify_test_output(first_test_output, context_prefix + " initial test");
    run_round_trip(ReserveLoopbackPort(), run_output_name, context_prefix + " run after tests");
    const std::string second_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                          project_path,
                                                                          build_dir,
                                                                          second_test_output_name,
                                                                          context_prefix + " retest after run");
    verify_test_output(second_test_output, context_prefix + " retest after run");
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
    ExpectOutputContains(output,
                         "ordinary tests target app: 2 cases, mode=checked, timeout=5000 ms",
                         "mc test should print the ordinary test scope");
    ExpectOutputContains(output, "PASS alpha_test.test_alpha_pass", "ordinary tests should report passing cases");
    ExpectOutputContains(output, "PASS beta_test.test_beta_pass", "ordinary tests should be discovered deterministically");
    ExpectOutputContains(output, "2 tests, 2 passed, 0 failed", "ordinary test summary should be printed");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target app (2 cases)",
                         "ordinary tests should print a target-scoped verdict");
    ExpectOutputContains(output,
                         "compiler regressions target app: 4 cases, timeout=5000 ms",
                         "compiler regressions should print their scope");
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
    ExpectOutputContains(output,
                         "PASS compiler regressions for target app (4 cases)",
                         "compiler regressions should print a target-scoped verdict");
    ExpectOutputContains(output,
                         "PASS target app",
                         "mc test should print the overall target verdict");
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

    ExpectOutputContains(output,
                         "ordinary tests target app: 1 cases, mode=checked, timeout=5000 ms",
                         "ordinary failure should still print the ordinary test scope");
    ExpectOutputContains(output, "FAIL failing_test.test_failure", "ordinary test failure should be reported");
    ExpectOutputContains(output, "1 tests, 0 passed, 1 failed", "ordinary test failure summary should be printed");
    ExpectOutputContains(output,
                         "FAIL ordinary tests for target app (1 cases)",
                         "ordinary failure should print a target-scoped verdict");
    ExpectOutputContains(output,
                         "FAIL target app",
                         "mc test should print the overall target failure verdict");
}

void TestMixedDirectSourceAndProjectOptionsFail(const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const std::filesystem::path source_path = binary_root / "phase19_mixed_mode.mc";
    WriteFile(source_path,
              "func main() i32 {\n"
              "    return 0\n"
              "}\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "check",
                                                      source_path.generic_string(),
                                                      "--target",
                                                      "app"},
                                                     binary_root / "phase19_mixed_mode_output.txt",
                                                     "phase19 mixed direct-source and project-mode check");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mixed direct-source and project-only options should fail");
    }
    ExpectOutputContains(output,
                         "cannot mix a direct source path with project-only options; choose direct-source mode or --project mode",
                         "mixed invocation should fail with a clear mode diagnostic");
}

void TestProjectTestRejectsDirectSourceInvocation(const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path source_path = binary_root / "phase19_direct_source_test_reject.mc";
    WriteFile(source_path,
              "func main() i32 {\n"
              "    return 0\n"
              "}\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      source_path.generic_string()},
                                                     binary_root / "phase19_direct_source_test_reject_output.txt",
                                                     "phase19 direct-source test rejection");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test should reject direct-source invocation");
    }
    ExpectOutputContains(output,
                         "mc test does not accept a direct source path; use --project <build.toml>",
                         "mc test should explain that it is project-only");
}

void TestUnknownTargetListsAvailableTargets(const std::filesystem::path& binary_root,
                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase19_unknown_target_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase19-unknown-target\"\n"
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
              "[targets.tool]\n"
              "kind = \"exe\"\n"
              "root = \"src/tool.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.tool.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.tool.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "src/tool.mc", "func main() i32 { return 0 }\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string(),
                                                      "--target",
                                                      "missing"},
                                                     binary_root / "phase19_unknown_target_output.txt",
                                                     "phase19 unknown target build");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("unknown target should fail project selection");
    }
    ExpectOutputContains(output,
                         "unknown target in project file: missing; available targets: app, tool",
                         "unknown target should list the available targets");
}

void TestDisabledTestTargetListsEnabledTargets(const std::filesystem::path& binary_root,
                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "phase19_disabled_test_target_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase19-disabled-test-target\"\n"
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
              "[targets.unit]\n"
              "kind = \"exe\"\n"
              "root = \"src/unit.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.unit.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.unit.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.unit.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 5000\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "src/unit.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "tests/sample_test.mc",
              "export { test_ok }\n"
              "\n"
              "func test_ok() *i32 {\n"
              "    return nil\n"
              "}\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string(),
                                                      "--target",
                                                      "app"},
                                                     binary_root / "phase19_disabled_test_target_output.txt",
                                                     "phase19 disabled test target selection");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test should fail when the selected target has tests disabled");
    }
    ExpectOutputContains(output,
                         "tests are not enabled for target 'app'; enabled test targets: unit",
                         "disabled test target diagnostic should list enabled test targets");
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
                         "ConstGlobal names=[helper.LIMIT] type=i32",
                         "merged project MIR should retain imported const globals");
    ExpectOutputContains(main_mir_text,
                         "init 9",
                         "merged project MIR should retain imported const global initializers");
    ExpectOutputContains(main_mir_text,
                         "VarGlobal names=[helper.counter] type=i32",
                         "merged project MIR should retain imported mutable globals");
    ExpectOutputContains(main_mir_text,
                         "init 4",
                         "merged project MIR should retain imported mutable global initializers");
    ExpectOutputContains(main_mir_text,
                         "store_target target=helper.counter target_kind=global target_name=helper.counter",
                         "merged project MIR should lower imported mutable global stores as global targets");
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
                         "project file does not declare a default target; available targets: first, second; pass --target <name>",
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
                         "ordinary tests target app: 1 cases, mode=checked, timeout=100 ms",
                         "ordinary timeout should still print the ordinary test scope");
    ExpectOutputContains(output,
                         "TIMEOUT ordinary tests for target app after 100 ms",
                         "ordinary test timeout should be reported deterministically");
    ExpectOutputContains(output,
                         "FAIL ordinary tests for target app (timeout)",
                         "ordinary timeout should print a target-scoped failure verdict");
    ExpectOutputContains(output,
                         "FAIL target app",
                         "mc test should print the overall target failure verdict on timeout");
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

void TestRealWorkerQueueProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path worker_queue_project_path = source_root / "examples/real/worker_queue/build.toml";
    const std::filesystem::path worker_queue_run_test_rerun_build_dir = binary_root / "phase20_worker_queue_run_test_rerun_build";
    std::filesystem::remove_all(worker_queue_run_test_rerun_build_dir);

    std::string worker_queue_run_output = RunWorkerQueueProjectAndExpectSuccess(mc_path,
                                                                                worker_queue_project_path,
                                                                                worker_queue_run_test_rerun_build_dir,
                                                                                "phase20_worker_queue_run_output.txt",
                                                                                "phase20 worker queue run before tests");
    ExpectWorkerQueueRunOutput(worker_queue_run_output,
                               "phase20 worker queue run before tests");

    std::string worker_queue_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                          worker_queue_project_path,
                                                                          worker_queue_run_test_rerun_build_dir,
                                                                          "phase20_worker_queue_test_output.txt",
                                                                          "phase20 worker queue test after run");
    ExpectWorkerQueueTestOutput(worker_queue_test_output,
                                "phase20 worker queue test after run");

    worker_queue_run_output = RunWorkerQueueProjectAndExpectSuccess(mc_path,
                                                                    worker_queue_project_path,
                                                                    worker_queue_run_test_rerun_build_dir,
                                                                    "phase20_worker_queue_rerun_output.txt",
                                                                    "phase20 worker queue rerun after tests");
    ExpectWorkerQueueRunOutput(worker_queue_run_output,
                               "phase20 worker queue rerun after tests");

    const std::filesystem::path worker_queue_test_run_rerun_build_dir = binary_root / "phase20_worker_queue_test_run_rerun_build";
    std::filesystem::remove_all(worker_queue_test_run_rerun_build_dir);

    worker_queue_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                              worker_queue_project_path,
                                                              worker_queue_test_run_rerun_build_dir,
                                                              "phase20_worker_queue_initial_test_output.txt",
                                                              "phase20 worker queue initial test");
    ExpectWorkerQueueTestOutput(worker_queue_test_output,
                                "phase20 worker queue initial test");

    worker_queue_run_output = RunWorkerQueueProjectAndExpectSuccess(mc_path,
                                                                    worker_queue_project_path,
                                                                    worker_queue_test_run_rerun_build_dir,
                                                                    "phase20_worker_queue_run_after_test_output.txt",
                                                                    "phase20 worker queue run after tests");
    ExpectWorkerQueueRunOutput(worker_queue_run_output,
                               "phase20 worker queue run after tests");

    worker_queue_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                              worker_queue_project_path,
                                                              worker_queue_test_run_rerun_build_dir,
                                                              "phase20_worker_queue_retest_output.txt",
                                                              "phase20 worker queue retest after run");
    ExpectWorkerQueueTestOutput(worker_queue_test_output,
                                "phase20 worker queue retest after run");
}

void TestRealEventedEchoProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/evented_echo/build.toml";
    const std::filesystem::path run_test_rerun_build_dir = binary_root / "phase19_evented_echo_run_test_rerun_build";
    std::filesystem::remove_all(run_test_rerun_build_dir);
    ExerciseProjectRunTestRerunSequence(mc_path,
                                        project_path,
                                        run_test_rerun_build_dir,
                                        [&](uint16_t port, std::string_view output_name, const std::string& context) {
                                            ExerciseEventedEchoRoundTrip(mc_path,
                                                                         project_path,
                                                                         run_test_rerun_build_dir,
                                                                         port,
                                                                         output_name,
                                                                         context);
                                        },
                                        ExpectEventedEchoTestOutput,
                                        "phase19_evented_echo_run_output.txt",
                                        "phase19_evented_echo_test_output.txt",
                                        "phase19_evented_echo_rerun_output.txt",
                                        "phase19 evented echo run-test-rerun");

    const std::filesystem::path test_run_rerun_build_dir = binary_root / "phase19_evented_echo_test_run_rerun_build";
    std::filesystem::remove_all(test_run_rerun_build_dir);
    ExerciseProjectTestRunRerunSequence(mc_path,
                                        project_path,
                                        test_run_rerun_build_dir,
                                        [&](uint16_t port, std::string_view output_name, const std::string& context) {
                                            ExerciseEventedEchoRoundTrip(mc_path,
                                                                         project_path,
                                                                         test_run_rerun_build_dir,
                                                                         port,
                                                                         output_name,
                                                                         context);
                                        },
                                        ExpectEventedEchoTestOutput,
                                        "phase19_evented_echo_initial_test_output.txt",
                                        "phase19_evented_echo_run_after_test_output.txt",
                                        "phase19_evented_echo_retest_output.txt",
                                        "phase19 evented echo test-run-rerun");

    const std::filesystem::path partial_write_project_path = source_root / "examples/real/evented_partial_write/build.toml";
    const std::filesystem::path partial_write_run_test_rerun_build_dir = binary_root / "phase19_evented_partial_write_run_test_rerun_build";
    std::filesystem::remove_all(partial_write_run_test_rerun_build_dir);
    ExerciseProjectRunTestRerunSequence(mc_path,
                                        partial_write_project_path,
                                        partial_write_run_test_rerun_build_dir,
                                        [&](uint16_t port, std::string_view output_name, const std::string& context) {
                                            ExercisePartialWriteRoundTrip(mc_path,
                                                                          partial_write_project_path,
                                                                          partial_write_run_test_rerun_build_dir,
                                                                          port,
                                                                          output_name,
                                                                          context);
                                        },
                                        ExpectPartialWriteTestOutput,
                                        "phase19_evented_partial_write_run_output.txt",
                                        "phase19_evented_partial_write_test_output.txt",
                                        "phase19_evented_partial_write_rerun_output.txt",
                                        "phase19 evented partial-write run-test-rerun");

    const std::filesystem::path partial_write_test_run_rerun_build_dir = binary_root / "phase19_evented_partial_write_test_run_rerun_build";
    std::filesystem::remove_all(partial_write_test_run_rerun_build_dir);
    ExerciseProjectTestRunRerunSequence(mc_path,
                                        partial_write_project_path,
                                        partial_write_test_run_rerun_build_dir,
                                        [&](uint16_t port, std::string_view output_name, const std::string& context) {
                                            ExercisePartialWriteRoundTrip(mc_path,
                                                                          partial_write_project_path,
                                                                          partial_write_test_run_rerun_build_dir,
                                                                          port,
                                                                          output_name,
                                                                          context);
                                        },
                                        ExpectPartialWriteTestOutput,
                                        "phase19_evented_partial_write_initial_test_output.txt",
                                        "phase19_evented_partial_write_run_after_test_output.txt",
                                        "phase19_evented_partial_write_retest_output.txt",
                                        "phase19 evented partial-write test-run-rerun");
}

void TestRealReviewBoardProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = source_root / "examples/real/review_board";
    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path sample_path = project_root / "tests/board_sample.txt";
    const std::filesystem::path graph_build_dir = binary_root / "phase22_review_board_graph_build";
    std::filesystem::remove_all(graph_build_dir);

    std::string audit_run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                                    project_path,
                                                                    graph_build_dir,
                                                                    "",
                                                                    sample_path,
                                                                    "phase22_review_board_audit_run_output.txt",
                                                                    "phase22 review board default-target run");
    ExpectReviewBoardRunOutput(audit_run_output,
                               "review-audit-pause\n",
                               "phase22 review board default-target run");

    std::string focus_run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                                    project_path,
                                                                    graph_build_dir,
                                                                    "focus",
                                                                    sample_path,
                                                                    "phase22_review_board_focus_run_output.txt",
                                                                    "phase22 review board explicit focus run");
    ExpectReviewBoardRunOutput(focus_run_output,
                               "review-focus-normal\n",
                               "phase22 review board explicit focus run");

    std::string audit_test_output = RunProjectTestTargetAndExpectSuccess(mc_path,
                                                                         project_path,
                                                                         graph_build_dir,
                                                                         "",
                                                                         "phase22_review_board_audit_test_output.txt",
                                                                         "phase22 review board default-target tests");
    ExpectReviewBoardTestOutput(audit_test_output,
                                "audit",
                                "phase22 review board default-target tests");

    std::string focus_test_output = RunProjectTestTargetAndExpectSuccess(mc_path,
                                                                         project_path,
                                                                         graph_build_dir,
                                                                         "focus",
                                                                         "phase22_review_board_focus_test_output.txt",
                                                                         "phase22 review board explicit focus tests");
    ExpectReviewBoardTestOutput(focus_test_output,
                                "focus",
                                "phase22 review board explicit focus tests");

    audit_run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                        project_path,
                                                        graph_build_dir,
                                                        "",
                                                        sample_path,
                                                        "phase22_review_board_audit_rerun_output.txt",
                                                        "phase22 review board default-target rerun");
    ExpectReviewBoardRunOutput(audit_run_output,
                               "review-audit-pause\n",
                               "phase22 review board default-target rerun");

    focus_run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                        project_path,
                                                        graph_build_dir,
                                                        "focus",
                                                        sample_path,
                                                        "phase22_review_board_focus_rerun_output.txt",
                                                        "phase22 review board explicit focus rerun");
    ExpectReviewBoardRunOutput(focus_run_output,
                               "review-focus-normal\n",
                               "phase22 review board explicit focus rerun");

    const std::filesystem::path cloned_project_root = binary_root / "phase22_review_board_clone";
    CopyDirectoryTree(project_root, cloned_project_root);
    WriteFile(cloned_project_root / "build.toml",
              "schema = 1\n"
              "project = \"review-board\"\n"
              "default = \"audit\"\n"
              "\n"
              "[targets.audit]\n"
              "kind = \"exe\"\n"
              "root = \"src/audit_main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.audit.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.audit.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.audit.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 5000\n"
              "\n"
              "[targets.focus]\n"
              "kind = \"exe\"\n"
              "root = \"src/focus_main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.focus.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.focus.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.focus.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 5000\n");
    const std::filesystem::path cloned_project_path = cloned_project_root / "build.toml";
    const std::filesystem::path cloned_sample_path = cloned_project_root / "tests/board_sample.txt";
    const std::filesystem::path rebuild_build_dir = binary_root / "phase22_review_board_rebuild_build";
    std::filesystem::remove_all(rebuild_build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "phase22_review_board_initial_audit_build.txt",
                                       "phase22 review board initial audit build");
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "focus",
                                       "phase22_review_board_initial_focus_build.txt",
                                       "phase22 review board initial focus build");

    const auto review_scan_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/review_scan.mc",
                                                                             rebuild_build_dir)
                                        .object;
    const auto review_status_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/review_status.mc",
                                                                               rebuild_build_dir)
                                          .object;
    const auto audit_main_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/audit_main.mc",
                                                                            rebuild_build_dir)
                                       .object;
    const auto focus_main_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/focus_main.mc",
                                                                            rebuild_build_dir)
                                       .object;
    const auto review_scan_mci = mc::support::ComputeDumpTargets(cloned_project_root / "src/review_scan.mc",
                                                                 rebuild_build_dir)
                                      .mci;
    const auto review_status_mci = mc::support::ComputeDumpTargets(cloned_project_root / "src/review_status.mc",
                                                                   rebuild_build_dir)
                                        .mci;
    const auto audit_executable = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/audit_main.mc",
                                                                           rebuild_build_dir)
                                      .executable;
    const auto focus_executable = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/focus_main.mc",
                                                                           rebuild_build_dir)
                                      .executable;

    const auto review_scan_object_time_1 = RequireWriteTime(review_scan_object);
    const auto review_status_object_time_1 = RequireWriteTime(review_status_object);
    const auto audit_main_object_time_1 = RequireWriteTime(audit_main_object);
    const auto focus_main_object_time_1 = RequireWriteTime(focus_main_object);
    const auto review_scan_mci_time_1 = RequireWriteTime(review_scan_mci);
    const auto review_status_mci_time_1 = RequireWriteTime(review_status_mci);
    const auto audit_executable_time_1 = RequireWriteTime(audit_executable);
    const auto focus_executable_time_1 = RequireWriteTime(focus_executable);

    SleepForTimestampTick();
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "phase22_review_board_noop_audit_build.txt",
                                       "phase22 review board no-op audit build");
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "focus",
                                       "phase22_review_board_noop_focus_build.txt",
                                       "phase22 review board no-op focus build");

    if (RequireWriteTime(review_scan_object) != review_scan_object_time_1 ||
        RequireWriteTime(review_status_object) != review_status_object_time_1 ||
        RequireWriteTime(audit_main_object) != audit_main_object_time_1 ||
        RequireWriteTime(focus_main_object) != focus_main_object_time_1 ||
        RequireWriteTime(review_scan_mci) != review_scan_mci_time_1 ||
        RequireWriteTime(review_status_mci) != review_status_mci_time_1 ||
        RequireWriteTime(audit_executable) != audit_executable_time_1 ||
        RequireWriteTime(focus_executable) != focus_executable_time_1) {
        Fail("phase22 no-op rebuild should reuse both target executables and all shared-module artifacts");
    }

    SleepForTimestampTick();
    WriteFile(cloned_project_root / "src/review_scan.mc",
              "export { count_closed_items, count_open_items, count_urgent_open_items }\n"
              "\n"
              "import strings\n"
              "\n"
              "func line_is_open(bytes: Slice<u8>, start: usize, newline: usize) bool {\n"
              "    if newline <= start {\n"
              "        return false\n"
              "    }\n"
              "    return bytes[start] == 79\n"
              "}\n"
              "\n"
              "func line_is_closed(bytes: Slice<u8>, start: usize, newline: usize) bool {\n"
              "    if newline <= start {\n"
              "        return false\n"
              "    }\n"
              "    return bytes[start] == 67\n"
              "}\n"
              "\n"
              "func line_is_urgent_open(bytes: Slice<u8>, start: usize, newline: usize) bool {\n"
              "    if newline <= start {\n"
              "        return false\n"
              "    }\n"
              "    if bytes[start] != 79 {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func count_open_items(text: str) usize {\n"
              "    bytes: Slice<u8> = strings.bytes(text)\n"
              "    count: usize = 0\n"
              "    start: usize = 0\n"
              "    while start < text.len {\n"
              "        newline: usize = start\n"
              "        while newline < text.len {\n"
              "            if bytes[newline] == 10 {\n"
              "                break\n"
              "            }\n"
              "            newline = newline + 1\n"
              "        }\n"
              "\n"
              "        if line_is_open(bytes, start, newline) {\n"
              "            count = count + 1\n"
              "        }\n"
              "\n"
              "        if newline == text.len {\n"
              "            break\n"
              "        }\n"
              "        start = newline + 1\n"
              "    }\n"
              "    return count\n"
              "}\n"
              "\n"
              "func count_closed_items(text: str) usize {\n"
              "    bytes: Slice<u8> = strings.bytes(text)\n"
              "    count: usize = 0\n"
              "    start: usize = 0\n"
              "    while start < text.len {\n"
              "        newline: usize = start\n"
              "        while newline < text.len {\n"
              "            if bytes[newline] == 10 {\n"
              "                break\n"
              "            }\n"
              "            newline = newline + 1\n"
              "        }\n"
              "\n"
              "        if line_is_closed(bytes, start, newline) {\n"
              "            count = count + 1\n"
              "        }\n"
              "\n"
              "        if newline == text.len {\n"
              "            break\n"
              "        }\n"
              "        start = newline + 1\n"
              "    }\n"
              "    return count\n"
              "}\n"
              "\n"
              "func count_urgent_open_items(text: str) usize {\n"
              "    bytes: Slice<u8> = strings.bytes(text)\n"
              "    count: usize = 0\n"
              "    start: usize = 0\n"
              "    while start < text.len {\n"
              "        newline: usize = start\n"
              "        while newline < text.len {\n"
              "            if bytes[newline] == 10 {\n"
              "                break\n"
              "            }\n"
              "            newline = newline + 1\n"
              "        }\n"
              "\n"
              "        if line_is_urgent_open(bytes, start, newline) {\n"
              "            count = count + 1\n"
              "        }\n"
              "\n"
              "        if newline == text.len {\n"
              "            break\n"
              "        }\n"
              "        start = newline + 1\n"
              "    }\n"
              "    return count\n"
              "}\n");

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "phase22_review_board_impl_audit_build.txt",
                                       "phase22 review board implementation-only audit rebuild");

    const auto review_scan_object_time_2 = RequireWriteTime(review_scan_object);
    const auto review_status_object_time_2 = RequireWriteTime(review_status_object);
    const auto audit_main_object_time_2 = RequireWriteTime(audit_main_object);
    const auto focus_main_object_time_2 = RequireWriteTime(focus_main_object);
    const auto review_scan_mci_time_2 = RequireWriteTime(review_scan_mci);
    const auto review_status_mci_time_2 = RequireWriteTime(review_status_mci);
    const auto audit_executable_time_2 = RequireWriteTime(audit_executable);

    if (!(review_scan_object_time_2 > review_scan_object_time_1)) {
        Fail("phase22 implementation-only shared scan edit should rebuild the deep shared object");
    }
    if (review_status_object_time_2 != review_status_object_time_1) {
        Fail("phase22 implementation-only shared scan edit should not rebuild the dependent shared status object");
    }
    if (audit_main_object_time_2 != audit_main_object_time_1) {
        Fail("phase22 implementation-only shared scan edit should not rebuild the default-target main object");
    }
    if (focus_main_object_time_2 != focus_main_object_time_1) {
        Fail("phase22 implementation-only shared scan edit should not rebuild the explicit-target main object");
    }
    if (review_scan_mci_time_2 != review_scan_mci_time_1) {
        Fail("phase22 implementation-only shared scan edit should preserve the deep shared interface artifact");
    }
    if (review_status_mci_time_2 != review_status_mci_time_1) {
        Fail("phase22 implementation-only shared scan edit should preserve the dependent shared interface artifact");
    }
    if (!(audit_executable_time_2 > audit_executable_time_1)) {
        Fail("phase22 implementation-only shared scan edit should relink the default-target executable");
    }
    if (RequireWriteTime(focus_executable) != focus_executable_time_1) {
        Fail("phase22 implementation-only audit rebuild should not touch the explicit-target executable before it is selected");
    }

    SleepForTimestampTick();
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "focus",
                                       "phase22_review_board_impl_focus_build.txt",
                                       "phase22 review board implementation-only focus rebuild");

    if (RequireWriteTime(review_scan_object) != review_scan_object_time_2) {
        Fail("phase22 implementation-only focus rebuild should reuse the already rebuilt deep shared object");
    }
    if (RequireWriteTime(review_status_object) != review_status_object_time_1) {
        Fail("phase22 implementation-only focus rebuild should still reuse the dependent shared status object");
    }
    if (RequireWriteTime(focus_main_object) != focus_main_object_time_1) {
        Fail("phase22 implementation-only focus rebuild should reuse the explicit-target main object");
    }
    if (!(RequireWriteTime(focus_executable) > focus_executable_time_1)) {
        Fail("phase22 implementation-only focus rebuild should relink the explicit-target executable");
    }

    const auto [impl_focus_outcome, impl_focus_output] = RunCommandCapture({focus_executable.generic_string(),
                                                                            cloned_sample_path.generic_string()},
                                                                           rebuild_build_dir /
                                                                               "phase22_review_board_impl_focus_run.txt",
                                                                           "phase22 review board implementation-only focus executable run");
    if (!impl_focus_outcome.exited || impl_focus_outcome.exit_code != 0) {
        Fail("phase22 implementation-only focus executable run should pass:\n" + impl_focus_output);
    }
    ExpectReviewBoardRunOutput(impl_focus_output,
                               "review-focus-escalate\n",
                               "phase22 review board implementation-only focus executable run");

    const auto focus_executable_time_2 = RequireWriteTime(focus_executable);

    SleepForTimestampTick();
    WriteFile(cloned_project_root / "src/review_status.mc",
              "export { audit_should_pause_text, focus_needs_escalation_text, helper_version, run_audit, run_focus }\n"
              "\n"
              "import fs\n"
              "import io\n"
              "import mem\n"
              "import review_scan\n"
              "\n"
              "func buffer_text(buf: *Buffer<u8>) str {\n"
              "    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)\n"
              "    return str{ ptr: bytes.ptr, len: bytes.len }\n"
              "}\n"
              "\n"
              "func load_review_text(path: str) *Buffer<u8> {\n"
              "    return fs.read_all(path, mem.default_allocator())\n"
              "}\n"
              "\n"
              "func audit_should_pause_text(text: str) bool {\n"
              "    return review_scan.count_open_items(text) > review_scan.count_closed_items(text)\n"
              "}\n"
              "\n"
              "func focus_needs_escalation_text(text: str) bool {\n"
              "    return review_scan.count_urgent_open_items(text) > 2\n"
              "}\n"
              "\n"
              "func helper_version() i32 {\n"
              "    return 22\n"
              "}\n"
              "\n"
              "func run_audit(path: str) i32 {\n"
              "    text_buf: *Buffer<u8> = load_review_text(path)\n"
              "    if text_buf == nil {\n"
              "        return 91\n"
              "    }\n"
              "    defer mem.buffer_free<u8>(text_buf)\n"
              "\n"
              "    if audit_should_pause_text(buffer_text(text_buf)) {\n"
              "        if io.write_line(\"review-audit-pause\") != 0 {\n"
              "            return 92\n"
              "        }\n"
              "        return 0\n"
              "    }\n"
              "\n"
              "    if io.write_line(\"review-audit-stable\") != 0 {\n"
              "        return 93\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func run_focus(path: str) i32 {\n"
              "    text_buf: *Buffer<u8> = load_review_text(path)\n"
              "    if text_buf == nil {\n"
              "        return 94\n"
              "    }\n"
              "    defer mem.buffer_free<u8>(text_buf)\n"
              "\n"
              "    if focus_needs_escalation_text(buffer_text(text_buf)) {\n"
              "        if io.write_line(\"review-focus-escalate\") != 0 {\n"
              "            return 95\n"
              "        }\n"
              "        return 0\n"
              "    }\n"
              "\n"
              "    if io.write_line(\"review-focus-normal\") != 0 {\n"
              "        return 96\n"
              "    }\n"
              "    return 0\n"
              "}\n");

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "phase22_review_board_interface_audit_build.txt",
                                       "phase22 review board interface-changing audit rebuild");

    const auto review_status_object_time_3 = RequireWriteTime(review_status_object);
    const auto audit_main_object_time_3 = RequireWriteTime(audit_main_object);
    const auto focus_main_object_time_3 = RequireWriteTime(focus_main_object);
    const auto review_status_mci_time_3 = RequireWriteTime(review_status_mci);
    const auto audit_executable_time_3 = RequireWriteTime(audit_executable);

    if (RequireWriteTime(review_scan_object) != review_scan_object_time_2) {
        Fail("phase22 interface-changing status edit should reuse the deep shared scan object");
    }
    if (!(review_status_object_time_3 > review_status_object_time_1)) {
        Fail("phase22 interface-changing status edit should rebuild the direct shared status object");
    }
    if (!(audit_main_object_time_3 > audit_main_object_time_1)) {
        Fail("phase22 interface-changing status edit should rebuild the default-target main object");
    }
    if (focus_main_object_time_3 != focus_main_object_time_1) {
        Fail("phase22 interface-changing audit rebuild should defer the explicit-target main rebuild until that target is selected");
    }
    if (!(review_status_mci_time_3 > review_status_mci_time_1)) {
        Fail("phase22 interface-changing status edit should rewrite the shared status interface artifact");
    }
    if (!(audit_executable_time_3 > audit_executable_time_2)) {
        Fail("phase22 interface-changing status edit should relink the default-target executable");
    }
    if (RequireWriteTime(focus_executable) != focus_executable_time_2) {
        Fail("phase22 interface-changing audit rebuild should not touch the explicit-target executable before it is selected");
    }

    SleepForTimestampTick();
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "focus",
                                       "phase22_review_board_interface_focus_build.txt",
                                       "phase22 review board interface-changing focus rebuild");

    if (RequireWriteTime(review_status_object) != review_status_object_time_3) {
        Fail("phase22 interface-changing focus rebuild should reuse the already rebuilt direct shared status object");
    }
    if (!(RequireWriteTime(focus_main_object) > focus_main_object_time_1)) {
        Fail("phase22 interface-changing focus rebuild should rebuild the explicit-target main object");
    }
    if (!(RequireWriteTime(focus_executable) > focus_executable_time_2)) {
        Fail("phase22 interface-changing focus rebuild should relink the explicit-target executable");
    }
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
    TestMixedDirectSourceAndProjectOptionsFail(binary_root, mc_path);
    TestProjectTestRejectsDirectSourceInvocation(binary_root, mc_path);
    TestUnknownTargetListsAvailableTargets(binary_root, mc_path);
    TestDisabledTestTargetListsEnabledTargets(binary_root, mc_path);
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
    TestRealWorkerQueueProject(source_root, binary_root, mc_path);
    TestRealEventedEchoProject(source_root, binary_root, mc_path);
    TestRealReviewBoardProject(source_root, binary_root, mc_path);
    return 0;
}
