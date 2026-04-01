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

std::optional<std::size_t> ParseArrayLength(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::size_t value = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = (value * 10) + static_cast<std::size_t>(ch - '0');
    }
    return value;
}

std::optional<std::filesystem::path> ResolveImportPath(const std::filesystem::path& importer_path,
                                                       std::string_view module_name,
                                                       const std::vector<std::filesystem::path>& import_roots,
                                                       DiagnosticSink& diagnostics,
                                                       const SourceSpan& span) {
    std::vector<std::filesystem::path> search_roots;
    search_roots.reserve(import_roots.size() + 1);
    search_roots.push_back(importer_path.parent_path());
    for (const auto& root : import_roots) {
        search_roots.push_back(root);
    }

    for (const auto& root : search_roots) {
        const std::filesystem::path candidate =
            std::filesystem::absolute(root / (std::string(module_name) + ".mc")).lexically_normal();
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    diagnostics.Report({
        .file_path = importer_path,
        .span = span,
        .severity = DiagnosticSeverity::kError,
        .message = "unable to resolve import module: " + std::string(module_name),
    });
    return std::nullopt;
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
