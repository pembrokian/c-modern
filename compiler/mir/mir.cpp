#include "compiler/mir/mir.h"

#include "compiler/mir/mir_internal.h"

#include <cassert>
#include <cctype>
#include <optional>
#include <queue>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "compiler/sema/type_utils.h"

namespace mc::mir {

struct SpecialCallEntry {
    std::string_view leaf_name;
    std::string_view module_name;
    SpecialCallKind kind;
};

constexpr SpecialCallEntry kSpecialCallTable[] = {
    {"panic", "", SpecialCallKind::kPanic},
    {"mmio_ptr", "hal", SpecialCallKind::kMmioPtr},
    {"buffer_new", "mem", SpecialCallKind::kBufferNew},
    {"buffer_free", "mem", SpecialCallKind::kBufferFree},
    {"arena_new", "mem", SpecialCallKind::kArenaNew},
    {"slice_from_buffer", "mem", SpecialCallKind::kSliceFromBuffer},
    {"memory_barrier", "hal", SpecialCallKind::kMemoryBarrier},
    {"volatile_load", "hal", SpecialCallKind::kVolatileLoad},
    {"volatile_store", "hal", SpecialCallKind::kVolatileStore},
    {"atomic_load", "sync", SpecialCallKind::kAtomicLoad},
    {"atomic_store", "sync", SpecialCallKind::kAtomicStore},
    {"atomic_exchange", "sync", SpecialCallKind::kAtomicExchange},
    {"atomic_compare_exchange", "sync", SpecialCallKind::kAtomicCompareExchange},
    {"atomic_fetch_add", "sync", SpecialCallKind::kAtomicFetchAdd},
};

bool MatchesSpecialCallName(std::string_view callee_name, const SpecialCallEntry& entry) {
    if (entry.module_name.empty()) {
        return callee_name == entry.leaf_name;
    }
    return callee_name == entry.leaf_name ||
           (callee_name.starts_with(entry.module_name) && callee_name.size() == entry.module_name.size() + 1 + entry.leaf_name.size() &&
            callee_name[entry.module_name.size()] == '.' &&
            callee_name.substr(entry.module_name.size() + 1) == entry.leaf_name);
}

SpecialCallKind ClassifySpecialCall(std::string_view callee_name) {
    for (const auto& entry : kSpecialCallTable) {
        if (MatchesSpecialCallName(callee_name, entry)) {
            return entry.kind;
        }
    }
    return SpecialCallKind::kNone;
}

bool IsDedicatedCallSurface(std::string_view callee_name) {
    return ClassifySpecialCall(callee_name) != SpecialCallKind::kNone;
}

bool HasAtomicOrderMetadata(const Instruction& instruction) {
    return !instruction.atomic_order.empty();
}

bool HasCompareExchangeOrderMetadata(const Instruction& instruction) {
    return !instruction.atomic_success_order.empty() && !instruction.atomic_failure_order.empty();
}

std::string AtomicMetadataText(const Instruction& instruction) {
    // This is a dump-formatting helper, not a validation API. An empty string
    // means either no atomic metadata or a partially-populated compare-exchange
    // pair that validation should reject before dumps matter.
    if (!instruction.atomic_success_order.empty() || !instruction.atomic_failure_order.empty()) {
        if (instruction.atomic_success_order.empty() || instruction.atomic_failure_order.empty()) {
            return {};
        }
        return "success=" + instruction.atomic_success_order + ",failure=" + instruction.atomic_failure_order;
    }
    if (!instruction.atomic_order.empty()) {
        return "order=" + instruction.atomic_order;
    }
    return {};
}

bool MatchGenericTypePattern(
    const Module& module,
    const sema::Type& pattern,
    const sema::Type& actual,
    const std::unordered_map<std::string, bool>& type_param_set,
    std::unordered_map<std::string, sema::Type>& bindings) {
    // This performs one-directional matching from an expected generic pattern
    // onto an actual type while accumulating type-parameter bindings.
    const sema::Type canonical_pattern = CanonicalMirType(module, pattern);
    const sema::Type canonical_actual = CanonicalMirType(module, actual);

    if (canonical_pattern.kind == sema::Type::Kind::kNamed && type_param_set.contains(canonical_pattern.name)) {
        const auto found = bindings.find(canonical_pattern.name);
        if (found == bindings.end()) {
            bindings.emplace(canonical_pattern.name, canonical_actual);
            return true;
        }
        return AreEquivalentTypes(module, found->second, canonical_actual);
    }

    if (canonical_pattern.kind != canonical_actual.kind) {
        return false;
    }

    switch (canonical_pattern.kind) {
        case sema::Type::Kind::kUnknown:
            return true;
        case sema::Type::Kind::kNamed:
            if (canonical_pattern.name != canonical_actual.name || canonical_pattern.subtypes.size() != canonical_actual.subtypes.size()) {
                return false;
            }
            break;
        case sema::Type::Kind::kArray:
            if (canonical_pattern.metadata != canonical_actual.metadata || canonical_pattern.subtypes.size() != canonical_actual.subtypes.size()) {
                return false;
            }
            break;
        default:
            if (canonical_pattern.metadata != canonical_actual.metadata) {
                return false;
            }
            break;
    }

    if (canonical_pattern.subtypes.size() != canonical_actual.subtypes.size()) {
        return false;
    }
    for (std::size_t index = 0; index < canonical_pattern.subtypes.size(); ++index) {
        if (!MatchGenericTypePattern(module,
                                     canonical_pattern.subtypes[index],
                                     canonical_actual.subtypes[index],
                                     type_param_set,
                                     bindings)) {
            return false;
        }
    }
    return true;
}

const TypeDecl* FindMirTypeDecl(const Module& module, std::string_view name) {
    // Bootstrap MIR still uses linear declaration scans here. Module sizes are
    // small, and a dedicated lookup table can be added when MIR metadata grows.
    for (const auto& type_decl : module.type_decls) {
        if (type_decl.name == name) {
            return &type_decl;
        }
    }
    return nullptr;
}

const GlobalDecl* FindMirGlobalDecl(const Module& module, std::string_view name) {
    // Bootstrap MIR still uses linear declaration scans here. Module sizes are
    // small, and a dedicated lookup table can be added when MIR metadata grows.
    for (const auto& global : module.globals) {
        for (const auto& global_name : global.names) {
            if (global_name == name) {
                return &global;
            }
        }
    }
    return nullptr;
}

std::optional<sema::Type> FindMirFieldType(
    const Module& module,
    const sema::Type& base_type,
    std::string_view field_name) {
    
    const auto builtin_fields = sema::BuiltinAggregateFields(base_type);
    for (const auto& field : builtin_fields) {
        if (field.first == field_name) {
            return field.second;
        }
    }

    const sema::Type canonical_base = StripMirAliasOrDistinct(module, sema::CanonicalizeBuiltinType(base_type));
    if (canonical_base.kind == sema::Type::Kind::kNamed) {
        if (const auto* type_decl = FindMirTypeDecl(module, canonical_base.name)) {
            const auto fields = InstantiateMirFields(*type_decl, canonical_base);
            for (const auto& field : fields) {
                if (field.first == field_name) {
                    return field.second;
                }
            }
        }
    }

    return std::nullopt;
}

const Function* FindMirFunction(const Module& module, std::string_view name) {
    // Bootstrap MIR still uses linear declaration scans here. Module sizes are
    // small, and a dedicated lookup table can be added when MIR metadata grows.
    for (const auto& function : module.functions) {
        if (function.name == name) {
            return &function;
        }
    }
    return nullptr;
}

sema::Type FunctionProcedureType(const Function& function) {
    std::vector<sema::Type> param_types;
    bool saw_non_parameter = false;
    for (const auto& local : function.locals) {
        if (local.is_parameter) {
            // MIR construction keeps parameters as a contiguous prefix in
            // function.locals so procedure signatures stay deterministic.
            assert(!saw_non_parameter && "FunctionProcedureType expects parameter locals to appear before non-parameters");
            param_types.push_back(local.type);
        } else {
            saw_non_parameter = true;
        }
    }
    return sema::ProcedureType(param_types, function.return_types);
}

bool MatchesGenericFunctionType(const Module& module,
                                const Function& function,
                                const sema::Type& actual_type) {
    if (function.type_params.empty()) {
        return AreEquivalentTypes(module, FunctionProcedureType(function), actual_type);
    }

    const sema::Type canonical_actual = CanonicalMirType(module, actual_type);
    if (canonical_actual.kind != sema::Type::Kind::kProcedure) {
        return false;
    }

    std::unordered_map<std::string, bool> type_param_set;
    for (const auto& type_param : function.type_params) {
        type_param_set.emplace(type_param, true);
    }

    std::unordered_map<std::string, sema::Type> bindings;
    const sema::Type expected_type = FunctionProcedureType(function);
    return MatchGenericTypePattern(module, expected_type, canonical_actual, type_param_set, bindings);
}

const VariantDecl* FindMirVariantDecl(const TypeDecl& type_decl, std::string_view variant_name) {
    // MIR qualified variant spellings use the final '.' to separate the owning
    // type name from the variant leaf, matching current qualified-name rules.
    const std::size_t separator = variant_name.rfind('.');
    const std::string qualified_type = separator == std::string_view::npos ? type_decl.name : std::string(variant_name.substr(0, separator));
    const std::string leaf_name = separator == std::string_view::npos ? std::string(variant_name) : std::string(variant_name.substr(separator + 1));
    if (!qualified_type.empty() && qualified_type != type_decl.name) {
        return nullptr;
    }
    for (const auto& variant : type_decl.variants) {
        if (variant.name == leaf_name) {
            return &variant;
        }
    }
    return nullptr;
}

std::string_view PrimaryTargetName(const Instruction& instruction) {
    return instruction.target_name;
}

namespace {

bool IsBuiltinExecutableErasedNamedType(std::string_view name) {
    return name == "bool" || name == "i8" || name == "u8" || name == "i16" || name == "u16" || name == "i32" ||
           name == "u32" || name == "i64" || name == "u64" || name == "isize" || name == "usize" ||
           name == "uintptr" || name == "f32" || name == "f64" || name == "str" || name == "string" ||
           name == "cstr" || name == "Slice" || name == "Buffer";
}

bool TypeContainsTypeParams(const sema::Type& type,
                           const std::unordered_map<std::string, bool>& type_param_set) {
    if (type.kind == sema::Type::Kind::kNamed && type_param_set.contains(type.name)) {
        return true;
    }

    for (const auto& subtype : type.subtypes) {
        if (TypeContainsTypeParams(subtype, type_param_set)) {
            return true;
        }
    }
    return false;
}

bool TypeNeedsExecutableSpecialization(
    const Module& module,
    const sema::Type& raw_type,
    const std::unordered_map<std::string, bool>& type_param_set) {
    
    const sema::Type type = sema::CanonicalizeBuiltinType(raw_type);
    switch (type.kind) {
        // simple types, never specified
        case sema::Type::Kind::kUnknown:
        case sema::Type::Kind::kVoid:
        case sema::Type::Kind::kBool:
        case sema::Type::Kind::kString:
        case sema::Type::Kind::kNil:
        case sema::Type::Kind::kIntLiteral:
        case sema::Type::Kind::kFloatLiteral:
        case sema::Type::Kind::kPointer:
        case sema::Type::Kind::kProcedure:
            return false;
        // single-subtype wrappers
        case sema::Type::Kind::kConst:
        case sema::Type::Kind::kArray:
        case sema::Type::Kind::kRange:
            return !type.subtypes.empty() && TypeNeedsExecutableSpecialization(module, type.subtypes.front(), type_param_set);
        
        // multi-subtype wrappers
        case sema::Type::Kind::kTuple:
            for (const auto& subtype : type.subtypes) {
                if (TypeNeedsExecutableSpecialization(module, subtype, type_param_set)) {
                    return true;
                }
            }
            return false;
        // named types
        case sema::Type::Kind::kNamed:
            if (type_param_set.contains(type.name)) {
                return true;
            }
            if (IsBuiltinExecutableErasedNamedType(type.name)) {
                return false;
            }
            if (const auto* type_decl = FindMirTypeDecl(module, type.name)) {
                if (type_decl->kind == TypeDecl::Kind::kAlias || type_decl->kind == TypeDecl::Kind::kDistinct) {
                    return TypeNeedsExecutableSpecialization(module, InstantiateMirAliasedType(*type_decl, type), type_param_set);
                }
                if (type_decl->kind == TypeDecl::Kind::kStruct) {
                    const auto fields = InstantiateMirFields(*type_decl, type);
                    for (const auto& field : fields) {
                        if (TypeNeedsExecutableSpecialization(module, field.second, type_param_set)) {
                            return true;
                        }
                    }
                    return false;
                }
                if (type_decl->kind == TypeDecl::Kind::kEnum) {
                    for (const auto& variant : type_decl->variants) {
                        const auto instantiated = InstantiateMirVariantDecl(*type_decl, type, variant.name);
                        if (!instantiated.has_value()) {
                            continue;
                        }
                        for (const auto& payload_field : instantiated->payload_fields) {
                            if (TypeNeedsExecutableSpecialization(module, payload_field.second, type_param_set)) {
                                return true;
                            }
                        }
                    }
                    return false;
                }
            }
            return true;
    }
    return true;
}

bool FunctionNeedsExecutableSpecialization(const Module& module,
                                           const Function& function) {
    if (function.type_params.empty()) {
        return false;
    }

    std::unordered_map<std::string, bool> type_param_set;
    for (const auto& type_param : function.type_params) {
        type_param_set.emplace(type_param, true);
    }

    for (const auto& local : function.locals) {
        if (TypeNeedsExecutableSpecialization(module, local.type, type_param_set)) {
            return true;
        }
    }
    for (const auto& return_type : function.return_types) {
        if (TypeNeedsExecutableSpecialization(module, return_type, type_param_set)) {
            return true;
        }
    }
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (TypeNeedsExecutableSpecialization(module, instruction.type, type_param_set) ||
                TypeNeedsExecutableSpecialization(module, instruction.target_base_type, type_param_set)) {
                return true;
            }
            for (const auto& aux_type : instruction.target_aux_types) {
                if (TypeNeedsExecutableSpecialization(module, aux_type, type_param_set)) {
                    return true;
                }
            }
        }
    }

