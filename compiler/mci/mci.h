#ifndef C_MODERN_COMPILER_MCI_MCI_H_
#define C_MODERN_COMPILER_MCI_MCI_H_

#include <filesystem>
#include <optional>
#include <string>

#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"

namespace mc::mci {

struct InterfaceArtifact {
    int format_version = 5;
    std::string target_identity;
    std::string module_name;
    std::filesystem::path source_path;
    sema::Module module;
    std::string interface_hash;
};

std::string ComputeInterfaceHash(const InterfaceArtifact& artifact);

bool WriteInterfaceArtifact(const std::filesystem::path& path,
                            const InterfaceArtifact& artifact,
                            support::DiagnosticSink& diagnostics);

std::optional<InterfaceArtifact> LoadInterfaceArtifact(const std::filesystem::path& path,
                                                       support::DiagnosticSink& diagnostics);

bool ValidateInterfaceArtifactMetadata(const InterfaceArtifact& artifact,
                                       const std::filesystem::path& artifact_path,
                                       std::string_view expected_target_identity,
                                       std::string_view expected_module_name,
                                       const std::filesystem::path& expected_source_path,
                                       support::DiagnosticSink& diagnostics);

}  // namespace mc::mci

#endif  // C_MODERN_COMPILER_MCI_MCI_H_