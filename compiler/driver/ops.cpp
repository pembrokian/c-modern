#include "compiler/driver/internal.h"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "compiler/support/assert.h"

namespace mc::driver {
namespace {

std::optional<std::string> ReadCapturedText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        return std::nullopt;
    }

    return buffer.str();
}

}  // namespace

std::string FormatCommandForDisplay(const std::vector<std::string>& args) {
    std::ostringstream command;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (index > 0) {
            command << ' ';
        }
        command << '\'';
        for (const char ch : args[index]) {
            if (ch == '\'') {
                command << "'\\''";
                continue;
            }
            command << ch;
        }
        command << '\'';
    }
    return command.str();
}

WaitForChildResult WaitForChildProcess(const pid_t pid,
                                       const std::optional<int>& timeout_ms,
                                       int& status) {
    if (!timeout_ms.has_value() || *timeout_ms <= 0) {
        return waitpid(pid, &status, 0) == pid ? WaitForChildResult::kExited : WaitForChildResult::kWaitError;
    }

    struct WaitState {
        pid_t wait_result = -1;
        int wait_status = 0;
        bool done = false;
    };

    WaitState state;
    std::mutex mutex;
    std::condition_variable condition;
    std::thread waiter([&]() {
        int local_status = 0;
        const pid_t local_result = waitpid(pid, &local_status, 0);

        {
            std::lock_guard<std::mutex> lock(mutex);
            state.wait_result = local_result;
            state.wait_status = local_status;
            state.done = true;
        }
        condition.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex);
    const bool completed = condition.wait_for(lock,
                                              std::chrono::milliseconds(*timeout_ms),
                                              [&]() { return state.done; });
    if (!completed) {
        kill(pid, SIGKILL);
        lock.unlock();
        waiter.join();
        if (state.wait_result < 0) {
            return WaitForChildResult::kWaitError;
        }
        if (WIFSIGNALED(state.wait_status) && WTERMSIG(state.wait_status) == SIGKILL) {
            return WaitForChildResult::kTimedOut;
        }
        status = state.wait_status;
        return state.wait_result == pid ? WaitForChildResult::kExited : WaitForChildResult::kWaitError;
    }

    status = state.wait_status;
    lock.unlock();
    waiter.join();
    return state.wait_result == pid ? WaitForChildResult::kExited : WaitForChildResult::kWaitError;
}

int RunExecutableCommand(const std::filesystem::path& executable_path,
                         const std::vector<std::string>& arguments,
                         const std::optional<int>& timeout_ms) {
    std::vector<std::string> command;
    command.reserve(arguments.size() + 1);
    command.push_back(executable_path.generic_string());
    command.insert(command.end(), arguments.begin(), arguments.end());

    std::vector<char*> argv;
    argv.reserve(command.size() + 1);
    for (auto& arg : command) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        return 127;
    }
    if (pid == 0) {
        execv(executable_path.c_str(), argv.data());
        _exit(127);
    }

    int status = 0;
    switch (WaitForChildProcess(pid, timeout_ms, status)) {
        case WaitForChildResult::kExited:
#ifdef WIFEXITED
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
#endif
#ifdef WIFSIGNALED
            if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
            }
#endif
            return status;
        case WaitForChildResult::kWaitError:
            return 127;
        case WaitForChildResult::kTimedOut:
            return 124;
    }

    MC_UNREACHABLE("unhandled child wait result");
}

CapturedCommandResult RunExecutableCapture(const std::filesystem::path& executable_path,
                                          const std::vector<std::string>& arguments,
                                          const std::filesystem::path& output_path,
                                          const std::optional<int>& timeout_ms) {
    std::filesystem::create_directories(output_path.parent_path());
    std::vector<std::string> command;
    command.reserve(arguments.size() + 1);
    command.push_back(executable_path.generic_string());
    command.insert(command.end(), arguments.begin(), arguments.end());

    std::vector<char*> argv;
    argv.reserve(command.size() + 1);
    for (auto& arg : command) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    CapturedCommandResult result;

    const pid_t pid = fork();
    if (pid < 0) {
        result.exited = true;
        result.exit_code = 127;
        return result;
    }
    if (pid == 0) {
        const int fd = ::open(output_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (fd < 0) {
            _exit(127);
        }
        if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
            close(fd);
            _exit(127);
        }
        close(fd);
        execv(executable_path.c_str(), argv.data());
        _exit(127);
    }

    int status = 0;
    switch (WaitForChildProcess(pid, timeout_ms, status)) {
        case WaitForChildResult::kExited:
#ifdef WIFEXITED
            if (WIFEXITED(status)) {
                result.exited = true;
                result.exit_code = WEXITSTATUS(status);
            }
#endif
#ifdef WIFSIGNALED
            if (WIFSIGNALED(status)) {
                result.signaled = true;
                result.signal_number = WTERMSIG(status);
            }
#endif
            break;
        case WaitForChildResult::kWaitError:
            result.exited = true;
            result.exit_code = 127;
            break;
        case WaitForChildResult::kTimedOut:
            result.timed_out = true;
            result.signaled = true;
            result.signal_number = SIGKILL;
            break;
    }

    result.output = ReadCapturedText(output_path).value_or("");
    return result;
}

}  // namespace mc::driver