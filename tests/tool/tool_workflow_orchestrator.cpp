#include <filesystem>

#include "tests/tool/tool_suite_common.h"
#include "tests/tool/tool_workflow_suite_internal.h"

namespace mc::tool_tests {

void RunWorkflowToolSuite(const std::filesystem::path& source_root,
                          const std::filesystem::path& binary_root,
                          const std::filesystem::path& mc_path) {
    const std::filesystem::path suite_root = binary_root / "tool" / "workflow";

    RunWorkflowHelpSuite(suite_root, mc_path);
    RunWorkflowTestCommandSuite(suite_root, mc_path);
    RunWorkflowProjectValidationSuite(source_root, suite_root, mc_path);
    RunWorkflowMultifileModuleSuite(suite_root, mc_path);
    RunWorkflowKernelResetLaneSuite(source_root, suite_root, mc_path);
}

}  // namespace mc::tool_tests
