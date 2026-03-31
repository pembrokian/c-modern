#include "compiler/support/diagnostics.h"

#include <sstream>

namespace mc::support {

std::string_view ToString(DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::kError:
            return "error";
        case DiagnosticSeverity::kWarning:
            return "warning";
        case DiagnosticSeverity::kNote:
            return "note";
    }

    return "error";
}

std::string FormatDiagnostic(const Diagnostic& diagnostic) {
    std::ostringstream stream;
    const auto rendered_path = diagnostic.file_path.empty() ? std::string("<unknown>") : diagnostic.file_path.generic_string();

    stream << rendered_path << ':' << diagnostic.span.begin.line << ':' << diagnostic.span.begin.column << ": "
           << ToString(diagnostic.severity) << ": " << diagnostic.message;

    return stream.str();
}

void DiagnosticSink::Report(Diagnostic diagnostic) {
    diagnostics_.push_back(std::move(diagnostic));
}

bool DiagnosticSink::HasErrors() const {
    for (const auto& diagnostic : diagnostics_) {
        if (diagnostic.severity == DiagnosticSeverity::kError) {
            return true;
        }
    }

    return false;
}

std::string DiagnosticSink::Render() const {
    std::ostringstream stream;

    for (std::size_t index = 0; index < diagnostics_.size(); ++index) {
        stream << FormatDiagnostic(diagnostics_[index]);
        if (index + 1 < diagnostics_.size()) {
            stream << '\n';
        }
    }

    return stream.str();
}

const std::vector<Diagnostic>& DiagnosticSink::diagnostics() const {
    return diagnostics_;
}

}  // namespace mc::support
