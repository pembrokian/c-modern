#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc {
namespace tool_tests {

using KernelTestFn = void(const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);

struct KernelTestCase {
    std::string_view name;
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

namespace {

using mc::test_support::Fail;

const std::array<KernelTestCase, 4> kKernelTestCases = {{
    {"phase85_endpoint_queue", &RunFreestandingKernelPhase85EndpointQueueSmoke},
    {"phase86_task_lifecycle", &RunFreestandingKernelPhase86TaskLifecycleProof},
    {"phase87_static_data", &RunFreestandingKernelPhase87StaticDataProof},
    {"phase88_build_integration", &RunFreestandingKernelPhase88BuildIntegrationAudit},
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

void RunFreestandingKernelSyntheticSuiteCase(const std::filesystem::path& source_root,
                                             const std::filesystem::path& binary_root,
                                             const std::filesystem::path& mc_path,
                                             std::string_view case_name) {
    const KernelTestCase* test_case = FindKernelTestCase(case_name);
    if (test_case != nullptr) {
        test_case->fn(source_root, binary_root, mc_path);
        return;
    }
    Fail("unknown freestanding kernel synthetic selector: " + std::string(case_name));
}

void RunFreestandingKernelSyntheticSuite(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path) {
    for (const auto& test_case : kKernelTestCases) {
        test_case.fn(source_root, binary_root, mc_path);
    }
}

void RunFreestandingKernelToolSuite(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    const auto phase_checks = LoadAllFreestandingKernelRuntimePhaseDescriptors(source_root);
    for (const auto& phase_check : phase_checks) {
        RunFreestandingKernelRuntimePhaseCase(source_root,
                                              binary_root,
                                              mc_path,
                                              phase_check.label,
                                              "kernel-runtime");
    }
}

void RunFreestandingKernelMetadataSuite(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path) {
    (void)binary_root;
    (void)mc_path;

    for (const auto& descriptor : LoadFreestandingKernelTextAuditDescriptors(source_root, "docs", "audit.toml")) {
        const std::string text = mc::test_support::ReadFile(source_root / descriptor.source_file);
        for (const auto& needle : descriptor.expected_contains) {
            mc::test_support::ExpectOutputContains(text, needle, descriptor.context + " [" + descriptor.label + "]");
        }
    }
}

}  // namespace tool_tests
}  // namespace mc
