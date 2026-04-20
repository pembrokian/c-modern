#include "tests/tool/tool_kernel_common.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <vector>

#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;

namespace {

constexpr std::string_view kKernelRuntimeRootRelativePath = "runtime";
constexpr std::string_view kFreestandingKernelRunContractStdoutContainsV1 =
    "stdout-contains-v1";
constexpr std::string_view kFreestandingKernelRunContractSuccessExitV1 =
    "success-exit-v1";
constexpr std::string_view kFreestandingKernelMirContractFirstMatchProjectionV1 =
    "first-match-projection-v1";

enum class RunContractKind {
    success_exit_v1,
    stdout_contains_v1,
};

enum class MirContractKind {
    first_match_projection_v1,
};

enum class ArtifactAuditKind {
    standalone_manual_relink,
    kernel_manual_relink,
    file_contains,
};

std::string TrimCopy(std::string_view text) {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return std::string(text.substr(start, end - start));
}

std::optional<std::string> ParseQuotedSetting(std::string_view line, std::string_view key) {
    const std::string prefix = std::string(key) + " = \"";
    if (!line.starts_with(prefix) || line.size() < prefix.size() + 1 || line.back() != '"') {
        return std::nullopt;
    }
    return std::string(line.substr(prefix.size(), line.size() - prefix.size() - 1));
}

std::optional<int> ParseIntSetting(std::string_view line, std::string_view key) {
    const std::string prefix = std::string(key) + " = ";
    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }
    return std::stoi(std::string(line.substr(prefix.size())));
}

std::optional<std::string> ParseArrayEntry(std::string_view line) {
    std::string trimmed = TrimCopy(line);
    if (trimmed == "]") {
        return std::nullopt;
    }
    if (!trimmed.empty() && trimmed.back() == ',') {
        trimmed.pop_back();
    }
    if (trimmed.size() < 2 || trimmed.front() != '"' || trimmed.back() != '"') {
        Fail("runtime phase directory contains an invalid array entry: " + trimmed);
    }
    return trimmed.substr(1, trimmed.size() - 2);
}

std::vector<std::string> ParseInlineQuotedArray(std::string_view line, std::string_view key) {
    const std::string prefix = std::string(key) + " = [";
    if (!line.starts_with(prefix) || line.size() < prefix.size() + 1 || line.back() != ']') {
        Fail("runtime phase directory contains an invalid inline array for key: " + std::string(key));
    }
    std::string_view inner = line;
    inner.remove_prefix(prefix.size());
    inner.remove_suffix(1);
    std::vector<std::string> values;

    while (!inner.empty()) {
        while (!inner.empty() && inner.front() == ' ') {
            inner.remove_prefix(1);
        }
        if (inner.empty()) {
            break;
        }
        if (inner.front() != '"') {
            Fail("runtime phase directory inline array should contain quoted strings for key: " + std::string(key));
        }
        inner.remove_prefix(1);

        std::string value;
        bool closed_quote = false;
        bool escape_next = false;
        while (!inner.empty()) {
            const char ch = inner.front();
            inner.remove_prefix(1);

            if (escape_next) {
                value.push_back(ch);
                escape_next = false;
                continue;
            }
            if (ch == '\\') {
                escape_next = true;
                continue;
            }
            if (ch == '"') {
                closed_quote = true;
                break;
            }
            value.push_back(ch);
        }

        if (escape_next) {
            Fail("runtime phase directory inline array ends with a dangling escape for key: " + std::string(key));
        }
        if (!closed_quote) {
            Fail("runtime phase directory inline array is missing a closing quote for key: " + std::string(key));
        }
        values.push_back(std::move(value));

        while (!inner.empty() && inner.front() == ' ') {
            inner.remove_prefix(1);
        }
        if (inner.empty()) {
            break;
        }
        if (inner.front() != ',') {
            Fail("runtime phase directory inline array should separate quoted strings with commas for key: " +
                 std::string(key));
        }
        inner.remove_prefix(1);
        while (!inner.empty() && inner.front() == ' ') {
            inner.remove_prefix(1);
        }
        if (inner.empty()) {
            Fail("runtime phase directory inline array should not end with a trailing comma for key: " +
                 std::string(key));
        }
    }

    return values;
}

