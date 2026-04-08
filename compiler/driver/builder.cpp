#include "compiler/driver/internal.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "compiler/codegen_llvm/backend.h"
#include "compiler/lex/lexer.h"
#include "compiler/mci/mci.h"
#include "compiler/support/assert.h"
#include "compiler/support/dump_paths.h"
#include "compiler/support/source_manager.h"

namespace mc::driver {
namespace {

constexpr std::string_view kHostedRuntimeSupportRelativePath = "runtime/hosted/mc_hosted_runtime.c";
constexpr std::string_view kFreestandingRuntimeDirectoryRelativePath = "runtime/freestanding";

bool IsInternalModulePath(const std::filesystem::path& path) {
    return path.filename() == "internal.mc";
}

bool IsPathWithinRoot(const std::filesystem::path& path,
                      const std::filesystem::path& root) {
    const auto normalized_path = std::filesystem::absolute(path).lexically_normal();
    const auto normalized_root = std::filesystem::absolute(root).lexically_normal();

    auto root_it = normalized_root.begin();
    auto path_it = normalized_path.begin();
    for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }
    return true;
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

struct ResolvedImportPath {
    std::filesystem::path path;
    std::optional<std::string> package_identity;
};

std::optional<ResolvedImportPath> ResolveImportPathWithPackageRules(
    const std::filesystem::path& importer_path,
    std::string_view module_name,
    const std::vector<std::filesystem::path>& search_roots,
    const std::optional<std::string>& importer_package_identity,
    const std::function<std::optional<std::string>(const std::filesystem::path&)>& package_identity_for_source,
    support::DiagnosticSink& diagnostics,
    const support::SourceSpan& span) {
    std::vector<ResolvedImportPath> matches;
    std::unordered_set<std::string> seen_matches;
    std::optional<std::string> rejected_internal_reason;

    for (const auto& root : search_roots) {
        const std::filesystem::path candidate = std::filesystem::absolute(root / (std::string(module_name) + ".mc")).lexically_normal();
        if (!std::filesystem::exists(candidate)) {
            continue;
        }
        if (!seen_matches.insert(candidate.generic_string()).second) {
            continue;
        }

        const auto candidate_package_identity = package_identity_for_source ? package_identity_for_source(candidate) : std::nullopt;
        if (IsInternalModulePath(candidate)) {
            if (!importer_package_identity.has_value()) {
                rejected_internal_reason =
                    "direct source mode does not support importing internal module: " + std::string(module_name);
                continue;
            }

            if (!candidate_package_identity.has_value()) {
                rejected_internal_reason = "unable to determine package identity for internal module: " + candidate.generic_string();
                continue;
            }
            if (*candidate_package_identity != *importer_package_identity) {
                rejected_internal_reason = "module " + std::string(module_name) + " is internal to package '" +
                    *candidate_package_identity + "' and cannot be imported from package '" + *importer_package_identity + "'";
                continue;
            }
        }

        matches.push_back({
            .path = candidate,
            .package_identity = candidate_package_identity,
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
                match_paths.push_back(match.path);
            }
            return "ambiguous import module '" + std::string(module_name) + "' matched multiple roots: " + JoinPaths(match_paths);
        }(),
    });
    return std::nullopt;
}

std::optional<std::filesystem::path> DiscoverRepositoryRoot(const std::filesystem::path& start_path) {
    std::filesystem::path current = std::filesystem::absolute(start_path).lexically_normal();
    if (!std::filesystem::is_directory(current)) {
        current = current.parent_path();
    }

    while (!current.empty()) {
        if (std::filesystem::exists(current / "stdlib") && std::filesystem::exists(current / "runtime")) {
            return current;
        }
        if (current == current.root_path()) {
            break;
        }
        current = current.parent_path();
    }

    return std::nullopt;
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

mc::mci::InterfaceArtifact MakeModuleInterfaceArtifact(const std::filesystem::path& source_path,
                                                       const ast::SourceFile& source_file,
                                                       const sema::Module& checked_module,
                                                       std::string target_identity) {
    return {
        .target_identity = std::move(target_identity),
        .package_identity = checked_module.package_identity,
        .module_name = source_path.stem().string(),
        .module_kind = source_file.module_kind,
        .source_path = source_path,
        .module = sema::BuildImportVisibleModuleSurface(checked_module, source_file),
    };
}

bool WriteModuleInterface(const std::filesystem::path& source_path,
                          const mc::mci::InterfaceArtifact& artifact,
                          const std::filesystem::path& build_dir,
                          support::DiagnosticSink& diagnostics) {
    const auto dump_targets = support::ComputeDumpTargets(source_path, build_dir);
    return mc::mci::WriteInterfaceArtifact(dump_targets.mci, artifact, diagnostics);
}

std::optional<ImportedInterfaceData> LoadImportedInterfaces(
    const CompileNode& node,
    std::string_view target_identity,
    const std::filesystem::path& build_dir,
    support::DiagnosticSink& diagnostics) {
    ImportedInterfaceData imported;
    for (const auto& import : node.imports) {
        const auto& import_name = import.module_name;
        const auto& import_path = import.source_path;
        const auto dump_targets = support::ComputeDumpTargets(import_path, build_dir);
        const auto artifact = mc::mci::LoadInterfaceArtifact(dump_targets.mci, diagnostics);
        if (!artifact.has_value()) {
            return std::nullopt;
        }
        if (!mc::mci::ValidateInterfaceArtifactMetadata(*artifact,
                                                        dump_targets.mci,
                                                        target_identity,
                                                        import.package_identity,
                                                        import_path.stem().string(),
                                                        import.module_kind,
                                                        import_path,
                                                        diagnostics)) {
            return std::nullopt;
        }
        imported.modules.emplace(import_name, mc::sema::RewriteImportedModuleSurfaceTypes(artifact->module, import_name));
        imported.interface_hashes.push_back({import_name, artifact->interface_hash});
    }
    return imported;
}

std::string QualifyImportedSymbol(std::string_view module_name,
                                  std::string_view symbol_name) {
    return std::string(module_name) + "." + std::string(symbol_name);
}

mc::mir::TypeDecl ConvertImportedTypeDecl(const mc::sema::TypeDeclSummary& type_decl) {
    mc::mir::TypeDecl mir_type_decl;
    switch (type_decl.kind) {
        case mc::ast::Decl::Kind::kStruct:
            mir_type_decl.kind = mc::mir::TypeDecl::Kind::kStruct;
            break;
        case mc::ast::Decl::Kind::kEnum:
            mir_type_decl.kind = mc::mir::TypeDecl::Kind::kEnum;
            break;
        case mc::ast::Decl::Kind::kDistinct:
            mir_type_decl.kind = mc::mir::TypeDecl::Kind::kDistinct;
            break;
        case mc::ast::Decl::Kind::kTypeAlias:
            mir_type_decl.kind = mc::mir::TypeDecl::Kind::kAlias;
            break;
        case mc::ast::Decl::Kind::kFunc:
        case mc::ast::Decl::Kind::kExternFunc:
        case mc::ast::Decl::Kind::kConst:
        case mc::ast::Decl::Kind::kVar:
            break;
    }
    mir_type_decl.name = type_decl.name;
    mir_type_decl.type_params = type_decl.type_params;
    mir_type_decl.is_packed = type_decl.is_packed;
    mir_type_decl.is_abi_c = type_decl.is_abi_c;
    mir_type_decl.fields = type_decl.fields;
    mir_type_decl.aliased_type = type_decl.aliased_type;
    for (const auto& variant : type_decl.variants) {
        mc::mir::VariantDecl mir_variant;
        mir_variant.name = variant.name;
        mir_variant.payload_fields = variant.payload_fields;
        mir_type_decl.variants.push_back(std::move(mir_variant));
    }
    return mir_type_decl;
}

mc::sema::Type RewriteImportedTypeNames(mc::sema::Type type,
                                        const std::unordered_map<std::string, std::string>& renamed_types) {
    for (auto& subtype : type.subtypes) {
        subtype = RewriteImportedTypeNames(std::move(subtype), renamed_types);
    }

    if (type.kind != mc::sema::Type::Kind::kNamed) {
        return type;
    }

    const auto found = renamed_types.find(type.name);
    if (found != renamed_types.end()) {
        type.name = found->second;
    }
    return type;
}

void RewriteImportedTypeDecl(mc::mir::TypeDecl& type_decl,
                             const std::unordered_map<std::string, std::string>& renamed_types) {
    type_decl.aliased_type = RewriteImportedTypeNames(std::move(type_decl.aliased_type), renamed_types);
    for (auto& field : type_decl.fields) {
        field.second = RewriteImportedTypeNames(std::move(field.second), renamed_types);
    }
    for (auto& variant : type_decl.variants) {
        for (auto& payload_field : variant.payload_fields) {
            payload_field.second = RewriteImportedTypeNames(std::move(payload_field.second), renamed_types);
        }
    }
}

void RewriteImportedSymbolReference(const std::unordered_map<std::string, std::string>& renamed_functions,
                                    const std::unordered_map<std::string, std::string>& renamed_globals,
                                    mc::mir::Instruction::TargetKind target_kind,
                                    std::string& value) {
    if (value.empty()) {
        return;
    }

    if (target_kind == mc::mir::Instruction::TargetKind::kFunction) {
        const auto found = renamed_functions.find(value);
        if (found != renamed_functions.end()) {
            value = found->second;
        }
        return;
    }

    if (target_kind == mc::mir::Instruction::TargetKind::kGlobal) {
        const auto found = renamed_globals.find(value);
        if (found != renamed_globals.end()) {
            value = found->second;
        }
    }
}

bool InstructionUsesTypeTargetMetadata(mc::mir::Instruction::Kind kind) {
    using Kind = mc::mir::Instruction::Kind;
    switch (kind) {
        case Kind::kArenaNew:
        case Kind::kConvert:
        case Kind::kConvertNumeric:
        case Kind::kConvertDistinct:
        case Kind::kPointerToInt:
        case Kind::kIntToPointer:
        case Kind::kArrayToSlice:
        case Kind::kBufferToSlice:
        case Kind::kAggregateInit:
            return true;
        case Kind::kConst:
        case Kind::kLocalAddr:
        case Kind::kLoadLocal:
        case Kind::kStoreLocal:
        case Kind::kStoreTarget:
        case Kind::kSymbolRef:
        case Kind::kBoundsCheck:
        case Kind::kDivCheck:
        case Kind::kShiftCheck:
        case Kind::kUnary:
        case Kind::kBinary:
        case Kind::kBufferNew:
        case Kind::kBufferFree:
        case Kind::kSliceFromBuffer:
        case Kind::kCall:
        case Kind::kVolatileLoad:
        case Kind::kVolatileStore:
        case Kind::kAtomicLoad:
        case Kind::kAtomicStore:
        case Kind::kAtomicExchange:
        case Kind::kAtomicCompareExchange:
        case Kind::kAtomicFetchAdd:
        case Kind::kField:
        case Kind::kIndex:
        case Kind::kSlice:
        case Kind::kVariantInit:
        case Kind::kVariantMatch:
        case Kind::kVariantExtract:
            return false;
    }
    return false;
}

bool InstructionUsesVariantTargetMetadata(mc::mir::Instruction::Kind kind) {
    using Kind = mc::mir::Instruction::Kind;
    return kind == Kind::kVariantInit || kind == Kind::kVariantMatch || kind == Kind::kVariantExtract;
}

std::string CanonicalVariantTarget(const mc::sema::Type& base_type,
                                   std::string_view variant_name) {
    if (variant_name.empty()) {
        return {};
    }
    if (base_type.kind == mc::sema::Type::Kind::kNamed && !base_type.name.empty()) {
        return base_type.name + "." + std::string(variant_name);
    }
    return std::string(variant_name);
}

void RewriteImportedInstructionMetadata(mc::mir::Instruction& instruction,
                                        const std::unordered_map<std::string, std::string>& renamed_functions,
                                        const std::unordered_map<std::string, std::string>& renamed_globals) {
    if (InstructionUsesTypeTargetMetadata(instruction.kind)) {
        const std::string formatted_type = mc::sema::FormatType(instruction.type);
        instruction.target = formatted_type;
        instruction.target_display = formatted_type;
    }

    if (InstructionUsesVariantTargetMetadata(instruction.kind)) {
        const std::string canonical_target = CanonicalVariantTarget(instruction.target_base_type, instruction.target_name);
        instruction.target = canonical_target;
        instruction.target_display = canonical_target;
    }

    RewriteImportedSymbolReference(renamed_functions, renamed_globals, instruction.target_kind, instruction.target);
    RewriteImportedSymbolReference(renamed_functions, renamed_globals, instruction.target_kind, instruction.target_name);
    RewriteImportedSymbolReference(renamed_functions, renamed_globals, instruction.target_kind, instruction.target_display);
}

void NamespaceImportedBuildUnit(mc::mir::Module& module,
                                std::string_view module_name) {
    std::unordered_map<std::string, std::string> renamed_types;
    std::unordered_map<std::string, std::string> renamed_functions;
    std::unordered_map<std::string, std::string> renamed_globals;

    for (auto& type_decl : module.type_decls) {
        if (type_decl.name.find('.') != std::string::npos) {
            continue;
        }
        const std::string qualified_name = QualifyImportedSymbol(module_name, type_decl.name);
        renamed_types.emplace(type_decl.name, qualified_name);
        type_decl.name = qualified_name;
    }

    for (auto& type_decl : module.type_decls) {
        RewriteImportedTypeDecl(type_decl, renamed_types);
    }

    for (auto& function : module.functions) {
        if (!function.is_extern) {
            const std::string qualified_name = QualifyImportedSymbol(module_name, function.name);
            renamed_functions.emplace(function.name, qualified_name);
            function.name = qualified_name;
        }

        for (auto& local : function.locals) {
            local.type = RewriteImportedTypeNames(std::move(local.type), renamed_types);
        }
        for (auto& return_type : function.return_types) {
            return_type = RewriteImportedTypeNames(std::move(return_type), renamed_types);
        }
    }

    for (auto& global : module.globals) {
        global.type = RewriteImportedTypeNames(std::move(global.type), renamed_types);
        for (auto& name : global.names) {
            const std::string qualified_name = QualifyImportedSymbol(module_name, name);
            renamed_globals.emplace(name, qualified_name);
            name = qualified_name;
        }
    }

    for (auto& function : module.functions) {
        for (auto& block : function.blocks) {
            for (auto& instruction : block.instructions) {
                instruction.type = RewriteImportedTypeNames(std::move(instruction.type), renamed_types);
                instruction.target_base_type = RewriteImportedTypeNames(std::move(instruction.target_base_type), renamed_types);
                for (auto& target_aux_type : instruction.target_aux_types) {
                    target_aux_type = RewriteImportedTypeNames(std::move(target_aux_type), renamed_types);
                }
                RewriteImportedInstructionMetadata(instruction, renamed_functions, renamed_globals);
            }
        }
    }
}

void AddImportedExternDeclarations(mc::mir::Module& module,
                                   const std::unordered_map<std::string, mc::sema::Module>& imported_modules) {
    std::unordered_set<std::string> existing_types;
    existing_types.reserve(module.type_decls.size());
    for (const auto& type_decl : module.type_decls) {
        existing_types.insert(type_decl.name);
    }

    std::unordered_set<std::string> existing_functions;
    existing_functions.reserve(module.functions.size());
    for (const auto& function : module.functions) {
        existing_functions.insert(function.name);
    }

    std::unordered_set<std::string> existing_globals;
    for (const auto& global : module.globals) {
        for (const auto& name : global.names) {
            existing_globals.insert(name);
        }
    }

    std::vector<std::pair<std::string, const mc::sema::Module*>> ordered_imports;
    ordered_imports.reserve(imported_modules.size());
    for (const auto& [module_name, imported_module] : imported_modules) {
        ordered_imports.push_back({module_name, &imported_module});
    }
    std::sort(ordered_imports.begin(), ordered_imports.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    for (const auto& [module_name, imported_module_ptr] : ordered_imports) {
        const auto& imported_module = *imported_module_ptr;
        for (const auto& type_decl : imported_module.type_decls) {
            if (!existing_types.insert(type_decl.name).second) {
                continue;
            }
            module.type_decls.push_back(ConvertImportedTypeDecl(type_decl));
        }

        for (const auto& function : imported_module.functions) {
            const std::string qualified_name = QualifyImportedSymbol(module_name, function.name);
            if (!existing_functions.insert(qualified_name).second) {
                continue;
            }

            mc::mir::Function extern_function;
            extern_function.name = qualified_name;
            extern_function.is_extern = true;
            extern_function.extern_abi = function.extern_abi;
            extern_function.type_params = function.type_params;
            for (std::size_t index = 0; index < function.param_types.size(); ++index) {
                extern_function.locals.push_back({
                    .name = "arg" + std::to_string(index),
                    .type = function.param_types[index],
                    .is_parameter = true,
                    .is_mutable = false,
                });
            }
            extern_function.return_types = function.return_types;
            module.functions.push_back(std::move(extern_function));
        }

        for (const auto& global : imported_module.globals) {
            mc::mir::GlobalDecl extern_global;
            extern_global.is_const = global.is_const;
            extern_global.is_extern = true;
            extern_global.type = global.type;
            for (const auto& name : global.names) {
                const std::string qualified_name = QualifyImportedSymbol(module_name, name);
                if (!existing_globals.insert(qualified_name).second) {
                    continue;
                }
                extern_global.names.push_back(qualified_name);
            }
            if (!extern_global.names.empty()) {
                module.globals.push_back(std::move(extern_global));
            }
        }
    }
}

std::unique_ptr<mc::mir::Module> MergeBuildUnits(const std::vector<BuildUnit>& units,
                                                 const mc::mir::Module& entry_module,
                                                 const std::filesystem::path& entry_source_path) {
    auto merged = std::make_unique<mc::mir::Module>();
    std::unordered_set<std::string> seen_imports;
    std::unordered_set<std::string> seen_types;
    std::unordered_set<std::string> seen_functions;
    std::unordered_set<std::string> seen_globals;
    std::unordered_set<std::string> defined_by_deps;
    for (const auto& unit : units) {
        if (unit.source_path == std::filesystem::absolute(entry_source_path).lexically_normal()) {
            continue;
        }
        const mc::mir::Module& unit_module = *unit.mir_result.module;
        for (const auto& import_name : unit_module.imports) {
            if (seen_imports.insert(import_name).second) {
                merged->imports.push_back(import_name);
            }
        }
        for (const auto& type_decl : unit_module.type_decls) {
            if (!seen_types.insert(type_decl.name).second) {
                continue;
            }
            merged->type_decls.push_back(type_decl);
        }
        for (const auto& global : unit_module.globals) {
            mc::mir::GlobalDecl merged_global = global;
            merged_global.names.clear();
            for (const auto& name : global.names) {
                if (seen_globals.insert(name).second) {
                    merged_global.names.push_back(name);
                }
            }
            if (!merged_global.names.empty()) {
                merged->globals.push_back(std::move(merged_global));
            }
        }
        for (const auto& function : unit_module.functions) {
            if (function.is_extern && !seen_functions.insert(function.name).second) {
                continue;
            }
            if (!function.is_extern) {
                defined_by_deps.insert(function.name);
                seen_functions.insert(function.name);
            }
            merged->functions.push_back(function);
        }
    }

    for (const auto& import_name : entry_module.imports) {
        if (seen_imports.insert(import_name).second) {
            merged->imports.push_back(import_name);
        }
    }
    for (const auto& type_decl : entry_module.type_decls) {
        if (!seen_types.insert(type_decl.name).second) {
            continue;
        }
        merged->type_decls.push_back(type_decl);
    }
    for (const auto& global : entry_module.globals) {
        mc::mir::GlobalDecl merged_global = global;
        merged_global.names.clear();
        for (const auto& name : global.names) {
            if (seen_globals.insert(name).second) {
                merged_global.names.push_back(name);
            }
        }
        if (!merged_global.names.empty()) {
            merged->globals.push_back(std::move(merged_global));
        }
    }
    for (const auto& function : entry_module.functions) {
        if (function.is_extern) {
            if (!seen_functions.insert(function.name).second) {
                continue;
            }
            merged->functions.push_back(function);
            continue;
        }
        if (!defined_by_deps.count(function.name)) {
            seen_functions.insert(function.name);
            merged->functions.push_back(function);
        }
    }
    return merged;
}

bool IsOutputOlderThan(const std::filesystem::path& output_path,
                      const std::filesystem::path& input_path) {
    if (!std::filesystem::exists(output_path) || !std::filesystem::exists(input_path)) {
        return true;
    }
    return std::filesystem::last_write_time(output_path) < std::filesystem::last_write_time(input_path);
}

bool ShouldRelinkProjectExecutable(const std::filesystem::path& executable_path,
                                   const std::filesystem::path& runtime_object_path,
                                   const std::optional<std::filesystem::path>& runtime_source_path,
                                   const std::vector<std::filesystem::path>& link_inputs,
                                   const std::vector<std::filesystem::path>& library_paths,
                                   const std::vector<BuildUnit>& units) {
    if (!std::filesystem::exists(executable_path) || !std::filesystem::exists(runtime_object_path)) {
        return true;
    }
    if (runtime_source_path.has_value() && IsOutputOlderThan(runtime_object_path, *runtime_source_path)) {
        return true;
    }
    if (IsOutputOlderThan(executable_path, runtime_object_path)) {
        return true;
    }
    for (const auto& unit : units) {
        if (!unit.reused_object || IsOutputOlderThan(executable_path, unit.object_path)) {
            return true;
        }
    }
    for (const auto& link_input : link_inputs) {
        if (IsOutputOlderThan(executable_path, link_input)) {
            return true;
        }
    }
    for (const auto& library_path : library_paths) {
        if (IsOutputOlderThan(executable_path, library_path)) {
            return true;
        }
    }
    return false;
}

bool ShouldArchiveProjectStaticLibrary(const std::filesystem::path& archive_path,
                                       const std::vector<BuildUnit>& units) {
    if (!std::filesystem::exists(archive_path)) {
        return true;
    }
    for (const auto& unit : units) {
        if (!unit.reused_object || IsOutputOlderThan(archive_path, unit.object_path)) {
            return true;
        }
    }
    return false;
}

void AppendUniquePaths(std::vector<std::filesystem::path>& destination,
                       const std::vector<std::filesystem::path>& paths) {
    std::unordered_set<std::string> seen;
    seen.reserve(destination.size() + paths.size());
    for (const auto& path : destination) {
        seen.insert(path.generic_string());
    }
    for (const auto& path : paths) {
        if (seen.insert(path.generic_string()).second) {
            destination.push_back(path);
        }
    }
}

}  // namespace

std::optional<std::filesystem::path> DiscoverHostedRuntimeSupportSource(const std::filesystem::path& source_path) {
    const auto repo_root = DiscoverRepositoryRoot(source_path);
    if (!repo_root.has_value()) {
        return std::nullopt;
    }

    const std::filesystem::path runtime_source = *repo_root / std::string(kHostedRuntimeSupportRelativePath);
    if (!std::filesystem::exists(runtime_source)) {
        return std::nullopt;
    }
    return runtime_source;
}

std::optional<std::filesystem::path> DiscoverRuntimeSupportSource(std::string_view env,
                                                                  std::string_view startup,
                                                                  const std::filesystem::path& source_path) {
    if (env == "hosted") {
        if (startup != "default") {
            return std::nullopt;
        }
        return DiscoverHostedRuntimeSupportSource(source_path);
    }

    if (env != "freestanding") {
        return std::nullopt;
    }

    const auto repo_root = DiscoverRepositoryRoot(source_path);
    if (!repo_root.has_value()) {
        return std::nullopt;
    }

    const std::filesystem::path runtime_source =
        *repo_root / std::string(kFreestandingRuntimeDirectoryRelativePath) / ("mc_" + std::string(startup) + ".c");
    if (!std::filesystem::exists(runtime_source)) {
        return std::nullopt;
    }
    return runtime_source;
}

std::vector<std::filesystem::path> ComputeEffectiveImportRoots(const std::filesystem::path& source_path,
                                                               const std::vector<std::filesystem::path>& import_roots) {
    std::vector<std::filesystem::path> resolved_roots;
    resolved_roots.reserve(import_roots.size() + 1);

    std::unordered_set<std::string> seen_roots;
    const auto add_root = [&](const std::filesystem::path& root) {
        const std::filesystem::path normalized = std::filesystem::absolute(root).lexically_normal();
        const std::string key = normalized.generic_string();
        if (seen_roots.insert(key).second) {
            resolved_roots.push_back(normalized);
        }
    };

    for (const auto& import_root : import_roots) {
        add_root(import_root);
    }

    if (const auto repo_root = DiscoverRepositoryRoot(source_path); repo_root.has_value()) {
        const std::filesystem::path stdlib_root = *repo_root / "stdlib";
        if (std::filesystem::exists(stdlib_root)) {
            add_root(stdlib_root);
        }
    }

    return resolved_roots;
}

std::optional<std::string> ReadSourceText(const std::filesystem::path& path,
                                          support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to read source file",
        });
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to read source file",
        });
        return std::nullopt;
    }

    return buffer.str();
}

