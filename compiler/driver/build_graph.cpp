#include "compiler/driver/internal.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "compiler/codegen_llvm/backend.h"
#include "compiler/support/assert.h"
#include "compiler/support/module_paths.h"

namespace mc::driver {
namespace {

constexpr std::size_t kMergedModulePartLineStride = 1000000;

bool IsInternalModulePath(const std::filesystem::path& path) {
    return mc::support::IsInternalModulePath(path);
}

std::optional<std::string> ResolveDirectSourcePackageIdentity(const std::filesystem::path& source_path,
                                                              const std::vector<std::filesystem::path>& import_roots) {
    std::optional<std::filesystem::path> best_root;
    std::size_t best_depth = 0;
    for (const auto& root : import_roots) {
        if (!IsPathWithinRoot(source_path, root)) {
            continue;
        }
        const auto normalized_root = std::filesystem::absolute(root).lexically_normal();
        const std::size_t depth = static_cast<std::size_t>(std::distance(normalized_root.begin(), normalized_root.end()));
        if (!best_root.has_value() || depth > best_depth) {
            best_root = normalized_root;
            best_depth = depth;
        }
    }

    if (best_root.has_value()) {
        return "direct:" + best_root->generic_string();
    }

    return "direct:" + std::filesystem::absolute(source_path).lexically_normal().parent_path().generic_string();
}

std::optional<std::string> ResolveExternalSourcePackageIdentity(const std::filesystem::path& source_path,
                                                                const std::vector<std::filesystem::path>& import_roots) {
    std::optional<std::filesystem::path> best_root;
    std::size_t best_depth = 0;
    for (const auto& root : import_roots) {
        if (!IsPathWithinRoot(source_path, root)) {
            continue;
        }
        const auto normalized_root = std::filesystem::absolute(root).lexically_normal();
        const std::size_t depth = static_cast<std::size_t>(std::distance(normalized_root.begin(), normalized_root.end()));
        if (!best_root.has_value() || depth > best_depth) {
            best_root = normalized_root;
            best_depth = depth;
        }
    }

    if (best_root.has_value()) {
        return "external:" + best_root->generic_string();
    }

    return "external:" + std::filesystem::absolute(source_path).lexically_normal().parent_path().generic_string();
}

std::string JoinPaths(const std::vector<std::filesystem::path>& paths) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < paths.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << paths[index].generic_string();
    }
    return stream.str();
}

std::string JoinBootstrapTargetChoices(const mc::codegen_llvm::TargetConfig& config) {
    if (config.target_family.empty() || config.target_family == config.triple) {
        return config.triple;
    }
    return config.triple + ", " + config.target_family;
}

void ReportBootstrapTargetNotes(const ProjectFile& project,
                                const ProjectTarget& target,
                                const mc::codegen_llvm::TargetConfig& config,
                                support::DiagnosticSink& diagnostics) {
    diagnostics.Report({
        .file_path = project.path,
        .span = support::kDefaultSourceSpan,
        .severity = support::DiagnosticSeverity::kNote,
        .message = "supported bootstrap targets: " + JoinBootstrapTargetChoices(config),
    });

    std::string target_assignment = "set [targets." + target.name + "] target = \"" + config.triple + "\"";
    if (!config.target_family.empty() && config.target_family != config.triple) {
        target_assignment += " or \"" + config.target_family + "\"";
    }
    target_assignment += " in build.toml";
    diagnostics.Report({
        .file_path = project.path,
        .span = support::kDefaultSourceSpan,
        .severity = support::DiagnosticSeverity::kNote,
        .message = std::move(target_assignment),
    });
}

void OffsetSpan(support::SourceSpan& span, std::size_t line_offset) {
    if (line_offset == 0) {
        return;
    }
    span.begin.line += line_offset;
    span.end.line += line_offset;
}

void OffsetTypeExprSpans(ast::TypeExpr* type_expr, std::size_t line_offset);
void OffsetExprSpans(ast::Expr* expr, std::size_t line_offset);
void OffsetStmtSpans(ast::Stmt* stmt, std::size_t line_offset);

