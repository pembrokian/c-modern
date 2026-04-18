#include "compiler/codegen_llvm/backend_internal.h"

#include <unordered_set>

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

bool EmitAggregateEqualityComparison(const ExecutableValue& lhs,
                                     const ExecutableValue& rhs,
                                     std::size_t function_index,
                                     std::size_t block_index,
                                     std::size_t instruction_index,
                                     std::string_view suffix,
                                     std::vector<std::string>& output_lines,
                                     std::string& eq_result) {
    const std::string lhs_slot = EmitAggregateStackSlot(lhs,
                                                        function_index,
                                                        block_index,
                                                        instruction_index,
                                                        std::string(suffix) + ".lhs.slot",
                                                        output_lines);
    const std::string rhs_slot = EmitAggregateStackSlot(rhs,
                                                        function_index,
                                                        block_index,
                                                        instruction_index,
                                                        std::string(suffix) + ".rhs.slot",
                                                        output_lines);

    const std::string temp = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix);
    std::string current_eq = "true";
    std::size_t offset = 0;
    std::size_t chunk_index = 0;
    while (offset < lhs.type.size) {
        std::size_t chunk_size = 8;
        if (lhs.type.size - offset < chunk_size) {
            chunk_size = lhs.type.size - offset;
        }
        if (chunk_size != 8 && chunk_size != 4 && chunk_size != 2) {
            chunk_size = 1;
        }

        const std::string lhs_ptr = temp + ".lhs.ptr." + std::to_string(chunk_index);
        const std::string rhs_ptr = temp + ".rhs.ptr." + std::to_string(chunk_index);
        output_lines.push_back(lhs_ptr + " = getelementptr inbounds i8, ptr " + lhs_slot + ", i64 " + std::to_string(offset));
        output_lines.push_back(rhs_ptr + " = getelementptr inbounds i8, ptr " + rhs_slot + ", i64 " + std::to_string(offset));

        const std::string chunk_type = "i" + std::to_string(chunk_size * 8);
        const std::string lhs_chunk = temp + ".lhs.chunk." + std::to_string(chunk_index);
        const std::string rhs_chunk = temp + ".rhs.chunk." + std::to_string(chunk_index);
        const std::string chunk_eq = temp + ".chunk.eq." + std::to_string(chunk_index);
        const std::string next_eq = temp + ".eq." + std::to_string(chunk_index);
        output_lines.push_back(lhs_chunk + " = load " + chunk_type + ", ptr " + lhs_ptr + ", align 1");
        output_lines.push_back(rhs_chunk + " = load " + chunk_type + ", ptr " + rhs_ptr + ", align 1");
        output_lines.push_back(chunk_eq + " = icmp eq " + chunk_type + " " + lhs_chunk + ", " + rhs_chunk);
        output_lines.push_back(next_eq + " = and i1 " + current_eq + ", " + chunk_eq);
        current_eq = next_eq;

        offset += chunk_size;
        chunk_index += 1;
    }

    eq_result = current_eq;
    return true;
}

std::string PreferredOperandType(const ExecutableValue& lhs,
                                 const ExecutableValue& rhs) {
    if (!IsProcedureSourceType(lhs.type.source_name) && lhs.type.source_name != "int_literal") {
        return lhs.type.backend_name;
    }
    if (!IsProcedureSourceType(rhs.type.source_name) && rhs.type.source_name != "int_literal") {
        return rhs.type.backend_name;
    }
    return lhs.type.backend_name;
}

std::optional<std::size_t> ParseEnumPayloadSize(const BackendTypeInfo& type_info) {
    if (type_info.backend_name == "{i64}") {
        return 0;
    }

    constexpr std::string_view kPrefix = "{i64, ";
    if (type_info.backend_name.rfind(kPrefix, 0) != 0 || type_info.backend_name.size() <= kPrefix.size() + 1 ||
        type_info.backend_name.back() != '}') {
        return std::nullopt;
    }

    const std::string_view payload_type = std::string_view(type_info.backend_name).substr(
        kPrefix.size(), type_info.backend_name.size() - kPrefix.size() - 1);
    return ParseBackendArrayLength(payload_type);
}

struct EnumCompareInfo {
    sema::Type source_type;
    const mir::TypeDecl* type_decl = nullptr;
    EnumBackendLayout layout;
    std::size_t payload_size = 0;
    std::vector<std::vector<sema::Type>> variant_field_types;
};

