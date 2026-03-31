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

DumpTargets ComputeDumpTargets(const std::filesystem::path& source_path,
                               const std::filesystem::path& build_dir);

std::string SanitizeArtifactStem(const std::filesystem::path& source_path);

}  // namespace mc::support

#endif  // C_MODERN_COMPILER_SUPPORT_DUMP_PATHS_H_
