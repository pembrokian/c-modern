#include "tests/support/fixture_utils.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace mc::test_support {

namespace {

std::filesystem::path ReplaceMcSuffix(const std::filesystem::path& source_path,
                                      std::string_view suffix) {
    auto without_extension = source_path;
    without_extension.replace_extension();
    return std::filesystem::path(without_extension.generic_string() + std::string(suffix));
}

}  // namespace

[[noreturn]] void Fail(const std::string& message) {
    std::cerr << "test failure: " << message << '\n';
    std::exit(1);
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Fail("unable to read fixture file: " + path.generic_string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string NormalizeFixtureText(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

void RequireExpectedDiagnosticSubstrings(std::string_view rendered,
                                         std::string_view expected_text,
                                         const std::filesystem::path& source_path) {
    std::istringstream expected_lines{std::string(expected_text)};
    std::string expected_line;
    while (std::getline(expected_lines, expected_line)) {
        if (expected_line.empty()) {
            continue;
        }
        if (rendered.find(expected_line) == std::string::npos) {
            std::cerr << "missing expected diagnostic substring for " << source_path.generic_string() << "\n";
            std::cerr << "expected substring: " << expected_line << "\n";
            std::cerr << "actual diagnostics:\n" << rendered << "\n";
            std::exit(1);
        }
    }
}

std::vector<DiscoveredFixture> DiscoverAdjacentFixtures(
    const std::filesystem::path& fixture_dir,
    std::string_view success_suffix,
    std::optional<std::string_view> failure_suffix,
    FixtureDiscoveryMode mode) {
    std::vector<DiscoveredFixture> fixtures;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(fixture_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".mc") {
            continue;
        }

        const auto source_path = entry.path();
        const auto success_path = ReplaceMcSuffix(source_path, success_suffix);
        const bool has_success = std::filesystem::exists(success_path);
        bool has_failure = false;
        std::filesystem::path failure_path;
        if (failure_suffix.has_value()) {
            failure_path = ReplaceMcSuffix(source_path, *failure_suffix);
            has_failure = std::filesystem::exists(failure_path);
        }

        if (has_success && has_failure) {
            Fail("fixture has both success and failure expectations: " +
                 std::filesystem::relative(source_path, fixture_dir).generic_string());
        }
        if (!has_success && !has_failure) {
            if (mode == FixtureDiscoveryMode::kRequireExpectation) {
                Fail("fixture is missing an adjacent expectation file: " +
                     std::filesystem::relative(source_path, fixture_dir).generic_string());
            }
            continue;
        }

        const auto expectation_path = has_success ? success_path : failure_path;
        fixtures.push_back({
            .source_path = source_path,
            .expectation_path = expectation_path,
            .source_name = std::filesystem::relative(source_path, fixture_dir).generic_string(),
            .expectation_name = std::filesystem::relative(expectation_path, fixture_dir).generic_string(),
            .expects_success = has_success,
        });
    }

    std::sort(fixtures.begin(), fixtures.end(), [](const DiscoveredFixture& lhs, const DiscoveredFixture& rhs) {
        return lhs.source_name < rhs.source_name;
    });
    return fixtures;
}

}  // namespace mc::test_support