std::optional<EnumCompareInfo> ResolveEnumCompareInfo(const mir::Module& module,
                                                      const ExecutableValue& value) {
    if (!value.source_type.has_value()) {
        return std::nullopt;
    }

    sema::Type source_type = StripMirAliasOrDistinct(module, *value.source_type);
    while (source_type.kind == sema::Type::Kind::kConst && !source_type.subtypes.empty()) {
        source_type = StripMirAliasOrDistinct(module, source_type.subtypes.front());
    }

    if (source_type.kind != sema::Type::Kind::kNamed) {
        return std::nullopt;
    }

    const mir::TypeDecl* type_decl = FindTypeDecl(module, source_type.name);
    if (type_decl == nullptr || type_decl->kind != mir::TypeDecl::Kind::kEnum) {
        return std::nullopt;
    }

    const auto lowered_layout = LowerEnumLayout(module, *type_decl, source_type);
    if (!lowered_layout.has_value()) {
        return std::nullopt;
    }

    const auto payload_size = ParseEnumPayloadSize(value.type);
    if (!payload_size.has_value()) {
        return std::nullopt;
    }

    std::vector<std::vector<sema::Type>> variant_field_types;
    variant_field_types.reserve(type_decl->variants.size());
    for (const auto& raw_variant : type_decl->variants) {
        mir::VariantDecl variant = raw_variant;
        if (!type_decl->type_params.empty()) {
            for (auto& field : variant.payload_fields) {
                field.second = sema::SubstituteTypeParams(std::move(field.second), type_decl->type_params, source_type.subtypes);
            }
        }

        std::vector<sema::Type> field_types;
        field_types.reserve(variant.payload_fields.size());
        for (const auto& field : variant.payload_fields) {
            field_types.push_back(field.second);
        }
        variant_field_types.push_back(std::move(field_types));
    }

    return EnumCompareInfo{
        .source_type = std::move(source_type),
        .type_decl = type_decl,
        .layout = *lowered_layout,
        .payload_size = *payload_size,
        .variant_field_types = std::move(variant_field_types),
    };
}

bool EmitScalarComparison(std::string_view op,
                          const ExecutableValue& lhs,
                          const ExecutableValue& rhs,
                          std::size_t function_index,
                          std::size_t block_index,
                          std::size_t instruction_index,
                          std::string_view suffix,
                          std::vector<std::string>& output_lines,
                          std::string& result) {
    const std::string temp = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(suffix);
    if (IsFloatType(lhs.type)) {
        const char* predicate = nullptr;
        if (op == "==") {
            predicate = "oeq";
        } else if (op == "!=") {
            predicate = "one";
        } else if (op == "<") {
            predicate = "olt";
        } else if (op == "<=") {
            predicate = "ole";
        } else if (op == ">") {
            predicate = "ogt";
        } else if (op == ">=") {
            predicate = "oge";
        }
        if (predicate == nullptr) {
            return false;
        }
        output_lines.push_back(temp + " = fcmp " + std::string(predicate) + " " + lhs.type.backend_name + " " + lhs.text + ", " + rhs.text);
        result = temp;
        return true;
    }

    if (lhs.type.backend_name == "ptr" && op != "==" && op != "!=") {
        return false;
    }

    const bool is_unsigned = lhs.type.backend_name == "i1" || IsUnsignedSourceType(lhs.type.source_name) || lhs.type.backend_name == "ptr";
    const char* predicate = nullptr;
    if (op == "==") {
        predicate = "eq";
    } else if (op == "!=") {
        predicate = "ne";
    } else if (op == "<") {
        predicate = is_unsigned ? "ult" : "slt";
    } else if (op == "<=") {
        predicate = is_unsigned ? "ule" : "sle";
    } else if (op == ">") {
        predicate = is_unsigned ? "ugt" : "sgt";
    } else if (op == ">=") {
        predicate = is_unsigned ? "uge" : "sge";
    }
    if (predicate == nullptr) {
        return false;
    }

    output_lines.push_back(temp + " = icmp " + std::string(predicate) + " " + lhs.type.backend_name + " " + lhs.text + ", " + rhs.text);
    result = temp;
    return true;
}

