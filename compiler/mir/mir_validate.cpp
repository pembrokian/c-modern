#include "compiler/mir/mir_internal.h"
#include "compiler/mir/mir_validate_internal.h"

#include <algorithm>
#include <exception>
#include <unordered_set>

namespace mc::mir {

struct ValidationContext {
    const Module& module;
    const Function& function;
    const std::vector<sema::Type>& operand_types;
    const Instruction* previous_instruction;
    const ValidationReporter& report;
};

void ValidateStoreTargetInstruction(const Instruction& instruction, const ValidationContext& ctx) {
    const auto& module = ctx.module;
    const auto& function = ctx.function;
    const auto& operand_types = ctx.operand_types;
    const auto& report = ctx.report;

    if (!instruction.result.empty()) {
        report("store_target must not produce a result in function " + function.name);
    }
    if (instruction.target.empty() && instruction.target_name.empty() && instruction.target_display.empty()) {
        report("store_target must name a target in function " + function.name);
    }
    if (!operand_types.empty() && !sema::IsUnknown(instruction.type) && !IsAssignableType(module, instruction.type, operand_types.front())) {
        report("store_target declared type mismatch in function " + function.name + ": expected " + sema::FormatType(instruction.type) +
               ", got " + sema::FormatType(operand_types.front()));
    }
    switch (instruction.target_kind) {
        case Instruction::TargetKind::kField:
        case Instruction::TargetKind::kDerefField: {
            if (instruction.operands.size() != 2) {
                report("store_target field access must use value and base operands in function " + function.name);
            }
            if (instruction.target_base_storage == Instruction::StorageBaseKind::kNone != instruction.target_base_name.empty()) {
                report("store_target field access direct-base metadata must pair kind and name in function " + function.name);
            }
            if (!instruction.target_aux_types.empty()) {
                report("store_target field access must not carry index metadata in function " + function.name);
            }
            if (instruction.target_name.empty()) {
                report("store_target field access must name a field in function " + function.name);
                break;
            }
            if (operand_types.size() >= 2 && !sema::IsUnknown(instruction.target_base_type) && instruction.target_base_type != operand_types[1]) {
                report("store_target field base metadata mismatch in function " + function.name);
            }
            if (instruction.target_base_type.kind != sema::Type::Kind::kNamed) {
                const auto builtin_fields = sema::BuiltinAggregateFields(instruction.target_base_type);
                if (!sema::IsUnknown(instruction.target_base_type) && builtin_fields.empty()) {
                    report("store_target field access requires named aggregate base in function " + function.name);
                }
                if (builtin_fields.empty()) {
                    break;
                }
            }
            const TypeDecl* type_decl = FindMirTypeDecl(module, instruction.target_base_type.name);
            const auto builtin_fields = sema::BuiltinAggregateFields(instruction.target_base_type);
            if (type_decl == nullptr && builtin_fields.empty()) {
                report("store_target field base type is unknown in function " + function.name + ": " + instruction.target_base_type.name);
                break;
            }
            bool found_field = false;
            const auto instantiated_fields =
                type_decl != nullptr ? InstantiateMirFields(*type_decl, instruction.target_base_type) : std::vector<std::pair<std::string, sema::Type>> {};
            const auto& fields = type_decl != nullptr ? instantiated_fields : builtin_fields;
            for (const auto& field : fields) {
                if (field.first == instruction.target_name) {
                    found_field = true;
                    if (!operand_types.empty() && !IsAssignableType(module, field.second, operand_types.front())) {
                        report("store_target field type mismatch in function " + function.name + " for " + instruction.target_name + ": expected " +
                               sema::FormatType(field.second) + ", got " + sema::FormatType(operand_types.front()));
                    }
                    break;
                }
            }
            if (!found_field) {
                report("store_target field is unknown in function " + function.name + ": " + instruction.target_name);
            }
            break;
        }
        case Instruction::TargetKind::kIndex: {
            if (instruction.operands.size() != 3) {
                report("store_target index access must use value, base, and index operands in function " + function.name);
            }
            if (instruction.target_base_storage == Instruction::StorageBaseKind::kNone != instruction.target_base_name.empty()) {
                report("store_target index access direct-base metadata must pair kind and name in function " + function.name);
            }
            if (!instruction.target_name.empty()) {
                report("store_target index access must not name a field target in function " + function.name);
            }
            if (instruction.target_aux_types.size() != 1) {
                report("store_target index access must carry exactly one index type in function " + function.name);
            }
            if (operand_types.size() >= 2 && !sema::IsUnknown(instruction.target_base_type) && instruction.target_base_type != operand_types[1]) {
                report("store_target index base metadata mismatch in function " + function.name);
            }
            sema::Type element_type = sema::UnknownType();
            if (instruction.target_base_type.kind == sema::Type::Kind::kArray && !instruction.target_base_type.subtypes.empty()) {
                element_type = instruction.target_base_type.subtypes.front();
            }
            if (instruction.target_base_type.kind == sema::Type::Kind::kNamed &&
                (IsNamedTypeFamily(instruction.target_base_type, "Slice") || IsNamedTypeFamily(instruction.target_base_type, "Buffer")) &&
                !instruction.target_base_type.subtypes.empty()) {
                element_type = instruction.target_base_type.subtypes.front();
            }
            if (sema::IsUnknown(element_type) && !sema::IsUnknown(instruction.target_base_type)) {
                report("store_target index requires array, slice, or buffer base in function " + function.name);
            }
            if (!instruction.target_aux_types.empty() && instruction.target_aux_types.front() != sema::NamedType("usize") &&
                instruction.target_aux_types.front().kind != sema::Type::Kind::kIntLiteral) {
                report("store_target index operand must be usize-compatible in function " + function.name);
            }
            if (operand_types.size() >= 3 && !instruction.target_aux_types.empty() && instruction.target_aux_types.front() != operand_types[2]) {
                report("store_target index metadata type mismatch in function " + function.name);
            }
            if (!operand_types.empty() && !sema::IsUnknown(element_type) && !IsAssignableType(module, element_type, operand_types.front())) {
                report("store_target indexed element type mismatch in function " + function.name + ": expected " + sema::FormatType(element_type) +
                       ", got " + sema::FormatType(operand_types.front()));
            }
            break;
        }
        case Instruction::TargetKind::kGlobal:
            if (instruction.operands.size() != 1) {
                report("store_target global access must use exactly one value operand in function " + function.name);
            }
            if (instruction.target_name.empty()) {
                report("store_target global access must name the target symbol in function " + function.name);
            }
            if (!instruction.target_aux_types.empty() || !instruction.target_name.empty() && !sema::IsUnknown(instruction.target_base_type)) {
                report("store_target global access must not carry structured base metadata in function " + function.name);
            }
            break;
        case Instruction::TargetKind::kNone:
        case Instruction::TargetKind::kFunction:
        case Instruction::TargetKind::kOther:
            if (instruction.operands.size() == 2 && instruction.target_base_type.kind == sema::Type::Kind::kPointer &&
                !instruction.target_base_type.subtypes.empty()) {
                if (!operand_types.empty() && !IsAssignableType(module, instruction.target_base_type.subtypes.front(), operand_types.front())) {
                    report("store_target pointer target type mismatch in function " + function.name + ": expected " +
                           sema::FormatType(instruction.target_base_type.subtypes.front()) + ", got " + sema::FormatType(operand_types.front()));
                }
                if (operand_types.size() >= 2 && operand_types[1] != instruction.target_base_type) {
                    report("store_target pointer base metadata mismatch in function " + function.name);
                }
            }
            break;
    }
}

void ValidateBinaryInstruction(const Instruction& instruction, const ValidationContext& ctx) {
    const auto& module = ctx.module;
    const auto& function = ctx.function;
    const auto& operand_types = ctx.operand_types;
    const Instruction* previous_instruction = ctx.previous_instruction;
    const auto& report = ctx.report;

    if (instruction.result.empty()) {
        report("binary must produce a result in function " + function.name);
    }
    if (instruction.operands.size() != 2) {
        report("binary must use exactly two operands in function " + function.name);
    }
    const sema::Type binary_result_type = StripMirAliasOrDistinct(module, instruction.type);
    const bool requires_wrap_semantics =
        IsWraparoundBinaryOp(instruction.op) && !sema::IsUnknown(binary_result_type) && IsIntegerType(module, binary_result_type);
    if (requires_wrap_semantics && instruction.arithmetic_semantics != Instruction::ArithmeticSemantics::kWrap) {
        report("integer arithmetic binary must record wrap semantics in function " + function.name);
    }
    if (instruction.arithmetic_semantics == Instruction::ArithmeticSemantics::kWrap &&
        (!IsWraparoundBinaryOp(instruction.op) || !sema::IsUnknown(binary_result_type) && !IsIntegerType(module, binary_result_type))) {
        report("wrap semantics are only valid on integer +, -, or * binaries in function " + function.name);
    }
    if ((instruction.op == "==" || instruction.op == "!=" || instruction.op == "<" || instruction.op == "<=" || instruction.op == ">" ||
         instruction.op == ">=" || instruction.op == "&&" || instruction.op == "||") &&
        !IsBoolType(module, instruction.type)) {
        report("comparison/logical binary must produce bool in function " + function.name);
    }
    if ((instruction.op == "==" || instruction.op == "!=" || instruction.op == "<" || instruction.op == "<=" || instruction.op == ">" ||
         instruction.op == ">=") &&
        operand_types.size() == 2 && !HasCompatibleComparisonTypes(module, operand_types.front(), operand_types.back())) {
        report("comparison requires compatible operand types in function " + function.name);
    }
    if (instruction.op == "<<" || instruction.op == ">>") {
        if (operand_types.size() == 2) {
            if (!sema::IsUnknown(operand_types.front()) && !IsIntegerLikeType(module, operand_types.front())) {
                report("shift binary requires integer left operand in function " + function.name);
            }
            if (!sema::IsUnknown(operand_types.back()) && !IsIntegerLikeType(module, operand_types.back())) {
                report("shift binary requires integer right operand in function " + function.name);
            }
            if (!sema::IsUnknown(operand_types.front()) && instruction.type != operand_types.front()) {
                report("shift binary must preserve the left operand type in function " + function.name);
            }
        }
        if (previous_instruction == nullptr || previous_instruction->kind != Instruction::Kind::kShiftCheck ||
            previous_instruction->op != instruction.op || previous_instruction->operands != instruction.operands) {
            report("shift binary must be preceded by matching shift_check in function " + function.name);
        }
    }
    if (instruction.op == "&&" || instruction.op == "||") {
        for (const auto& operand_type : operand_types) {
            if (!sema::IsUnknown(operand_type) && !IsBoolType(module, operand_type)) {
                report("logical binary requires bool operands in function " + function.name);
            }
        }
    }
    if (instruction.op == "&" || instruction.op == "|" || instruction.op == "^") {
        for (const auto& operand_type : operand_types) {
            if (!sema::IsUnknown(operand_type) && !IsIntegerLikeType(module, operand_type)) {
                report("bitwise binary requires integer operands in function " + function.name);
            }
        }
        if (operand_types.size() == 2 && !HasCompatibleNumericTypes(module, operand_types.front(), operand_types.back())) {
            report("bitwise binary requires compatible operand types in function " + function.name);
        }
        if (!sema::IsUnknown(instruction.type) && !IsIntegerLikeType(module, instruction.type)) {
            report("bitwise binary must produce integer type in function " + function.name);
        }
    }
    if (instruction.op == "+" || instruction.op == "-" || instruction.op == "*" || instruction.op == "/" || instruction.op == "%") {
        for (const auto& operand_type : operand_types) {
            if (!sema::IsUnknown(operand_type) && !IsNumericType(module, operand_type)) {
                report("arithmetic binary requires numeric operands in function " + function.name);
            }
        }
        if (operand_types.size() == 2 && !HasCompatibleNumericTypes(module, operand_types.front(), operand_types.back())) {
            report("arithmetic binary requires compatible operand types in function " + function.name);
        }
        if (!sema::IsUnknown(instruction.type) && !IsNumericType(module, instruction.type)) {
            report("arithmetic binary must produce numeric type in function " + function.name);
        }
        if ((instruction.op == "/" || instruction.op == "%") &&
            (previous_instruction == nullptr || previous_instruction->kind != Instruction::Kind::kDivCheck ||
             previous_instruction->op != instruction.op || previous_instruction->operands != instruction.operands)) {
            report("division and remainder must be preceded by matching div_check in function " + function.name);
        }
    }
}

enum class AtomicOrderKind {
    kInvalid,
    kRelaxed,
    kAcquire,
    kRelease,
    kAcqRel,
    kSeqCst,
};

AtomicOrderKind ClassifyAtomicOrder(std::string_view order_name) {
    const std::string leaf = VariantLeafName(order_name);
    if (leaf == "Relaxed") {
        return AtomicOrderKind::kRelaxed;
    }
    if (leaf == "Acquire") {
        return AtomicOrderKind::kAcquire;
    }
    if (leaf == "Release") {
        return AtomicOrderKind::kRelease;
    }
    if (leaf == "AcqRel") {
        return AtomicOrderKind::kAcqRel;
    }
    if (leaf == "SeqCst") {
        return AtomicOrderKind::kSeqCst;
    }
    return AtomicOrderKind::kInvalid;
}

bool IsConstantAtomicOrderMetadata(std::string_view order_name) {
    return ClassifyAtomicOrder(order_name) != AtomicOrderKind::kInvalid;
}

bool AtomicOrderAllowedForInstruction(Instruction::Kind kind, std::string_view order_name) {
    const AtomicOrderKind order = ClassifyAtomicOrder(order_name);
    switch (kind) {
        case Instruction::Kind::kAtomicLoad:
            return order == AtomicOrderKind::kRelaxed || order == AtomicOrderKind::kAcquire ||
                   order == AtomicOrderKind::kSeqCst;
        case Instruction::Kind::kAtomicStore:
            return order == AtomicOrderKind::kRelaxed || order == AtomicOrderKind::kRelease ||
                   order == AtomicOrderKind::kSeqCst;
        case Instruction::Kind::kAtomicExchange:
        case Instruction::Kind::kAtomicFetchAdd:
        case Instruction::Kind::kAtomicCompareExchange:
            return order == AtomicOrderKind::kRelaxed || order == AtomicOrderKind::kAcquire ||
                   order == AtomicOrderKind::kRelease || order == AtomicOrderKind::kAcqRel ||
                   order == AtomicOrderKind::kSeqCst;
        default:
            return false;
    }
}

bool AtomicCompareExchangeFailureOrderAllowed(std::string_view success_order_name,
                                              std::string_view failure_order_name) {
    const AtomicOrderKind success = ClassifyAtomicOrder(success_order_name);
    const AtomicOrderKind failure = ClassifyAtomicOrder(failure_order_name);
    switch (success) {
        case AtomicOrderKind::kRelaxed:
            return failure == AtomicOrderKind::kRelaxed;
        case AtomicOrderKind::kAcquire:
            return failure == AtomicOrderKind::kRelaxed || failure == AtomicOrderKind::kAcquire;
        case AtomicOrderKind::kRelease:
            return failure == AtomicOrderKind::kRelaxed;
        case AtomicOrderKind::kAcqRel:
            return failure == AtomicOrderKind::kRelaxed || failure == AtomicOrderKind::kAcquire;
        case AtomicOrderKind::kSeqCst:
            return failure == AtomicOrderKind::kRelaxed || failure == AtomicOrderKind::kAcquire ||
                   failure == AtomicOrderKind::kSeqCst;
        case AtomicOrderKind::kInvalid:
            return false;
    }

    return false;
}

void ValidateAtomicInstruction(const Instruction& instruction, const ValidationContext& ctx) {
    const auto& module = ctx.module;
    const auto& function = ctx.function;
    const auto& operand_types = ctx.operand_types;
    const auto& report = ctx.report;

    const bool is_load = instruction.kind == Instruction::Kind::kAtomicLoad;
    const bool is_store = instruction.kind == Instruction::Kind::kAtomicStore;
    const bool is_exchange = instruction.kind == Instruction::Kind::kAtomicExchange;
    const bool is_compare_exchange = instruction.kind == Instruction::Kind::kAtomicCompareExchange;
    const bool is_fetch_add = instruction.kind == Instruction::Kind::kAtomicFetchAdd;
    const SpecialCallKind expected_special_kind = is_load                 ? SpecialCallKind::kAtomicLoad
                                                : is_store               ? SpecialCallKind::kAtomicStore
                                                : is_exchange            ? SpecialCallKind::kAtomicExchange
                                                : is_compare_exchange    ? SpecialCallKind::kAtomicCompareExchange
                                                                          : SpecialCallKind::kAtomicFetchAdd;
    if (ClassifySpecialCall(PrimaryTargetName(instruction)) != expected_special_kind) {
        report(std::string(ToString(instruction.kind)) + " must use matching atomic call metadata in function " + function.name);
    }
    const std::size_t expected_operands = is_load ? 2 : (is_store || is_exchange || is_fetch_add ? 3 : 5);
    if (instruction.operands.size() != expected_operands) {
        report(std::string(ToString(instruction.kind)) + " operand count mismatch in function " + function.name);
        return;
    }
    if ((is_load || is_store || is_exchange || is_fetch_add) && !HasAtomicOrderMetadata(instruction)) {
        report(std::string(ToString(instruction.kind)) + " must record order metadata in function " + function.name);
    }
    if (is_compare_exchange && !HasCompareExchangeOrderMetadata(instruction)) {
        report("atomic_compare_exchange must record success/failure order metadata in function " + function.name);
    }
    if ((is_load || is_store || is_exchange || is_fetch_add) && HasAtomicOrderMetadata(instruction) &&
        IsConstantAtomicOrderMetadata(instruction.atomic_order) &&
        !AtomicOrderAllowedForInstruction(instruction.kind, instruction.atomic_order)) {
        report(std::string(ToString(instruction.kind)) + " uses unsupported MemoryOrder metadata in function " + function.name);
    }
    if (is_compare_exchange && HasCompareExchangeOrderMetadata(instruction)) {
        if (IsConstantAtomicOrderMetadata(instruction.atomic_success_order) &&
            !AtomicOrderAllowedForInstruction(instruction.kind, instruction.atomic_success_order)) {
            report("atomic_compare_exchange uses unsupported success MemoryOrder metadata in function " + function.name);
        }
        if (IsConstantAtomicOrderMetadata(instruction.atomic_success_order) &&
            IsConstantAtomicOrderMetadata(instruction.atomic_failure_order) &&
            !AtomicCompareExchangeFailureOrderAllowed(instruction.atomic_success_order, instruction.atomic_failure_order)) {
            report("atomic_compare_exchange uses unsupported failure MemoryOrder metadata in function " + function.name);
        }
    }
    if (!instruction.op.empty()) {
        report(std::string(ToString(instruction.kind)) + " must not encode atomic order metadata in generic op text in function " + function.name);
    }
    if ((is_load || is_exchange || is_fetch_add) && instruction.result.empty()) {
        report(std::string(ToString(instruction.kind)) + " must produce a result in function " + function.name);
    }
    if (is_store && !instruction.result.empty()) {
        report("atomic_store must not produce a result in function " + function.name);
    }
    if (is_compare_exchange && !IsBoolType(module, instruction.type)) {
        report("atomic_compare_exchange must produce bool in function " + function.name);
    }
    if (operand_types.empty()) {
        return;
    }
    const auto atomic_element = AtomicElementType(module, operand_types.front());
    if (!atomic_element.has_value()) {
        if (!sema::IsUnknown(operand_types.front())) {
            report(std::string(ToString(instruction.kind)) + " requires *Atomic<T> operand in function " + function.name);
        }
        return;
    }
    const auto require_order = [&](std::size_t index, const std::string& label) {
        if (index < operand_types.size() && !sema::IsUnknown(operand_types[index]) && !IsMemoryOrderType(module, operand_types[index])) {
            report(label + " must use MemoryOrder operand in function " + function.name);
        }
    };
    if (is_load) {
        if (!IsAssignableType(module, instruction.type, *atomic_element)) {
            report("atomic_load result type mismatch in function " + function.name);
        }
        require_order(1, "atomic_load");
    }
    if (is_store) {
        if (operand_types.size() > 1 && !IsAssignableType(module, *atomic_element, operand_types[1])) {
            report("atomic_store value type mismatch in function " + function.name);
        }
        require_order(2, "atomic_store");
    }
    if (is_exchange) {
        if (!IsAssignableType(module, instruction.type, *atomic_element)) {
            report("atomic_exchange result type mismatch in function " + function.name);
        }
        if (operand_types.size() > 1 && !IsAssignableType(module, *atomic_element, operand_types[1])) {
            report("atomic_exchange value type mismatch in function " + function.name);
        }
        require_order(2, "atomic_exchange");
    }
    if (is_compare_exchange) {
        if (operand_types.size() > 1) {
            const auto expected_pointee = PointerPointeeType(StripMirAliasOrDistinct(module, operand_types[1]));
            if (!expected_pointee.has_value() || !IsAssignableType(module, *expected_pointee, *atomic_element)) {
                report("atomic_compare_exchange expected pointer type mismatch in function " + function.name);
            }
        }
        if (operand_types.size() > 2 && !IsAssignableType(module, *atomic_element, operand_types[2])) {
            report("atomic_compare_exchange desired value type mismatch in function " + function.name);
        }
        require_order(3, "atomic_compare_exchange success order");
        require_order(4, "atomic_compare_exchange failure order");
    }
    if (is_fetch_add) {
        if (!IsAssignableType(module, instruction.type, *atomic_element)) {
            report("atomic_fetch_add result type mismatch in function " + function.name);
        }
        if (operand_types.size() > 1 && !IsAssignableType(module, *atomic_element, operand_types[1])) {
            report("atomic_fetch_add value type mismatch in function " + function.name);
        }
        require_order(2, "atomic_fetch_add");
    }
}

// Bootstrap MIR validation runs three ordered passes per non-extern function:
//   1. Block structure and reachability.
//   2. Instruction, operand, and type validation.
//   3. SSA dominance validation.
bool ValidateModule(const Module& module,
                    const std::filesystem::path& file_path,
                    support::DiagnosticSink& diagnostics) {
    bool ok = true;

    auto report = [&](const std::string& message) {
        diagnostics.Report({
            .file_path = file_path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = mc::support::DiagnosticSeverity::kError,
            .message = message,
        });
        ok = false;
    };

    std::unordered_set<std::string> function_names;
    for (const auto& function : module.functions) {
        if (!function_names.insert(function.name).second) {
            report("duplicate MIR function: " + function.name);
        }

        if (function.is_extern) {
            continue;
        }

        std::unordered_map<std::string, const BasicBlock*> blocks_by_label;
        std::unordered_set<std::string> reachable_blocks;
        if (!ValidateFunctionStructure(function, report, blocks_by_label, reachable_blocks)) {
            continue;
        }

        std::unordered_set<std::string> defined_values;
        std::unordered_map<std::string, sema::Type> value_types;
        std::unordered_map<std::string, sema::Type> local_types;
        for (const auto& local : function.locals) {
            local_types[local.name] = local.type;
        }
        for (const auto& block : function.blocks) {
            if (!reachable_blocks.contains(block.label)) {
                continue;
            }

            for (std::size_t instruction_index = 0; instruction_index < block.instructions.size(); ++instruction_index) {
                const auto& instruction = block.instructions[instruction_index];
                const Instruction* previous_instruction = instruction_index == 0 ? nullptr : &block.instructions[instruction_index - 1];
                std::vector<sema::Type> operand_types;
                operand_types.reserve(instruction.operands.size());
                for (const auto& operand : instruction.operands) {
                    if (!operand.empty() && !defined_values.contains(operand)) {
                        report("undefined MIR operand in function " + function.name + ": " + operand);
                        operand_types.push_back(sema::UnknownType());
                        continue;
                    }
                    const auto found_type = value_types.find(operand);
                    operand_types.push_back(found_type == value_types.end() ? sema::UnknownType() : found_type->second);
                }

                const ValidationContext validation_ctx {module, function, operand_types, previous_instruction, report};

                switch (instruction.kind) {
                    case Instruction::Kind::kLocalAddr: {
                        if (instruction.result.empty()) {
                            report("local_addr must produce a result in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("local_addr must name a target in function " + function.name);
                            break;
                        }
                        if (instruction.type.kind != sema::Type::Kind::kPointer || instruction.type.subtypes.empty()) {
                            report("local_addr must produce a pointer type in function " + function.name);
                            break;
                        }
                        if (instruction.target_kind == Instruction::TargetKind::kGlobal) {
                            const std::string_view global_name = PrimaryTargetName(instruction);
                            if (global_name.empty()) {
                                report("local_addr global target must record target_name metadata in function " + function.name);
                                break;
                            }
                            const GlobalDecl* global = FindMirGlobalDecl(module, global_name);
                            if (global == nullptr) {
                                report("local_addr references unknown global in function " + function.name + ": " + std::string(global_name));
                                break;
                            }
                            if (instruction.type.subtypes.front() != global->type) {
                                report("local_addr type mismatch in function " + function.name + " for " + std::string(global_name) + ": expected *" +
                                       sema::FormatType(global->type) + ", got " + sema::FormatType(instruction.type));
                            }
                            break;
                        }
                        if (instruction.target_kind == Instruction::TargetKind::kField) {
                            if (instruction.target_name.empty()) {
                                report("local_addr field target must record target_name metadata in function " + function.name);
                                break;
                            }
                            if (instruction.target_base_storage != Instruction::StorageBaseKind::kLocal &&
                                instruction.target_base_storage != Instruction::StorageBaseKind::kGlobal) {
                                report("local_addr field target must record direct local/global base storage in function " + function.name);
                                break;
                            }
                            if (instruction.target_base_name.empty()) {
                                report("local_addr field target must record a base name in function " + function.name);
                                break;
                            }
                            if (sema::IsUnknown(instruction.target_base_type)) {
                                report("local_addr field target must record a base type in function " + function.name);
                                break;
                            }
                            const auto field_type = FindMirFieldType(module, instruction.target_base_type, instruction.target_name);
                            if (!field_type.has_value()) {
                                report("local_addr could not resolve field '" + instruction.target_name + "' in function " + function.name);
                                break;
                            }
                            if (instruction.type.subtypes.front() != *field_type) {
                                report("local_addr field type mismatch in function " + function.name + " for " + instruction.target + ": expected *" +
                                       sema::FormatType(*field_type) + ", got " + sema::FormatType(instruction.type));
                            }
                            break;
                        }
                        if (instruction.target_kind == Instruction::TargetKind::kIndex) {
                            if (instruction.operands.size() != 2) {
                                report("local_addr index target must use base and index operands in function " + function.name);
                            }
                            if (instruction.target_base_storage == Instruction::StorageBaseKind::kNone != instruction.target_base_name.empty()) {
                                report("local_addr index target direct-base metadata must pair kind and name in function " + function.name);
                            }
                            if (!instruction.target_name.empty()) {
                                report("local_addr index target must not record field-name metadata in function " + function.name);
                            }
                            if (instruction.target_aux_types.size() != 1) {
                                report("local_addr index target must carry exactly one index type in function " + function.name);
                            }
                            if (operand_types.size() >= 1 && !sema::IsUnknown(instruction.target_base_type) && instruction.target_base_type != operand_types[0]) {
                                report("local_addr index base metadata mismatch in function " + function.name);
                            }
                            sema::Type element_type = sema::UnknownType();
                            if (instruction.target_base_type.kind == sema::Type::Kind::kArray && !instruction.target_base_type.subtypes.empty()) {
                                element_type = instruction.target_base_type.subtypes.front();
                            }
                            if (instruction.target_base_type.kind == sema::Type::Kind::kNamed &&
                                (IsNamedTypeFamily(instruction.target_base_type, "Slice") || IsNamedTypeFamily(instruction.target_base_type, "Buffer")) &&
                                !instruction.target_base_type.subtypes.empty()) {
                                element_type = instruction.target_base_type.subtypes.front();
                            }
                            if (sema::IsUnknown(element_type) && !sema::IsUnknown(instruction.target_base_type)) {
                                report("local_addr index requires array, slice, or buffer base in function " + function.name);
                                break;
                            }
                            if (!instruction.target_aux_types.empty() && instruction.target_aux_types.front() != sema::NamedType("usize") &&
                                instruction.target_aux_types.front().kind != sema::Type::Kind::kIntLiteral) {
                                report("local_addr index operand must be usize-compatible in function " + function.name);
                            }
                            if (operand_types.size() >= 2 && !instruction.target_aux_types.empty() && instruction.target_aux_types.front() != operand_types[1]) {
                                report("local_addr index metadata type mismatch in function " + function.name);
                            }
                            if (!sema::IsUnknown(element_type) && instruction.type.subtypes.front() != element_type) {
                                report("local_addr index type mismatch in function " + function.name + " for " + instruction.target + ": expected *" +
                                       sema::FormatType(element_type) + ", got " + sema::FormatType(instruction.type));
                            }
                            break;
                        }
                        if (!instruction.operands.empty()) {
                            report("local_addr must not take operands in function " + function.name);
                            break;
                        }
                        if (instruction.target_kind != Instruction::TargetKind::kNone) {
                            report("local_addr uses unsupported target kind in function " + function.name + ": " +
                                   std::string(ToString(instruction.target_kind)));
                            break;
                        }
                        const auto found_local = local_types.find(instruction.target);
                        if (found_local == local_types.end()) {
                            report("local_addr references unknown local in function " + function.name + ": " + instruction.target);
                            break;
                        }
                        if (instruction.type.subtypes.front() != found_local->second) {
                            report("local_addr type mismatch in function " + function.name + " for " + instruction.target + ": expected *" +
                                   sema::FormatType(found_local->second) + ", got " + sema::FormatType(instruction.type));
                        }
                        break;
                    }
                    case Instruction::Kind::kLoadLocal: {
                        if (instruction.result.empty()) {
                            report("load_local must produce a result in function " + function.name);
                        }
                        if (!instruction.operands.empty()) {
                            report("load_local must not take operands in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("load_local must name a local target in function " + function.name);
                            break;
                        }
                        const auto found_local = local_types.find(instruction.target);
                        if (found_local == local_types.end()) {
                            report("load_local references unknown local in function " + function.name + ": " + instruction.target);
                            break;
                        }
                        if (instruction.type != found_local->second) {
                            report("load_local type mismatch in function " + function.name + " for " + instruction.target + ": expected " +
                                   sema::FormatType(found_local->second) + ", got " + sema::FormatType(instruction.type));
                        }
                        break;
                    }
                    case Instruction::Kind::kStoreLocal: {
                        if (!instruction.result.empty()) {
                            report("store_local must not produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("store_local must use exactly one operand in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("store_local must name a local target in function " + function.name);
                            break;
                        }
                        const auto found_local = local_types.find(instruction.target);
                        if (found_local == local_types.end()) {
                            report("store_local references unknown local in function " + function.name + ": " + instruction.target);
                            break;
                        }
                        if (instruction.type != found_local->second) {
                            report("store_local declared type mismatch in function " + function.name + " for " + instruction.target + ": expected " +
                                   sema::FormatType(found_local->second) + ", got " + sema::FormatType(instruction.type));
                        }
                        if (operand_types.size() == 1 && !IsAssignableType(module, found_local->second, operand_types.front())) {
                            report("store_local operand type mismatch in function " + function.name + " for " + instruction.target + ": expected " +
                                   sema::FormatType(found_local->second) + ", got " + sema::FormatType(operand_types.front()));
                        }
                        break;
                    }
                    case Instruction::Kind::kStoreTarget:
                        ValidateStoreTargetInstruction(instruction, validation_ctx);
                        break;
                    case Instruction::Kind::kUnary: {
                        if (instruction.result.empty()) {
                            report("unary must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("unary must use exactly one operand in function " + function.name);
                        }
                        if (instruction.op == "!" && !IsBoolType(module, instruction.type)) {
                            report("logical unary must produce bool in function " + function.name);
                        }
                        if (instruction.op == "!" && operand_types.size() == 1 && !IsBoolType(module, operand_types.front()) &&
                            !sema::IsUnknown(operand_types.front())) {
                            report("logical unary requires bool operand in function " + function.name);
                        }
                        if (instruction.op == "&" && instruction.type.kind == sema::Type::Kind::kPointer && !instruction.type.subtypes.empty() &&
                            operand_types.size() == 1 && !IsAssignableType(module, instruction.type.subtypes.front(), operand_types.front())) {
                            report("address-of unary result type mismatch in function " + function.name);
                        }
                        if (instruction.op == "&" && !sema::IsUnknown(instruction.type) && instruction.type.kind != sema::Type::Kind::kPointer) {
                            report("address-of unary must produce pointer type in function " + function.name);
                        }
                        const sema::Type deref_operand = operand_types.size() == 1 ? StripMirAliasOrDistinct(module, operand_types.front()) : sema::UnknownType();
                        if (instruction.op == "*" && operand_types.size() == 1 && !sema::IsUnknown(deref_operand) &&
                            deref_operand.kind != sema::Type::Kind::kPointer) {
                            report("pointer dereference requires pointer operand in function " + function.name);
                        }
                        if (instruction.op == "*" && operand_types.size() == 1 && deref_operand.kind == sema::Type::Kind::kPointer &&
                            !deref_operand.subtypes.empty() && instruction.type != deref_operand.subtypes.front()) {
                            report("pointer dereference result type mismatch in function " + function.name);
                        }
                        if (instruction.op == "-" && operand_types.size() == 1) {
                            if (!sema::IsUnknown(operand_types.front()) && !IsNumericType(module, operand_types.front())) {
                                report("numeric unary requires numeric operand in function " + function.name);
                            }
                            if (!sema::IsUnknown(instruction.type) && !IsNumericType(module, instruction.type)) {
                                report("numeric unary must produce numeric type in function " + function.name);
                            }
                        }
                        if (instruction.op != "!" && instruction.op != "&" && instruction.op != "*" && instruction.op != "-") {
                            report("unary uses unsupported opcode in function " + function.name + ": " + instruction.op);
                        }
                        break;
                    }
                    case Instruction::Kind::kBoundsCheck:
                        if (!instruction.result.empty()) {
                            report("bounds_check must not produce a result in function " + function.name);
                        }
                        if (instruction.op == "index") {
                            if (instruction.operands.size() != 2) {
                                report("bounds_check index must use index and length operands in function " + function.name);
                                break;
                            }
                        } else if (instruction.op == "slice") {
                            if (instruction.operands.size() != 2 && instruction.operands.size() != 3) {
                                report("bounds_check slice must use bound/length or start/end/length operands in function " + function.name);
                                break;
                            }
                        } else {
                            report("bounds_check must record index or slice mode in function " + function.name);
                            break;
                        }
                        for (const auto& operand_type : operand_types) {
                            if (!sema::IsUnknown(operand_type) && !IsUsizeCompatibleType(module, operand_type)) {
                                report("bounds_check operands must be usize-compatible in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kDivCheck:
                        if (!instruction.result.empty()) {
                            report("div_check must not produce a result in function " + function.name);
                        }
                        if (instruction.op != "/" && instruction.op != "%") {
                            report("div_check must record '/' or '%' mode in function " + function.name);
                        }
                        if (instruction.operands.size() != 2) {
                            report("div_check must use dividend and divisor operands in function " + function.name);
                            break;
                        }
                        for (const auto& operand_type : operand_types) {
                            if (!sema::IsUnknown(operand_type) && !IsNumericType(module, operand_type)) {
                                report("div_check operands must be numeric in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kShiftCheck:
                        if (!instruction.result.empty()) {
                            report("shift_check must not produce a result in function " + function.name);
                        }
                        if (instruction.op != "<<" && instruction.op != ">>") {
                            report("shift_check must record '<<' or '>>' mode in function " + function.name);
                        }
                        if (instruction.operands.size() != 2) {
                            report("shift_check must use value and count operands in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 2) {
                            if (!sema::IsUnknown(operand_types.front()) && !IsIntegerLikeType(module, operand_types.front())) {
                                report("shift_check value operand must be integer-typed in function " + function.name);
                            }
                            if (!sema::IsUnknown(operand_types.back()) && !IsIntegerLikeType(module, operand_types.back())) {
                                report("shift_check count operand must be integer-typed in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kBinary:
                        ValidateBinaryInstruction(instruction, validation_ctx);
                        break;
                    case Instruction::Kind::kConvert:
                        if (instruction.result.empty()) {
                            report("convert must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("convert must use exactly one operand in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("convert must name a target type in function " + function.name);
                        }
                        if (!sema::IsUnknown(instruction.type) && !MatchesTargetDisplay(instruction.target, instruction.type)) {
                            report("convert target metadata must match result type in function " + function.name);
                        }
                        if (operand_types.size() == 1 &&
                            ClassifyMirConversion(module, operand_types.front(), instruction.type) != ExplicitConversionKind::kGeneric) {
                            report("convert must use a dedicated conversion opcode when one exists in function " + function.name);
                        }
                        break;
                    case Instruction::Kind::kConvertNumeric:
                        if (instruction.result.empty()) {
                            report("convert_numeric must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("convert_numeric must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (instruction.target.empty()) {
                            report("convert_numeric must name a target type in function " + function.name);
                        } else if (!sema::IsUnknown(instruction.type) && !MatchesTargetDisplay(instruction.target, instruction.type)) {
                            report("convert_numeric target metadata must match result type in function " + function.name);
                        }
                        if (operand_types.size() == 1 &&
                            ClassifyMirConversion(module, operand_types.front(), instruction.type) != ExplicitConversionKind::kNumeric) {
                            report("convert_numeric must encode a numeric conversion family in function " + function.name);
                        }
                        if (operand_types.size() == 1 && (!IsNumericType(module, operand_types.front()) || !IsNumericType(module, instruction.type))) {
                            report("convert_numeric requires numeric source and target types in function " + function.name);
                        }
                        break;
                    case Instruction::Kind::kConvertDistinct: {
                        if (instruction.result.empty()) {
                            report("convert_distinct must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("convert_distinct must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (instruction.target.empty()) {
                            report("convert_distinct must name a target type in function " + function.name);
                        } else if (!sema::IsUnknown(instruction.type) && !MatchesTargetDisplay(instruction.target, instruction.type)) {
                            report("convert_distinct target metadata must match result type in function " + function.name);
                        }
                        if (operand_types.size() == 1 &&
                            ClassifyMirConversion(module, operand_types.front(), instruction.type) != ExplicitConversionKind::kDistinct) {
                            report("convert_distinct must encode a distinct or alias conversion family in function " + function.name);
                        }
                        if (operand_types.empty()) {
                            break;
                        }
                        const sema::Type stripped_source = StripMirAliasOrDistinct(module, operand_types.front());
                        const sema::Type stripped_target = StripMirAliasOrDistinct(module, instruction.type);
                        if (stripped_source != stripped_target || (stripped_source == operand_types.front() && stripped_target == instruction.type)) {
                            report("convert_distinct requires representation-preserving distinct or alias conversion in function " + function.name);
                        }
                        break;
                    }
                    case Instruction::Kind::kPointerToInt:
                        if (instruction.result.empty()) {
                            report("pointer_to_int must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("pointer_to_int must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (instruction.target.empty()) {
                            report("pointer_to_int must name a target type in function " + function.name);
                        } else if (!sema::IsUnknown(instruction.type) && !MatchesTargetDisplay(instruction.target, instruction.type)) {
                            report("pointer_to_int target metadata must match result type in function " + function.name);
                        }
                        if (operand_types.size() == 1 &&
                            ClassifyMirConversion(module, operand_types.front(), instruction.type) != ExplicitConversionKind::kPointerToInt) {
                            report("pointer_to_int must encode a pointer-to-uintptr conversion family in function " + function.name);
                        }
                        if (operand_types.size() == 1 && (!IsUintPtrConvertibleType(module, operand_types.front()) || !IsUintPtrType(module, instruction.type))) {
                            report("pointer_to_int requires pointer source and uintptr target in function " + function.name);
                        }
                        break;
                    case Instruction::Kind::kIntToPointer:
                        if (instruction.result.empty()) {
                            report("int_to_pointer must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("int_to_pointer must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (instruction.target.empty()) {
                            report("int_to_pointer must name a target type in function " + function.name);
                        } else if (!sema::IsUnknown(instruction.type) && !MatchesTargetDisplay(instruction.target, instruction.type)) {
                            report("int_to_pointer target metadata must match result type in function " + function.name);
                        }
                        if (operand_types.size() == 1 &&
                            ClassifyMirConversion(module, operand_types.front(), instruction.type) != ExplicitConversionKind::kIntToPointer) {
                            report("int_to_pointer must encode a uintptr-to-pointer conversion family in function " + function.name);
                        }
                        if (operand_types.size() == 1 && (!IsUintPtrType(module, operand_types.front()) || !IsUintPtrConvertibleType(module, instruction.type))) {
                            report("int_to_pointer requires uintptr source and pointer target in function " + function.name);
                        }
                        break;
                    case Instruction::Kind::kArrayToSlice:
                        if (instruction.result.empty()) {
                            report("array_to_slice must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("array_to_slice must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (instruction.target.empty()) {
                            report("array_to_slice must name a target type in function " + function.name);
                        } else if (!sema::IsUnknown(instruction.type) && !MatchesTargetDisplay(instruction.target, instruction.type)) {
                            report("array_to_slice target metadata must match result type in function " + function.name);
                        }
                        if (operand_types.size() == 1 &&
                            ClassifyMirConversion(module, operand_types.front(), instruction.type) != ExplicitConversionKind::kArrayToSlice) {
                            report("array_to_slice must encode an array-to-slice conversion family in function " + function.name);
                        }
                        if (operand_types.size() == 1) {
                            if (operand_types.front().kind != sema::Type::Kind::kArray || instruction.type.kind != sema::Type::Kind::kNamed ||
                                instruction.type.name != "Slice") {
                                report("array_to_slice requires array source and Slice target in function " + function.name);
                            } else if (!operand_types.front().subtypes.empty() && !instruction.type.subtypes.empty() &&
                                       !IsAssignableType(module, instruction.type.subtypes.front(), operand_types.front().subtypes.front())) {
                                report("array_to_slice element type mismatch in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kBufferToSlice:
                        if (instruction.result.empty()) {
                            report("buffer_to_slice must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("buffer_to_slice must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (instruction.target.empty()) {
                            report("buffer_to_slice must name a target type in function " + function.name);
                        } else if (!sema::IsUnknown(instruction.type) && !MatchesTargetDisplay(instruction.target, instruction.type)) {
                            report("buffer_to_slice target metadata must match result type in function " + function.name);
                        }
                        if (operand_types.size() == 1 &&
                            ClassifyMirConversion(module, operand_types.front(), instruction.type) != ExplicitConversionKind::kBufferToSlice) {
                            report("buffer_to_slice must encode a buffer-to-slice conversion family in function " + function.name);
                        }
                        if (operand_types.size() == 1) {
                            if (operand_types.front().kind != sema::Type::Kind::kNamed || !IsNamedTypeFamily(operand_types.front(), "Buffer") ||
                                instruction.type.kind != sema::Type::Kind::kNamed || !IsNamedTypeFamily(instruction.type, "Slice")) {
                                report("buffer_to_slice requires Buffer source and Slice target in function " + function.name);
                            } else if (!operand_types.front().subtypes.empty() && !instruction.type.subtypes.empty() &&
                                       !IsAssignableType(module, instruction.type.subtypes.front(), operand_types.front().subtypes.front())) {
                                report("buffer_to_slice element type mismatch in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kBufferNew: {
                        if (instruction.result.empty()) {
                            report("buffer_new must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 2) {
                            report("buffer_new must use allocator and capacity operands in function " + function.name);
                            break;
                        }
                        const sema::Type pointee = PointerPointeeType(instruction.type).value_or(sema::UnknownType());
                        if (instruction.type.kind != sema::Type::Kind::kPointer || pointee.kind != sema::Type::Kind::kNamed ||
                            !IsNamedTypeFamily(pointee, "Buffer")) {
                            report("buffer_new must produce *Buffer<T> in function " + function.name);
                        }
                        if (operand_types.size() == 2) {
                            const auto allocator_pointee = PointerPointeeType(StripMirAliasOrDistinct(module, operand_types[0]));
                            if (!allocator_pointee.has_value() || !IsNamedTypeFamily(*allocator_pointee, "Allocator")) {
                                report("buffer_new allocator operand must be *Allocator in function " + function.name);
                            }
                            if (!IsIntegerLikeType(module, operand_types[1])) {
                                report("buffer_new capacity operand must be an integer type in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kBufferFree: {
                        if (!instruction.result.empty()) {
                            report("buffer_free must not produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("buffer_free must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 1) {
                            const sema::Type pointee = PointerPointeeType(operand_types.front()).value_or(sema::UnknownType());
                            if (operand_types.front().kind != sema::Type::Kind::kPointer || pointee.kind != sema::Type::Kind::kNamed ||
                                !IsNamedTypeFamily(pointee, "Buffer")) {
                                report("buffer_free requires *Buffer<T> operand in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kArenaNew: {
                        if (instruction.result.empty()) {
                            report("arena_new must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("arena_new must use exactly one arena operand in function " + function.name);
                            break;
                        }
                        const sema::Type pointee = PointerPointeeType(instruction.type).value_or(sema::UnknownType());
                        if (instruction.type.kind != sema::Type::Kind::kPointer || sema::IsUnknown(pointee)) {
                            report("arena_new must produce *T in function " + function.name);
                        }
                        if (operand_types.size() == 1) {
                            const sema::Type arena_operand = StripMirAliasOrDistinct(module, operand_types.front());
                            const auto arena_pointee = PointerPointeeType(arena_operand);
                            const bool named_arena = operand_types.front().kind == sema::Type::Kind::kNamed &&
                                                     IsNamedTypeFamily(operand_types.front(), "Arena");
                            const bool pointer_sized_arena = arena_pointee.has_value() && arena_pointee->kind == sema::Type::Kind::kNamed &&
                                                             arena_pointee->name == "u8";
                            if (!named_arena && !pointer_sized_arena) {
                                report("arena_new requires Arena operand in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kSliceFromBuffer:
                        report("slice_from_buffer should lower to unary '*' plus buffer_to_slice before MIR validation in function " +
                               function.name);
                        break;
                    case Instruction::Kind::kCall: {
                        if (instruction.operands.empty()) {
                            report("call must include a callee operand in function " + function.name);
                            break;
                        }
                        const std::string_view call_target_name = PrimaryTargetName(instruction);
                        if (!call_target_name.empty() && IsDedicatedCallSurface(call_target_name)) {
                            report("call must use a dedicated semantic-boundary opcode when one exists in function " + function.name);
                        }
                        if (instruction.target_kind == Instruction::TargetKind::kFunction && instruction.target_name.empty()) {
                            report("call must record target_name metadata for direct function calls in function " + function.name);
                        }
                        const sema::Type& callee_type = operand_types.front();
                        if (!sema::IsUnknown(callee_type) && callee_type.kind != sema::Type::Kind::kProcedure) {
                            report("call callee must have procedure type in function " + function.name);
                            break;
                        }
                        if (instruction.target_kind == Instruction::TargetKind::kFunction && !instruction.target_name.empty()) {
                            if (const Function* direct_callee = FindMirFunction(module, instruction.target_name); direct_callee != nullptr &&
                                !sema::IsUnknown(callee_type) && !MatchesGenericFunctionType(module, *direct_callee, callee_type)) {
                                report("call target metadata type mismatch in function " + function.name + " for " + direct_callee->name);
                            }
                        }
                        if (callee_type.kind == sema::Type::Kind::kProcedure) {
                            const std::size_t param_count = ProcedureParamCount(callee_type);
                            if (instruction.operands.size() - 1 != param_count) {
                                report("call argument count mismatch in function " + function.name + ": expected " + std::to_string(param_count) +
                                       ", got " + std::to_string(instruction.operands.size() - 1));
                            }
                            for (std::size_t index = 0; index < param_count && index + 1 < operand_types.size(); ++index) {
                                const sema::Type& expected = callee_type.subtypes[index];
                                const sema::Type& actual = operand_types[index + 1];
                                if (!IsAssignableType(module, expected, actual)) {
                                    report("call argument type mismatch in function " + function.name + ": expected " +
                                           sema::FormatType(expected) + ", got " + sema::FormatType(actual));
                                }
                            }
                            if (!AreEquivalentTypes(module, instruction.type, ProcedureResultType(callee_type))) {
                                report("call result type mismatch in function " + function.name + ": expected " +
                                       sema::FormatType(ProcedureResultType(callee_type)) + ", got " + sema::FormatType(instruction.type));
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kVolatileLoad: {
                        if (ClassifySpecialCall(PrimaryTargetName(instruction)) != SpecialCallKind::kVolatileLoad) {
                            report("volatile_load must use volatile_load call metadata in function " + function.name);
                        }
                        if (instruction.result.empty()) {
                            report("volatile_load must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("volatile_load must use exactly one pointer operand in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 1) {
                            const auto pointee = PointerPointeeType(StripMirAliasOrDistinct(module, operand_types.front()));
                            if (!pointee.has_value()) {
                                if (!sema::IsUnknown(operand_types.front())) {
                                    report("volatile_load requires pointer operand in function " + function.name);
                                }
                            } else if (!IsAssignableType(module, instruction.type, *pointee)) {
                                report("volatile_load result type mismatch in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kVolatileStore: {
                        if (ClassifySpecialCall(PrimaryTargetName(instruction)) != SpecialCallKind::kVolatileStore) {
                            report("volatile_store must use volatile_store call metadata in function " + function.name);
                        }
                        if (!instruction.result.empty()) {
                            report("volatile_store must not produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 2) {
                            report("volatile_store must use pointer and value operands in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 2) {
                            const auto pointee = PointerPointeeType(StripMirAliasOrDistinct(module, operand_types.front()));
                            if (!pointee.has_value()) {
                                if (!sema::IsUnknown(operand_types.front())) {
                                    report("volatile_store requires pointer operand in function " + function.name);
                                }
                            } else if (!IsAssignableType(module, *pointee, operand_types[1])) {
                                report("volatile_store value type mismatch in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kAtomicLoad:
                    case Instruction::Kind::kAtomicStore:
                    case Instruction::Kind::kAtomicExchange:
                    case Instruction::Kind::kAtomicCompareExchange:
                    case Instruction::Kind::kAtomicFetchAdd:
                        ValidateAtomicInstruction(instruction, validation_ctx);
                        break;
                    case Instruction::Kind::kField: {
                        if (instruction.result.empty()) {
                            report("field must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("field must use exactly one base operand in function " + function.name);
                            break;
                        }
                        const std::string_view field_name = PrimaryTargetName(instruction);
                        if (field_name.empty()) {
                            report("field must name a member target in function " + function.name);
                            break;
                        }
                        if (instruction.target_kind != Instruction::TargetKind::kNone && instruction.target_kind != Instruction::TargetKind::kField &&
                            instruction.target_kind != Instruction::TargetKind::kDerefField) {
                            report("field must use field or deref_field target metadata in function " + function.name);
                        }
                        const sema::Type& operand_base_type = operand_types.front();
                        const sema::Type base_type = !sema::IsUnknown(instruction.target_base_type) ? instruction.target_base_type : operand_base_type;
                        if (!sema::IsUnknown(instruction.target_base_type) && !sema::IsUnknown(operand_base_type) &&
                            instruction.target_base_type != operand_base_type) {
                            report("field target base metadata mismatch in function " + function.name);
                        }
                        const auto builtin_fields = BuiltinAggregateFields(base_type);
                        bool handled_builtin_field = false;
                        if (!builtin_fields.empty()) {
                            for (const auto& field : builtin_fields) {
                                if (field.first == field_name) {
                                    handled_builtin_field = true;
                                    if (instruction.type != field.second) {
                                        report("field result type mismatch in function " + function.name + " for " + std::string(field_name));
                                    }
                                    break;
                                }
                            }
                            if (!handled_builtin_field && base_type.kind != sema::Type::Kind::kNamed) {
                                report("field references unknown member in function " + function.name + ": " + std::string(field_name));
                            }
                        }
                        if (handled_builtin_field) {
                            break;
                        }
                        if (base_type.kind == sema::Type::Kind::kNamed) {
                            const TypeDecl* type_decl = FindMirTypeDecl(module, base_type.name);
                            if (type_decl != nullptr) {
                                const auto fields = InstantiateMirFields(*type_decl, base_type);
                                bool found_field = false;
                                for (const auto& field : fields) {
                                    if (field.first == field_name) {
                                        found_field = true;
                                        if (instruction.type != field.second) {
                                            report("field result type mismatch in function " + function.name + " for " + std::string(field_name));
                                        }
                                        break;
                                    }
                                }
                                if (!found_field && !type_decl->variants.empty()) {
                                    for (const auto& variant : type_decl->variants) {
                                        if (variant.name == field_name) {
                                            found_field = true;
                                            if (instruction.type != base_type) {
                                                report("field result type mismatch in function " + function.name + " for " + std::string(field_name));
                                            }
                                            break;
                                        }
                                    }
                                }
                                if (!found_field) {
                                    report("field references unknown member in function " + function.name + ": " + std::string(field_name));
                                }
                            } else if (IsBuiltinNamedNonAggregate(module, base_type)) {
                                report("field requires named aggregate base in function " + function.name);
                            }
                        } else if (!sema::IsUnknown(base_type)) {
                            report("field requires named aggregate base in function " + function.name);
                        }
                        break;
                    }
                    case Instruction::Kind::kIndex: {
                        if (instruction.result.empty()) {
                            report("index must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 2) {
                            report("index must use base and index operands in function " + function.name);
                            break;
                        }
                        const sema::Type& operand_base_type = operand_types.front();
                        const sema::Type base_type = !sema::IsUnknown(instruction.target_base_type) ? instruction.target_base_type : operand_base_type;
                        if (!sema::IsUnknown(instruction.target_base_type) && !sema::IsUnknown(operand_base_type) &&
                            instruction.target_base_type != operand_base_type) {
                            report("index target base metadata mismatch in function " + function.name);
                        }
                        sema::Type index_type = operand_types[1];
                        if (!instruction.target_aux_types.empty()) {
                            if (instruction.target_aux_types.size() != 1) {
                                report("index target metadata must carry exactly one index type in function " + function.name);
                            } else {
                                if (!sema::IsUnknown(index_type) && instruction.target_aux_types.front() != index_type) {
                                    report("index target metadata type mismatch in function " + function.name);
                                }
                                index_type = instruction.target_aux_types.front();
                            }
                        }
                        sema::Type expected_type = sema::UnknownType();
                        if (base_type.kind == sema::Type::Kind::kArray && !base_type.subtypes.empty()) {
                            expected_type = base_type.subtypes.front();
                        }
                        if (base_type.kind == sema::Type::Kind::kNamed &&
                            (IsNamedTypeFamily(base_type, "Slice") || IsNamedTypeFamily(base_type, "Buffer")) &&
                            !base_type.subtypes.empty()) {
                            expected_type = base_type.subtypes.front();
                        }
                        if (sema::IsUnknown(expected_type) && !sema::IsUnknown(base_type)) {
                            report("index requires array, slice, or buffer base in function " + function.name);
                        }
                        if (!sema::IsUnknown(expected_type) && instruction.type != expected_type) {
                            report("index result type mismatch in function " + function.name + ": expected " + sema::FormatType(expected_type) +
                                   ", got " + sema::FormatType(instruction.type));
                        }
                        if (!sema::IsUnknown(index_type) && index_type != sema::NamedType("usize") && index_type.kind != sema::Type::Kind::kIntLiteral) {
                            report("index operand must be usize-compatible in function " + function.name);
                        }
                        break;
                    }
                    case Instruction::Kind::kSlice: {
                        if (instruction.result.empty()) {
                            report("slice must produce a result in function " + function.name);
                        }
                        if (instruction.operands.empty() || instruction.operands.size() > 3) {
                            report("slice must use one to three operands in function " + function.name);
                            break;
                        }
                        const sema::Type& operand_base_type = operand_types.front();
                        const sema::Type base_type = !sema::IsUnknown(instruction.target_base_type) ? instruction.target_base_type : operand_base_type;
                        if (!sema::IsUnknown(instruction.target_base_type) && !sema::IsUnknown(operand_base_type) &&
                            instruction.target_base_type != operand_base_type) {
                            report("slice target base metadata mismatch in function " + function.name);
                        }
                        sema::Type expected_type = sema::UnknownType();
                        if (base_type.kind == sema::Type::Kind::kArray && !base_type.subtypes.empty()) {
                            expected_type = sema::NamedType("Slice");
                            expected_type.subtypes.push_back(base_type.subtypes.front());
                        }
                        if (base_type.kind == sema::Type::Kind::kNamed && IsNamedTypeFamily(base_type, "Slice")) {
                            expected_type = base_type;
                        }
                        if (base_type.kind == sema::Type::Kind::kNamed && IsNamedTypeFamily(base_type, "Buffer")) {
                            expected_type = sema::NamedType("Slice");
                            expected_type.subtypes = base_type.subtypes;
                        }
                        if (base_type.kind == sema::Type::Kind::kString) {
                            expected_type = sema::StringType();
                        }
                        if (sema::IsUnknown(expected_type) && !sema::IsUnknown(base_type)) {
                            report("slice requires array, slice, buffer, or string base in function " + function.name);
                        }
                        if (!sema::IsUnknown(expected_type) && instruction.type != expected_type) {
                            report("slice result type mismatch in function " + function.name + ": expected " + sema::FormatType(expected_type) +
                                   ", got " + sema::FormatType(instruction.type));
                        }
                        if (!instruction.target_aux_types.empty() && instruction.target_aux_types.size() != operand_types.size() - 1) {
                            report("slice target metadata must track each bound type in function " + function.name);
                        }
                        for (std::size_t index = 1; index < operand_types.size(); ++index) {
                            sema::Type bound_type = operand_types[index];
                            if (index - 1 < instruction.target_aux_types.size()) {
                                if (!sema::IsUnknown(bound_type) && instruction.target_aux_types[index - 1] != bound_type) {
                                    report("slice target metadata type mismatch in function " + function.name);
                                }
                                bound_type = instruction.target_aux_types[index - 1];
                            }
                            if (!sema::IsUnknown(bound_type) && bound_type != sema::NamedType("usize") &&
                                bound_type.kind != sema::Type::Kind::kIntLiteral) {
                                report("slice bounds must be usize-compatible in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kAggregateInit: {
                        if (instruction.result.empty()) {
                            report("aggregate_init must produce a result in function " + function.name);
                        }
                        if (instruction.field_names.size() != instruction.operands.size()) {
                            report("aggregate_init field metadata mismatch in function " + function.name);
                        }
                        const auto builtin_fields = sema::BuiltinAggregateFields(instruction.type);
                        if (instruction.type.kind != sema::Type::Kind::kNamed && builtin_fields.empty()) {
                            if (!sema::IsUnknown(instruction.type)) {
                                report("aggregate_init must produce a named aggregate type in function " + function.name);
                            }
                            break;
                        }
                        const TypeDecl* type_decl = FindMirTypeDecl(module, instruction.type.name);
                        if (type_decl == nullptr && builtin_fields.empty()) {
                            report("aggregate_init references unknown type in function " + function.name + ": " + instruction.type.name);
                        } else {
                            if (!instruction.target.empty() && !MatchesTargetDisplay(instruction.target, instruction.type)) {
                                report("aggregate_init target metadata must match result type in function " + function.name);
                            }
                            const auto fields =
                                type_decl != nullptr ? InstantiateMirFields(*type_decl, instruction.type) : std::vector<std::pair<std::string, sema::Type>> {};
                            std::unordered_set<std::string> seen_named_fields;
                            for (std::size_t index = 0; index < instruction.field_names.size() && index < operand_types.size(); ++index) {
                                const std::string& field_name = instruction.field_names[index];
                                sema::Type expected_type = sema::UnknownType();
                                if (field_name == "_") {
                                    const std::size_t field_count = type_decl != nullptr ? fields.size() : builtin_fields.size();
                                    if (index >= field_count) {
                                        report("aggregate_init has too many positional fields in function " + function.name);
                                        continue;
                                    }
                                    expected_type = type_decl != nullptr ? fields[index].second : builtin_fields[index].second;
                                } else {
                                    if (!seen_named_fields.insert(field_name).second) {
                                        report("aggregate_init has duplicate named field in function " + function.name + ": " + field_name);
                                        continue;
                                    }
                                    const auto& candidate_fields = type_decl != nullptr ? fields : builtin_fields;
                                    for (const auto& field : candidate_fields) {
                                        if (field.first == field_name) {
                                            expected_type = field.second;
                                            break;
                                        }
                                    }
                                    if (sema::IsUnknown(expected_type)) {
                                        report("aggregate_init field is unknown in function " + function.name + ": " + field_name);
                                        continue;
                                    }
                                }
                                if (!IsAssignableType(module, expected_type, operand_types[index])) {
                                    report("aggregate_init field type mismatch in function " + function.name + " for " + field_name + ": expected " +
                                           sema::FormatType(expected_type) + ", got " + sema::FormatType(operand_types[index]));
                                }
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kVariantInit: {
                        if (instruction.result.empty()) {
                            report("variant_init must produce a result in function " + function.name);
                        }
                        const std::string_view variant_name = PrimaryTargetName(instruction);
                        if (variant_name.empty()) {
                            report("variant_init must name a target variant in function " + function.name);
                        }
                        if (instruction.type.kind != sema::Type::Kind::kNamed) {
                            report("variant_init must produce a named enum type in function " + function.name);
                            break;
                        }
                        if (!sema::IsUnknown(instruction.target_base_type) && instruction.target_base_type != instruction.type) {
                            report("variant_init target metadata must match result type in function " + function.name);
                        }
                        const TypeDecl* type_decl = FindMirTypeDecl(module, instruction.type.name);
                        if (type_decl == nullptr) {
                            report("variant_init references unknown type in function " + function.name + ": " + instruction.type.name);
                            break;
                        }
                        if (type_decl->kind != TypeDecl::Kind::kEnum) {
                            report("variant_init requires enum result type in function " + function.name);
                            break;
                        }
                        const auto variant_decl = InstantiateMirVariantDecl(*type_decl, instruction.type, variant_name);
                        if (!variant_decl.has_value()) {
                            report("variant_init references unknown variant in function " + function.name + ": " + std::string(variant_name));
                            break;
                        }
                        if (instruction.target_index >= 0) {
                            const std::size_t variant_index = static_cast<std::size_t>(instruction.target_index);
                            if (variant_index >= type_decl->variants.size() || type_decl->variants[variant_index].name != variant_decl->name) {
                                report("variant_init variant metadata mismatch in function " + function.name);
                            }
                        }
                        if (instruction.operands.size() != variant_decl->payload_fields.size()) {
                            report("variant_init operand count mismatch in function " + function.name);
                            break;
                        }
                        for (std::size_t index = 0; index < operand_types.size() && index < variant_decl->payload_fields.size(); ++index) {
                            if (!IsAssignableType(module, variant_decl->payload_fields[index].second, operand_types[index])) {
                                report("variant_init operand type mismatch in function " + function.name + " for " + variant_decl->name +
                                       ": expected " + sema::FormatType(variant_decl->payload_fields[index].second) + ", got " +
                                       sema::FormatType(operand_types[index]));
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kVariantMatch: {
                        if (instruction.result.empty()) {
                            report("variant_match must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("variant_match must use exactly one selector operand in function " + function.name);
                        }
                        const std::string_view variant_name = PrimaryTargetName(instruction);
                        if (variant_name.empty()) {
                            report("variant_match must name a target variant in function " + function.name);
                        }
                        if (instruction.type != sema::BoolType()) {
                            report("variant_match must produce bool in function " + function.name);
                        }
                        const sema::Type selector_type = !sema::IsUnknown(instruction.target_base_type) ? instruction.target_base_type
                                                                                                        : (operand_types.empty()
                                                                                                               ? sema::UnknownType()
                                                                                                               : operand_types.front());
                        if (operand_types.size() == 1 && !sema::IsUnknown(instruction.target_base_type) &&
                            !sema::IsUnknown(operand_types.front()) && instruction.target_base_type != operand_types.front()) {
                            report("variant_match selector metadata mismatch in function " + function.name);
                        }
                        if (selector_type.kind == sema::Type::Kind::kNamed) {
                            const TypeDecl* type_decl = FindMirTypeDecl(module, selector_type.name);
                            if (type_decl == nullptr) {
                                report("variant_match selector type is unknown in function " + function.name + ": " + selector_type.name);
                            } else if (type_decl->kind != TypeDecl::Kind::kEnum) {
                                report("variant_match requires enum selector type in function " + function.name);
                            } else if (FindMirVariantDecl(*type_decl, variant_name) == nullptr) {
                                report("variant_match references unknown variant in function " + function.name + ": " + std::string(variant_name));
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kVariantExtract: {
                        if (instruction.result.empty()) {
                            report("variant_extract must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("variant_extract must use exactly one selector operand in function " + function.name);
                        }
                        const std::string_view variant_name = PrimaryTargetName(instruction);
                        if (variant_name.empty()) {
                            report("variant_extract must name a target variant in function " + function.name);
                        }
                        if (instruction.op.empty() && instruction.target_index < 0) {
                            report("variant_extract must record a payload index in function " + function.name);
                        }
                        bool payload_index_ok = instruction.target_index >= 0;
                        std::size_t payload_index = instruction.target_index >= 0 ? static_cast<std::size_t>(instruction.target_index) : 0;
                        if (!instruction.op.empty()) {
                            try {
                                const std::size_t encoded_payload_index = static_cast<std::size_t>(std::stoul(instruction.op));
                                if (payload_index_ok && encoded_payload_index != payload_index) {
                                    report("variant_extract payload metadata mismatch in function " + function.name);
                                }
                                payload_index = encoded_payload_index;
                                payload_index_ok = true;
                            } catch (const std::exception&) {
                                report("variant_extract payload index must be numeric in function " + function.name);
                            }
                        }
                        const sema::Type selector_type = !sema::IsUnknown(instruction.target_base_type) ? instruction.target_base_type
                                                                                                        : (operand_types.empty()
                                                                                                               ? sema::UnknownType()
                                                                                                               : operand_types.front());
                        if (operand_types.size() == 1 && !sema::IsUnknown(instruction.target_base_type) &&
                            !sema::IsUnknown(operand_types.front()) && instruction.target_base_type != operand_types.front()) {
                            report("variant_extract selector metadata mismatch in function " + function.name);
                        }
                        if (selector_type.kind == sema::Type::Kind::kNamed) {
                            const TypeDecl* type_decl = FindMirTypeDecl(module, selector_type.name);
                            if (type_decl == nullptr) {
                                report("variant_extract selector type is unknown in function " + function.name + ": " + selector_type.name);
                                break;
                            }
                            if (type_decl->kind != TypeDecl::Kind::kEnum) {
                                report("variant_extract requires enum selector type in function " + function.name);
                                break;
                            }
                            const auto variant_decl = InstantiateMirVariantDecl(*type_decl, selector_type, variant_name);
                            if (!variant_decl.has_value()) {
                                report("variant_extract references unknown variant in function " + function.name + ": " + std::string(variant_name));
                                break;
                            }
                            if (!payload_index_ok) {
                                break;
                            }
                            if (payload_index >= variant_decl->payload_fields.size()) {
                                report("variant_extract payload index out of range in function " + function.name + " for " + std::string(variant_name));
                                break;
                            }
                            if (instruction.type != variant_decl->payload_fields[payload_index].second) {
                                report("variant_extract result type mismatch in function " + function.name + " for " + std::string(variant_name) + ": expected " +
                                       sema::FormatType(variant_decl->payload_fields[payload_index].second) + ", got " +
                                       sema::FormatType(instruction.type));
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kConst:
                        if (instruction.result.empty()) {
                            report("const must produce a result in function " + function.name);
                        }
                        if (!instruction.operands.empty()) {
                            report("const must not take operands in function " + function.name);
                        }
                        if (instruction.op.empty()) {
                            report("const must record literal payload text in function " + function.name);
                        }
                        break;
                    case Instruction::Kind::kSymbolRef: {
                        if (instruction.result.empty()) {
                            report("symbol_ref must produce a result in function " + function.name);
                        }
                        if (!instruction.operands.empty()) {
                            report("symbol_ref must not take operands in function " + function.name);
                        }
                        const std::string_view symbol_name = PrimaryTargetName(instruction);
                        if (symbol_name.empty()) {
                            report("symbol_ref must name a target symbol in function " + function.name);
                        }
                        if (instruction.target_kind == Instruction::TargetKind::kNone) {
                            report("symbol_ref must record a target kind in function " + function.name);
                        }
                        if ((instruction.target_kind == Instruction::TargetKind::kFunction || instruction.target_kind == Instruction::TargetKind::kGlobal) &&
                            instruction.target_name.empty()) {
                            report("symbol_ref must record target_name metadata for known symbols in function " + function.name);
                        }
                        if (instruction.target_kind == Instruction::TargetKind::kFunction) {
                            const Function* callee = FindMirFunction(module, symbol_name);
                            if (callee != nullptr && !MatchesGenericFunctionType(module, *callee, instruction.type)) {
                                report("symbol_ref function type mismatch in function " + function.name + " for " + callee->name + ": expected " +
                                       sema::FormatType(FunctionProcedureType(*callee)) + ", got " + sema::FormatType(instruction.type));
                            }
                        }
                        if (instruction.target_kind == Instruction::TargetKind::kGlobal) {
                            const GlobalDecl* global = FindMirGlobalDecl(module, symbol_name);
                            if (global != nullptr && instruction.type != global->type) {
                                report("symbol_ref global type mismatch in function " + function.name + " for " +
                                       std::string(symbol_name) + ": expected " +
                                       sema::FormatType(global->type) + ", got " + sema::FormatType(instruction.type));
                            }
                        }
                        break;
                    }
                }

                if (!instruction.result.empty()) {
                    if (!defined_values.insert(instruction.result).second) {
                        report("duplicate MIR value in function " + function.name + ": " + instruction.result);
                    }
                    value_types[instruction.result] = instruction.type;
                }
            }

            switch (block.terminator.kind) {
                case Terminator::Kind::kNone:
                    break;
                case Terminator::Kind::kReturn:
                    if (block.terminator.values.size() != function.return_types.size()) {
                        report("return value count mismatch in function " + function.name + ": expected " +
                               std::to_string(function.return_types.size()) + ", got " + std::to_string(block.terminator.values.size()));
                    }
                    for (std::size_t index = 0; index < block.terminator.values.size(); ++index) {
                        const auto& value = block.terminator.values[index];
                        if (!defined_values.contains(value)) {
                            report("return uses undefined MIR value in function " + function.name + ": " + value);
                            continue;
                        }
                        if (index < function.return_types.size()) {
                            const auto found_type = value_types.find(value);
                            const sema::Type actual = found_type == value_types.end() ? sema::UnknownType() : found_type->second;
                            if (!IsAssignableType(module, function.return_types[index], actual)) {
                                report("return type mismatch in function " + function.name + ": expected " +
                                       sema::FormatType(function.return_types[index]) + ", got " + sema::FormatType(actual));
                            }
                        }
                    }
                    break;
                case Terminator::Kind::kBranch:
                    break;
                case Terminator::Kind::kCondBranch:
                    if (block.terminator.values.size() == 1 && !defined_values.contains(block.terminator.values.front())) {
                        report("conditional branch must use one defined MIR condition in function " + function.name);
                    } else if (block.terminator.values.size() == 1) {
                        const auto found_type = value_types.find(block.terminator.values.front());
                        const sema::Type condition_type = found_type == value_types.end() ? sema::UnknownType() : found_type->second;
                        if (!sema::IsUnknown(condition_type) && !IsBoolType(module, condition_type)) {
                            report("conditional branch condition must have bool type in function " + function.name);
                        }
                    }
                    break;
            }
        }

        ValidateFunctionDominance(function, blocks_by_label, reachable_blocks, report);
    }

    return ok;
}


}  // namespace mc::mir