RunContractKind ParseRunContractKind(std::string_view run_contract,
                                     const std::string& context) {
    if (run_contract == kFreestandingKernelRunContractSuccessExitV1) {
        return RunContractKind::success_exit_v1;
    }
    if (run_contract == kFreestandingKernelRunContractStdoutContainsV1) {
        return RunContractKind::stdout_contains_v1;
    }
    Fail(context + std::string(run_contract));
}

MirContractKind ParseMirContractKind(std::string_view mir_contract,
                                     const std::string& context) {
    if (mir_contract == kFreestandingKernelMirContractFirstMatchProjectionV1) {
        return MirContractKind::first_match_projection_v1;
    }
    Fail(context + std::string(mir_contract));
}

ArtifactAuditKind ParseArtifactAuditKind(std::string_view kind,
                                         const std::string& context) {
    if (kind == "standalone_manual_relink") {
        return ArtifactAuditKind::standalone_manual_relink;
    }
    if (kind == "kernel_manual_relink") {
        return ArtifactAuditKind::kernel_manual_relink;
    }
    if (kind == "file_contains") {
        return ArtifactAuditKind::file_contains;
    }
    Fail(context + std::string(kind));
}

void ValidateFreestandingKernelRuntimePhaseDescriptor(const FreestandingKernelRuntimePhaseDescriptor& phase_check,
                                                      const std::filesystem::path& phase_toml_path,
                                                      int shard) {
    if (phase_check.phase == 0 || phase_check.shard != shard || phase_check.project != "kernel/build.toml" ||
        phase_check.target != "kernel" ||
        phase_check.mir_contract != kFreestandingKernelMirContractFirstMatchProjectionV1 || phase_check.label.empty() ||
        phase_check.expected_mir_contract_file.empty() ||
        phase_check.mir_selector_storage.empty()) {
        Fail("runtime phase directory missing or invalid required fields: " + phase_toml_path.generic_string());
    }
    switch (ParseRunContractKind(phase_check.run_contract,
                                 "runtime phase directory has an unsupported run contract: ")) {
    case RunContractKind::stdout_contains_v1:
        if (phase_check.expected_run_lines_file.empty()) {
            Fail("runtime phase directory missing stdout run golden for run contract: " +
                 phase_toml_path.generic_string());
        }
        return;
    case RunContractKind::success_exit_v1:
        return;
    }
}

void ExpectFreestandingKernelRunContract(std::string_view run_output,
                                         const FreestandingKernelPhaseCheck& phase_check,
                                         const std::filesystem::path& source_root,
                                         const std::string& context) {
    switch (ParseRunContractKind(phase_check.run_contract, context + " unsupported run contract: ")) {
    case RunContractKind::success_exit_v1:
        return;
    case RunContractKind::stdout_contains_v1:
        ExpectTextContainsLinesFile(run_output,
                                    ResolveFreestandingKernelGoldenPath(source_root,
                                                                        phase_check.expected_run_lines_file),
                                    context);
        return;
    }
}

void ExpectFreestandingKernelMirContract(std::string_view mir_text,
                                         const FreestandingKernelPhaseCheck& phase_check,
                                         const std::filesystem::path& expected_contract_path,
                                         const std::string& context) {
    switch (ParseMirContractKind(phase_check.mir_contract, context + " unsupported MIR contract: ")) {
    case MirContractKind::first_match_projection_v1:
        ExpectMirFirstMatchProjectionFileSpan(mir_text,
                                              phase_check.mir_selectors,
                                              expected_contract_path,
                                              context);
        return;
    }
}

void ValidateFreestandingKernelTextAuditDescriptor(const FreestandingKernelTextAuditDescriptor& descriptor,
                                                   const std::filesystem::path& descriptor_path) {
    if (descriptor.label.empty() || descriptor.source_file.empty() || descriptor.context.empty() ||
        descriptor.expected_contains.empty()) {
        Fail("kernel text audit descriptor missing required fields: " + descriptor_path.generic_string());
    }
}

