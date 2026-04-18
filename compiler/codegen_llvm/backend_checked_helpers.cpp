#include "compiler/codegen_llvm/backend_internal.h"

namespace mc::codegen_llvm {
namespace {

struct CheckedHelperAnalysisContext {
    const mir::Module& module;
    const mir::Function& function;
    const mir::BasicBlock& block;
    const std::unordered_map<std::string, sema::Type>& value_types;
    const std::filesystem::path& source_path;
    support::DiagnosticSink& diagnostics;
    CheckedHelperRequirements& requirements;
};

void SeedFunctionValueTypes(const mir::Function& function,
                            std::unordered_map<std::string, sema::Type>& value_types) {
    value_types.clear();
    value_types.reserve(function.locals.size());
    for (const auto& local : function.locals) {
        value_types.emplace(local.name, local.type);
    }
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (!instruction.result.empty()) {
                value_types.insert_or_assign(instruction.result, instruction.type);
            }
        }
    }
}

const sema::Type* LookupCheckedHelperValueType(const CheckedHelperAnalysisContext& context,
                                               std::string_view value_name,
                                               std::string_view check_name) {
    const auto it = context.value_types.find(std::string(value_name));
    if (it != context.value_types.end()) {
        return &it->second;
    }

    ReportBackendError(context.source_path,
                       "LLVM bootstrap executable emission could not resolve " + std::string(check_name) +
                           " operand type for '" + std::string(value_name) + "' in function '" + context.function.name +
                           "' block '" + context.block.label + "'",
                       context.diagnostics);
    return nullptr;
}

std::string CheckedHelperAnalysisContextString(std::string_view check_name,
                                               const CheckedHelperAnalysisContext& context) {
    return "checked-helper analysis for " + std::string(check_name) + " in function '" + context.function.name +
           "' block '" + context.block.label + "'";
}

bool AddDivCheckRequirement(const CheckedHelperAnalysisContext& context,
                            const mir::Instruction& instruction) {
    if (instruction.operands.size() != 2) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission requires div_check to use exactly two operands in function '" +
                               context.function.name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    const sema::Type* rhs_type = LookupCheckedHelperValueType(context, instruction.operands[1], "div_check");
    if (rhs_type == nullptr) {
        return false;
    }

    BackendTypeInfo lowered_type;
    if (!LowerInstructionType(context.module,
                              *rhs_type,
                              context.source_path,
                              context.diagnostics,
                              CheckedHelperAnalysisContextString("div_check", context),
                              lowered_type)) {
        return false;
    }

    context.requirements.div_backend_names.insert(lowered_type.backend_name);
    return true;
}

bool AddShiftCheckRequirement(const CheckedHelperAnalysisContext& context,
                              const mir::Instruction& instruction) {
    if (instruction.operands.size() != 2) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission requires shift_check to use exactly two operands in function '" +
                               context.function.name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    const sema::Type* value_type = LookupCheckedHelperValueType(context, instruction.operands[0], "shift_check");
    if (value_type == nullptr) {
        return false;
    }

    BackendTypeInfo lowered_type;
    if (!LowerInstructionType(context.module,
                              *value_type,
                              context.source_path,
                              context.diagnostics,
                              CheckedHelperAnalysisContextString("shift_check", context),
                              lowered_type)) {
        return false;
    }

    const std::size_t bit_width = IntegerBitWidth(lowered_type);
    if (bit_width == 0) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission requires integer shift_check values in function '" +
                               context.function.name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    context.requirements.shift_widths.insert(bit_width);
    return true;
}

}  // namespace

bool CollectCheckedHelperRequirements(const mir::Module& module,
                                      const std::filesystem::path& source_path,
                                      support::DiagnosticSink& diagnostics,
                                      CheckedHelperRequirements& requirements) {
    std::unordered_map<std::string, sema::Type> value_types;
    for (const auto& function : module.functions) {
        SeedFunctionValueTypes(function, value_types);
        for (const auto& block : function.blocks) {
            const CheckedHelperAnalysisContext context {
                .module = module,
                .function = function,
                .block = block,
                .value_types = value_types,
                .source_path = source_path,
                .diagnostics = diagnostics,
                .requirements = requirements,
            };
            for (const auto& instruction : block.instructions) {
                switch (instruction.kind) {
                    case mir::Instruction::Kind::kDivCheck:
                        if (!AddDivCheckRequirement(context, instruction)) {
                            return false;
                        }
                        break;
                    case mir::Instruction::Kind::kShiftCheck:
                        if (!AddShiftCheckRequirement(context, instruction)) {
                            return false;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    return true;
}

}  // namespace mc::codegen_llvm