void OffsetNamePatternSpans(ast::NamePattern& pattern, std::size_t line_offset) { OffsetSpan(pattern.span, line_offset); }
void OffsetAttributeArgSpans(ast::AttributeArg& arg, std::size_t line_offset) { OffsetSpan(arg.span, line_offset); }
void OffsetAttributeSpans(ast::Attribute& attribute, std::size_t line_offset) {
    OffsetSpan(attribute.span, line_offset);
    for (auto& arg : attribute.args) {
        OffsetAttributeArgSpans(arg, line_offset);
    }
}
void OffsetFieldDeclSpans(ast::FieldDecl& field_decl, std::size_t line_offset) {
    OffsetSpan(field_decl.span, line_offset);
    OffsetTypeExprSpans(field_decl.type.get(), line_offset);
}
void OffsetEnumVariantDeclSpans(ast::EnumVariantDecl& variant_decl, std::size_t line_offset) {
    OffsetSpan(variant_decl.span, line_offset);
    for (auto& payload_field : variant_decl.payload_fields) {
        OffsetFieldDeclSpans(payload_field, line_offset);
    }
}
void OffsetParamDeclSpans(ast::ParamDecl& param_decl, std::size_t line_offset) {
    OffsetSpan(param_decl.span, line_offset);
    for (auto& attribute : param_decl.attributes) {
        OffsetAttributeSpans(attribute, line_offset);
    }
    OffsetTypeExprSpans(param_decl.type.get(), line_offset);
}
void OffsetFieldInitSpans(ast::FieldInit& field_init, std::size_t line_offset) {
    OffsetSpan(field_init.span, line_offset);
    OffsetExprSpans(field_init.value.get(), line_offset);
}
void OffsetCasePatternSpans(ast::CasePattern& pattern, std::size_t line_offset) {
    OffsetSpan(pattern.span, line_offset);
    OffsetExprSpans(pattern.expr.get(), line_offset);
}
void OffsetSwitchCaseSpans(ast::SwitchCase& switch_case, std::size_t line_offset) {
    OffsetCasePatternSpans(switch_case.pattern, line_offset);
    for (auto& stmt : switch_case.statements) {
        OffsetStmtSpans(stmt.get(), line_offset);
    }
}
void OffsetDefaultCaseSpans(ast::DefaultCase& default_case, std::size_t line_offset) {
    OffsetSpan(default_case.span, line_offset);
    for (auto& stmt : default_case.statements) {
        OffsetStmtSpans(stmt.get(), line_offset);
    }
}
void OffsetTypeExprSpans(ast::TypeExpr* type_expr, std::size_t line_offset) {
    if (type_expr == nullptr) {
        return;
    }
    OffsetSpan(type_expr->span, line_offset);
    for (auto& type_arg : type_expr->type_args) {
        OffsetTypeExprSpans(type_arg.get(), line_offset);
    }
    OffsetTypeExprSpans(type_expr->inner.get(), line_offset);
    OffsetExprSpans(type_expr->length_expr.get(), line_offset);
    for (auto& param : type_expr->params) {
        OffsetTypeExprSpans(param.get(), line_offset);
    }
    for (auto& ret : type_expr->returns) {
        OffsetTypeExprSpans(ret.get(), line_offset);
    }
}
void OffsetExprSpans(ast::Expr* expr, std::size_t line_offset) {
    if (expr == nullptr) {
        return;
    }
    OffsetSpan(expr->span, line_offset);
    for (auto& type_arg : expr->type_args) {
        OffsetTypeExprSpans(type_arg.get(), line_offset);
    }
    OffsetTypeExprSpans(expr->type_target.get(), line_offset);
    for (auto& arg : expr->args) {
        OffsetExprSpans(arg.get(), line_offset);
    }
    for (auto& field_init : expr->field_inits) {
        OffsetFieldInitSpans(field_init, line_offset);
    }
    OffsetExprSpans(expr->left.get(), line_offset);
    OffsetExprSpans(expr->right.get(), line_offset);
    OffsetExprSpans(expr->extra.get(), line_offset);
}
void OffsetStmtSpans(ast::Stmt* stmt, std::size_t line_offset) {
    if (stmt == nullptr) {
        return;
    }
    OffsetSpan(stmt->span, line_offset);
    for (auto& nested_stmt : stmt->statements) {
        OffsetStmtSpans(nested_stmt.get(), line_offset);
    }
    OffsetNamePatternSpans(stmt->pattern, line_offset);
    OffsetTypeExprSpans(stmt->type_ann.get(), line_offset);
    for (auto& expr : stmt->exprs) {
        OffsetExprSpans(expr.get(), line_offset);
    }
    for (auto& target : stmt->assign_targets) {
        OffsetExprSpans(target.get(), line_offset);
    }
    for (auto& value : stmt->assign_values) {
        OffsetExprSpans(value.get(), line_offset);
    }
    for (auto& switch_case : stmt->switch_cases) {
        OffsetSwitchCaseSpans(switch_case, line_offset);
    }
    if (stmt->has_default_case) {
        OffsetDefaultCaseSpans(stmt->default_case, line_offset);
    }
    OffsetStmtSpans(stmt->then_branch.get(), line_offset);
    OffsetStmtSpans(stmt->else_branch.get(), line_offset);
}
void OffsetDeclSpans(ast::Decl& decl, std::size_t line_offset) {
    OffsetSpan(decl.span, line_offset);
    for (auto& attribute : decl.attributes) {
        OffsetAttributeSpans(attribute, line_offset);
    }
    for (auto& param : decl.params) {
        OffsetParamDeclSpans(param, line_offset);
    }
    for (auto& return_type : decl.return_types) {
        OffsetTypeExprSpans(return_type.get(), line_offset);
    }
    OffsetStmtSpans(decl.body.get(), line_offset);
    for (auto& field : decl.fields) {
        OffsetFieldDeclSpans(field, line_offset);
    }
    for (auto& variant : decl.variants) {
        OffsetEnumVariantDeclSpans(variant, line_offset);
    }
    OffsetNamePatternSpans(decl.pattern, line_offset);
    OffsetTypeExprSpans(decl.type_ann.get(), line_offset);
    for (auto& value : decl.values) {
        OffsetExprSpans(value.get(), line_offset);
    }
    OffsetTypeExprSpans(decl.aliased_type.get(), line_offset);
}
void OffsetSourceFileSpans(ast::SourceFile& source_file, std::size_t line_offset) {
    OffsetSpan(source_file.span, line_offset);
    for (auto& import_decl : source_file.imports) {
        OffsetSpan(import_decl.span, line_offset);
    }
    for (auto& decl : source_file.decls) {
        OffsetDeclSpans(decl, line_offset);
    }
}

