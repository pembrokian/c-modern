#include "compiler/codegen_llvm/backend_internal.h"

#include <algorithm>
#include <cctype>

namespace mc::codegen_llvm {

bool ResolveEnumInstructionLayout(const mir::Instruction& instruction,
                                  const mir::Module& module,
                                  const std::filesystem::path& source_path,
                                  support::DiagnosticSink& diagnostics,
                                  const std::string& context,
                                  EnumBackendLayout& layout,
                                  std::size_t& variant_index) {
    const sema::Type enum_type = instruction.kind == mir::Instruction::Kind::kVariantInit ? instruction.type : instruction.target_base_type;
    if (enum_type.kind != sema::Type::Kind::kNamed) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires named enum metadata for " + context,
                           diagnostics);
        return false;
    }

    const mir::TypeDecl* type_decl = FindTypeDecl(module, enum_type.name);
    if (type_decl == nullptr || type_decl->kind != mir::TypeDecl::Kind::kEnum) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission could not resolve enum layout for type '" +
                               sema::FormatType(enum_type) + "' in " + context,
                           diagnostics);
        return false;
    }

    const auto lowered_layout = LowerEnumLayout(module, *type_decl, enum_type);
    if (!lowered_layout.has_value()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission could not lower enum layout for type '" +
                               sema::FormatType(enum_type) + "' in " + context,
                           diagnostics);
        return false;
    }
    layout = *lowered_layout;

    const mir::VariantDecl* variant_decl =
        FindVariantDecl(*type_decl, instruction.target_name.empty() ? instruction.target : instruction.target_name, &variant_index);
    if (variant_decl == nullptr) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission references unknown enum variant '" +
                               (instruction.target_name.empty() ? instruction.target : instruction.target_name) +
                               "' in " + context,
                           diagnostics);
        return false;
    }
    if (variant_index >= layout.variant_field_types.size() || variant_index >= layout.variant_payload_offsets.size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission produced inconsistent enum layout metadata for type '" +
                               sema::FormatType(enum_type) + "' in " + context,
                           diagnostics);
        return false;
    }
    return true;
}

std::string EmitEnumPayloadFieldPointer(const std::string& enum_slot,
                                        const EnumBackendLayout& layout,
                                        std::size_t payload_offset,
                                        std::size_t function_index,
                                        std::size_t block_index,
                                        std::size_t instruction_index,
                                        std::string_view suffix,
                                        std::vector<std::string>& output_lines) {
    const std::string payload_slot = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix) + ".payload";
    output_lines.push_back(payload_slot + " = getelementptr inbounds " + layout.aggregate_type.backend_name + ", ptr " + enum_slot + ", i64 0, i32 1");
    if (payload_offset == 0) {
        return payload_slot;
    }

    const std::string field_slot = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix) + ".field";
    output_lines.push_back(field_slot + " = getelementptr inbounds " + layout.payload_type.backend_name + ", ptr " + payload_slot + ", i64 0, i64 " +
                           std::to_string(payload_offset));
    return field_slot;
}

