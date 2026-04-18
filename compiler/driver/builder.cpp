#include "compiler/driver/internal.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "compiler/codegen_llvm/backend.h"

namespace mc::driver {
namespace {

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
        if (node.source_paths.size() > 1) {
            sema_options.module_part_paths = CanonicalizeModuleSourcePaths(node.source_paths);
            sema_options.module_part_line_stride = kMergedModulePartLineStride;
        }

        const auto source_text = ReadLogicalModuleSourceText(node.source_paths, diagnostics);
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
                                                                                    node.module_name,
                                                                                    *node.parse_result.source_file,
                                                                                    *sema_result.module,
                                                                                    graph.target_config.triple);
        const std::string interface_hash = mc::mci::ComputeInterfaceHash(interface_artifact);
        const auto dump_targets = support::ComputeLogicalDumpTargets(node.artifact_key, graph.build_dir);
        const auto build_targets = support::ComputeLogicalBuildArtifactTargets(node.artifact_key, graph.build_dir);
        const auto state_path = ComputeLogicalModuleStatePath(node.artifact_key, graph.build_dir);

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
                                                            node.module_name,
                                                            node.parse_result.source_file->module_kind,
                                                            node.source_path,
                                                            diagnostics)) {
                return std::nullopt;
            }
        }
        const bool interface_changed = !previous_interface.has_value() || previous_interface->interface_hash != interface_hash;
        if (interface_changed) {
            if (!WriteModuleInterface(node.artifact_key, interface_artifact, graph.build_dir, diagnostics)) {
                return std::nullopt;
            }
            interface_written = true;
        }

        auto mir_result = mc::mir::LowerSourceFile(*node.parse_result.source_file,
                                                   *sema_result.module,
                                                   node.source_path,
                                                   &imported_data->modules,
                                                   diagnostics);
        if (emit_objects && mir_result.ok && !imported_data->modules.empty()) {
            AddImportedExternDeclarations(*mir_result.module, imported_data->modules);
        }
        if (mir_result.ok &&
            (node.source_path != graph.entry_source_path || graph.namespace_entry_module)) {
            NamespaceImportedBuildUnit(*mir_result.module, node.module_name);
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
                    if (!WriteModuleInterface(node.artifact_key, interface_artifact, graph.build_dir, diagnostics)) {
                        return std::nullopt;
                    }
                }
            }
        } else if (!interface_changed && !std::filesystem::exists(dump_targets.mci)) {
            if (!WriteModuleInterface(node.artifact_key, interface_artifact, graph.build_dir, diagnostics)) {
                return std::nullopt;
            }
        }

        BuildUnit unit {
            .module_name = node.module_name,
            .artifact_key = node.artifact_key,
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
        auto merged_module = MergeBuildUnits(*units, *entry_unit.mir_result.module, entry_unit.source_path, diagnostics);
        if (!merged_module) {
            return std::nullopt;
        }
        if (!mc::mir::ValidateModule(*merged_module, entry_unit.source_path, diagnostics)) {
            return std::nullopt;
        }
        entry_unit.mir_result.module = std::move(merged_module);
    }

    return BuildUnit {
        .module_name = entry_unit.module_name,
        .artifact_key = entry_unit.artifact_key,
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
    const auto build_targets = support::ComputeLogicalBuildArtifactTargets(entry_unit.artifact_key, graph.compile_graph.build_dir);

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

    const auto runtime_object_path = ComputeLogicalRuntimeObjectPath(entry_unit.artifact_key,
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
