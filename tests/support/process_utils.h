#ifndef MODERN_C_TESTS_SUPPORT_PROCESS_UTILS_H
#define MODERN_C_TESTS_SUPPORT_PROCESS_UTILS_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sys/types.h>

#include "tests/support/fixture_utils.h"

namespace mc::test_support {

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

std::string ReadFile(const std::filesystem::path& path);

void WriteFile(const std::filesystem::path& path,
               std::string_view contents);

void CopyDirectoryTree(const std::filesystem::path& source,
                       const std::filesystem::path& destination);

CommandOutcome RunCommand(const std::vector<std::string>& args,
                          const std::string& context);

std::pair<CommandOutcome, std::string> RunCommandCapture(
    const std::vector<std::string>& args,
    const std::filesystem::path& output_path,
    const std::string& context);

void ExpectCommandSuccess(const std::vector<std::string>& args,
                          const std::string& context);

void ExpectExecutableExit(const std::filesystem::path& executable,
                          const std::vector<std::string>& args,
                          int expected_exit_code,
                          const std::string& context);

void ExpectExecutableFailure(const std::filesystem::path& executable,
                             const std::vector<std::string>& args,
                             const std::string& context);

void ExpectExecutableOutput(const std::filesystem::path& executable,
                            const std::vector<std::string>& args,
                            int expected_exit_code,
                            std::string_view expected_output,
                            const std::filesystem::path& output_path,
                            const std::string& context);

void ExpectOutputContains(std::string_view output,
                          std::string_view needle,
                          const std::string& context);

void SleepForTimestampTick();

std::filesystem::file_time_type RequireWriteTime(const std::filesystem::path& path);

void CloseFd(int fd);

BackgroundProcess SpawnBackgroundCommand(const std::vector<std::string>& args,
                                         const std::filesystem::path& output_path,
                                         const std::string& context);

BackgroundProcess SpawnBackgroundExecutable(const std::filesystem::path& executable,
                                            const std::vector<std::string>& args,
                                            const std::filesystem::path& output_path,
                                            const std::string& context);

CommandOutcome WaitForProcessExit(pid_t pid,
                                  int timeout_ms,
                                  const std::string& context);

void ExpectBackgroundProcessSuccess(const BackgroundProcess& process,
                                    int timeout_ms,
                                    const std::string& context);

uint16_t ReserveLoopbackPort();

int CreateLoopbackListener(uint16_t* out_port);

int ConnectLoopbackWithRetry(uint16_t port,
                             int timeout_ms,
                             const std::string& context);

int AcceptLoopbackClient(int listener_fd,
                         int timeout_ms,
                         const std::string& context);

void WriteAllToSocket(int fd,
                      std::string_view data,
                      const std::string& context);

std::string ReadExactFromSocket(int fd,
                                size_t expected_size,
                                const std::string& context);

}  // namespace mc::test_support

#endif