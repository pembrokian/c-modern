#include "compiler/driver/internal.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "compiler/sema/const_eval.h"

namespace mc::driver {
namespace {

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

void OffsetNamePatternSpans(ast::NamePattern& pattern, std::size_t line_offset) {
    OffsetSpan(pattern.span, line_offset);
}

void OffsetAttributeArgSpans(ast::AttributeArg& arg, std::size_t line_offset) {
    OffsetSpan(arg.span, line_offset);
}

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

mc::sema::ConstValue RewriteImportedConstValue(mc::sema::ConstValue value,
                                               const std::unordered_map<std::string, std::string>& renamed_types,
                                               const std::unordered_map<std::string, std::string>& renamed_functions) {
    if (value.kind == mc::sema::ConstValue::Kind::kEnum) {
        value.enum_type = RewriteImportedTypeNames(std::move(value.enum_type), renamed_types);
    }
    if (value.kind == mc::sema::ConstValue::Kind::kProcedure) {
        if (const auto found = renamed_functions.find(value.procedure_name); found != renamed_functions.end()) {
            value.procedure_name = found->second;
        }
    }
    for (auto& element : value.elements) {
        element = RewriteImportedConstValue(std::move(element), renamed_types, renamed_functions);
    }
    value.text = mc::sema::RenderConstValue(value);
    return value;
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
        case Kind::kMemoryBarrier:
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

}  // namespace

std::string MakeModuleArtifactKey(std::string_view package_identity,
                                  std::string_view module_name) {
    if (package_identity.empty()) {
        return std::string(module_name);
    }
    return std::string(package_identity) + "::" + std::string(module_name);
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

std::optional<std::string> ReadLogicalModuleSourceText(const std::vector<std::filesystem::path>& source_paths,
                                                       support::DiagnosticSink& diagnostics) {
    std::string text;
    for (const auto& source_path : CanonicalizeModuleSourcePaths(source_paths)) {
        const auto source_text = ReadSourceText(source_path, diagnostics);
        if (!source_text.has_value()) {
            return std::nullopt;
        }
        text += source_path.generic_string();
        text.push_back('\n');
        text += *source_text;
        text.push_back('\0');
    }
    return text;
}

mc::mci::InterfaceArtifact MakeModuleInterfaceArtifact(const std::filesystem::path& source_path,
                                                       std::string_view module_name,
                                                       const ast::SourceFile& source_file,
                                                       const sema::Module& checked_module,
                                                       std::string target_identity) {
    return {
        .target_identity = std::move(target_identity),
        .package_identity = checked_module.package_identity,
        .module_name = std::string(module_name),
        .module_kind = source_file.module_kind,
        .source_path = source_path,
        .module = sema::BuildImportVisibleModuleSurface(checked_module, source_file),
    };
}

bool WriteModuleInterface(std::string_view artifact_key,
                          const mc::mci::InterfaceArtifact& artifact,
                          const std::filesystem::path& build_dir,
                          support::DiagnosticSink& diagnostics) {
    const auto dump_targets = support::ComputeLogicalDumpTargets(artifact_key, build_dir);
    return mc::mci::WriteInterfaceArtifact(dump_targets.mci, artifact, diagnostics);
}

std::optional<ImportedInterfaceData> LoadImportedInterfaces(const CompileNode& node,
                                                            std::string_view target_identity,
                                                            const std::filesystem::path& build_dir,
                                                            support::DiagnosticSink& diagnostics) {
    ImportedInterfaceData imported;
    for (const auto& import : node.imports) {
        const auto dump_targets = support::ComputeLogicalDumpTargets(import.artifact_key, build_dir);
        const auto artifact = mc::mci::LoadInterfaceArtifact(dump_targets.mci, diagnostics);
        if (!artifact.has_value()) {
            return std::nullopt;
        }
        if (!mc::mci::ValidateInterfaceArtifactMetadata(*artifact,
                                                        dump_targets.mci,
                                                        target_identity,
                                                        import.package_identity,
                                                        import.module_name,
                                                        import.module_kind,
                                                        import.source_path,
                                                        diagnostics)) {
            return std::nullopt;
        }
        imported.modules.emplace(import.module_name,
                                 mc::sema::RewriteImportedModuleSurfaceTypes(artifact->module, import.module_name));
        imported.interface_hashes.push_back({import.module_name, artifact->interface_hash});
    }
    return imported;
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
            if (name.find('.') != std::string::npos) {
                continue;
            }
            const std::string qualified_name = QualifyImportedSymbol(module_name, name);
            renamed_globals.emplace(name, qualified_name);
            name = qualified_name;
        }
        for (auto& value : global.constant_values) {
            if (!value.has_value()) {
                continue;
            }
            value = RewriteImportedConstValue(std::move(*value), renamed_types, renamed_functions);
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
                                                 const std::filesystem::path& entry_source_path,
                                                 support::DiagnosticSink& diagnostics) {
    auto merged = std::make_unique<mc::mir::Module>();
    std::unordered_set<std::string> seen_imports;
    std::unordered_set<std::string> seen_types;
    std::unordered_set<std::string> seen_functions;
    std::unordered_set<std::string> seen_globals;
    std::unordered_map<std::string, std::filesystem::path> defined_functions;
    const auto append_function = [&](const mc::mir::Function& function,
                                     const std::filesystem::path& source_path) -> bool {
        if (function.is_extern) {
            if (!seen_functions.insert(function.name).second) {
                return true;
            }
            merged->functions.push_back(function);
            return true;
        }

        const std::filesystem::path normalized_source = std::filesystem::absolute(source_path).lexically_normal();
        const auto [existing, inserted] = defined_functions.emplace(function.name, normalized_source);
        if (!inserted) {
            diagnostics.Report({
                .file_path = normalized_source,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "duplicate non-extern function definition during module merge: " + function.name +
                           "; first defined in " + existing->second.generic_string(),
            });
            return false;
        }

        seen_functions.insert(function.name);
        merged->functions.push_back(function);
        return true;
    };

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
            if (!append_function(function, unit.source_path)) {
                return {};
            }
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
        if (!append_function(function, entry_source_path)) {
            return {};
        }
    }
    return merged;
}

}  // namespace mc::driver