void ValidateFreestandingKernelArtifactAuditDescriptor(const FreestandingKernelArtifactAuditDescriptor& descriptor,
                                                       const std::filesystem::path& descriptor_path) {
    if (descriptor.label.empty() || descriptor.kind.empty()) {
        Fail("kernel artifact audit descriptor missing required fields: " + descriptor_path.generic_string());
    }
    switch (ParseArtifactAuditKind(descriptor.kind,
                                   "unknown kernel artifact audit kind in descriptor: ")) {
    case ArtifactAuditKind::standalone_manual_relink:
        if (descriptor.project_name.empty() || descriptor.main_source_file.empty() ||
            descriptor.driver_support_source_file.empty() || descriptor.manual_support_source_file.empty() ||
            descriptor.expected_exit_code < 0) {
            Fail("standalone manual relink descriptor missing required fields: " + descriptor_path.generic_string());
        }
        return;
    case ArtifactAuditKind::kernel_manual_relink:
        if (descriptor.output_stem.empty() || descriptor.build_context.empty() || descriptor.run_context.empty() ||
            descriptor.expected_run_lines_file.empty()) {
            Fail("kernel manual relink descriptor missing required fields: " + descriptor_path.generic_string());
        }
        return;
    case ArtifactAuditKind::file_contains:
        if (descriptor.source_file.empty() || descriptor.expected_run_lines_file.empty()) {
            Fail("file contains descriptor missing required fields: " + descriptor_path.generic_string());
        }
        return;
    }
}

std::vector<std::filesystem::path> CollectPhaseDirectories(const std::filesystem::path& runtime_root) {
    std::vector<std::filesystem::path> phase_dirs;
    for (const auto& entry : std::filesystem::directory_iterator(runtime_root)) {
        if (entry.is_directory()) {
            phase_dirs.push_back(entry.path());
        }
    }
    std::sort(phase_dirs.begin(), phase_dirs.end());
    return phase_dirs;
}

std::vector<std::filesystem::path> CollectDescriptorDirectories(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> descriptor_dirs;
    if (!std::filesystem::exists(root)) {
        return descriptor_dirs;
    }
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_directory()) {
            descriptor_dirs.push_back(entry.path());
        }
    }
    std::sort(descriptor_dirs.begin(), descriptor_dirs.end());
    return descriptor_dirs;
}

FreestandingKernelRuntimePhaseDescriptor ParseRuntimePhaseDescriptor(const std::filesystem::path& phase_dir,
                                                                    const std::filesystem::path& phase_toml_path) {
    enum class ArrayMode {
        none,
        selectors,
    };

    FreestandingKernelRuntimePhaseDescriptor current;
    ArrayMode array_mode = ArrayMode::none;
    std::istringstream stream(ReadFile(phase_toml_path));
    std::string raw_line;
    while (std::getline(stream, raw_line)) {
        const std::string line = TrimCopy(raw_line);
        if (line.empty() || line.starts_with('#')) {
            continue;
        }
        if (array_mode == ArrayMode::selectors) {
            if (line == "]") {
                array_mode = ArrayMode::none;
                continue;
            }
            current.mir_selector_storage.push_back(*ParseArrayEntry(line));
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "label")) {
            current.label = *value;
            continue;
        }
        if (const auto value = ParseIntSetting(line, "phase")) {
            current.phase = *value;
            continue;
        }
        if (const auto value = ParseIntSetting(line, "shard")) {
            current.shard = *value;
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "project")) {
            current.project = *value;
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "target")) {
            current.target = *value;
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "output_stem")) {
            current.output_stem = *value;
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "build_context")) {
            current.build_context = *value;
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "run_context")) {
            current.run_context = *value;
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "run_contract")) {
            current.run_contract = *value;
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "run_golden")) {
            current.expected_run_lines_file = (std::filesystem::path(kKernelRuntimeRootRelativePath) /
                                               phase_dir.filename() / *value)
                                                  .generic_string();
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "mir_golden")) {
            current.expected_mir_contract_file = (std::filesystem::path(kKernelRuntimeRootRelativePath) /
                                                  phase_dir.filename() / *value)
                                                     .generic_string();
            continue;
        }
        if (const auto value = ParseQuotedSetting(line, "mir_contract")) {
            current.mir_contract = *value;
            continue;
        }
        if (line.starts_with("required_objects = [")) {
            current.required_object_file_storage = ParseInlineQuotedArray(line, "required_objects");
            continue;
        }
        if (line == "selectors = [") {
            array_mode = ArrayMode::selectors;
            continue;
        }
        Fail("runtime phase directory contains an unknown directive: " + line);
    }
    if (array_mode != ArrayMode::none) {
        Fail("runtime phase directory has an unterminated selectors array: " + phase_toml_path.generic_string());
    }
    return current;
}

