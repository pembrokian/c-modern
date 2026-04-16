#include <cstdlib>
#include <filesystem>
#include <fstream>
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

mc::mir::Module MakePanicModule() {
    mc::mir::Instruction code {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v0",
        .type = mc::sema::NamedType("i64"),
        .op = "77",
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {code},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kPanic,
            .values = {"%v0"},
        },
    };

    mc::mir::Function main_function {
        .name = "main",
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
    Expect(diagnostics.Render().find("only supports bootstrap 'arm64-apple-darwin' targets") != std::string::npos,
           "target diagnostic should name the frozen bootstrap target family");
}

void TestLowerModuleAcceptsFreestandingBootstrapTarget() {
    mc::support::DiagnosticSink diagnostics;
    auto target = mc::codegen_llvm::BootstrapTargetConfig();
    target.hosted = false;

    mc::codegen_llvm::LowerOptions options {
        .target = target,
    };

    const auto result = mc::codegen_llvm::LowerModule(MakeMinimalSupportedModule(), "tests/cases/hello.mc", options, diagnostics);
    Expect(result.ok, "freestanding bootstrap targets should pass backend preflight");
    Expect(!diagnostics.HasErrors(), "freestanding bootstrap targets should not emit backend diagnostics");
}

void TestLowerModuleRejectsUnsupportedInstruction() {
    auto module = MakeMinimalSupportedModule();
    module.functions[0].blocks[0].instructions.push_back({
        .kind = mc::mir::Instruction::Kind::kAtomicCompareExchange,
        .result = "%v1",
        .type = mc::sema::BoolType(),
        .operands = {"%v0", "%v0", "%v0", "%v0", "%v0"},
    });

    mc::support::DiagnosticSink diagnostics;
    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };

    const auto result = mc::codegen_llvm::LowerModule(module, "tests/cases/hello.mc", options, diagnostics);
    Expect(!result.ok, "out-of-scope MIR should fail backend preflight");
    Expect(diagnostics.HasErrors(), "out-of-scope MIR should emit backend diagnostics");
    Expect(diagnostics.Render().find("does not yet support MIR instruction 'atomic_compare_exchange'") != std::string::npos,
           "instruction diagnostic should name the unsupported MIR opcode");
}

void TestBuildObjectFileRejectsUnsupportedExecutableInstructionBeforeIrEmission() {
    auto module = MakeMinimalSupportedModule();
    module.functions[0].blocks[0].instructions.push_back({
        .kind = mc::mir::Instruction::Kind::kAtomicCompareExchange,
        .result = "%v1",
        .type = mc::sema::BoolType(),
        .operands = {"%v0", "%v0", "%v0", "%v0", "%v0"},
    });

    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_atomic_compare_exchange_preflight";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        module,
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "atomic_compare_exchange.ll",
                .object_path = artifact_dir / "atomic_compare_exchange.o",
            },
        },
        diagnostics);

    Expect(!object_result.ok, "unsupported executable MIR should fail before object emission");
    Expect(diagnostics.HasErrors(), "unsupported executable MIR should emit backend diagnostics");
    Expect(diagnostics.Render().find("does not yet support MIR instruction 'atomic_compare_exchange' before LLVM IR emission") !=
               std::string::npos,
           "object emission diagnostic should report the executable preflight failure boundary");
    Expect(!std::filesystem::exists(artifact_dir / "atomic_compare_exchange.ll"),
           "unsupported executable MIR should not write an LLVM IR artifact");
}

void TestBuildObjectFileRejectsReturnWithoutDeclaredValue() {
    auto module = MakeMinimalSupportedModule();
    module.functions[0].blocks[0].terminator.values.clear();

    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_return_signature_mismatch";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        module,
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "return_signature_mismatch.ll",
                .object_path = artifact_dir / "return_signature_mismatch.o",
            },
        },
        diagnostics);

    Expect(!object_result.ok, "non-void return terminators without values should fail executable emission");
    Expect(diagnostics.HasErrors(), "non-void return terminators without values should emit backend diagnostics");
    Expect(diagnostics.Render().find("return value count does not match function signature") != std::string::npos,
           "non-void return terminators without values should fail before emitting ret void");
    Expect(!std::filesystem::exists(artifact_dir / "return_signature_mismatch.ll"),
           "return signature mismatches should not write an LLVM IR artifact");
}

