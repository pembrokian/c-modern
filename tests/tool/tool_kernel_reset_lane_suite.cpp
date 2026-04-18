#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iostream>
#include <sstream>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"
#include "tests/tool/tool_workflow_suite_internal.h"

namespace {

using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;
using mc::tool_tests::BuildProjectTargetAndCapture;

struct ResetLaneScenario {
    std::string_view label;
    std::string_view fixture_relative_path;
    std::string_view scenario_key;
    std::string_view context_label;
    std::string_view target_name;
    bool include_in_fast = false;
    int build_warn_ms = 900;
    int run_warn_ms = 100;
    bool requires_clean_build = false;
};

struct ResetLaneScenarioTiming {
    std::string label;
    std::chrono::milliseconds build_duration {};
    std::chrono::milliseconds run_duration {};
    bool reused_cached_build = false;
};

struct InFlightScenario {
    std::size_t index = 0;
    std::future<ResetLaneScenarioTiming> future;
};

enum class ResetLaneMode {
    fast,
    full,
};

std::string FormatDuration(std::chrono::milliseconds duration) {
    std::ostringstream stream;
    stream << duration.count() << "ms";
    return stream.str();
}

int ParsePositiveEnvOrDefault(const char* name,
                              int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return fallback;
    }

    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

std::size_t DetermineResetLaneParallelism(std::size_t scenario_count) {
    const unsigned int hardware = std::thread::hardware_concurrency();
    const int fallback = hardware == 0 ? 4 : static_cast<int>(hardware);
    const int requested = ParsePositiveEnvOrDefault("MC_RESET_LANE_JOBS", fallback);
    const std::size_t bounded = std::max<std::size_t>(1, static_cast<std::size_t>(requested));
    return std::min(bounded, std::max<std::size_t>(1, scenario_count));
}

std::string_view ResetLaneContextLabel(const ResetLaneScenario& scenario) {
    if (!scenario.context_label.empty()) {
        return scenario.context_label;
    }
    return scenario.label;
}

std::string ResetLaneScenarioStem(const ResetLaneScenario& scenario) {
    return "kernel_reset_lane_" + std::string(scenario.scenario_key);
}

std::string ResetLaneProjectDirName(const ResetLaneScenario& scenario) {
    return ResetLaneScenarioStem(scenario) + "_project";
}

std::string ResetLaneBuildDirName(const ResetLaneScenario& scenario) {
    return ResetLaneScenarioStem(scenario) + "_build";
}

std::string ResetLaneBuildOutputName(const ResetLaneScenario& scenario) {
    return ResetLaneScenarioStem(scenario) + "_build_output.txt";
}

std::string ResetLaneRunOutputName(const ResetLaneScenario& scenario) {
    return ResetLaneScenarioStem(scenario) + "_run_output.txt";
}

std::string ResetLaneBuildContext(const ResetLaneScenario& scenario) {
    return "kernel reset lane " + std::string(ResetLaneContextLabel(scenario)) + " build";
}

std::string ResetLaneRunContext(const ResetLaneScenario& scenario) {
    return "kernel reset lane " + std::string(ResetLaneContextLabel(scenario)) + " run";
}

std::string TimingFlagsForScenario(const ResetLaneScenario& scenario,
                                   const ResetLaneScenarioTiming& timing,
                                   bool enable_per_scenario_warnings,
                                   int* slow_build_count,
                                   int* slow_run_count) {
    if (!enable_per_scenario_warnings) {
        return {};
    }
    std::vector<std::string_view> flags;
    if (scenario.build_warn_ms > 0 && timing.build_duration.count() > scenario.build_warn_ms) {
        flags.push_back("slow-build");
        *slow_build_count += 1;
    }
    if (scenario.run_warn_ms > 0 && timing.run_duration.count() > scenario.run_warn_ms) {
        flags.push_back("slow-run");
        *slow_run_count += 1;
    }
    if (flags.empty()) {
        return {};
    }

    std::ostringstream stream;
    stream << " flags=";
    for (std::size_t i = 0; i < flags.size(); ++i) {
        if (i > 0) {
            stream << ',';
        }
        stream << flags[i];
    }
    return stream.str();
}

bool CollectCompletedResetLaneScenario(std::vector<ResetLaneScenarioTiming>* timings,
                                      std::vector<struct InFlightScenario>* in_flight) {
    for (std::size_t index = 0; index < in_flight->size(); ++index) {
        auto& current = (*in_flight)[index];
        if (current.future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            continue;
        }
        (*timings)[current.index] = current.future.get();
        in_flight->erase(in_flight->begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }
    return false;
}

void WaitForAnyCompletedResetLaneScenario(std::vector<ResetLaneScenarioTiming>* timings,
                                         std::vector<struct InFlightScenario>* in_flight) {
    while (!CollectCompletedResetLaneScenario(timings, in_flight)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int ResetLaneWallWarnMs(ResetLaneMode mode) {
    if (mode == ResetLaneMode::fast) {
        return ParsePositiveEnvOrDefault("MC_RESET_LANE_FAST_WARN_MS", 12000);
    }
    return ParsePositiveEnvOrDefault("MC_RESET_LANE_FULL_WARN_MS", 30000);
}

std::filesystem::path ParseBuiltExecutablePath(std::string_view build_output,
                                               std::string_view context) {
    constexpr std::string_view kPrefix = "built target ";
    const size_t line_start = build_output.rfind(kPrefix);
    if (line_start == std::string_view::npos) {
        Fail("expected build output summary for " + std::string(context) + ":\n" +
             std::string(build_output));
    }

    const size_t arrow_pos = build_output.find(" -> ", line_start);
    if (arrow_pos == std::string_view::npos) {
        Fail("expected build output executable path for " + std::string(context) + ":\n" +
             std::string(build_output));
    }

    const size_t path_start = arrow_pos + 4;
    size_t path_end = build_output.find(" (", path_start);
    if (path_end == std::string_view::npos) {
        path_end = build_output.find('\n', path_start);
    }
    if (path_end == std::string_view::npos) {
        path_end = build_output.size();
    }
    if (path_end <= path_start) {
        Fail("expected non-empty executable path for " + std::string(context) + ":\n" +
             std::string(build_output));
    }

    return std::filesystem::path(std::string(build_output.substr(path_start, path_end - path_start)));
}

std::string LoadKernelResetLaneFixtureFile(const std::filesystem::path& source_root,
                                          const std::filesystem::path& fixture_root,
                                          const std::filesystem::path& relative_path) {
    constexpr std::string_view kKernelNewSourcePlaceholder = "__KERNEL_NEW_SRC__";

    std::string text = ReadFile(fixture_root / relative_path);
    if (relative_path != std::filesystem::path{"build.toml"}) {
        return text;
    }

    if (text.find(kKernelNewSourcePlaceholder) == std::string::npos) {
        Fail("kernel reset-lane fixture build.toml is missing the kernel_new source placeholder");
    }

    const std::string kernel_src = (source_root / "kernel" / "src").generic_string();
    const std::string stdlib_src = (source_root / "stdlib").generic_string();
    size_t pos = 0;
    while ((pos = text.find(kKernelNewSourcePlaceholder, pos)) != std::string::npos) {
        text.replace(pos, kKernelNewSourcePlaceholder.size(), kernel_src);
        pos += kernel_src.size();
    }
    const std::string transport_entry = kernel_src + "/transport";
    const std::string stdlib_entry = "\"" + stdlib_src + "\"";
    const size_t transport_pos = text.find(transport_entry);
    if (transport_pos != std::string::npos && text.find(stdlib_entry) == std::string::npos) {
        const size_t insert_pos = transport_pos + transport_entry.size() + 1;
        text.insert(insert_pos, ",\n\t\"" + stdlib_src + "\"");
    }
    return text;
}

bool TreeHasNewerWriteTime(const std::filesystem::path& path,
                          std::filesystem::file_time_type reference_time) {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    if (std::filesystem::is_regular_file(path)) {
        return std::filesystem::last_write_time(path) > reference_time;
    }
    if (!std::filesystem::is_directory(path)) {
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (std::filesystem::last_write_time(entry.path()) > reference_time) {
            return true;
        }
    }
    return false;
}

bool CanReuseResetLaneBuild(const std::filesystem::path& source_root,
                           const std::filesystem::path& project_root,
                           const std::filesystem::path& project_path,
                           const std::filesystem::path& build_dir,
                           const std::filesystem::path& mc_path,
                           const ResetLaneScenario& scenario,
                           std::filesystem::path* executable_path) {
    if (scenario.requires_clean_build) {
        return false;
    }

    const std::string build_output_name = ResetLaneBuildOutputName(scenario);
    const std::string build_context = ResetLaneBuildContext(scenario);
    const std::filesystem::path build_output_path =
        build_dir / build_output_name;
    if (!std::filesystem::exists(build_output_path)) {
        return false;
    }

    const std::filesystem::path cached_executable =
        ParseBuiltExecutablePath(ReadFile(build_output_path), build_context);
    if (!std::filesystem::exists(cached_executable)) {
        return false;
    }

    const std::filesystem::file_time_type executable_time =
        std::filesystem::last_write_time(cached_executable);
    if (std::filesystem::last_write_time(mc_path) > executable_time) {
        return false;
    }

    const std::array<std::filesystem::path, 5> relevant_roots = {
        project_root,
        project_path,
        source_root / "kernel" / "src",
        source_root / "stdlib",
        source_root / "runtime",
    };
    for (const auto& root : relevant_roots) {
        if (TreeHasNewerWriteTime(root, executable_time)) {
            return false;
        }
    }

    *executable_path = cached_executable;
    return true;
}

void WriteFileIfChanged(const std::filesystem::path& path,
                        std::string_view contents) {
    if (std::filesystem::exists(path) && ReadFile(path) == contents) {
        return;
    }
    WriteFile(path, contents);
}

void RemoveExtraneousFixtureProjectEntries(const std::filesystem::path& fixture_root,
                                          const std::filesystem::path& project_root) {
    if (!std::filesystem::exists(project_root)) {
        return;
    }

    std::vector<std::filesystem::path> stale_entries;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(project_root)) {
        const std::filesystem::path relative_path = std::filesystem::relative(entry.path(), project_root);
        if (!std::filesystem::exists(fixture_root / relative_path)) {
            stale_entries.push_back(entry.path());
        }
    }

    std::sort(stale_entries.begin(), stale_entries.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.generic_string().size() > rhs.generic_string().size();
    });
    for (const auto& stale_entry : stale_entries) {
        std::filesystem::remove_all(stale_entry);
    }
}

std::set<std::string> CollectResetLaneFixtureRelativePaths(const std::filesystem::path& source_root,
                                                           const std::filesystem::path& relative_root) {
    std::set<std::string> fixture_paths;
    const std::filesystem::path root = source_root / relative_root;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }
        const std::string name = entry.path().filename().generic_string();
        if (!name.starts_with("kernel_reset_lane_")) {
            continue;
        }
        if (!std::filesystem::exists(entry.path() / "build.toml")) {
            continue;
        }
        fixture_paths.insert((relative_root / name).generic_string());
    }
    return fixture_paths;
}

