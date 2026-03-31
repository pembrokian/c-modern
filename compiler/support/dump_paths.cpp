#include "compiler/support/dump_paths.h"

#include <cctype>

namespace mc::support {

std::string SanitizeArtifactStem(const std::filesystem::path& source_path) {
    const auto normalized = source_path.lexically_normal().generic_string();
    std::string stem;
    stem.reserve(normalized.size());

    for (const unsigned char byte : normalized) {
        if (std::isalnum(byte) != 0) {
            stem.push_back(static_cast<char>(byte));
            continue;
        }

        if (byte == '.') {
            stem.push_back('.');
            continue;
        }

        stem.push_back('_');
    }

    if (stem.empty()) {
        stem = "module";
    }

    return stem;
}

DumpTargets ComputeDumpTargets(const std::filesystem::path& source_path,
                               const std::filesystem::path& build_dir) {
    const auto stem = SanitizeArtifactStem(source_path);
    return {
        .ast = build_dir / "dumps" / "ast" / (stem + ".ast.txt"),
        .mir = build_dir / "dumps" / "mir" / (stem + ".mir.txt"),
        .backend = build_dir / "dumps" / "backend" / (stem + ".backend.txt"),
        .mci = build_dir / "mci" / (stem + ".mci"),
    };
}

}  // namespace mc::support
