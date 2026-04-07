#include "compiler/driver/project.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace mc::driver {
namespace {

using mc::support::DiagnosticSeverity;

bool IsSupportedBootstrapTargetKind(std::string_view kind) {
    return kind == "exe" || kind == "test" || kind == "staticlib";
}

struct ParsedValue {
    enum class Kind {
        kString,
        kInteger,
        kBoolean,
        kStringArray,
    };

    Kind kind = Kind::kString;
    std::string string_value;
    int integer_value = 0;
    bool bool_value = false;
    std::vector<std::string> string_array_value;
};

struct SourceLine {
    std::size_t number = 0;
    std::string text;
};

std::string Trim(std::string_view text) {
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

mc::support::SourceSpan MakeLineSpan(std::size_t line) {
    return {{line, 1}, {line, 1}};
}

void ReportProjectError(const std::filesystem::path& path,
                        std::size_t line,
                        std::string message,
                        support::DiagnosticSink& diagnostics) {
    diagnostics.Report({
        .file_path = path,
        .span = MakeLineSpan(line),
        .severity = DiagnosticSeverity::kError,
        .message = std::move(message),
    });
}

bool ReportDuplicateConfiguredPaths(const std::filesystem::path& project_path,
                                   std::string_view target_name,
                                   std::string_view label,
                                   const std::vector<std::filesystem::path>& paths,
                                   support::DiagnosticSink& diagnostics) {
    std::unordered_set<std::string> seen;
    for (const auto& path : paths) {
        const std::string normalized = path.lexically_normal().generic_string();
        if (!seen.insert(normalized).second) {
            diagnostics.Report({
                .file_path = project_path,
                .span = mc::support::kDefaultSourceSpan,
                .severity = DiagnosticSeverity::kError,
                .message = "target '" + std::string(target_name) + "' declares duplicate " + std::string(label) + ": " + normalized,
            });
            return true;
        }
    }
    return false;
}

bool IsPathWithinRoot(const std::filesystem::path& path,
                     const std::filesystem::path& root) {
    const std::filesystem::path normalized_path = std::filesystem::absolute(path).lexically_normal();
    const std::filesystem::path normalized_root = std::filesystem::absolute(root).lexically_normal();
    auto path_it = normalized_path.begin();
    auto root_it = normalized_root.begin();
    for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }
    return true;
}

std::string StripComment(std::string_view line) {
    std::string output;
    output.reserve(line.size());

    bool in_string = false;
    bool escape = false;
    for (char ch : line) {
        if (escape) {
            output.push_back(ch);
            escape = false;
            continue;
        }

        if (ch == '\\') {
            output.push_back(ch);
            escape = true;
            continue;
        }

        if (ch == '"') {
            output.push_back(ch);
            in_string = !in_string;
            continue;
        }

        if (ch == '#' && !in_string) {
            break;
        }

        output.push_back(ch);
    }

    return output;
}

std::string JoinSortedNames(const std::vector<std::string>& names) {
    std::string text;
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index > 0) {
            text += ", ";
        }
        text += names[index];
    }
    return text;
}

