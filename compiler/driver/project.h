#ifndef C_MODERN_COMPILER_DRIVER_PROJECT_H_
#define C_MODERN_COMPILER_DRIVER_PROJECT_H_

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "compiler/support/diagnostics.h"

namespace mc::driver {

struct ProjectTargetTests {
    bool enabled = false;
    std::vector<std::filesystem::path> roots;
    std::string mode = "checked";
    std::optional<int> timeout_ms;
};

struct ProjectTarget {
    std::string name;
    std::string kind = "exe";
    std::filesystem::path root;
    std::string mode = "debug";
    std::string env = "hosted";
    std::string target;
    std::vector<std::string> links;
    std::vector<std::filesystem::path> module_search_paths;
    std::string runtime_startup = "default";
    ProjectTargetTests tests;
};

struct ProjectFile {
    std::filesystem::path path;
    std::filesystem::path root_dir;
    std::string project_name;
    std::optional<std::string> default_target;
    std::filesystem::path build_dir = "build";
    std::unordered_map<std::string, ProjectTarget> targets;
};

std::optional<ProjectFile> LoadProjectFile(const std::filesystem::path& path,
                                          support::DiagnosticSink& diagnostics);

const ProjectTarget* SelectProjectTarget(const ProjectFile& project,
                                         const std::optional<std::string>& explicit_target,
                                         support::DiagnosticSink& diagnostics);

std::vector<std::filesystem::path> ComputeProjectImportRoots(const ProjectFile& project,
                                                             const ProjectTarget& target,
                                                             const std::vector<std::filesystem::path>& cli_import_roots);

}  // namespace mc::driver

#endif  // C_MODERN_COMPILER_DRIVER_PROJECT_H_