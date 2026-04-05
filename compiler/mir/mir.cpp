#include "compiler/mir/mir.h"

#include "compiler/mir/mir_internal.h"

#include <optional>
#include <string_view>

namespace mc::mir {

struct SpecialCallEntry {
    std::string_view leaf_name;
    std::string_view module_name;
    SpecialCallKind kind;
};

constexpr SpecialCallEntry kSpecialCallTable[] = {
    {"mmio_ptr", "hal", SpecialCallKind::kMmioPtr},
    {"buffer_new", "mem", SpecialCallKind::kBufferNew},
    {"buffer_free", "mem", SpecialCallKind::kBufferFree},
    {"arena_new", "mem", SpecialCallKind::kArenaNew},
    {"slice_from_buffer", "mem", SpecialCallKind::kSliceFromBuffer},
    {"volatile_load", "hal", SpecialCallKind::kVolatileLoad},
    {"volatile_store", "hal", SpecialCallKind::kVolatileStore},
    {"atomic_load", "sync", SpecialCallKind::kAtomicLoad},
    {"atomic_store", "sync", SpecialCallKind::kAtomicStore},
    {"atomic_exchange", "sync", SpecialCallKind::kAtomicExchange},
    {"atomic_compare_exchange", "sync", SpecialCallKind::kAtomicCompareExchange},
    {"atomic_fetch_add", "sync", SpecialCallKind::kAtomicFetchAdd},
};

bool MatchesSpecialCallName(std::string_view callee_name, const SpecialCallEntry& entry) {
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

std::optional<sema::Type> FindMirFieldType(const Module& module,
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
    for (const auto& local : function.locals) {
        if (local.is_parameter) {
            param_types.push_back(local.type);
        }
    }
    return sema::ProcedureType(param_types, function.return_types);
}

const VariantDecl* FindMirVariantDecl(const TypeDecl& type_decl, std::string_view variant_name) {
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

}  // namespace mc::mir