    return false;
}

std::optional<std::vector<sema::Type>> InferGenericFunctionTypeArgs(const Module& module,
                                                                    const Function& function,
                                                                    const sema::Type& actual_type) {
    if (function.type_params.empty()) {
        return std::vector<sema::Type> {};
    }

    const sema::Type canonical_actual = CanonicalMirType(module, actual_type);
    if (canonical_actual.kind != sema::Type::Kind::kProcedure) {
        return std::nullopt;
    }

    std::unordered_map<std::string, bool> type_param_set;
    for (const auto& type_param : function.type_params) {
        type_param_set.emplace(type_param, true);
    }

    std::unordered_map<std::string, sema::Type> bindings;
    if (!MatchGenericTypePattern(module, FunctionProcedureType(function), canonical_actual, type_param_set, bindings)) {
        return std::nullopt;
    }

    std::vector<sema::Type> type_args;
    type_args.reserve(function.type_params.size());
    for (const auto& type_param : function.type_params) {
        const auto found = bindings.find(type_param);
        if (found == bindings.end()) {
            return std::nullopt;
        }
        type_args.push_back(found->second);
    }
    return type_args;
}

std::string SanitizeSpecializationNamePart(std::string_view text) {
    std::ostringstream sanitized;
    for (const unsigned char ch : text) {
        if (std::isalnum(ch) || ch == '_' || ch == '.') {
            sanitized << static_cast<char>(ch);
            continue;
        }

        constexpr char kHex[] = "0123456789ABCDEF";
        sanitized << '_'
                  << kHex[(ch >> 4) & 0xF]
                  << kHex[ch & 0xF]
                  << '_';
    }
    return sanitized.str();
}

