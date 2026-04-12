#include <array>
#include <filesystem>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc {
namespace tool_tests {

using KernelTestFn = void(const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);

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

namespace {

using mc::test_support::Fail;

const std::array<KernelTestCase, 38> kKernelTestCases = {{
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
    {"phase128_service_death_observation", 5, &RunFreestandingKernelPhase128ServiceDeathObservation},
    {"phase129_partial_failure_propagation", 5, &RunFreestandingKernelPhase129PartialFailurePropagation},
    {"phase130_explicit_restart_or_replacement", 5, &RunFreestandingKernelPhase130ExplicitRestartOrReplacement},
    {"phase131_fan_in_or_fan_out_composition", 5, &RunFreestandingKernelPhase131FanInOrFanOutComposition},
    {"phase132_backpressure_and_blocking", 5, &RunFreestandingKernelPhase132BackpressureAndBlocking},
    {"phase133_message_lifetime_and_reuse", 5, &RunFreestandingKernelPhase133MessageLifetimeAndReuse},
    {"phase134_minimal_device_service_handoff", 5, &RunFreestandingKernelPhase134MinimalDeviceServiceHandoff},
    {"phase135_buffer_ownership_boundary_audit", 5, &RunFreestandingKernelPhase135BufferOwnershipBoundaryAudit},
    {"phase136_device_failure_containment_probe", 5, &RunFreestandingKernelPhase136DeviceFailureContainmentProbe},
    {"phase137_optional_dma_or_equivalent_follow_through", 5, &RunFreestandingKernelPhase137OptionalDmaOrEquivalentFollowThrough},
    {"phase140_serial_ingress_composed_service_graph", 5, &RunFreestandingKernelPhase140SerialIngressComposedServiceGraph},
    {"phase141_interactive_service_system_scope_freeze", 5, &RunFreestandingKernelPhase141InteractiveServiceSystemScopeFreeze},
}};

void RunFreestandingKernelRegistry(const std::filesystem::path& source_root,
                                   const std::filesystem::path& binary_root,
                                   const std::filesystem::path& mc_path,
                                   int shard) {
    for (const auto& test_case : kKernelTestCases) {
        if (shard != 0 && test_case.shard != shard) {
            continue;
        }
        test_case.fn(source_root, binary_root, mc_path);
    }
}

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
    RunFreestandingKernelRegistry(source_root, binary_root, mc_path, shard);
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

void RunFreestandingKernelToolSuiteShard1(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path) {
    RunFreestandingKernelToolSuiteShard(source_root, binary_root, mc_path, 1);
}

void RunFreestandingKernelToolSuiteShard2(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path) {
    RunFreestandingKernelToolSuiteShard(source_root, binary_root, mc_path, 2);
}

void RunFreestandingKernelToolSuiteShard3(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path) {
    RunFreestandingKernelToolSuiteShard(source_root, binary_root, mc_path, 3);
}

void RunFreestandingKernelToolSuiteShard4(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path) {
    RunFreestandingKernelToolSuiteShard(source_root, binary_root, mc_path, 4);
}

void RunFreestandingKernelToolSuiteShard5(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path) {
    RunFreestandingKernelToolSuiteShard(source_root, binary_root, mc_path, 5);
}

void RunFreestandingKernelToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    RunFreestandingKernelToolSuiteShard(source_root, binary_root, mc_path, 0);
}

}  // namespace tool_tests
}  // namespace mc