void TestBuildObjectFileRejectsMalformedIndirectCallSignatureMetadata() {
    mc::mir::Instruction callee_value {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%fn",
        .type = mc::sema::Type {
            .kind = mc::sema::Type::Kind::kProcedure,
            .name = "proc(i32) -> i32",
            .metadata = "2",
            .subtypes = {mc::sema::NamedType("i32")},
        },
        .op = "0",
    };

    mc::mir::Instruction arg {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%arg0",
        .type = mc::sema::NamedType("i32"),
        .op = "1",
    };

    mc::mir::Instruction call {
        .kind = mc::mir::Instruction::Kind::kCall,
        .result = "%v1",
        .type = mc::sema::NamedType("i32"),
        .operands = {"%fn", "%arg0"},
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {callee_value, arg, call},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v1"},
        },
    };

    mc::mir::Function main_function {
        .name = "main",
        .return_types = {mc::sema::NamedType("i32")},
        .blocks = {entry},
    };

    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_malformed_indirect_call_signature";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        mc::mir::Module {.functions = {main_function}},
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "malformed_indirect_call_signature.ll",
                .object_path = artifact_dir / "malformed_indirect_call_signature.o",
            },
        },
        diagnostics);

    Expect(!object_result.ok, "malformed indirect call signature metadata should fail executable emission");
    Expect(diagnostics.HasErrors(), "malformed indirect call signature metadata should emit backend diagnostics");
    Expect(diagnostics.Render().find("malformed procedure signature metadata") != std::string::npos,
           "indirect call lowering should reject malformed procedure metadata before indexing subtypes");
    Expect(!std::filesystem::exists(artifact_dir / "malformed_indirect_call_signature.ll"),
           "malformed indirect call signature metadata should not write an LLVM IR artifact");
}

void TestBuildObjectFileUsesDirectCalleeDeclarationForCallSignature() {
    mc::mir::Instruction callee_symbol {
        .kind = mc::mir::Instruction::Kind::kSymbolRef,
        .result = "%fn",
        .type = mc::sema::Type {
            .kind = mc::sema::Type::Kind::kProcedure,
            .name = "proc(i32) -> i32",
            .metadata = "2",
            .subtypes = {mc::sema::NamedType("i32")},
        },
        .target_kind = mc::mir::Instruction::TargetKind::kFunction,
        .target_name = "add1",
    };

    mc::mir::Instruction arg {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%arg0",
        .type = mc::sema::NamedType("i32"),
        .op = "41",
    };

    mc::mir::Instruction call {
        .kind = mc::mir::Instruction::Kind::kCall,
        .result = "%v1",
        .type = mc::sema::NamedType("i32"),
        .target_kind = mc::mir::Instruction::TargetKind::kFunction,
        .target_name = "add1",
        .operands = {"%fn", "%arg0"},
    };

    mc::mir::BasicBlock main_entry {
        .label = "entry0",
        .instructions = {callee_symbol, arg, call},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v1"},
        },
    };

    mc::mir::Instruction load_param {
        .kind = mc::mir::Instruction::Kind::kLoadLocal,
        .result = "%v0",
        .type = mc::sema::NamedType("i32"),
        .target = "x",
    };

    mc::mir::BasicBlock callee_entry {
        .label = "entry0",
        .instructions = {load_param},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v0"},
        },
    };

    mc::mir::Function add1 {
        .name = "add1",
        .locals = {
            mc::mir::Local {
                .name = "x",
                .type = mc::sema::NamedType("i32"),
                .is_parameter = true,
            },
        },
        .return_types = {mc::sema::NamedType("i32")},
        .blocks = {callee_entry},
    };

    mc::mir::Function main_function {
        .name = "main",
        .return_types = {mc::sema::NamedType("i32")},
        .blocks = {main_entry},
    };

    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_direct_call_signature";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        mc::mir::Module {.functions = {add1, main_function}},
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "direct_call_signature.ll",
                .object_path = artifact_dir / "direct_call_signature.o",
            },
            .wrap_hosted_main = false,
        },
        diagnostics);

    Expect(object_result.ok, "direct call lowering should derive its signature from the direct callee declaration");
    Expect(!diagnostics.HasErrors(), "direct call lowering should ignore malformed operand metadata when the direct callee declaration is valid");

    std::ifstream ir_stream(artifact_dir / "direct_call_signature.ll");
    Expect(ir_stream.good(), "direct call signature lowering should write LLVM IR output");
    const std::string ir((std::istreambuf_iterator<char>(ir_stream)), std::istreambuf_iterator<char>());
    Expect(ir.find("call i32 @add1(i32 41)") != std::string::npos,
           "direct call lowering should still emit the direct callee call using the declaration signature");
}