std::vector<std::string> SortedTargetNames(const ProjectFile& project) {
    std::vector<std::string> names;
    names.reserve(project.targets.size());
    for (const auto& [name, target] : project.targets) {
        (void)target;
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::optional<std::string> ParseQuotedString(std::string_view text) {
    if (text.size() < 2 || text.front() != '"' || text.back() != '"') {
        return std::nullopt;
    }

    std::string output;
    output.reserve(text.size() - 2);
    bool escape = false;
    for (std::size_t index = 1; index + 1 < text.size(); ++index) {
        const char ch = text[index];
        if (escape) {
            switch (ch) {
                case 'n':
                    output.push_back('\n');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                case '\\':
                case '"':
                    output.push_back(ch);
                    break;
                default:
                    return std::nullopt;
            }
            escape = false;
            continue;
        }

        if (ch == '\\') {
            escape = true;
            continue;
        }

        output.push_back(ch);
    }

    if (escape) {
        return std::nullopt;
    }

    return output;
}

std::optional<std::vector<std::string>> ParseStringArray(std::string_view text) {
    if (text.size() < 2 || text.front() != '[' || text.back() != ']') {
        return std::nullopt;
    }

    std::vector<std::string> values;
    std::string current;
    bool in_string = false;
    bool escape = false;
    bool saw_value = false;

    for (std::size_t index = 1; index + 1 < text.size(); ++index) {
        const char ch = text[index];
        if (!in_string) {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                continue;
            }
            if (ch == ',') {
                if (!saw_value) {
                    return std::nullopt;
                }
                saw_value = false;
                continue;
            }
            if (ch != '"') {
                return std::nullopt;
            }
            current.clear();
            in_string = true;
            escape = false;
            continue;
        }

        if (escape) {
            switch (ch) {
                case 'n':
                    current.push_back('\n');
                    break;
                case 't':
                    current.push_back('\t');
                    break;
                case '\\':
                case '"':
                    current.push_back(ch);
                    break;
                default:
                    return std::nullopt;
            }
            escape = false;
            continue;
        }

        if (ch == '\\') {
            escape = true;
            continue;
        }

        if (ch == '"') {
            values.push_back(current);
            in_string = false;
            saw_value = true;
            continue;
        }

        current.push_back(ch);
    }

    if (in_string || escape) {
        return std::nullopt;
    }

    return values;
}

std::optional<ParsedValue> ParseValue(std::string_view text) {
    const std::string trimmed = Trim(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    if (trimmed.front() == '"') {
        const auto parsed = ParseQuotedString(trimmed);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        return ParsedValue {
            .kind = ParsedValue::Kind::kString,
            .string_value = *parsed,
        };
    }

    if (trimmed.front() == '[') {
        const auto parsed = ParseStringArray(trimmed);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        return ParsedValue {
            .kind = ParsedValue::Kind::kStringArray,
            .string_array_value = std::move(*parsed),
        };
    }

    if (trimmed == "true" || trimmed == "false") {
        return ParsedValue {
            .kind = ParsedValue::Kind::kBoolean,
            .bool_value = trimmed == "true",
        };
    }

    bool negative = false;
    std::size_t start = 0;
    if (trimmed.front() == '-') {
        negative = true;
        start = 1;
    }
    if (start < trimmed.size() &&
        std::all_of(trimmed.begin() + static_cast<std::ptrdiff_t>(start), trimmed.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) {
        int value = 0;
        for (std::size_t index = start; index < trimmed.size(); ++index) {
            value = (value * 10) + (trimmed[index] - '0');
        }
        if (negative) {
            value = -value;
        }
        return ParsedValue {
            .kind = ParsedValue::Kind::kInteger,
            .integer_value = value,
        };
    }

    return std::nullopt;
}

std::optional<std::vector<std::string>> ParseTablePath(std::string_view text) {
    if (text.size() < 3 || text.front() != '[' || text.back() != ']') {
        return std::nullopt;
    }

    std::vector<std::string> parts;
    std::string current;
    for (std::size_t index = 1; index + 1 < text.size(); ++index) {
        const char ch = text[index];
        if (ch == '.') {
            if (current.empty()) {
                return std::nullopt;
            }
            parts.push_back(current);
            current.clear();
            continue;
        }
        if (!(std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '-')) {
            return std::nullopt;
        }
        current.push_back(ch);
    }

    if (current.empty()) {
        return std::nullopt;
    }
    parts.push_back(current);
    return parts;
}

bool ExpectValueKind(const ParsedValue& value,
                     ParsedValue::Kind expected,
                     const std::filesystem::path& path,
                     std::size_t line,
                     std::string_view key,
                     support::DiagnosticSink& diagnostics) {
    if (value.kind == expected) {
        return true;
    }

    ReportProjectError(path, line, "invalid value type for key '" + std::string(key) + "'", diagnostics);
    return false;
}

ProjectTarget* LookupTarget(ProjectFile& project,
                            std::string_view target_name,
                            const std::filesystem::path& path,
                            std::size_t line,
                            support::DiagnosticSink& diagnostics) {
    if (target_name.empty()) {
        ReportProjectError(path, line, "target section is missing a target name", diagnostics);
        return nullptr;
    }

    auto [it, inserted] = project.targets.emplace(std::string(target_name), ProjectTarget {});
    if (inserted) {
        it->second.name = it->first;
    }
    return &it->second;
}

bool IsSupportedMode(std::string_view mode) {
    return mode == "debug" || mode == "release" || mode == "checked";
}

}  // namespace

std::optional<ProjectFile> LoadProjectFile(const std::filesystem::path& path,
                                          support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        diagnostics.Report({
            .file_path = std::filesystem::absolute(path).lexically_normal(),
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "unable to read project file",
        });
        return std::nullopt;
    }

    ProjectFile project;
    project.path = std::filesystem::absolute(path).lexically_normal();
    project.root_dir = project.path.parent_path();

    std::vector<std::string> section;
    std::string raw_line;
    std::size_t line_number = 0;
    while (std::getline(input, raw_line)) {
        ++line_number;
        const std::string without_comment = StripComment(raw_line);
        const std::string line = Trim(without_comment);
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[') {
            const auto parsed_section = ParseTablePath(line);
            if (!parsed_section.has_value()) {
                ReportProjectError(project.path, line_number, "invalid table header", diagnostics);
                continue;
            }
            section = std::move(*parsed_section);
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            ReportProjectError(project.path, line_number, "expected 'key = value' entry", diagnostics);
            continue;
        }

        const std::string key = Trim(std::string_view(line).substr(0, equals));
        const auto parsed_value = ParseValue(std::string_view(line).substr(equals + 1));
        if (key.empty() || !parsed_value.has_value()) {
            ReportProjectError(project.path, line_number, "invalid project entry", diagnostics);
            continue;
        }

        if (section.empty()) {
            if (key == "schema") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kInteger, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                if (parsed_value->integer_value != 1) {
                    ReportProjectError(project.path, line_number, "unsupported build.toml schema version", diagnostics);
                }
                continue;
            }

            if (key == "project") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                project.project_name = parsed_value->string_value;
                continue;
            }

            if (key == "default") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                project.default_target = parsed_value->string_value;
                continue;
            }

            if (key == "build_dir") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                project.build_dir = parsed_value->string_value;
                continue;
            }

            ReportProjectError(project.path, line_number, "unknown top-level key: " + key, diagnostics);
            continue;
        }

        if (section.size() < 2 || section[0] != "targets") {
            ReportProjectError(project.path, line_number, "unsupported table path in project file", diagnostics);
            continue;
        }

        ProjectTarget* target = LookupTarget(project, section[1], project.path, line_number, diagnostics);
        if (target == nullptr) {
            continue;
        }

        if (section.size() == 2) {
            if (key == "kind") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->kind = parsed_value->string_value;
                continue;
            }
            if (key == "package") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->package_name = parsed_value->string_value;
                continue;
            }
            if (key == "root") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->root = parsed_value->string_value;
                continue;
            }
            if (key == "mode") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->mode = parsed_value->string_value;
                continue;
            }
            if (key == "env") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->env = parsed_value->string_value;
                continue;
            }
            if (key == "target") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->target = parsed_value->string_value;
                continue;
            }
            if (key == "links") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kStringArray, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->links = parsed_value->string_array_value;
                continue;
            }

            ReportProjectError(project.path, line_number, "unknown target key: " + key, diagnostics);
            continue;
        }

        if (section.size() == 3 && section[2] == "search_paths") {
            if (key != "modules") {
                ReportProjectError(project.path, line_number, "unknown search_paths key: " + key, diagnostics);
                continue;
            }
            if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kStringArray, project.path, line_number, key, diagnostics)) {
                continue;
            }
            target->module_search_paths.clear();
            for (const auto& entry : parsed_value->string_array_value) {
                target->module_search_paths.push_back(entry);
            }
            continue;
        }

        if (section.size() == 4 && section[2] == "packages") {
            if (key != "roots") {
                ReportProjectError(project.path, line_number, "unknown packages key: " + key, diagnostics);
                continue;
            }
            if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kStringArray, project.path, line_number, key, diagnostics)) {
                continue;
            }
            auto& package_roots = target->package_roots[section[3]];
            package_roots.clear();
            for (const auto& entry : parsed_value->string_array_value) {
                package_roots.push_back(entry);
            }
            continue;
        }

        if (section.size() == 3 && section[2] == "runtime") {
            if (key != "startup") {
                ReportProjectError(project.path, line_number, "unknown runtime key: " + key, diagnostics);
                continue;
            }
            if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                continue;
            }
            target->runtime_startup = parsed_value->string_value;
            continue;
        }

        if (section.size() == 3 && section[2] == "tests") {
            if (key == "enabled") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kBoolean, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->tests.enabled = parsed_value->bool_value;
                continue;
            }
            if (key == "roots") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kStringArray, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->tests.roots.clear();
                for (const auto& entry : parsed_value->string_array_value) {
                    target->tests.roots.push_back(entry);
                }
                continue;
            }
            if (key == "mode") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kString, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->tests.mode = parsed_value->string_value;
                continue;
            }
            if (key == "timeout_ms") {
                if (!ExpectValueKind(*parsed_value, ParsedValue::Kind::kInteger, project.path, line_number, key, diagnostics)) {
                    continue;
                }
                target->tests.timeout_ms = parsed_value->integer_value;
                continue;
            }

            ReportProjectError(project.path, line_number, "unknown tests key: " + key, diagnostics);
            continue;
        }

        ReportProjectError(project.path, line_number, "unsupported nested table in project file", diagnostics);
    }

    if (project.project_name.empty()) {
        diagnostics.Report({
            .file_path = project.path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "project file is missing required top-level key: project",
        });
    }

    if (project.targets.empty()) {
        diagnostics.Report({
            .file_path = project.path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "project file does not define any targets",
        });
    }

    for (auto& [name, target] : project.targets) {
        if (!IsSupportedBootstrapTargetKind(target.kind)) {
            diagnostics.Report({
                .file_path = project.path,
                .span = mc::support::kDefaultSourceSpan,
                .severity = DiagnosticSeverity::kError,
                .message = "target '" + name + "' uses unsupported bootstrap target kind: " + target.kind,
            });
        }
        if (target.root.empty()) {
            diagnostics.Report({
                .file_path = project.path,
                .span = mc::support::kDefaultSourceSpan,
                .severity = DiagnosticSeverity::kError,
                .message = "target '" + name + "' is missing required key: root",
            });
        } else {
            target.root = std::filesystem::absolute(project.root_dir / target.root).lexically_normal();
        }
        if (target.package_name.empty()) {
            target.package_name = name;
        }
        if (!IsSupportedMode(target.mode)) {
            diagnostics.Report({
                .file_path = project.path,
                .span = mc::support::kDefaultSourceSpan,
                .severity = DiagnosticSeverity::kError,
                .message = "target '" + name + "' uses unsupported build mode: " + target.mode,
            });
        }
        if (target.env != "hosted") {
            diagnostics.Report({
                .file_path = project.path,
                .span = mc::support::kDefaultSourceSpan,
                .severity = DiagnosticSeverity::kError,
                .message = "target '" + name + "' uses unsupported Phase 7 environment: " + target.env,
            });
        }
        if (target.runtime_startup != "default") {
            diagnostics.Report({
                .file_path = project.path,
                .span = mc::support::kDefaultSourceSpan,
                .severity = DiagnosticSeverity::kError,
                .message = "target '" + name + "' uses unsupported runtime startup: " + target.runtime_startup,
            });
        }
        for (auto& module_root : target.module_search_paths) {
            module_root = std::filesystem::absolute(project.root_dir / module_root).lexically_normal();
        }
        for (auto& [package_name, package_roots] : target.package_roots) {
            if (package_name.empty()) {
                diagnostics.Report({
                    .file_path = project.path,
                    .span = mc::support::kDefaultSourceSpan,
                    .severity = DiagnosticSeverity::kError,
                    .message = "target '" + name + "' declares an empty package identity",
                });
            }
            for (auto& package_root : package_roots) {
                package_root = std::filesystem::absolute(project.root_dir / package_root).lexically_normal();
            }
            ReportDuplicateConfiguredPaths(project.path,
                                           name,
                                           "package root",
                                           package_roots,
                                           diagnostics);
        }
        for (auto& test_root : target.tests.roots) {
            test_root = std::filesystem::absolute(project.root_dir / test_root).lexically_normal();
        }
        ReportDuplicateConfiguredPaths(project.path,
                                       name,
                                       "module search path",
                                       target.module_search_paths,
                                       diagnostics);
        ReportDuplicateConfiguredPaths(project.path,
                                       name,
                                       "test root",
                                       target.tests.roots,
                                       diagnostics);
        std::unordered_map<std::string, std::string> owned_package_roots;
        for (const auto& [package_name, package_roots] : target.package_roots) {
            for (const auto& package_root : package_roots) {
                const std::string normalized_root = package_root.lexically_normal().generic_string();
                const auto [it, inserted] = owned_package_roots.emplace(normalized_root, package_name);
                if (!inserted && it->second != package_name) {
                    diagnostics.Report({
                        .file_path = project.path,
                        .span = mc::support::kDefaultSourceSpan,
                        .severity = DiagnosticSeverity::kError,
                        .message = "target '" + name + "' assigns source root '" + normalized_root +
                                   "' to multiple package identities: '" + it->second + "' and '" + package_name + "'",
                    });
                }
            }
        }
        if (!IsSupportedMode(target.tests.mode)) {
            diagnostics.Report({
                .file_path = project.path,
                .span = mc::support::kDefaultSourceSpan,
                .severity = DiagnosticSeverity::kError,
                .message = "target '" + name + "' tests use unsupported build mode: " + target.tests.mode,
            });
        }
    }

    std::unordered_map<std::string, std::string> root_to_target;
    for (const auto& [name, target] : project.targets) {
        if (target.root.empty()) {
            continue;
        }
        const std::string normalized_root = target.root.lexically_normal().generic_string();
        const auto [it, inserted] = root_to_target.emplace(normalized_root, name);
        if (!inserted) {
            diagnostics.Report({
                .file_path = project.path,
                .span = mc::support::kDefaultSourceSpan,
                .severity = DiagnosticSeverity::kError,
                .message = "targets '" + it->second + "' and '" + name + "' declare the same root module: " + normalized_root,
            });
        }
    }

    if (project.default_target.has_value() && !project.targets.contains(*project.default_target)) {
        diagnostics.Report({
            .file_path = project.path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "project default target is not defined: " + *project.default_target,
        });
    }

    project.build_dir = std::filesystem::absolute(project.root_dir / project.build_dir).lexically_normal();
    if (diagnostics.HasErrors()) {
        return std::nullopt;
    }

    return project;
}

