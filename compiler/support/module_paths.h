#ifndef C_MODERN_COMPILER_SUPPORT_MODULE_PATHS_H_
#define C_MODERN_COMPILER_SUPPORT_MODULE_PATHS_H_

#include <filesystem>
#include <string_view>

namespace mc::support {

constexpr std::string_view kInternalModuleFilename = "internal.mc";

inline bool IsInternalModulePath(const std::filesystem::path& path) {
    return path.filename() == kInternalModuleFilename;
}

}  // namespace mc::support

#endif  // C_MODERN_COMPILER_SUPPORT_MODULE_PATHS_H_