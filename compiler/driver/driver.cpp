#include "compiler/driver/driver.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "compiler/ast/ast.h"
#include "compiler/codegen_llvm/backend.h"
#include "compiler/lex/lexer.h"
#include "compiler/mir/mir.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"
#include "compiler/support/dump_paths.h"
#include "compiler/support/source_manager.h"

namespace mc::driver {
namespace {

constexpr std::string_view kUsage =
    "Modern C bootstrap driver\n"
    "\n"
    "Usage:\n"
    "  mc check <source> [--build-dir <dir>] [--import-root <dir>] [--emit-dump-paths] [--dump-ast] [--dump-mir] [--dump-backend]\n"
    "  mc build <source> [--build-dir <dir>] [--import-root <dir>] [--emit-dump-paths] [--dump-ast] [--dump-mir] [--dump-backend]\n"
    "  mc dump-paths <source> [--build-dir <dir>]\n"
    "  mc --help\n";

struct CommandOptions {
    std::filesystem::path source_path;
    std::filesystem::path build_dir = "build/debug";
    std::vector<std::filesystem::path> import_roots;
    bool emit_dump_paths = false;
    bool dump_ast = false;
    bool dump_mir = false;
    bool dump_backend = false;
};

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

mc::sema::CheckOptions BuildSemaOptions(const std::filesystem::path& source_path,
                                        const CommandOptions& options) {
    mc::sema::CheckOptions sema_options;
    sema_options.import_roots = ComputeEffectiveImportRoots(source_path, options.import_roots);
    return sema_options;
}

void PrintUsage(std::ostream& stream) {
    stream << kUsage;
}

std::optional<CommandOptions> ParseCommandOptions(int argc,
                                                  const char* argv[],
                                                  int start_index,
                                                  std::ostream& errors) {
    if (start_index >= argc) {
        errors << "missing source path\n";
        return std::nullopt;
    }

    CommandOptions options;
    options.source_path = argv[start_index];

    for (int index = start_index + 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--emit-dump-paths") {
            options.emit_dump_paths = true;
            continue;
        }

        if (argument == "--dump-ast") {
            options.dump_ast = true;
            continue;
        }

        if (argument == "--dump-mir") {
            options.dump_mir = true;
            continue;
        }

        if (argument == "--dump-backend") {
            options.dump_backend = true;
            continue;
        }

        if (argument == "--build-dir") {
            if (index + 1 >= argc) {
                errors << "missing value for --build-dir\n";
                return std::nullopt;
            }

            options.build_dir = argv[++index];
            continue;
        }

        if (argument == "--import-root") {
            if (index + 1 >= argc) {
                errors << "missing value for --import-root\n";
                return std::nullopt;
            }

            options.import_roots.push_back(argv[++index]);
            continue;
        }

        errors << "unknown argument: " << argument << '\n';
        return std::nullopt;
    }