const ProjectTarget* SelectProjectTarget(const ProjectFile& project,
                                         const std::optional<std::string>& explicit_target,
                                         support::DiagnosticSink& diagnostics) {
    if (explicit_target.has_value()) {
        const auto found = project.targets.find(*explicit_target);
        if (found != project.targets.end()) {
            return &found->second;
        }

        diagnostics.Report({
            .file_path = project.path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "unknown target in project file: " + *explicit_target +
                       "; available targets: " + JoinSortedNames(SortedTargetNames(project)),
        });
        return nullptr;
    }

    if (project.default_target.has_value()) {
        return &project.targets.at(*project.default_target);
    }

    if (project.targets.size() == 1) {
        return &project.targets.begin()->second;
    }

    diagnostics.Report({
        .file_path = project.path,
        .span = mc::support::kDefaultSourceSpan,
        .severity = DiagnosticSeverity::kError,
        .message = "project file does not declare a default target; available targets: " +
                   JoinSortedNames(SortedTargetNames(project)) + "; pass --target <name>",
    });
    return nullptr;
}

std::vector<std::filesystem::path> ComputeProjectImportRoots(const ProjectFile& project,
                                                             const ProjectTarget& target,
                                                             const std::vector<std::filesystem::path>& cli_import_roots) {
    std::vector<std::filesystem::path> roots;
    roots.reserve(cli_import_roots.size() + target.module_search_paths.size() + 1);

    std::unordered_set<std::string> seen;
    const auto append = [&](const std::filesystem::path& root) {
        const std::filesystem::path normalized = std::filesystem::absolute(root).lexically_normal();
        const std::string key = normalized.generic_string();
        if (seen.insert(key).second) {
            roots.push_back(normalized);
        }
    };

    for (const auto& root : cli_import_roots) {
        append(root);
    }
    for (const auto& root : target.module_search_paths) {
        append(root);
    }
    if (target.module_search_paths.empty() && !target.root.empty()) {
        append(target.root.parent_path());
    }

    (void)project;
    return roots;
}

std::optional<std::string> ResolveTargetPackageIdentity(const ProjectTarget& target,
                                                        const std::filesystem::path& source_path) {
    std::optional<std::string> best_package;
    std::size_t best_depth = 0;
    for (const auto& [package_name, package_roots] : target.package_roots) {
        for (const auto& package_root : package_roots) {
            if (!IsPathWithinRoot(source_path, package_root)) {
                continue;
            }
            const std::size_t depth = static_cast<std::size_t>(std::distance(package_root.begin(), package_root.end()));
            if (!best_package.has_value() || depth > best_depth) {
                best_package = package_name;
                best_depth = depth;
            }
        }
    }
    if (best_package.has_value()) {
        return best_package;
    }
    if (!target.package_name.empty()) {
        return target.package_name;
    }
    return std::nullopt;
}

}  // namespace mc::driver