std::optional<mc::parse::ParseResult> ParseSourcePath(const std::filesystem::path& path,
                                                      support::DiagnosticSink& diagnostics) {
    const auto source_text = ReadSourceText(path, diagnostics);
    if (!source_text.has_value()) {
        return std::nullopt;
    }

    const auto lex_result = mc::lex::Lex(*source_text, path.generic_string(), diagnostics);
    auto parse_result = mc::parse::Parse(lex_result, path, diagnostics);
    if (!parse_result.ok) {
        return std::nullopt;
    }

    return parse_result;
}

std::filesystem::path DefaultProjectPath() {
    return std::filesystem::current_path() / "build.toml";
}

std::string BootstrapTargetIdentity() {
    const auto config = mc::codegen_llvm::BootstrapTargetConfig();
    return config.triple;
}

bool IsSupportedMode(std::string_view mode) {
    return mode == "debug" || mode == "release" || mode == "checked";
}

bool IsSupportedEnv(std::string_view env) {
    return env == "hosted" || env == "freestanding";
}

bool IsExecutableTargetKind(std::string_view kind) {
    return kind == "exe" || kind == "test";
}

bool IsStaticLibraryTargetKind(std::string_view kind) {
    return kind == "staticlib";
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

std::string HashText(std::string_view text) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char byte : text) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ull;
    }
    return HexU64(hash);
}

