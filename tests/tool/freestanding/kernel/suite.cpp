#include <filesystem>

#include "tests/tool/tool_suite_common.h"

namespace mc {
namespace tool_tests {

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

void RunFreestandingKernelToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    RunFreestandingKernelPhase85EndpointQueueSmoke(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase86TaskLifecycleProof(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase87StaticDataProof(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase88BuildIntegrationAudit(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase105RealLogServiceHandshake(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase106RealEchoServiceRequestReply(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase107RealUserToUserCapabilityTransfer(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase108KernelImageProgramCapAudit(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase109FirstRunningKernelSliceAudit(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase110KernelOwnershipSplitAudit(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase111SchedulerLifecycleOwnershipClarification(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase112SyscallBoundaryThinnessAudit(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase113InterruptEntryAndGenericDispatchBoundary(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase114AddressSpaceAndMmuOwnershipSplit(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase115TimerOwnershipHardening(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase116MmuActivationBarrierFollowThrough(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase117InitOrchestratedMultiServiceBringUp(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase118RequestReplyAndDelegationFollowThrough(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase119NamespacePressureAudit(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase120RunningSystemSupportStatement(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase121KernelImageContractHardening(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase122TargetSurfaceAudit(source_root, binary_root, mc_path);
}

}  // namespace tool_tests
}  // namespace mc
