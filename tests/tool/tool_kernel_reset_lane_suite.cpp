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
    std::string_view project_dir_name;
    std::string_view build_dir_name;
    std::string_view target_name;
    std::string_view build_output_name;
    std::string_view run_output_name;
    std::string_view build_context;
    std::string_view run_context;
    bool include_in_fast = false;
    int build_warn_ms = 0;
    int run_warn_ms = 0;
    bool requires_clean_build = false;
};

struct ResetLaneScenarioTiming {
    std::string label;
    std::chrono::milliseconds build_duration {};
    std::chrono::milliseconds run_duration {};
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
    size_t pos = 0;
    while ((pos = text.find(kKernelNewSourcePlaceholder, pos)) != std::string::npos) {
        text.replace(pos, kKernelNewSourcePlaceholder.size(), kernel_src);
        pos += kernel_src.size();
    }
    return text;
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
    stale_entries.erase(std::unique(stale_entries.begin(), stale_entries.end()), stale_entries.end());
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
    std::filesystem::path project_path;
    if (scenario.fixture_relative_path.empty()) {
        project_path = source_root / "kernel" / "build.toml";
    } else {
        const std::filesystem::path fixture_root = source_root / std::string(scenario.fixture_relative_path);
        const std::filesystem::path project_root = binary_root / std::string(scenario.project_dir_name);
        project_path = InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);
    }

    const std::filesystem::path build_dir = binary_root / std::string(scenario.build_dir_name);
    if (scenario.requires_clean_build) {
        std::filesystem::remove_all(build_dir);
    }
    const auto build_start = std::chrono::steady_clock::now();
    const std::string build_output = BuildProjectTargetAndCapture(mc_path,
                                                                  project_path,
                                                                  build_dir,
                                                                  scenario.target_name,
                                                                  scenario.build_output_name,
                                                                  std::string(scenario.build_context));
    const auto build_end = std::chrono::steady_clock::now();
    const std::filesystem::path executable_path =
        ParseBuiltExecutablePath(build_output, scenario.build_context);

    std::vector<std::string> command = {executable_path.generic_string()};

    const auto run_start = std::chrono::steady_clock::now();
    const auto [run_outcome, run_output] = RunCommandCapture(command,
                                                             build_dir / std::string(scenario.run_output_name),
                                                             std::string(scenario.run_context));
    const auto run_end = std::chrono::steady_clock::now();
    ExpectKernelResetLaneRunSuccess(scenario, run_outcome, run_output);

    return {
        .label = std::string(scenario.label),
        .build_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start),
        .run_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(run_end - run_start),
    };
}