std::filesystem::path ComputeModuleStatePath(const std::filesystem::path& source_path,
                                             const std::filesystem::path& build_dir) {
    return build_dir / "state" / (support::SanitizeArtifactStem(source_path) + ".state.txt");
}

std::filesystem::path ComputeRuntimeObjectPath(const std::filesystem::path& entry_source_path,
                                               const std::filesystem::path& build_dir,
                                               std::string_view env,
                                               std::string_view startup) {
    const auto build_targets = support::ComputeBuildArtifactTargets(entry_source_path, build_dir);
    const std::string runtime_object_name =
        build_targets.object.stem().generic_string() + "." + std::string(env) + "." + std::string(startup) + ".runtime.o";
    return build_targets.object.parent_path() / runtime_object_name;
}

std::optional<ModuleBuildState> LoadModuleBuildState(const std::filesystem::path& path,
                                                     support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    const auto read_state_line = [&](std::string_view expected_key) -> std::optional<std::string> {
        std::string line;
        if (!std::getline(input, line)) {
            return std::nullopt;
        }

        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || std::string_view(line.data(), tab) != expected_key) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        return line.substr(tab + 1);
    };

    const auto format_text = read_state_line("format");
    if (!format_text.has_value()) {
        return std::nullopt;
    }
    const auto parsed_format = support::ParseArrayLength(*format_text);
    if (!parsed_format.has_value()) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "invalid module build state format entry",
        });
        return std::nullopt;
    }
    if (static_cast<int>(*parsed_format) != kModuleBuildStateFormatVersion) {
        return std::nullopt;
    }

    auto target_identity = read_state_line("target");
    auto mode = read_state_line("mode");
    auto env = read_state_line("env");
    auto package_identity = read_state_line("package");
    auto source_hash = read_state_line("source_hash");
    auto interface_hash = read_state_line("interface_hash");
    auto wrap_hosted_main_text = read_state_line("wrap_hosted_main");
    if (!target_identity.has_value() || !mode.has_value() || !env.has_value() || !package_identity.has_value() ||
        !source_hash.has_value() || !interface_hash.has_value() || !wrap_hosted_main_text.has_value()) {
        return std::nullopt;
    }

    ModuleBuildState state {
        .target_identity = std::move(*target_identity),
        .package_identity = std::move(*package_identity),
        .mode = std::move(*mode),
        .env = std::move(*env),
        .source_hash = std::move(*source_hash),
        .interface_hash = std::move(*interface_hash),
        .wrap_hosted_main = *wrap_hosted_main_text == "1",
    };

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || std::string_view(line.data(), tab) != "import_hash") {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        const std::string value = line.substr(tab + 1);
        const std::size_t equals = value.find('=');
        if (equals == std::string::npos) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid imported interface hash entry",
            });
            return std::nullopt;
        }
        state.imported_interface_hashes.push_back({value.substr(0, equals), value.substr(equals + 1)});
    }

    return state;
}

