#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"
#include "tests/tool/tool_workflow_suite_internal.h"

namespace {

using mc::test_support::CopyDirectoryTree;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;
using namespace mc::tool_tests;

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
};

std::filesystem::path InstallKernelResetLaneFixtureProject(const std::filesystem::path& source_root,
                                                           const std::filesystem::path& fixture_root,
                                                           const std::filesystem::path& project_root) {
    constexpr std::string_view kKernelNewSourcePlaceholder = "__KERNEL_NEW_SRC__";

    std::filesystem::remove_all(project_root);
    CopyDirectoryTree(fixture_root, project_root);

    const std::filesystem::path build_toml_path = project_root / "build.toml";
    std::string build_toml_text = ReadFile(build_toml_path);
    if (build_toml_text.find(kKernelNewSourcePlaceholder) == std::string::npos) {
        Fail("kernel reset-lane fixture build.toml is missing the kernel_new source placeholder");
    }

    const std::string kernel_src = (source_root / "kernel" / "src").generic_string();
    size_t pos = 0;
    while ((pos = build_toml_text.find(kKernelNewSourcePlaceholder, pos)) != std::string::npos) {
        build_toml_text.replace(pos, kKernelNewSourcePlaceholder.size(), kernel_src);
        pos += kernel_src.size();
    }
    WriteFile(build_toml_path, build_toml_text);
    return build_toml_path;
}

void ExpectKernelResetLaneRunSuccess(const ResetLaneScenario& scenario,
                                     const mc::test_support::CommandOutcome& outcome,
                                     std::string_view output) {
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("kernel reset lane " + std::string(scenario.label) + " project should return 0, got:\n" +
             std::string(output));
    }
}

void RunKernelResetLaneScenario(const std::filesystem::path& source_root,
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
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       scenario.target_name,
                                       scenario.build_output_name,
                                       std::string(scenario.build_context));

    std::vector<std::string> command = {mc_path.generic_string(),
                                        "run",
                                        "--project",
                                        project_path.generic_string()};
    if (scenario.target_name != "app") {
        command.push_back("--target");
        command.push_back(std::string(scenario.target_name));
    }
    command.push_back("--build-dir");
    command.push_back(build_dir.generic_string());

    const auto [run_outcome, run_output] = RunCommandCapture(command,
                                                             build_dir / std::string(scenario.run_output_name),
                                                             std::string(scenario.run_context));
    ExpectKernelResetLaneRunSuccess(scenario, run_outcome, run_output);
}

