#include <filesystem>

#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

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
void RunFreestandingKernelPhase96BootEntryProof(const std::filesystem::path& source_root,
                                                const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path);

void RunFreestandingKernelToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    RunFreestandingKernelPhase85EndpointQueueSmoke(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase86TaskLifecycleProof(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase87StaticDataProof(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase88BuildIntegrationAudit(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase96BootEntryProof(source_root, binary_root, mc_path);
}

}  // namespace mc::tool_tests