constexpr std::array<ResetLaneScenario, 46> kResetLaneScenarios = {{
    {"repo project", "", "", "kernel_reset_lane_repo_build", "kernel", "kernel_reset_lane_repo_build_output.txt", "kernel_reset_lane_repo_run_output.txt", "kernel reset lane repo project build", "kernel reset lane repo project run", true, 2000, 100},
    {"smoke", "tests/smoke/kernel_reset_lane_serial_round_trip", "kernel_reset_lane_smoke_project", "kernel_reset_lane_smoke_build", "app", "kernel_reset_lane_smoke_build_output.txt", "kernel_reset_lane_smoke_run_output.txt", "kernel reset lane smoke build", "kernel reset lane smoke run", true, 900, 100},
    {"retained state", "tests/system/kernel_reset_lane_retained_log", "kernel_reset_lane_retained_state_project", "kernel_reset_lane_retained_state_build", "app", "kernel_reset_lane_retained_state_build_output.txt", "kernel_reset_lane_retained_state_run_output.txt", "kernel reset lane retained-state build", "kernel reset lane retained-state run", true, 900, 100},
    {"observability", "tests/system/kernel_reset_lane_serial_observability", "kernel_reset_lane_observability_project", "kernel_reset_lane_observability_build", "app", "kernel_reset_lane_observability_build_output.txt", "kernel_reset_lane_observability_run_output.txt", "kernel reset lane observability build", "kernel reset lane observability run", false, 900, 100},
    {"kv roundtrip", "tests/system/kernel_reset_lane_kv_roundtrip", "kernel_reset_lane_kv_roundtrip_project", "kernel_reset_lane_kv_roundtrip_build", "app", "kernel_reset_lane_kv_roundtrip_build_output.txt", "kernel_reset_lane_kv_roundtrip_run_output.txt", "kernel reset lane kv-roundtrip build", "kernel reset lane kv-roundtrip run", false, 900, 100},
    {"service composition", "tests/system/kernel_reset_lane_service_composition", "kernel_reset_lane_service_composition_project", "kernel_reset_lane_service_composition_build", "app", "kernel_reset_lane_service_composition_build_output.txt", "kernel_reset_lane_service_composition_run_output.txt", "kernel reset lane service composition build", "kernel reset lane service composition run", false, 900, 100},
    {"boot", "tests/smoke/kernel_reset_lane_boot", "kernel_reset_lane_boot_project", "kernel_reset_lane_boot_build", "app", "kernel_reset_lane_boot_build_output.txt", "kernel_reset_lane_boot_run_output.txt", "kernel reset lane boot build", "kernel reset lane boot run", false, 900, 100},
    {"image", "tests/smoke/kernel_reset_lane_image", "kernel_reset_lane_image_project", "kernel_reset_lane_image_build", "app", "kernel_reset_lane_image_build_output.txt", "kernel_reset_lane_image_run_output.txt", "kernel reset lane image build", "kernel reset lane image run", false, 2000, 100},
    {"service identity", "tests/system/kernel_reset_lane_service_identity", "kernel_reset_lane_service_identity_project", "kernel_reset_lane_service_identity_build", "app", "kernel_reset_lane_service_identity_build_output.txt", "kernel_reset_lane_service_identity_run_output.txt", "kernel reset lane service identity build", "kernel reset lane service identity run", true, 900, 100},
    {"temporal backpressure", "tests/system/kernel_reset_lane_temporal_backpressure", "kernel_reset_lane_temporal_backpressure_project", "kernel_reset_lane_temporal_backpressure_build", "app", "kernel_reset_lane_temporal_backpressure_build_output.txt", "kernel_reset_lane_temporal_backpressure_run_output.txt", "kernel reset lane temporal backpressure build", "kernel reset lane temporal backpressure run", false, 900, 100},
    {"multi client", "tests/system/kernel_reset_lane_multi_client", "kernel_reset_lane_multi_client_project", "kernel_reset_lane_multi_client_build", "app", "kernel_reset_lane_multi_client_build_output.txt", "kernel_reset_lane_multi_client_run_output.txt", "kernel reset lane multi-client build", "kernel reset lane multi-client run", false, 900, 100},
    {"long lived coherence", "tests/system/kernel_reset_lane_long_lived_coherence", "kernel_reset_lane_long_lived_coherence_project", "kernel_reset_lane_long_lived_coherence_build", "app", "kernel_reset_lane_long_lived_coherence_build_output.txt", "kernel_reset_lane_long_lived_coherence_run_output.txt", "kernel reset lane long-lived coherence build", "kernel reset lane long-lived coherence run", false, 900, 100},
    {"cross service failure", "tests/system/kernel_reset_lane_cross_service_failure", "kernel_reset_lane_cross_service_failure_project", "kernel_reset_lane_cross_service_failure_build", "app", "kernel_reset_lane_cross_service_failure_build_output.txt", "kernel_reset_lane_cross_service_failure_run_output.txt", "kernel reset lane cross-service failure build", "kernel reset lane cross-service failure run", false, 900, 100},
    {"observability boundary", "tests/system/kernel_reset_lane_observability", "kernel_reset_lane_phase159_observability_project", "kernel_reset_lane_phase159_observability_build", "app", "kernel_reset_lane_phase159_observability_build_output.txt", "kernel_reset_lane_phase159_observability_run_output.txt", "kernel reset lane phase 159 observability build", "kernel reset lane phase 159 observability run", false, 900, 100},
    {"model boundary", "tests/system/kernel_reset_lane_model_boundary", "kernel_reset_lane_phase160_model_boundary_project", "kernel_reset_lane_phase160_model_boundary_build", "app", "kernel_reset_lane_phase160_model_boundary_build_output.txt", "kernel_reset_lane_phase160_model_boundary_run_output.txt", "kernel reset lane phase 160 model boundary build", "kernel reset lane phase 160 model boundary run", false, 900, 100},
    {"delivery witness", "tests/system/kernel_reset_lane_delivery_witness", "kernel_reset_lane_phase161_delivery_witness_project", "kernel_reset_lane_phase161_delivery_witness_build", "app", "kernel_reset_lane_phase161_delivery_witness_build_output.txt", "kernel_reset_lane_phase161_delivery_witness_run_output.txt", "kernel reset lane phase 161 delivery witness build", "kernel reset lane phase 161 delivery witness run", false, 900, 100},
    {"static topology", "tests/system/kernel_reset_lane_static_topology", "kernel_reset_lane_phase170_static_topology_project", "kernel_reset_lane_phase170_static_topology_build", "app", "kernel_reset_lane_phase170_static_topology_build_output.txt", "kernel_reset_lane_phase170_static_topology_run_output.txt", "kernel reset lane phase 170 static topology build", "kernel reset lane phase 170 static topology run", true, 900, 100},
    {"smp boundary", "tests/system/kernel_reset_lane_smp_boundary", "kernel_reset_lane_phase173_smp_boundary_project", "kernel_reset_lane_phase173_smp_boundary_build", "app", "kernel_reset_lane_phase173_smp_boundary_build_output.txt", "kernel_reset_lane_phase173_smp_boundary_run_output.txt", "kernel reset lane phase 173 smp boundary build", "kernel reset lane phase 173 smp boundary run", false, 900, 100},
    {"topology growth", "tests/system/kernel_reset_lane_phase176_growth", "kernel_reset_lane_phase176_growth_project", "kernel_reset_lane_phase176_growth_build", "app", "kernel_reset_lane_phase176_growth_build_output.txt", "kernel_reset_lane_phase176_growth_run_output.txt", "kernel reset lane phase 176 growth build", "kernel reset lane phase 176 growth run", false, 900, 100},
    {"hostile shell", "tests/system/kernel_reset_lane_phase177_hostile_shell", "kernel_reset_lane_phase177_hostile_shell_project", "kernel_reset_lane_phase177_hostile_shell_build", "app", "kernel_reset_lane_phase177_hostile_shell_build_output.txt", "kernel_reset_lane_phase177_hostile_shell_run_output.txt", "kernel reset lane phase 177 hostile shell build", "kernel reset lane phase 177 hostile shell run", false, 900, 100},
    {"topology restart", "tests/system/kernel_reset_lane_phase178_topology_restart", "kernel_reset_lane_phase178_topology_restart_project", "kernel_reset_lane_phase178_topology_restart_build", "app", "kernel_reset_lane_phase178_topology_restart_build_output.txt", "kernel_reset_lane_phase178_topology_restart_run_output.txt", "kernel reset lane phase 178 topology restart build", "kernel reset lane phase 178 topology restart run", true, 900, 100},
    {"retained reload", "tests/system/kernel_reset_lane_phase179_retained_reload", "kernel_reset_lane_phase179_retained_reload_project", "kernel_reset_lane_phase179_retained_reload_build", "app", "kernel_reset_lane_phase179_retained_reload_build_output.txt", "kernel_reset_lane_phase179_retained_reload_run_output.txt", "kernel reset lane phase 179 retained reload build", "kernel reset lane phase 179 retained reload run", false, 900, 100},
    {"kv retained reload", "tests/system/kernel_reset_lane_phase181_kv_retained_reload", "kernel_reset_lane_phase181_kv_retained_reload_project", "kernel_reset_lane_phase181_kv_retained_reload_build", "app", "kernel_reset_lane_phase181_kv_retained_reload_build_output.txt", "kernel_reset_lane_phase181_kv_retained_reload_run_output.txt", "kernel reset lane phase 181 kv retained reload build", "kernel reset lane phase 181 kv retained reload run", false, 900, 100},
    {"endpoint authority", "tests/system/kernel_reset_lane_phase183_endpoint_authority", "kernel_reset_lane_phase183_endpoint_authority_project", "kernel_reset_lane_phase183_endpoint_authority_build", "app", "kernel_reset_lane_phase183_endpoint_authority_build_output.txt", "kernel_reset_lane_phase183_endpoint_authority_run_output.txt", "kernel reset lane phase 183 endpoint authority build", "kernel reset lane phase 183 endpoint authority run", false, 900, 100},
    {"capability leakage", "tests/system/kernel_reset_lane_phase184_capability_leakage", "kernel_reset_lane_phase184_capability_leakage_project", "kernel_reset_lane_phase184_capability_leakage_build", "app", "kernel_reset_lane_phase184_capability_leakage_build_output.txt", "kernel_reset_lane_phase184_capability_leakage_run_output.txt", "kernel reset lane phase 184 capability leakage build", "kernel reset lane phase 184 capability leakage run", false, 900, 100},
    {"transferred handle", "tests/system/kernel_reset_lane_phase185_transferred_handle", "kernel_reset_lane_phase185_transferred_handle_project", "kernel_reset_lane_phase185_transferred_handle_build", "app", "kernel_reset_lane_phase185_transferred_handle_build_output.txt", "kernel_reset_lane_phase185_transferred_handle_run_output.txt", "kernel reset lane phase 185 transferred handle build", "kernel reset lane phase 185 transferred handle run", false, 900, 100},
    {"multi-handle transfer", "tests/system/kernel_reset_lane_phase198_multi_handle_transfer", "kernel_reset_lane_phase198_multi_handle_transfer_project", "kernel_reset_lane_phase198_multi_handle_transfer_build", "app", "kernel_reset_lane_phase198_multi_handle_transfer_build_output.txt", "kernel_reset_lane_phase198_multi_handle_transfer_run_output.txt", "kernel reset lane phase 198 multi-handle transfer build", "kernel reset lane phase 198 multi-handle transfer run", false, 1000, 100},
    {"retained queue reload", "tests/system/kernel_reset_lane_phase187_retained_queue_reload", "kernel_reset_lane_phase187_retained_queue_reload_project", "kernel_reset_lane_phase187_retained_queue_reload_build", "app", "kernel_reset_lane_phase187_retained_queue_reload_build_output.txt", "kernel_reset_lane_phase187_retained_queue_reload_run_output.txt", "kernel reset lane phase 187 retained queue reload build", "kernel reset lane phase 187 retained queue reload run", false, 900, 100},
    {"ticket restart", "tests/system/kernel_reset_lane_phase188_ticket_restart", "kernel_reset_lane_phase188_ticket_restart_project", "kernel_reset_lane_phase188_ticket_restart_build", "app", "kernel_reset_lane_phase188_ticket_restart_build_output.txt", "kernel_reset_lane_phase188_ticket_restart_run_output.txt", "kernel reset lane phase 188 ticket restart build", "kernel reset lane phase 188 ticket restart run", false, 900, 100},
    {"shell lifecycle", "tests/system/kernel_reset_lane_phase189_shell_lifecycle", "kernel_reset_lane_phase189_shell_lifecycle_project", "kernel_reset_lane_phase189_shell_lifecycle_build", "app", "kernel_reset_lane_phase189_shell_lifecycle_build_output.txt", "kernel_reset_lane_phase189_shell_lifecycle_run_output.txt", "kernel reset lane phase 189 shell lifecycle build", "kernel reset lane phase 189 shell lifecycle run", false, 900, 100},
    {"shell identity", "tests/system/kernel_reset_lane_phase193_shell_identity", "kernel_reset_lane_phase193_shell_identity_project", "kernel_reset_lane_phase193_shell_identity_build", "app", "kernel_reset_lane_phase193_shell_identity_build_output.txt", "kernel_reset_lane_phase193_shell_identity_run_output.txt", "kernel reset lane phase 193 shell identity build", "kernel reset lane phase 193 shell identity run", false, 900, 100},
    {"queue observability", "tests/system/kernel_reset_lane_phase196_queue_observability", "kernel_reset_lane_phase196_queue_observability_project", "kernel_reset_lane_phase196_queue_observability_build", "app", "kernel_reset_lane_phase196_queue_observability_build_output.txt", "kernel_reset_lane_phase196_queue_observability_run_output.txt", "kernel reset lane phase 196 queue observability build", "kernel reset lane phase 196 queue observability run", false, 1000, 100},
    {"retained coordination", "tests/system/kernel_reset_lane_phase197_retained_coordination", "kernel_reset_lane_phase197_retained_coordination_project", "kernel_reset_lane_phase197_retained_coordination_build", "app", "kernel_reset_lane_phase197_retained_coordination_build_output.txt", "kernel_reset_lane_phase197_retained_coordination_run_output.txt", "kernel reset lane phase 197 retained coordination build", "kernel reset lane phase 197 retained coordination run", true, 900, 100},
    {"workset identity", "tests/system/kernel_reset_lane_phase200_workset_identity", "kernel_reset_lane_phase200_workset_identity_project", "kernel_reset_lane_phase200_workset_identity_build", "app", "kernel_reset_lane_phase200_workset_identity_build_output.txt", "kernel_reset_lane_phase200_workset_identity_run_output.txt", "kernel reset lane phase 200 workset identity build", "kernel reset lane phase 200 workset identity run", false, 1000, 100},
    {"retained audit coordination", "tests/system/kernel_reset_lane_phase201_retained_audit_coordination", "kernel_reset_lane_phase201_retained_audit_coordination_project", "kernel_reset_lane_phase201_retained_audit_coordination_build", "app", "kernel_reset_lane_phase201_retained_audit_coordination_build_output.txt", "kernel_reset_lane_phase201_retained_audit_coordination_run_output.txt", "kernel reset lane phase 201 retained audit coordination build", "kernel reset lane phase 201 retained audit coordination run", false, 900, 100},
    {"retained summary", "tests/system/kernel_reset_lane_phase206_retained_summary", "kernel_reset_lane_phase206_retained_summary_project", "kernel_reset_lane_phase206_retained_summary_build", "app", "kernel_reset_lane_phase206_retained_summary_build_output.txt", "kernel_reset_lane_phase206_retained_summary_run_output.txt", "kernel reset lane phase 206 retained summary build", "kernel reset lane phase 206 retained summary run", false, 900, 100},
    {"retained restart identity", "tests/system/kernel_reset_lane_phase227_retained_restart_identity", "kernel_reset_lane_phase227_retained_restart_identity_project", "kernel_reset_lane_phase227_retained_restart_identity_build", "app", "kernel_reset_lane_phase227_retained_restart_identity_build_output.txt", "kernel_reset_lane_phase227_retained_restart_identity_run_output.txt", "kernel reset lane phase 227 retained restart identity build", "kernel reset lane phase 227 retained restart identity run", true, 900, 100},
    {"retained policy", "tests/system/kernel_reset_lane_phase208_retained_policy", "kernel_reset_lane_phase208_retained_policy_project", "kernel_reset_lane_phase208_retained_policy_build", "app", "kernel_reset_lane_phase208_retained_policy_build_output.txt", "kernel_reset_lane_phase208_retained_policy_run_output.txt", "kernel reset lane phase 208 retained policy build", "kernel reset lane phase 208 retained policy run", false, 1000, 100},
    {"authority inspection", "tests/system/kernel_reset_lane_phase212_authority_inspection", "kernel_reset_lane_phase212_authority_inspection_project", "kernel_reset_lane_phase212_authority_inspection_build", "app", "kernel_reset_lane_phase212_authority_inspection_build_output.txt", "kernel_reset_lane_phase212_authority_inspection_run_output.txt", "kernel reset lane phase 212 authority inspection build", "kernel reset lane phase 212 authority inspection run", false, 1000, 100},
    {"service state", "tests/system/kernel_reset_lane_phase214_service_state", "kernel_reset_lane_phase214_service_state_project", "kernel_reset_lane_phase214_service_state_build", "app", "kernel_reset_lane_phase214_service_state_build_output.txt", "kernel_reset_lane_phase214_service_state_run_output.txt", "kernel reset lane phase 214 service state build", "kernel reset lane phase 214 service state run", true, 1000, 100},
    {"queue pressure", "tests/system/kernel_reset_lane_phase215_queue_pressure", "kernel_reset_lane_phase215_queue_pressure_project", "kernel_reset_lane_phase215_queue_pressure_build", "app", "kernel_reset_lane_phase215_queue_pressure_build_output.txt", "kernel_reset_lane_phase215_queue_pressure_run_output.txt", "kernel reset lane phase 215 queue pressure build", "kernel reset lane phase 215 queue pressure run", true, 900, 100},
    {"stall consequence", "tests/system/kernel_reset_lane_phase216_stall_consequence", "kernel_reset_lane_phase216_stall_consequence_project", "kernel_reset_lane_phase216_stall_consequence_build", "app", "kernel_reset_lane_phase216_stall_consequence_build_output.txt", "kernel_reset_lane_phase216_stall_consequence_run_output.txt", "kernel reset lane phase 216 stall consequence build", "kernel reset lane phase 216 stall consequence run", true, 900, 100},
    {"retained durable boundary", "tests/system/kernel_reset_lane_phase217_retained_durable_boundary", "kernel_reset_lane_phase217_retained_durable_boundary_project", "kernel_reset_lane_phase217_retained_durable_boundary_build", "app", "kernel_reset_lane_phase217_retained_durable_boundary_build_output.txt", "kernel_reset_lane_phase217_retained_durable_boundary_run_output.txt", "kernel reset lane phase 217 retained durable boundary build", "kernel reset lane phase 217 retained durable boundary run", true, 1000, 100},
    {"timer task service", "tests/system/kernel_reset_lane_phase220_timer_task_service", "kernel_reset_lane_phase220_timer_task_service_project", "kernel_reset_lane_phase220_timer_task_service_build", "app", "kernel_reset_lane_phase220_timer_task_service_build_output.txt", "kernel_reset_lane_phase220_timer_task_service_run_output.txt", "kernel reset lane phase 220 timer task service build", "kernel reset lane phase 220 timer task service run", true, 1000, 100},
    {"task completion", "tests/system/kernel_reset_lane_phase225_task_completion", "kernel_reset_lane_phase225_task_completion_project", "kernel_reset_lane_phase225_task_completion_build", "app", "kernel_reset_lane_phase225_task_completion_build_output.txt", "kernel_reset_lane_phase225_task_completion_run_output.txt", "kernel reset lane phase 225 task completion build", "kernel reset lane phase 225 task completion run", true, 1000, 100},
    {"file growth", "tests/system/kernel_reset_lane_phase226_file_growth", "kernel_reset_lane_phase226_file_growth_project", "kernel_reset_lane_phase226_file_growth_build", "app", "kernel_reset_lane_phase226_file_growth_build_output.txt", "kernel_reset_lane_phase226_file_growth_run_output.txt", "kernel reset lane phase 226 file growth build", "kernel reset lane phase 226 file growth run", true, 1000, 100},
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

    struct InFlightScenario {
        std::size_t index = 0;
        std::future<ResetLaneScenarioTiming> future;
    };

    std::vector<InFlightScenario> in_flight;
    in_flight.reserve(jobs);

    for (std::size_t index = 0; index < selected.size(); ++index) {
        const ResetLaneScenario scenario = *selected[index];
        if (in_flight.size() >= jobs) {
            InFlightScenario current = std::move(in_flight.front());
            timings[current.index] = current.future.get();
            in_flight.erase(in_flight.begin());
        }
        in_flight.push_back(InFlightScenario{
            .index = index,
            .future = std::async(std::launch::async,
                                 [source_root, binary_root, mc_path, scenario]() {
                                     return RunKernelResetLaneScenario(source_root, binary_root, mc_path, scenario);
                                 }),
        });
    }

    for (auto& current : in_flight) {
        timings[current.index] = current.future.get();
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
                  << FormatDuration(timing.run_duration) << flags << '\n';
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