std::vector<FreestandingKernelRuntimePhaseDescriptor> LoadRuntimePhaseDescriptorsImpl(
    const std::filesystem::path& source_root,
    std::optional<int> requested_shard) {
    const std::filesystem::path runtime_root = ResolveFreestandingKernelGoldenPath(source_root,
                                                                                   kKernelRuntimeRootRelativePath);
    std::vector<FreestandingKernelRuntimePhaseDescriptor> phase_checks;
    if (!std::filesystem::exists(runtime_root)) {
        return phase_checks;
    }

    for (const auto& phase_dir : CollectPhaseDirectories(runtime_root)) {
        const std::filesystem::path phase_toml_path = phase_dir / "phase.toml";
        if (!std::filesystem::exists(phase_toml_path)) {
            continue;
        }

        auto current = ParseRuntimePhaseDescriptor(phase_dir, phase_toml_path);
        if (requested_shard.has_value() && current.shard != *requested_shard) {
            continue;
        }
        if (current.run_contract.empty()) {
            current.run_contract = current.expected_run_lines_file.empty()
                                       ? std::string(kFreestandingKernelRunContractSuccessExitV1)
                                       : std::string(kFreestandingKernelRunContractStdoutContainsV1);
        }
        current.RefreshViews();
        ValidateFreestandingKernelRuntimePhaseDescriptor(current,
                                                        phase_toml_path,
                                                        requested_shard.value_or(current.shard));
        phase_checks.push_back(std::move(current));
    }

    return phase_checks;
}

void MaybeCleanBuildDir(const std::filesystem::path& build_dir) {
    const char* force_clean_env = std::getenv("MC_TEST_FORCE_CLEAN_BUILDS");
    if (force_clean_env == nullptr || std::string_view(force_clean_env) == "0") {
        return;
    }
    std::filesystem::remove_all(build_dir);
}

}  // namespace

void FreestandingKernelRuntimePhaseDescriptor::RefreshViews() {
    required_object_files.clear();
    for (const auto& object_name : required_object_file_storage) {
        required_object_files.push_back(object_name);
    }
    mir_selectors.clear();
    for (const auto& selector : mir_selector_storage) {
        mir_selectors.push_back(selector);
    }
}

FreestandingKernelPhaseCheck FreestandingKernelRuntimePhaseDescriptor::View() const {
    return {
        .label = label,
        .expected_run_lines_file = expected_run_lines_file,
        .run_contract = run_contract,
        .required_object_files = required_object_files,
        .mir_selectors = mir_selectors,
        .expected_mir_contract_file = expected_mir_contract_file,
        .mir_contract = mir_contract,
    };
}

FreestandingKernelCommonPaths MakeFreestandingKernelCommonPaths(const std::filesystem::path& source_root) {
    return {
        .project_path = source_root / "kernel_old" / "build.toml",
        .main_source_path = source_root / "kernel_old" / "src" / "main.mc",
        .roadmap_path = source_root / "docs" / "plan" / "admin" / "canopus_post_phase109_speculative_roadmap.txt",
        .kernel_readme_path = source_root / "kernel_old" / "README.md",
        .repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md",
        .freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md",
        .decision_log_path = source_root / "docs" / "plan" / "decision_log.txt",
        .backlog_path = source_root / "docs" / "plan" / "backlog.txt",
    };
}

