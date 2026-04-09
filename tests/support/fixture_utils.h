#ifndef MODERN_C_TESTS_SUPPORT_FIXTURE_UTILS_H
#define MODERN_C_TESTS_SUPPORT_FIXTURE_UTILS_H

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mc::test_support {

enum class FixtureDiscoveryMode {
    kRequireExpectation,
    kExistingExpectationOnly,
};

struct DiscoveredFixture {
    std::filesystem::path source_path;
    std::filesystem::path expectation_path;
    std::string source_name;
    std::string expectation_name;
    bool expects_success = true;
};

[[noreturn]] void Fail(const std::string& message);

std::string ReadTextFile(const std::filesystem::path& path);

std::string NormalizeFixtureText(std::string text);

void RequireExpectedDiagnosticSubstrings(std::string_view rendered,
                                         std::string_view expected_text,
                                         const std::filesystem::path& source_path);

std::vector<DiscoveredFixture> DiscoverAdjacentFixtures(
    const std::filesystem::path& fixture_dir,
    std::string_view success_suffix,
    std::optional<std::string_view> failure_suffix = std::nullopt,
    FixtureDiscoveryMode mode = FixtureDiscoveryMode::kExistingExpectationOnly);

}  // namespace mc::test_support

#endif