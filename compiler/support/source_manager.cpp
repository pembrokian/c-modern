#include "compiler/support/source_manager.h"

#include <fstream>
#include <sstream>

namespace mc::support {

std::optional<std::size_t> SourceManager::LoadFile(const std::filesystem::path& path, DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        diagnostics.Report({
            .file_path = std::filesystem::absolute(path).lexically_normal(),
            .span = kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = "unable to read source file",
        });
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    const auto next_id = files_.size();
    files_.push_back({
        .id = next_id,
        .path = std::filesystem::absolute(path).lexically_normal(),
        .contents = buffer.str(),
    });
    return next_id;
}

const SourceFile* SourceManager::GetFile(std::size_t id) const {
    if (id >= files_.size()) {
        return nullptr;
    }

    return &files_[id];
}

}  // namespace mc::support
