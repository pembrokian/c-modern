#include "compiler/codegen_llvm/backend_internal.h"

namespace mc::codegen_llvm {
namespace {

constexpr std::string_view kBufferStructType = "{ptr, i64, i64, ptr}";

sema::Type 
InstantiateAliasedType(const mir::TypeDecl& type_decl, const sema::Type& instantiated_type) {
    sema::Type aliased_type = type_decl.aliased_type;
    if (!type_decl.type_params.empty()) {
        aliased_type = sema::SubstituteTypeParams(std::move(aliased_type),
                                                  type_decl.type_params,
                                                  instantiated_type.subtypes);
    }
    return aliased_type;
}

std::vector<std::pair<std::string, sema::Type>> 
InstantiateFields(const mir::TypeDecl& type_decl, const sema::Type& instantiated_type) {
    std::vector<std::pair<std::string, sema::Type>> fields = type_decl.fields;
    if (type_decl.type_params.empty()) {
        return fields;
    }
    for (auto& field : fields) {
        field.second = sema::SubstituteTypeParams(std::move(field.second),
                                                  type_decl.type_params,
                                                  instantiated_type.subtypes);
    }
    return fields;
}

std::optional<std::size_t> 
FindFieldIndexForMemory(const mir::Module& module, const sema::Type& base_type, std::string_view field_name) {
    const auto builtin_fields = sema::BuiltinAggregateFields(base_type);
    if (!builtin_fields.empty()) {
        for (std::size_t index = 0; index < builtin_fields.size(); ++index) {
            if (builtin_fields[index].first == field_name) {
                return index;
            }
        }
    }
    const sema::Type lowered_base = sema::CanonicalizeBuiltinType(base_type);
    if (lowered_base.kind == sema::Type::Kind::kNamed) {
        if (const auto* type_decl = FindTypeDecl(module, lowered_base.name)) {
            const auto fields = InstantiateFields(*type_decl, lowered_base);
            for (std::size_t index = 0; index < fields.size(); ++index) {
                if (fields[index].first == field_name) {
                    return index;
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<sema::Type> 
FindFieldTypeForMemory(const mir::Module& module, const sema::Type& base_type, std::string_view field_name) {
    const auto builtin_fields = sema::BuiltinAggregateFields(base_type);
    for (const auto& field : builtin_fields) {
        if (field.first == field_name) {
            return field.second;
        }
    }
    const sema::Type canonical_base = sema::CanonicalizeBuiltinType(base_type);
    if (canonical_base.kind == sema::Type::Kind::kNamed) {
        if (const auto* type_decl = FindTypeDecl(module, canonical_base.name)) {
            const auto fields = InstantiateFields(*type_decl, canonical_base);
            for (const auto& field : fields) {
                if (field.first == field_name) {
                    return field.second;
                }
            }
        }
    }
    return std::nullopt;
}

const mir::Local* FindFunctionLocal(const mir::Function& function,
                                    std::string_view name) {
    for (const auto& local : function.locals) {
        if (local.name == name) {
            return &local;
        }
    }
    return nullptr;
}

bool IsAggregateExecutableType(const BackendTypeInfo& type_info) {
    return !type_info.backend_name.empty() &&
           (type_info.backend_name.front() == '{' ||
            type_info.backend_name.front() == '[' ||
            type_info.backend_name.front() == '<');
}

bool ResolveExecutableLocalSlot(const ExecutableFunctionState& state,
                                const std::string& local_name,
                                const mir::BasicBlock& block,
                                const std::filesystem::path& source_path,
                                support::DiagnosticSink& diagnostics,
                                std::string& local_slot) {
    const auto it = state.local_slots.find(local_name);
    if (it != state.local_slots.end()) {
        local_slot = it->second;
        return true;
    }

    ReportBackendError(source_path,
                       "LLVM bootstrap executable emission references unknown local '" + local_name +
                           "' in function '" + state.function->name + "' block '" + block.label + "'",
                       diagnostics);
    return false;
}

bool 
ResolveArrayOrSliceElementPtr(
    const ExecutableValue& base,
    const std::string& index_i64,
    const BackendTypeInfo& element_type,
    mir::Instruction::StorageBaseKind base_storage,
    std::string_view base_name,
    const ExecutableEmissionContext& context,
    std::string_view tag,
    std::string_view operation,
    std::string& ptr_temp
) {
    ptr_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + "." + std::string(tag) + ".ptr";
    if (!base.type.backend_name.empty() && base.type.backend_name.front() == '[') {
        std::string array_ptr;
        if (base_storage == mir::Instruction::StorageBaseKind::kLocal && !base_name.empty()) {
            const auto local_it = context.state.local_slots.find(std::string(base_name));
            if (local_it != context.state.local_slots.end()) {
                array_ptr = local_it->second;
            }
        } else if (base_storage == mir::Instruction::StorageBaseKind::kGlobal && !base_name.empty()) {
            const auto global_it = context.state.globals->find(std::string(base_name));
            if (global_it != context.state.globals->end()) {
                array_ptr = global_it->second.backend_name;
            }
        }
        if (array_ptr.empty()) {
            array_ptr = EmitAggregateStackSlot(base,
                                              context.function_index,
                                              context.block_index,
                                              context.instruction_index,
                                              tag,
                                              context.output_lines);
        }
        context.output_lines.push_back(ptr_temp + " = getelementptr inbounds " + base.type.backend_name + ", ptr " +
                                       array_ptr + ", i64 0, i64 " + index_i64);
        return true;
    }

    if (base.type.backend_name == "{ptr, i64}") {
        const std::string data_ptr = LLVMTempName(context.function_index,
                                                  context.block_index,
                                                  context.instruction_index) + "." + std::string(tag) + ".data";
        context.output_lines.push_back(data_ptr + " = extractvalue {ptr, i64} " + base.text + ", 0");
        context.output_lines.push_back(ptr_temp + " = getelementptr inbounds " + element_type.backend_name + ", ptr " +
                                       data_ptr + ", i64 " + index_i64);
        return true;
    }

    ReportBackendError(context.source_path,
                       "LLVM bootstrap executable emission does not support " + std::string(operation) + " base type '" +
                           base.type.source_name + "' in function '" + context.state.function->name +
                           "' block '" + context.block.label + "'",
                       context.diagnostics);
    return false;
}

bool EmitLocalAddrGlobal(const mir::Instruction& instruction,
                         const ExecutableEmissionContext& context,
                         const BackendTypeInfo& type_info) {
    if (instruction.target_name.empty()) {
        ReportBackendError(context.source_path,
                           "local_addr global target is missing target_name metadata in function '" +
                               context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    const auto global_it = context.state.globals->find(instruction.target_name);
    if (global_it == context.state.globals->end()) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission references unknown global address target '" +
                               instruction.target_name + "' in function '" + context.state.function->name + "' block '" +
                               context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    RecordExecutableValue(context.state, instruction.result, global_it->second.backend_name, type_info);
    return true;
}

bool EmitLocalAddrField(const mir::Instruction& instruction,
                        const ExecutableEmissionContext& context,
                        const BackendTypeInfo& type_info) {
    if (instruction.target_name.empty()) {
        ReportBackendError(context.source_path,
                           "local_addr field target is missing target_name metadata in function '" +
                               context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }
    if (instruction.target_base_name.empty()) {
        ReportBackendError(context.source_path,
                           "local_addr field target is missing base-name metadata in function '" +
                               context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    std::string base_slot;
    if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kLocal) {
        if (!ResolveExecutableLocalSlot(context.state,
                                        instruction.target_base_name,
                                        context.block,
                                        context.source_path,
                                        context.diagnostics,
                                        base_slot)) {
            return false;
        }
    } else if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kGlobal) {
        const auto global_it = context.state.globals->find(instruction.target_base_name);
        if (global_it == context.state.globals->end()) {
            ReportBackendError(context.source_path,
                               "LLVM bootstrap executable emission references unknown global field-address base '" +
                                   instruction.target_base_name + "' in function '" + context.state.function->name +
                                   "' block '" + context.block.label + "'",
                               context.diagnostics);
            return false;
        }
        base_slot = global_it->second.backend_name;
    } else {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission currently supports only direct local/global field addresses in function '" +
                               context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    const auto field_index = FindFieldIndexForMemory(*context.state.module,
                                                     instruction.target_base_type,
                                                     instruction.target_name);
    if (!field_index.has_value()) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission could not resolve field-address target '" + instruction.target_name +
                               "' in function '" + context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    BackendTypeInfo base_type;
    if (!LowerInstructionType(*context.state.module,
                              instruction.target_base_type,
                              context.source_path,
                              context.diagnostics,
                              ExecutableFunctionBlockContext("local_addr field base", context.state, context.block),
                              base_type)) {
        return false;
    }

    const std::string field_ptr = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".field.addr";
    context.output_lines.push_back(field_ptr + " = getelementptr inbounds " + base_type.backend_name + ", ptr " + base_slot +
                                   ", i32 0, i32 " + std::to_string(*field_index));
    RecordExecutableValue(context.state, instruction.result, field_ptr, type_info);
    return true;
}

bool EmitLocalAddrIndex(const mir::Instruction& instruction,
                        const ExecutableEmissionContext& context,
                        const BackendTypeInfo& type_info) {
    ExecutableValue base;
    ExecutableValue index;
    if (!RequireOperandCount(instruction,
                             2,
                             "LLVM bootstrap executable emission",
                             "local_addr index",
                             context.state.function->name,
                             context.block,
                             context.source_path,
                             context.diagnostics) ||
        !ResolveExecutableValue(context.state, instruction.operands[0], context.block, context.source_path, context.diagnostics, base) ||
        !ResolveExecutableValue(context.state, instruction.operands[1], context.block, context.source_path, context.diagnostics, index)) {
        return false;
    }

    std::string index_i64;
    if (!ExtendIntegerToI64(index,
                            context.function_index,
                            context.block_index,
                            context.instruction_index,
                            "local_addr_index",
                            context.output_lines,
                            index_i64)) {
        return false;
    }

    BackendTypeInfo element_type;
    if (!LowerInstructionType(*context.state.module,
                              instruction.type.subtypes.front(),
                              context.source_path,
                              context.diagnostics,
                              ExecutableFunctionBlockContext("local_addr index element", context.state, context.block),
                              element_type)) {
        return false;
    }

    std::string ptr_temp;
    if (!ResolveArrayOrSliceElementPtr(base,
                                      index_i64,
                                      element_type,
                                      instruction.target_base_storage,
                                      instruction.target_base_name,
                                      context,
                                      "local_addr_index",
                                      "local_addr index",
                                      ptr_temp)) {
        return false;
    }

    RecordExecutableValue(context.state, instruction.result, ptr_temp, type_info);
    return true;
}

bool EmitLocalAddrLocal(const mir::Instruction& instruction,
                        const ExecutableEmissionContext& context,
                        const BackendTypeInfo& type_info) {
    std::string local_slot;
    if (!ResolveExecutableLocalSlot(context.state,
                                    instruction.target,
                                    context.block,
                                    context.source_path,
                                    context.diagnostics,
                                    local_slot)) {
        return false;
    }

    RecordExecutableValue(context.state, instruction.result, local_slot, type_info);
    return true;
}

bool EmitLocalAddrInstruction(const mir::Instruction& instruction,
                              const ExecutableEmissionContext& context) {
    if (!RequireInstructionResult(instruction, context.block, context.source_path, context.diagnostics)) {
        return false;
    }

    BackendTypeInfo type_info;
    if (!LowerInstructionType(*context.state.module,
                              instruction.type,
                              context.source_path,
                              context.diagnostics,
                              ExecutableFunctionBlockContext("local_addr", context.state, context.block),
                              type_info)) {
        return false;
    }

    if (instruction.target_kind == mir::Instruction::TargetKind::kGlobal) {
        return EmitLocalAddrGlobal(instruction, context, type_info);
    }
    if (instruction.target_kind == mir::Instruction::TargetKind::kField) {
        return EmitLocalAddrField(instruction, context, type_info);
    }
    if (instruction.target_kind == mir::Instruction::TargetKind::kIndex) {
        return EmitLocalAddrIndex(instruction, context, type_info);
    }
    return EmitLocalAddrLocal(instruction, context, type_info);
}

bool EmitStoreTargetGlobal(const mir::Instruction& instruction,
                           const ExecutableEmissionContext& context) {
    ExecutableValue operand;
    if (!RequireOperandCount(instruction,
                             1,
                             "LLVM bootstrap executable emission",
                             "store_target",
                             context.state.function->name,
                             context.block,
                             context.source_path,
                             context.diagnostics) ||
        !ResolveExecutableValue(context.state,
                                instruction.operands.front(),
                                context.block,
                                context.source_path,
                                context.diagnostics,
                                operand)) {
        return false;
    }

    const auto global_it = context.state.globals->find(instruction.target);
    if (global_it == context.state.globals->end()) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission references unknown global store target '" + instruction.target +
                               "' in function '" + context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    context.output_lines.push_back("store " + global_it->second.lowered_type.backend_name + " " + operand.text + ", ptr " +
                                   global_it->second.backend_name + ", align " +
                                   std::to_string(global_it->second.lowered_type.alignment));
    return true;
}

bool EmitStoreTargetField(const mir::Instruction& instruction,
                          const ExecutableEmissionContext& context) {
    ExecutableValue stored_value;
    ExecutableValue base;
    if (!ResolveExecutableOperands(instruction,
                                   context.block,
                                   context.source_path,
                                   context.diagnostics,
                                   context.state,
                                   stored_value,
                                   base)) {
        return false;
    }

    const auto base_type = ResolveExecutableFieldBaseType(*context.state.module, instruction.target_base_type, base);
    const auto field_index = base_type.has_value()
                                 ? FindFieldIndexForMemory(*context.state.module, *base_type, instruction.target_name)
                                 : std::nullopt;
    const auto field_storage_type = base_type.has_value()
                                        ? LowerAggregateFieldStorageType(*context.state.module, *base_type, instruction.target_name)
                                        : std::nullopt;
    if (!field_index.has_value()) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission could not resolve store_target field '" + instruction.target_name +
                               "' in function '" + context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }
    if (!field_storage_type.has_value()) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission could not lower store_target field storage for '" +
                               instruction.target_name + "' in function '" + context.state.function->name + "' block '" +
                               context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kNone ||
        instruction.target_base_name.empty()) {
        ReportBackendError(context.source_path,
                           "LLVM bootstrap executable emission requires a direct field store target in function '" +
                               context.state.function->name + "' block '" + context.block.label + "'",
                           context.diagnostics);
        return false;
    }

    std::string coerced_value = stored_value.text;
    if ((stored_value.type.backend_name == "i1" && field_storage_type->backend_name == "i8") ||
        (stored_value.type.backend_name == "i8" && field_storage_type->backend_name == "i1")) {
        if (!EmitValueRepresentationCoercion(stored_value,
                                             *field_storage_type,
                                             context.function_index,
                                             context.block_index,
                                             context.instruction_index,
                                             "store_field",
                                             context.output_lines,
                                             coerced_value)) {
            ReportBackendError(context.source_path,
                               "LLVM bootstrap executable emission could not coerce store_target field value for '" +
                                   instruction.target_name + "' in function '" + context.state.function->name + "' block '" +
                                   context.block.label + "'",
                               context.diagnostics);
            return false;
        }
    }

    const std::string updated_value = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".field";
    context.output_lines.push_back(updated_value + " = insertvalue " + base.type.backend_name + " " + base.text + ", " +
                                   field_storage_type->backend_name + " " + coerced_value + ", " +
                                   std::to_string(*field_index));

    if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kLocal) {
        const auto local_it = context.state.local_slots.find(instruction.target_base_name);
        if (local_it != context.state.local_slots.end()) {
            context.output_lines.push_back("store " + base.type.backend_name + " " + updated_value + ", ptr " + local_it->second +
                                           ", align " + std::to_string(base.type.alignment));
            return true;
        }
    }

    if (instruction.target_base_storage == mir::Instruction::StorageBaseKind::kGlobal) {
        const auto global_it = context.state.globals->find(instruction.target_base_name);
        if (global_it != context.state.globals->end()) {
            context.output_lines.push_back("store " + global_it->second.lowered_type.backend_name + " " + updated_value + ", ptr " +
                                           global_it->second.backend_name + ", align " +
                                           std::to_string(global_it->second.lowered_type.alignment));
            return true;
        }
    }

    ReportBackendError(context.source_path,
                       "LLVM bootstrap executable emission currently supports only direct local/global field store_target effects in function '" +
                           context.state.function->name + "' block '" + context.block.label + "'",
                       context.diagnostics);
    return false;
}

bool EmitStoreTargetPointer(const mir::Instruction& instruction,
                            const ExecutableEmissionContext& context) {
    ExecutableValue stored_value;
    ExecutableValue base;
    if (!ResolveExecutableOperands(instruction,
                                   context.block,
                                   context.source_path,
                                   context.diagnostics,
                                   context.state,
                                   stored_value,
                                   base)) {
        return false;
    }

    BackendTypeInfo stored_type;
    if (!LowerInstructionType(*context.state.module,
                              instruction.type,
                              context.source_path,
                              context.diagnostics,
                              ExecutableFunctionBlockContext("store_target", context.state, context.block),
                              stored_type)) {
        return false;
    }

    context.output_lines.push_back("store " + stored_type.backend_name + " " + stored_value.text + ", ptr " + base.text +
                                   ", align " + std::to_string(stored_type.alignment));
    return true;
}

bool EmitStoreTargetIndex(const mir::Instruction& instruction,
                          const ExecutableEmissionContext& context) {
    ExecutableValue stored_value;
    ExecutableValue base;
    ExecutableValue index;
    if (!ResolveExecutableOperands(instruction,
                                   context.block,
                                   context.source_path,
                                   context.diagnostics,
                                   context.state,
                                   stored_value,
                                   base,
                                   index)) {
        return false;
    }

    BackendTypeInfo stored_type;
    if (!LowerInstructionType(*context.state.module,
                              instruction.type,
                              context.source_path,
                              context.diagnostics,
                              ExecutableFunctionBlockContext("store_target", context.state, context.block),
                              stored_type)) {
        return false;
    }

    std::string index_i64;
    if (!ExtendIntegerToI64(index,
                            context.function_index,
                            context.block_index,
                            context.instruction_index,
                            "store_index",
                            context.output_lines,
                            index_i64)) {
        return false;
    }

    std::string ptr_temp;
    if (!ResolveArrayOrSliceElementPtr(base,
                                      index_i64,
                                      stored_type,
                                      instruction.target_base_storage,
                                      instruction.target_base_name,
                                      context,
                                      "store_array",
                                      "store_target index",
                                      ptr_temp)) {
        return false;
    }

    context.output_lines.push_back("store " + stored_type.backend_name + " " + stored_value.text + ", ptr " + ptr_temp +
                                   ", align " + std::to_string(stored_type.alignment));
    return true;
}

bool EmitStoreTargetInstruction(const mir::Instruction& instruction,
                                const ExecutableEmissionContext& context) {
    if (instruction.target_kind == mir::Instruction::TargetKind::kGlobal && !instruction.target.empty()) {
        return EmitStoreTargetGlobal(instruction, context);
    }
    if (instruction.target_kind == mir::Instruction::TargetKind::kField && instruction.operands.size() == 2) {
        return EmitStoreTargetField(instruction, context);
    }
    if (instruction.target_kind == mir::Instruction::TargetKind::kOther && instruction.operands.size() == 2 &&
        instruction.target_base_type.kind == sema::Type::Kind::kPointer) {
        return EmitStoreTargetPointer(instruction, context);
    }
    if (instruction.target_kind == mir::Instruction::TargetKind::kIndex && instruction.operands.size() == 3) {
        return EmitStoreTargetIndex(instruction, context);
    }

    ReportBackendError(context.source_path,
                       "LLVM bootstrap executable emission currently supports only global, direct field, and indexed store_target effects in function '" +
                           context.state.function->name + "' block '" + context.block.label + "'",
                       context.diagnostics);
    return false;
}

}  // namespace

bool 
EmitMemoryInstruction(const mir::Instruction& instruction, const ExecutableEmissionContext& context) {
    switch (instruction.kind) {
        case mir::Instruction::Kind::kLocalAddr: {
            return EmitLocalAddrInstruction(instruction, context);
        }

        case mir::Instruction::Kind::kLoadLocal: {
            if (!RequireInstructionResult(instruction, context.block, context.source_path, context.diagnostics)) {
                return false;
            }
            std::string local_slot;
            if (!ResolveExecutableLocalSlot(context.state,
                                            instruction.target,
                                            context.block,
                                            context.source_path,
                                            context.diagnostics,
                                            local_slot)) {
                return false;
            }
            BackendTypeInfo type_info;
            if (!LowerInstructionType(*context.state.module,
                                      instruction.type,
                                      context.source_path,
                                      context.diagnostics,
                                      ExecutableFunctionBlockContext("load_local", context.state, context.block),
                                      type_info)) {
                return false;
            }

            const std::string temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index);
            context.output_lines.push_back(temp + " = load " + type_info.backend_name + ", ptr " + local_slot + ", align " +
                                           std::to_string(type_info.alignment));
            RecordExecutableValue(context.state,
                                  instruction.result,
                                  temp,
                                  type_info,
                                  IsAggregateExecutableType(type_info) ? std::optional<std::string>(local_slot) : std::nullopt,
                                  instruction.type);
            return true;
        }

        case mir::Instruction::Kind::kStoreLocal: {
            const auto* local = FindFunctionLocal(*context.state.function, instruction.target);
            if (local == nullptr) {
                ReportBackendError(context.source_path,
                                   "LLVM bootstrap executable emission references unknown local '" + instruction.target +
                                       "' in function '" + context.state.function->name + "' block '" + context.block.label + "'",
                                   context.diagnostics);
                return false;
            }

            std::string local_slot;
            if (!ResolveExecutableLocalSlot(context.state,
                                            instruction.target,
                                            context.block,
                                            context.source_path,
                                            context.diagnostics,
                                            local_slot)) {
                return false;
            }

            ExecutableValue operand;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "store_local",
                                     context.state.function->name,
                                     context.block,
                                     context.source_path,
                                     context.diagnostics) ||
                !ResolveExecutableValue(context.state,
                                        instruction.operands.front(),
                                        context.block,
                                        context.source_path,
                                        context.diagnostics,
                                        operand)) {
                return false;
            }

            BackendTypeInfo type_info;
            if (!LowerInstructionType(*context.state.module,
                                      local->type,
                                      context.source_path,
                                      context.diagnostics,
                                      FunctionBlockContext("store_local destination", context.state.function->name, context.block),
                                      type_info)) {
                return false;
            }

            context.output_lines.push_back("store " + type_info.backend_name + " " + operand.text + ", ptr " + local_slot +
                                           ", align " + std::to_string(type_info.alignment));
            return true;
        }

        case mir::Instruction::Kind::kStoreTarget: {
            return EmitStoreTargetInstruction(instruction, context);
        }

        case mir::Instruction::Kind::kArrayToSlice: {
            if (!RequireInstructionResult(instruction, context.block, context.source_path, context.diagnostics)) {
                return false;
            }
            ExecutableValue base;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "array_to_slice",
                                     context.state.function->name,
                                     context.block,
                                     context.source_path,
                                     context.diagnostics) ||
                !ResolveExecutableValue(context.state,
                                        instruction.operands.front(),
                                        context.block,
                                        context.source_path,
                                        context.diagnostics,
                                        base)) {
                return false;
            }

            const auto length = ParseBackendArrayLength(base.type.backend_name);
            if (!length.has_value()) {
                ReportBackendError(context.source_path,
                                   "LLVM bootstrap executable emission requires statically known array length for array_to_slice in function '" +
                                       context.state.function->name + "' block '" + context.block.label + "'",
                                   context.diagnostics);
                return false;
            }

            BackendTypeInfo slice_type;
            if (!LowerInstructionType(*context.state.module,
                                      instruction.type,
                                      context.source_path,
                                      context.diagnostics,
                                      FunctionBlockContext("array_to_slice", context.state.function->name, context.block),
                                      slice_type)) {
                return false;
            }

            const std::string array_slot = base.storage_slot.has_value()
                                               ? *base.storage_slot
                                               : EmitAggregateStackSlot(base,
                                                                        context.function_index,
                                                                        context.block_index,
                                                                        context.instruction_index,
                                                                        "array",
                                                                        context.output_lines);
            const std::string ptr_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".ptr";
            const std::string init_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".init";
            const std::string result_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index);
            context.output_lines.push_back(ptr_temp + " = getelementptr inbounds " + base.type.backend_name + ", ptr " + array_slot + ", i64 0, i64 0");
            context.output_lines.push_back(init_temp + " = insertvalue " + slice_type.backend_name + " zeroinitializer, ptr " + ptr_temp + ", 0");
            context.output_lines.push_back(result_temp + " = insertvalue " + slice_type.backend_name + " " + init_temp + ", i64 " +
                                           std::to_string(*length) + ", 1");
            RecordExecutableValue(context.state, instruction.result, result_temp, slice_type);
            return true;
        }

        case mir::Instruction::Kind::kBufferToSlice: {
            if (!RequireInstructionResult(instruction, context.block, context.source_path, context.diagnostics)) {
                return false;
            }
            ExecutableValue base;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "buffer_to_slice",
                                     context.state.function->name,
                                     context.block,
                                     context.source_path,
                                     context.diagnostics) ||
                !ResolveExecutableValue(context.state,
                                        instruction.operands.front(),
                                        context.block,
                                        context.source_path,
                                        context.diagnostics,
                                        base)) {
                return false;
            }