bool EmitOrderedEnumFieldComparison(const mir::Module& module,
                                    const sema::Type& field_type,
                                    const BackendTypeInfo& backend_type,
                                    const std::string& lhs_text,
                                    const std::string& rhs_text,
                                    std::size_t function_index,
                                    std::size_t block_index,
                                    std::size_t instruction_index,
                                    std::string_view suffix,
                                    const std::filesystem::path& source_path,
                                    support::DiagnosticSink& diagnostics,
                                    std::vector<std::string>& output_lines,
                                    std::string& eq_result,
                                    std::string& lt_result,
                                    std::string& gt_result) {
    const sema::Type stripped_type = StripMirAliasOrDistinct(module, field_type);
    ExecutableValue lhs_value{.text = lhs_text, .type = backend_type, .source_type = stripped_type};
    ExecutableValue rhs_value{.text = rhs_text, .type = backend_type, .source_type = stripped_type};

    if (const auto enum_info = ResolveEnumCompareInfo(module, lhs_value); enum_info.has_value()) {
        const auto emit_enum_ordering_components = [&](const ExecutableValue& enum_lhs,
                                                       const ExecutableValue& enum_rhs,
                                                       const EnumCompareInfo& nested_info,
                                                       std::string_view nested_suffix,
                                                       std::string& nested_eq,
                                                       std::string& nested_lt,
                                                       std::string& nested_gt,
                                                       const auto& self) -> bool {
            const std::string nested_base = LLVMTempName(function_index, block_index, instruction_index) + "." + std::string(nested_suffix);
            const std::string nested_lhs_tag = nested_base + ".lhs.tag";
            const std::string nested_rhs_tag = nested_base + ".rhs.tag";
            const std::string nested_tag_eq = nested_base + ".tag.eq";
            const std::string nested_tag_lt = nested_base + ".tag.lt";
            const std::string nested_tag_gt = nested_base + ".tag.gt";
            output_lines.push_back(nested_lhs_tag + " = extractvalue " + enum_lhs.type.backend_name + " " + enum_lhs.text + ", 0");
            output_lines.push_back(nested_rhs_tag + " = extractvalue " + enum_rhs.type.backend_name + " " + enum_rhs.text + ", 0");
            output_lines.push_back(nested_tag_eq + " = icmp eq i64 " + nested_lhs_tag + ", " + nested_rhs_tag);
            output_lines.push_back(nested_tag_lt + " = icmp slt i64 " + nested_lhs_tag + ", " + nested_rhs_tag);
            output_lines.push_back(nested_tag_gt + " = icmp sgt i64 " + nested_lhs_tag + ", " + nested_rhs_tag);

            if (nested_info.payload_size == 0) {
                nested_eq = nested_tag_eq;
                nested_lt = nested_tag_lt;
                nested_gt = nested_tag_gt;
                return true;
            }

            const std::string nested_lhs_slot = EmitAggregateStackSlot(enum_lhs,
                                                                       function_index,
                                                                       block_index,
                                                                       instruction_index,
                                                                       std::string(nested_suffix) + ".lhs.slot",
                                                                       output_lines);
            const std::string nested_rhs_slot = EmitAggregateStackSlot(enum_rhs,
                                                                       function_index,
                                                                       block_index,
                                                                       instruction_index,
                                                                       std::string(nested_suffix) + ".rhs.slot",
                                                                       output_lines);

            std::string same_variant_eq = "false";
            std::string same_variant_lt = "false";
            std::string same_variant_gt = "false";
            for (std::size_t variant_index = nested_info.variant_field_types.size(); variant_index > 0; --variant_index) {
                const std::size_t actual_index = variant_index - 1;
                std::string variant_eq = "true";
                std::string variant_lt = "false";
                std::string variant_gt = "false";
                for (std::size_t field_index = nested_info.variant_field_types[actual_index].size(); field_index > 0; --field_index) {
                    const std::size_t actual_field_index = field_index - 1;
                    const std::string field_suffix_base = std::string(nested_suffix) + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index);
                    const std::string nested_lhs_field_slot = EmitEnumPayloadFieldPointer(nested_lhs_slot,
                                                                                          nested_info.layout,
                                                                                          nested_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                                          function_index,
                                                                                          block_index,
                                                                                          instruction_index,
                                                                                          field_suffix_base + ".lhs.ptr",
                                                                                          output_lines);
                    const std::string nested_rhs_field_slot = EmitEnumPayloadFieldPointer(nested_rhs_slot,
                                                                                          nested_info.layout,
                                                                                          nested_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                                          function_index,
                                                                                          block_index,
                                                                                          instruction_index,
                                                                                          field_suffix_base + ".rhs.ptr",
                                                                                          output_lines);
                    const BackendTypeInfo& nested_field_backend_type = nested_info.layout.variant_field_types[actual_index][actual_field_index];
                    const std::string nested_lhs_field = nested_base + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".lhs";
                    const std::string nested_rhs_field = nested_base + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".rhs";
                    output_lines.push_back(nested_lhs_field + " = load " + nested_field_backend_type.backend_name + ", ptr " + nested_lhs_field_slot + ", align " + std::to_string(nested_field_backend_type.alignment));
                    output_lines.push_back(nested_rhs_field + " = load " + nested_field_backend_type.backend_name + ", ptr " + nested_rhs_field_slot + ", align " + std::to_string(nested_field_backend_type.alignment));

                    ExecutableValue nested_lhs_field_value{.text = nested_lhs_field, .type = nested_field_backend_type, .source_type = nested_info.variant_field_types[actual_index][actual_field_index]};
                    ExecutableValue nested_rhs_field_value{.text = nested_rhs_field, .type = nested_field_backend_type, .source_type = nested_info.variant_field_types[actual_index][actual_field_index]};

                    std::string field_eq;
                    std::string field_lt;
                    std::string field_gt;
                    if (const auto deeper_enum = ResolveEnumCompareInfo(module, nested_lhs_field_value); deeper_enum.has_value()) {
                        if (!self(nested_lhs_field_value,
                                  nested_rhs_field_value,
                                  *deeper_enum,
                                  field_suffix_base,
                                  field_eq,
                                  field_lt,
                                  field_gt,
                                  self)) {
                            return false;
                        }
                    } else if (!EmitScalarComparison("==", nested_lhs_field_value, nested_rhs_field_value, function_index, block_index, instruction_index, field_suffix_base + ".eq", output_lines, field_eq) ||
                               !EmitScalarComparison("<", nested_lhs_field_value, nested_rhs_field_value, function_index, block_index, instruction_index, field_suffix_base + ".lt", output_lines, field_lt) ||
                               !EmitScalarComparison(">", nested_lhs_field_value, nested_rhs_field_value, function_index, block_index, instruction_index, field_suffix_base + ".gt", output_lines, field_gt)) {
                        ReportBackendError(source_path,
                                           "LLVM bootstrap executable emission does not support payload-bearing enum ordering for field type '" +
                                               sema::FormatType(nested_info.variant_field_types[actual_index][actual_field_index]) + "'",
                                           diagnostics);
                        return false;
                    }

                    const std::string next_variant_eq = nested_base + ".variant." + std::to_string(actual_index) + ".eq." + std::to_string(actual_field_index);
                    const std::string next_variant_lt = nested_base + ".variant." + std::to_string(actual_index) + ".lt." + std::to_string(actual_field_index);
                    const std::string next_variant_gt = nested_base + ".variant." + std::to_string(actual_index) + ".gt." + std::to_string(actual_field_index);
                    output_lines.push_back(next_variant_eq + " = and i1 " + field_eq + ", " + variant_eq);
                    output_lines.push_back(next_variant_lt + " = select i1 " + field_eq + ", i1 " + variant_lt + ", i1 " + field_lt);
                    output_lines.push_back(next_variant_gt + " = select i1 " + field_eq + ", i1 " + variant_gt + ", i1 " + field_gt);
                    variant_eq = next_variant_eq;
                    variant_lt = next_variant_lt;
                    variant_gt = next_variant_gt;
                }

                const std::string variant_match = nested_base + ".variant." + std::to_string(actual_index) + ".match";
                const std::string next_same_eq = nested_base + ".same.eq." + std::to_string(actual_index);
                const std::string next_same_lt = nested_base + ".same.lt." + std::to_string(actual_index);
                const std::string next_same_gt = nested_base + ".same.gt." + std::to_string(actual_index);
                output_lines.push_back(variant_match + " = icmp eq i64 " + nested_lhs_tag + ", " + std::to_string(actual_index));
                output_lines.push_back(next_same_eq + " = select i1 " + variant_match + ", i1 " + variant_eq + ", i1 " + same_variant_eq);
                output_lines.push_back(next_same_lt + " = select i1 " + variant_match + ", i1 " + variant_lt + ", i1 " + same_variant_lt);
                output_lines.push_back(next_same_gt + " = select i1 " + variant_match + ", i1 " + variant_gt + ", i1 " + same_variant_gt);
                same_variant_eq = next_same_eq;
                same_variant_lt = next_same_lt;
                same_variant_gt = next_same_gt;
            }

            nested_eq = nested_base + ".eq";
            nested_lt = nested_base + ".lt";
            nested_gt = nested_base + ".gt";
            output_lines.push_back(nested_eq + " = select i1 " + nested_tag_eq + ", i1 " + same_variant_eq + ", i1 false");
            output_lines.push_back(nested_lt + " = select i1 " + nested_tag_eq + ", i1 " + same_variant_lt + ", i1 " + nested_tag_lt);
            output_lines.push_back(nested_gt + " = select i1 " + nested_tag_eq + ", i1 " + same_variant_gt + ", i1 " + nested_tag_gt);
            return true;
        };

        return emit_enum_ordering_components(lhs_value,
                                             rhs_value,
                                             *enum_info,
                                             suffix,
                                             eq_result,
                                             lt_result,
                                             gt_result,
                                             emit_enum_ordering_components);
    }

    if (!EmitScalarComparison("==", lhs_value, rhs_value, function_index, block_index, instruction_index, std::string(suffix) + ".eq", output_lines, eq_result) ||
        !EmitScalarComparison("<", lhs_value, rhs_value, function_index, block_index, instruction_index, std::string(suffix) + ".lt", output_lines, lt_result) ||
        !EmitScalarComparison(">", lhs_value, rhs_value, function_index, block_index, instruction_index, std::string(suffix) + ".gt", output_lines, gt_result)) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission does not support payload-bearing enum ordering for field type '" +
                               sema::FormatType(field_type) + "'",
                           diagnostics);
        return false;
    }

    return true;
}