std::string MakeModuleArtifactKey(std::string_view package_identity,
                                  std::string_view module_name) {
    if (package_identity.empty()) {
        return std::string(module_name);
    }
    return std::string(package_identity) + "::" + std::string(module_name);
}

struct ResolvedModuleSource {
    std::string module_name;
    std::string artifact_key;
    std::filesystem::path primary_source_path;
    std::vector<std::filesystem::path> source_paths;
    std::optional<std::string> package_identity;
    ast::SourceFile::ModuleKind module_kind = ast::SourceFile::ModuleKind::kOrdinary;
};

std::optional<ResolvedModuleSource> ResolveImportPathWithPackageRules(
    const std::filesystem::path& importer_path,
    std::string_view module_name,
    const std::vector<std::filesystem::path>& search_roots,
    const std::optional<std::string>& importer_package_identity,
    const std::function<std::optional<std::string>(const std::filesystem::path&)>& package_identity_for_source,
    const std::function<std::vector<CompileImport>(std::string_view)>& known_module_sources,
    support::DiagnosticSink& diagnostics,
    const support::SourceSpan& span) {
    std::vector<ResolvedModuleSource> matches;
    std::unordered_set<std::string> seen_matches;
    std::optional<std::string> rejected_internal_reason;

    const auto append_match = [&](ResolvedModuleSource match) {
        const std::string key = match.primary_source_path.generic_string();
        if (!seen_matches.insert(key).second) {
            return;
        }

        if (match.module_kind == ast::SourceFile::ModuleKind::kInternal) {
            if (!importer_package_identity.has_value()) {
                rejected_internal_reason =
                    "direct source mode does not support importing internal module: " + std::string(module_name);
                return;
            }

            if (!match.package_identity.has_value()) {
                rejected_internal_reason =
                    "unable to determine package identity for internal module: " + match.primary_source_path.generic_string();
                return;
            }
            if (*match.package_identity != *importer_package_identity) {
                rejected_internal_reason = "module " + std::string(module_name) + " is internal to package '" +
                    *match.package_identity + "' and cannot be imported from package '" + *importer_package_identity + "'";
                return;
            }
        }

        matches.push_back(std::move(match));
    };

    if (known_module_sources) {
        for (auto& known_match : known_module_sources(module_name)) {
            append_match({
                .module_name = known_match.module_name,
                .artifact_key = known_match.artifact_key,
                .primary_source_path = known_match.source_path,
                .source_paths = known_match.source_paths,
                .package_identity = known_match.package_identity.empty() ? std::optional<std::string> {}
                                                                        : std::optional<std::string> {known_match.package_identity},
                .module_kind = known_match.module_kind,
            });
        }
    }

    for (const auto& root : search_roots) {
        const std::filesystem::path candidate = std::filesystem::absolute(root / (std::string(module_name) + ".mc")).lexically_normal();
        if (!std::filesystem::exists(candidate)) {
            continue;
        }

        const auto candidate_package_identity = package_identity_for_source ? package_identity_for_source(candidate) : std::nullopt;
        append_match({
            .module_name = std::string(module_name),
            .artifact_key = candidate.generic_string(),
            .primary_source_path = candidate,
            .source_paths = {candidate},
            .package_identity = candidate_package_identity,
            .module_kind = IsInternalModulePath(candidate) ? ast::SourceFile::ModuleKind::kInternal
                                                           : ast::SourceFile::ModuleKind::kOrdinary,
        });
    }

    if (matches.size() == 1) {
        return matches.front();
    }
    if (matches.empty()) {
        diagnostics.Report({
            .file_path = importer_path,
            .span = span,
            .severity = support::DiagnosticSeverity::kError,
            .message = rejected_internal_reason.has_value()
                ? *rejected_internal_reason
                : "unable to resolve import module: " + std::string(module_name),
        });
        return std::nullopt;
    }

    diagnostics.Report({
        .file_path = importer_path,
        .span = span,
        .severity = support::DiagnosticSeverity::kError,
        .message = [&]() {
            std::vector<std::filesystem::path> match_paths;
            match_paths.reserve(matches.size());
            for (const auto& match : matches) {
                match_paths.push_back(match.primary_source_path);
            }
            return "ambiguous import module '" + std::string(module_name) + "' matched multiple roots: " + JoinPaths(match_paths);
        }(),
    });
    return std::nullopt;
}

