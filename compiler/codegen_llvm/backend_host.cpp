#include "compiler/codegen_llvm/backend.h"
#include "compiler/codegen_llvm/backend_internal.h"

#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace mc::codegen_llvm {
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

std::string JoinCommand(const std::vector<std::string>& args) {
    std::ostringstream command;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (index > 0) {
            command << ' ';
        }
        command << QuoteShellArg(args[index]);
    }
    return command.str();
}

struct HostCommandResult {
    int exit_code = 127;
    std::string output;
};

HostCommandResult RunCapturedHostCommand(const std::vector<std::string>& args) {
    HostCommandResult result;
    if (args.empty()) {
        result.output = "empty host toolchain command";
        return result;
    }

    int pipe_fds[2] = {-1, -1};
    if (pipe(pipe_fds) != 0) {
        result.output = std::string("failed to create host toolchain output pipe: ") + std::strerror(errno);
        return result;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        result.output = std::string("failed to fork host toolchain process: ") + std::strerror(errno);
        return result;
    }

    if (pid == 0) {
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0 || dup2(pipe_fds[1], STDERR_FILENO) < 0) {
            close(pipe_fds[1]);
            _exit(127);
        }
        close(pipe_fds[1]);

        std::vector<std::string> child_args = args;
        std::vector<char*> argv;
        argv.reserve(child_args.size() + 1);
        for (auto& arg : child_args) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        execvp(argv.front(), argv.data());
        _exit(errno == ENOENT ? 127 : 126);
    }

    close(pipe_fds[1]);
    char buffer[4096];
    while (true) {
        const ssize_t count = read(pipe_fds[0], buffer, sizeof(buffer));
        if (count > 0) {
            result.output.append(buffer, static_cast<std::size_t>(count));
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
    close(pipe_fds[0]);

    int raw_status = 0;
    while (waitpid(pid, &raw_status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        result.output += (result.output.empty() ? "" : "\n");
        result.output += std::string("failed to wait for host toolchain process: ") + std::strerror(errno);
        result.exit_code = 127;
        return result;
    }

#ifdef WIFEXITED
    if (WIFEXITED(raw_status)) {
        result.exit_code = WEXITSTATUS(raw_status);
        return result;
    }
#endif
#ifdef WIFSIGNALED
    if (WIFSIGNALED(raw_status)) {
        result.exit_code = 128 + WTERMSIG(raw_status);
        return result;
    }
#endif
    return result;
}

std::string TrimTrailingNewlines(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

void RemoveArtifactIfExists(const std::filesystem::path& artifact_path) {
    std::error_code ignored;
    std::filesystem::remove(artifact_path, ignored);
}

std::vector<std::string> BuildHostToolCommand(const TargetConfig& target,
                                              std::string_view tool,
                                              std::initializer_list<std::string> args) {
    std::vector<std::string> command = target.host_tool_prefix;
    command.reserve(command.size() + 1 + args.size());
    command.emplace_back(tool);
    command.insert(command.end(), args.begin(), args.end());
    return command;
}

bool RunHostCommand(const std::vector<std::string>& args,
                    const std::filesystem::path& source_path,
                    support::DiagnosticSink& diagnostics,
                    const std::string& description,
                    const std::optional<std::filesystem::path>& related_artifact_path = std::nullopt) {
    const std::string command = JoinCommand(args);
    HostCommandResult result = RunCapturedHostCommand(args);
    if (result.exit_code == 0) {
        return true;
    }

    std::string message = "LLVM bootstrap backend failed to " + description + " using host toolchain command: " + command +
                          " (exit=" + std::to_string(result.exit_code) + ")";
    if (related_artifact_path.has_value()) {
        message += "; inspect emitted artifact '" + related_artifact_path->generic_string() + "'";
    }
    const std::string output = TrimTrailingNewlines(std::move(result.output));
    if (!output.empty()) {
        message += "\nhost tool output:\n" + output;
    }

    ReportBackendError(source_path, message, diagnostics);
    return false;
}

}  // namespace

ObjectBuildResult BuildObjectFile(const mir::Module& module,
                                  const std::filesystem::path& source_path,
                                  const ObjectBuildOptions& options,
                                  support::DiagnosticSink& diagnostics) {
    mir::Module executable_module;
    if (!mir::SpecializeExecutableGenericFunctions(module, source_path, diagnostics, executable_module)) {
        return {};
    }

    if (!ValidateBootstrapTarget(options.target, source_path, diagnostics)) {
        return {};
    }
    if (!ValidateExecutableBackendCapabilities(executable_module, options.target, source_path, diagnostics)) {
        return {};
    }

    std::string llvm_ir;
    if (!RenderLlvmModule(executable_module, options.target, source_path, options.wrap_hosted_main, diagnostics, llvm_ir)) {
        return {};
    }

    std::filesystem::create_directories(options.artifacts.llvm_ir_path.parent_path());
    std::filesystem::create_directories(options.artifacts.object_path.parent_path());

    {
        std::ofstream output(options.artifacts.llvm_ir_path, std::ios::binary);
        output << llvm_ir;
        if (!output) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend could not write LLVM IR artifact '" +
                                   options.artifacts.llvm_ir_path.generic_string() + "'",
                               diagnostics);
            return {};
        }
    }

    RemoveArtifactIfExists(options.artifacts.object_path);

    if (!RunHostCommand(BuildHostToolCommand(options.target,
                                             "clang",
                                             {"-target",
                                              options.target.triple,
                                              "-x",
                                              "ir",
                                              options.artifacts.llvm_ir_path.generic_string(),
                                              "-c",
                                              "-o",
                                              options.artifacts.object_path.generic_string()}),
                        source_path,
                        diagnostics,
                        "emit an object file",
                        options.artifacts.llvm_ir_path)) {
        return {};
    }

    return {
        .artifacts = options.artifacts,
        .ok = true,
    };
}