bool EmitEnumPayloadEqualityComparison(const mir::Module& module,
                                       const EnumCompareInfo& compare_info,
                                       const std::string& lhs_slot,
                                       const std::string& rhs_slot,
                                       const std::string& lhs_tag,
                                       std::size_t function_index,
                                       std::size_t block_index,
                                       std::size_t instruction_index,
                                       const std::filesystem::path& source_path,
                                       support::DiagnosticSink& diagnostics,
                                       std::vector<std::string>& output_lines,
                                       std::string& payload_eq_result) {
    const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
    std::string same_tag_eq = "true";
    for (std::size_t variant_index = compare_info.variant_field_types.size(); variant_index > 0; --variant_index) {
        const std::size_t actual_index = variant_index - 1;
        std::string variant_eq = "true";
        for (std::size_t field_index = compare_info.variant_field_types[actual_index].size(); field_index > 0; --field_index) {
            const std::size_t actual_field_index = field_index - 1;
            const std::string field_suffix = "enum.equal.variant." + std::to_string(actual_index) + ".field." +
                                             std::to_string(actual_field_index);
            const std::string lhs_field_slot = EmitEnumPayloadFieldPointer(lhs_slot,
                                                                           compare_info.layout,
                                                                           compare_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                           function_index,
                                                                           block_index,
                                                                           instruction_index,
                                                                           field_suffix + ".lhs.ptr",
                                                                           output_lines);
            const std::string rhs_field_slot = EmitEnumPayloadFieldPointer(rhs_slot,
                                                                           compare_info.layout,
                                                                           compare_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                           function_index,
                                                                           block_index,
                                                                           instruction_index,
                                                                           field_suffix + ".rhs.ptr",
                                                                           output_lines);
            const BackendTypeInfo& field_backend_type = compare_info.layout.variant_field_types[actual_index][actual_field_index];
            const std::string lhs_field = temp + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".lhs";
            const std::string rhs_field = temp + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".rhs";
            output_lines.push_back(lhs_field + " = load " + field_backend_type.backend_name + ", ptr " + lhs_field_slot + ", align " + std::to_string(field_backend_type.alignment));
            output_lines.push_back(rhs_field + " = load " + field_backend_type.backend_name + ", ptr " + rhs_field_slot + ", align " + std::to_string(field_backend_type.alignment));

            std::string field_eq;
            std::string field_lt;
            std::string field_gt;
            if (!EmitOrderedEnumFieldComparison(module,
                                                compare_info.variant_field_types[actual_index][actual_field_index],
                                                field_backend_type,
                                                lhs_field,
                                                rhs_field,
                                                function_index,
                                                block_index,
                                                instruction_index,
                                                field_suffix,
                                                source_path,
                                                diagnostics,
                                                output_lines,
                                                field_eq,
                                                field_lt,
                                                field_gt)) {
                return false;
            }

            const std::string next_variant_eq = temp + ".variant." + std::to_string(actual_index) + ".eq." +
                                                std::to_string(actual_field_index);
            output_lines.push_back(next_variant_eq + " = and i1 " + variant_eq + ", " + field_eq);
            variant_eq = next_variant_eq;
        }

        const std::string variant_match = temp + ".variant." + std::to_string(actual_index) + ".match";
        const std::string next_same_tag = temp + ".same_tag.eq." + std::to_string(actual_index);
        output_lines.push_back(variant_match + " = icmp eq i64 " + lhs_tag + ", " + std::to_string(actual_index));
        output_lines.push_back(next_same_tag + " = select i1 " + variant_match + ", i1 " + variant_eq + ", i1 " + same_tag_eq);
        same_tag_eq = next_same_tag;
    }

    payload_eq_result = same_tag_eq;
    return true;
}