std::vector<std::filesystem::path> CanonicalizeModuleSourcePaths(const std::vector<std::filesystem::path>& source_paths) {
    std::vector<std::filesystem::path> normalized;
    normalized.reserve(source_paths.size());
    std::unordered_set<std::string> seen;
    for (const auto& source_path : source_paths) {
        const auto path = std::filesystem::absolute(source_path).lexically_normal();
        if (seen.insert(path.generic_string()).second) {
            normalized.push_back(path);
        }
    }
    return normalized;
}

mc::parse::ParseResult MergeParsedModuleSources(std::vector<mc::parse::ParseResult> parsed_parts) {
    mc::parse::ParseResult merged;
    merged.ok = true;
    merged.source_file = std::make_unique<mc::ast::SourceFile>();
    std::unordered_set<std::string> seen_imports;
    bool module_kind_initialized = false;
    for (auto& part : parsed_parts) {
        if (part.source_file == nullptr) {
            merged.ok = false;
            return merged;
        }
        if (!module_kind_initialized) {
            merged.source_file->module_kind = part.source_file->module_kind;
            merged.source_file->span = part.source_file->span;
            module_kind_initialized = true;
        }
        for (const auto& import_decl : part.source_file->imports) {
            if (!seen_imports.insert(import_decl.module_name).second) {
                continue;
            }
            merged.source_file->imports.push_back(import_decl);
        }
        for (auto& decl : part.source_file->decls) {
            merged.source_file->decls.push_back(std::move(decl));
        }
    }
    std::sort(merged.source_file->imports.begin(), merged.source_file->imports.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.module_name < rhs.module_name;
    });
    return merged;
}

std::optional<mc::parse::ParseResult> ParseLogicalModuleSources(const std::vector<std::filesystem::path>& source_paths,
                                                                support::DiagnosticSink& diagnostics) {
    std::vector<mc::parse::ParseResult> parsed_parts;
    parsed_parts.reserve(source_paths.size());
    ast::SourceFile::ModuleKind expected_module_kind = ast::SourceFile::ModuleKind::kOrdinary;
    bool module_kind_initialized = false;
    std::size_t part_index = 0;

    for (const auto& source_path : CanonicalizeModuleSourcePaths(source_paths)) {
        auto parsed = ParseSourcePath(source_path, diagnostics);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        if (!module_kind_initialized) {
            expected_module_kind = parsed->source_file->module_kind;
            module_kind_initialized = true;
        } else if (parsed->source_file->module_kind != expected_module_kind) {
            diagnostics.Report({
                .file_path = source_path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "logical module source set mixes ordinary and internal module files",
            });
            return std::nullopt;
        }
        OffsetSourceFileSpans(*parsed->source_file, part_index * kMergedModulePartLineStride);
        parsed_parts.push_back(std::move(*parsed));
        ++part_index;
    }

    return MergeParsedModuleSources(std::move(parsed_parts));
}