std::filesystem::path ResolveFreestandingKernelGoldenPath(const std::filesystem::path& source_root,
                                                          std::string_view file_name) {
    const std::filesystem::path kernel_root = source_root / "tests" / "tool" / "freestanding" / "kernel";
    const std::filesystem::path direct_path = kernel_root / std::string(file_name);
    if (file_name.find('/') != std::string_view::npos || std::filesystem::exists(direct_path)) {
        return direct_path;
    }
    if (file_name.ends_with(".mirproj.txt") || file_name.ends_with(".mir.contains.txt")) {
        return kernel_root / "goldens" / "mir" / std::string(file_name);
    }
    if (file_name.ends_with(".run.contains.txt")) {
        return kernel_root / "goldens" / "run" / std::string(file_name);
    }
    if (file_name.ends_with(".contains.txt")) {
        return kernel_root / "goldens" / "contracts" / std::string(file_name);
    }
    return direct_path;
}

std::vector<FreestandingKernelRuntimePhaseDescriptor> LoadFreestandingKernelRuntimePhaseDescriptors(
    const std::filesystem::path& source_root,
    int shard) {
    return LoadRuntimePhaseDescriptorsImpl(source_root, shard);
}

std::vector<FreestandingKernelRuntimePhaseDescriptor> LoadAllFreestandingKernelRuntimePhaseDescriptors(
    const std::filesystem::path& source_root) {
    return LoadRuntimePhaseDescriptorsImpl(source_root, std::nullopt);
}

std::vector<FreestandingKernelTextAuditDescriptor> LoadFreestandingKernelTextAuditDescriptors(
    const std::filesystem::path& source_root,
    std::string_view relative_root,
    std::string_view descriptor_file_name) {
    const std::filesystem::path root = ResolveFreestandingKernelGoldenPath(source_root, relative_root);
    std::vector<FreestandingKernelTextAuditDescriptor> descriptors;

    enum class ArrayMode {
        none,
        contains,
    };

    for (const auto& descriptor_dir : CollectDescriptorDirectories(root)) {
        const std::filesystem::path descriptor_path = descriptor_dir / std::string(descriptor_file_name);
        if (!std::filesystem::exists(descriptor_path)) {
            continue;
        }

        FreestandingKernelTextAuditDescriptor current;
        ArrayMode array_mode = ArrayMode::none;
        std::istringstream stream(ReadFile(descriptor_path));
        std::string raw_line;
        while (std::getline(stream, raw_line)) {
            const std::string line = TrimCopy(raw_line);
            if (line.empty() || line.starts_with('#')) {
                continue;
            }
            if (array_mode == ArrayMode::contains) {
                if (line == "]") {
                    array_mode = ArrayMode::none;
                    continue;
                }
                current.expected_contains.push_back(*ParseArrayEntry(line));
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "label")) {
                current.label = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "source")) {
                current.source_file = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "context")) {
                current.context = *value;
                continue;
            }
            if (line == "contains = [") {
                array_mode = ArrayMode::contains;
                continue;
            }
            Fail("kernel text audit descriptor contains an unknown directive: " + line);
        }
        if (array_mode != ArrayMode::none) {
            Fail("kernel text audit descriptor has an unterminated contains array: " + descriptor_path.generic_string());
        }
        ValidateFreestandingKernelTextAuditDescriptor(current, descriptor_path);
        descriptors.push_back(std::move(current));
    }

    return descriptors;
}

