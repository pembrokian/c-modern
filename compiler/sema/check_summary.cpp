#include "compiler/sema/check_internal.h"

#include <utility>

#include "compiler/sema/module_resolver.h"

namespace mc::sema {

using mc::ast::Decl;

void TopLevelSummaryBuilder::SeedImportedSymbols() {
    if (context_.imported_modules == nullptr) {
        return;
    }

    const auto lookup_seeded_type = [&](std::string_view name) -> const TypeDeclSummary* {
        for (const auto& type_decl : context_.module.type_decls) {
            if (type_decl.name == name) {
                return &type_decl;
            }
        }
        return nullptr;
    };

    std::unordered_set<std::string> seen_import_names;
    for (const auto& import_decl : context_.source_file.imports) {
        if (!seen_import_names.insert(import_decl.module_name).second) {
            context_.report(import_decl.span, "duplicate import module: " + import_decl.module_name);
            continue;
        }
        const Module* imported_module = context_.find_imported_module(import_decl.module_name);
        if (imported_module == nullptr) {
            context_.report(import_decl.span, "internal error: missing checked import module: " + import_decl.module_name);
            continue;
        }

        std::unordered_set<std::string> imported_type_names;
        for (const auto& type_decl : imported_module->type_decls) {
            imported_type_names.insert(type_decl.name);
        }

        for (const auto& type_decl : imported_module->type_decls) {
            TypeDeclSummary qualified_type = RewriteImportedTypeDecl(type_decl, import_decl.module_name, imported_type_names);
            if (!context_.type_symbols.emplace(qualified_type.name, qualified_type.kind).second) {
                const TypeDeclSummary* existing_type = lookup_seeded_type(qualified_type.name);
                if (existing_type != nullptr && EquivalentTypeDeclSummary(*existing_type, qualified_type)) {
                    continue;
                }
                context_.report(import_decl.span, "duplicate imported type symbol: " + qualified_type.name);
                continue;
            }
            context_.module.type_decls.push_back(std::move(qualified_type));
        }
    }
}

void TopLevelSummaryBuilder::CollectTopLevelDecls() {
    for (const auto& decl : context_.source_file.decls) {
        switch (decl.kind) {
            case Decl::Kind::kFunc:
            case Decl::Kind::kExternFunc:
                if (!context_.value_symbols.emplace(decl.name, decl.kind).second) {
                    context_.report(decl.span, "duplicate top-level value symbol: " + decl.name);
                    break;
                }
                context_.module.functions.push_back(context_.build_function_signature(decl));
                break;
            case Decl::Kind::kStruct:
            case Decl::Kind::kEnum:
            case Decl::Kind::kDistinct:
            case Decl::Kind::kTypeAlias:
                if (!context_.type_symbols.emplace(decl.name, decl.kind).second) {
                    context_.report(decl.span, "duplicate top-level type symbol: " + decl.name);
                    break;
                }
                context_.module.type_decls.push_back(context_.build_type_decl_summary(decl));
                break;
            case Decl::Kind::kConst:
            case Decl::Kind::kVar: {
                GlobalSummary global;
                global.is_const = decl.kind == Decl::Kind::kConst;
                global.type = context_.semantic_type_from_ast(decl.type_ann.get(), {});
                for (const auto& name : decl.pattern.names) {
                    if (!context_.value_symbols.emplace(name, decl.kind).second) {
                        context_.report(decl.span, "duplicate top-level value symbol: " + name);
                        continue;
                    }
                    global.names.push_back(name);
                    context_.global_symbols[name] = {
                        .type = global.type,
                        .is_mutable = decl.kind == Decl::Kind::kVar,
                    };
                }
                context_.module.globals.push_back(std::move(global));
                break;
            }
        }
    }
}

}  // namespace mc::sema