bool WriteModuleBuildState(const std::filesystem::path& path,
                           const ModuleBuildState& state,
                           support::DiagnosticSink& diagnostics) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to write module build state",
        });
        return false;
    }

    output << "format\t" << kModuleBuildStateFormatVersion << '\n';
    output << "target\t" << state.target_identity << '\n';
    output << "mode\t" << state.mode << '\n';
    output << "env\t" << state.env << '\n';
    output << "package\t" << state.package_identity << '\n';
    output << "source_hash\t" << state.source_hash << '\n';
    output << "interface_hash\t" << state.interface_hash << '\n';
    output << "wrap_hosted_main\t" << (state.wrap_hosted_main ? "1" : "0") << '\n';
    for (const auto& [import_name, hash] : state.imported_interface_hashes) {
        output << "import_hash\t" << import_name << '=' << hash << '\n';
    }
    return static_cast<bool>(output);
}

bool ShouldReuseModuleObject(const ModuleBuildState& state,
                             const ModuleBuildState& current,
                             const std::filesystem::path& object_path,
                             const std::filesystem::path& mci_path) {
    if (!std::filesystem::exists(object_path) || !std::filesystem::exists(mci_path)) {
        return false;
    }
    if (state.target_identity != current.target_identity || state.package_identity != current.package_identity || state.mode != current.mode ||
        state.env != current.env || state.source_hash != current.source_hash ||
        state.interface_hash != current.interface_hash ||
        state.wrap_hosted_main != current.wrap_hosted_main) {
        return false;
    }
    return state.imported_interface_hashes == current.imported_interface_hashes;
}

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
            diagnostics.Report({
                .file_path = project.path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "target '" + target.name + "' must declare an explicit freestanding target",
            });
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
    return false;
}