std::vector<FreestandingKernelArtifactAuditDescriptor> LoadFreestandingKernelArtifactAuditDescriptors(
    const std::filesystem::path& source_root) {
    const std::filesystem::path root = ResolveFreestandingKernelGoldenPath(source_root, "artifact_specs");
    std::vector<FreestandingKernelArtifactAuditDescriptor> descriptors;

    for (const auto& descriptor_dir : CollectDescriptorDirectories(root)) {
        const std::filesystem::path descriptor_path = descriptor_dir / "artifact.toml";
        if (!std::filesystem::exists(descriptor_path)) {
            continue;
        }

        FreestandingKernelArtifactAuditDescriptor current;
        current.descriptor_dir = descriptor_dir;
        std::istringstream stream(ReadFile(descriptor_path));
        std::string raw_line;
        while (std::getline(stream, raw_line)) {
            const std::string line = TrimCopy(raw_line);
            if (line.empty() || line.starts_with('#')) {
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "label")) {
                current.label = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "kind")) {
                current.kind = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "project_name")) {
                current.project_name = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "output_stem")) {
                current.output_stem = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "build_context")) {
                current.build_context = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "run_context")) {
                current.run_context = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "run_golden")) {
                current.expected_run_lines_file = (std::filesystem::path("artifact_specs") /
                                                   descriptor_dir.filename() /
                                                   *value)
                                                      .generic_string();
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "source_file")) {
                current.source_file = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "main_source")) {
                current.main_source_file = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "driver_support_source")) {
                current.driver_support_source_file = *value;
                continue;
            }
            if (const auto value = ParseQuotedSetting(line, "manual_support_source")) {
                current.manual_support_source_file = *value;
                continue;
            }
            if (const auto value = ParseIntSetting(line, "expected_exit_code")) {
                current.expected_exit_code = *value;
                continue;
            }
            Fail("kernel artifact audit descriptor contains an unknown directive: " + line);
        }

        ValidateFreestandingKernelArtifactAuditDescriptor(current, descriptor_path);
        descriptors.push_back(std::move(current));
    }

    return descriptors;
}

bool FreestandingKernelCaseNameMatchesExpectedRunFile(std::string_view expected_run_lines_file,
                                                      std::string_view case_name) {
    constexpr std::string_view kRunContainsSuffix = ".run.contains.txt";
    const std::size_t slash = expected_run_lines_file.find_last_of('/');
    if (slash != std::string_view::npos) {
        expected_run_lines_file.remove_prefix(slash + 1);
    }
    if (!expected_run_lines_file.ends_with(kRunContainsSuffix)) {
        return false;
    }
    return expected_run_lines_file.substr(0, expected_run_lines_file.size() - kRunContainsSuffix.size()) == case_name;
}

const FreestandingKernelRuntimePhaseDescriptor& RequireFreestandingKernelRuntimePhaseDescriptor(
    const std::vector<FreestandingKernelRuntimePhaseDescriptor>& phase_checks,
    std::string_view expected_run_lines_file,
    std::string_view context_label) {
    constexpr std::string_view kRunContainsSuffix = ".run.contains.txt";
    std::string_view case_name = expected_run_lines_file;
    if (case_name.ends_with(kRunContainsSuffix)) {
        case_name = case_name.substr(0, case_name.size() - kRunContainsSuffix.size());
    }
    for (const auto& phase_check : phase_checks) {
        if (phase_check.label == case_name) {
            return phase_check;
        }
    }
    for (const auto& phase_check : phase_checks) {
        if (FreestandingKernelCaseNameMatchesExpectedRunFile(phase_check.expected_run_lines_file, case_name)) {
            return phase_check;
        }
    }
    Fail("missing " + std::string(context_label) + " runtime phase entry for run golden: " +
         std::string(expected_run_lines_file));
}

const FreestandingKernelRuntimePhaseDescriptor* FindFreestandingKernelRuntimePhaseDescriptorForCase(
    std::span<const FreestandingKernelRuntimePhaseDescriptor> phase_checks,
    std::string_view case_name) {
    for (const auto& phase_check : phase_checks) {
        if (phase_check.label == case_name) {
            return &phase_check;
        }
    }
    for (const auto& phase_check : phase_checks) {
        if (FreestandingKernelCaseNameMatchesExpectedRunFile(phase_check.expected_run_lines_file, case_name)) {
            return &phase_check;
        }
    }
    return nullptr;
}

