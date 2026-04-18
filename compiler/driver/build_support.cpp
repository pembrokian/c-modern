#include "compiler/driver/internal.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "compiler/codegen_llvm/backend.h"
#include "compiler/lex/lexer.h"
#include "compiler/support/dump_paths.h"

namespace mc::driver {
namespace {

constexpr std::string_view kHostedRuntimeSupportRelativePath = "runtime/hosted/mc_hosted_runtime.c";
constexpr std::string_view kFreestandingRuntimeDirectoryRelativePath = "runtime/freestanding";

}  // namespace

bool IsPathWithinRoot(const std::filesystem::path& path,
                      const std::filesystem::path& root) {
    const auto normalized_path = std::filesystem::absolute(path).lexically_normal();
    const auto normalized_root = std::filesystem::absolute(root).lexically_normal();

    auto root_it = normalized_root.begin();
    auto path_it = normalized_path.begin();
    for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }
    return true;
}

std::optional<std::filesystem::path> DiscoverRepositoryRoot(const std::filesystem::path& start_path) {
    std::filesystem::path current = std::filesystem::absolute(start_path).lexically_normal();
    if (!std::filesystem::is_directory(current)) {
        current = current.parent_path();
    }

    while (!current.empty()) {
        if (std::filesystem::exists(current / "stdlib") && std::filesystem::exists(current / "runtime")) {
            return current;
        }
        if (current == current.root_path()) {
            break;
        }
        current = current.parent_path();
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> DiscoverHostedRuntimeSupportSource(const std::filesystem::path& source_path) {
    const auto repo_root = DiscoverRepositoryRoot(source_path);
    if (!repo_root.has_value()) {
        return std::nullopt;
    }

    const std::filesystem::path runtime_source = *repo_root / std::string(kHostedRuntimeSupportRelativePath);
    if (!std::filesystem::exists(runtime_source)) {
        return std::nullopt;
    }
    return runtime_source;
}

std::optional<std::filesystem::path> DiscoverRuntimeSupportSource(std::string_view env,
                                                                  std::string_view startup,
                                                                  const std::filesystem::path& source_path) {
    if (env == "hosted") {
        if (startup != "default") {
            return std::nullopt;
        }
        return DiscoverHostedRuntimeSupportSource(source_path);
    }

    if (env != "freestanding") {
        return std::nullopt;
    }

    const auto repo_root = DiscoverRepositoryRoot(source_path);
    if (!repo_root.has_value()) {
        return std::nullopt;
    }

    const std::filesystem::path runtime_source =
        *repo_root / std::string(kFreestandingRuntimeDirectoryRelativePath) / ("mc_" + std::string(startup) + ".c");
    if (!std::filesystem::exists(runtime_source)) {
        return std::nullopt;
    }
    return runtime_source;
}

std::vector<std::filesystem::path> ComputeEffectiveImportRoots(const std::filesystem::path& source_path,
                                                               const std::vector<std::filesystem::path>& import_roots) {
    std::vector<std::filesystem::path> resolved_roots;
    resolved_roots.reserve(import_roots.size() + 1);

    std::unordered_set<std::string> seen_roots;
    const auto add_root = [&](const std::filesystem::path& root) {
        const std::filesystem::path normalized = std::filesystem::absolute(root).lexically_normal();
        const std::string key = normalized.generic_string();
        if (seen_roots.insert(key).second) {
            resolved_roots.push_back(normalized);
        }
    };

    for (const auto& import_root : import_roots) {
        add_root(import_root);
    }

    if (const auto repo_root = DiscoverRepositoryRoot(source_path); repo_root.has_value()) {
        const std::filesystem::path stdlib_root = *repo_root / "stdlib";
        if (std::filesystem::exists(stdlib_root)) {
            add_root(stdlib_root);
        }
    }

    return resolved_roots;
}

std::optional<std::string> ReadSourceText(const std::filesystem::path& path,
                                          support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to read source file",
        });
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to read source file",
        });
        return std::nullopt;
    }

    return buffer.str();
}

std::optional<mc::parse::ParseResult> ParseSourcePath(const std::filesystem::path& path,
                                                      support::DiagnosticSink& diagnostics) {
    const auto source_text = ReadSourceText(path, diagnostics);
    if (!source_text.has_value()) {
        return std::nullopt;
    }

    const auto lex_result = mc::lex::Lex(*source_text, path.generic_string(), diagnostics);
    auto parse_result = mc::parse::Parse(lex_result, path, diagnostics);
    if (!parse_result.ok) {
        return std::nullopt;
    }

    return parse_result;
}

std::filesystem::path DefaultProjectPath() {
    return std::filesystem::current_path() / "build.toml";
}

std::string BootstrapTargetIdentity() {
    const auto config = mc::codegen_llvm::BootstrapTargetConfig();
    return config.triple;
}

bool IsSupportedMode(std::string_view mode) {
    return mode == "debug" || mode == "release" || mode == "checked";
}

bool IsSupportedEnv(std::string_view env) {
    return env == "hosted" || env == "freestanding";
}

bool IsExecutableTargetKind(std::string_view kind) {
    return kind == "exe" || kind == "test";
}

bool IsStaticLibraryTargetKind(std::string_view kind) {
    return kind == "staticlib";
}

std::string HexU64(std::uint64_t value) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(16, '0');
    for (int index = 15; index >= 0; --index) {
        text[static_cast<std::size_t>(index)] = kDigits[value & 0xfu];
        value >>= 4u;
    }
    return text;
}

