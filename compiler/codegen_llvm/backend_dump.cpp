#include "compiler/codegen_llvm/backend_internal.h"

#include <sstream>

namespace mc::codegen_llvm {

namespace {

void WriteIndent(std::ostringstream& stream, int indent) {
    for (int count = 0; count < indent; ++count) {
        stream << "  ";
    }
}

void WriteLine(std::ostringstream& stream, int indent, const std::string& text) {
    WriteIndent(stream, indent);
    stream << text << '\n';
}

}  // namespace

std::string DumpModule(const BackendModule& module) {
    std::ostringstream stream;
    stream << "BackendModule surface=" << module.inspect_surface << " triple=" << module.target.triple
           << " target_family=" << module.target.target_family << " object_format=" << module.target.object_format
           << " hosted=" << (module.target.hosted ? "true" : "false") << '\n';

    for (const auto& global : module.globals) {
        std::ostringstream global_line;
        global_line << "Global source=" << global.source_name << " backend=" << global.backend_name
                    << " type=" << FormatTypeInfo(global.lowered_type);
        if (global.is_const) {
            global_line << " const";
        }
        if (global.is_extern) {
            global_line << " extern";
        }
        if (!global.initializers.empty()) {
            global_line << " init=[";
            for (std::size_t index = 0; index < global.initializers.size(); ++index) {
                if (index > 0) {
                    global_line << ", ";
                }
                global_line << global.initializers[index];
            }
            global_line << "]";
        }
        WriteLine(stream, 1, global_line.str());
    }

    for (const auto& function : module.functions) {
        std::ostringstream header;
        header << "Function source=" << function.source_name << " backend=" << function.backend_name;
        if (!function.backend_return_types.empty()) {
            header << " returns=" << FormatReturnTypes(function.backend_return_types);
        }
        WriteLine(stream, 1, header.str());

        for (const auto& local : function.locals) {
            std::ostringstream local_line;
            local_line << "Local source=" << local.source_name << " backend=" << local.backend_name
                       << " type=" << FormatTypeInfo(local.lowered_type);
            if (local.is_parameter) {
                local_line << " param";
            }
            if (!local.is_mutable) {
                local_line << " readonly";
            }
            WriteLine(stream, 2, local_line.str());
        }

        for (const auto& block : function.blocks) {
            WriteLine(stream, 2, "Block source=" + block.source_label + " backend=" + block.backend_label);
            for (const auto& instruction : block.instructions) {
                WriteLine(stream, 3, instruction);
            }
            WriteLine(stream, 3, block.terminator);
        }
    }

    return stream.str();
}

}  // namespace mc::codegen_llvm