void RunFreestandingKernelRuntimePhaseCase(const std::filesystem::path& source_root,
                                           const std::filesystem::path& binary_root,
                                           const std::filesystem::path& mc_path,
                                           int shard,
                                           std::string_view case_name,
                                           std::string_view context_label) {
    const auto phase_checks = LoadFreestandingKernelRuntimePhaseDescriptors(source_root, shard);
    const auto* phase_check = FindFreestandingKernelRuntimePhaseDescriptorForCase(phase_checks, case_name);
    if (phase_check == nullptr) {
        Fail("missing " + std::string(context_label) + " runtime phase entry for case: " + std::string(case_name));
    }
    if (phase_check->output_stem.empty() || phase_check->build_context.empty() || phase_check->run_context.empty()) {
        Fail("missing " + std::string(context_label) + " runtime execution metadata for case: " +
             std::string(case_name));
    }
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               phase_check->output_stem,
                                                               phase_check->build_context,
                                                               phase_check->run_context);
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                               artifacts.build_targets.object.parent_path(),
                                               artifacts.run_output,
                                               kernel_mir,
                                               phase_check->View());
}

void RunFreestandingKernelRuntimePhaseCase(const std::filesystem::path& source_root,
                                           const std::filesystem::path& binary_root,
                                           const std::filesystem::path& mc_path,
                                           std::string_view case_name,
                                           std::string_view context_label) {
    const auto phase_checks = LoadAllFreestandingKernelRuntimePhaseDescriptors(source_root);
    const auto* phase_check = FindFreestandingKernelRuntimePhaseDescriptorForCase(phase_checks, case_name);
    if (phase_check == nullptr) {
        Fail("missing " + std::string(context_label) + " runtime phase entry for case: " + std::string(case_name));
    }
    if (phase_check->output_stem.empty() || phase_check->build_context.empty() || phase_check->run_context.empty()) {
        Fail("missing " + std::string(context_label) + " runtime execution metadata for case: " +
             std::string(case_name));
    }
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               phase_check->output_stem,
                                                               phase_check->build_context,
                                                               phase_check->run_context);
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                               artifacts.build_targets.object.parent_path(),
                                               artifacts.run_output,
                                               kernel_mir,
                                               phase_check->View());
}

std::filesystem::path ResolveCanopusRoadmapPath(const std::filesystem::path& source_root,
                                                int phase_number) {
    const std::filesystem::path admin_root = source_root / "docs" / "plan" / "admin";
    if (phase_number <= 123) {
        return admin_root / "specuative_roadmap_post_phase109.txt";
    }
    if (phase_number <= 137) {
        return admin_root / "specuative_roadmap_post_phase123.txt";
    }
    if (phase_number <= 145) {
        return admin_root / "canopus_post_phase109_speculative_roadmap.txt";
    }
    return admin_root / "speculative_roadmap_post_phase145.txt";
}

FreestandingKernelRunArtifacts BuildAndRunFreestandingKernelTarget(const std::filesystem::path& mc_path,
                                                                  const std::filesystem::path& project_path,
                                                                  const std::filesystem::path& main_source_path,
                                                                  const std::filesystem::path& build_dir,
                                                                  std::string_view output_stem,
                                                                  const std::string& build_context,
                                                                  const std::string& run_context) {
    MaybeCleanBuildDir(build_dir);

    const std::string output_stem_text(output_stem);
    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / (output_stem_text + "_build_output.txt"),
                                                                 build_context);
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail(build_context + " should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / (output_stem_text + "_run_output.txt"),
                                                             run_context);
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail(run_context + " should succeed:\n" + run_output);
    }

    return {
        .build_dir = build_dir,
        .build_targets = build_targets,
        .dump_targets = dump_targets,
        .run_output = run_output,
    };
}

