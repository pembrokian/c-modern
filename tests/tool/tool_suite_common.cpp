#include "tests/tool/tool_suite_common.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <vector>

#include "compiler/support/target.h"
#include "tests/support/process_utils.h"

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

std::string NormalizeProjectionText(std::string_view text) {
    std::string normalized(text);
    while (!normalized.empty() && (normalized.back() == '\n' || normalized.back() == '\r')) {
        normalized.pop_back();
    }
    return normalized;
}

std::vector<std::string> SplitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::istringstream stream{std::string(text)};
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(std::move(line));
    }
    return lines;
}

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
    if (!line.starts_with(prefix) || line.back() != ']') {
        Fail("runtime phase directory contains an invalid inline array for key: " + std::string(key));
    }
    std::string_view inner = line.substr(prefix.size(), line.size() - prefix.size() - 1);
    std::vector<std::string> values;
    while (!inner.empty()) {
        while (!inner.empty() && (inner.front() == ' ' || inner.front() == ',')) {
            inner.remove_prefix(1);
        }
        if (inner.empty()) {
            break;
        }
        if (inner.front() != '"') {
            Fail("runtime phase directory inline array should contain quoted strings for key: " + std::string(key));
        }
        inner.remove_prefix(1);
        const std::size_t end_quote = inner.find('"');
        if (end_quote == std::string_view::npos) {
            Fail("runtime phase directory inline array is missing a closing quote for key: " + std::string(key));
        }
        values.emplace_back(inner.substr(0, end_quote));
        inner.remove_prefix(end_quote + 1);
    }
    return values;
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
    if (phase_check.run_contract == kFreestandingKernelRunContractStdoutContainsV1) {
        if (phase_check.expected_run_lines_file.empty()) {
            Fail("runtime phase directory missing stdout run golden for run contract: " +
                 phase_toml_path.generic_string());
        }
        return;
    }
    if (phase_check.run_contract == kFreestandingKernelRunContractSuccessExitV1) {
        return;
    }
    Fail("runtime phase directory has an unsupported run contract: " + phase_toml_path.generic_string());
}

void ExpectFreestandingKernelRunContract(std::string_view run_output,
                                         const FreestandingKernelPhaseCheck& phase_check,
                                         const std::filesystem::path& source_root,
                                         const std::string& context) {
    if (phase_check.run_contract == kFreestandingKernelRunContractSuccessExitV1) {
        return;
    }
    if (phase_check.run_contract == kFreestandingKernelRunContractStdoutContainsV1) {
        ExpectTextContainsLinesFile(run_output,
                                    ResolveFreestandingKernelGoldenPath(source_root,
                                                                        phase_check.expected_run_lines_file),
                                    context);
        return;
    }
    Fail(context + " unsupported run contract: " + std::string(phase_check.run_contract));
}

