#include <cstdlib>
#include <iostream>
#include <string>

#include "compiler/codegen_llvm/backend.h"
#include "compiler/mir/mir.h"
#include "compiler/sema/type.h"
#include "compiler/support/diagnostics.h"

namespace {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "test failure: " << message << '\n';
        std::exit(1);
    }
}

mc::mir::Module MakeMinimalSupportedModule() {
    mc::mir::Instruction constant {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v0",
        .type = mc::sema::NamedType("i32"),
        .op = "0",
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {constant},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v0"},
        },
    };

    mc::mir::Function main_function {
        .name = "main",
        .return_types = {mc::sema::NamedType("i32")},
        .blocks = {entry},
    };

    return {
        .functions = {main_function},
    };
}

void TestBootstrapTargetConfigIsStable() {
    const auto target = mc::codegen_llvm::BootstrapTargetConfig();
    Expect(target.triple == "arm64-apple-darwin25.4.0", "bootstrap target triple should stay frozen");
    Expect(target.target_family == "arm64-apple-darwin", "bootstrap target family should stay frozen");
    Expect(target.object_format == "macho", "bootstrap object format should match Darwin host output");
    Expect(target.hosted, "bootstrap target should stay hosted");
}

void TestLowerModuleAcceptsStageOneSlice() {
    mc::support::DiagnosticSink diagnostics;
    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };

    const auto result = mc::codegen_llvm::LowerModule(MakeMinimalSupportedModule(), "tests/cases/hello.mc", options, diagnostics);
        Expect(result.ok, "supported bootstrap MIR should lower successfully");
        Expect(!diagnostics.HasErrors(), "supported bootstrap MIR should not emit backend diagnostics");
    Expect(result.module != nullptr, "successful lowering should materialize a backend module");
    Expect(result.module->functions.size() == 1, "backend module should preserve the function count");
    Expect(result.module->functions[0].source_name == "main", "backend module should preserve function source names");
    Expect(result.module->functions[0].backend_name == "@main", "backend module should assign deterministic backend names");
        Expect(result.module->functions[0].backend_return_types.size() == 1,
            "backend module should record target representation for return types");
        Expect(result.module->functions[0].backend_return_types[0].backend_name == "i32",
            "i32 returns should map to i32 backend representation");
}

void TestLowerModuleLowersStoreBranchAndCompare() {
    mc::mir::Instruction zero {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v0",
        .type = mc::sema::IntLiteralType(),
        .op = "0",
    };

    mc::mir::Instruction init_result {
        .kind = mc::mir::Instruction::Kind::kStoreLocal,
        .target = "result",
        .operands = {"%v0"},
    };

    mc::mir::Instruction load_value {
        .kind = mc::mir::Instruction::Kind::kLoadLocal,
        .result = "%v1",
        .type = mc::sema::NamedType("i32"),
        .target = "value",
    };

    mc::mir::Instruction one {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v2",
        .type = mc::sema::IntLiteralType(),
        .op = "1",
    };

    mc::mir::Instruction compare {
        .kind = mc::mir::Instruction::Kind::kBinary,
        .result = "%v3",
        .type = mc::sema::BoolType(),
        .op = "==",
        .operands = {"%v1", "%v2"},
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {zero, init_result, load_value, one, compare},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kCondBranch,
            .values = {"%v3"},
            .true_target = "if_then1",
            .false_target = "if_merge2",
        },
    };

    mc::mir::Instruction two {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v4",
        .type = mc::sema::IntLiteralType(),
        .op = "2",
    };

    mc::mir::Instruction set_result {
        .kind = mc::mir::Instruction::Kind::kStoreLocal,
        .target = "result",
        .operands = {"%v4"},
    };

    mc::mir::BasicBlock then_block {
        .label = "if_then1",
        .instructions = {two, set_result},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kBranch,
            .true_target = "if_merge2",
        },
    };

    mc::mir::Instruction load_result {
        .kind = mc::mir::Instruction::Kind::kLoadLocal,
        .result = "%v5",
        .type = mc::sema::NamedType("i32"),
        .target = "result",
    };

    mc::mir::BasicBlock merge_block {
        .label = "if_merge2",
        .instructions = {load_result},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v5"},
        },
    };

    mc::mir::Function function {
        .name = "choose",
        .locals = {
            {.name = "value", .type = mc::sema::NamedType("i32"), .is_parameter = true, .is_mutable = false},
            {.name = "result", .type = mc::sema::NamedType("i32")},
        },
        .return_types = {mc::sema::NamedType("i32")},
        .blocks = {entry, then_block, merge_block},
    };

    mc::mir::Module module {
        .functions = {function},
    };

    mc::support::DiagnosticSink diagnostics;
    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };

    const auto result = mc::codegen_llvm::LowerModule(module, "tests/cases/hello.mc", options, diagnostics);
    Expect(result.ok, "store_local and branch MIR should lower successfully");
    Expect(!diagnostics.HasErrors(), "supported control-flow MIR should not emit backend diagnostics");

    const auto dump = mc::codegen_llvm::DumpModule(*result.module);
    Expect(dump.find("store i32 %t0_0_0 -> %local.result") != std::string::npos,
           "backend dump should lower store_local to an explicit store line");
    Expect(dump.find("binary == bool [repr=i1, size=1, align=1] %t0_0_2, %t0_0_3") != std::string::npos,
           "backend dump should lower comparison-shaped binary instructions");
    Expect(dump.find("br bb0_2.if_merge2") != std::string::npos,
           "backend dump should lower plain branch terminators");
}

