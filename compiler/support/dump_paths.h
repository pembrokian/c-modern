#ifndef C_MODERN_COMPILER_SUPPORT_DUMP_PATHS_H_
#define C_MODERN_COMPILER_SUPPORT_DUMP_PATHS_H_

#include <filesystem>
#include <string>

namespace mc::support {

struct DumpTargets {
    std::filesystem::path ast;
    std::filesystem::path mir;
    std::filesystem::path backend;
    std::filesystem::path mci;
};

struct BuildArtifactTargets {
    std::filesystem::path llvm_ir;
    std::filesystem::path object;
    std::filesystem::path executable;
    std::filesystem::path static_library;
};

DumpTargets ComputeDumpTargets(const std::filesystem::path& source_path,
                               const std::filesystem::path& build_dir);

DumpTargets ComputeLogicalDumpTargets(std::string_view artifact_key,
                                      const std::filesystem::path& build_dir);

BuildArtifactTargets ComputeBuildArtifactTargets(const std::filesystem::path& source_path,
                                                 const std::filesystem::path& build_dir);

BuildArtifactTargets ComputeLogicalBuildArtifactTargets(std::string_view artifact_key,
                                                        const std::filesystem::path& build_dir);

std::string SanitizeArtifactStem(const std::filesystem::path& source_path);
std::string SanitizeLogicalArtifactStem(std::string_view artifact_key);

}  // namespace mc::support

#endif  // C_MODERN_COMPILER_SUPPORT_DUMP_PATHS_H_