void TestBuildObjectFileEscapesSourceFilenameInModuleHeader() {
    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_source_filename_escape";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        MakeMinimalSupportedModule(),
        std::filesystem::path("tests/cases/quote\"slash\\name.mc"),
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "escaped_source_filename.ll",
                .object_path = artifact_dir / "escaped_source_filename.o",
            },
        },
        diagnostics);

    Expect(object_result.ok, "quoted source filenames should still emit LLVM IR successfully");
    Expect(!diagnostics.HasErrors(), "quoted source filenames should not emit backend diagnostics");

    std::ifstream ir_stream(artifact_dir / "escaped_source_filename.ll");
    Expect(ir_stream.good(), "quoted source filenames should write LLVM IR output");
    const std::string ir((std::istreambuf_iterator<char>(ir_stream)), std::istreambuf_iterator<char>());
    Expect(ir.find("source_filename = \"quote\\22slash\\5Cname.mc\"") != std::string::npos,
           "module header should escape quotes and backslashes in source_filename");
}

    void TestBuildObjectFileFreestandingOmitsHostedRuntimePrologue() {
        auto target = mc::codegen_llvm::BootstrapTargetConfig();
        target.hosted = false;

        const std::filesystem::path artifact_dir =
         std::filesystem::temp_directory_path() / "mc_codegen_llvm_freestanding_object";
        std::error_code remove_error;
        std::filesystem::remove_all(artifact_dir, remove_error);

        mc::support::DiagnosticSink diagnostics;
        const auto object_result = mc::codegen_llvm::BuildObjectFile(
         MakeMinimalSupportedModule(),
         "tests/cases/hello.mc",
         {
             .target = target,
             .artifacts = {
              .llvm_ir_path = artifact_dir / "freestanding.ll",
              .object_path = artifact_dir / "freestanding.o",
             },
             .wrap_hosted_main = false,
         },
         diagnostics);

        Expect(object_result.ok, "freestanding object emission should succeed for minimal supported MIR");
        Expect(!diagnostics.HasErrors(), "freestanding object emission should not emit backend diagnostics for minimal supported MIR");

        std::ifstream ir_stream(artifact_dir / "freestanding.ll");
        Expect(ir_stream.good(), "freestanding object emission should write LLVM IR output");
        const std::string ir((std::istreambuf_iterator<char>(ir_stream)), std::istreambuf_iterator<char>());
        Expect(ir.find("declare void @exit(i32)") == std::string::npos,
            "freestanding object emission should not declare hosted exit support when unused");
        Expect(ir.find("declare ptr @malloc(i64)") == std::string::npos,
            "freestanding object emission should not declare malloc when unused");
        Expect(ir.find("declare void @free(ptr)") == std::string::npos,
            "freestanding object emission should not declare free when unused");
        Expect(ir.find("define i32 @__mc_hosted_entry") == std::string::npos,
            "freestanding object emission should not synthesize the hosted entry wrapper");
        Expect(ir.find("define private void @__mc_trap()") != std::string::npos,
            "freestanding object emission should still provide the shared trap helper");
        Expect(ir.find("call void @exit(i32 134)") == std::string::npos,
            "freestanding trap helper should not call hosted exit support");
    }