std::string SpecializationName(const Function& function,
                               const std::vector<sema::Type>& type_args) {
    std::ostringstream name;
    name << function.name << "$inst";
    for (const auto& type_arg : type_args) {
        name << '$' << SanitizeSpecializationNamePart(sema::FormatType(type_arg));
    }
    return name.str();
}

Function CloneFunctionSpecialization(const Function& function,
                                     const std::vector<sema::Type>& type_args,
                                     const std::string& specialized_name) {
    Function specialized = function;
    specialized.name = specialized_name;
    specialized.type_params.clear();

    for (auto& local : specialized.locals) {
        local.type = sema::SubstituteTypeParams(std::move(local.type), function.type_params, type_args);
    }
    for (auto& return_type : specialized.return_types) {
        return_type = sema::SubstituteTypeParams(std::move(return_type), function.type_params, type_args);
    }
    for (std::size_t block_index = 0; block_index < specialized.blocks.size(); ++block_index) {
        for (std::size_t instruction_index = 0; instruction_index < specialized.blocks[block_index].instructions.size(); ++instruction_index) {
            auto& instruction = specialized.blocks[block_index].instructions[instruction_index];
            const sema::Type original_type = instruction.type;
            instruction.type = sema::SubstituteTypeParams(std::move(instruction.type), function.type_params, type_args);
            instruction.target_base_type =
                sema::SubstituteTypeParams(std::move(instruction.target_base_type), function.type_params, type_args);
            for (auto& aux_type : instruction.target_aux_types) {
                aux_type = sema::SubstituteTypeParams(std::move(aux_type), function.type_params, type_args);
            }
            if (!instruction.target.empty() && MatchesTargetDisplay(instruction.target, original_type)) {
                instruction.target = sema::FormatType(instruction.type);
            }
        }
    }

    return specialized;
}

