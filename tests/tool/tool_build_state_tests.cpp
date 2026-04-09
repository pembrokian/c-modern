#include <filesystem>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

int main(int argc, char** argv) {
	if (argc != 3) {
		mc::test_support::Fail("expected source root and binary root arguments");
	}

	const std::filesystem::path binary_root = argv[2];
	const std::filesystem::path mc_path = binary_root / "bin" / "mc";

	mc::tool_tests::RunPhase7BuildStateSuite(binary_root, mc_path);
	return 0;
}