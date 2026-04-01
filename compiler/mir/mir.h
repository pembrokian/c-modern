#ifndef C_MODERN_COMPILER_MIR_MIR_H_
#define C_MODERN_COMPILER_MIR_MIR_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "compiler/ast/ast.h"
#include "compiler/sema/check.h"
#include "compiler/sema/type.h"
#include "compiler/support/diagnostics.h"

namespace mc::mir {

struct VariantDecl {
    std::string name;
    std::vector<std::pair<std::string, sema::Type>> payload_fields;
};

struct TypeDecl {
    enum class Kind {
        kStruct,
        kEnum,
        kDistinct,
        kAlias,
    };

    Kind kind = Kind::kStruct;
    std::string name;
    std::vector<std::string> type_params;
    std::vector<std::pair<std::string, sema::Type>> fields;
    std::vector<VariantDecl> variants;
    sema::Type aliased_type;
};

struct GlobalDecl {
    bool is_const = false;
    std::vector<std::string> names;
    sema::Type type;
    std::vector<std::string> initializers;
};

struct Local {
    std::string name;
    sema::Type type;
    bool is_parameter = false;
    bool is_mutable = true;
};

struct Instruction {
    enum class TargetKind {
        kNone,
        kFunction,
        kGlobal,
        kField,
        kDerefField,
        kIndex,
        kOther,
    };

    enum class ArithmeticSemantics {
        kNone,
        kWrap,
    };

    enum class Kind {
        kConst,
        kLoadLocal,
        kStoreLocal,
        kStoreTarget,
        kSymbolRef,
        kBoundsCheck,
        kDivCheck,
        kShiftCheck,
        kUnary,
        kBinary,
        kConvert,
        kConvertNumeric,
        kConvertDistinct,
        kPointerToInt,
        kIntToPointer,
        kArrayToSlice,
        kBufferToSlice,
        kBufferNew,
        kBufferFree,
        kSliceFromBuffer,
        kCall,
        kVolatileLoad,
        kVolatileStore,
        kAtomicLoad,
        kAtomicStore,
        kAtomicExchange,
        kAtomicCompareExchange,
        kAtomicFetchAdd,
        kField,
        kIndex,
        kSlice,
        kAggregateInit,
        kVariantMatch,
        kVariantExtract,
    };

    Kind kind = Kind::kConst;
    std::string result;
    sema::Type type;
    std::string op;
    std::string target;
    TargetKind target_kind = TargetKind::kNone;
    std::string target_name;
    std::string target_display;
    sema::Type target_base_type;
    std::vector<sema::Type> target_aux_types;
    std::int64_t target_index = -1;
    std::vector<std::string> operands;
    std::vector<std::string> field_names;
    ArithmeticSemantics arithmetic_semantics = ArithmeticSemantics::kNone;
};

struct Terminator {
    enum class Kind {
        kNone,
        kReturn,
        kBranch,
        kCondBranch,
    };

    Kind kind = Kind::kNone;
    std::vector<std::string> values;
    std::string true_target;
    std::string false_target;
};

struct BasicBlock {
    std::string label;
    std::vector<Instruction> instructions;
    Terminator terminator;
};

struct Function {
    std::string name;
    bool is_extern = false;
    std::string extern_abi;
    std::vector<std::string> type_params;
    std::vector<Local> locals;
    std::vector<sema::Type> return_types;
    std::vector<BasicBlock> blocks;
};

struct Module {
    std::vector<std::string> imports;
    std::vector<TypeDecl> type_decls;
    std::vector<GlobalDecl> globals;
    std::vector<Function> functions;
};

struct LowerResult {
    std::unique_ptr<Module> module;
    bool ok = false;
};

LowerResult LowerSourceFile(const ast::SourceFile& source_file,
                            const sema::Module& sema_module,
                            const std::filesystem::path& file_path,
                            support::DiagnosticSink& diagnostics);

bool ValidateModule(const Module& module,
                    const std::filesystem::path& file_path,
                    support::DiagnosticSink& diagnostics);

std::string DumpModule(const Module& module);

}  // namespace mc::mir

#endif  // C_MODERN_COMPILER_MIR_MIR_H_