ResolvedModuleSource BuildResolvedModuleSource(std::string_view module_name,
                                               std::optional<std::string> package_identity,
                                               std::vector<std::filesystem::path> source_paths,
                                               ast::SourceFile::ModuleKind module_kind,
                                               std::optional<std::filesystem::path> preferred_primary_source = std::nullopt) {
    ResolvedModuleSource resolved;
    resolved.module_name = std::string(module_name);
    resolved.package_identity = std::move(package_identity);
    resolved.source_paths = CanonicalizeModuleSourcePaths(source_paths);
    resolved.module_kind = module_kind;
    resolved.artifact_key = MakeModuleArtifactKey(resolved.package_identity.value_or(std::string {}), module_name);
    resolved.primary_source_path = preferred_primary_source.has_value()
        ? std::filesystem::absolute(*preferred_primary_source).lexically_normal()
        : resolved.source_paths.front();
    return resolved;
}

mc::codegen_llvm::TargetConfig ResolveProjectTargetConfig(const ProjectFile& project,
                                                          const ProjectTarget& target,
                                                          support::DiagnosticSink& diagnostics) {
    mc::codegen_llvm::TargetConfig target_config = mc::codegen_llvm::BootstrapTargetConfig();
    target_config.hosted = target.env == "hosted";
    if (!target.target.empty() && target.target == target_config.triple) {
        target_config.triple = target.target;
    }
    if (!target.target.empty() && target.target != target_config.triple && target.target != target_config.target_family) {
        diagnostics.Report({
            .file_path = project.path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unsupported target triple for bootstrap codegen: " + target.target,
        });
    }
    return target_config;
}

void CollectOwnedSourcePathsForLink(const TargetBuildGraph& graph,
                                    std::string_view owner_name,
                                    std::unordered_set<std::string>& owned_paths,
                                    std::unordered_map<std::string, std::string>& owners,
                                    const ProjectTarget& consumer_target,
                                    const ProjectFile& project,
                                    support::DiagnosticSink& diagnostics) {
    for (const auto& node : graph.compile_graph.nodes) {
        const std::string key = node.source_path.generic_string();
        const auto [it, inserted] = owners.emplace(key, std::string(owner_name));
        if (!inserted && it->second != owner_name) {
            diagnostics.Report({
                .file_path = project.path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "target '" + consumer_target.name + "' links static libraries '" + it->second +
                           "' and '" + std::string(owner_name) + "' that both provide module source: " + key,
            });
            continue;
        }
        owned_paths.insert(key);
    }

    for (const auto& linked_graph : graph.linked_targets) {
        CollectOwnedSourcePathsForLink(linked_graph,
                                       owner_name,
                                       owned_paths,
                                       owners,
                                       consumer_target,
                                       project,
                                       diagnostics);
    }
}

std::vector<CompileImport> LookupKnownProjectModuleSources(const ProjectTarget& target,
                                                           const std::vector<TargetBuildGraph>& linked_targets,
                                                           std::string_view module_name) {
    std::vector<CompileImport> matches;
    if (const auto* module_set = LookupProjectModuleSet(target, module_name); module_set != nullptr) {
        const auto primary_source = std::find(module_set->files.begin(), module_set->files.end(), target.root) != module_set->files.end()
            ? std::optional<std::filesystem::path>(target.root)
            : std::optional<std::filesystem::path>(module_set->files.front());
        const auto resolved = BuildResolvedModuleSource(module_set->module_name,
                                                        ResolveTargetPackageIdentity(target, *primary_source),
                                                        module_set->files,
                                                        ast::SourceFile::ModuleKind::kOrdinary,
                                                        primary_source);
        matches.push_back({
            .module_name = resolved.module_name,
            .artifact_key = resolved.artifact_key,
            .package_identity = resolved.package_identity.value_or(std::string {}),
            .source_path = resolved.primary_source_path,
            .source_paths = resolved.source_paths,
            .module_kind = resolved.module_kind,
        });
    }

    for (const auto& linked_graph : linked_targets) {
        for (const auto& node : linked_graph.compile_graph.nodes) {
            if (node.module_name != module_name) {
                continue;
            }
            matches.push_back({
                .module_name = node.module_name,
                .artifact_key = node.artifact_key,
                .package_identity = node.package_identity,
                .source_path = node.source_path,
                .source_paths = node.source_paths,
                .module_kind = node.parse_result.source_file != nullptr ? node.parse_result.source_file->module_kind
                                                                        : ast::SourceFile::ModuleKind::kOrdinary,
            });
        }
        auto nested = LookupKnownProjectModuleSources(linked_graph.target, linked_graph.linked_targets, module_name);
        matches.insert(matches.end(), nested.begin(), nested.end());
    }
    return matches;
}