void TestBuildObjectFileLowersPanicTerminator() {
    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_panic_object";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        MakePanicModule(),
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "panic.ll",
                .object_path = artifact_dir / "panic.o",
            },
            .wrap_hosted_main = false,
        },
        diagnostics);

    Expect(object_result.ok, "panic terminator object emission should succeed");
    Expect(!diagnostics.HasErrors(), "panic terminator object emission should not emit backend diagnostics");

    std::ifstream ir_stream(artifact_dir / "panic.ll");
    Expect(ir_stream.good(), "panic terminator object emission should write LLVM IR output");
    const std::string ir((std::istreambuf_iterator<char>(ir_stream)), std::istreambuf_iterator<char>());
    Expect(ir.find("define private void @__mc_panic(i64 %code)") != std::string::npos,
           "panic terminator lowering should emit the shared panic helper");
    Expect(ir.find("call void @__mc_panic(i64 77)") != std::string::npos,
           "panic terminator lowering should preserve the fault code operand");
    Expect(ir.find("call void @exit(i32 %exit.code)") != std::string::npos,
           "hosted panic lowering should route through exit with the panic code");
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

void TestEnumPayloadLayoutUsesSharedAlignedStorage() {
    mc::mir::TypeDecl choice {
        .kind = mc::mir::TypeDecl::Kind::kEnum,
        .name = "Choice",
        .variants = {
            {.name = "Small",
             .payload_fields = {{"tag", mc::sema::NamedType("i8")}, {"value", mc::sema::NamedType("f64")}}},
            {.name = "Count", .payload_fields = {{"count", mc::sema::NamedType("i32")}}},
        },
    };

    mc::mir::Instruction small_tag {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v0",
        .type = mc::sema::NamedType("i8"),
        .op = "7",
    };

    mc::mir::Instruction small_value {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v1",
        .type = mc::sema::NamedType("f64"),
        .op = "4.25",
    };

    mc::mir::Instruction make_small {
        .kind = mc::mir::Instruction::Kind::kVariantInit,
        .result = "%v2",
        .type = mc::sema::NamedType("Choice"),
        .target = "Choice.Small",
        .target_name = "Small",
        .operands = {"%v0", "%v1"},
    };

    mc::mir::Instruction match_small {
        .kind = mc::mir::Instruction::Kind::kVariantMatch,
        .result = "%v3",
        .type = mc::sema::BoolType(),
        .target = "Choice.Small",
        .target_name = "Small",
        .target_base_type = mc::sema::NamedType("Choice"),
        .operands = {"%v2"},
    };

    mc::mir::Instruction extract_small_value {
        .kind = mc::mir::Instruction::Kind::kVariantExtract,
        .result = "%v4",
        .type = mc::sema::NamedType("f64"),
        .op = "1",
        .target = "Choice.Small",
        .target_name = "Small",
        .target_base_type = mc::sema::NamedType("Choice"),
        .target_index = 1,
        .operands = {"%v2"},
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {small_tag, small_value, make_small, match_small, extract_small_value},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v4"},
        },
    };

    mc::mir::Function function {
        .name = "enum_payload_roundtrip",
        .return_types = {mc::sema::NamedType("f64")},
        .blocks = {entry},
    };

    mc::mir::Module module {
        .type_decls = {choice},
        .functions = {function},
    };

    mc::support::DiagnosticSink diagnostics;
    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };

    const auto lowered = mc::codegen_llvm::LowerModule(module, "tests/cases/hello.mc", options, diagnostics);
    Expect(lowered.ok, "mixed-alignment enum payloads should lower successfully");
    Expect(!diagnostics.HasErrors(), "mixed-alignment enum payload lowering should not emit backend diagnostics");

    const auto dump = mc::codegen_llvm::DumpModule(*lowered.module);
    Expect(dump.find("Choice [repr={i64, [16 x i8]}, size=24, align=8]") != std::string::npos,
           "enum lowering should use a shared 16-byte payload region instead of flattening all variant fields");
    Expect(dump.find("variant_extract f64 [repr=double, size=8, align=8]") != std::string::npos,
           "variant_extract should preserve extracted payload type information");

    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_enum_payload_layout";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink build_diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        module,
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "enum_payload.ll",
                .object_path = artifact_dir / "enum_payload.o",
            },
        },
        build_diagnostics);
    Expect(object_result.ok, "mixed-alignment enum payloads should emit executable LLVM IR successfully");
    Expect(!build_diagnostics.HasErrors(), "object emission for mixed-alignment enum payloads should not emit backend diagnostics");
}