void ExpectFreestandingKernelMirContract(std::string_view mir_text,
                                         const FreestandingKernelPhaseCheck& phase_check,
                                         const std::filesystem::path& expected_contract_path,
                                         const std::string& context) {
    if (phase_check.mir_contract == kFreestandingKernelMirContractFirstMatchProjectionV1) {
        ExpectMirFirstMatchProjectionFileSpan(mir_text,
                                              phase_check.mir_selectors,
                                              expected_contract_path,
                                              context);
        return;
    }
    Fail(context + " unsupported MIR contract: " + std::string(phase_check.mir_contract));
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
    if (descriptor.kind == "standalone_manual_relink") {
        if (descriptor.project_name.empty() || descriptor.main_source_file.empty() ||
            descriptor.driver_support_source_file.empty() || descriptor.manual_support_source_file.empty() ||
            descriptor.expected_exit_code < 0) {
            Fail("standalone manual relink descriptor missing required fields: " + descriptor_path.generic_string());
        }
        return;
    }
    if (descriptor.kind == "kernel_manual_relink") {
        if (descriptor.output_stem.empty() || descriptor.build_context.empty() || descriptor.run_context.empty() ||
            descriptor.expected_run_lines_file.empty()) {
            Fail("kernel manual relink descriptor missing required fields: " + descriptor_path.generic_string());
        }
        return;
    }
    if (descriptor.kind == "file_contains") {
        if (descriptor.source_file.empty() || descriptor.expected_run_lines_file.empty()) {
            Fail("file contains descriptor missing required fields: " + descriptor_path.generic_string());
        }
        return;
    }
    Fail("unknown kernel artifact audit kind in descriptor: " + descriptor_path.generic_string());
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
    const std::filesystem::path runtime_root = ResolveFreestandingKernelGoldenPath(source_root,
                                                                                   kKernelRuntimeRootRelativePath);
    std::vector<FreestandingKernelRuntimePhaseDescriptor> phase_checks;
    if (!std::filesystem::exists(runtime_root)) {
        return phase_checks;
    }

    std::vector<std::filesystem::path> phase_dirs;
    for (const auto& entry : std::filesystem::directory_iterator(runtime_root)) {
        if (entry.is_directory()) {
            phase_dirs.push_back(entry.path());
        }
    }
    std::sort(phase_dirs.begin(), phase_dirs.end());

    enum class ArrayMode {
        none,
        selectors,
    };

    for (const auto& phase_dir : phase_dirs) {
        const std::filesystem::path phase_toml_path = phase_dir / "phase.toml";
        if (!std::filesystem::exists(phase_toml_path)) {
            continue;
        }

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
        if (current.shard != shard) {
            continue;
        }
        if (current.run_contract.empty()) {
            current.run_contract = current.expected_run_lines_file.empty()
                                       ? std::string(kFreestandingKernelRunContractSuccessExitV1)
                                       : std::string(kFreestandingKernelRunContractStdoutContainsV1);
        }
        current.RefreshViews();
        ValidateFreestandingKernelRuntimePhaseDescriptor(current, phase_toml_path, shard);
        phase_checks.push_back(std::move(current));
    }

    return phase_checks;
}

std::vector<FreestandingKernelRuntimePhaseDescriptor> LoadAllFreestandingKernelRuntimePhaseDescriptors(
    const std::filesystem::path& source_root) {
    const std::filesystem::path runtime_root = ResolveFreestandingKernelGoldenPath(source_root,
                                                                                   kKernelRuntimeRootRelativePath);
    std::vector<FreestandingKernelRuntimePhaseDescriptor> phase_checks;
    if (!std::filesystem::exists(runtime_root)) {
        return phase_checks;
    }

    std::vector<std::filesystem::path> phase_dirs;
    for (const auto& entry : std::filesystem::directory_iterator(runtime_root)) {
        if (entry.is_directory()) {
            phase_dirs.push_back(entry.path());
        }
    }
    std::sort(phase_dirs.begin(), phase_dirs.end());

    enum class ArrayMode {
        none,
        selectors,
    };

    for (const auto& phase_dir : phase_dirs) {
        const std::filesystem::path phase_toml_path = phase_dir / "phase.toml";
        if (!std::filesystem::exists(phase_toml_path)) {
            continue;
        }

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
        if (current.run_contract.empty()) {
            current.run_contract = current.expected_run_lines_file.empty()
                                       ? std::string(kFreestandingKernelRunContractSuccessExitV1)
                                       : std::string(kFreestandingKernelRunContractStdoutContainsV1);
        }
        current.RefreshViews();
        ValidateFreestandingKernelRuntimePhaseDescriptor(current, phase_toml_path, current.shard);
        phase_checks.push_back(std::move(current));
    }

    return phase_checks;
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

void MaybeCleanBuildDir(const std::filesystem::path& build_dir) {
    const char* force_clean_env = std::getenv("MC_TEST_FORCE_CLEAN_BUILDS");
    if (force_clean_env == nullptr || std::string_view(force_clean_env) == "0") {
        return;
    }
    std::filesystem::remove_all(build_dir);
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
              "package = \"app\"\n"
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
              "package = \"app\"\n"
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

std::string BuildProjectTargetAndCapture(const std::filesystem::path& mc_path,
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
    return output;
}

std::string BuildProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
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
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail(context + " should fail:\n" + output);
    }
    return output;
}

