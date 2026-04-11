#include <filesystem>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

int main(int argc, char** argv) {
	if (argc != 3 && argc != 4) {
		mc::test_support::Fail("expected source root, binary root, and optional real-project case selector");
	}

	const std::filesystem::path source_root = argv[1];
	const std::filesystem::path binary_root = argv[2];
	const std::filesystem::path mc_path = binary_root / "bin" / "mc";
	const std::string_view case_name = argc == 4 ? std::string_view(argv[3]) : std::string_view();

	if (case_name.empty()) {
		mc::tool_tests::RunRealProjectToolSuite(source_root, binary_root, mc_path);
	} else {
		mc::tool_tests::RunRealProjectToolSuiteCase(source_root, binary_root, mc_path, case_name);
	}
	return 0;
}