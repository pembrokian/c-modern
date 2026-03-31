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
    Expect(result.ok, "supported Stage 1 MIR should pass backend preflight");
    Expect(!diagnostics.HasErrors(), "supported Stage 1 MIR should not emit backend diagnostics");
    Expect(result.module != nullptr, "successful lowering should materialize a backend module scaffold");
    Expect(result.module->functions.size() == 1 && result.module->functions[0] == "main",
           "backend module scaffold should preserve function names");
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
        .kind = mc::mir::Instruction::Kind::kBoundsCheck,
        .operands = {"%v0", "%v0"},
    });

    mc::support::DiagnosticSink diagnostics;
    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };

    const auto result = mc::codegen_llvm::LowerModule(module, "tests/cases/hello.mc", options, diagnostics);
    Expect(!result.ok, "out-of-scope MIR should fail backend preflight");
    Expect(diagnostics.HasErrors(), "out-of-scope MIR should emit backend diagnostics");
    Expect(diagnostics.Render().find("does not support MIR instruction 'bounds_check'") != std::string::npos,
           "instruction diagnostic should name the unsupported MIR opcode");
}

}  // namespace

int main() {
    TestBootstrapTargetConfigIsStable();
    TestLowerModuleAcceptsStageOneSlice();
    TestLowerModuleRejectsUnsupportedTarget();
    TestLowerModuleRejectsUnsupportedInstruction();
    return 0;
}