            BackendTypeInfo slice_type;
            if (!LowerInstructionType(*context.state.module,
                                      instruction.type,
                                      context.source_path,
                                      context.diagnostics,
                                      FunctionBlockContext("buffer_to_slice", context.state.function->name, context.block),
                                      slice_type)) {
                return false;
            }

            const std::string ptr_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".ptr";
            const std::string len_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".len";
            const std::string init_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".init";
            const std::string result_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index);
            context.output_lines.push_back(ptr_temp + " = extractvalue " + base.type.backend_name + " " + base.text + ", 0");
            context.output_lines.push_back(len_temp + " = extractvalue " + base.type.backend_name + " " + base.text + ", 1");
            context.output_lines.push_back(init_temp + " = insertvalue " + slice_type.backend_name + " zeroinitializer, ptr " + ptr_temp + ", 0");
            context.output_lines.push_back(result_temp + " = insertvalue " + slice_type.backend_name + " " + init_temp + ", i64 " + len_temp + ", 1");
            RecordExecutableValue(context.state, instruction.result, result_temp, slice_type);
            return true;
        }

        case mir::Instruction::Kind::kBufferNew: {
            if (!RequireInstructionResult(instruction, context.block, context.source_path, context.diagnostics)) {
                return false;
            }
            ExecutableValue alloc;
            ExecutableValue cap;
            if (!RequireOperandCount(instruction,
                                     2,
                                     "LLVM bootstrap executable emission",
                                     "buffer_new",
                                     context.state.function->name,
                                     context.block,
                                     context.source_path,
                                     context.diagnostics) ||
                !ResolveExecutableValue(context.state, instruction.operands[0], context.block, context.source_path, context.diagnostics, alloc) ||
                !ResolveExecutableValue(context.state, instruction.operands[1], context.block, context.source_path, context.diagnostics, cap)) {
                return false;
            }

            BackendTypeInfo result_type;
            if (!LowerInstructionType(*context.state.module,
                                      instruction.type,
                                      context.source_path,
                                      context.diagnostics,
                                      FunctionBlockContext("buffer_new", context.state.function->name, context.block),
                                      result_type)) {
                return false;
            }

            const sema::Type buffer_type = instruction.type.subtypes.empty() ? sema::UnknownType() : instruction.type.subtypes.front();
            const auto element_ptr_type = FindFieldTypeForMemory(*context.state.module, buffer_type, "ptr");
            if (!element_ptr_type.has_value() || element_ptr_type->kind != sema::Type::Kind::kPointer || element_ptr_type->subtypes.empty()) {
                ReportBackendError(context.source_path,
                                   "LLVM bootstrap executable emission could not resolve Buffer element type for buffer_new in function '" +
                                       context.state.function->name + "' block '" + context.block.label + "'",
                                   context.diagnostics);
                return false;
            }

            BackendTypeInfo element_type;
            if (!LowerInstructionType(*context.state.module,
                                      element_ptr_type->subtypes.front(),
                                      context.source_path,
                                      context.diagnostics,
                                      FunctionBlockContext("buffer_new element", context.state.function->name, context.block),
                                      element_type)) {
                return false;
            }

            std::string cap_i64;
            if (!ExtendIntegerToI64(cap,
                                    context.function_index,
                                    context.block_index,
                                    context.instruction_index,
                                    "cap",
                                    context.output_lines,
                                    cap_i64)) {
                return false;
            }
            const std::string bytes_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".bytes";
            const std::string zero_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".zero";
            const std::string alloc_bytes = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".alloc_bytes";
            const std::string data_ptr = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".data_ptr";
            const std::string buffer_size_ptr = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".buf_size.ptr";
            const std::string buffer_size = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".buf_size";
            const std::string buffer_ptr = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".buf_ptr";
            context.output_lines.push_back(bytes_temp + " = mul i64 " + cap_i64 + ", " + std::to_string(element_type.size));
            context.output_lines.push_back(zero_temp + " = icmp eq i64 " + bytes_temp + ", 0");
            context.output_lines.push_back(alloc_bytes + " = select i1 " + zero_temp + ", i64 1, i64 " + bytes_temp);
            context.output_lines.push_back(data_ptr + " = call ptr @malloc(i64 " + alloc_bytes + ")");
            context.output_lines.push_back(buffer_size_ptr + " = getelementptr inbounds " + std::string(kBufferStructType) + ", ptr null, i64 1");
            context.output_lines.push_back(buffer_size + " = ptrtoint ptr " + buffer_size_ptr + " to i64");
            context.output_lines.push_back(buffer_ptr + " = call ptr @malloc(i64 " + buffer_size + ")");

            const std::string ptr_slot = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".ptr_slot";
            const std::string len_slot = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".len_slot";
            const std::string cap_slot = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".cap_slot";
            const std::string alloc_slot = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".alloc_slot";
            context.output_lines.push_back(ptr_slot + " = getelementptr inbounds " + std::string(kBufferStructType) + ", ptr " + buffer_ptr + ", i64 0, i32 0");
            context.output_lines.push_back(len_slot + " = getelementptr inbounds " + std::string(kBufferStructType) + ", ptr " + buffer_ptr + ", i64 0, i32 1");
            context.output_lines.push_back(cap_slot + " = getelementptr inbounds " + std::string(kBufferStructType) + ", ptr " + buffer_ptr + ", i64 0, i32 2");
            context.output_lines.push_back(alloc_slot + " = getelementptr inbounds " + std::string(kBufferStructType) + ", ptr " + buffer_ptr + ", i64 0, i32 3");
            context.output_lines.push_back("store ptr " + data_ptr + ", ptr " + ptr_slot + ", align 8");
            context.output_lines.push_back("store i64 " + cap_i64 + ", ptr " + len_slot + ", align 8");
            context.output_lines.push_back("store i64 " + cap_i64 + ", ptr " + cap_slot + ", align 8");
            context.output_lines.push_back("store ptr " + alloc.text + ", ptr " + alloc_slot + ", align 8");
            RecordExecutableValue(context.state, instruction.result, buffer_ptr, result_type);
            return true;
        }

        case mir::Instruction::Kind::kBufferFree: {
            ExecutableValue buffer;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "buffer_free",
                                     context.state.function->name,
                                     context.block,
                                     context.source_path,
                                     context.diagnostics) ||
                !ResolveExecutableValue(context.state,
                                        instruction.operands.front(),
                                        context.block,
                                        context.source_path,
                                        context.diagnostics,
                                        buffer)) {
                return false;
            }
            const std::string loaded = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".buffer";
            const std::string data_ptr = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".data_ptr";
            context.output_lines.push_back(loaded + " = load " + std::string(kBufferStructType) + ", ptr " + buffer.text + ", align 8");
            context.output_lines.push_back(data_ptr + " = extractvalue " + std::string(kBufferStructType) + " " + loaded + ", 0");
            context.output_lines.push_back("call void @free(ptr " + data_ptr + ")");
            context.output_lines.push_back("call void @free(ptr " + buffer.text + ")");
            return true;
        }

        case mir::Instruction::Kind::kField: {
            if (!RequireInstructionResult(instruction, context.block, context.source_path, context.diagnostics)) {
                return false;
            }
            ExecutableValue base;
            if (!RequireOperandCount(instruction,
                                     1,
                                     "LLVM bootstrap executable emission",
                                     "field",
                                     context.state.function->name,
                                     context.block,
                                     context.source_path,
                                     context.diagnostics) ||
                !ResolveExecutableValue(context.state,
                                        instruction.operands.front(),
                                        context.block,
                                        context.source_path,
                                        context.diagnostics,
                                        base)) {
                return false;
            }

            const std::string field_name = instruction.target_name.empty() ? instruction.target : instruction.target_name;
            const auto field_base_type = ResolveExecutableFieldBaseType(*context.state.module,
                                                                       instruction.target_base_type,
                                                                       base);
            const auto field_index = field_base_type.has_value()
                                         ? FindFieldIndexForMemory(*context.state.module, *field_base_type, field_name)
                                         : std::nullopt;
            if (!field_index.has_value()) {
                ReportBackendError(context.source_path,
                                   "LLVM bootstrap executable emission could not resolve field '" + field_name + "' in function '" +
                                       context.state.function->name + "' block '" + context.block.label + "'",
                                   context.diagnostics);
                return false;
            }

            BackendTypeInfo field_type;
            if (!LowerInstructionType(*context.state.module,
                                      instruction.type,
                                      context.source_path,
                                      context.diagnostics,
                                      FunctionBlockContext("field", context.state.function->name, context.block),
                                      field_type)) {
                return false;
            }

            const auto storage_type = field_base_type.has_value()
                                          ? LowerAggregateFieldStorageType(*context.state.module, *field_base_type, field_name)
                                          : std::nullopt;
            if (!storage_type.has_value()) {
                ReportBackendError(context.source_path,
                                   "LLVM bootstrap executable emission could not lower field storage for '" + field_name +
                                       "' in function '" + context.state.function->name + "' block '" + context.block.label + "'",
                                   context.diagnostics);
                return false;
            }

            const std::string extracted = LLVMTempName(context.function_index, context.block_index, context.instruction_index) + ".field";
            context.output_lines.push_back(extracted + " = extractvalue " + base.type.backend_name + " " + base.text + ", " +
                                           std::to_string(*field_index));

            ExecutableValue extracted_value {
                .text = extracted,
                .type = *storage_type,
            };
            std::string coerced_value;
            if (!EmitValueRepresentationCoercion(extracted_value,
                                                 field_type,
                                                 context.function_index,
                                                 context.block_index,
                                                 context.instruction_index,
                                                 "field_value",
                                                 context.output_lines,
                                                 coerced_value)) {
                ReportBackendError(context.source_path,
                                   "LLVM bootstrap executable emission could not coerce extracted field '" + field_name +
                                       "' in function '" + context.state.function->name + "' block '" + context.block.label + "'",
                                   context.diagnostics);
                return false;
            }

            RecordExecutableValue(context.state, instruction.result, coerced_value, field_type, std::nullopt, instruction.type);
            return true;
        }

        case mir::Instruction::Kind::kIndex: {
            if (!RequireInstructionResult(instruction, context.block, context.source_path, context.diagnostics)) {
                return false;
            }
            ExecutableValue base;
            ExecutableValue index;
            if (!RequireOperandCount(instruction,
                                     2,
                                     "LLVM bootstrap executable emission",
                                     "index",
                                     context.state.function->name,
                                     context.block,
                                     context.source_path,
                                     context.diagnostics) ||
                !ResolveExecutableValue(context.state, instruction.operands[0], context.block, context.source_path, context.diagnostics, base) ||
                !ResolveExecutableValue(context.state, instruction.operands[1], context.block, context.source_path, context.diagnostics, index)) {
                return false;
            }

            BackendTypeInfo result_type;
            if (!LowerInstructionType(*context.state.module,
                                      instruction.type,
                                      context.source_path,
                                      context.diagnostics,
                                      FunctionBlockContext("index", context.state.function->name, context.block),
                                      result_type)) {
                return false;
            }

            std::string index_i64;
            if (!ExtendIntegerToI64(index,
                                    context.function_index,
                                    context.block_index,
                                    context.instruction_index,
                                    "index",
                                    context.output_lines,
                                    index_i64)) {
                return false;
            }

            std::string ptr_temp;
            if (!ResolveArrayOrSliceElementPtr(base,
                                              index_i64,
                                              result_type,
                                              mir::Instruction::StorageBaseKind::kNone,
                                              "",
                                              context,
                                              "array",
                                              "index",
                                              ptr_temp)) {
                return false;
            }

            const std::string result_temp = LLVMTempName(context.function_index, context.block_index, context.instruction_index);
            context.output_lines.push_back(result_temp + " = load " + result_type.backend_name + ", ptr " + ptr_temp + ", align " +
                                           std::to_string(result_type.alignment));
            RecordExecutableValue(context.state, instruction.result, result_temp, result_type, std::nullopt, instruction.type);
            return true;
        }

        default:
            MC_UNREACHABLE("unexpected non-memory instruction in executable memory lowering");
    }
}

}  // namespace mc::codegen_llvm