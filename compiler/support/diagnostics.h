#ifndef C_MODERN_COMPILER_SUPPORT_DIAGNOSTICS_H_
#define C_MODERN_COMPILER_SUPPORT_DIAGNOSTICS_H_

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "compiler/support/source_span.h"

namespace mc::support {

enum class DiagnosticSeverity {
    kError,
    kWarning,
    kNote,
};

struct Diagnostic {
    std::filesystem::path file_path;
    SourceSpan span {};
    DiagnosticSeverity severity = DiagnosticSeverity::kError;
    std::string message;
};

std::string FormatDiagnostic(const Diagnostic& diagnostic);
std::string_view ToString(DiagnosticSeverity severity);

class DiagnosticSink {
  public:
    void Report(Diagnostic diagnostic);
    bool HasErrors() const;
    std::string Render() const;
    const std::vector<Diagnostic>& diagnostics() const;

  private:
    std::vector<Diagnostic> diagnostics_;
};

}  // namespace mc::support

#endif  // C_MODERN_COMPILER_SUPPORT_DIAGNOSTICS_H_