std::filesystem::path InstallKernelResetLaneFixtureProject(const std::filesystem::path& source_root,
                                                           const std::filesystem::path& fixture_root,
                                                           const std::filesystem::path& project_root) {
    std::filesystem::create_directories(project_root);
    RemoveExtraneousFixtureProjectEntries(fixture_root, project_root);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(fixture_root)) {
        const std::filesystem::path relative_path = std::filesystem::relative(entry.path(), fixture_root);
        const std::filesystem::path destination_path = project_root / relative_path;
        if (entry.is_directory()) {
            std::filesystem::create_directories(destination_path);
            continue;
        }
        if (!entry.is_regular_file()) {
            continue;
        }

        WriteFileIfChanged(destination_path,
                           LoadKernelResetLaneFixtureFile(source_root, fixture_root, relative_path));
    }

    return project_root / "build.toml";
}

void ExpectKernelResetLaneRunSuccess(const ResetLaneScenario& scenario,
                                     const mc::test_support::CommandOutcome& outcome,
                                     std::string_view output) {
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("kernel reset lane " + std::string(scenario.label) + " project should return 0, got:\n" +
             std::string(output));
    }
}

ResetLaneScenarioTiming RunKernelResetLaneScenario(const std::filesystem::path& source_root,
                                                   const std::filesystem::path& binary_root,
                                                   const std::filesystem::path& mc_path,
                                                   const ResetLaneScenario& scenario) {
    const std::string build_dir_name = ResetLaneBuildDirName(scenario);
    const std::string build_output_name = ResetLaneBuildOutputName(scenario);
    const std::string run_output_name = ResetLaneRunOutputName(scenario);
    const std::string build_context = ResetLaneBuildContext(scenario);
    const std::string run_context = ResetLaneRunContext(scenario);

    std::filesystem::path project_root;
    std::filesystem::path project_path;
    if (scenario.fixture_relative_path.empty()) {
        project_root = source_root / "kernel";
        project_path = source_root / "kernel" / "build.toml";
    } else {
        const std::filesystem::path fixture_root = source_root / std::string(scenario.fixture_relative_path);
        project_root = binary_root / ResetLaneProjectDirName(scenario);
        project_path = InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);
    }

    const std::filesystem::path build_dir = binary_root / build_dir_name;
    std::filesystem::path executable_path;
    bool reused_cached_build = false;
    if (scenario.requires_clean_build) {
        std::filesystem::remove_all(build_dir);
    }
    const auto build_start = std::chrono::steady_clock::now();
    if (CanReuseResetLaneBuild(source_root,
                               project_root,
                               project_path,
                               build_dir,
                               mc_path,
                               scenario,
                               &executable_path)) {
        reused_cached_build = true;
    } else {
        const std::string build_output = BuildProjectTargetAndCapture(mc_path,
                                                                      project_path,
                                                                      build_dir,
                                                                      scenario.target_name,
                                                                      build_output_name,
                                                                      build_context);
        executable_path = ParseBuiltExecutablePath(build_output, build_context);
    }
    const auto build_end = std::chrono::steady_clock::now();

    std::vector<std::string> command = {executable_path.generic_string()};

    const auto run_start = std::chrono::steady_clock::now();
    const auto [run_outcome, run_output] = mc::test_support::RunCommandCaptureInDir(command,
                                                                                     build_dir,
                                                                                     build_dir / run_output_name,
                                                                                     run_context);
    const auto run_end = std::chrono::steady_clock::now();
    ExpectKernelResetLaneRunSuccess(scenario, run_outcome, run_output);

    return {
        .label = std::string(scenario.label),
        .build_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start),
        .run_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(run_end - run_start),
        .reused_cached_build = reused_cached_build,
    };
}

