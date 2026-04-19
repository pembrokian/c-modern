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

std::string HexU64(std::uint64_t value) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(16, '0');
    for (int index = 15; index >= 0; --index) {
        text[static_cast<std::size_t>(index)] = kDigits[value & 0xfu];
        value >>= 4u;
    }
    return text;
}

std::string HashArtifactKey(std::string_view text) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char byte : text) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ull;
    }
    return HexU64(hash);
}

}  // namespace

std::string SanitizeArtifactStem(const std::filesystem::path& source_path) {
    return SanitizeArtifactStemText(source_path.lexically_normal().generic_string());
}

std::string SanitizeLogicalArtifactStem(std::string_view artifact_key) {
    std::string stem = SanitizeArtifactStemText(artifact_key);
    stem += ".";
    stem += HashArtifactKey(artifact_key);
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

DumpTargets ComputeLogicalDumpTargets(std::string_view artifact_key,
                                      const std::filesystem::path& build_dir) {
    const auto stem = SanitizeLogicalArtifactStem(artifact_key);
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
    const auto stem = SanitizeLogicalArtifactStem(artifact_key);
    return {
        .llvm_ir = build_dir / "dumps" / "backend" / (stem + ".ll"),
        .object = build_dir / "obj" / (stem + ".o"),
        .executable = build_dir / "bin" / stem,
        .static_library = build_dir / "lib" / ("lib" + stem + ".a"),
    };
}

}  // namespace mc::support