bool DiscoverModuleGraphRecursive(const ResolvedModuleSource& module_source,
                                  const std::vector<std::filesystem::path>& import_roots,
                                  const std::function<std::optional<std::string>(const std::filesystem::path&)>& package_identity_for_source,
                                  const std::function<std::vector<CompileImport>(std::string_view)>& known_module_sources,
                                  const std::unordered_set<std::string>& external_module_paths,
                                  support::DiagnosticSink& diagnostics,
                                  std::unordered_set<std::string>& visiting,
                                  std::unordered_set<std::string>& visited,
                                  std::vector<CompileNode>& nodes) {
    const std::filesystem::path normalized_path = std::filesystem::absolute(module_source.primary_source_path).lexically_normal();
    const std::string path_key = module_source.artifact_key;
    if (visited.contains(path_key)) {
        return true;
    }
    if (!visiting.insert(path_key).second) {
        diagnostics.Report({
            .file_path = normalized_path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "import cycle detected involving module: " + module_source.module_name,
        });
        return false;
    }

    auto parse_result = ParseLogicalModuleSources(module_source.source_paths, diagnostics);
    if (!parse_result.has_value()) {
        visiting.erase(path_key);
        return false;
    }

    const auto current_package_identity = module_source.package_identity.has_value()
        ? module_source.package_identity
        : (package_identity_for_source ? package_identity_for_source(normalized_path) : std::nullopt);
    if (module_source.module_kind == ast::SourceFile::ModuleKind::kInternal && !current_package_identity.has_value()) {
        diagnostics.Report({
            .file_path = normalized_path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "direct source mode does not support internal module roots",
        });
        visiting.erase(path_key);
        return false;
    }

    CompileNode node {
        .module_name = module_source.module_name,
        .artifact_key = module_source.artifact_key,
        .package_identity = current_package_identity.value_or(std::string {}),
        .source_path = normalized_path,
        .source_paths = module_source.source_paths,
        .parse_result = std::move(*parse_result),
    };

    for (const auto& import_decl : node.parse_result.source_file->imports) {
        const auto import_path = ResolveImportPathWithPackageRules(normalized_path,
                                                                   import_decl.module_name,
                                                                   import_roots,
                                                                   current_package_identity,
                                                                   package_identity_for_source,
                                                                   known_module_sources,
                                                                   diagnostics,
                                                                   import_decl.span);
        if (!import_path.has_value()) {
            visiting.erase(path_key);
            return false;
        }
        node.imports.push_back({
            .module_name = import_decl.module_name,
            .artifact_key = import_path->artifact_key,
            .package_identity = import_path->package_identity.value_or(std::string {}),
            .source_path = import_path->primary_source_path,
            .source_paths = import_path->source_paths,
            .module_kind = import_path->module_kind,
        });
        const std::string import_key = std::filesystem::absolute(import_path->primary_source_path).lexically_normal().generic_string();
        if (external_module_paths.contains(import_key)) {
            continue;
        }
        if (!DiscoverModuleGraphRecursive(*import_path,
                                          import_roots,
                                          package_identity_for_source,
                                          known_module_sources,
                                          external_module_paths,
                                          diagnostics,
                                          visiting,
                                          visited,
                                          nodes)) {
            visiting.erase(path_key);
            return false;
        }
    }

    visited.insert(path_key);
    visiting.erase(path_key);
    nodes.push_back(std::move(node));
    return true;
}

}  // namespace

std::optional<ProjectFile> LoadSelectedProject(const CommandOptions& options,
                                               support::DiagnosticSink& diagnostics) {
    const std::filesystem::path project_path = options.project_path.has_value()
                                                   ? std::filesystem::absolute(*options.project_path).lexically_normal()
                                                   : std::filesystem::absolute(DefaultProjectPath()).lexically_normal();
    return LoadProjectFile(project_path, diagnostics);
}

bool SupportsBootstrapTarget(const ProjectTarget& target,
                             const ProjectFile& project,
                             support::DiagnosticSink& diagnostics) {
    if (target.target.empty()) {
        if (target.env == "freestanding") {
            const auto config = mc::codegen_llvm::BootstrapTargetConfig();
            diagnostics.Report({
                .file_path = project.path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "target '" + target.name + "' must declare an explicit freestanding target",
            });
            ReportBootstrapTargetNotes(project, target, config, diagnostics);
            return false;
        }
        return true;
    }

    const auto config = mc::codegen_llvm::BootstrapTargetConfig();
    if (target.target == config.triple || target.target == config.target_family) {
        return true;
    }

    diagnostics.Report({
        .file_path = project.path,
        .span = support::kDefaultSourceSpan,
        .severity = support::DiagnosticSeverity::kError,
        .message = "target '" + target.name + "' requests unsupported bootstrap target: " + target.target,
    });
    ReportBootstrapTargetNotes(project, target, config, diagnostics);
    return false;
}