    return options;
}

void PrintDumpTargets(const support::DumpTargets& targets, std::ostream& stream) {
    stream << "ast: " << targets.ast.generic_string() << '\n';
    stream << "mir: " << targets.mir.generic_string() << '\n';
    stream << "backend: " << targets.backend.generic_string() << '\n';
    stream << "mci: " << targets.mci.generic_string() << '\n';
}

void PrintArtifactTargets(const support::DumpTargets& dump_targets,
                          const support::BuildArtifactTargets& build_targets,
                          std::ostream& stream) {
    PrintDumpTargets(dump_targets, stream);
    stream << "llvm: " << build_targets.llvm_ir.generic_string() << '\n';
    stream << "object: " << build_targets.object.generic_string() << '\n';
    stream << "executable: " << build_targets.executable.generic_string() << '\n';
}

struct CheckedProgram {
    const support::SourceFile* source_file = nullptr;
    mc::parse::ParseResult parse_result;
    mc::sema::CheckResult sema_result;
    mc::mir::LowerResult mir_result;
};

struct BuildUnit {
    std::filesystem::path source_path;
    mc::parse::ParseResult parse_result;
    mc::sema::CheckResult sema_result;
    mc::mir::LowerResult mir_result;
};

std::string QualifyImportedSymbol(std::string_view module_name,
                                  std::string_view symbol_name) {
    return std::string(module_name) + "." + std::string(symbol_name);
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

void NamespaceImportedBuildUnit(mc::mir::Module& module,
                                std::string_view module_name) {
    std::unordered_map<std::string, std::string> renamed_types;
    std::unordered_map<std::string, std::string> renamed_functions;
    std::unordered_map<std::string, std::string> renamed_globals;

    module.type_decls.erase(std::remove_if(module.type_decls.begin(),
                                           module.type_decls.end(),
                                           [](const mc::mir::TypeDecl& type_decl) { return type_decl.name.find('.') != std::string::npos; }),
                           module.type_decls.end());

    for (auto& type_decl : module.type_decls) {
        const std::string qualified_name = QualifyImportedSymbol(module_name, type_decl.name);
        renamed_types.emplace(type_decl.name, qualified_name);
        type_decl.name = qualified_name;
    }

    for (auto& type_decl : module.type_decls) {
        RewriteImportedTypeDecl(type_decl, renamed_types);
    }

    for (auto& function : module.functions) {
        if (function.is_extern) {
        } else {
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
                RewriteImportedSymbolReference(renamed_functions, renamed_globals, instruction.target_kind, instruction.target);
                RewriteImportedSymbolReference(renamed_functions, renamed_globals, instruction.target_kind, instruction.target_name);
                RewriteImportedSymbolReference(renamed_functions, renamed_globals, instruction.target_kind, instruction.target_display);
            }
        }
    }
}

bool CollectBuildUnits(const std::filesystem::path& source_path,
                       const CommandOptions& options,
                       support::DiagnosticSink& diagnostics,
                       std::unordered_set<std::string>& visiting,
                       std::unordered_map<std::string, std::size_t>& built_indices,
                       std::vector<BuildUnit>& units) {
    const std::filesystem::path normalized_path = std::filesystem::absolute(source_path).lexically_normal();
    const std::string path_key = normalized_path.generic_string();
    if (built_indices.contains(path_key)) {
        return true;
    }
    if (!visiting.insert(path_key).second) {
        diagnostics.Report({
            .file_path = normalized_path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "import cycle detected involving module: " + normalized_path.stem().string(),
        });
        return false;
    }

    auto parse_result = ParseSourcePath(normalized_path, diagnostics);
    if (!parse_result.has_value()) {
        visiting.erase(path_key);
        return false;
    }

    const auto effective_import_roots = ComputeEffectiveImportRoots(normalized_path, options.import_roots);
    for (const auto& import_decl : parse_result->source_file->imports) {
        const auto import_path = mc::support::ResolveImportPath(normalized_path,
                                                                import_decl.module_name,
                                                                effective_import_roots,
                                                                diagnostics,
                                                                import_decl.span);
        if (!import_path.has_value()) {
            visiting.erase(path_key);
            return false;
        }
        if (!CollectBuildUnits(*import_path, options, diagnostics, visiting, built_indices, units)) {
            visiting.erase(path_key);
            return false;
        }
    }

    auto sema_result = mc::sema::CheckProgram(*parse_result->source_file,
                                              normalized_path,
                                              BuildSemaOptions(normalized_path, options),
                                              diagnostics);
    if (!sema_result.ok) {
        visiting.erase(path_key);
        return false;
    }

    auto mir_result = mc::mir::LowerSourceFile(*parse_result->source_file,
                                               *sema_result.module,
                                               normalized_path,
                                               diagnostics);
    if (!mir_result.ok || !mc::mir::ValidateModule(*mir_result.module, normalized_path, diagnostics)) {
        visiting.erase(path_key);
        return false;
    }

    built_indices.emplace(path_key, units.size());
    units.push_back(BuildUnit {
        .source_path = normalized_path,
        .parse_result = std::move(*parse_result),
        .sema_result = std::move(sema_result),
        .mir_result = std::move(mir_result),
    });
    visiting.erase(path_key);
    return true;
}

std::unique_ptr<mc::mir::Module> MergeBuildUnits(const std::vector<BuildUnit>& units,
                                                 const mc::mir::Module& entry_module,
                                                 const std::filesystem::path& entry_source_path) {
    auto merged = std::make_unique<mc::mir::Module>();
    std::unordered_set<std::string> seen_imports;
    for (const auto& unit : units) {
        mc::mir::Module namespaced_module = *unit.mir_result.module;
        if (unit.source_path != std::filesystem::absolute(entry_source_path).lexically_normal()) {
            NamespaceImportedBuildUnit(namespaced_module, unit.source_path.stem().string());
        }
        for (const auto& import_name : unit.mir_result.module->imports) {
            if (seen_imports.insert(import_name).second) {
                merged->imports.push_back(import_name);
            }
        }
        merged->type_decls.insert(merged->type_decls.end(),
                                  namespaced_module.type_decls.begin(),
                                  namespaced_module.type_decls.end());
        merged->globals.insert(merged->globals.end(),
                               namespaced_module.globals.begin(),
                               namespaced_module.globals.end());
        merged->functions.insert(merged->functions.end(),
                                 namespaced_module.functions.begin(),
                                 namespaced_module.functions.end());
    }

    for (const auto& import_name : entry_module.imports) {
        if (seen_imports.insert(import_name).second) {
            merged->imports.push_back(import_name);
        }
    }
    merged->type_decls.insert(merged->type_decls.end(),
                              entry_module.type_decls.begin(),
                              entry_module.type_decls.end());
    merged->globals.insert(merged->globals.end(),
                           entry_module.globals.begin(),
                           entry_module.globals.end());
    merged->functions.insert(merged->functions.end(),
                             entry_module.functions.begin(),
                             entry_module.functions.end());
    return merged;
}

std::optional<CheckedProgram> CompileToMir(const CommandOptions& options,
                                           support::SourceManager& source_manager,
                                           support::DiagnosticSink& diagnostics,
                                           bool include_imports_for_build) {
    const auto file_id = source_manager.LoadFile(options.source_path, diagnostics);
    if (!file_id.has_value()) {
        return std::nullopt;
    }

    const support::SourceFile* source_file = source_manager.GetFile(*file_id);
    const auto lex_result = mc::lex::Lex(source_file->contents, source_file->path.generic_string(), diagnostics);
    auto parse_result = mc::parse::Parse(lex_result, source_file->path, diagnostics);
    if (!parse_result.ok) {
        return std::nullopt;
    }

    auto sema_result = mc::sema::CheckProgram(*parse_result.source_file,
                                              source_file->path,
                                              BuildSemaOptions(source_file->path, options),
                                              diagnostics);
    if (!sema_result.ok) {
        return std::nullopt;
    }

    auto mir_result = mc::mir::LowerSourceFile(*parse_result.source_file, *sema_result.module, source_file->path, diagnostics);
    if (!mir_result.ok) {
        return std::nullopt;
    }

    if (!mc::mir::ValidateModule(*mir_result.module, source_file->path, diagnostics)) {
        return std::nullopt;
    }

    if (include_imports_for_build) {
        const std::filesystem::path normalized_entry_path = std::filesystem::absolute(source_file->path).lexically_normal();
        const std::string entry_key = normalized_entry_path.generic_string();
        std::unordered_set<std::string> visiting {entry_key};
        std::unordered_map<std::string, std::size_t> built_indices;
        std::vector<BuildUnit> units;

        const auto effective_import_roots = ComputeEffectiveImportRoots(normalized_entry_path, options.import_roots);
        for (const auto& import_decl : parse_result.source_file->imports) {
            const auto import_path = mc::support::ResolveImportPath(normalized_entry_path,
                                                                    import_decl.module_name,
                                                                    effective_import_roots,
                                                                    diagnostics,
                                                                    import_decl.span);
            if (!import_path.has_value()) {
                return std::nullopt;
            }
            if (!CollectBuildUnits(*import_path, options, diagnostics, visiting, built_indices, units)) {
                return std::nullopt;
            }
        }
        visiting.erase(entry_key);

        auto merged_module = MergeBuildUnits(units, *mir_result.module, normalized_entry_path);
        if (!mc::mir::ValidateModule(*merged_module, source_file->path, diagnostics)) {
            return std::nullopt;
        }
        mir_result.module = std::move(merged_module);
    }

    return CheckedProgram {
        .source_file = source_file,
        .parse_result = std::move(parse_result),
        .sema_result = std::move(sema_result),
        .mir_result = std::move(mir_result),
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

int RunCheck(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    support::SourceManager source_manager;
    const auto checked = CompileToMir(options, source_manager, diagnostics, false);
    if (!checked.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const auto targets = support::ComputeDumpTargets(checked->source_file->path, options.build_dir);

    if (options.dump_ast) {
        if (!WriteTextArtifact(targets.ast,
                               mc::ast::DumpSourceFile(*checked->parse_result.source_file),
                               "AST dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    if (options.dump_mir) {
        if (!WriteTextArtifact(targets.mir,
                               mc::mir::DumpModule(*checked->mir_result.module),
                               "MIR dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    if (options.dump_backend) {
        mc::codegen_llvm::LowerOptions backend_options {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
        };
        const auto backend_result = mc::codegen_llvm::LowerModule(*checked->mir_result.module,
                                                                  checked->source_file->path,
                                                                  backend_options,
                                                                  diagnostics);
        if (!backend_result.ok) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        if (!WriteTextArtifact(targets.backend,
                               mc::codegen_llvm::DumpModule(*backend_result.module),
                               "backend dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    std::cout << "checked " << checked->source_file->path.generic_string()
              << " (bootstrap phase 3 sema, phase 4 MIR)" << '\n';

    if (options.emit_dump_paths) {
        PrintDumpTargets(targets, std::cout);
    }

    return 0;
}

int RunBuild(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    support::SourceManager source_manager;
    const auto checked = CompileToMir(options, source_manager, diagnostics, true);
    if (!checked.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const auto dump_targets = support::ComputeDumpTargets(checked->source_file->path, options.build_dir);
    const auto build_targets = support::ComputeBuildArtifactTargets(checked->source_file->path, options.build_dir);

    if (options.dump_ast &&
        !WriteTextArtifact(dump_targets.ast,
                           mc::ast::DumpSourceFile(*checked->parse_result.source_file),
                           "AST dump",
                           diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    if (options.dump_mir &&
        !WriteTextArtifact(dump_targets.mir,
                           mc::mir::DumpModule(*checked->mir_result.module),
                           "MIR dump",
                           diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    if (options.dump_backend) {
        const auto backend_result = mc::codegen_llvm::LowerModule(*checked->mir_result.module,
                                                                  checked->source_file->path,
                                                                  {.target = mc::codegen_llvm::BootstrapTargetConfig()},
                                                                  diagnostics);
        if (!backend_result.ok) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
        if (!WriteTextArtifact(dump_targets.backend,
                               mc::codegen_llvm::DumpModule(*backend_result.module),
                               "backend dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    const auto build_result = mc::codegen_llvm::BuildExecutable(
        *checked->mir_result.module,
        checked->source_file->path,
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = build_targets.llvm_ir,
                .object_path = build_targets.object,
                .executable_path = build_targets.executable,
            },
        },
        diagnostics);
    if (!build_result.ok) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    std::cout << "built " << checked->source_file->path.generic_string() << " -> "
              << build_result.artifacts.executable_path.generic_string()
              << " (bootstrap phase 5 executable path)" << '\n';

    if (options.emit_dump_paths) {
        PrintArtifactTargets(dump_targets, build_targets, std::cout);
    }

    return 0;
}

int RunDumpPaths(const CommandOptions& options) {
    PrintArtifactTargets(support::ComputeDumpTargets(options.source_path, options.build_dir),
                         support::ComputeBuildArtifactTargets(options.source_path, options.build_dir),
                         std::cout);
    return 0;
}

}  // namespace

int Run(int argc, const char* argv[]) {
    if (argc <= 1) {
        PrintUsage(std::cout);
        return 0;
    }

    const std::string_view command = argv[1];
    if (command == "--help" || command == "-h" || command == "help") {
        PrintUsage(std::cout);
        return 0;
    }

    if (command == "check") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value()) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunCheck(*options);
    }

    if (command == "build") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value()) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunBuild(*options);
    }

    if (command == "dump-paths") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value()) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunDumpPaths(*options);
    }

    std::cerr << "unknown command: " << command << '\n';
    PrintUsage(std::cerr);
    return 1;
}

}  // namespace mc::driver