void TestLowerModuleRejectsUnsupportedTarget() {
    mc::support::DiagnosticSink diagnostics;
    mc::codegen_llvm::LowerOptions options {
        .target = {
            .triple = "x86_64-unknown-linux-gnu",
            .target_family = "x86_64-linux",
            .object_format = "elf",
            .hosted = true,
        },
    };

    const auto result = mc::codegen_llvm::LowerModule(MakeMinimalSupportedModule(), "tests/cases/hello.mc", options, diagnostics);
    Expect(!result.ok, "unsupported targets should fail backend preflight");
    Expect(diagnostics.HasErrors(), "unsupported targets should emit a direct backend diagnostic");
    Expect(diagnostics.Render().find("only supports hosted 'arm64-apple-darwin'") != std::string::npos,
           "target diagnostic should name the frozen bootstrap target family");
}

void TestLowerModuleRejectsUnsupportedInstruction() {
    auto module = MakeMinimalSupportedModule();
    module.functions[0].blocks[0].instructions.push_back({
        .kind = mc::mir::Instruction::Kind::kVariantMatch,
        .operands = {"%v0", "%v0"},
    });

    mc::support::DiagnosticSink diagnostics;
    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };

    const auto result = mc::codegen_llvm::LowerModule(module, "tests/cases/hello.mc", options, diagnostics);
    Expect(!result.ok, "out-of-scope MIR should fail backend preflight");
    Expect(diagnostics.HasErrors(), "out-of-scope MIR should emit backend diagnostics");
    Expect(diagnostics.Render().find("does not support MIR instruction 'variant_match'") != std::string::npos,
           "instruction diagnostic should name the unsupported MIR opcode");
}

void TestLowerModuleLowersCheckedIntegerSemantics() {
    mc::mir::Instruction lhs {
        .kind = mc::mir::Instruction::Kind::kLoadLocal,
        .result = "%v0",
        .type = mc::sema::NamedType("i32"),
        .target = "lhs",
    };

    mc::mir::Instruction rhs {
        .kind = mc::mir::Instruction::Kind::kLoadLocal,
        .result = "%v1",
        .type = mc::sema::NamedType("i32"),
        .target = "rhs",
    };

    mc::mir::Instruction div_check {
        .kind = mc::mir::Instruction::Kind::kDivCheck,
        .op = "/",
        .operands = {"%v0", "%v1"},
    };

    mc::mir::Instruction div {
        .kind = mc::mir::Instruction::Kind::kBinary,
        .result = "%v2",
        .type = mc::sema::NamedType("i32"),
        .op = "/",
        .operands = {"%v0", "%v1"},
    };

    mc::mir::Instruction count {
        .kind = mc::mir::Instruction::Kind::kLoadLocal,
        .result = "%v3",
        .type = mc::sema::NamedType("i32"),
        .target = "count",
    };

    mc::mir::Instruction shift_check {
        .kind = mc::mir::Instruction::Kind::kShiftCheck,
        .op = "<<",
        .operands = {"%v2", "%v3"},
    };

    mc::mir::Instruction shift {
        .kind = mc::mir::Instruction::Kind::kBinary,
        .result = "%v4",
        .type = mc::sema::NamedType("i32"),
        .op = "<<",
        .operands = {"%v2", "%v3"},
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {lhs, rhs, div_check, div, count, shift_check, shift},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v4"},
        },
    };

    mc::mir::Function function {
        .name = "checked_math",
        .locals = {
            {.name = "lhs", .type = mc::sema::NamedType("i32"), .is_parameter = true, .is_mutable = false},
            {.name = "rhs", .type = mc::sema::NamedType("i32"), .is_parameter = true, .is_mutable = false},
            {.name = "count", .type = mc::sema::NamedType("i32"), .is_parameter = true, .is_mutable = false},
        },
        .return_types = {mc::sema::NamedType("i32")},
        .blocks = {entry},
    };

    mc::mir::Module module {
        .functions = {function},
    };

    mc::support::DiagnosticSink diagnostics;
    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };

    const auto result = mc::codegen_llvm::LowerModule(module, "tests/cases/hello.mc", options, diagnostics);
    Expect(result.ok, "checked integer MIR should lower successfully");
    Expect(!diagnostics.HasErrors(), "checked integer MIR should not emit backend diagnostics");

    const auto dump = mc::codegen_llvm::DumpModule(*result.module);
    Expect(dump.find("check.div / lhs=%t0_0_0 rhs=%t0_0_1") != std::string::npos,
           "backend dump should keep explicit division checks");
    Expect(dump.find("check.shift << value=%t0_0_3 count=%t0_0_4") != std::string::npos,
           "backend dump should keep explicit shift checks");
    Expect(dump.find("binary / i32 [repr=i32, size=4, align=4]") != std::string::npos,
           "binary division should use target scalar representation");
}