LinkResult LinkExecutable(const std::filesystem::path& source_path,
                          const LinkOptions& options,
                          support::DiagnosticSink& diagnostics) {
    if (!ValidateBootstrapTarget(options.target, source_path, diagnostics)) {
        return {};
    }

    std::filesystem::create_directories(options.executable_path.parent_path());
    std::filesystem::create_directories(options.runtime_object_path.parent_path());

    if (!options.runtime_source_path.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend requires an explicit runtime support source path",
                           diagnostics);
        return {};
    }

    const auto& runtime_support_source = *options.runtime_source_path;
    if (!std::filesystem::exists(runtime_support_source)) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend could not read runtime support source '" +
                               runtime_support_source.generic_string() + "'",
                           diagnostics);
        return {};
    }
    for (const auto& input_path : options.extra_input_paths) {
        if (!std::filesystem::exists(input_path)) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend could not read explicit link input '" +
                                   input_path.generic_string() + "'",
                               diagnostics);
            return {};
        }
    }

    const bool runtime_object_missing = !std::filesystem::exists(options.runtime_object_path);
    const bool runtime_source_is_newer = !runtime_object_missing &&
                                         std::filesystem::last_write_time(runtime_support_source) >
                                             std::filesystem::last_write_time(options.runtime_object_path);
    if (runtime_object_missing || runtime_source_is_newer) {
        RemoveArtifactIfExists(options.runtime_object_path);
        if (!RunHostCommand(BuildHostToolCommand(options.target,
                                                 "clang",
                                                 {"-target",
                                                  options.target.triple,
                                                  "-c",
                                                  runtime_support_source.generic_string(),
                                                  "-o",
                                                  options.runtime_object_path.generic_string()}),
                            source_path,
                            diagnostics,
                            "compile runtime support")) {
            return {};
        }
    }

    std::vector<std::string> link_command = BuildHostToolCommand(options.target,
                                                                 "clang",
                                                                 {"-target", options.target.triple});
    for (const auto& object_path : options.object_paths) {
        link_command.push_back(object_path.generic_string());
    }
    for (const auto& input_path : options.extra_input_paths) {
        link_command.push_back(input_path.generic_string());
    }
    for (const auto& library_path : options.library_paths) {
        link_command.push_back(library_path.generic_string());
    }
    link_command.push_back(options.runtime_object_path.generic_string());
    link_command.push_back("-o");
    link_command.push_back(options.executable_path.generic_string());

    RemoveArtifactIfExists(options.executable_path);

    if (!RunHostCommand(link_command,
                        source_path,
                        diagnostics,
                        "link an executable")) {
        return {};
    }

    return {
        .runtime_object_path = options.runtime_object_path,
        .executable_path = options.executable_path,
        .ok = true,
    };
}

ArchiveResult ArchiveStaticLibrary(const std::filesystem::path& source_path,
                                   const ArchiveOptions& options,
                                   support::DiagnosticSink& diagnostics) {
    if (!ValidateBootstrapTarget(options.target, source_path, diagnostics)) {
        return {};
    }

    std::filesystem::create_directories(options.archive_path.parent_path());
    if (options.object_paths.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend cannot archive an empty static library",
                           diagnostics);
        return {};
    }

    std::vector<std::string> archive_command = BuildHostToolCommand(options.target,
                                                                    "libtool",
                                                                    {"-static",
                                                                     "-o",
                                                                     options.archive_path.generic_string()});
    for (const auto& object_path : options.object_paths) {
        archive_command.push_back(object_path.generic_string());
    }

    RemoveArtifactIfExists(options.archive_path);

    if (!RunHostCommand(archive_command,
                        source_path,
                        diagnostics,
                        "archive a hosted static library")) {
        return {};
    }

    return {
        .archive_path = options.archive_path,
        .ok = true,
    };
}

BuildResult BuildExecutable(const mir::Module& module,
                            const std::filesystem::path& source_path,
                            const BuildOptions& options,
                            support::DiagnosticSink& diagnostics) {
    const auto object_result = BuildObjectFile(module,
                                               source_path,
                                               {
                                                   .target = options.target,
                                                   .artifacts = {
                                                       .llvm_ir_path = options.artifacts.llvm_ir_path,
                                                       .object_path = options.artifacts.object_path,
                                                   },
                                                   .wrap_hosted_main = options.target.hosted,
                                               },
                                               diagnostics);
    if (!object_result.ok) {
        return {};
    }
    const std::filesystem::path runtime_object_path = options.artifacts.object_path.parent_path() /
                                                      (options.artifacts.object_path.stem().generic_string() + ".runtime.o");

    const auto link_result = LinkExecutable(source_path,
                                            {
                                                .target = options.target,
                                                .object_paths = {options.artifacts.object_path},
                                                .extra_input_paths = {},
                                                .library_paths = {},
                                                .runtime_source_path = options.runtime_source_path,
                                                .runtime_object_path = runtime_object_path,
                                                .executable_path = options.artifacts.executable_path,
                                            },
                                            diagnostics);
    if (!link_result.ok) {
        return {};
    }

    return {
        .artifacts = options.artifacts,
        .ok = true,
    };
}

}  // namespace mc::codegen_llvm