void EmitEnumCompare(const mir::Instruction& instruction,
                     std::size_t function_index,
                     std::size_t block_index,
                     std::size_t instruction_index,
                     std::string_view compare_predicate,
                     const ExecutableValue& lhs,
                     const ExecutableValue& rhs,
                     const EnumCompareInfo& compare_info,
                     const std::filesystem::path& source_path,
                     const mir::BasicBlock& block,
                     support::DiagnosticSink& diagnostics,
                     const BackendTypeInfo& result_type,
                     ExecutableFunctionState& state,
                     std::vector<std::string>& output_lines,
                     bool& emitted) {
    emitted = false;
    const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
    const std::string lhs_tag = temp + ".lhs.tag";
    const std::string rhs_tag = temp + ".rhs.tag";
    output_lines.push_back(lhs_tag + " = extractvalue " + lhs.type.backend_name + " " + lhs.text + ", 0");
    output_lines.push_back(rhs_tag + " = extractvalue " + rhs.type.backend_name + " " + rhs.text + ", 0");

    if (compare_info.payload_size == 0) {
        output_lines.push_back(temp + " = icmp " + std::string(compare_predicate) + " i64 " + lhs_tag + ", " + rhs_tag);
        RecordExecutableValue(state, instruction.result, temp, result_type);
        emitted = true;
        return;
    }

    const std::string tag_eq = temp + ".tag.eq";
    output_lines.push_back(tag_eq + " = icmp eq i64 " + lhs_tag + ", " + rhs_tag);

    const std::string lhs_slot = EmitAggregateStackSlot(lhs,
                                                        function_index,
                                                        block_index,
                                                        instruction_index,
                                                        "enum.compare.lhs.slot",
                                                        output_lines);
    const std::string rhs_slot = EmitAggregateStackSlot(rhs,
                                                        function_index,
                                                        block_index,
                                                        instruction_index,
                                                        "enum.compare.rhs.slot",
                                                        output_lines);

    if (instruction.op == "==" || instruction.op == "!=") {
        std::string payload_eq;
        if (!EmitEnumPayloadEqualityComparison(*state.module,
                                               compare_info,
                                               lhs_slot,
                                               rhs_slot,
                                               lhs_tag,
                                               function_index,
                                               block_index,
                                               instruction_index,
                                               source_path,
                                               diagnostics,
                                               output_lines,
                                               payload_eq)) {
            return;
        }

        if (instruction.op == "==") {
            output_lines.push_back(temp + " = and i1 " + tag_eq + ", " + payload_eq);
        } else {
            const std::string tag_ne = temp + ".tag.ne";
            const std::string payload_ne = temp + ".payload.ne";
            output_lines.push_back(tag_ne + " = icmp ne i64 " + lhs_tag + ", " + rhs_tag);
            output_lines.push_back(payload_ne + " = xor i1 " + payload_eq + ", true");
            output_lines.push_back(temp + " = or i1 " + tag_ne + ", " + payload_ne);
        }
        RecordExecutableValue(state, instruction.result, temp, result_type);
        emitted = true;
        return;
    }

    const std::string tag_order = temp + ".tag.order";
    output_lines.push_back(tag_order + " = icmp " + std::string(compare_predicate) + " i64 " + lhs_tag + ", " + rhs_tag);

    const std::string default_same_tag = instruction.op == "<=" || instruction.op == ">=" ? "true" : "false";
    std::string same_tag_result = default_same_tag;
    for (std::size_t variant_index = compare_info.variant_field_types.size(); variant_index > 0; --variant_index) {
        const std::size_t actual_index = variant_index - 1;
        std::string variant_result = default_same_tag;
        for (std::size_t field_index = compare_info.variant_field_types[actual_index].size(); field_index > 0; --field_index) {
            const std::size_t actual_field_index = field_index - 1;
            const std::string field_suffix = "enum.order.variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index);
            const std::string lhs_field_slot = EmitEnumPayloadFieldPointer(lhs_slot,
                                                                           compare_info.layout,
                                                                           compare_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                           function_index,
                                                                           block_index,
                                                                           instruction_index,
                                                                           field_suffix + ".lhs.ptr",
                                                                           output_lines);
            const std::string rhs_field_slot = EmitEnumPayloadFieldPointer(rhs_slot,
                                                                           compare_info.layout,
                                                                           compare_info.layout.variant_payload_offsets[actual_index][actual_field_index],
                                                                           function_index,
                                                                           block_index,
                                                                           instruction_index,
                                                                           field_suffix + ".rhs.ptr",
                                                                           output_lines);
            const BackendTypeInfo& field_backend_type = compare_info.layout.variant_field_types[actual_index][actual_field_index];
            const std::string lhs_field = temp + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".lhs";
            const std::string rhs_field = temp + ".variant." + std::to_string(actual_index) + ".field." + std::to_string(actual_field_index) + ".rhs";
            output_lines.push_back(lhs_field + " = load " + field_backend_type.backend_name + ", ptr " + lhs_field_slot + ", align " + std::to_string(field_backend_type.alignment));
            output_lines.push_back(rhs_field + " = load " + field_backend_type.backend_name + ", ptr " + rhs_field_slot + ", align " + std::to_string(field_backend_type.alignment));

            std::string field_eq;
            std::string field_lt;
            std::string field_gt;
            if (!EmitOrderedEnumFieldComparison(*state.module,
                                                compare_info.variant_field_types[actual_index][actual_field_index],
                                                field_backend_type,
                                                lhs_field,
                                                rhs_field,
                                                function_index,
                                                block_index,
                                                instruction_index,
                                                field_suffix,
                                                source_path,
                                                diagnostics,
                                                output_lines,
                                                field_eq,
                                                field_lt,
                                                field_gt)) {
                return;
            }

            const std::string selected_order = instruction.op == "<" || instruction.op == "<=" ? field_lt : field_gt;
            const std::string next_variant_result = temp + ".variant." + std::to_string(actual_index) + ".step." + std::to_string(actual_field_index);
            output_lines.push_back(next_variant_result + " = select i1 " + field_eq + ", i1 " + variant_result + ", i1 " + selected_order);
            variant_result = next_variant_result;
        }

        const std::string variant_match = temp + ".variant." + std::to_string(actual_index) + ".match";
        const std::string next_same_tag = temp + ".same_tag." + std::to_string(actual_index);
        output_lines.push_back(variant_match + " = icmp eq i64 " + lhs_tag + ", " + std::to_string(actual_index));
        output_lines.push_back(next_same_tag + " = select i1 " + variant_match + ", i1 " + variant_result + ", i1 " + same_tag_result);
        same_tag_result = next_same_tag;
    }

    output_lines.push_back(temp + " = select i1 " + tag_eq + ", i1 " + same_tag_result + ", i1 " + tag_order);
    RecordExecutableValue(state, instruction.result, temp, result_type);
    emitted = true;
}

