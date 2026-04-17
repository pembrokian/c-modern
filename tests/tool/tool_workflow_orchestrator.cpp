#include <filesystem>
#include <string>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"
#include "tests/tool/tool_workflow_suite_internal.h"

namespace mc::tool_tests {

namespace {

void RunWorkflowHelpCase(const std::filesystem::path&,
                         const std::filesystem::path& binary_root,
                         const std::filesystem::path& mc_path) {
    RunWorkflowHelpSuite(binary_root, mc_path);
}

void RunWorkflowTestCommandCase(const std::filesystem::path&,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    RunWorkflowTestCommandSuite(binary_root, mc_path);
}

void RunWorkflowProjectValidationCase(const std::filesystem::path& source_root,
                                      const std::filesystem::path& binary_root,
                                      const std::filesystem::path& mc_path) {
    RunWorkflowProjectValidationSuite(source_root, binary_root, mc_path);
}

void RunWorkflowMultifileModuleCase(const std::filesystem::path&,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    RunWorkflowMultifileModuleSuite(binary_root, mc_path);
}

void RunWorkflowKernelResetLaneCase(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    RunWorkflowKernelResetLaneSuite(source_root, binary_root, mc_path);
}

void RunWorkflowKernelResetLaneFullCase(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path) {
    RunWorkflowKernelResetLaneFullSuite(source_root, binary_root, mc_path);
}

struct WorkflowTestCase {
    std::string_view name;
    void (*fn)(const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);
};

constexpr WorkflowTestCase kWorkflowTestCases[] = {
#include "tool_workflow_cases.inc"
};

const WorkflowTestCase* FindWorkflowTestCase(std::string_view case_name) {
    for (const auto& test_case : kWorkflowTestCases) {
        if (test_case.name == case_name) {
            return &test_case;
        }
    }
    return nullptr;
}

}  // namespace

void RunWorkflowToolSuite(const std::filesystem::path& source_root,
                          const std::filesystem::path& binary_root,
                          const std::filesystem::path& mc_path) {
    const std::filesystem::path suite_root = binary_root / "tool" / "workflow";

    for (const auto& test_case : kWorkflowTestCases) {
        test_case.fn(source_root, suite_root, mc_path);
    }
}

void RunWorkflowToolSuiteCase(const std::filesystem::path& source_root,
                              const std::filesystem::path& binary_root,
                              const std::filesystem::path& mc_path,
                              std::string_view case_name) {
    const WorkflowTestCase* test_case = FindWorkflowTestCase(case_name);
    if (test_case == nullptr) {
        mc::test_support::Fail("unknown workflow suite selector: " + std::string(case_name));
    }

    const std::filesystem::path suite_root = binary_root / "tool" / "workflow";
    test_case->fn(source_root, suite_root, mc_path);
}

}  // namespace mc::tool_tests
