#include "compiler/mir/mir_internal.h"

#include <sstream>

namespace mc::mir {

std::string DumpModule(const Module& module) {
    std::ostringstream stream;
    WriteLine(stream, 0, "Module");

    for (const auto& import_name : module.imports) {
        WriteLine(stream, 1, "Import name=" + import_name);
    }

    for (const auto& type_decl : module.type_decls) {
        WriteLine(stream, 1, std::string("TypeDecl kind=") + std::string(ToString(type_decl.kind)) + " name=" + type_decl.name);
        if (!type_decl.type_params.empty()) {
            std::ostringstream type_params;
            type_params << "typeParams=[";
            for (std::size_t index = 0; index < type_decl.type_params.size(); ++index) {
                if (index > 0) {
                    type_params << ", ";
                }
                type_params << type_decl.type_params[index];
            }
            type_params << ']';
            WriteLine(stream, 2, type_params.str());
        }
        if (type_decl.is_packed) {
            WriteLine(stream, 2, "attributes=[packed]");
        }
        if (type_decl.is_abi_c) {
            WriteLine(stream, 2, "attributes=[abi(c)]");
        }
        for (const auto& field : type_decl.fields) {
            WriteLine(stream, 2, "Field name=" + field.first + " type=" + sema::FormatType(field.second));
        }
        for (const auto& variant : type_decl.variants) {
            WriteLine(stream, 2, "Variant name=" + variant.name);
            for (const auto& payload_field : variant.payload_fields) {
                WriteLine(stream, 3, "PayloadField name=" + payload_field.first + " type=" + sema::FormatType(payload_field.second));
            }
        }
        if (!sema::IsUnknown(type_decl.aliased_type)) {
            WriteLine(stream, 2, "AliasedType=" + sema::FormatType(type_decl.aliased_type));
        }
    }

    for (const auto& global : module.globals) {
        std::ostringstream header;
        header << (global.is_const ? "ConstGlobal names=[" : "VarGlobal names=[");
        for (std::size_t index = 0; index < global.names.size(); ++index) {
            if (index > 0) {
                header << ", ";
            }
            header << global.names[index];
        }
        header << ']';
        if (!sema::IsUnknown(global.type)) {
            header << " type=" << sema::FormatType(global.type);
        }
        if (global.is_extern) {
            header << " extern";
        }
        WriteLine(stream, 1, header.str());
        for (const auto& initializer : global.initializers) {
            WriteLine(stream, 2, "init " + initializer);
        }
    }

    for (const auto& function : module.functions) {
        std::ostringstream header;
        header << "Function name=" << function.name;
        if (function.is_extern) {
            header << " extern=" << function.extern_abi;
        }
        if (!function.return_types.empty()) {
            header << " returns=[";
            for (std::size_t index = 0; index < function.return_types.size(); ++index) {
                if (index > 0) {
                    header << ", ";
                }
                header << sema::FormatType(function.return_types[index]);
            }
            header << ']';
        }
        WriteLine(stream, 1, header.str());

        for (const auto& local : function.locals) {
            std::ostringstream local_line;
            local_line << "Local name=" << local.name << " type=" << sema::FormatType(local.type);
            if (local.is_parameter) {
                local_line << " param";
                if (local.is_noalias) {
                    local_line << " noalias";
                }
            }
            if (!local.is_mutable) {
                local_line << " readonly";
            }
            WriteLine(stream, 2, local_line.str());
        }

        for (const auto& block : function.blocks) {
            WriteLine(stream, 2, "Block label=" + block.label);
            for (const auto& instruction : block.instructions) {
                std::ostringstream line;
                line << ToString(instruction.kind);
                if (!instruction.result.empty()) {
                    line << ' ' << instruction.result << ':' << sema::FormatType(instruction.type);
                }
                const std::string atomic_metadata = AtomicMetadataText(instruction);
                if (!atomic_metadata.empty()) {
                    line << " op=" << atomic_metadata;
                } else if (!instruction.op.empty()) {
                    line << " op=" << instruction.op;
                }
                if (instruction.arithmetic_semantics != Instruction::ArithmeticSemantics::kNone) {
                    line << " arithmetic=" << ToString(instruction.arithmetic_semantics);
                }
                if (!instruction.target.empty()) {
                    line << " target=" << instruction.target;
                }
                if (instruction.target_kind != Instruction::TargetKind::kNone) {
                    line << " target_kind=" << ToString(instruction.target_kind);
                }
                if (!instruction.target_name.empty()) {
                    line << " target_name=" << instruction.target_name;
                }
                if (!instruction.target_display.empty() && instruction.target_display != instruction.target) {
                    line << " target_display=" << instruction.target_display;
                }
                if (!sema::IsUnknown(instruction.target_base_type)) {
                    line << " target_base=" << sema::FormatType(instruction.target_base_type);
                }
                if (instruction.target_index >= 0) {
                    line << " target_index=" << instruction.target_index;
                }
                if (!instruction.operands.empty()) {
                    line << " operands=[";
                    for (std::size_t index = 0; index < instruction.operands.size(); ++index) {
                        if (index > 0) {
                            line << ", ";
                        }
                        line << instruction.operands[index];
                    }
                    line << ']';
                }
                if (!instruction.target_aux_types.empty()) {
                    line << " target_types=[";
                    for (std::size_t index = 0; index < instruction.target_aux_types.size(); ++index) {
                        if (index > 0) {
                            line << ", ";
                        }
                        line << sema::FormatType(instruction.target_aux_types[index]);
                    }
                    line << ']';
                }
                if (!instruction.field_names.empty()) {
                    line << " fields=[";
                    for (std::size_t index = 0; index < instruction.field_names.size(); ++index) {
                        if (index > 0) {
                            line << ", ";
                        }
                        line << instruction.field_names[index];
                    }
                    line << ']';
                }
                WriteLine(stream, 3, line.str());
            }

            std::ostringstream terminator;
            switch (block.terminator.kind) {
                case Terminator::Kind::kNone:
                    terminator << "terminator none";
                    break;
                case Terminator::Kind::kReturn:
                    terminator << "terminator return";
                    if (!block.terminator.values.empty()) {
                        terminator << " values=[";
                        for (std::size_t index = 0; index < block.terminator.values.size(); ++index) {
                            if (index > 0) {
                                terminator << ", ";
                            }
                            terminator << block.terminator.values[index];
                        }
                        terminator << ']';
                    }
                    break;
                case Terminator::Kind::kBranch:
                    terminator << "terminator branch target=" << block.terminator.true_target;
                    break;
                case Terminator::Kind::kCondBranch:
                    terminator << "terminator cond value="
                               << (block.terminator.values.empty() ? std::string("<?>") : block.terminator.values.front())
                               << " true=" << block.terminator.true_target << " false=" << block.terminator.false_target;
                    break;
            }
            WriteLine(stream, 3, terminator.str());
        }
    }

    return stream.str();
}

}  // namespace mc::mir