#include "compiler/codegen_llvm/backend.h"

#include <string_view>

namespace mc::codegen_llvm {
namespace {

constexpr std::string_view kBootstrapTriple = "arm64-apple-darwin25.4.0";
constexpr std::string_view kBootstrapTargetFamily = "arm64-apple-darwin";
constexpr std::string_view kBootstrapObjectFormat = "macho";

std::string_view ToString(mir::Instruction::Kind kind) {
    switch (kind) {
        case mir::Instruction::Kind::kConst:
            return "const";
        case mir::Instruction::Kind::kLoadLocal:
            return "load_local";
        case mir::Instruction::Kind::kStoreLocal:
            return "store_local";
        case mir::Instruction::Kind::kStoreTarget:
            return "store_target";
        case mir::Instruction::Kind::kSymbolRef:
            return "symbol_ref";
        case mir::Instruction::Kind::kBoundsCheck:
            return "bounds_check";
        case mir::Instruction::Kind::kDivCheck:
            return "div_check";
        case mir::Instruction::Kind::kShiftCheck:
            return "shift_check";
        case mir::Instruction::Kind::kUnary:
            return "unary";
        case mir::Instruction::Kind::kBinary:
            return "binary";
        case mir::Instruction::Kind::kConvert:
            return "convert";
        case mir::Instruction::Kind::kConvertNumeric:
            return "convert_numeric";
        case mir::Instruction::Kind::kConvertDistinct:
            return "convert_distinct";
        case mir::Instruction::Kind::kPointerToInt:
            return "pointer_to_int";
        case mir::Instruction::Kind::kIntToPointer:
            return "int_to_pointer";
        case mir::Instruction::Kind::kArrayToSlice:
            return "array_to_slice";
        case mir::Instruction::Kind::kBufferToSlice:
            return "buffer_to_slice";
        case mir::Instruction::Kind::kCall:
            return "call";
        case mir::Instruction::Kind::kVolatileLoad:
            return "volatile_load";
        case mir::Instruction::Kind::kVolatileStore:
            return "volatile_store";
        case mir::Instruction::Kind::kAtomicLoad:
            return "atomic_load";
        case mir::Instruction::Kind::kAtomicStore:
            return "atomic_store";
        case mir::Instruction::Kind::kAtomicExchange:
            return "atomic_exchange";
        case mir::Instruction::Kind::kAtomicCompareExchange:
            return "atomic_compare_exchange";
        case mir::Instruction::Kind::kAtomicFetchAdd:
            return "atomic_fetch_add";
        case mir::Instruction::Kind::kField:
            return "field";
        case mir::Instruction::Kind::kIndex:
            return "index";
        case mir::Instruction::Kind::kSlice:
            return "slice";
        case mir::Instruction::Kind::kAggregateInit:
            return "aggregate_init";
        case mir::Instruction::Kind::kVariantMatch:
            return "variant_match";
        case mir::Instruction::Kind::kVariantExtract:
            return "variant_extract";
    }

    return "instruction";
}

std::string_view ToString(mir::Instruction::TargetKind kind) {
    switch (kind) {
        case mir::Instruction::TargetKind::kNone:
            return "none";
        case mir::Instruction::TargetKind::kFunction:
            return "function";
        case mir::Instruction::TargetKind::kGlobal:
            return "global";
        case mir::Instruction::TargetKind::kField:
            return "field";
        case mir::Instruction::TargetKind::kDerefField:
            return "deref_field";
        case mir::Instruction::TargetKind::kIndex:
            return "index";
        case mir::Instruction::TargetKind::kOther:
            return "other";
    }

    return "target";
}

std::string_view ToString(mir::Terminator::Kind kind) {
    switch (kind) {
        case mir::Terminator::Kind::kNone:
            return "none";
        case mir::Terminator::Kind::kReturn:
            return "return";
        case mir::Terminator::Kind::kBranch:
            return "branch";
        case mir::Terminator::Kind::kCondBranch:
            return "cond_branch";
    }

    return "terminator";
}

void ReportBackendError(const std::filesystem::path& source_path,
                        const std::string& message,
                        support::DiagnosticSink& diagnostics) {
    diagnostics.Report({
        .file_path = source_path,
        .span = support::kDefaultSourceSpan,
        .severity = support::DiagnosticSeverity::kError,
        .message = message,
    });
}

bool IsSupportedInstruction(const mir::Instruction& instruction,
                            const mir::Function& function,
                            const mir::BasicBlock& block,
                            const std::filesystem::path& source_path,
                            support::DiagnosticSink& diagnostics) {
    switch (instruction.kind) {
        case mir::Instruction::Kind::kConst:
        case mir::Instruction::Kind::kLoadLocal:
        case mir::Instruction::Kind::kStoreLocal:
        case mir::Instruction::Kind::kUnary:
        case mir::Instruction::Kind::kBinary:
            return true;

        case mir::Instruction::Kind::kSymbolRef:
            if (instruction.target_kind == mir::Instruction::TargetKind::kFunction) {
                return true;
            }
            ReportBackendError(source_path,
                               "LLVM bootstrap backend only admits function symbol_ref values in Stage 1; function '" +
                                   function.name + "' block '" + block.label + "' uses target_kind='" +
                                   std::string(ToString(instruction.target_kind)) + "'",
                               diagnostics);
            return false;

        case mir::Instruction::Kind::kCall:
            if (instruction.target_kind == mir::Instruction::TargetKind::kFunction && !instruction.target_name.empty()) {
                return true;
            }
            ReportBackendError(source_path,
                               "LLVM bootstrap backend only admits direct function calls in Stage 1; function '" +
                                   function.name + "' block '" + block.label + "' is not a direct call",
                               diagnostics);
            return false;

        case mir::Instruction::Kind::kStoreTarget:
        case mir::Instruction::Kind::kBoundsCheck:
        case mir::Instruction::Kind::kDivCheck:
        case mir::Instruction::Kind::kShiftCheck:
        case mir::Instruction::Kind::kConvert:
        case mir::Instruction::Kind::kConvertNumeric:
        case mir::Instruction::Kind::kConvertDistinct:
        case mir::Instruction::Kind::kPointerToInt:
        case mir::Instruction::Kind::kIntToPointer:
        case mir::Instruction::Kind::kArrayToSlice:
        case mir::Instruction::Kind::kBufferToSlice:
        case mir::Instruction::Kind::kVolatileLoad:
        case mir::Instruction::Kind::kVolatileStore:
        case mir::Instruction::Kind::kAtomicLoad:
        case mir::Instruction::Kind::kAtomicStore:
        case mir::Instruction::Kind::kAtomicExchange:
        case mir::Instruction::Kind::kAtomicCompareExchange:
        case mir::Instruction::Kind::kAtomicFetchAdd:
        case mir::Instruction::Kind::kField:
        case mir::Instruction::Kind::kIndex:
        case mir::Instruction::Kind::kSlice:
        case mir::Instruction::Kind::kAggregateInit:
        case mir::Instruction::Kind::kVariantMatch:
        case mir::Instruction::Kind::kVariantExtract:
            ReportBackendError(source_path,
                               "LLVM bootstrap backend does not support MIR instruction '" +
                                   std::string(ToString(instruction.kind)) + "' in Stage 1; function '" +
                                   function.name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
    }

    return false;
}

bool IsSupportedTerminator(const mir::Terminator& terminator,
                           const mir::Function& function,
                           const mir::BasicBlock& block,
                           const std::filesystem::path& source_path,
                           support::DiagnosticSink& diagnostics) {
    switch (terminator.kind) {
        case mir::Terminator::Kind::kReturn:
        case mir::Terminator::Kind::kBranch:
        case mir::Terminator::Kind::kCondBranch:
            return true;
        case mir::Terminator::Kind::kNone:
            ReportBackendError(source_path,
                               "LLVM bootstrap backend requires explicit MIR terminators; function '" + function.name +
                                   "' block '" + block.label + "' ends with terminator '" +
                                   std::string(ToString(terminator.kind)) + "'",
                               diagnostics);
            return false;
    }

    return false;
}

bool IsBootstrapTarget(const TargetConfig& target) {
    return target.hosted && target.triple == kBootstrapTriple && target.target_family == kBootstrapTargetFamily &&
           target.object_format == kBootstrapObjectFormat;
}

}  // namespace

TargetConfig BootstrapTargetConfig() {
    return {
        .triple = std::string(kBootstrapTriple),
        .target_family = std::string(kBootstrapTargetFamily),
        .object_format = std::string(kBootstrapObjectFormat),
        .hosted = true,
    };
}

LowerResult LowerModule(const mir::Module& module,
                        const std::filesystem::path& source_path,
                        const LowerOptions& options,
                        support::DiagnosticSink& diagnostics) {
    if (!IsBootstrapTarget(options.target)) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend only supports hosted 'arm64-apple-darwin' in Stage 1; got triple='" +
                               options.target.triple + "' target_family='" + options.target.target_family + "'",
                           diagnostics);
        return {};
    }

    if (!module.globals.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap backend does not lower globals in Stage 1",
                           diagnostics);
        return {};
    }

    auto backend_module = std::make_unique<BackendModule>();
    backend_module->target = options.target;
    backend_module->functions.reserve(module.functions.size());

    for (const auto& function : module.functions) {
        if (function.is_extern) {
            ReportBackendError(source_path,
                               "LLVM bootstrap backend does not lower extern functions in Stage 1; function '" +
                                   function.name + "'",
                               diagnostics);
            return {};
        }

        backend_module->functions.push_back(function.name);
        for (const auto& block : function.blocks) {
            for (const auto& instruction : block.instructions) {
                if (!IsSupportedInstruction(instruction, function, block, source_path, diagnostics)) {
                    return {};
                }
            }

            if (!IsSupportedTerminator(block.terminator, function, block, source_path, diagnostics)) {
                return {};
            }
        }
    }

    return {
        .module = std::move(backend_module),
        .ok = true,
    };
}

}  // namespace mc::codegen_llvm