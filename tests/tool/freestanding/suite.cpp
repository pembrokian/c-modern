#include <filesystem>

#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

void RunFreestandingToolSuite(const std::filesystem::path& source_root,
                              const std::filesystem::path& binary_root,
                              const std::filesystem::path& mc_path) {
    const std::filesystem::path suite_root = binary_root / "tool" / "freestanding";

    RunFreestandingBootstrapToolSuite(source_root, suite_root, mc_path);
    RunFreestandingKernelToolSuite(source_root, suite_root, mc_path);
    RunFreestandingSystemToolSuite(source_root, suite_root, mc_path);
}

}  // namespace mc::tool_tests
