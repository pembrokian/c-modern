#include "tests/support/process_utils.h"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <poll.h>
#include <sstream>
#include <thread>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tests/support/fixture_utils.h"

namespace mc::test_support {

namespace {

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

std::filesystem::path TimestampTickProbePath() {
    return std::filesystem::current_path() / ".mc_timestamp_tick_probe";
}

std::filesystem::file_time_type TouchTimestampTickProbe(const std::filesystem::path& probe_path,
                                                        std::uint64_t generation) {
    std::ofstream stream(probe_path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        Fail("failed to open timestamp probe file: " + probe_path.generic_string());
    }
    stream << generation << '\n';
    stream.close();
    if (!stream) {
        Fail("failed to write timestamp probe file: " + probe_path.generic_string());
    }
    return std::filesystem::last_write_time(probe_path);
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

std::string SummarizeTextForFailure(std::string_view text) {
    constexpr std::size_t kMaxPreview = 320;
    constexpr std::size_t kHeadPreview = 200;
    constexpr std::size_t kTailPreview = 80;

    if (text.size() <= kMaxPreview) {
        return std::string(text);
    }

    const std::size_t truncated_chars = text.size() - (kHeadPreview + kTailPreview);
    return std::string(text.substr(0, kHeadPreview)) +
           " ...<truncated " + std::to_string(truncated_chars) + " chars>... " +
           std::string(text.substr(text.size() - kTailPreview));
}

void SetSocketTimeout(int fd,
                      int timeout_ms,
                      const std::string& context) {
    timeval timeout {};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        const int saved_errno = errno;
        Fail(context + ": unable to configure socket timeout: errno=" + std::to_string(saved_errno));
    }
}

}  // namespace

std::string ReadFile(const std::filesystem::path& path) {
    return ReadTextFile(path);
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

std::pair<CommandOutcome, std::string> RunCommandCapture(
    const std::vector<std::string>& args,
    const std::filesystem::path& output_path,
    const std::string& context) {
    std::filesystem::create_directories(output_path.parent_path());
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

void ExpectOutputContains(std::string_view output,
                          std::string_view needle,
                          const std::string& context) {
    if (output.find(needle) == std::string::npos) {
        Fail(context + ": expected output to contain '" + std::string(needle) +
             "', output preview '" + SummarizeTextForFailure(output) +
             "' (length=" + std::to_string(output.size()) + ")");
    }
}

void SleepForTimestampTick() {
    const std::filesystem::path probe_path = TimestampTickProbePath();
    std::uint64_t generation = 0;
    std::filesystem::file_time_type baseline{};
    if (std::filesystem::exists(probe_path)) {
        baseline = std::filesystem::last_write_time(probe_path);
    } else {
        baseline = TouchTimestampTickProbe(probe_path, generation);
        ++generation;
    }

    for (int attempt = 0; attempt < 200; ++attempt) {
        const auto next_time = TouchTimestampTickProbe(probe_path, generation);
        if (next_time > baseline) {
            return;
        }
        ++generation;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Fail("timed out waiting for filesystem timestamp tick at " + probe_path.generic_string());
}

std::filesystem::file_time_type RequireWriteTime(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        Fail("expected file to exist: " + path.generic_string());
    }
    return std::filesystem::last_write_time(path);
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

BackgroundProcess SpawnBackgroundExecutable(const std::filesystem::path& executable,
                                            const std::vector<std::string>& args,
                                            const std::filesystem::path& output_path,
                                            const std::string& context) {
    std::vector<std::string> command;
    command.reserve(args.size() + 1);
    command.push_back(executable.generic_string());
    command.insert(command.end(), args.begin(), args.end());
    return SpawnBackgroundCommand(command, output_path, context);
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

int ConnectLoopbackWithRetry(uint16_t port,
                             int timeout_ms,
                             const std::string& context) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        const int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            Fail(context + ": unable to create client socket");
        }

        SetSocketTimeout(fd, timeout_ms, context);

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

    SetSocketTimeout(fd, timeout_ms, context);
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

}  // namespace mc::test_support