void CompileBootstrapCObjectAndExpectSuccess(const std::filesystem::path& source_path,
                                             const std::filesystem::path& object_path,
                                             const std::filesystem::path& output_path,
                                             const std::string& context) {
    const auto [outcome, output] = RunCommandCapture({"xcrun",
                                                      "clang",
                                                      "-target",
                                                      std::string(mc::kBootstrapTriple),
                                                      "-c",
                                                      source_path.generic_string(),
                                                      "-o",
                                                      object_path.generic_string()},
                                                     output_path,
                                                     context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should pass:\n" + output);
    }
}

void LinkBootstrapObjectsAndExpectSuccess(const std::vector<std::filesystem::path>& object_paths,
                                          const std::filesystem::path& executable_path,
                                          const std::filesystem::path& output_path,
                                          const std::string& context) {
    std::vector<std::string> args{"xcrun", "clang", "-target", std::string(mc::kBootstrapTriple)};
    for (const auto& object_path : object_paths) {
        args.push_back(object_path.generic_string());
    }
    args.push_back("-o");
    args.push_back(executable_path.generic_string());

    const auto [outcome, output] = RunCommandCapture(args, output_path, context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should succeed:\n" + output);
    }
}

std::vector<std::filesystem::path> CollectBootstrapObjectFiles(const std::filesystem::path& object_dir) {
    std::vector<std::filesystem::path> object_paths;
    for (const auto& entry : std::filesystem::directory_iterator(object_dir)) {
        if (entry.path().extension() == ".o") {
            object_paths.push_back(entry.path());
        }
    }
    std::sort(object_paths.begin(), object_paths.end());
    return object_paths;
}

std::string LinkBootstrapObjectsAndRunExpectExitCode(const std::vector<std::filesystem::path>& object_paths,
                                                     const std::filesystem::path& executable_path,
                                                     const std::filesystem::path& link_output_path,
                                                     const std::filesystem::path& run_output_path,
                                                     int expected_exit_code,
                                                     const std::string& link_context,
                                                     const std::string& run_context) {
    LinkBootstrapObjectsAndExpectSuccess(object_paths, executable_path, link_output_path, link_context);

    const auto [run_outcome, run_output] = RunCommandCapture({executable_path.generic_string()},
                                                             run_output_path,
                                                             run_context);
    if (!run_outcome.exited || run_outcome.exit_code != expected_exit_code) {
        Fail(run_context + " should exit with code " + std::to_string(expected_exit_code) + ":\n" + run_output);
    }
    return run_output;
}

std::filesystem::path ResolvePlanDocPath(const std::filesystem::path& source_root,
                                         std::string_view file_name) {
    const std::filesystem::path plan_root = source_root / "docs" / "plan";
    const std::filesystem::path active_path = plan_root / "active" / std::string(file_name);
    if (std::filesystem::exists(active_path)) {
        return active_path;
    }

    const std::filesystem::path root_path = plan_root / std::string(file_name);
    if (std::filesystem::exists(root_path)) {
        return root_path;
    }

    const std::filesystem::path archive_path = plan_root / "archive" / std::string(file_name);
    if (std::filesystem::exists(archive_path)) {
        return archive_path;
    }

    return active_path;
}

void ExpectFreestandingKernelPhaseFromArtifacts(const std::filesystem::path& source_root,
                                                const std::filesystem::path& object_dir,
                                                std::string_view run_output,
                                                std::string_view mir_text,
                                                const FreestandingKernelPhaseCheck& phase_check) {
    const std::string label(phase_check.label);
    ExpectFreestandingKernelRunContract(run_output, phase_check, source_root, label + " run");

    for (const auto object_name : phase_check.required_object_files) {
        if (!std::filesystem::exists(object_dir / std::string(object_name))) {
            Fail(label + " missing required object artifact: " + std::string(object_name));
        }
    }

    ExpectFreestandingKernelMirContract(mir_text,
                                        phase_check,
                                        ResolveFreestandingKernelGoldenPath(source_root,
                                                                            phase_check.expected_mir_contract_file),
                                        label + " MIR");
}

}  // namespace mc::tool_tests
