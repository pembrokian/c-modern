#include "compiler/driver/internal.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "compiler/ast/ast.h"
#include "compiler/codegen_llvm/backend.h"

namespace mc::driver {
namespace {

std::string JoinNames(const std::vector<std::string>& names) {
    std::string text;
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index > 0) {
            text += ", ";
        }
        text += names[index];
    }
    return text;
}

std::vector<std::string> CollectEnabledTestTargetNames(const ProjectFile& project) {
    std::vector<std::string> names;
    for (const auto& [name, target] : project.targets) {
        if (target.tests.enabled) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

struct OrdinaryTestCase {
    std::filesystem::path source_path;
    std::string module_name;
    std::string function_name;
};

enum class CompilerRegressionKind {
    kCheckPass,
    kCheckFail,
    kRunOutput,
    kMir,
};

struct CompilerRegressionCase {
    CompilerRegressionKind kind = CompilerRegressionKind::kCheckPass;
    std::filesystem::path source_path;
    std::filesystem::path expectation_path;
    int expected_exit_code = 0;
};

struct DiscoveredTargetTests {
    std::vector<std::filesystem::path> roots;
    std::vector<OrdinaryTestCase> ordinary_tests;
    std::vector<CompilerRegressionCase> regression_cases;
};

std::string TrimWhitespace(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return std::string(text.substr(begin, end - begin));
}

std::string StripComment(std::string_view text) {
    const std::size_t comment = text.find('#');
    if (comment == std::string_view::npos) {
        return std::string(text);
    }
    return std::string(text.substr(0, comment));
}

bool HasSuffix(std::string_view text,
               std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

bool IsBootstrapTestReturnType(const ast::TypeExpr& type_expr) {
    if (type_expr.kind == ast::TypeExpr::Kind::kPointer) {
        return true;
    }
    return type_expr.kind == ast::TypeExpr::Kind::kNamed &&
           (type_expr.name == "Error" || type_expr.name == "testing.Error");
}

std::vector<std::filesystem::path> ComputeTargetTestRoots(const ProjectFile& project,
                                                          const ProjectTarget& target) {
    if (!target.tests.roots.empty()) {
        return target.tests.roots;
    }
    return {project.root_dir / "tests"};
}

void SortPaths(std::vector<std::filesystem::path>& paths) {
    std::sort(paths.begin(), paths.end(), [](const std::filesystem::path& left, const std::filesystem::path& right) {
        return left.generic_string() < right.generic_string();
    });
}

std::vector<std::filesystem::path> DiscoverFilesRecursive(const std::vector<std::filesystem::path>& roots,
                                                          std::string_view suffix) {
    std::vector<std::filesystem::path> files;
    for (const auto& root : roots) {
        if (!std::filesystem::exists(root)) {
            continue;
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string path = entry.path().generic_string();
            if (HasSuffix(path, suffix)) {
                files.push_back(entry.path());
            }
        }
    }
    SortPaths(files);
    return files;
}

bool DiscoverOrdinaryTestsInFile(const std::filesystem::path& source_path,
                                 support::DiagnosticSink& diagnostics,
                                 std::vector<OrdinaryTestCase>& ordinary_tests) {
    auto parse_result = ParseSourcePath(source_path, diagnostics);
    if (!parse_result.has_value()) {
        return false;
    }

    const std::string module_name = source_path.stem().string();
    for (const auto& decl : parse_result->source_file->decls) {
        if (decl.kind != ast::Decl::Kind::kFunc || !decl.name.starts_with("test_")) {
            continue;
        }
        if (!decl.type_params.empty() || !decl.params.empty() || decl.return_types.size() != 1 ||
            decl.return_types.front() == nullptr || !IsBootstrapTestReturnType(*decl.return_types.front())) {
            diagnostics.Report({
                .file_path = source_path,
                .span = decl.span,
                .severity = support::DiagnosticSeverity::kError,
                .message = "test function '" + decl.name + "' must use the bootstrap test signature func() *T or func() Error",
            });
            return false;
        }
        ordinary_tests.push_back({
            .source_path = source_path,
            .module_name = module_name,
            .function_name = decl.name,
        });
    }
    return true;
}

std::optional<CompilerRegressionKind> ParseRegressionKind(std::string_view token) {
    if (token == "check-pass") {
        return CompilerRegressionKind::kCheckPass;
    }
    if (token == "check-fail") {
        return CompilerRegressionKind::kCheckFail;
    }
    if (token == "run-output") {
        return CompilerRegressionKind::kRunOutput;
    }
    if (token == "mir") {
        return CompilerRegressionKind::kMir;
    }
    return std::nullopt;
}

std::string AcceptedCompilerRegressionKinds() {
    return "check-pass, check-fail, run-output, mir";
}

bool ParseCompilerManifest(const std::filesystem::path& manifest_path,
                          support::DiagnosticSink& diagnostics,
                          std::vector<CompilerRegressionCase>& regression_cases) {
    std::ifstream input(manifest_path, std::ios::binary);
    if (!input) {
        diagnostics.Report({
            .file_path = manifest_path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to read compiler regression manifest",
        });
        return false;
    }

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const std::string trimmed = TrimWhitespace(StripComment(line));
        if (trimmed.empty()) {
            continue;
        }

        std::istringstream tokens(trimmed);
        std::string kind_text;
        std::string source_text;
        if (!(tokens >> kind_text >> source_text)) {
            diagnostics.Report({
                .file_path = manifest_path,
                .span = {{line_number, 1}, {line_number, 1}},
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid compiler regression manifest entry",
            });
            return false;
        }

        const auto kind = ParseRegressionKind(kind_text);
        if (!kind.has_value()) {
            diagnostics.Report({
                .file_path = manifest_path,
                .span = {{line_number, 1}, {line_number, 1}},
                .severity = support::DiagnosticSeverity::kError,
                .message = "unknown compiler regression kind: " + kind_text,
            });
            diagnostics.Report({
                .file_path = manifest_path,
                .span = {{line_number, 1}, {line_number, 1}},
                .severity = support::DiagnosticSeverity::kNote,
                .message = "accepted kinds: " + AcceptedCompilerRegressionKinds(),
            });
            return false;
        }

        CompilerRegressionCase regression_case {
            .kind = *kind,
            .source_path = std::filesystem::absolute(manifest_path.parent_path() / source_text).lexically_normal(),
        };

        switch (*kind) {
            case CompilerRegressionKind::kCheckPass:
                break;

            case CompilerRegressionKind::kCheckFail:
            case CompilerRegressionKind::kMir: {
                std::string expectation_text;
                if (!(tokens >> expectation_text)) {
                    diagnostics.Report({
                        .file_path = manifest_path,
                        .span = {{line_number, 1}, {line_number, 1}},
                        .severity = support::DiagnosticSeverity::kError,
                        .message = "compiler regression entry is missing an expectation path",
                    });
                    return false;
                }
                regression_case.expectation_path = std::filesystem::absolute(manifest_path.parent_path() / expectation_text).lexically_normal();
                break;
            }

            case CompilerRegressionKind::kRunOutput: {
                std::string expectation_text;
                if (!(tokens >> regression_case.expected_exit_code >> expectation_text)) {
                    diagnostics.Report({
                        .file_path = manifest_path,
                        .span = {{line_number, 1}, {line_number, 1}},
                        .severity = support::DiagnosticSeverity::kError,
                        .message = "run-output regression entry must include exit code and stdout expectation path",
                    });
                    return false;
                }
                regression_case.expectation_path = std::filesystem::absolute(manifest_path.parent_path() / expectation_text).lexically_normal();
                break;
            }
        }

        regression_cases.push_back(std::move(regression_case));
    }

    return true;
}

std::optional<DiscoveredTargetTests> DiscoverTargetTests(const ProjectFile& project,
                                                        const ProjectTarget& target,
                                                        support::DiagnosticSink& diagnostics) {
    DiscoveredTargetTests discovered;
    discovered.roots = ComputeTargetTestRoots(project, target);
    SortPaths(discovered.roots);

    std::unordered_set<std::string> seen_modules;
    for (const auto& test_file : DiscoverFilesRecursive(discovered.roots, "_test.mc")) {
        const std::string module_name = test_file.stem().string();
        if (!seen_modules.insert(module_name).second) {
            diagnostics.Report({
                .file_path = test_file,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "duplicate ordinary test module name discovered: " + module_name +
                           "; bootstrap ordinary test discovery requires globally unique file stems across all configured test roots",
            });
            return std::nullopt;
        }
        if (!DiscoverOrdinaryTestsInFile(test_file, diagnostics, discovered.ordinary_tests)) {
            return std::nullopt;
        }
    }

    const auto manifests = DiscoverFilesRecursive(discovered.roots, "compiler_manifest.txt");
    for (const auto& manifest_path : manifests) {
        if (!ParseCompilerManifest(manifest_path, diagnostics, discovered.regression_cases)) {
            return std::nullopt;
        }
    }

    std::sort(discovered.ordinary_tests.begin(), discovered.ordinary_tests.end(), [](const OrdinaryTestCase& left, const OrdinaryTestCase& right) {
        if (left.source_path != right.source_path) {
            return left.source_path.generic_string() < right.source_path.generic_string();
        }
        return left.function_name < right.function_name;
    });
    std::sort(discovered.regression_cases.begin(), discovered.regression_cases.end(), [](const CompilerRegressionCase& left, const CompilerRegressionCase& right) {
        if (left.kind != right.kind) {
            return static_cast<int>(left.kind) < static_cast<int>(right.kind);
        }
        return left.source_path.generic_string() < right.source_path.generic_string();
    });
    return discovered;
}

std::vector<std::string> ReadExpectedLines(const std::filesystem::path& path,
                                           support::DiagnosticSink& diagnostics) {
    const auto text = ReadSourceText(path, diagnostics);
    if (!text.has_value()) {
        return {};
    }

    std::vector<std::string> lines;
    std::istringstream input(*text);
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = TrimWhitespace(line);
        if (!trimmed.empty()) {
            lines.push_back(trimmed);
        }
    }
    return lines;
}

std::optional<std::vector<std::filesystem::path>> BuildTestImportRoots(const ProjectFile& project,
                                                                       const ProjectTarget& target,
                                                                       const std::vector<std::filesystem::path>& cli_import_roots,
                                                                       const std::vector<std::filesystem::path>& test_roots,
                                                                       const std::filesystem::path& generated_root,
                                                                       support::DiagnosticSink& diagnostics) {
    std::vector<std::filesystem::path> roots;
    std::unordered_set<std::string> seen;
    const auto append = [&](const std::filesystem::path& root) {
        const std::filesystem::path normalized = std::filesystem::absolute(root).lexically_normal();
        if (seen.insert(normalized.generic_string()).second) {
            roots.push_back(normalized);
        }
    };

    append(generated_root);
    for (const auto& root : test_roots) {
        append(root);
    }
    for (const auto& root : ComputeProjectImportRoots(project, target, cli_import_roots)) {
        append(root);
    }
    const auto repo_root = DiscoverRepositoryRoot(generated_root);
    if (!repo_root.has_value()) {
        diagnostics.Report({
            .file_path = project.path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to discover repository root for generated ordinary test runner imports from " +
                       generated_root.generic_string() + "; repository stdlib root is required",
        });
        return std::nullopt;
    }
    const std::filesystem::path stdlib_root = *repo_root / "stdlib";
    if (!std::filesystem::exists(stdlib_root)) {
        diagnostics.Report({
            .file_path = project.path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "generated ordinary test runner requires repository stdlib import root, but it was not found at " +
                       stdlib_root.generic_string(),
        });
        return std::nullopt;
    }
    append(stdlib_root);
    return roots;
}

std::string BuildOrdinaryRunnerSource(const std::vector<OrdinaryTestCase>& ordinary_tests) {
    std::ostringstream source;
    const std::string total_tests = std::to_string(ordinary_tests.size());
    source << "import io\n";
    source << "import mem\n";
    for (const auto& ordinary_test : ordinary_tests) {
        source << "import " << ordinary_test.module_name << "\n";
    }
    source << "\n";
    source << "@private\n";
    source << "const ASCII_ZERO: u8 = 48\n";
    source << "@private\n";
    source << "const ASCII_MINUS: u8 = 45\n";
    source << "@private\n";
    source << "const I32_MIN_VALUE: i32 = -2147483648\n";
    source << "@private\n";
    source << "const I32_MIN_ABS: u64 = 2147483648\n";
    source << "\n";
    source << "func decimal_len_u64(value: u64) usize {\n";
    source << "    digits: usize = 1\n";
    source << "    current: u64 = value\n";
    source << "    while current >= 10 {\n";
    source << "        current = current / 10\n";
    source << "        digits = digits + 1\n";
    source << "    }\n";
    source << "    return digits\n";
    source << "}\n";
    source << "\n";
    source << "func write_decimal_u64(dst: Slice<u8>, value: u64) usize {\n";
    source << "    digits: usize = decimal_len_u64(value)\n";
    source << "    index: usize = digits\n";
    source << "    current: u64 = value\n";
    source << "    while index > 0 {\n";
    source << "        index = index - 1\n";
    source << "        dst[index] = (u8)((u64)(ASCII_ZERO) + (current % 10))\n";
    source << "        current = current / 10\n";
    source << "    }\n";
    source << "    return digits\n";
    source << "}\n";
    source << "\n";
    source << "func sprint_i32(value: i32) *Buffer<u8> {\n";
    source << "    alloc: *mem.Allocator = mem.default_allocator()\n";
    source << "    negative: bool = value < 0\n";
    source << "    magnitude: u64\n";
    source << "    if negative {\n";
    source << "        if value == I32_MIN_VALUE {\n";
    source << "            magnitude = I32_MIN_ABS\n";
    source << "        } else {\n";
    source << "            magnitude = (u64)(0 - value)\n";
    source << "        }\n";
    source << "    } else {\n";
    source << "        magnitude = (u64)(value)\n";
    source << "    }\n";
    source << "\n";
    source << "    digits: usize = decimal_len_u64(magnitude)\n";
    source << "    cap: usize = digits\n";
    source << "    if negative {\n";
    source << "        cap = cap + 1\n";
    source << "    }\n";
    source << "\n";
    source << "    buf: *Buffer<u8> = mem.buffer_new<u8>(alloc, cap)\n";
    source << "    if buf == nil {\n";
    source << "        return nil\n";
    source << "    }\n";
    source << "\n";
    source << "    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)\n";
    source << "    if negative {\n";
    source << "        bytes[0] = ASCII_MINUS\n";
    source << "        write_decimal_u64(bytes[1:cap], magnitude)\n";
    source << "    } else {\n";
    source << "        write_decimal_u64(bytes, magnitude)\n";
    source << "    }\n";
    source << "    return buf\n";
    source << "}\n";
    source << "\n";
    source << "func write_summary(total: i32, failures: i32) i32 {\n";
    source << "    passed: i32 = total - failures\n";
    source << "    total_buf: *Buffer<u8> = sprint_i32(total)\n";
    source << "    passed_buf: *Buffer<u8> = sprint_i32(passed)\n";
    source << "    failed_buf: *Buffer<u8> = sprint_i32(failures)\n";
    source << "    if total_buf == nil || passed_buf == nil || failed_buf == nil {\n";
    source << "        if total_buf != nil {\n";
    source << "            mem.buffer_free<u8>(total_buf)\n";
    source << "        }\n";
    source << "        if passed_buf != nil {\n";
    source << "            mem.buffer_free<u8>(passed_buf)\n";
    source << "        }\n";
    source << "        if failed_buf != nil {\n";
    source << "            mem.buffer_free<u8>(failed_buf)\n";
    source << "        }\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    defer mem.buffer_free<u8>(total_buf)\n";
    source << "    defer mem.buffer_free<u8>(passed_buf)\n";
    source << "    defer mem.buffer_free<u8>(failed_buf)\n";
    source << "    total_bytes: Slice<u8> = mem.slice_from_buffer<u8>(total_buf)\n";
    source << "    passed_bytes: Slice<u8> = mem.slice_from_buffer<u8>(passed_buf)\n";
    source << "    failed_bytes: Slice<u8> = mem.slice_from_buffer<u8>(failed_buf)\n";
    source << "    total_text: str = str{ ptr: total_bytes.ptr, len: total_bytes.len }\n";
    source << "    passed_text: str = str{ ptr: passed_bytes.ptr, len: passed_bytes.len }\n";
    source << "    failed_text: str = str{ ptr: failed_bytes.ptr, len: failed_bytes.len }\n";
    source << "    if io.write(total_text) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write(\" tests, \" ) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write(passed_text) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write(\" passed, \" ) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write(failed_text) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write_line(\" failed\") != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    return 0\n";
    source << "}\n";
    source << "\n";
    source << "func main() i32 {\n";
    source << "    failures: i32 = 0\n";
    for (const auto& ordinary_test : ordinary_tests) {
        const std::string test_name = ordinary_test.module_name + "." + ordinary_test.function_name;
        source << "    if " << ordinary_test.module_name << "." << ordinary_test.function_name << "() == nil {\n";
        source << "        if io.write_line(\"PASS " << test_name << "\") != 0 {\n";
        source << "            return 1\n";
        source << "        }\n";
        source << "    } else {\n";
        source << "        if io.write_line(\"FAIL " << test_name << "\") != 0 {\n";
        source << "            return 1\n";
        source << "        }\n";
        source << "        failures = failures + 1\n";
        source << "    }\n";
    }
    source << "    if write_summary(" << total_tests << ", failures) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if failures == 0 {\n";
    source << "        return 0\n";
    source << "    }\n";
    source << "    return 1\n";
    source << "}\n";
    return source.str();
}

bool RunOrdinaryTestsForTarget(const ProjectFile& project,
                               const ProjectTarget& target,
                               const CommandOptions& options,
                               const std::vector<std::filesystem::path>& test_roots,
                               const std::vector<OrdinaryTestCase>& ordinary_tests,
                               const std::filesystem::path& build_dir) {
    const std::filesystem::path generated_root = build_dir / "generated_tests" / target.name;
    const std::filesystem::path runner_path = generated_root / "__mc_test_runner.mc";
    const std::string test_mode = options.mode_override.value_or(target.tests.mode);

    std::cout << "ordinary tests target " << target.name << ": " << ordinary_tests.size()
              << " cases, mode=" << test_mode;
    if (target.tests.timeout_ms.has_value()) {
        std::cout << ", timeout=" << *target.tests.timeout_ms << " ms";
    }
    std::cout << '\n';

    support::DiagnosticSink diagnostics;
    if (!WriteTextArtifact(runner_path, BuildOrdinaryRunnerSource(ordinary_tests), "test runner source", diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return false;
    }

    const auto runner_import_roots = BuildTestImportRoots(project,
                                                          target,
                                                          options.import_roots,
                                                          test_roots,
                                                          generated_root,
                                                          diagnostics);
    if (!runner_import_roots.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return false;
    }
    CommandOptions runner_options;
    runner_options.source_path = runner_path;
    runner_options.build_dir = generated_root;
    runner_options.build_dir_explicit = true;
    runner_options.import_roots = *runner_import_roots;

    const auto build_result = BuildDirectSource(runner_options, diagnostics);
    if (!build_result.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return false;
    }

    const auto output_path = generated_root / "runner.stdout.txt";
    const auto run_result = RunExecutableCapture(build_result->executable_path, {}, output_path, target.tests.timeout_ms);
    if (!run_result.output.empty()) {
        std::cout << run_result.output;
        if (run_result.output.back() != '\n') {
            std::cout << '\n';
        }
    }
    if (run_result.timed_out) {
        std::cout << "TIMEOUT ordinary tests for target " << target.name << " after " << target.tests.timeout_ms.value_or(0) << " ms" << '\n';
        std::cout << "FAIL ordinary tests for target " << target.name << " (timeout)" << '\n';
        return false;
    }

    const bool ok = run_result.exited && run_result.exit_code == 0;
    std::cout << (ok ? "PASS ordinary tests for target " : "FAIL ordinary tests for target ") << target.name
              << " (" << ordinary_tests.size() << " cases)" << '\n';
    return ok;
}

bool EvaluateCheckPassCase(const CompilerRegressionCase& regression_case,
                           const std::vector<std::filesystem::path>& import_roots,
                           const std::filesystem::path& build_dir,
                           std::string& failure_detail) {
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    if (!CompileToMir(options, diagnostics, false).has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }
    return true;
}

bool EvaluateCheckFailCase(const CompilerRegressionCase& regression_case,
                           const std::vector<std::filesystem::path>& import_roots,
                           const std::filesystem::path& build_dir,
                           std::string& failure_detail) {
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    if (CompileToMir(options, diagnostics, false).has_value()) {
        failure_detail = "compiler regression should fail semantic checking";
        return false;
    }

    const auto expected_lines = ReadExpectedLines(regression_case.expectation_path, diagnostics);
    const std::string rendered = diagnostics.Render();
    for (const auto& expected_line : expected_lines) {
        if (rendered.find(expected_line) == std::string::npos) {
            failure_detail = "missing expected diagnostic substring: " + expected_line + "\n" + rendered;
            return false;
        }
    }
    return true;
}

bool EvaluateMirCase(const CompilerRegressionCase& regression_case,
                     const std::vector<std::filesystem::path>& import_roots,
                     const std::filesystem::path& build_dir,
                     std::string& failure_detail) {
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    const auto checked = CompileToMir(options, diagnostics, false);
    if (!checked.has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }

    const auto expected = ReadSourceText(regression_case.expectation_path, diagnostics);
    if (!expected.has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }
    const std::string actual = mc::mir::DumpModule(*checked->mir_result.module);
    if (actual != *expected) {
        failure_detail = "expected MIR fixture mismatch";
        return false;
    }
    return true;
}

bool EvaluateRunOutputCase(const CompilerRegressionCase& regression_case,
                           const std::vector<std::filesystem::path>& import_roots,
                           const std::filesystem::path& build_dir,
                           const std::optional<int>& timeout_ms,
                           std::string& failure_detail) {
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    const auto checked = CompileToMir(options, diagnostics, true);
    if (!checked.has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }

    const auto build_targets = support::ComputeBuildArtifactTargets(regression_case.source_path, build_dir);
    const auto runtime_source_path = DiscoverHostedRuntimeSupportSource(regression_case.source_path);
    const auto build_result = mc::codegen_llvm::BuildExecutable(
        *checked->mir_result.module,
        regression_case.source_path,
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .runtime_source_path = runtime_source_path,
            .artifacts = {
                .llvm_ir_path = build_targets.llvm_ir,
                .object_path = build_targets.object,
                .executable_path = build_targets.executable,
            },
        },
        diagnostics);
    if (!build_result.ok) {
        failure_detail = diagnostics.Render();
        return false;
    }

    const std::string build_diagnostics = diagnostics.Render();

    support::DiagnosticSink output_diagnostics;
    const auto expected = ReadSourceText(regression_case.expectation_path, output_diagnostics);
    if (!expected.has_value()) {
        failure_detail = MergeRenderedDiagnostics(build_diagnostics, output_diagnostics.Render());
        return false;
    }
    const CapturedCommandResult run_result = RunExecutableCapture(build_targets.executable,
                                                                  {},
                                                                  build_dir / (support::SanitizeArtifactStem(regression_case.source_path) + ".stdout.txt"),
                                                                  timeout_ms);
    if (run_result.timed_out) {
        failure_detail = "test executable timed out after " + std::to_string(timeout_ms.value_or(0)) + " ms";
        return false;
    }
    if (!run_result.exited || run_result.exit_code != regression_case.expected_exit_code || run_result.output != *expected) {
        failure_detail = "expected exit code " + std::to_string(regression_case.expected_exit_code) + " and stdout '" + *expected +
                         "', got exit=" + std::to_string(run_result.exit_code) + " stdout='" + run_result.output + "'";
        return false;
    }
    return true;
}

bool RunCompilerRegressionCases(const std::vector<CompilerRegressionCase>& regression_cases,
                                const std::vector<std::filesystem::path>& import_roots,
                                const std::filesystem::path& build_dir,
                                const std::optional<int>& timeout_ms) {
    bool ok = true;
    for (std::size_t index = 0; index < regression_cases.size(); ++index) {
        const auto& regression_case = regression_cases[index];
        std::string failure_detail;
        const std::filesystem::path case_build_dir = build_dir / "compiler_regressions" / std::to_string(index);
        std::string label;
        bool case_ok = false;
        switch (regression_case.kind) {
            case CompilerRegressionKind::kCheckPass:
                label = "check-pass ";
                case_ok = EvaluateCheckPassCase(regression_case, import_roots, case_build_dir, failure_detail);
                break;
            case CompilerRegressionKind::kCheckFail:
                label = "check-fail ";
                case_ok = EvaluateCheckFailCase(regression_case, import_roots, case_build_dir, failure_detail);
                break;
            case CompilerRegressionKind::kRunOutput:
                label = "run-output ";
                case_ok = EvaluateRunOutputCase(regression_case, import_roots, case_build_dir, timeout_ms, failure_detail);
                break;
            case CompilerRegressionKind::kMir:
                label = "mir ";
                case_ok = EvaluateMirCase(regression_case, import_roots, case_build_dir, failure_detail);
                break;
        }

        std::cout << (case_ok ? "PASS " : "FAIL ") << label << regression_case.source_path.generic_string() << '\n';
        if (!case_ok) {
            ok = false;
            if (!failure_detail.empty()) {
                std::cout << failure_detail << '\n';
            }
        }
    }
    return ok;
}

std::vector<const ProjectTarget*> SelectTestTargets(const ProjectFile& project,
                                                    const std::optional<std::string>& explicit_target,
                                                    support::DiagnosticSink& diagnostics) {
    std::vector<const ProjectTarget*> targets;
    if (explicit_target.has_value()) {
        const ProjectTarget* target = SelectProjectTarget(project, explicit_target, diagnostics);
        if (target != nullptr) {
            if (!target->tests.enabled) {
                diagnostics.Report({
                    .file_path = project.path,
                    .span = support::kDefaultSourceSpan,
                    .severity = support::DiagnosticSeverity::kError,
                    .message = "tests are not enabled for target '" + target->name +
                               "'; enabled test targets: " + JoinNames(CollectEnabledTestTargetNames(project)),
                });
            } else {
                targets.push_back(target);
            }
        }
        return targets;
    }

    for (const auto& [name, target] : project.targets) {
        (void)name;
        if (target.tests.enabled) {
            targets.push_back(&target);
        }
    }
    std::sort(targets.begin(), targets.end(), [](const ProjectTarget* left, const ProjectTarget* right) {
        return left->name < right->name;
    });
    if (targets.empty()) {
        diagnostics.Report({
            .file_path = project.path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "project file does not enable tests for any target",
        });
    }
    return targets;
}

}  // namespace

