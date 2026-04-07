#include <cstdlib>
#include <cerrno>
#include <chrono>
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
#include <poll.h>
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

void CloseFd(int fd) {
    if (fd >= 0) {
        close(fd);
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

int CreateLoopbackListener(uint16_t* out_port) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        Fail("unable to create loopback listener socket");
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
        Fail("unable to bind loopback listener: errno=" + std::to_string(saved_errno));
    }
    if (listen(fd, 8) != 0) {
        const int saved_errno = errno;
        CloseFd(fd);
        Fail("unable to listen on loopback listener: errno=" + std::to_string(saved_errno));
    }

    sockaddr_in bound_addr {};
    socklen_t bound_len = sizeof(bound_addr);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&bound_addr), &bound_len) != 0) {
        const int saved_errno = errno;
        CloseFd(fd);
        Fail("unable to read loopback listener port: errno=" + std::to_string(saved_errno));
    }

    *out_port = ntohs(bound_addr.sin_port);
    return fd;
}

BackgroundProcess SpawnBackgroundExecutable(const std::filesystem::path& executable,
                                           const std::vector<std::string>& args,
                                           const std::filesystem::path& output_path,
                                           const std::string& context) {
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

        std::vector<std::string> argv_storage;
        argv_storage.reserve(args.size() + 1);
        argv_storage.push_back(executable.generic_string());
        argv_storage.insert(argv_storage.end(), args.begin(), args.end());

        std::vector<char*> argv_ptrs;
        argv_ptrs.reserve(argv_storage.size() + 1);
        for (std::string& value : argv_storage) {
            argv_ptrs.push_back(value.data());
        }
        argv_ptrs.push_back(nullptr);

        execv(executable.c_str(), argv_ptrs.data());
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

int AcceptLoopbackClient(int listener_fd,
                         int timeout_ms,
                         const std::string& context) {
    pollfd poll_entry {};
    poll_entry.fd = listener_fd;
    poll_entry.events = POLLIN;
    const int ready = poll(&poll_entry, 1, timeout_ms);
    if (ready <= 0) {
        Fail(context + ": timed out waiting for client connection");
    }

    const int fd = accept(listener_fd, nullptr, nullptr);
    if (fd < 0) {
        const int saved_errno = errno;
        Fail(context + ": accept failed: errno=" + std::to_string(saved_errno));
    }

    timeval timeout {};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    return fd;
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

void RunBuiltNetworkEchoServerFixture(const std::filesystem::path& mc_path,
                                      const std::filesystem::path& source_path,
                                      const std::filesystem::path& build_dir,
                                      std::string_view request,
                                      std::string_view expected_reply) {
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    ExpectCommandSuccess({mc_path.generic_string(),
                          "build",
                          source_path.generic_string(),
                          "--build-dir",
                          build_dir.generic_string()},
                         "mc build " + source_path.generic_string());

    const auto artifacts = mc::support::ComputeBuildArtifactTargets(source_path, build_dir);
    const uint16_t port = ReserveLoopbackPort();
    const BackgroundProcess process = SpawnBackgroundExecutable(
        artifacts.executable,
        {std::to_string(port)},
        build_dir / "server_output.txt",
        "spawn background network echo server");

    const int client_fd = ConnectLoopbackWithRetry(port, 2000, "connect to built network echo server");
    WriteAllToSocket(client_fd, request, "write request to built network echo server");
    const std::string reply = ReadExactFromSocket(client_fd, expected_reply.size(), "read reply from built network echo server");
    CloseFd(client_fd);

    if (reply != expected_reply) {
        Fail("network echo server returned unexpected payload: '" + reply + "'");
    }

    ExpectBackgroundProcessSuccess(process, 2000, "wait for built network echo server");
}

void RunBuiltPartialWriteServerFixture(const std::filesystem::path& mc_path,
                                       const std::filesystem::path& source_path,
                                       const std::filesystem::path& build_dir,
                                       std::string_view request,
                                       std::string_view ack,
                                       std::string_view expected_reply) {
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    ExpectCommandSuccess({mc_path.generic_string(),
                          "build",
                          source_path.generic_string(),
                          "--build-dir",
                          build_dir.generic_string()},
                         "mc build " + source_path.generic_string());

    const auto artifacts = mc::support::ComputeBuildArtifactTargets(source_path, build_dir);
    const uint16_t port = ReserveLoopbackPort();
    const BackgroundProcess process = SpawnBackgroundExecutable(
        artifacts.executable,
        {std::to_string(port)},
        build_dir / "partial_write_server_output.txt",
        "spawn background partial-write server");

    const int client_fd = ConnectLoopbackWithRetry(port, 2000, "connect to built partial-write server");
    WriteAllToSocket(client_fd, request, "write request to built partial-write server");
    const std::string reply = ReadExactFromSocket(client_fd,
                                                  expected_reply.size(),
                                                  "read reply from built partial-write server");
    if (reply != expected_reply) {
        CloseFd(client_fd);
        Fail("partial-write server returned unexpected payload");
    }

    WriteAllToSocket(client_fd, ack, "write ack to built partial-write server");
    CloseFd(client_fd);

    ExpectBackgroundProcessSuccess(process, 2000, "wait for built partial-write server");
}

void RunBuiltNetworkClientFixture(const std::filesystem::path& mc_path,
                                  const std::filesystem::path& source_path,
                                  const std::filesystem::path& build_dir,
                                  std::string_view expected_request,
                                  std::string_view response) {
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    ExpectCommandSuccess({mc_path.generic_string(),
                          "build",
                          source_path.generic_string(),
                          "--build-dir",
                          build_dir.generic_string()},
                         "mc build " + source_path.generic_string());

    const auto artifacts = mc::support::ComputeBuildArtifactTargets(source_path, build_dir);
    uint16_t port = 0;
    const int listener_fd = CreateLoopbackListener(&port);

    const BackgroundProcess process = SpawnBackgroundExecutable(
        artifacts.executable,
        {std::to_string(port)},
        build_dir / "client_output.txt",
        "spawn background network client");

    const int conn_fd = AcceptLoopbackClient(listener_fd, 2000, "accept built network client");
    CloseFd(listener_fd);

    const std::string request = ReadExactFromSocket(conn_fd, expected_request.size(), "read request from built network client");
    if (request != expected_request) {
        CloseFd(conn_fd);
        Fail("network client sent unexpected payload: '" + request + "'");
    }

    WriteAllToSocket(conn_fd, response, "write response to built network client");
    CloseFd(conn_fd);

    ExpectBackgroundProcessSuccess(process, 2000, "wait for built network client");
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

void RunBuiltFixtureWithIrSnippets(const std::filesystem::path& mc_path,
                                   const std::filesystem::path& source_path,
                                   const std::filesystem::path& build_dir,
                                   int expected_exit_code,
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
        Fail("expected build artifacts for fixture: " + source_path.generic_string());
    }

    const std::string llvm_ir = ReadFile(artifacts.llvm_ir);
    for (const auto& snippet : required_ir_snippets) {
        if (llvm_ir.find(snippet) == std::string::npos) {
            Fail("expected LLVM IR snippet '" + snippet + "' in " + artifacts.llvm_ir.generic_string());
        }
    }

    ExpectExecutableExit(artifacts.executable,
                         run_args,
                         expected_exit_code,
                         "run built executable " + artifacts.executable.generic_string());
}

void RunBuiltIrFixtureWithSnippets(const std::filesystem::path& mc_path,
                                   const std::filesystem::path& source_path,
                                   const std::filesystem::path& build_dir,
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
        Fail("expected build artifacts for fixture: " + source_path.generic_string());
    }

    const std::string llvm_ir = ReadFile(artifacts.llvm_ir);
    for (const auto& snippet : required_ir_snippets) {
        if (llvm_ir.find(snippet) == std::string::npos) {
            Fail("expected LLVM IR snippet '" + snippet + "' in " + artifacts.llvm_ir.generic_string());
        }
    }
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

void RunBuiltProjectNetworkEchoFixture(const std::filesystem::path& mc_path,
                                       const std::filesystem::path& project_path,
                                       const std::filesystem::path& root_source_path,
                                       const std::filesystem::path& build_dir,
                                       std::string_view request,
                                       std::string_view expected_reply) {
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
    const uint16_t port = ReserveLoopbackPort();
    const BackgroundProcess process = SpawnBackgroundExecutable(
        artifacts.executable,
        {std::to_string(port)},
        build_dir / "project_server_output.txt",
        "spawn built project network echo server");

    const int client_fd = ConnectLoopbackWithRetry(port, 2000, "connect to built project network echo server");
    WriteAllToSocket(client_fd, request, "write request to built project network echo server");
    const std::string reply = ReadExactFromSocket(client_fd, expected_reply.size(), "read reply from built project network echo server");
    CloseFd(client_fd);

    if (reply != expected_reply) {
        Fail("project network echo server returned unexpected payload: '" + reply + "'");
    }

    ExpectBackgroundProcessSuccess(process, 2000, "wait for built project network echo server");
}

void RunBuiltProjectPartialWriteFixture(const std::filesystem::path& mc_path,
                                        const std::filesystem::path& project_path,
                                        const std::filesystem::path& root_source_path,
                                        const std::filesystem::path& build_dir,
                                        std::string_view request,
                                        std::string_view ack,
                                        std::string_view expected_reply) {
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
    const uint16_t port = ReserveLoopbackPort();
    const BackgroundProcess process = SpawnBackgroundExecutable(
        artifacts.executable,
        {std::to_string(port)},
        build_dir / "project_partial_write_output.txt",
        "spawn built project partial-write server");

    const int client_fd = ConnectLoopbackWithRetry(port, 2000, "connect to built project partial-write server");
    WriteAllToSocket(client_fd, request, "write request to built project partial-write server");
    const std::string reply = ReadExactFromSocket(client_fd,
                                                  expected_reply.size(),
                                                  "read reply from built project partial-write server");
    if (reply != expected_reply) {
        CloseFd(client_fd);
        Fail("project partial-write server returned unexpected payload");
    }

    WriteAllToSocket(client_fd, ack, "write ack to built project partial-write server");
    CloseFd(client_fd);

    ExpectBackgroundProcessSuccess(process, 2000, "wait for built project partial-write server");
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

    const std::filesystem::path field_address_source = work_root / "field_address_local.mc";
    WriteFile(field_address_source,
              "struct Pair {\n"
              "    left: i32,\n"
              "    right: i32,\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    pair: Pair = Pair{ left: 3, right: 7 }\n"
              "    right_ptr: *i32 = &pair.right\n"
              "    *right_ptr = 19\n"
              "    return pair.left + pair.right\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    field_address_source,
                    work_root / "field_address_local_build",
                    22,
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
                           "requires supported constant MemoryOrder metadata for 'atomic_load'");

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

    const std::filesystem::path hosted_args_roundtrip_source = work_root / "hosted_main_args_roundtrip.mc";
    WriteFile(hosted_args_roundtrip_source,
              "import io\n"
              "\n"
              "func main(args: Slice<cstr>) i32 {\n"
              "    if args.len != 4 {\n"
              "        return 10\n"
              "    }\n"
              "    if args[1].len != 0 {\n"
              "        return 11\n"
              "    }\n"
              "    if io.write_line(args[2]) != 0 {\n"
              "        return 12\n"
              "    }\n"
              "    if io.write_line(args[3]) != 0 {\n"
              "        return 13\n"
              "    }\n"
              "    return 0\n"
              "}\n");
    RunBuiltOutputFixture(mc_path,
                          hosted_args_roundtrip_source,
                          work_root / "hosted_main_args_roundtrip_build",
                          {"", "two words", "tail"},
                          0,
                          "two words\ntail\n");

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
                          source_root / "tests/stdlib/fmt_smoke.mc",
                          work_root / "phase6_fmt_smoke_build",
                          {},
                          0,
                          "fmt");

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
                    source_root / "tests/stdlib/array_local_store_roundtrip.mc",
                    work_root / "phase12_array_local_store_roundtrip_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/string_helpers.mc",
                    work_root / "phase12_string_helpers_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/utf8_byte_len.mc",
                    work_root / "phase6_utf8_byte_len_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/utf8_codepoint_len.mc",
                    work_root / "phase12_utf8_codepoint_len_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/utf8_valid.mc",
                    work_root / "phase6_utf8_valid_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/utf8_valid_bytes.mc",
                    work_root / "phase12_utf8_valid_bytes_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/utf8_invalid_bytes.mc",
                    work_root / "phase6_utf8_invalid_bytes_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/utf8_decode_encode.mc",
                    work_root / "phase12_utf8_decode_encode_build",
                    0,
                    {});

    const std::filesystem::path phase6_file_input = work_root / "phase6_file_input.txt";
    WriteFile(phase6_file_input, "phase6\n");

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/file_size.mc",
                    work_root / "phase6_file_size_build",
                    0,
                    {phase6_file_input.generic_string()});

    const std::filesystem::path phase6_dir_input = work_root / "phase6_dir_input";
    std::filesystem::create_directories(phase6_dir_input / "nested");
    WriteFile(phase6_dir_input / "alpha.txt", "a\n");
    WriteFile(phase6_dir_input / "zeta.txt", "z\n");

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/is_dir_kind.mc",
                    work_root / "phase6_is_dir_kind_build",
                    0,
                    {phase6_dir_input.generic_string(), phase6_file_input.generic_string()});

    RunBuiltOutputFixture(mc_path,
                          source_root / "tests/stdlib/list_dir_stdout.mc",
                          work_root / "phase6_list_dir_stdout_build",
                          {phase6_dir_input.generic_string()},
                          0,
                          "alpha.txt\nnested/\nzeta.txt\n");

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/file_read_len.mc",
                    work_root / "phase6_file_read_len_build",
                    7,
                    {phase6_file_input.generic_string()});

    RunBuiltFixture(mc_path,
                    source_root / "tests/stdlib/file_read_default_allocator_len.mc",
                    work_root / "phase6_file_read_default_allocator_len_build",
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

    RunBuiltNetworkEchoServerFixture(mc_path,
                                     source_root / "tests/stdlib/poller_echo_server.mc",
                                     work_root / "phase12_poller_echo_server_build",
                                     "hello",
                                     "hello");

    RunBuiltNetworkClientFixture(mc_path,
                                 source_root / "tests/stdlib/poller_connect_roundtrip.mc",
                                 work_root / "phase12_poller_connect_roundtrip_build",
                                 "ping",
                                 "pong");

    RunBuiltPartialWriteServerFixture(mc_path,
                                      source_root / "tests/stdlib/poller_partial_write_response.mc",
                                      work_root / "phase16_poller_partial_write_response_build",
                                      "push",
                                      "!",
                                      BuildPartialWriteResponse(1536));

    RunBuiltOutputFixture(mc_path,
                          source_root / "tests/stdlib/subprocess_tr_capture.mc",
                          work_root / "phase45_subprocess_tr_capture_build",
                          {},
                          0,
                          "SUBPROCESS SMOKE\n");

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

    RunBuiltProjectNetworkEchoFixture(mc_path,
                                      source_root / "examples/real/evented_echo/build.toml",
                                      source_root / "examples/real/evented_echo/src/main.mc",
                                      work_root / "phase14_evented_echo_project_build",
                                      "hello",
                                      "hello");

    RunBuiltProjectPartialWriteFixture(mc_path,
                                       source_root / "examples/real/evented_partial_write/build.toml",
                                       source_root / "examples/real/evented_partial_write/src/main.mc",
                                       work_root / "phase16_evented_partial_write_project_build",
                                       "push",
                                       "!",
                                       BuildPartialWriteResponse(1536));

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

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/fixed_array_slice_buffer_conversion.mc",
                    work_root / "phase15_fixed_array_slice_buffer_conversion_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/hosted_default_allocator_use.mc",
                    work_root / "phase15_hosted_default_allocator_use_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/bounded_ring_buffer.mc",
                    work_root / "phase15_bounded_ring_buffer_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/codegen/field_address_local_global.mc",
                    work_root / "phase15_field_address_local_global_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "tests/codegen/bool_struct_fields.mc",
                    work_root / "phase21_bool_struct_fields_build",
                    0,
                    {});

    RunBuiltFixtureWithIrSnippets(mc_path,
                                  source_root / "examples/canonical/shared_counter_mutex.mc",
                                  work_root / "phase15_shared_counter_mutex_build",
                                  0,
                                  {},
                                  {"define {{i64}, i64} @sync.thread_spawn(",
                                   "call {{i64}, i64} @sync.thread_spawn("});

    const std::filesystem::path deferred_thread_spawn_source = work_root / "phase51_deferred_imported_thread_spawn.mc";
    WriteFile(deferred_thread_spawn_source,
              "import sync\n"
              "\n"
              "var worker_status: i32 = 0\n"
              "\n"
              "func worker(ctx: *i32) {\n"
              "    ignored: uintptr = (uintptr)(ctx)\n"
              "    if ignored == 0 {\n"
              "        worker_status = 1\n"
              "    }\n"
              "}\n"
              "\n"
              "func launch() {\n"
              "    defer sync.thread_spawn<i32>(worker, &worker_status)\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    launch()\n"
              "    return 0\n"
              "}\n");
    RunBuiltIrFixtureWithSnippets(mc_path,
                                  deferred_thread_spawn_source,
                                  work_root / "phase51_deferred_imported_thread_spawn_build",
                                  {"define {{i64}, i64} @sync.thread_spawn(",
                                   "call {{i64}, i64} @sync.thread_spawn("});

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/producer_consumer_condvar.mc",
                    work_root / "phase15_producer_consumer_condvar_build",
                    0,
                    {});

    RunBuiltFixture(mc_path,
                    source_root / "examples/canonical/atomic_flag_publication.mc",
                    work_root / "phase15_atomic_flag_publication_build",
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

    const std::filesystem::path tuple_return_source = work_root / "tuple_return_roundtrip.mc";
    WriteFile(tuple_return_source,
              "func pair() (i32, i32) {\n"
              "    return 4, 6\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    left: i32 = 0\n"
              "    right: i32 = 0\n"
              "    left, right = pair()\n"
              "    return left + right\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    tuple_return_source,
                    work_root / "phase27_tuple_return_roundtrip_build",
                    10,
                    {});

    const std::filesystem::path range_loop_source = work_root / "range_loop_roundtrip.mc";
    WriteFile(range_loop_source,
              "func main() i32 {\n"
              "    total: i32 = 0\n"
              "    for idx in 0..5 {\n"
              "        total = total + idx\n"
              "    }\n"
              "    return total\n"
              "}\n");
    RunBuiltFixture(mc_path,
                    range_loop_source,
                    work_root / "phase27_range_loop_roundtrip_build",
                    10,
                    {});

    const std::filesystem::path noalias_param_source = work_root / "noalias_param_ir.mc";
    WriteFile(noalias_param_source,
              "func load(@noalias ptr: *i32) i32 {\n"
              "    return *ptr\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    value: i32 = 7\n"
              "    return load(&value)\n"
              "}\n");
    RunBuiltFixtureWithIrSnippets(mc_path,
                                  noalias_param_source,
                                  work_root / "phase26_noalias_param_ir_build",
                                  7,
                                  {},
                                  {"define i32 @load(ptr noalias %arg.ptr)", "call i32 @load(ptr %local.value)"});

    return 0;
}