void TestLowerModuleLowersRepresentationPreservingConversions() {
    mc::mir::TypeDecl user_id {
        .kind = mc::mir::TypeDecl::Kind::kDistinct,
        .name = "UserId",
        .aliased_type = mc::sema::NamedType("i32"),
    };

    mc::mir::Instruction load_raw {
        .kind = mc::mir::Instruction::Kind::kLoadLocal,
        .result = "%v0",
        .type = mc::sema::NamedType("i32"),
        .target = "raw",
    };

    mc::mir::Instruction promote {
        .kind = mc::mir::Instruction::Kind::kConvertDistinct,
        .result = "%v1",
        .type = mc::sema::NamedType("UserId"),
        .target = "UserId",
        .operands = {"%v0"},
    };

    mc::mir::BasicBlock promote_entry {
        .label = "entry0",
        .instructions = {load_raw, promote},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v1"},
        },
    };

    mc::mir::Function promote_fn {
        .name = "promote",
        .locals = {
            {.name = "raw", .type = mc::sema::NamedType("i32"), .is_parameter = true, .is_mutable = false},
        },
        .return_types = {mc::sema::NamedType("UserId")},
        .blocks = {promote_entry},
    };

    mc::mir::Instruction load_ptr {
        .kind = mc::mir::Instruction::Kind::kLoadLocal,
        .result = "%v0",
        .type = mc::sema::PointerType(mc::sema::NamedType("i32")),
        .target = "ptr",
    };

    mc::mir::Instruction to_int {
        .kind = mc::mir::Instruction::Kind::kPointerToInt,
        .result = "%v1",
        .type = mc::sema::NamedType("uintptr"),
        .target = "uintptr",
        .operands = {"%v0"},
    };

    mc::mir::Instruction to_ptr {
        .kind = mc::mir::Instruction::Kind::kIntToPointer,
        .result = "%v2",
        .type = mc::sema::PointerType(mc::sema::NamedType("i32")),
        .target = "*i32",
        .operands = {"%v1"},
    };

    mc::mir::BasicBlock roundtrip_entry {
        .label = "entry0",
        .instructions = {load_ptr, to_int, to_ptr},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v2"},
        },
    };

    mc::mir::Function roundtrip_fn {
        .name = "roundtrip_ptr",
        .locals = {
            {.name = "ptr", .type = mc::sema::PointerType(mc::sema::NamedType("i32")), .is_parameter = true, .is_mutable = false},
        },
        .return_types = {mc::sema::PointerType(mc::sema::NamedType("i32"))},
        .blocks = {roundtrip_entry},
    };

    mc::mir::Module module {
        .type_decls = {user_id},
        .functions = {promote_fn, roundtrip_fn},
    };

    mc::support::DiagnosticSink diagnostics;
    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };

    const auto result = mc::codegen_llvm::LowerModule(module, "tests/cases/hello.mc", options, diagnostics);
    Expect(result.ok, "representation-preserving conversions should lower successfully");
    Expect(!diagnostics.HasErrors(), "representation-preserving conversions should not emit backend diagnostics");

    const auto dump = mc::codegen_llvm::DumpModule(*result.module);
    Expect(dump.find("returns=[UserId [repr=i32, size=4, align=4]]") != std::string::npos,
           "distinct returns should carry source type plus backend representation");
    Expect(dump.find("convert_distinct UserId [repr=i32, size=4, align=4]") != std::string::npos,
           "distinct conversions should preserve backend representation info");
    Expect(dump.find("ptrtoint uintptr [repr=i64, size=8, align=8]") != std::string::npos,
           "pointer-to-int conversions should lower through target uintptr representation");
    Expect(dump.find("inttoptr *i32 [repr=ptr, size=8, align=8]") != std::string::npos,
           "int-to-pointer conversions should lower through target pointer representation");
}

}  // namespace

int main() {
    TestBootstrapTargetConfigIsStable();
    TestLowerModuleAcceptsStageOneSlice();
    TestLowerModuleLowersStoreBranchAndCompare();
    TestLowerModuleRejectsUnsupportedTarget();
    TestLowerModuleRejectsUnsupportedInstruction();
    TestLowerModuleLowersCheckedIntegerSemantics();
    TestLowerModuleLowersRepresentationPreservingConversions();
    return 0;
}