struct BinaryOpcodeEntry {
    std::string_view op;
    const char* float_opcode;
    const char* unsigned_opcode;
    const char* signed_opcode;
};

constexpr BinaryOpcodeEntry kBinaryOpcodeTable[] = {
    {"+", "fadd", "add", "add"},
    {"-", "fsub", "sub", "sub"},
    {"*", "fmul", "mul", "mul"},
    {"/", "fdiv", "udiv", "sdiv"},
    {"%", "frem", "urem", "srem"},
    {"<<", nullptr, "shl", "shl"},
    {">>", nullptr, "lshr", "ashr"},
    {"&", nullptr, "and", "and"},
    {"|", nullptr, "or", "or"},
    {"^", nullptr, "xor", "xor"},
};

struct ComparePredicateEntry {
    std::string_view op;
    const char* float_predicate;
    const char* unsigned_predicate;
    const char* signed_predicate;
};

constexpr ComparePredicateEntry kComparePredicateTable[] = {
    {"==", "oeq", "eq", "eq"},
    {"!=", "one", "ne", "ne"},
    {"<", "olt", "ult", "slt"},
    {"<=", "ole", "ule", "sle"},
    {">", "ogt", "ugt", "sgt"},
    {">=", "oge", "uge", "sge"},
};

std::string BinaryOpcodeForLLVM(const mir::Instruction& instruction,
                                const ExecutableValue& lhs,
                                const BackendTypeInfo& result_type) {
    const bool is_float = IsFloatType(result_type);
    const bool is_unsigned = IsUnsignedSourceType(lhs.type.source_name);

    for (const auto& entry : kBinaryOpcodeTable) {
        if (instruction.op != entry.op) {
            continue;
        }
        if (is_float) {
            return entry.float_opcode == nullptr ? std::string() : std::string(entry.float_opcode);
        }
        return std::string(is_unsigned ? entry.unsigned_opcode : entry.signed_opcode);
    }
    return {};
}

