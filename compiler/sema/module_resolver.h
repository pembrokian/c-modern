#ifndef C_MODERN_COMPILER_SEMA_MODULE_RESOLVER_H_
#define C_MODERN_COMPILER_SEMA_MODULE_RESOLVER_H_

#include <string>
#include <string_view>
#include <unordered_set>

#include "compiler/sema/check.h"

namespace mc::sema {

std::string QualifyImportedName(std::string_view module_name, std::string_view name);
TypeDeclSummary RewriteImportedTypeDecl(const TypeDeclSummary& type_decl,
                                        std::string_view module_name,
                                        const std::unordered_set<std::string>& local_type_names);
bool EquivalentTypeDeclSummary(const TypeDeclSummary& left, const TypeDeclSummary& right);
Module BuildImportVisibleModuleSurface(const Module& module,
                                       const ast::SourceFile& source_file);
Module RewriteImportedModuleSurfaceTypes(const Module& module,
                                         std::string_view module_name);

}  // namespace mc::sema

#endif  // C_MODERN_COMPILER_SEMA_MODULE_RESOLVER_H_