constexpr std::array<ResetLaneScenario, 61> kResetLaneScenarios = {{
    {.label = "repo project", .scenario_key = "repo", .target_name = "kernel", .include_in_fast = true, .build_warn_ms = 2000},
    {.label = "smoke", .fixture_relative_path = "tests/smoke/kernel_reset_lane_serial_round_trip", .scenario_key = "smoke", .target_name = "app", .include_in_fast = true},
    {.label = "retained state", .fixture_relative_path = "tests/system/kernel_reset_lane_retained_log", .scenario_key = "retained_state", .context_label = "retained-state", .target_name = "app", .include_in_fast = true},
    {.label = "observability", .fixture_relative_path = "tests/system/kernel_reset_lane_serial_observability", .scenario_key = "observability", .target_name = "app"},
    {.label = "kv roundtrip", .fixture_relative_path = "tests/system/kernel_reset_lane_kv_roundtrip", .scenario_key = "kv_roundtrip", .context_label = "kv-roundtrip", .target_name = "app"},
    {.label = "service composition", .fixture_relative_path = "tests/system/kernel_reset_lane_service_composition", .scenario_key = "service_composition", .target_name = "app"},
    {.label = "boot", .fixture_relative_path = "tests/smoke/kernel_reset_lane_boot", .scenario_key = "boot", .target_name = "app"},
    {.label = "image", .fixture_relative_path = "tests/smoke/kernel_reset_lane_image", .scenario_key = "image", .target_name = "app", .build_warn_ms = 2000},
    {.label = "service identity", .fixture_relative_path = "tests/system/kernel_reset_lane_service_identity", .scenario_key = "service_identity", .target_name = "app", .include_in_fast = true},
    {.label = "temporal backpressure", .fixture_relative_path = "tests/system/kernel_reset_lane_temporal_backpressure", .scenario_key = "temporal_backpressure", .target_name = "app"},
    {.label = "multi client", .fixture_relative_path = "tests/system/kernel_reset_lane_multi_client", .scenario_key = "multi_client", .context_label = "multi-client", .target_name = "app"},
    {.label = "long lived coherence", .fixture_relative_path = "tests/system/kernel_reset_lane_long_lived_coherence", .scenario_key = "long_lived_coherence", .context_label = "long-lived coherence", .target_name = "app"},
    {.label = "cross service failure", .fixture_relative_path = "tests/system/kernel_reset_lane_cross_service_failure", .scenario_key = "cross_service_failure", .context_label = "cross-service failure", .target_name = "app"},
    {.label = "workflow service", .fixture_relative_path = "tests/system/kernel_reset_lane_workflow_service", .scenario_key = "workflow_service", .context_label = "workflow-service", .target_name = "app"},
    {.label = "observability boundary", .fixture_relative_path = "tests/system/kernel_reset_lane_observability", .scenario_key = "phase159_observability", .context_label = "phase 159 observability", .target_name = "app"},
    {.label = "model boundary", .fixture_relative_path = "tests/system/kernel_reset_lane_model_boundary", .scenario_key = "phase160_model_boundary", .context_label = "phase 160 model boundary", .target_name = "app"},
    {.label = "delivery witness", .fixture_relative_path = "tests/system/kernel_reset_lane_delivery_witness", .scenario_key = "phase161_delivery_witness", .context_label = "phase 161 delivery witness", .target_name = "app"},
    {.label = "static topology", .fixture_relative_path = "tests/system/kernel_reset_lane_static_topology", .scenario_key = "phase170_static_topology", .context_label = "phase 170 static topology", .target_name = "app", .include_in_fast = true},
    {.label = "smp boundary", .fixture_relative_path = "tests/system/kernel_reset_lane_smp_boundary", .scenario_key = "phase173_smp_boundary", .context_label = "phase 173 smp boundary", .target_name = "app"},
    {.label = "topology growth", .fixture_relative_path = "tests/system/kernel_reset_lane_phase176_growth", .scenario_key = "phase176_growth", .context_label = "phase 176 growth", .target_name = "app"},
    {.label = "hostile shell", .fixture_relative_path = "tests/system/kernel_reset_lane_phase177_hostile_shell", .scenario_key = "phase177_hostile_shell", .context_label = "phase 177 hostile shell", .target_name = "app"},
    {.label = "topology restart", .fixture_relative_path = "tests/system/kernel_reset_lane_phase178_topology_restart", .scenario_key = "phase178_topology_restart", .context_label = "phase 178 topology restart", .target_name = "app", .include_in_fast = true},
    {.label = "retained reload", .fixture_relative_path = "tests/system/kernel_reset_lane_phase179_retained_reload", .scenario_key = "phase179_retained_reload", .context_label = "phase 179 retained reload", .target_name = "app"},
    {.label = "kv retained reload", .fixture_relative_path = "tests/system/kernel_reset_lane_phase181_kv_retained_reload", .scenario_key = "phase181_kv_retained_reload", .context_label = "phase 181 kv retained reload", .target_name = "app"},
    {.label = "endpoint authority", .fixture_relative_path = "tests/system/kernel_reset_lane_phase183_endpoint_authority", .scenario_key = "phase183_endpoint_authority", .context_label = "phase 183 endpoint authority", .target_name = "app"},
    {.label = "capability leakage", .fixture_relative_path = "tests/system/kernel_reset_lane_phase184_capability_leakage", .scenario_key = "phase184_capability_leakage", .context_label = "phase 184 capability leakage", .target_name = "app"},
    {.label = "transferred handle", .fixture_relative_path = "tests/system/kernel_reset_lane_phase185_transferred_handle", .scenario_key = "phase185_transferred_handle", .context_label = "phase 185 transferred handle", .target_name = "app"},
    {.label = "multi-handle transfer", .fixture_relative_path = "tests/system/kernel_reset_lane_phase198_multi_handle_transfer", .scenario_key = "phase198_multi_handle_transfer", .context_label = "phase 198 multi-handle transfer", .target_name = "app", .build_warn_ms = 1000},
    {.label = "retained queue reload", .fixture_relative_path = "tests/system/kernel_reset_lane_phase187_retained_queue_reload", .scenario_key = "phase187_retained_queue_reload", .context_label = "phase 187 retained queue reload", .target_name = "app"},
    {.label = "ticket restart", .fixture_relative_path = "tests/system/kernel_reset_lane_phase188_ticket_restart", .scenario_key = "phase188_ticket_restart", .context_label = "phase 188 ticket restart", .target_name = "app"},
    {.label = "shell lifecycle", .fixture_relative_path = "tests/system/kernel_reset_lane_phase189_shell_lifecycle", .scenario_key = "phase189_shell_lifecycle", .context_label = "phase 189 shell lifecycle", .target_name = "app"},
    {.label = "shell identity", .fixture_relative_path = "tests/system/kernel_reset_lane_phase193_shell_identity", .scenario_key = "phase193_shell_identity", .context_label = "phase 193 shell identity", .target_name = "app"},
    {.label = "queue observability", .fixture_relative_path = "tests/system/kernel_reset_lane_phase196_queue_observability", .scenario_key = "phase196_queue_observability", .context_label = "phase 196 queue observability", .target_name = "app", .build_warn_ms = 1000},
    {.label = "retained coordination", .fixture_relative_path = "tests/system/kernel_reset_lane_phase197_retained_coordination", .scenario_key = "phase197_retained_coordination", .context_label = "phase 197 retained coordination", .target_name = "app", .include_in_fast = true},
    {.label = "workset identity", .fixture_relative_path = "tests/system/kernel_reset_lane_phase200_workset_identity", .scenario_key = "phase200_workset_identity", .context_label = "phase 200 workset identity", .target_name = "app", .build_warn_ms = 1000},
    {.label = "retained audit coordination", .fixture_relative_path = "tests/system/kernel_reset_lane_phase201_retained_audit_coordination", .scenario_key = "phase201_retained_audit_coordination", .context_label = "phase 201 retained audit coordination", .target_name = "app"},
    {.label = "retained summary", .fixture_relative_path = "tests/system/kernel_reset_lane_phase206_retained_summary", .scenario_key = "phase206_retained_summary", .context_label = "phase 206 retained summary", .target_name = "app"},
    {.label = "retained restart identity", .fixture_relative_path = "tests/system/kernel_reset_lane_phase227_retained_restart_identity", .scenario_key = "phase227_retained_restart_identity", .context_label = "phase 227 retained restart identity", .target_name = "app", .include_in_fast = true},
    {.label = "retained policy", .fixture_relative_path = "tests/system/kernel_reset_lane_phase208_retained_policy", .scenario_key = "phase208_retained_policy", .context_label = "phase 208 retained policy", .target_name = "app", .build_warn_ms = 1000},
    {.label = "authority inspection", .fixture_relative_path = "tests/system/kernel_reset_lane_phase212_authority_inspection", .scenario_key = "phase212_authority_inspection", .context_label = "phase 212 authority inspection", .target_name = "app", .build_warn_ms = 1000},
    {.label = "service state", .fixture_relative_path = "tests/system/kernel_reset_lane_phase214_service_state", .scenario_key = "phase214_service_state", .context_label = "phase 214 service state", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "queue pressure", .fixture_relative_path = "tests/system/kernel_reset_lane_phase215_queue_pressure", .scenario_key = "phase215_queue_pressure", .context_label = "phase 215 queue pressure", .target_name = "app", .include_in_fast = true},
    {.label = "stall consequence", .fixture_relative_path = "tests/system/kernel_reset_lane_phase216_stall_consequence", .scenario_key = "phase216_stall_consequence", .context_label = "phase 216 stall consequence", .target_name = "app", .include_in_fast = true},
    {.label = "retained durable boundary", .fixture_relative_path = "tests/system/kernel_reset_lane_phase217_retained_durable_boundary", .scenario_key = "phase217_retained_durable_boundary", .context_label = "phase 217 retained durable boundary", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "timer task service", .fixture_relative_path = "tests/system/kernel_reset_lane_phase220_timer_task_service", .scenario_key = "phase220_timer_task_service", .context_label = "phase 220 timer task service", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "task completion", .fixture_relative_path = "tests/system/kernel_reset_lane_phase225_task_completion", .scenario_key = "phase225_task_completion", .context_label = "phase 225 task completion", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "file growth", .fixture_relative_path = "tests/system/kernel_reset_lane_phase226_file_growth", .scenario_key = "phase226_file_growth", .context_label = "phase 226 file growth", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "durable journal", .fixture_relative_path = "tests/system/kernel_reset_lane_phase230_durable_journal", .scenario_key = "phase230_durable_journal", .context_label = "phase 230 durable journal", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "completion mailbox", .fixture_relative_path = "tests/system/kernel_reset_lane_phase232_completion_mailbox", .scenario_key = "phase232_completion_mailbox", .context_label = "phase 232 completion mailbox", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "delegation lease", .fixture_relative_path = "tests/system/kernel_reset_lane_phase233_delegation_lease", .scenario_key = "phase233_delegation_lease", .context_label = "phase 233 delegation lease", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "durable object store", .fixture_relative_path = "tests/system/kernel_reset_lane_phase234_durable_object_store", .scenario_key = "phase234_durable_object_store", .context_label = "phase 234 durable object store", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "named object update workflow", .fixture_relative_path = "tests/system/kernel_reset_lane_phase235_named_object_update_workflow", .scenario_key = "phase235_named_object_update_workflow", .context_label = "phase 235 named object update workflow", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "delegated named object processing", .fixture_relative_path = "tests/system/kernel_reset_lane_phase236_delegated_named_object_processing", .scenario_key = "phase236_delegated_named_object_processing", .context_label = "phase 236 delegated named object processing", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "named object delivery pressure", .fixture_relative_path = "tests/system/kernel_reset_lane_phase237_named_object_delivery_pressure", .scenario_key = "phase237_named_object_delivery_pressure", .context_label = "phase 237 named object delivery pressure", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "connection service", .fixture_relative_path = "tests/system/kernel_reset_lane_phase238_connection_service", .scenario_key = "phase238_connection_service", .context_label = "phase 238 connection service", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "connection backed workflow", .fixture_relative_path = "tests/system/kernel_reset_lane_phase239_connection_backed_workflow", .scenario_key = "phase239_connection_backed_workflow", .context_label = "phase 239 connection backed workflow", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "external ingress completion pressure", .fixture_relative_path = "tests/system/kernel_reset_lane_phase240_external_ingress_completion_pressure", .scenario_key = "phase240_external_ingress_completion_pressure", .context_label = "phase 240 external ingress completion pressure", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "delegated external request handling", .fixture_relative_path = "tests/system/kernel_reset_lane_phase241_delegated_external_request_handling", .scenario_key = "phase241_delegated_external_request_handling", .context_label = "phase 241 delegated external request handling", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "update manifest and staged artifact store", .fixture_relative_path = "tests/system/kernel_reset_lane_phase242_update_store", .scenario_key = "phase242_update_store", .context_label = "phase 242 update manifest and staged artifact store", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "restart-safe update apply workflow", .fixture_relative_path = "tests/system/kernel_reset_lane_phase243_update_apply_workflow", .scenario_key = "phase243_update_apply_workflow", .context_label = "phase 243 restart-safe update apply workflow", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
    {.label = "delegated installer authority", .fixture_relative_path = "tests/system/kernel_reset_lane_phase244_delegated_installer_authority", .scenario_key = "phase244_delegated_installer_authority", .context_label = "phase 244 delegated installer authority", .target_name = "app", .include_in_fast = true, .build_warn_ms = 1000},
}};

std::vector<const ResetLaneScenario*> SelectResetLaneScenarios(ResetLaneMode mode) {
    std::vector<const ResetLaneScenario*> selected;
    selected.reserve(kResetLaneScenarios.size());
    for (const auto& scenario : kResetLaneScenarios) {
        if (mode == ResetLaneMode::fast && !scenario.include_in_fast) {
            continue;
        }
        selected.push_back(&scenario);
    }
    return selected;
}

std::set<std::string> CollectCoveredResetLaneFixturePaths() {
    std::set<std::string> covered_paths;
    for (const auto& scenario : kResetLaneScenarios) {
        if (scenario.fixture_relative_path.empty()) {
            continue;
        }
        covered_paths.insert(std::string(scenario.fixture_relative_path));
    }
    return covered_paths;
}

void ValidateResetLaneScenarioCoverage(const std::filesystem::path& source_root) {
    std::set<std::string> expected_paths =
        CollectResetLaneFixtureRelativePaths(source_root, "tests/smoke");
    const std::set<std::string> system_paths =
        CollectResetLaneFixtureRelativePaths(source_root, "tests/system");
    expected_paths.insert(system_paths.begin(), system_paths.end());

    const std::set<std::string> covered_paths = CollectCoveredResetLaneFixturePaths();

    std::vector<std::string> missing_paths;
    for (const auto& expected_path : expected_paths) {
        if (!covered_paths.contains(expected_path)) {
            missing_paths.push_back(expected_path);
        }
    }

    if (!missing_paths.empty()) {
        std::string message =
            "reset-lane workflow table is missing fixture coverage for:";
        for (const auto& missing_path : missing_paths) {
            message += "\n- " + missing_path;
        }
        Fail(message);
    }
}

}  // namespace

