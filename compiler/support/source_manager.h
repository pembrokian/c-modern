#ifndef C_MODERN_COMPILER_SUPPORT_SOURCE_MANAGER_H_
#define C_MODERN_COMPILER_SUPPORT_SOURCE_MANAGER_H_

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "compiler/support/diagnostics.h"

namespace mc::support {

struct SourceFile {
    std::size_t id = 0;
    std::filesystem::path path;
    std::string contents;
};

class SourceManager {
  public:
    std::optional<std::size_t> LoadFile(const std::filesystem::path& path, DiagnosticSink& diagnostics);
    const SourceFile* GetFile(std::size_t id) const;

  private:
    std::vector<SourceFile> files_;
};

}  // namespace mc::support

#endif  // C_MODERN_COMPILER_SUPPORT_SOURCE_MANAGER_H_