void TestPayloadFreeEnumEqualityBuildsScalarTagCompare() {
    mc::mir::TypeDecl flag {
        .kind = mc::mir::TypeDecl::Kind::kEnum,
        .name = "Flag",
        .variants = {
            {.name = "Ready"},
            {.name = "Waiting"},
        },
    };

    mc::mir::Instruction ready_lhs {
        .kind = mc::mir::Instruction::Kind::kSymbolRef,
        .result = "%v0",
        .type = mc::sema::NamedType("Flag"),
        .target_kind = mc::mir::Instruction::TargetKind::kOther,
        .target_name = "Ready",
    };

    mc::mir::Instruction ready_rhs {
        .kind = mc::mir::Instruction::Kind::kSymbolRef,
        .result = "%v1",
        .type = mc::sema::NamedType("Flag"),
        .target_kind = mc::mir::Instruction::TargetKind::kOther,
        .target_name = "Ready",
    };

    mc::mir::Instruction equal {
        .kind = mc::mir::Instruction::Kind::kBinary,
        .result = "%v2",
        .type = mc::sema::BoolType(),
        .op = "==",
        .operands = {"%v0", "%v1"},
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {ready_lhs, ready_rhs, equal},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v2"},
        },
    };

    mc::mir::Function function {
        .name = "payload_free_enum_eq",
        .return_types = {mc::sema::BoolType()},
        .blocks = {entry},
    };

    mc::mir::Module module {
        .type_decls = {flag},
        .functions = {function},
    };

    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_payload_free_enum_eq";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        module,
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "payload_free_enum_eq.ll",
                .object_path = artifact_dir / "payload_free_enum_eq.o",
            },
        },
        diagnostics);
    Expect(object_result.ok, "payload-free enum equality should emit executable LLVM IR successfully");
    Expect(!diagnostics.HasErrors(), "payload-free enum equality should not emit backend diagnostics");

    std::ifstream ir_stream(artifact_dir / "payload_free_enum_eq.ll");
    Expect(ir_stream.good(), "payload-free enum equality should write LLVM IR output");
    const std::string ir((std::istreambuf_iterator<char>(ir_stream)), std::istreambuf_iterator<char>());
    Expect(ir.find("extractvalue {i64}") != std::string::npos,
           "payload-free enum equality should extract the enum tag before comparing");
    Expect(ir.find("icmp eq i64") != std::string::npos,
           "payload-free enum equality should compare scalar i64 tags");
}

