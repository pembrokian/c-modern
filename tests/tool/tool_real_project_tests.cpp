#include <filesystem>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

int main(int argc, char** argv) {
	if (argc != 3) {
		mc::test_support::Fail("expected source root and binary root arguments");
	}

	const std::filesystem::path source_root = argv[1];
	const std::filesystem::path binary_root = argv[2];
	const std::filesystem::path mc_path = binary_root / "mc";

	mc::tool_tests::RunPhase7RealProjectSuite(source_root, binary_root, mc_path);
	return 0;
}