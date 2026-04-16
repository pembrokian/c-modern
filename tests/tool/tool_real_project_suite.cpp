#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::Fail;
}  // namespace

namespace mc::tool_tests {

namespace {

struct RealProjectTestCase {
    std::string_view name;
    void (*fn)(const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);
};

constexpr RealProjectTestCase kRealProjectTestCases[] = {
#include "tool_real_project_cases.inc"
};

const RealProjectTestCase* FindRealProjectTestCase(std::string_view case_name) {
    for (const auto& test_case : kRealProjectTestCases) {
        if (test_case.name == case_name) {
            return &test_case;
        }
    }
    return nullptr;
}

}  // namespace

void RunRealProjectToolSuite(const std::filesystem::path& source_root,
                             const std::filesystem::path& binary_root,
                             const std::filesystem::path& mc_path) {
    const std::filesystem::path suite_root = binary_root / "tool" / "real_projects";

    for (const auto& test_case : kRealProjectTestCases) {
        test_case.fn(source_root, suite_root, mc_path);
    }
}

void RunRealProjectToolSuiteCase(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path,
                                 std::string_view case_name) {
    const RealProjectTestCase* test_case = FindRealProjectTestCase(case_name);
    if (test_case == nullptr) {
        Fail("unknown real-project case selector: " + std::string(case_name));
    }
    const std::filesystem::path suite_root = binary_root / "tool" / "real_projects";
    test_case->fn(source_root, suite_root, mc_path);
}

}  // namespace mc::tool_tests
