#include <filesystem>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace {

bool ParseKernelRuntimeSelector(std::string_view suite, std::string_view& selector_label) {
	constexpr std::string_view runtime_prefix = "kernel-runtime:";
	if (suite.starts_with(runtime_prefix)) {
		selector_label = suite.substr(runtime_prefix.size());
		return !selector_label.empty();
	}
	return false;
}

bool ParseKernelSyntheticSelector(std::string_view suite, std::string_view& selector_label) {
	constexpr std::string_view prefix = "kernel-synthetic:";
	if (!suite.starts_with(prefix)) {
		return false;
	}

	selector_label = suite.substr(prefix.size());
	return !selector_label.empty();
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
	if (suite == "kernel-runtime") {
		mc::tool_tests::RunFreestandingKernelToolSuite(source_root, suite_root, mc_path);
		return;
	}
	if (suite == "kernel-synthetic") {
		mc::tool_tests::RunFreestandingKernelSyntheticSuite(source_root, suite_root, mc_path);
		return;
	}
	if (suite == "kernel-docs") {
		mc::tool_tests::RunFreestandingKernelMetadataSuite(source_root, suite_root, mc_path);
		return;
	}
	if (suite == "kernel-artifacts") {
		mc::tool_tests::RunFreestandingKernelArtifactSuite(source_root, suite_root, mc_path);
		return;
	}
	std::string_view selector_label;
	if (ParseKernelRuntimeSelector(suite, selector_label)) {
		mc::tool_tests::RunFreestandingKernelRuntimePhaseCase(source_root, suite_root, mc_path, selector_label, "kernel-runtime");
		return;
	}
	if (ParseKernelSyntheticSelector(suite, selector_label)) {
		mc::tool_tests::RunFreestandingKernelSyntheticSuiteCase(source_root, suite_root, mc_path, selector_label);
		return;
	}
	if (suite == "system") {
		mc::tool_tests::RunFreestandingSystemToolSuite(source_root, suite_root, mc_path);
		return;
	}

	mc::test_support::Fail("unknown freestanding suite selector: " + std::string(suite) +
					  " (supported kernel selectors: kernel-runtime, kernel-runtime:<label>, kernel-synthetic, kernel-synthetic:<label>, kernel-docs, kernel-artifacts)");
}

}  // namespace

int main(int argc, char** argv) {
	if (argc != 3 && argc != 4) {
		mc::test_support::Fail(
			"expected source root, binary root, and optional freestanding suite selector "
			"(for example: kernel-runtime, kernel-runtime:<label>, kernel-synthetic, kernel-synthetic:<label>, kernel-docs, kernel-artifacts)");
	}

	const std::filesystem::path source_root = argv[1];
	const std::filesystem::path binary_root = argv[2];
	const std::filesystem::path mc_path = binary_root / "bin" / "mc";
	const std::string_view suite = argc == 4 ? std::string_view(argv[3]) : std::string_view("all");

	RunSelectedFreestandingSuite(suite, source_root, binary_root, mc_path);
	return 0;
}