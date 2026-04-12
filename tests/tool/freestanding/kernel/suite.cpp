#include <array>
#include <filesystem>
#include <string>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc {
namespace tool_tests {

using KernelTestFn = void(const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);
using KernelShardFn = KernelTestFn;

struct KernelTestCase {
    std::string_view name;
    int shard;
    KernelTestFn* fn;
};

void RunFreestandingKernelPhase85EndpointQueueSmoke(const std::filesystem::path& source_root,
                                                    const std::filesystem::path& binary_root,
                                                    const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase86TaskLifecycleProof(const std::filesystem::path& source_root,
                                                    const std::filesystem::path& binary_root,
                                                    const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase87StaticDataProof(const std::filesystem::path& source_root,
                                                 const std::filesystem::path& binary_root,
                                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase88BuildIntegrationAudit(const std::filesystem::path& source_root,
                                                       const std::filesystem::path& binary_root,
                                                       const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase105RealLogServiceHandshake(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase106RealEchoServiceRequestReply(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase107RealUserToUserCapabilityTransfer(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase108KernelImageProgramCapAudit(const std::filesystem::path& source_root,
                                                             const std::filesystem::path& binary_root,
                                                             const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase109FirstRunningKernelSliceAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase110KernelOwnershipSplitAudit(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase111SchedulerLifecycleOwnershipClarification(const std::filesystem::path& source_root,
                                                                          const std::filesystem::path& binary_root,
                                                                          const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase112SyscallBoundaryThinnessAudit(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase113InterruptEntryAndGenericDispatchBoundary(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase114AddressSpaceAndMmuOwnershipSplit(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase115TimerOwnershipHardening(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase116MmuActivationBarrierFollowThrough(const std::filesystem::path& source_root,
                                                                    const std::filesystem::path& binary_root,
                                                                    const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase117InitOrchestratedMultiServiceBringUp(const std::filesystem::path& source_root,
                                                                      const std::filesystem::path& binary_root,
                                                                      const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase118RequestReplyAndDelegationFollowThrough(const std::filesystem::path& source_root,
                                                                         const std::filesystem::path& binary_root,
                                                                         const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase119NamespacePressureAudit(const std::filesystem::path& source_root,
                                                         const std::filesystem::path& binary_root,
                                                         const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase120RunningSystemSupportStatement(const std::filesystem::path& source_root,
                                                                const std::filesystem::path& binary_root,
                                                                const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase121KernelImageContractHardening(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase122TargetSurfaceAudit(const std::filesystem::path& source_root,
                                                     const std::filesystem::path& binary_root,
                                                     const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase123NextPlateauAudit(const std::filesystem::path& source_root,
                                                   const std::filesystem::path& binary_root,
                                                   const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase124DelegationChainStress(const std::filesystem::path& source_root,
                                                        const std::filesystem::path& binary_root,
                                                        const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase125InvalidationAndRejectionAudit(const std::filesystem::path& source_root,
                                                                const std::filesystem::path& binary_root,
                                                                const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase126AuthorityLifetimeClassification(const std::filesystem::path& source_root,
                                                                 const std::filesystem::path& binary_root,
                                                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase128ServiceDeathObservation(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase129PartialFailurePropagation(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase130ExplicitRestartOrReplacement(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase131FanInOrFanOutComposition(const std::filesystem::path& source_root,
                                                           const std::filesystem::path& binary_root,
                                                           const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase132BackpressureAndBlocking(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase133MessageLifetimeAndReuse(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase134MinimalDeviceServiceHandoff(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase135BufferOwnershipBoundaryAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase136DeviceFailureContainmentProbe(const std::filesystem::path& source_root,
                                                                const std::filesystem::path& binary_root,
                                                                const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase137OptionalDmaOrEquivalentFollowThrough(const std::filesystem::path& source_root,
                                                                       const std::filesystem::path& binary_root,
                                                                       const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase140SerialIngressComposedServiceGraph(const std::filesystem::path& source_root,
                                                                    const std::filesystem::path& binary_root,
                                                                    const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase141InteractiveServiceSystemScopeFreeze(const std::filesystem::path& source_root,
                                                                      const std::filesystem::path& binary_root,
                                                                      const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase142SerialShellCommandRouting(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase143LongLivedLogServiceFollowThrough(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase144StatefulKeyValueServiceFollowThrough(const std::filesystem::path& source_root,
                                                                       const std::filesystem::path& binary_root,
                                                                       const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase145ServiceRestartFailureAndUsagePressureAudit(const std::filesystem::path& source_root,
                                                                             const std::filesystem::path& binary_root,
                                                                             const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase146ServiceShapeConsolidation(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path);
void RunFreestandingKernelPhase147IpcShapeAuditUnderRealUsage(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path);
void RunFreestandingKernelShard1(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelShard2(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelShard3(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelShard4(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelShard5(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelShard6(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelShard7(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelShard8(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);
void RunFreestandingKernelShard9(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path);

namespace {

using mc::test_support::Fail;

constexpr int kFirstKernelShard = 1;
constexpr int kLastKernelShard = 9;

const std::array<KernelShardFn*, kLastKernelShard> kKernelShardFns = {{
    &RunFreestandingKernelShard1,
    &RunFreestandingKernelShard2,
    &RunFreestandingKernelShard3,
    &RunFreestandingKernelShard4,
    &RunFreestandingKernelShard5,
    &RunFreestandingKernelShard6,
    &RunFreestandingKernelShard7,
    &RunFreestandingKernelShard8,
    &RunFreestandingKernelShard9,
}};

void RunFreestandingKernelShardDispatch(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path,
                                        int shard) {
    if (shard < kFirstKernelShard || shard > kLastKernelShard) {
        Fail("unknown freestanding kernel shard selector: " + std::to_string(shard));
    }
    kKernelShardFns[static_cast<std::size_t>(shard - kFirstKernelShard)](source_root, binary_root, mc_path);
}

const std::array<KernelTestCase, 44> kKernelTestCases = {{
    {"phase85_endpoint_queue", 1, &RunFreestandingKernelPhase85EndpointQueueSmoke},
    {"phase86_task_lifecycle", 1, &RunFreestandingKernelPhase86TaskLifecycleProof},
    {"phase87_static_data", 1, &RunFreestandingKernelPhase87StaticDataProof},
    {"phase88_build_integration", 1, &RunFreestandingKernelPhase88BuildIntegrationAudit},
    {"phase105_real_log_service_handshake", 1, &RunFreestandingKernelPhase105RealLogServiceHandshake},
    {"phase106_real_echo_service_request_reply", 1, &RunFreestandingKernelPhase106RealEchoServiceRequestReply},
    {"phase107_real_user_to_user_capability_transfer", 2, &RunFreestandingKernelPhase107RealUserToUserCapabilityTransfer},
    {"phase108_kernel_image_program_cap_audit", 2, &RunFreestandingKernelPhase108KernelImageProgramCapAudit},
    {"phase109_first_running_kernel_slice_audit", 2, &RunFreestandingKernelPhase109FirstRunningKernelSliceAudit},
    {"phase110_kernel_ownership_split_audit", 2, &RunFreestandingKernelPhase110KernelOwnershipSplitAudit},
    {"phase111_scheduler_lifecycle_ownership_clarification", 2, &RunFreestandingKernelPhase111SchedulerLifecycleOwnershipClarification},
    {"phase112_syscall_boundary_thinness_audit", 3, &RunFreestandingKernelPhase112SyscallBoundaryThinnessAudit},
    {"phase113_interrupt_entry_and_generic_dispatch_boundary", 3, &RunFreestandingKernelPhase113InterruptEntryAndGenericDispatchBoundary},
    {"phase114_address_space_and_mmu_ownership_split", 3, &RunFreestandingKernelPhase114AddressSpaceAndMmuOwnershipSplit},
    {"phase115_timer_ownership_hardening", 3, &RunFreestandingKernelPhase115TimerOwnershipHardening},
    {"phase116_mmu_activation_barrier_follow_through", 3, &RunFreestandingKernelPhase116MmuActivationBarrierFollowThrough},
    {"phase117_init_orchestrated_multi_service_bring_up", 4, &RunFreestandingKernelPhase117InitOrchestratedMultiServiceBringUp},
    {"phase118_request_reply_and_delegation_follow_through", 4, &RunFreestandingKernelPhase118RequestReplyAndDelegationFollowThrough},
    {"phase119_namespace_pressure_audit", 4, &RunFreestandingKernelPhase119NamespacePressureAudit},
    {"phase120_running_system_support_statement", 4, &RunFreestandingKernelPhase120RunningSystemSupportStatement},
    {"phase121_kernel_image_contract_hardening", 4, &RunFreestandingKernelPhase121KernelImageContractHardening},
    {"phase122_target_surface_audit", 5, &RunFreestandingKernelPhase122TargetSurfaceAudit},
    {"phase123_next_plateau_audit", 5, &RunFreestandingKernelPhase123NextPlateauAudit},
    {"phase124_delegation_chain_stress", 5, &RunFreestandingKernelPhase124DelegationChainStress},
    {"phase125_invalidation_and_rejection_audit", 5, &RunFreestandingKernelPhase125InvalidationAndRejectionAudit},
    {"phase126_authority_lifetime_classification", 5, &RunFreestandingKernelPhase126AuthorityLifetimeClassification},
    {"phase128_service_death_observation", 6, &RunFreestandingKernelPhase128ServiceDeathObservation},
    {"phase129_partial_failure_propagation", 6, &RunFreestandingKernelPhase129PartialFailurePropagation},
    {"phase130_explicit_restart_or_replacement", 6, &RunFreestandingKernelPhase130ExplicitRestartOrReplacement},
    {"phase131_fan_in_or_fan_out_composition", 6, &RunFreestandingKernelPhase131FanInOrFanOutComposition},
    {"phase132_backpressure_and_blocking", 6, &RunFreestandingKernelPhase132BackpressureAndBlocking},
    {"phase133_message_lifetime_and_reuse", 7, &RunFreestandingKernelPhase133MessageLifetimeAndReuse},
    {"phase134_minimal_device_service_handoff", 7, &RunFreestandingKernelPhase134MinimalDeviceServiceHandoff},
    {"phase135_buffer_ownership_boundary_audit", 7, &RunFreestandingKernelPhase135BufferOwnershipBoundaryAudit},
    {"phase136_device_failure_containment_probe", 7, &RunFreestandingKernelPhase136DeviceFailureContainmentProbe},
    {"phase137_optional_dma_or_equivalent_follow_through", 7, &RunFreestandingKernelPhase137OptionalDmaOrEquivalentFollowThrough},
    {"phase140_serial_ingress_composed_service_graph", 8, &RunFreestandingKernelPhase140SerialIngressComposedServiceGraph},
    {"phase141_interactive_service_system_scope_freeze", 8, &RunFreestandingKernelPhase141InteractiveServiceSystemScopeFreeze},
    {"phase142_serial_shell_command_routing", 8, &RunFreestandingKernelPhase142SerialShellCommandRouting},
    {"phase143_long_lived_log_service_follow_through", 9, &RunFreestandingKernelPhase143LongLivedLogServiceFollowThrough},
    {"phase144_stateful_key_value_service_follow_through", 9, &RunFreestandingKernelPhase144StatefulKeyValueServiceFollowThrough},
    {"phase145_service_restart_failure_and_usage_pressure_audit", 9, &RunFreestandingKernelPhase145ServiceRestartFailureAndUsagePressureAudit},
    {"phase146_service_shape_consolidation", 9, &RunFreestandingKernelPhase146ServiceShapeConsolidation},
    {"phase147_ipc_shape_audit_under_real_usage", 9, &RunFreestandingKernelPhase147IpcShapeAuditUnderRealUsage},
}};

const KernelTestCase* FindKernelTestCase(std::string_view case_name) {
    for (const auto& test_case : kKernelTestCases) {
        if (test_case.name == case_name) {
            return &test_case;
        }
    }
    return nullptr;
}

}  // namespace

void RunFreestandingKernelToolSuiteShard(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path,
                                         int shard) {
    if (shard == 0) {
        for (int selected_shard = kFirstKernelShard; selected_shard <= kLastKernelShard; ++selected_shard) {
            RunFreestandingKernelShardDispatch(source_root, binary_root, mc_path, selected_shard);
        }
        return;
    }
    RunFreestandingKernelShardDispatch(source_root, binary_root, mc_path, shard);
}

void RunFreestandingKernelToolSuiteCase(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path,
                                        std::string_view case_name) {
    const KernelTestCase* test_case = FindKernelTestCase(case_name);
    if (test_case == nullptr) {
        Fail("unknown freestanding kernel case selector: " + std::string(case_name));
    }
    test_case->fn(source_root, binary_root, mc_path);
}

void RunFreestandingKernelToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    RunFreestandingKernelToolSuiteShard(source_root, binary_root, mc_path, 0);
    RunFreestandingKernelArtifactSuite(source_root, binary_root, mc_path);
}

void RunFreestandingKernelMetadataSuite(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path) {
    (void)binary_root;
    (void)mc_path;

    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase109_first_running_canopus_kernel_slice_audit.txt");

    const std::string phase_doc = mc::test_support::ReadFile(phase_doc_path);
    mc::test_support::ExpectOutputContains(phase_doc,
                                           "Phase 109 -- First Running Canopus Kernel Slice Audit",
                                           "phase109 plan note should exist in canonical phase-doc style");
    mc::test_support::ExpectOutputContains(phase_doc,
                                           "explicit boot entry, first user entry, endpoint-and-handle object",
                                           "phase109 plan note should publish the admitted running-kernel slice explicitly");
    mc::test_support::ExpectOutputContains(phase_doc,
                                           "Phase 109 is complete as a first running Canopus kernel slice audit.",
                                           "phase109 plan note should close out the running-kernel support statement explicitly");

    const std::string repo_map = mc::test_support::ReadFile(common_paths.repo_map_path);
    mc::test_support::ExpectOutputContains(repo_map,
                                           "tests/tool/freestanding/kernel/shard2.cpp",
                                           "repository map should route the phase109 follow-through through shard2 ownership");
    mc::test_support::ExpectOutputContains(repo_map,
                                           "phases 107-111",
                                           "repository map should describe shard-based kernel ownership instead of one phase file per proof");
}

}  // namespace tool_tests
}  // namespace mc