std::string CheckProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context) {
    std::vector<std::string> args {
        mc_path.generic_string(),
        "check",
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
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail(context + " should fail:\n" + output);
    }
    return output;
}

    void ExpectExitCodeAtLeast(const mc::test_support::CommandOutcome& outcome,
                              int minimum_exit_code,
                              std::string_view output,
                              const std::string& context) {
        if (!outcome.exited) {
            std::string message = context + ": process did not exit";
            if (!output.empty()) {
                message += "\n";
                message += output;
            }
            Fail(message);
        }
        if (outcome.exit_code < minimum_exit_code) {
            std::string message = context + ": expected exit code >= " + std::to_string(minimum_exit_code) +
                                  ", got " + std::to_string(outcome.exit_code);
            if (!output.empty()) {
                message += "\n";
                message += output;
            }
            Fail(message);
        }
    }

std::string CheckProjectTargetAndExpectSuccess(const std::filesystem::path& mc_path,
                                               const std::filesystem::path& project_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view target_name,
                                               std::string_view output_name,
                                               const std::string& context) {
    std::vector<std::string> args {
        mc_path.generic_string(),
        "check",
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

std::string RunProjectTargetAndExpectFailure(const std::filesystem::path& mc_path,
                                             const std::filesystem::path& project_path,
                                             const std::filesystem::path& build_dir,
                                             std::string_view target_name,
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

    const auto [outcome, output] = RunCommandCapture(args,
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail(context + " should fail:\n" + output);
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

void ExpectMirFirstMatchProjection(std::string_view mir_text,
                                   std::initializer_list<std::string_view> selectors,
                                   std::string_view expected_projection,
                                   const std::string& context) {
    ExpectMirFirstMatchProjectionSpan(mir_text,
                                      std::span<const std::string_view>(selectors.begin(), selectors.size()),
                                      expected_projection,
                                      context);
}

void ExpectMirFirstMatchProjectionSpan(std::string_view mir_text,
                                       std::span<const std::string_view> selectors,
                                       std::string_view expected_projection,
                                       const std::string& context) {
    const auto lines = SplitLines(mir_text);

    std::string actual_projection;
    bool first_line = true;
    for (const auto selector : selectors) {
        bool found = false;
        for (const auto& line : lines) {
            if (line.find(selector) == std::string::npos) {
                continue;
            }
            if (!first_line) {
                actual_projection += '\n';
            }
            actual_projection += line;
            first_line = false;
            found = true;
            break;
        }
        if (!found) {
            Fail(context + " missing MIR projection selector: " + std::string(selector));
        }
    }

    const auto normalized_expected = NormalizeProjectionText(expected_projection);
    const auto normalized_actual = NormalizeProjectionText(actual_projection);
    if (normalized_actual != normalized_expected) {
        Fail(context + " projection mismatch:\nexpected:\n" + normalized_expected + "\nactual:\n" + normalized_actual);
    }
}

void ExpectTextContainsLines(std::string_view actual_text,
                             std::string_view expected_lines,
                             const std::string& context) {
    for (auto expected_line : SplitLines(expected_lines)) {
        if (!expected_line.empty() && expected_line.back() == '\r') {
            expected_line.pop_back();
        }
        if (expected_line.empty()) {
            continue;
        }
        if (actual_text.find(expected_line) == std::string::npos) {
            Fail(context + " missing expected text line: " + expected_line);
        }
    }
}

void ExpectTextContainsLinesFile(std::string_view actual_text,
                                 const std::filesystem::path& expected_lines_path,
                                 const std::string& context) {
    ExpectTextContainsLines(actual_text,
                            ReadFile(expected_lines_path),
                            context + " golden=" + expected_lines_path.generic_string());
}

void ExpectMirFirstMatchProjectionFile(std::string_view mir_text,
                                       std::initializer_list<std::string_view> selectors,
                                       const std::filesystem::path& expected_projection_path,
                                       const std::string& context) {
    ExpectMirFirstMatchProjectionFileSpan(mir_text,
                                          std::span<const std::string_view>(selectors.begin(), selectors.size()),
                                          expected_projection_path,
                                          context);
}

void ExpectMirFirstMatchProjectionFileSpan(std::string_view mir_text,
                                           std::span<const std::string_view> selectors,
                                           const std::filesystem::path& expected_projection_path,
                                           const std::string& context) {
    ExpectMirFirstMatchProjectionSpan(mir_text,
                                      selectors,
                                      ReadFile(expected_projection_path),
                                      context + " golden=" + expected_projection_path.generic_string());
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