std::unordered_map<std::string, sema::Type> BuildInstructionValueTypes(const Function& function) {
    std::unordered_map<std::string, sema::Type> value_types;
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (!instruction.result.empty()) {
                value_types.emplace(instruction.result, instruction.type);
            }
        }
    }
    return value_types;
}

void ReportExecutableGenericSpecializationError(const std::filesystem::path& file_path,
                                                support::DiagnosticSink& diagnostics,
                                                const std::string& message) {
    diagnostics.Report({
        .file_path = file_path,
        .span = {{1, 1}, {1, 1}},
        .severity = support::DiagnosticSeverity::kError,
        .message = message,
    });
}

}  // namespace

std::string CanonicalVariantDisplayName(const sema::Type& selector_type, std::string_view variant_name) {
    if (variant_name.find('.') != std::string_view::npos) {
        return std::string(variant_name);
    }
    if (selector_type.kind == sema::Type::Kind::kNamed && !selector_type.name.empty()) {
        return selector_type.name + "." + std::string(variant_name);
    }
    return std::string(variant_name);
}

bool MatchesTargetDisplay(std::string_view target, const sema::Type& type) {
    const std::string canonical = sema::FormatType(type);
    if (target == canonical) {
        return true;
    }
    if (type.kind == sema::Type::Kind::kNamed && !type.name.empty()) {
        const std::size_t separator = type.name.rfind('.');
        if (separator != std::string::npos && target == type.name.substr(separator + 1)) {
            return true;
        }
    }
    return false;
}