bool EmitAggregateInitInstruction(const mir::Instruction& instruction,
                                  const ExecutableEmissionContext& context) {
    const std::size_t function_index = context.function_index;
    const std::size_t block_index = context.block_index;
    const std::size_t instruction_index = context.instruction_index;
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    BackendTypeInfo aggregate_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("aggregate_init", state.function->name, block),
                              aggregate_type)) {
        return false;
    }

    std::vector<BackendTypeInfo> field_types;
    field_types.reserve(instruction.operands.size());
    const auto builtin_fields = sema::BuiltinAggregateFields(instruction.type);
    const bool is_array_aggregate = instruction.type.kind == sema::Type::Kind::kArray;
    if (instruction.type.kind == sema::Type::Kind::kNamed || !builtin_fields.empty()) {
        const mir::TypeDecl* type_decl = FindTypeDecl(*state.module, instruction.type.name);
        const auto field_count = type_decl != nullptr ? type_decl->fields.size() : builtin_fields.size();
        if ((type_decl != nullptr && type_decl->kind != mir::TypeDecl::Kind::kStruct) || field_count != instruction.operands.size()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission could not resolve aggregate_init field layout for type '" +
                                   sema::FormatType(instruction.type) + "' in function '" + state.function->name +
                                   "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        const auto& fields = type_decl != nullptr ? type_decl->fields : builtin_fields;
        for (const auto& field : fields) {
            const auto lowered_field = LowerAggregateFieldStorageType(*state.module, instruction.type, field.first);
            if (!lowered_field.has_value()) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not lower aggregate_init storage for field '" + field.first +
                                       "' in function '" + state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
            field_types.push_back(*lowered_field);
        }
    } else if (is_array_aggregate) {
        if (instruction.type.subtypes.empty()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission requires an array element type for aggregate_init in function '" +
                                   state.function->name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        const auto length = mc::support::ParseArrayLength(instruction.type.metadata);
        if (!length.has_value() || *length != instruction.operands.size()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission requires exact array aggregate_init element count in function '" +
                                   state.function->name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        for (const auto& field_name : instruction.field_names) {
            if (field_name != "_") {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission requires positional array aggregate_init elements in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }
        }
        const auto element_type = LowerTypeInfo(*state.module, instruction.type.subtypes.front());
        if (!element_type.has_value()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission could not lower aggregate_init array element type '" +
                                   sema::FormatType(instruction.type.subtypes.front()) + "' in function '" + state.function->name +
                                   "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        field_types.assign(instruction.operands.size(), *element_type);
    } else {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission only supports named-struct aggregate_init in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    std::string current_value = "zeroinitializer";
    for (std::size_t index = 0; index < instruction.operands.size(); ++index) {
        ExecutableValue operand;
        if (!ResolveExecutableValue(state, instruction.operands[index], block, source_path, diagnostics, operand)) {
            return false;
        }

        std::string coerced_operand = operand.text;
        if ((operand.type.backend_name == "i1" && field_types[index].backend_name == "i8") ||
            (operand.type.backend_name == "i8" && field_types[index].backend_name == "i1")) {
            if (!EmitValueRepresentationCoercion(operand,
                                                 field_types[index],
                                                 function_index,
                                                 block_index,
                                                 instruction_index,
                                                 "agg_coerce" + std::to_string(index),
                                                 output_lines,
                                                 coerced_operand)) {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission could not coerce aggregate_init operand " +
                                       std::to_string(index) + " in function '" + state.function->name + "' block '" +
                                       block.label + "'",
                                   diagnostics);
                return false;
            }
        }

        const std::string next_value = index + 1 == instruction.operands.size()
                                           ? LLVMTempName(function_index, block_index, instruction_index)
                                           : LLVMTempName(function_index, block_index, instruction_index) + ".agg" + std::to_string(index);
        output_lines.push_back(next_value + " = insertvalue " + aggregate_type.backend_name + " " + current_value + ", " +
                               field_types[index].backend_name + " " + coerced_operand + ", " + std::to_string(index));
        current_value = next_value;
    }

    RecordExecutableValue(state, instruction.result, current_value, aggregate_type, std::nullopt, instruction.type);
    return true;
}

bool EmitVariantInitInstruction(const mir::Instruction& instruction,
                                const ExecutableEmissionContext& context) {
    const std::size_t function_index = context.function_index;
    const std::size_t block_index = context.block_index;
    const std::size_t instruction_index = context.instruction_index;
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }
    EnumBackendLayout layout;
    std::size_t variant_index = 0;
    if (!ResolveEnumInstructionLayout(instruction,
                                      *state.module,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("variant_init", state.function->name, block),
                                      layout,
                                      variant_index)) {
        return false;
    }
    if (instruction.operands.size() != layout.variant_field_types[variant_index].size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission found enum payload operand mismatch in " +
                               FunctionBlockContext("variant_init", state.function->name, block),
                           diagnostics);
        return false;
    }

    const std::string enum_slot = LLVMTempName(function_index, block_index, instruction_index) + ".variant.slot";
    output_lines.push_back(enum_slot + " = alloca " + layout.aggregate_type.backend_name + ", align " +
                           std::to_string(layout.aggregate_type.alignment));
    output_lines.push_back("store " + layout.aggregate_type.backend_name + " zeroinitializer, ptr " + enum_slot + ", align " +
                           std::to_string(layout.aggregate_type.alignment));

    const std::string tag_slot = LLVMTempName(function_index, block_index, instruction_index) + ".variant.tag";
    output_lines.push_back(tag_slot + " = getelementptr inbounds " + layout.aggregate_type.backend_name + ", ptr " + enum_slot + ", i64 0, i32 0");
    output_lines.push_back("store i64 " + std::to_string(variant_index) + ", ptr " + tag_slot + ", align 8");

    for (std::size_t index = 0; index < instruction.operands.size(); ++index) {
        ExecutableValue operand;
        if (!ResolveExecutableValue(state, instruction.operands[index], block, source_path, diagnostics, operand)) {
            return false;
        }

        const std::string field_slot = EmitEnumPayloadFieldPointer(enum_slot,
                                                                   layout,
                                                                   layout.variant_payload_offsets[variant_index][index],
                                                                   function_index,
                                                                   block_index,
                                                                   instruction_index,
                                                                   "variant_init." + std::to_string(index),
                                                                   output_lines);
        output_lines.push_back("store " + layout.variant_field_types[variant_index][index].backend_name + " " + operand.text + ", ptr " +
                               field_slot + ", align " +
                               std::to_string(layout.variant_field_types[variant_index][index].alignment));
    }

    const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
    output_lines.push_back(result_temp + " = load " + layout.aggregate_type.backend_name + ", ptr " + enum_slot + ", align " +
                           std::to_string(layout.aggregate_type.alignment));
    RecordExecutableValue(state, instruction.result, result_temp, layout.aggregate_type, std::nullopt, instruction.type);
    return true;
}

bool EmitVariantMatchInstruction(const mir::Instruction& instruction,
                                 const ExecutableEmissionContext& context) {
    const std::size_t function_index = context.function_index;
    const std::size_t block_index = context.block_index;
    const std::size_t instruction_index = context.instruction_index;
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue selector;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "variant_match",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, selector)) {
        return false;
    }
    EnumBackendLayout layout;
    std::size_t variant_index = 0;
    if (!ResolveEnumInstructionLayout(instruction,
                                      *state.module,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("variant_match", state.function->name, block),
                                      layout,
                                      variant_index)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("variant_match", state.function->name, block),
                              result_type)) {
        return false;
    }

    const std::string tag_temp = LLVMTempName(function_index, block_index, instruction_index) + ".tag";
    const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
    output_lines.push_back(tag_temp + " = extractvalue " + layout.aggregate_type.backend_name + " " + selector.text + ", 0");
    output_lines.push_back(result_temp + " = icmp eq i64 " + tag_temp + ", " + std::to_string(variant_index));
    RecordExecutableValue(state, instruction.result, result_temp, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitVariantExtractInstruction(const mir::Instruction& instruction,
                                   const ExecutableEmissionContext& context) {
    const std::size_t function_index = context.function_index;
    const std::size_t block_index = context.block_index;
    const std::size_t instruction_index = context.instruction_index;
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue selector;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "variant_extract",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, selector)) {
        return false;
    }
    EnumBackendLayout layout;
    std::size_t variant_index = 0;
    if (!ResolveEnumInstructionLayout(instruction,
                                      *state.module,
                                      source_path,
                                      diagnostics,
                                      FunctionBlockContext("variant_extract", state.function->name, block),
                                      layout,
                                      variant_index)) {
        return false;
    }

    std::size_t payload_index = instruction.target_index >= 0 ? static_cast<std::size_t>(instruction.target_index) : 0;
    if (instruction.target_index < 0 && !instruction.op.empty()) {
        if (!std::all_of(instruction.op.begin(), instruction.op.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
            ReportBackendError(source_path,
                               "backend: variant_extract instruction has non-numeric op field: " + instruction.op,
                               diagnostics);
            return false;
        }
        payload_index = static_cast<std::size_t>(std::stoul(instruction.op));
    }
    if (payload_index >= layout.variant_field_types[variant_index].size() ||
        payload_index >= layout.variant_payload_offsets[variant_index].size()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission references out-of-range enum payload index in " +
                               FunctionBlockContext("variant_extract", state.function->name, block),
                           diagnostics);
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("variant_extract", state.function->name, block),
                              result_type)) {
        return false;
    }

    const std::string enum_slot = EmitAggregateStackSlot(selector,
                                                         function_index,
                                                         block_index,
                                                         instruction_index,
                                                         "variant_extract.enum",
                                                         output_lines);
    const std::string field_slot = EmitEnumPayloadFieldPointer(enum_slot,
                                                               layout,
                                                               layout.variant_payload_offsets[variant_index][payload_index],
                                                               function_index,
                                                               block_index,
                                                               instruction_index,
                                                               "variant_extract." + std::to_string(payload_index),
                                                               output_lines);
    const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
    output_lines.push_back(result_temp + " = load " + result_type.backend_name + ", ptr " + field_slot + ", align " +
                           std::to_string(result_type.alignment));
    RecordExecutableValue(state, instruction.result, result_temp, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitArenaNewInstruction(const mir::Instruction& instruction,
                             const ExecutableEmissionContext& context) {
    const std::size_t function_index = context.function_index;
    const std::size_t block_index = context.block_index;
    const std::size_t instruction_index = context.instruction_index;
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue arena;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "arena_new",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, arena)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("arena_new", state.function->name, block),
                              result_type)) {
        return false;
    }

    const sema::Type pointee_type = instruction.type.subtypes.empty() ? sema::UnknownType() : instruction.type.subtypes.front();
    BackendTypeInfo element_type;
    if (!LowerInstructionType(*state.module,
                              pointee_type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("arena_new element", state.function->name, block),
                              element_type)) {
        return false;
    }

    const std::string loaded = LLVMTempName(function_index, block_index, instruction_index) + ".arena";
    const std::string data_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".data";
    const std::string cap = LLVMTempName(function_index, block_index, instruction_index) + ".cap";
    const std::string used = LLVMTempName(function_index, block_index, instruction_index) + ".used";
    output_lines.push_back(loaded + " = load {ptr, i64, i64, ptr}, ptr " + arena.text + ", align 8");
    output_lines.push_back(data_ptr + " = extractvalue {ptr, i64, i64, ptr} " + loaded + ", 0");
    output_lines.push_back(cap + " = extractvalue {ptr, i64, i64, ptr} " + loaded + ", 1");
    output_lines.push_back(used + " = extractvalue {ptr, i64, i64, ptr} " + loaded + ", 2");

    std::string aligned_used = used;
    if (element_type.alignment > 1) {
        const std::string sum = LLVMTempName(function_index, block_index, instruction_index) + ".align_sum";
        const std::string aligned = LLVMTempName(function_index, block_index, instruction_index) + ".aligned_used";
        output_lines.push_back(sum + " = add i64 " + used + ", " + std::to_string(element_type.alignment - 1));
        output_lines.push_back(aligned + " = and i64 " + sum + ", " + std::to_string(-static_cast<long long>(element_type.alignment)));
        aligned_used = aligned;
    }

    const std::string next_used = LLVMTempName(function_index, block_index, instruction_index) + ".next_used";
    const std::string fits = LLVMTempName(function_index, block_index, instruction_index) + ".fits";
    const std::string raw_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".raw_ptr";
    const std::string result_ptr = LLVMTempName(function_index, block_index, instruction_index);
    const std::string used_slot = LLVMTempName(function_index, block_index, instruction_index) + ".used_slot";
    const std::string stored_used = LLVMTempName(function_index, block_index, instruction_index) + ".stored_used";
    output_lines.push_back(next_used + " = add i64 " + aligned_used + ", " + std::to_string(element_type.size));
    output_lines.push_back(fits + " = icmp ule i64 " + next_used + ", " + cap);
    output_lines.push_back(raw_ptr + " = getelementptr i8, ptr " + data_ptr + ", i64 " + aligned_used);
    output_lines.push_back(result_ptr + " = select i1 " + fits + ", ptr " + raw_ptr + ", ptr null");
    output_lines.push_back(used_slot + " = getelementptr inbounds {ptr, i64, i64, ptr}, ptr " + arena.text + ", i64 0, i32 2");
    output_lines.push_back(stored_used + " = select i1 " + fits + ", i64 " + next_used + ", i64 " + used);
    output_lines.push_back("store i64 " + stored_used + ", ptr " + used_slot + ", align 8");
    RecordExecutableValue(state, instruction.result, result_ptr, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitSliceInstruction(const mir::Instruction& instruction,
                          const ExecutableEmissionContext& context) {
    const std::size_t function_index = context.function_index;
    const std::size_t block_index = context.block_index;
    const std::size_t instruction_index = context.instruction_index;
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;
    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }
    if (!RequireOperandRange(instruction,
                             2,
                             3,
                             "LLVM bootstrap executable emission",
                             "slice to use base plus one or two bounds",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics)) {
        return false;
    }

    ExecutableValue base;
    ExecutableValue lower;
    if (!ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, base) ||
        !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, lower)) {
        return false;
    }

    std::optional<ExecutableValue> upper;
    if (instruction.operands.size() == 3) {
        ExecutableValue resolved_upper;
        if (!ResolveExecutableValue(state, instruction.operands[2], block, source_path, diagnostics, resolved_upper)) {
            return false;
        }
        upper = resolved_upper;
    }

    BackendTypeInfo slice_type;
    BackendTypeInfo element_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              FunctionBlockContext("slice", state.function->name, block),
                              slice_type)) {
        return false;
    }

    const bool string_slice = instruction.type.kind == sema::Type::Kind::kString ||
                              instruction.type.name == "str" || instruction.type.name == "string" ||
                              instruction.type.name == "cstr";
    if (string_slice) {
        if (!LowerInstructionType(*state.module,
                                  sema::NamedType("u8"),
                                  source_path,
                                  diagnostics,
                                  FunctionBlockContext("slice element", state.function->name, block),
                                  element_type)) {
            return false;
        }
    } else if (instruction.type.subtypes.empty() ||
               !LowerInstructionType(*state.module,
                                     instruction.type.subtypes.front(),
                                     source_path,
                                     diagnostics,
                                     FunctionBlockContext("slice element", state.function->name, block),
                                     element_type)) {
        return false;
    }

    std::string lower_i64;
    if (!ExtendIntegerToI64(lower,
                            function_index,
                            block_index,
                            instruction_index,
                            "slice_lower",
                            output_lines,
                            lower_i64)) {
        return false;
    }

    std::string upper_i64;
    if (upper.has_value() &&
        !ExtendIntegerToI64(*upper,
                            function_index,
                            block_index,
                            instruction_index + 1,
                            "slice_upper",
                            output_lines,
                            upper_i64)) {
        return false;
    }

    const std::string data_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".slice_ptr";
    std::string len_value;
    if (!base.type.backend_name.empty() && base.type.backend_name.front() == '[') {
        const auto array_length = ParseBackendArrayLength(base.type.backend_name);
        if (!array_length.has_value()) {
            ReportBackendError(source_path,
                               "LLVM bootstrap executable emission requires statically known array length for slice base in function '" +
                                   state.function->name + "' block '" + block.label + "'",
                               diagnostics);
            return false;
        }
        const std::string array_slot = EmitAggregateStackSlot(base,
                                                              function_index,
                                                              block_index,
                                                              instruction_index,
                                                              "slice_array",
                                                              output_lines);
        output_lines.push_back(data_ptr + " = getelementptr inbounds " + base.type.backend_name + ", ptr " + array_slot + ", i64 0, i64 " + lower_i64);
        len_value = EmitSliceLengthValue(function_index,
                                         block_index,
                                         instruction_index,
                                         "slice_len",
                                         lower_i64,
                                         upper.has_value() ? std::optional<std::string>(upper_i64) : std::nullopt,
                                         std::to_string(*array_length),
                                         output_lines);
    } else if (base.type.backend_name == "{ptr, i64}") {
        const std::string base_ptr = LLVMTempName(function_index, block_index, instruction_index) + ".base_ptr";
        const std::string base_len = LLVMTempName(function_index, block_index, instruction_index) + ".base_len";
        output_lines.push_back(base_ptr + " = extractvalue {ptr, i64} " + base.text + ", 0");
        output_lines.push_back(base_len + " = extractvalue {ptr, i64} " + base.text + ", 1");
        output_lines.push_back(data_ptr + " = getelementptr inbounds " + element_type.backend_name + ", ptr " + base_ptr + ", i64 " + lower_i64);
        len_value = EmitSliceLengthValue(function_index,
                                         block_index,
                                         instruction_index,
                                         "slice_len",
                                         lower_i64,
                                         upper.has_value() ? std::optional<std::string>(upper_i64) : std::nullopt,
                                         base_len,
                                         output_lines);
    } else {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission does not support slice base type '" + base.type.source_name +
                               "' in function '" + state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    const std::string init_temp = LLVMTempName(function_index, block_index, instruction_index) + ".slice_init";
    const std::string result_temp = LLVMTempName(function_index, block_index, instruction_index);
    output_lines.push_back(init_temp + " = insertvalue " + slice_type.backend_name + " zeroinitializer, ptr " + data_ptr + ", 0");
    output_lines.push_back(result_temp + " = insertvalue " + slice_type.backend_name + " " + init_temp + ", i64 " + len_value + ", 1");
    RecordExecutableValue(state, instruction.result, result_temp, slice_type);
    return true;
}

}  // namespace mc::codegen_llvm