namespace mc::tool_tests {

void RunWorkflowKernelResetLaneSuiteImpl(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path,
                                         ResetLaneMode mode) {
    const auto coverage_start = std::chrono::steady_clock::now();
    ValidateResetLaneScenarioCoverage(source_root);
    const auto coverage_end = std::chrono::steady_clock::now();
    const auto execution_start = std::chrono::steady_clock::now();

    const std::vector<const ResetLaneScenario*> selected = SelectResetLaneScenarios(mode);
    const std::size_t jobs = DetermineResetLaneParallelism(selected.size());
    const bool enable_per_scenario_warnings = jobs == 1;

    std::vector<ResetLaneScenarioTiming> timings(selected.size());
    std::chrono::milliseconds total_build_duration {};
    std::chrono::milliseconds total_run_duration {};
    int slow_build_count = 0;
    int slow_run_count = 0;

    std::vector<InFlightScenario> in_flight;
    in_flight.reserve(jobs);

    for (std::size_t index = 0; index < selected.size(); ++index) {
        const ResetLaneScenario scenario = *selected[index];
        if (in_flight.size() >= jobs) {
            WaitForAnyCompletedResetLaneScenario(&timings, &in_flight);
        }
        in_flight.push_back(InFlightScenario{
            .index = index,
            .future = std::async(std::launch::async,
                                 [source_root, binary_root, mc_path, scenario]() {
                                     return RunKernelResetLaneScenario(source_root, binary_root, mc_path, scenario);
                                 }),
        });
    }

    while (!in_flight.empty()) {
        WaitForAnyCompletedResetLaneScenario(&timings, &in_flight);
    }

    for (std::size_t index = 0; index < selected.size(); ++index) {
        const auto& scenario = *selected[index];
        const ResetLaneScenarioTiming& timing = timings[index];
        total_build_duration += timing.build_duration;
        total_run_duration += timing.run_duration;
        const std::string flags = TimingFlagsForScenario(scenario,
                                                         timing,
                                                         enable_per_scenario_warnings,
                                                         &slow_build_count,
                                                         &slow_run_count);
        std::cout << "kernel-reset-lane: " << timing.label << " build="
                  << FormatDuration(timing.build_duration) << " run="
                  << FormatDuration(timing.run_duration)
                  << (timing.reused_cached_build ? " cache=hit" : " cache=miss")
                  << flags << '\n';
    }

    const std::chrono::milliseconds coverage_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(coverage_end - coverage_start);
    const std::chrono::milliseconds wall_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - execution_start);
    const std::string_view mode_label = mode == ResetLaneMode::fast ? "fast" : "full";
    const bool slow_wall = wall_duration.count() > ResetLaneWallWarnMs(mode);
    std::cout << "kernel-reset-lane: coverage=" << FormatDuration(coverage_duration)
              << " mode=" << mode_label << " parallel=" << jobs
              << " scenarios=" << timings.size() << " wall=" << FormatDuration(wall_duration)
              << " aggregate-build="
              << FormatDuration(total_build_duration) << " aggregate-run="
              << FormatDuration(total_run_duration) << " aggregate-total="
              << FormatDuration(coverage_duration + total_build_duration + total_run_duration)
              << (slow_wall ? " flags=slow-wall" : "")
              << " slow-build=" << slow_build_count << " slow-run=" << slow_run_count
              << '\n';
}

void RunWorkflowKernelResetLaneSuite(const std::filesystem::path& source_root,
                                     const std::filesystem::path& binary_root,
                                     const std::filesystem::path& mc_path) {
    RunWorkflowKernelResetLaneSuiteImpl(source_root, binary_root, mc_path, ResetLaneMode::fast);
}

void RunWorkflowKernelResetLaneFullSuite(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path) {
    RunWorkflowKernelResetLaneSuiteImpl(source_root, binary_root, mc_path, ResetLaneMode::full);
}

}  // namespace mc::tool_tests