std::string ComparePredicateForLLVM(const mir::Instruction& instruction,
                                    const ExecutableValue& lhs) {
    const bool is_float = IsFloatType(lhs.type);
    const bool is_unsigned = IsUnsignedSourceType(lhs.type.source_name);

    for (const auto& entry : kComparePredicateTable) {
        if (instruction.op != entry.op) {
            continue;
        }
        if (is_float) {
            return std::string(entry.float_predicate);
        }
        return std::string(is_unsigned ? entry.unsigned_predicate : entry.signed_predicate);
    }
    return {};
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
            const CheckedHelperAnalysisContext context{
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

bool EmitConvertDistinctInstruction(const mir::Instruction& instruction,
                                    const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;

    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue operand;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "convert_distinct",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("convert_distinct", state, block),
                              result_type)) {
        return false;
    }

    RecordExecutableValue(state, instruction.result, operand.text, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitConvertInstruction(const mir::Instruction& instruction,
                            const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;

    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue operand;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "convert",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("convert", state, block),
                              result_type)) {
        return false;
    }

    if (operand.type.backend_name != result_type.backend_name) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission only supports representation-preserving convert in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    RecordExecutableValue(state, instruction.result, operand.text, result_type);
    return true;
}

bool EmitConvertNumericInstruction(const mir::Instruction& instruction,
                                   const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;

    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue operand;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "convert_numeric",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("convert_numeric", state, block),
                              result_type)) {
        return false;
    }

    std::string converted;
    if (!EmitNumericConversion(operand,
                               result_type,
                               context.function_index,
                               context.block_index,
                               context.instruction_index,
                               output_lines,
                               converted)) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission could not lower numeric conversion in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    RecordExecutableValue(state, instruction.result, converted, result_type);
    return true;
}

bool EmitPointerToIntInstruction(const mir::Instruction& instruction,
                                 const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;

    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue operand;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "pointer_to_int",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
        return false;
    }
    if (operand.type.backend_name != "ptr") {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires pointer operand for pointer_to_int in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("pointer_to_int", state, block),
                              result_type)) {
        return false;
    }
    if (IntegerBitWidth(result_type) == 0) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires integer result type for pointer_to_int in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    const std::string temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index);
    context.output_lines.push_back(temp + " = ptrtoint ptr " + operand.text + " to " + result_type.backend_name);
    RecordExecutableValue(state, instruction.result, temp, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitIntToPointerInstruction(const mir::Instruction& instruction,
                                 const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;

    if (!RequireInstructionResult(instruction, block, source_path, diagnostics)) {
        return false;
    }

    ExecutableValue operand;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "int_to_pointer",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands.front(), block, source_path, diagnostics, operand)) {
        return false;
    }
    if (IntegerBitWidth(operand.type) == 0) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires integer operand for int_to_pointer in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("int_to_pointer", state, block),
                              result_type)) {
        return false;
    }
    if (result_type.backend_name != "ptr") {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission requires pointer result type for int_to_pointer in function '" +
                               state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    const std::string temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index);
    context.output_lines.push_back(temp + " = inttoptr " + operand.type.backend_name + " " + operand.text + " to ptr");
    RecordExecutableValue(state, instruction.result, temp, result_type, std::nullopt, instruction.type);
    return true;
}

bool EmitBoundsCheckCall(const mir::Instruction& instruction,
                         const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;
    std::vector<std::string>& output_lines = context.output_lines;

    if (!RequireOperandRange(instruction,
                             2,
                             3,
                             "LLVM bootstrap executable emission",
                             "bounds_check to use two or three operands",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics)) {
        return false;
    }

    std::vector<std::string> operands_i64;
    operands_i64.reserve(instruction.operands.size());
    for (std::size_t index = 0; index < instruction.operands.size(); ++index) {
        ExecutableValue operand;
        if (!ResolveExecutableValue(state, instruction.operands[index], block, source_path, diagnostics, operand)) {
            return false;
        }
        std::string extended;
        if (!ExtendIntegerToI64(operand,
                                context.function_index,
                                context.block_index,
                                context.instruction_index + index,
                                "bounds",
                                output_lines,
                                extended)) {
            return false;
        }
        operands_i64.push_back(std::move(extended));
    }

    if (instruction.op == "index") {
        output_lines.push_back("call void @__mc_check_bounds_index(i64 " + operands_i64[0] + ", i64 " + operands_i64[1] + ")");
        return true;
    }
    if (instruction.op == "slice") {
        if (operands_i64.size() == 2) {
            output_lines.push_back("call void @__mc_check_bounds_index(i64 " + operands_i64[0] + ", i64 " + operands_i64[1] + ")");
            return true;
        }
        output_lines.push_back("call void @__mc_check_bounds_slice(i64 " + operands_i64[0] + ", i64 " + operands_i64[1] + ", i64 " + operands_i64[2] + ")");
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap executable emission does not recognize bounds_check mode '" + instruction.op +
                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

bool EmitDivCheckCall(const mir::Instruction& instruction,
                      const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;

    ExecutableValue lhs;
    ExecutableValue rhs;
    if (!RequireOperandCount(instruction,
                             2,
                             "LLVM bootstrap executable emission",
                             "div_check",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, lhs) ||
        !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, rhs)) {
        return false;
    }
    context.output_lines.push_back("call void @__mc_check_div_" + rhs.type.backend_name + "(" + rhs.type.backend_name + " " + rhs.text + ")");
    return true;
}

bool EmitShiftCheckCall(const mir::Instruction& instruction,
                        const ExecutableEmissionContext& context) {
    const mir::BasicBlock& block = context.block;
    const std::filesystem::path& source_path = context.source_path;
    support::DiagnosticSink& diagnostics = context.diagnostics;
    ExecutableFunctionState& state = context.state;

    ExecutableValue value;
    ExecutableValue count;
    if (!RequireOperandCount(instruction,
                             2,
                             "LLVM bootstrap executable emission",
                             "shift_check",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, value) ||
        !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, count)) {
        return false;
    }

    std::string count_i64;
    if (!ExtendIntegerToI64(count,
                            context.function_index,
                            context.block_index,
                            context.instruction_index,
                            "shift",
                            context.output_lines,
                            count_i64)) {
        return false;
    }
    context.output_lines.push_back("call void @__mc_check_shift_" + std::to_string(IntegerBitWidth(value.type)) + "(i64 " + count_i64 + ")");
    return true;
}

