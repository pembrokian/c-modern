#include "compiler/support/dump_paths.h"

#include <cctype>

namespace mc::support {

namespace {

std::string SanitizeArtifactStemText(std::string_view text) {
    std::string stem;
    stem.reserve(text.size());

    for (const unsigned char byte : text) {
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

}  // namespace

std::string SanitizeArtifactStem(const std::filesystem::path& source_path) {
    return SanitizeArtifactStemText(source_path.lexically_normal().generic_string());
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

DumpTargets ComputeLogicalDumpTargets(std::string_view artifact_key,
                                      const std::filesystem::path& build_dir) {
    const auto stem = SanitizeArtifactStemText(artifact_key);
    return {
        .ast = build_dir / "dumps" / "ast" / (stem + ".ast.txt"),
        .mir = build_dir / "dumps" / "mir" / (stem + ".mir.txt"),
        .backend = build_dir / "dumps" / "backend" / (stem + ".backend.txt"),
        .mci = build_dir / "mci" / (stem + ".mci"),
    };
}

BuildArtifactTargets ComputeBuildArtifactTargets(const std::filesystem::path& source_path,
                                                 const std::filesystem::path& build_dir) {
    const auto stem = SanitizeArtifactStem(source_path);
    return {
        .llvm_ir = build_dir / "dumps" / "backend" / (stem + ".ll"),
        .object = build_dir / "obj" / (stem + ".o"),
        .executable = build_dir / "bin" / stem,
        .static_library = build_dir / "lib" / ("lib" + stem + ".a"),
    };
}

BuildArtifactTargets ComputeLogicalBuildArtifactTargets(std::string_view artifact_key,
                                                        const std::filesystem::path& build_dir) {
    const auto stem = SanitizeArtifactStemText(artifact_key);
    return {
        .llvm_ir = build_dir / "dumps" / "backend" / (stem + ".ll"),
        .object = build_dir / "obj" / (stem + ".o"),
        .executable = build_dir / "bin" / stem,
        .static_library = build_dir / "lib" / ("lib" + stem + ".a"),
    };
}

}  // namespace mc::support
