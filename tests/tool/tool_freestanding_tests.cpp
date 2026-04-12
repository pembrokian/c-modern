#include <charconv>
#include <filesystem>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace {

bool ParseKernelShardSelector(std::string_view suite, int& shard) {
	constexpr std::string_view prefix = "kernel-";
	if (!suite.starts_with(prefix)) {
		return false;
	}

	const std::string_view shard_text = suite.substr(prefix.size());
	const char* begin = shard_text.data();
	const char* end = begin + shard_text.size();
	return std::from_chars(begin, end, shard).ec == std::errc{} && begin != end;
}

bool ParseKernelCaseSelector(std::string_view suite, std::string_view& case_name) {
	constexpr std::string_view prefix = "kernel-case:";
	if (!suite.starts_with(prefix)) {
		return false;
	}

	case_name = suite.substr(prefix.size());
	return !case_name.empty();
}

void RunSelectedFreestandingSuite(std::string_view suite,
								  const std::filesystem::path& source_root,
								  const std::filesystem::path& binary_root,
								  const std::filesystem::path& mc_path) {
	if (suite == "all") {
		mc::tool_tests::RunFreestandingToolSuite(source_root, binary_root, mc_path);
		return;
	}

	const std::filesystem::path suite_root = binary_root / "tool" / ("freestanding_" + std::string(suite));
	std::filesystem::remove_all(suite_root);

	if (suite == "bootstrap") {
		mc::tool_tests::RunFreestandingBootstrapToolSuite(source_root, suite_root, mc_path);
		return;
	}
	if (suite == "kernel") {
		mc::tool_tests::RunFreestandingKernelToolSuite(source_root, suite_root, mc_path);
		return;
	}
	if (suite == "kernel-docs") {
		mc::tool_tests::RunFreestandingKernelMetadataSuite(source_root, suite_root, mc_path);
		return;
	}
	int kernel_shard = 0;
	if (ParseKernelShardSelector(suite, kernel_shard)) {
		mc::tool_tests::RunFreestandingKernelToolSuiteShard(source_root, suite_root, mc_path, kernel_shard);
		return;
	}
	std::string_view kernel_case_name;
	if (ParseKernelCaseSelector(suite, kernel_case_name)) {
		mc::tool_tests::RunFreestandingKernelToolSuiteCase(source_root, suite_root, mc_path, kernel_case_name);
		return;
	}
	if (suite == "system") {
		mc::tool_tests::RunFreestandingSystemToolSuite(source_root, suite_root, mc_path);
		return;
	}

	mc::test_support::Fail("unknown freestanding suite selector: " + std::string(suite));
}

}  // namespace

int main(int argc, char** argv) {
	if (argc != 3 && argc != 4) {
		mc::test_support::Fail("expected source root, binary root, and optional freestanding suite selector");
	}

	const std::filesystem::path source_root = argv[1];
	const std::filesystem::path binary_root = argv[2];
	const std::filesystem::path mc_path = binary_root / "bin" / "mc";
	const std::string_view suite = argc == 4 ? std::string_view(argv[3]) : std::string_view("all");

	RunSelectedFreestandingSuite(suite, source_root, binary_root, mc_path);
	return 0;
}