bool EmitBinaryInstruction(const mir::Instruction& instruction,
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

    ExecutableValue lhs;
    ExecutableValue rhs;
    if (!RequireOperandCount(instruction,
                             2,
                             "LLVM bootstrap executable emission",
                             "binary",
                             state.function->name,
                             block,
                             source_path,
                             diagnostics) ||
        !ResolveExecutableValue(state, instruction.operands[0], block, source_path, diagnostics, lhs) ||
        !ResolveExecutableValue(state, instruction.operands[1], block, source_path, diagnostics, rhs)) {
        return false;
    }

    BackendTypeInfo result_type;
    if (!LowerInstructionType(*state.module,
                              instruction.type,
                              source_path,
                              diagnostics,
                              ExecutableFunctionBlockContext("binary", state, block),
                              result_type)) {
        return false;
    }

    const std::string temp = LLVMTempName(function_index, block_index, instruction_index);
    const std::string compare_predicate = ComparePredicateForLLVM(instruction, lhs);
    if (!compare_predicate.empty()) {
        const auto lhs_enum = ResolveEnumCompareInfo(*state.module, lhs);
        const auto rhs_enum = ResolveEnumCompareInfo(*state.module, rhs);
        if (lhs_enum.has_value() && rhs_enum.has_value()) {
            bool emitted = false;
            EmitEnumCompare(instruction,
                            function_index,
                            block_index,
                            instruction_index,
                            compare_predicate,
                            lhs,
                            rhs,
                            *lhs_enum,
                            source_path,
                            block,
                            diagnostics,
                            result_type,
                            state,
                            output_lines,
                            emitted);
            return emitted;
        }
        if (!lhs.type.backend_name.empty() &&
            (lhs.type.backend_name.front() == '[' || lhs.type.backend_name.front() == '{')) {
            if (instruction.op != "==" && instruction.op != "!=") {
                ReportBackendError(source_path,
                                   "LLVM bootstrap executable emission only supports aggregate comparison for '==' and '!=' in function '" +
                                       state.function->name + "' block '" + block.label + "'",
                                   diagnostics);
                return false;
            }

            std::string aggregate_eq;
            if (!EmitAggregateEqualityComparison(lhs,
                                                rhs,
                                                function_index,
                                                block_index,
                                                instruction_index,
                                                "aggregate.compare",
                                                output_lines,
                                                aggregate_eq)) {
                return false;
            }
            if (instruction.op == "==") {
                output_lines.push_back(temp + " = and i1 true, " + aggregate_eq);
            } else {
                output_lines.push_back(temp + " = xor i1 " + aggregate_eq + ", true");
            }
            RecordExecutableValue(state, instruction.result, temp, result_type);
            return true;
        }
        const std::string operand_type = PreferredOperandType(lhs, rhs);
        const std::string compare_opcode = IsFloatType(lhs.type) ? "fcmp" : "icmp";
        output_lines.push_back(temp + " = " + compare_opcode + " " + compare_predicate + " " + operand_type +
                               " " + lhs.text + ", " + rhs.text);
        RecordExecutableValue(state, instruction.result, temp, result_type);
        return true;
    }

    const std::string opcode = BinaryOpcodeForLLVM(instruction, lhs, result_type);
    if (opcode.empty()) {
        ReportBackendError(source_path,
                           "LLVM bootstrap executable emission does not support binary operator '" + instruction.op +
                               "' in function '" + state.function->name + "' block '" + block.label + "'",
                           diagnostics);
        return false;
    }

    output_lines.push_back(temp + " = " + opcode + " " + result_type.backend_name + " " + lhs.text + ", " +
                           rhs.text);
    RecordExecutableValue(state, instruction.result, temp, result_type);
    return true;
}

}  // namespace mc::codegen_llvm