bool DiscoverModuleGraphRecursive(const std::string& module_name,
                                  const std::filesystem::path& source_path,
                                  const std::vector<std::filesystem::path>& import_roots,
                                  const std::function<std::optional<std::string>(const std::filesystem::path&)>& package_identity_for_source,
                                  const std::unordered_set<std::string>& external_module_paths,
                                  support::DiagnosticSink& diagnostics,
                                  std::unordered_set<std::string>& visiting,
                                  std::unordered_set<std::string>& visited,
                                  std::vector<CompileNode>& nodes) {
    const std::filesystem::path normalized_path = std::filesystem::absolute(source_path).lexically_normal();
    const std::string path_key = normalized_path.generic_string();
    if (visited.contains(path_key)) {
        return true;
    }
    if (!visiting.insert(path_key).second) {
        diagnostics.Report({
            .file_path = normalized_path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "import cycle detected involving module: " + module_name,
        });
        return false;
    }

    auto parse_result = ParseSourcePath(normalized_path, diagnostics);
    if (!parse_result.has_value()) {
        visiting.erase(path_key);
        return false;
    }

    const auto current_package_identity = package_identity_for_source ? package_identity_for_source(normalized_path) : std::nullopt;
    if (IsInternalModulePath(normalized_path) && !current_package_identity.has_value()) {
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
        .module_name = module_name,
        .package_identity = current_package_identity.value_or(std::string {}),
        .source_path = normalized_path,
        .parse_result = std::move(*parse_result),
    };

    for (const auto& import_decl : node.parse_result.source_file->imports) {
        const auto import_path = ResolveImportPathWithPackageRules(normalized_path,
                                                                   import_decl.module_name,
                                                                   import_roots,
                                                                   current_package_identity,
                                                                   package_identity_for_source,
                                                                   diagnostics,
                                                                   import_decl.span);
        if (!import_path.has_value()) {
            visiting.erase(path_key);
            return false;
        }
        node.imports.push_back({
            .module_name = import_decl.module_name,
            .package_identity = import_path->package_identity.value_or(std::string {}),
            .source_path = import_path->path,
            .module_kind = IsInternalModulePath(import_path->path) ? ast::SourceFile::ModuleKind::kInternal
                                                                   : ast::SourceFile::ModuleKind::kOrdinary,
        });
        const std::string import_key = std::filesystem::absolute(import_path->path).lexically_normal().generic_string();
        if (external_module_paths.contains(import_key)) {
            continue;
        }
        if (!DiscoverModuleGraphRecursive(import_decl.module_name,
                                          import_path->path,
                                          import_roots,
                                          package_identity_for_source,
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

std::optional<CompileGraph> DiscoverModuleGraph(const std::filesystem::path& entry_source_path,
                                                const std::vector<std::filesystem::path>& import_roots,
                                                const std::function<std::optional<std::string>(const std::filesystem::path&)>& package_identity_for_source,
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
    CompileGraph graph {
        .entry_source_path = normalized_entry,
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
    if (!DiscoverModuleGraphRecursive(normalized_entry.stem().string(),
                                      normalized_entry,
                                      import_roots,
                                      package_identity_for_source,
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
        node_indices.emplace(graph.nodes[index].source_path.generic_string(), index);
    }

    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        for (const auto& import : graph.nodes[index].imports) {
            const std::string import_key = std::filesystem::absolute(import.source_path).lexically_normal().generic_string();
            const auto found = node_indices.find(import_key);
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

std::optional<std::vector<BuildUnit>> CompileModuleGraph(CompileGraph& graph,
                                                         support::DiagnosticSink& diagnostics,
                                                         bool emit_objects) {
    AssertCompileGraphTopologicalOrder(graph);

    std::vector<BuildUnit> units;
    units.reserve(graph.nodes.size());

    for (auto& node : graph.nodes) {
        sema::CheckOptions sema_options;
        const auto imported_data = LoadImportedInterfaces(node, graph.target_config.triple, graph.build_dir, diagnostics);
        if (!imported_data.has_value()) {
            return std::nullopt;
        }
        if (!imported_data->modules.empty()) {
            sema_options.imported_modules = &imported_data->modules;
        }
        if (!node.package_identity.empty()) {
            sema_options.current_package_identity = node.package_identity;
        }

        const auto source_text = ReadSourceText(node.source_path, diagnostics);
        if (!source_text.has_value()) {
            return std::nullopt;
        }

        auto sema_result = mc::sema::CheckProgram(*node.parse_result.source_file,
                                                  node.source_path,
                                                  sema_options,
                                                  diagnostics);
        if (!sema_result.ok) {
            return std::nullopt;
        }

        mc::mci::InterfaceArtifact interface_artifact = MakeModuleInterfaceArtifact(node.source_path,
                                                                                    *node.parse_result.source_file,
                                                                                    *sema_result.module,
                                                                                    graph.target_config.triple);
        const std::string interface_hash = mc::mci::ComputeInterfaceHash(interface_artifact);
        const auto dump_targets = support::ComputeDumpTargets(node.source_path, graph.build_dir);
        const auto build_targets = support::ComputeBuildArtifactTargets(node.source_path, graph.build_dir);
        const auto state_path = ComputeModuleStatePath(node.source_path, graph.build_dir);

        ModuleBuildState current_state {
            .target_identity = graph.target_config.triple,
            .package_identity = node.package_identity,
            .mode = graph.mode,
            .env = graph.env,
            .source_hash = HashText(*source_text),
            .interface_hash = interface_hash,
            .wrap_hosted_main = graph.wrap_entry_main && node.source_path == graph.entry_source_path,
            .imported_interface_hashes = imported_data->interface_hashes,
        };
        std::sort(current_state.imported_interface_hashes.begin(), current_state.imported_interface_hashes.end());

        const auto previous_state = LoadModuleBuildState(state_path, diagnostics);
        const bool reuse_object = emit_objects && previous_state.has_value() &&
                                  ShouldReuseModuleObject(*previous_state, current_state, build_targets.object, dump_targets.mci);

        bool interface_written = false;
        std::optional<mc::mci::InterfaceArtifact> previous_interface;
        if (std::filesystem::exists(dump_targets.mci)) {
            previous_interface = mc::mci::LoadInterfaceArtifact(dump_targets.mci, diagnostics);
            if (!previous_interface.has_value()) {
                return std::nullopt;
            }
            if (!mc::mci::ValidateInterfaceArtifactMetadata(*previous_interface,
                                                            dump_targets.mci,
                                                            graph.target_config.triple,
                                                            node.package_identity,
                                                            node.source_path.stem().string(),
                                                            node.parse_result.source_file->module_kind,
                                                            node.source_path,
                                                            diagnostics)) {
                return std::nullopt;
            }
        }
        const bool interface_changed = !previous_interface.has_value() || previous_interface->interface_hash != interface_hash;
        if (interface_changed) {
            if (!WriteModuleInterface(node.source_path, interface_artifact, graph.build_dir, diagnostics)) {
                return std::nullopt;
            }
            interface_written = true;
        }

        auto mir_result = mc::mir::LowerSourceFile(*node.parse_result.source_file,
                                                   *sema_result.module,
                                                   node.source_path,
                                                   diagnostics);
        if (emit_objects && mir_result.ok && !imported_data->modules.empty()) {
            AddImportedExternDeclarations(*mir_result.module, imported_data->modules);
        }
        if (mir_result.ok &&
            (node.source_path != graph.entry_source_path || graph.namespace_entry_module)) {
            NamespaceImportedBuildUnit(*mir_result.module, node.source_path.stem().string());
        }
        if (!mir_result.ok || !mc::mir::ValidateModule(*mir_result.module, node.source_path, diagnostics)) {
            return std::nullopt;
        }

        bool reused_object = false;
        if (emit_objects) {
            if (!reuse_object) {
                const auto object_result = mc::codegen_llvm::BuildObjectFile(
                    *mir_result.module,
                    node.source_path,
                    {
                        .target = graph.target_config,
                        .artifacts = {
                            .llvm_ir_path = build_targets.llvm_ir,
                            .object_path = build_targets.object,
                        },
                        .wrap_hosted_main = current_state.wrap_hosted_main,
                    },
                    diagnostics);
                if (!object_result.ok) {
                    return std::nullopt;
                }
                if (!WriteModuleBuildState(state_path, current_state, diagnostics)) {
                    return std::nullopt;
                }
            } else {
                reused_object = true;
                if (!interface_written && !std::filesystem::exists(dump_targets.mci)) {
                    if (!WriteModuleInterface(node.source_path, interface_artifact, graph.build_dir, diagnostics)) {
                        return std::nullopt;
                    }
                }
            }
        } else if (!interface_changed && !std::filesystem::exists(dump_targets.mci)) {
            if (!WriteModuleInterface(node.source_path, interface_artifact, graph.build_dir, diagnostics)) {
                return std::nullopt;
            }
        }

        BuildUnit unit {
            .source_path = node.source_path,
            .parse_result = std::move(node.parse_result),
            .sema_result = std::move(sema_result),
            .mir_result = std::move(mir_result),
            .llvm_ir_path = build_targets.llvm_ir,
            .object_path = build_targets.object,
            .interface_hash = interface_hash,
            .reused_object = reused_object,
        };
        units.push_back(std::move(unit));
    }

    return units;
}

bool PrepareLinkedTargetInterfaces(std::vector<TargetBuildGraph>& linked_targets,
                                   support::DiagnosticSink& diagnostics) {
    for (auto& linked_graph : linked_targets) {
        if (!PrepareLinkedTargetInterfaces(linked_graph.linked_targets, diagnostics)) {
            return false;
        }
        if (!CompileModuleGraph(linked_graph.compile_graph, diagnostics, false).has_value()) {
            return false;
        }
    }
    return true;
}

std::optional<BuildUnit> CompileToMir(const CommandOptions& options,
                                      support::DiagnosticSink& diagnostics,
                                      bool include_imports_for_build) {
    const auto entry_path = std::filesystem::absolute(options.source_path).lexically_normal();
    const auto import_roots = ComputeEffectiveImportRoots(entry_path, options.import_roots);
    const auto bootstrap_config = mc::codegen_llvm::BootstrapTargetConfig();
    auto graph = DiscoverModuleGraph(
        entry_path,
        import_roots,
        [&](const std::filesystem::path& path) {
            return ResolveDirectSourcePackageIdentity(path, import_roots);
        },
        {},
        options.build_dir,
        bootstrap_config,
        "debug",
        "hosted",
        true,
        false,
        diagnostics);
    if (!graph.has_value()) {
        return std::nullopt;
    }

    auto units = CompileModuleGraph(*graph, diagnostics, false);
    if (!units.has_value() || units->empty()) {
        return std::nullopt;
    }

    BuildUnit& entry_unit = units->back();
    if (include_imports_for_build && units->size() > 1) {
        auto merged_module = MergeBuildUnits(*units, *entry_unit.mir_result.module, entry_unit.source_path);
        if (!mc::mir::ValidateModule(*merged_module, entry_unit.source_path, diagnostics)) {
            return std::nullopt;
        }
        entry_unit.mir_result.module = std::move(merged_module);
    }

    return BuildUnit {
        .source_path = entry_unit.source_path,
        .parse_result = std::move(entry_unit.parse_result),
        .sema_result = std::move(entry_unit.sema_result),
        .mir_result = std::move(entry_unit.mir_result),
    };
}

bool WriteTextArtifact(const std::filesystem::path& path,
                       std::string_view contents,
                       std::string_view description,
                       support::DiagnosticSink& diagnostics) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << contents;
    if (output) {
        return true;
    }

    diagnostics.Report({
        .file_path = path,
        .span = support::kDefaultSourceSpan,
        .severity = support::DiagnosticSeverity::kError,
        .message = "unable to write " + std::string(description),
    });
    return false;
}

std::optional<ProjectBuildResult> BuildProjectTarget(TargetBuildGraph& graph,
                                                     support::DiagnosticSink& diagnostics) {
    std::vector<std::filesystem::path> linked_library_paths;
    linked_library_paths.reserve(graph.linked_targets.size());
    for (auto& linked_graph : graph.linked_targets) {
        auto linked_result = BuildProjectTarget(linked_graph, diagnostics);
        if (!linked_result.has_value()) {
            return std::nullopt;
        }
        AppendUniquePaths(linked_library_paths, linked_result->link_library_paths);
    }

    auto units = CompileModuleGraph(graph.compile_graph, diagnostics, true);
    if (!units.has_value() || units->empty()) {
        return std::nullopt;
    }

    const BuildUnit& entry_unit = units->back();
    const auto build_targets = support::ComputeBuildArtifactTargets(entry_unit.source_path, graph.compile_graph.build_dir);

    if (IsStaticLibraryTargetKind(graph.target.kind)) {
        if (ShouldArchiveProjectStaticLibrary(build_targets.static_library, *units)) {
            std::vector<std::filesystem::path> object_paths;
            object_paths.reserve(units->size());
            for (const auto& unit : *units) {
                object_paths.push_back(unit.object_path);
            }

            const auto archive_result = mc::codegen_llvm::ArchiveStaticLibrary(entry_unit.source_path,
                                                                               {
                                                                                   .target = graph.compile_graph.target_config,
                                                                                   .object_paths = object_paths,
                                                                                   .archive_path = build_targets.static_library,
                                                                               },
                                                                               diagnostics);
            if (!archive_result.ok) {
                return std::nullopt;
            }
        }

        std::vector<std::filesystem::path> link_paths {build_targets.static_library};
        AppendUniquePaths(link_paths, linked_library_paths);
        return ProjectBuildResult {
            .units = std::move(*units),
            .static_library_path = build_targets.static_library,
            .link_library_paths = std::move(link_paths),
        };
    }

    const auto runtime_object_path = ComputeRuntimeObjectPath(entry_unit.source_path,
                                                              graph.compile_graph.build_dir,
                                                              graph.target.env,
                                                              graph.target.runtime_startup);
    const auto runtime_source_path = DiscoverRuntimeSupportSource(graph.target.env,
                                                                  graph.target.runtime_startup,
                                                                  entry_unit.source_path);

    if (ShouldRelinkProjectExecutable(build_targets.executable,
                                      runtime_object_path,
                                      runtime_source_path,
                                      graph.target.link_inputs,
                                      linked_library_paths,
                                      *units)) {
        std::vector<std::filesystem::path> object_paths;
        object_paths.reserve(units->size());
        for (const auto& unit : *units) {
            object_paths.push_back(unit.object_path);
        }

        const auto link_result = mc::codegen_llvm::LinkExecutable(entry_unit.source_path,
                                                                  {
                                                                      .target = graph.compile_graph.target_config,
                                                                      .object_paths = object_paths,
                                                                      .extra_input_paths = graph.target.link_inputs,
                                                                      .library_paths = linked_library_paths,
                                                                      .runtime_source_path = runtime_source_path,
                                                                      .runtime_object_path = runtime_object_path,
                                                                      .executable_path = build_targets.executable,
                                                                  },
                                                                  diagnostics);
        if (!link_result.ok) {
            return std::nullopt;
        }
    }

    return ProjectBuildResult {
        .units = std::move(*units),
        .executable_path = build_targets.executable,
        .link_library_paths = std::move(linked_library_paths),
    };
}

}  // namespace mc::driver