std::optional<CompileGraph> DiscoverModuleGraph(const std::filesystem::path& entry_source_path,
                                                const std::vector<std::filesystem::path>& import_roots,
                                                const std::function<std::optional<std::string>(const std::filesystem::path&)>& package_identity_for_source,
                                                const std::function<std::vector<CompileImport>(std::string_view)>& known_module_sources,
                                                const std::unordered_set<std::string>& external_module_paths,
                                                const std::filesystem::path& build_dir,
                                                const mc::codegen_llvm::TargetConfig& target_config,
                                                std::string mode,
                                                std::string env,
                                                bool wrap_entry_main,
                                                bool namespace_entry_module,
                                                support::DiagnosticSink& diagnostics) {
    const std::filesystem::path normalized_entry =
        std::filesystem::absolute(entry_source_path).lexically_normal();
    ResolvedModuleSource entry_module {
        .module_name = normalized_entry.stem().string(),
        .artifact_key = normalized_entry.generic_string(),
        .primary_source_path = normalized_entry,
        .source_paths = {normalized_entry},
        .package_identity = package_identity_for_source ? package_identity_for_source(normalized_entry) : std::nullopt,
        .module_kind = IsInternalModulePath(normalized_entry) ? ast::SourceFile::ModuleKind::kInternal
                                                              : ast::SourceFile::ModuleKind::kOrdinary,
    };
    if (known_module_sources) {
        const auto known_entry_matches = known_module_sources(normalized_entry.stem().string());
        for (const auto& known_entry : known_entry_matches) {
            const bool owns_entry = std::any_of(known_entry.source_paths.begin(), known_entry.source_paths.end(), [&](const auto& path) {
                return std::filesystem::absolute(path).lexically_normal() == normalized_entry;
            });
            if (owns_entry) {
                entry_module = {
                    .module_name = known_entry.module_name,
                    .artifact_key = known_entry.artifact_key,
                    .primary_source_path = known_entry.source_path,
                    .source_paths = known_entry.source_paths,
                    .package_identity = known_entry.package_identity.empty() ? std::optional<std::string> {}
                                                                            : std::optional<std::string> {known_entry.package_identity},
                    .module_kind = known_entry.module_kind,
                };
                break;
            }
        }
    }
    CompileGraph graph {
        .entry_source_path = entry_module.primary_source_path,
        .import_roots = import_roots,
        .build_dir = build_dir,
        .mode = std::move(mode),
        .env = std::move(env),
        .wrap_entry_main = wrap_entry_main,
        .namespace_entry_module = namespace_entry_module,
        .target_config = target_config,
    };
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    if (!DiscoverModuleGraphRecursive(entry_module,
                                      import_roots,
                                      package_identity_for_source,
                                      known_module_sources,
                                      external_module_paths,
                                      diagnostics,
                                      visiting,
                                      visited,
                                      graph.nodes)) {
        return std::nullopt;
    }
    return graph;
}

void AssertCompileGraphTopologicalOrder(const CompileGraph& graph) {
#ifdef NDEBUG
    (void)graph;
#else
    std::unordered_map<std::string, std::size_t> node_indices;
    node_indices.reserve(graph.nodes.size());
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        node_indices.emplace(graph.nodes[index].artifact_key, index);
    }

    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        for (const auto& import : graph.nodes[index].imports) {
            const auto found = node_indices.find(import.artifact_key);
            if (found == node_indices.end()) {
                continue;
            }
            (void)import;
            assert(found->second < index && "compile graph nodes must be topologically ordered");
        }
    }
#endif
}