bool SpecializeExecutableGenericFunctions(const Module& module,
                                          const std::filesystem::path& file_path,
                                          support::DiagnosticSink& diagnostics,
                                          Module& specialized_module) {
    specialized_module = module;
    specialized_module.functions.clear();

    std::unordered_map<std::string, std::size_t> function_indices;
    function_indices.reserve(module.functions.size());
    std::queue<std::size_t> rewrite_queue;

    const auto append_function = [&](Function function) {
        const std::size_t index = specialized_module.functions.size();
        function_indices[function.name] = index;
        specialized_module.functions.push_back(std::move(function));
        rewrite_queue.push(index);
    };

    for (const auto& function : module.functions) {
        if (function.type_params.empty() || !FunctionNeedsExecutableSpecialization(module, function)) {
            append_function(function);
        }
    }

    const auto ensure_specialization = [&](const Function& function,
                                           const std::vector<sema::Type>& type_args) -> std::string {
        const std::string specialized_name = SpecializationName(function, type_args);
        if (!function_indices.contains(specialized_name)) {
            append_function(CloneFunctionSpecialization(function, type_args, specialized_name));
        }
        return specialized_name;
    };

    while (!rewrite_queue.empty()) {
        const std::size_t function_index = rewrite_queue.front();
        rewrite_queue.pop();

        Function& function = specialized_module.functions[function_index];
        const std::unordered_map<std::string, sema::Type> value_types = BuildInstructionValueTypes(function);
        std::unordered_map<std::string, bool> owner_type_param_set;
        for (const auto& type_param : function.type_params) {
            owner_type_param_set.emplace(type_param, true);
        }

        for (auto& block : function.blocks) {
            for (auto& instruction : block.instructions) {
                if (instruction.target_kind != Instruction::TargetKind::kFunction || instruction.target_name.empty()) {
                    continue;
                }

                const Function* callee = FindMirFunction(module, instruction.target_name);
                if (callee == nullptr || callee->type_params.empty() || !FunctionNeedsExecutableSpecialization(module, *callee)) {
                    continue;
                }

                sema::Type actual_procedure_type = sema::UnknownType();
                if (instruction.kind == Instruction::Kind::kSymbolRef) {
                    actual_procedure_type = instruction.type;
                } else if (!instruction.operands.empty()) {
                    const auto found = value_types.find(instruction.operands.front());
                    if (found != value_types.end()) {
                        actual_procedure_type = found->second;
                    }
                }

                if (sema::IsUnknown(actual_procedure_type)) {
                    ReportExecutableGenericSpecializationError(
                        file_path,
                        diagnostics,
                        "executable generic specialization requires concrete procedure metadata for generic helper '" +
                            callee->name + "'");
                    return false;
                }

                const auto type_args = InferGenericFunctionTypeArgs(module, *callee, actual_procedure_type);
                if (!type_args.has_value()) {
                    ReportExecutableGenericSpecializationError(
                        file_path,
                        diagnostics,
                        "executable generic specialization could not infer concrete type arguments for generic helper '" +
                            callee->name + "' from procedure type '" + sema::FormatType(actual_procedure_type) + "'");
                    return false;
                }

                bool concrete_type_args = true;
                for (const auto& type_arg : *type_args) {
                    if (TypeContainsTypeParams(type_arg, owner_type_param_set)) {
                        concrete_type_args = false;
                        break;
                    }
                }
                if (!concrete_type_args) {
                    ReportExecutableGenericSpecializationError(
                        file_path,
                        diagnostics,
                        "executable generic specialization requires concrete type arguments for generic helper '" +
                            callee->name + "'; got '" + sema::FormatType(actual_procedure_type) + "'");
                    return false;
                }

                const std::string specialized_name = ensure_specialization(*callee, *type_args);
                instruction.target_name = specialized_name;
                if (instruction.target == callee->name) {
                    instruction.target = specialized_name;
                }
            }
        }
    }

    return true;
}

}  // namespace mc::mir