void TestPayloadBearingEnumEqualityBuildsTagAndPayloadCompare() {
    mc::mir::TypeDecl option {
        .kind = mc::mir::TypeDecl::Kind::kEnum,
        .name = "Option",
        .variants = {
            {.name = "None"},
            {.name = "Some", .payload_fields = {{"value", mc::sema::NamedType("i32")}}},
        },
    };

    mc::mir::Instruction lhs_value {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v0",
        .type = mc::sema::NamedType("i32"),
        .op = "7",
    };

    mc::mir::Instruction rhs_value {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v1",
        .type = mc::sema::NamedType("i32"),
        .op = "7",
    };

    mc::mir::Instruction lhs_some {
        .kind = mc::mir::Instruction::Kind::kVariantInit,
        .result = "%v2",
        .type = mc::sema::NamedType("Option"),
        .target = "Option.Some",
        .target_name = "Some",
        .target_base_type = mc::sema::NamedType("Option"),
        .operands = {"%v0"},
    };

    mc::mir::Instruction rhs_some {
        .kind = mc::mir::Instruction::Kind::kVariantInit,
        .result = "%v3",
        .type = mc::sema::NamedType("Option"),
        .target = "Option.Some",
        .target_name = "Some",
        .target_base_type = mc::sema::NamedType("Option"),
        .operands = {"%v1"},
    };

    mc::mir::Instruction equal {
        .kind = mc::mir::Instruction::Kind::kBinary,
        .result = "%v4",
        .type = mc::sema::BoolType(),
        .op = "==",
        .operands = {"%v2", "%v3"},
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {lhs_value, rhs_value, lhs_some, rhs_some, equal},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v4"},
        },
    };

    mc::mir::Function function {
        .name = "payload_bearing_enum_eq",
        .return_types = {mc::sema::BoolType()},
        .blocks = {entry},
    };

    mc::mir::Module module {
        .type_decls = {option},
        .functions = {function},
    };

    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_payload_bearing_enum_eq";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        module,
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "payload_bearing_enum_eq.ll",
                .object_path = artifact_dir / "payload_bearing_enum_eq.o",
            },
        },
        diagnostics);
    Expect(object_result.ok, "payload-bearing enum equality should emit executable LLVM IR successfully");
    Expect(!diagnostics.HasErrors(), "payload-bearing enum equality should not emit backend diagnostics");

    std::ifstream ir_stream(artifact_dir / "payload_bearing_enum_eq.ll");
    Expect(ir_stream.good(), "payload-bearing enum equality should write LLVM IR output");
    const std::string ir((std::istreambuf_iterator<char>(ir_stream)), std::istreambuf_iterator<char>());
    Expect(ir.find("extractvalue {i64, [4 x i8]}") != std::string::npos,
           "payload-bearing enum equality should extract enum tags before comparing");
    Expect(ir.find("getelementptr inbounds {i64, [4 x i8]}, ptr") != std::string::npos,
           "payload-bearing enum equality should address the payload storage for bytewise comparison");
    Expect(ir.find("load i32") != std::string::npos,
           "payload-bearing enum equality should compare the payload bytes through integer chunk loads");
    Expect(ir.find("and i1") != std::string::npos,
           "payload-bearing enum equality should combine tag and payload equality checks");
}

void TestPayloadBearingEnumOrderingBuildsTagAndFieldLexicographicCompare() {
    mc::mir::TypeDecl option {
        .kind = mc::mir::TypeDecl::Kind::kEnum,
        .name = "Option",
        .variants = {
            {.name = "None"},
            {.name = "Some", .payload_fields = {{"value", mc::sema::NamedType("i32")}}},
        },
    };

    mc::mir::Instruction lhs_value {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v0",
        .type = mc::sema::NamedType("i32"),
        .op = "1",
    };

    mc::mir::Instruction rhs_value {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v1",
        .type = mc::sema::NamedType("i32"),
        .op = "2",
    };

    mc::mir::Instruction lhs_some {
        .kind = mc::mir::Instruction::Kind::kVariantInit,
        .result = "%v2",
        .type = mc::sema::NamedType("Option"),
        .target = "Option.Some",
        .target_name = "Some",
        .target_base_type = mc::sema::NamedType("Option"),
        .operands = {"%v0"},
    };

    mc::mir::Instruction rhs_some {
        .kind = mc::mir::Instruction::Kind::kVariantInit,
        .result = "%v3",
        .type = mc::sema::NamedType("Option"),
        .target = "Option.Some",
        .target_name = "Some",
        .target_base_type = mc::sema::NamedType("Option"),
        .operands = {"%v1"},
    };

    mc::mir::Instruction less_than {
        .kind = mc::mir::Instruction::Kind::kBinary,
        .result = "%v4",
        .type = mc::sema::BoolType(),
        .op = "<",
        .operands = {"%v2", "%v3"},
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {lhs_value, rhs_value, lhs_some, rhs_some, less_than},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v4"},
        },
    };

    mc::mir::Function function {
        .name = "payload_bearing_enum_lt",
        .return_types = {mc::sema::BoolType()},
        .blocks = {entry},
    };

    mc::mir::Module module {
        .type_decls = {option},
        .functions = {function},
    };

    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_payload_bearing_enum_lt";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        module,
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "payload_bearing_enum_lt.ll",
                .object_path = artifact_dir / "payload_bearing_enum_lt.o",
            },
        },
        diagnostics);
    Expect(object_result.ok, "payload-bearing enum ordering should emit executable LLVM IR successfully");
    Expect(!diagnostics.HasErrors(), "payload-bearing enum ordering should not emit backend diagnostics");

    std::ifstream ir_stream(artifact_dir / "payload_bearing_enum_lt.ll");
    Expect(ir_stream.good(), "payload-bearing enum ordering should write LLVM IR output");
    const std::string ir((std::istreambuf_iterator<char>(ir_stream)), std::istreambuf_iterator<char>());
    Expect(ir.find("icmp slt i64") != std::string::npos,
           "payload-bearing enum ordering should compare enum tags or signed payload fields with scalar ordering ops");
    Expect(ir.find("select i1") != std::string::npos,
           "payload-bearing enum ordering should build lexicographic field ordering with select instructions");
    Expect(ir.find("load i32") != std::string::npos,
           "payload-bearing enum ordering should load typed payload fields before ordering comparisons");
}