constexpr std::array<ResetLaneScenario, 35> kResetLaneScenarios = {{
    {"repo project", "", "", "kernel_reset_lane_repo_build", "kernel", "kernel_reset_lane_repo_build_output.txt", "kernel_reset_lane_repo_run_output.txt", "kernel reset lane repo project build", "kernel reset lane repo project run"},
    {"smoke", "tests/smoke/kernel_reset_lane_serial_round_trip", "kernel_reset_lane_smoke_project", "kernel_reset_lane_smoke_build", "app", "kernel_reset_lane_smoke_build_output.txt", "kernel_reset_lane_smoke_run_output.txt", "kernel reset lane smoke build", "kernel reset lane smoke run"},
    {"retained state", "tests/system/kernel_reset_lane_retained_log", "kernel_reset_lane_retained_state_project", "kernel_reset_lane_retained_state_build", "app", "kernel_reset_lane_retained_state_build_output.txt", "kernel_reset_lane_retained_state_run_output.txt", "kernel reset lane retained-state build", "kernel reset lane retained-state run"},
    {"observability", "tests/system/kernel_reset_lane_serial_observability", "kernel_reset_lane_observability_project", "kernel_reset_lane_observability_build", "app", "kernel_reset_lane_observability_build_output.txt", "kernel_reset_lane_observability_run_output.txt", "kernel reset lane observability build", "kernel reset lane observability run"},
    {"kv roundtrip", "tests/system/kernel_reset_lane_kv_roundtrip", "kernel_reset_lane_kv_roundtrip_project", "kernel_reset_lane_kv_roundtrip_build", "app", "kernel_reset_lane_kv_roundtrip_build_output.txt", "kernel_reset_lane_kv_roundtrip_run_output.txt", "kernel reset lane kv-roundtrip build", "kernel reset lane kv-roundtrip run"},
    {"service composition", "tests/system/kernel_reset_lane_service_composition", "kernel_reset_lane_service_composition_project", "kernel_reset_lane_service_composition_build", "app", "kernel_reset_lane_service_composition_build_output.txt", "kernel_reset_lane_service_composition_run_output.txt", "kernel reset lane service composition build", "kernel reset lane service composition run"},
    {"boot", "tests/smoke/kernel_reset_lane_boot", "kernel_reset_lane_boot_project", "kernel_reset_lane_boot_build", "app", "kernel_reset_lane_boot_build_output.txt", "kernel_reset_lane_boot_run_output.txt", "kernel reset lane boot build", "kernel reset lane boot run"},
    {"image", "tests/smoke/kernel_reset_lane_image", "kernel_reset_lane_image_project", "kernel_reset_lane_image_build", "app", "kernel_reset_lane_image_build_output.txt", "kernel_reset_lane_image_run_output.txt", "kernel reset lane image build", "kernel reset lane image run"},
    {"service identity", "tests/system/kernel_reset_lane_service_identity", "kernel_reset_lane_service_identity_project", "kernel_reset_lane_service_identity_build", "app", "kernel_reset_lane_service_identity_build_output.txt", "kernel_reset_lane_service_identity_run_output.txt", "kernel reset lane service identity build", "kernel reset lane service identity run"},
    {"temporal backpressure", "tests/system/kernel_reset_lane_temporal_backpressure", "kernel_reset_lane_temporal_backpressure_project", "kernel_reset_lane_temporal_backpressure_build", "app", "kernel_reset_lane_temporal_backpressure_build_output.txt", "kernel_reset_lane_temporal_backpressure_run_output.txt", "kernel reset lane temporal backpressure build", "kernel reset lane temporal backpressure run"},
    {"multi client", "tests/system/kernel_reset_lane_multi_client", "kernel_reset_lane_multi_client_project", "kernel_reset_lane_multi_client_build", "app", "kernel_reset_lane_multi_client_build_output.txt", "kernel_reset_lane_multi_client_run_output.txt", "kernel reset lane multi-client build", "kernel reset lane multi-client run"},
    {"long lived coherence", "tests/system/kernel_reset_lane_long_lived_coherence", "kernel_reset_lane_long_lived_coherence_project", "kernel_reset_lane_long_lived_coherence_build", "app", "kernel_reset_lane_long_lived_coherence_build_output.txt", "kernel_reset_lane_long_lived_coherence_run_output.txt", "kernel reset lane long-lived coherence build", "kernel reset lane long-lived coherence run"},
    {"cross service failure", "tests/system/kernel_reset_lane_cross_service_failure", "kernel_reset_lane_cross_service_failure_project", "kernel_reset_lane_cross_service_failure_build", "app", "kernel_reset_lane_cross_service_failure_build_output.txt", "kernel_reset_lane_cross_service_failure_run_output.txt", "kernel reset lane cross-service failure build", "kernel reset lane cross-service failure run"},
    {"phase 159 observability", "tests/system/kernel_reset_lane_observability", "kernel_reset_lane_phase159_observability_project", "kernel_reset_lane_phase159_observability_build", "app", "kernel_reset_lane_phase159_observability_build_output.txt", "kernel_reset_lane_phase159_observability_run_output.txt", "kernel reset lane phase 159 observability build", "kernel reset lane phase 159 observability run"},
    {"phase 160 model boundary", "tests/system/kernel_reset_lane_model_boundary", "kernel_reset_lane_phase160_model_boundary_project", "kernel_reset_lane_phase160_model_boundary_build", "app", "kernel_reset_lane_phase160_model_boundary_build_output.txt", "kernel_reset_lane_phase160_model_boundary_run_output.txt", "kernel reset lane phase 160 model boundary build", "kernel reset lane phase 160 model boundary run"},
    {"phase 161 delivery witness", "tests/system/kernel_reset_lane_delivery_witness", "kernel_reset_lane_phase161_delivery_witness_project", "kernel_reset_lane_phase161_delivery_witness_build", "app", "kernel_reset_lane_phase161_delivery_witness_build_output.txt", "kernel_reset_lane_phase161_delivery_witness_run_output.txt", "kernel reset lane phase 161 delivery witness build", "kernel reset lane phase 161 delivery witness run"},
    {"phase 170 static topology", "tests/system/kernel_reset_lane_static_topology", "kernel_reset_lane_phase170_static_topology_project", "kernel_reset_lane_phase170_static_topology_build", "app", "kernel_reset_lane_phase170_static_topology_build_output.txt", "kernel_reset_lane_phase170_static_topology_run_output.txt", "kernel reset lane phase 170 static topology build", "kernel reset lane phase 170 static topology run"},
    {"phase 173 smp boundary", "tests/system/kernel_reset_lane_smp_boundary", "kernel_reset_lane_phase173_smp_boundary_project", "kernel_reset_lane_phase173_smp_boundary_build", "app", "kernel_reset_lane_phase173_smp_boundary_build_output.txt", "kernel_reset_lane_phase173_smp_boundary_run_output.txt", "kernel reset lane phase 173 smp boundary build", "kernel reset lane phase 173 smp boundary run"},
    {"phase 176 growth", "tests/system/kernel_reset_lane_phase176_growth", "kernel_reset_lane_phase176_growth_project", "kernel_reset_lane_phase176_growth_build", "app", "kernel_reset_lane_phase176_growth_build_output.txt", "kernel_reset_lane_phase176_growth_run_output.txt", "kernel reset lane phase 176 growth build", "kernel reset lane phase 176 growth run"},
    {"phase 177 hostile shell", "tests/system/kernel_reset_lane_phase177_hostile_shell", "kernel_reset_lane_phase177_hostile_shell_project", "kernel_reset_lane_phase177_hostile_shell_build", "app", "kernel_reset_lane_phase177_hostile_shell_build_output.txt", "kernel_reset_lane_phase177_hostile_shell_run_output.txt", "kernel reset lane phase 177 hostile shell build", "kernel reset lane phase 177 hostile shell run"},
    {"phase 178 topology restart", "tests/system/kernel_reset_lane_phase178_topology_restart", "kernel_reset_lane_phase178_topology_restart_project", "kernel_reset_lane_phase178_topology_restart_build", "app", "kernel_reset_lane_phase178_topology_restart_build_output.txt", "kernel_reset_lane_phase178_topology_restart_run_output.txt", "kernel reset lane phase 178 topology restart build", "kernel reset lane phase 178 topology restart run"},
    {"phase 179 retained reload", "tests/system/kernel_reset_lane_phase179_retained_reload", "kernel_reset_lane_phase179_retained_reload_project", "kernel_reset_lane_phase179_retained_reload_build", "app", "kernel_reset_lane_phase179_retained_reload_build_output.txt", "kernel_reset_lane_phase179_retained_reload_run_output.txt", "kernel reset lane phase 179 retained reload build", "kernel reset lane phase 179 retained reload run"},
    {"phase 181 kv retained reload", "tests/system/kernel_reset_lane_phase181_kv_retained_reload", "kernel_reset_lane_phase181_kv_retained_reload_project", "kernel_reset_lane_phase181_kv_retained_reload_build", "app", "kernel_reset_lane_phase181_kv_retained_reload_build_output.txt", "kernel_reset_lane_phase181_kv_retained_reload_run_output.txt", "kernel reset lane phase 181 kv retained reload build", "kernel reset lane phase 181 kv retained reload run"},
    {"phase 183 endpoint authority", "tests/system/kernel_reset_lane_phase183_endpoint_authority", "kernel_reset_lane_phase183_endpoint_authority_project", "kernel_reset_lane_phase183_endpoint_authority_build", "app", "kernel_reset_lane_phase183_endpoint_authority_build_output.txt", "kernel_reset_lane_phase183_endpoint_authority_run_output.txt", "kernel reset lane phase 183 endpoint authority build", "kernel reset lane phase 183 endpoint authority run"},
    {"phase 184 capability leakage", "tests/system/kernel_reset_lane_phase184_capability_leakage", "kernel_reset_lane_phase184_capability_leakage_project", "kernel_reset_lane_phase184_capability_leakage_build", "app", "kernel_reset_lane_phase184_capability_leakage_build_output.txt", "kernel_reset_lane_phase184_capability_leakage_run_output.txt", "kernel reset lane phase 184 capability leakage build", "kernel reset lane phase 184 capability leakage run"},
    {"phase 185 transferred handle", "tests/system/kernel_reset_lane_phase185_transferred_handle", "kernel_reset_lane_phase185_transferred_handle_project", "kernel_reset_lane_phase185_transferred_handle_build", "app", "kernel_reset_lane_phase185_transferred_handle_build_output.txt", "kernel_reset_lane_phase185_transferred_handle_run_output.txt", "kernel reset lane phase 185 transferred handle build", "kernel reset lane phase 185 transferred handle run"},
    {"phase 198 multi-handle transfer", "tests/system/kernel_reset_lane_phase198_multi_handle_transfer", "kernel_reset_lane_phase198_multi_handle_transfer_project", "kernel_reset_lane_phase198_multi_handle_transfer_build", "app", "kernel_reset_lane_phase198_multi_handle_transfer_build_output.txt", "kernel_reset_lane_phase198_multi_handle_transfer_run_output.txt", "kernel reset lane phase 198 multi-handle transfer build", "kernel reset lane phase 198 multi-handle transfer run"},
    {"phase 187 retained queue reload", "tests/system/kernel_reset_lane_phase187_retained_queue_reload", "kernel_reset_lane_phase187_retained_queue_reload_project", "kernel_reset_lane_phase187_retained_queue_reload_build", "app", "kernel_reset_lane_phase187_retained_queue_reload_build_output.txt", "kernel_reset_lane_phase187_retained_queue_reload_run_output.txt", "kernel reset lane phase 187 retained queue reload build", "kernel reset lane phase 187 retained queue reload run"},
    {"phase 188 ticket restart", "tests/system/kernel_reset_lane_phase188_ticket_restart", "kernel_reset_lane_phase188_ticket_restart_project", "kernel_reset_lane_phase188_ticket_restart_build", "app", "kernel_reset_lane_phase188_ticket_restart_build_output.txt", "kernel_reset_lane_phase188_ticket_restart_run_output.txt", "kernel reset lane phase 188 ticket restart build", "kernel reset lane phase 188 ticket restart run"},
    {"phase 189 shell lifecycle", "tests/system/kernel_reset_lane_phase189_shell_lifecycle", "kernel_reset_lane_phase189_shell_lifecycle_project", "kernel_reset_lane_phase189_shell_lifecycle_build", "app", "kernel_reset_lane_phase189_shell_lifecycle_build_output.txt", "kernel_reset_lane_phase189_shell_lifecycle_run_output.txt", "kernel reset lane phase 189 shell lifecycle build", "kernel reset lane phase 189 shell lifecycle run"},
    {"phase 193 shell identity", "tests/system/kernel_reset_lane_phase193_shell_identity", "kernel_reset_lane_phase193_shell_identity_project", "kernel_reset_lane_phase193_shell_identity_build", "app", "kernel_reset_lane_phase193_shell_identity_build_output.txt", "kernel_reset_lane_phase193_shell_identity_run_output.txt", "kernel reset lane phase 193 shell identity build", "kernel reset lane phase 193 shell identity run"},
    {"phase 196 queue observability", "tests/system/kernel_reset_lane_phase196_queue_observability", "kernel_reset_lane_phase196_queue_observability_project", "kernel_reset_lane_phase196_queue_observability_build", "app", "kernel_reset_lane_phase196_queue_observability_build_output.txt", "kernel_reset_lane_phase196_queue_observability_run_output.txt", "kernel reset lane phase 196 queue observability build", "kernel reset lane phase 196 queue observability run"},
    {"phase 197 retained coordination", "tests/system/kernel_reset_lane_phase197_retained_coordination", "kernel_reset_lane_phase197_retained_coordination_project", "kernel_reset_lane_phase197_retained_coordination_build", "app", "kernel_reset_lane_phase197_retained_coordination_build_output.txt", "kernel_reset_lane_phase197_retained_coordination_run_output.txt", "kernel reset lane phase 197 retained coordination build", "kernel reset lane phase 197 retained coordination run"},
    {"phase 200 workset identity", "tests/system/kernel_reset_lane_phase200_workset_identity", "kernel_reset_lane_phase200_workset_identity_project", "kernel_reset_lane_phase200_workset_identity_build", "app", "kernel_reset_lane_phase200_workset_identity_build_output.txt", "kernel_reset_lane_phase200_workset_identity_run_output.txt", "kernel reset lane phase 200 workset identity build", "kernel reset lane phase 200 workset identity run"},
    {"phase 201 retained audit coordination", "tests/system/kernel_reset_lane_phase201_retained_audit_coordination", "kernel_reset_lane_phase201_retained_audit_coordination_project", "kernel_reset_lane_phase201_retained_audit_coordination_build", "app", "kernel_reset_lane_phase201_retained_audit_coordination_build_output.txt", "kernel_reset_lane_phase201_retained_audit_coordination_run_output.txt", "kernel reset lane phase 201 retained audit coordination build", "kernel reset lane phase 201 retained audit coordination run"},
}};

}  // namespace

namespace mc::tool_tests {

void RunWorkflowKernelResetLaneSuite(const std::filesystem::path& source_root,
                                     const std::filesystem::path& binary_root,
                                     const std::filesystem::path& mc_path) {
    for (const auto& scenario : kResetLaneScenarios) {
        RunKernelResetLaneScenario(source_root, binary_root, mc_path, scenario);
    }
}

}  // namespace mc::tool_tests