std::string HashText(std::string_view text) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char byte : text) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ull;
    }
    return HexU64(hash);
}

std::filesystem::path ComputeModuleStatePath(const std::filesystem::path& source_path,
                                             const std::filesystem::path& build_dir) {
    return build_dir / "state" / (support::SanitizeArtifactStem(source_path) + ".state.txt");
}

std::filesystem::path ComputeLogicalModuleStatePath(std::string_view artifact_key,
                                                    const std::filesystem::path& build_dir) {
    return build_dir / "state" / (support::SanitizeArtifactStem(std::filesystem::path(std::string(artifact_key))) + ".state.txt");
}

std::filesystem::path ComputeRuntimeObjectPath(const std::filesystem::path& entry_source_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view env,
                                               std::string_view startup) {
    const auto build_targets = support::ComputeBuildArtifactTargets(entry_source_path, build_dir);
    const std::string runtime_object_name =
        build_targets.object.stem().generic_string() + "." + std::string(env) + "." + std::string(startup) + ".runtime.o";
    return build_targets.object.parent_path() / runtime_object_name;
}

std::filesystem::path ComputeLogicalRuntimeObjectPath(std::string_view artifact_key,
                                                      const std::filesystem::path& build_dir,
                                                      std::string_view env,
                                                      std::string_view startup) {
    const auto build_targets = support::ComputeLogicalBuildArtifactTargets(artifact_key, build_dir);
    const std::string runtime_object_name =
        build_targets.object.stem().generic_string() + "." + std::string(env) + "." + std::string(startup) + ".runtime.o";
    return build_targets.object.parent_path() / runtime_object_name;
}

std::optional<ModuleBuildState> LoadModuleBuildState(const std::filesystem::path& path,
                                                     support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    const auto read_state_line = [&](std::string_view expected_key) -> std::optional<std::string> {
        std::string line;
        if (!std::getline(input, line)) {
            return std::nullopt;
        }

        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || std::string_view(line.data(), tab) != expected_key) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        return line.substr(tab + 1);
    };

    const auto format_text = read_state_line("format");
    if (!format_text.has_value()) {
        return std::nullopt;
    }
    const auto parsed_format = support::ParseArrayLength(*format_text);
    if (!parsed_format.has_value()) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "invalid module build state format entry",
        });
        return std::nullopt;
    }
    if (static_cast<int>(*parsed_format) != kModuleBuildStateFormatVersion) {
        return std::nullopt;
    }

    auto target_identity = read_state_line("target");
    auto mode = read_state_line("mode");
    auto env = read_state_line("env");
    auto package_identity = read_state_line("package");
    auto source_hash = read_state_line("source_hash");
    auto interface_hash = read_state_line("interface_hash");
    auto wrap_hosted_main_text = read_state_line("wrap_hosted_main");
    if (!target_identity.has_value() || !mode.has_value() || !env.has_value() || !package_identity.has_value() ||
        !source_hash.has_value() || !interface_hash.has_value() || !wrap_hosted_main_text.has_value()) {
        return std::nullopt;
    }

    ModuleBuildState state {
        .target_identity = std::move(*target_identity),
        .package_identity = std::move(*package_identity),
        .mode = std::move(*mode),
        .env = std::move(*env),
        .source_hash = std::move(*source_hash),
        .interface_hash = std::move(*interface_hash),
        .wrap_hosted_main = *wrap_hosted_main_text == "1",
    };

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || std::string_view(line.data(), tab) != "import_hash") {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        const std::string value = line.substr(tab + 1);
        const std::size_t equals = value.find('=');
        if (equals == std::string::npos) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid imported interface hash entry",
            });
            return std::nullopt;
        }
        state.imported_interface_hashes.push_back({value.substr(0, equals), value.substr(equals + 1)});
    }

    return state;
}

bool WriteModuleBuildState(const std::filesystem::path& path,
                           const ModuleBuildState& state,
                           support::DiagnosticSink& diagnostics) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to write module build state",
        });
        return false;
    }

    output << "format\t" << kModuleBuildStateFormatVersion << '\n';
    output << "target\t" << state.target_identity << '\n';
    output << "mode\t" << state.mode << '\n';
    output << "env\t" << state.env << '\n';
    output << "package\t" << state.package_identity << '\n';
    output << "source_hash\t" << state.source_hash << '\n';
    output << "interface_hash\t" << state.interface_hash << '\n';
    output << "wrap_hosted_main\t" << (state.wrap_hosted_main ? "1" : "0") << '\n';
    for (const auto& [import_name, hash] : state.imported_interface_hashes) {
        output << "import_hash\t" << import_name << '=' << hash << '\n';
    }
    return static_cast<bool>(output);
}

bool ShouldReuseModuleObject(const ModuleBuildState& state,
                             const ModuleBuildState& current,
                             const std::filesystem::path& object_path,
                             const std::filesystem::path& mci_path) {
    if (!std::filesystem::exists(object_path) || !std::filesystem::exists(mci_path)) {
        return false;
    }
    if (state.target_identity != current.target_identity || state.package_identity != current.package_identity || state.mode != current.mode ||
        state.env != current.env || state.source_hash != current.source_hash ||
        state.interface_hash != current.interface_hash ||
        state.wrap_hosted_main != current.wrap_hosted_main) {
        return false;
    }
    return state.imported_interface_hashes == current.imported_interface_hashes;
}

}  // namespace mc::driver