void TestNestedPayloadBearingEnumOrderingBuildsRecursiveCompare() {
    mc::mir::TypeDecl inner {
        .kind = mc::mir::TypeDecl::Kind::kEnum,
        .name = "Inner",
        .variants = {
            {.name = "Small", .payload_fields = {{"value", mc::sema::NamedType("i32")}}},
            {.name = "Large", .payload_fields = {{"value", mc::sema::NamedType("i32")}}},
        },
    };

    mc::mir::TypeDecl outer {
        .kind = mc::mir::TypeDecl::Kind::kEnum,
        .name = "Outer",
        .variants = {
            {.name = "Wrap", .payload_fields = {{"inner", mc::sema::NamedType("Inner")}}},
            {.name = "Empty"},
        },
    };

    mc::mir::Instruction lhs_scalar {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v0",
        .type = mc::sema::NamedType("i32"),
        .op = "1",
    };

    mc::mir::Instruction rhs_scalar {
        .kind = mc::mir::Instruction::Kind::kConst,
        .result = "%v1",
        .type = mc::sema::NamedType("i32"),
        .op = "2",
    };

    mc::mir::Instruction lhs_inner {
        .kind = mc::mir::Instruction::Kind::kVariantInit,
        .result = "%v2",
        .type = mc::sema::NamedType("Inner"),
        .target = "Inner.Small",
        .target_name = "Small",
        .target_base_type = mc::sema::NamedType("Inner"),
        .operands = {"%v0"},
    };

    mc::mir::Instruction rhs_inner {
        .kind = mc::mir::Instruction::Kind::kVariantInit,
        .result = "%v3",
        .type = mc::sema::NamedType("Inner"),
        .target = "Inner.Small",
        .target_name = "Small",
        .target_base_type = mc::sema::NamedType("Inner"),
        .operands = {"%v1"},
    };

    mc::mir::Instruction lhs_outer {
        .kind = mc::mir::Instruction::Kind::kVariantInit,
        .result = "%v4",
        .type = mc::sema::NamedType("Outer"),
        .target = "Outer.Wrap",
        .target_name = "Wrap",
        .target_base_type = mc::sema::NamedType("Outer"),
        .operands = {"%v2"},
    };

    mc::mir::Instruction rhs_outer {
        .kind = mc::mir::Instruction::Kind::kVariantInit,
        .result = "%v5",
        .type = mc::sema::NamedType("Outer"),
        .target = "Outer.Wrap",
        .target_name = "Wrap",
        .target_base_type = mc::sema::NamedType("Outer"),
        .operands = {"%v3"},
    };

    mc::mir::Instruction less_than {
        .kind = mc::mir::Instruction::Kind::kBinary,
        .result = "%v6",
        .type = mc::sema::BoolType(),
        .op = "<",
        .operands = {"%v4", "%v5"},
    };

    mc::mir::BasicBlock entry {
        .label = "entry0",
        .instructions = {lhs_scalar, rhs_scalar, lhs_inner, rhs_inner, lhs_outer, rhs_outer, less_than},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v6"},
        },
    };

    mc::mir::Function function {
        .name = "nested_payload_bearing_enum_lt",
        .return_types = {mc::sema::BoolType()},
        .blocks = {entry},
    };

    mc::mir::Module module {
        .type_decls = {inner, outer},
        .functions = {function},
    };

    const std::filesystem::path artifact_dir =
        std::filesystem::temp_directory_path() / "mc_codegen_llvm_nested_payload_bearing_enum_lt";
    std::error_code remove_error;
    std::filesystem::remove_all(artifact_dir, remove_error);

    mc::support::DiagnosticSink diagnostics;
    const auto object_result = mc::codegen_llvm::BuildObjectFile(
        module,
        "tests/cases/hello.mc",
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .artifacts = {
                .llvm_ir_path = artifact_dir / "nested_payload_bearing_enum_lt.ll",
                .object_path = artifact_dir / "nested_payload_bearing_enum_lt.o",
            },
        },
        diagnostics);
    Expect(object_result.ok, "nested payload-bearing enum ordering should emit executable LLVM IR successfully");
    Expect(!diagnostics.HasErrors(), "nested payload-bearing enum ordering should not emit backend diagnostics");

    std::ifstream ir_stream(artifact_dir / "nested_payload_bearing_enum_lt.ll");
    Expect(ir_stream.good(), "nested payload-bearing enum ordering should write LLVM IR output");
    const std::string ir((std::istreambuf_iterator<char>(ir_stream)), std::istreambuf_iterator<char>());
    Expect(ir.find("variant.0.field.0.lhs") != std::string::npos,
           "nested payload-bearing enum ordering should materialize nested field loads");
    Expect(ir.find("same.lt") != std::string::npos,
           "nested payload-bearing enum ordering should build recursive same-variant ordering state");
    Expect(ir.find("icmp slt i64") != std::string::npos,
           "nested payload-bearing enum ordering should still compare nested enum tags as scalar discriminants");
}

}  // namespace