int RunTestCommand(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    auto project = LoadSelectedProject(options, diagnostics);
    if (!project.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const auto targets = SelectTestTargets(*project, options.target_name, diagnostics);
    if (diagnostics.HasErrors()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    bool ok = true;
    for (const auto* target : targets) {
        CommandOptions target_options = options;
        target_options.project_path = project->path;
        target_options.target_name = target->name;
        if (!target_options.mode_override.has_value()) {
            target_options.mode_override = target->tests.mode;
        }

        auto graph = BuildTargetGraph(target_options, diagnostics);
        if (!graph.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        const auto discovered = DiscoverTargetTests(*project, *target, diagnostics);
        if (!discovered.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        std::cout << "testing target " << target->name << '\n';
        bool target_ok = true;
        if (!discovered->ordinary_tests.empty()) {
            target_ok = RunOrdinaryTestsForTarget(*project,
                                                 *target,
                                                 target_options,
                                                 discovered->roots,
                                                 discovered->ordinary_tests,
                                                 graph->compile_graph.build_dir) && target_ok;
        }
        if (!discovered->regression_cases.empty()) {
            const auto import_roots = ComputeProjectImportRoots(*project, *target, options.import_roots);
            std::cout << "compiler regressions target " << target->name << ": " << discovered->regression_cases.size() << " cases";
            if (target->tests.timeout_ms.has_value()) {
                std::cout << ", timeout=" << *target->tests.timeout_ms << " ms";
            }
            std::cout << '\n';
            const bool regressions_ok = RunCompilerRegressionCases(discovered->regression_cases,
                                                                   import_roots,
                                                                   graph->compile_graph.build_dir,
                                                                   target->tests.timeout_ms);
            std::cout << (regressions_ok ? "PASS compiler regressions for target " : "FAIL compiler regressions for target ")
                      << target->name << " (" << discovered->regression_cases.size() << " cases)" << '\n';
            target_ok = regressions_ok && target_ok;
        }
        if (discovered->ordinary_tests.empty() && discovered->regression_cases.empty()) {
            std::cout << "SKIP target " << target->name << " (no tests discovered)" << '\n';
        }
        std::cout << (target_ok ? "PASS target " : "FAIL target ") << target->name << '\n';
        ok = ok && target_ok;
    }

    return ok ? 0 : 1;
}

}  // namespace mc::driver