#ifndef C_MODERN_COMPILER_DRIVER_PROJECT_H_
#define C_MODERN_COMPILER_DRIVER_PROJECT_H_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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

struct ProjectModuleSet {
    std::string module_name;
    std::vector<std::filesystem::path> files;
    std::size_t files_line = 0;
};

struct ProjectTarget {
    std::string name;
    std::size_t decl_line = 0;
    std::string kind = "exe";
    std::string package_name;
    std::filesystem::path root;
    std::string mode = "debug";
    std::string env = "hosted";
    std::string target;
    std::vector<std::string> links;
    std::vector<std::filesystem::path> link_inputs;
    std::vector<std::filesystem::path> module_search_paths;
    std::unordered_map<std::string, std::vector<std::filesystem::path>> package_roots;
    std::unordered_map<std::string, ProjectModuleSet> module_sets;
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

const ProjectModuleSet* LookupProjectModuleSet(const ProjectTarget& target,
                                               std::string_view module_name);

const ProjectModuleSet* FindProjectModuleSetForSource(const ProjectTarget& target,
                                                      const std::filesystem::path& source_path);

std::optional<std::string> ResolveTargetPackageIdentity(const ProjectTarget& target,
                                                        const std::filesystem::path& source_path);

void ReportBootstrapTargetNotes(const ProjectFile& project,
                                std::string_view target_name,
                                support::DiagnosticSink& diagnostics);

}  // namespace mc::driver

#endif  // C_MODERN_COMPILER_DRIVER_PROJECT_H_