int main() {
    TestBootstrapTargetConfigIsStable();
    TestLowerModuleAcceptsStageOneSlice();
    TestLowerModuleLowersStoreBranchAndCompare();
    TestLowerModuleRejectsUnsupportedTarget();
    TestLowerModuleAcceptsFreestandingBootstrapTarget();
    TestLowerModuleRejectsUnsupportedInstruction();
    TestBuildObjectFileRejectsUnsupportedExecutableInstructionBeforeIrEmission();
    TestBuildObjectFileRejectsReturnWithoutDeclaredValue();
    TestBuildObjectFileRejectsMalformedIndirectCallSignatureMetadata();
    TestBuildObjectFileUsesDirectCalleeDeclarationForCallSignature();
    TestBuildObjectFileEscapesSourceFilenameInModuleHeader();
    TestBuildObjectFileFreestandingOmitsHostedRuntimePrologue();
    TestBuildObjectFileLowersPanicTerminator();
    TestLowerModuleLowersCheckedIntegerSemantics();
    TestLowerModuleLowersRepresentationPreservingConversions();
    TestEnumPayloadLayoutUsesSharedAlignedStorage();
    TestPayloadFreeEnumEqualityBuildsScalarTagCompare();
    TestPayloadBearingEnumEqualityBuildsTagAndPayloadCompare();
    TestPayloadBearingEnumOrderingBuildsTagAndFieldLexicographicCompare();
    TestNestedPayloadBearingEnumOrderingBuildsRecursiveCompare();
    return 0;
}