std::optional<TargetBuildGraph> BuildResolvedTargetGraph(const ProjectFile& project,
                                                         const ProjectTarget& target,
                                                         const std::vector<std::filesystem::path>& cli_import_roots,
                                                         support::DiagnosticSink& diagnostics,
                                                         std::unordered_set<std::string>& visiting_targets) {
    if (!SupportsBootstrapTarget(target, project, diagnostics)) {
        return std::nullopt;
    }

    if (!visiting_targets.insert(target.name).second) {
        diagnostics.Report({
            .file_path = project.path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "target dependency cycle detected involving '" + target.name + "'",
        });
        return std::nullopt;
    }

    std::vector<TargetBuildGraph> linked_targets;
    linked_targets.reserve(target.links.size());
    for (const auto& link_name : target.links) {
        auto linked_target_it = project.targets.find(link_name);
        if (linked_target_it == project.targets.end()) {
            diagnostics.Report({
                .file_path = project.path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "target '" + target.name + "' links unknown target '" + link_name + "'",
            });
            continue;
        }

        ProjectTarget linked_target = linked_target_it->second;
        linked_target.mode = target.mode;
        linked_target.env = target.env;
        if (!IsStaticLibraryTargetKind(linked_target.kind)) {
            diagnostics.Report({
                .file_path = project.path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "target '" + target.name + "' can only link static libraries, but '" + linked_target.name +
                           "' has kind '" + linked_target.kind + "'",
            });
            continue;
        }

        auto linked_graph = BuildResolvedTargetGraph(project, linked_target, cli_import_roots, diagnostics, visiting_targets);
        if (!linked_graph.has_value()) {
            visiting_targets.erase(target.name);
            return std::nullopt;
        }
        linked_targets.push_back(std::move(*linked_graph));
    }
    visiting_targets.erase(target.name);

    if (diagnostics.HasErrors()) {
        return std::nullopt;
    }

    std::unordered_set<std::string> external_module_paths;
    std::unordered_map<std::string, std::string> external_owners;
    for (const auto& linked_graph : linked_targets) {
        CollectOwnedSourcePathsForLink(linked_graph,
                                       linked_graph.target.name,
                                       external_module_paths,
                                       external_owners,
                                       target,
                                       project,
                                       diagnostics);
    }
    if (diagnostics.HasErrors()) {
        return std::nullopt;
    }

    const auto import_roots = ComputeProjectImportRoots(project, target, cli_import_roots);
    auto compile_graph = DiscoverModuleGraph(target.root,
                                             import_roots,
                                             [&](const std::filesystem::path& path) -> std::optional<std::string> {
                                                 if (IsPathWithinRoot(path, project.root_dir)) {
                                                     return ResolveTargetPackageIdentity(target, path);
                                                 }
                                                 return ResolveExternalSourcePackageIdentity(path, import_roots);
                                             },
                                             [&](std::string_view module_name) {
                                                 return LookupKnownProjectModuleSources(target, linked_targets, module_name);
                                             },
                                             external_module_paths,
                                             project.build_dir,
                                             ResolveProjectTargetConfig(project, target, diagnostics),
                                             target.mode,
                                             target.env,
                                             IsExecutableTargetKind(target.kind) && target.env == "hosted",
                                             !IsExecutableTargetKind(target.kind),
                                             diagnostics);
    if (!compile_graph.has_value()) {
        return std::nullopt;
    }

    return TargetBuildGraph {
        .project = project,
        .target = target,
        .compile_graph = std::move(*compile_graph),
        .linked_targets = std::move(linked_targets),
    };
}

std::optional<TargetBuildGraph> BuildTargetGraph(const CommandOptions& options,
                                                 support::DiagnosticSink& diagnostics) {
    auto project = LoadSelectedProject(options, diagnostics);
    if (!project.has_value()) {
        return std::nullopt;
    }

    const ProjectTarget* target = SelectProjectTarget(*project, options.target_name, diagnostics);
    if (target == nullptr || diagnostics.HasErrors()) {
        return std::nullopt;
    }
    const std::string mode = options.mode_override.value_or(target->mode);
    const std::string env = options.env_override.value_or(target->env);

    if (!IsSupportedMode(mode)) {
        diagnostics.Report({
            .file_path = project->path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unsupported build mode for target build: " + mode,
        });
        return std::nullopt;
    }
    if (!IsSupportedEnv(env)) {
        diagnostics.Report({
            .file_path = project->path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unsupported environment for target build: " + env,
        });
        return std::nullopt;
    }

    const std::filesystem::path build_dir = options.build_dir_explicit
        ? std::filesystem::absolute(options.build_dir).lexically_normal()
        : std::filesystem::absolute(project->build_dir / mode).lexically_normal();

    ProjectFile resolved_project = *project;
    resolved_project.build_dir = build_dir;
    ProjectTarget resolved_target = *target;
    resolved_target.mode = mode;
    resolved_target.env = env;
    std::unordered_set<std::string> visiting_targets;
    return BuildResolvedTargetGraph(resolved_project,
                                    resolved_target,
                                    options.import_roots,
                                    diagnostics,
                                    visiting_targets);
}

}  // namespace mc::driver