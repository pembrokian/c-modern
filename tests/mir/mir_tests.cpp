#include <cstdlib>
#include <iostream>
#include <string>

#include "compiler/lex/lexer.h"
#include "compiler/mir/mir.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"

namespace {

void Fail(const std::string& message) {
    std::cerr << "test failure: " << message << '\n';
    std::exit(1);
}

mc::mir::LowerResult Lower(const std::string& source, mc::support::DiagnosticSink& diagnostics) {
    const auto lexed = mc::lex::Lex(source, "<mir-test>", diagnostics);
    const auto parsed = mc::parse::Parse(lexed, "<mir-test>", diagnostics);
    if (!parsed.ok) {
        Fail("source should parse before MIR lowering:\n" + diagnostics.Render());
    }
    const auto checked = mc::sema::CheckSourceFile(*parsed.source_file, "<mir-test>", diagnostics);
    if (!checked.ok) {
        Fail("source should pass semantic checking before MIR lowering:\n" + diagnostics.Render());
    }
    return mc::mir::LowerSourceFile(*parsed.source_file, *checked.module, "<mir-test>", diagnostics);
}

void TestLoweringProducesDeterministicDump() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func sum(limit: i32) i32 {\n"
        "    total: i32 = 0\n"
        "    while total < limit {\n"
        "        total = total + 1\n"
        "    }\n"
        "    return total\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("lowering should succeed:\n" + diagnostics.Render());
    }

    if (!mc::mir::ValidateModule(*lowered.module, "<mir-test>", diagnostics)) {
        Fail("lowered module should validate:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("Function name=sum returns=[i32]") == std::string::npos) {
        Fail("dump should include function signature");
    }
    if (dump.find("Block label=loop_cond") == std::string::npos) {
        Fail("dump should include loop condition block");
    }
    if (dump.find("binary %v") == std::string::npos && dump.find("binary") == std::string::npos) {
        Fail("dump should include binary instructions");
    }
    if (dump.find("terminator return values=[") == std::string::npos) {
        Fail("dump should include return terminator");
    }
}

void TestIfElseLoweringBranchesToMerge() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func main(flag: bool) i32 {\n"
        "    result: i32 = 1\n"
        "    if flag {\n"
        "        result = result + 1\n"
        "    } else {\n"
        "        result = result + 2\n"
        "    }\n"
        "    return result\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("if/else lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("terminator cond value=") == std::string::npos) {
        Fail("if lowering should emit conditional branch terminator");
    }
    if (dump.find("Block label=if_merge") == std::string::npos) {
        Fail("if lowering should create merge block");
    }
}

void TestVariantSwitchLoweringSucceeds() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "enum Result {\n"
        "    Ok(value: i32),\n"
        "    Err,\n"
        "}\n"
        "\n"
        "func dispatch(value: Result) i32 {\n"
        "    switch value {\n"
        "    case Result.Ok(v):\n"
        "        return v\n"
        "    default:\n"
        "        return 2\n"
        "    }\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("variant switch lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("variant_match") == std::string::npos) {
        Fail("variant switch lowering should emit variant_match instructions");
    }
    if (dump.find("variant_extract") == std::string::npos) {
        Fail("variant switch lowering should emit variant_extract instructions for payload bindings");
    }
    if (dump.find("Local name=v type=i32 readonly") == std::string::npos) {
        Fail("variant switch lowering should preserve payload binding types");
    }
    if (dump.find("variant_extract %v") == std::string::npos || dump.find(":i32 op=0 target=Result.Ok") == std::string::npos) {
        Fail("variant switch lowering should emit typed variant_extract instructions");
    }
    if (dump.find("unknown") != std::string::npos) {
        Fail("supported variant switch lowering should not emit unknown MIR types");
    }
}

void TestForEachAndDeferLoweringSucceed() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func visit(values: Slice<i32>) i32 {\n"
        "    total: i32 = 0\n"
        "    defer cleanup()\n"
        "    for index, value in values {\n"
        "        total = total + value\n"
        "    }\n"
        "    return total\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("foreach and defer lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("Block label=foreach_cond") == std::string::npos) {
        Fail("foreach lowering should create foreach condition block");
    }
    if (dump.find("target=cleanup") == std::string::npos) {
        Fail("defer lowering should emit deferred cleanup call");
    }
    if (dump.find("Local name=value type=i32") == std::string::npos) {
        Fail("foreach lowering should preserve indexed element types");
    }
    if (dump.find("unknown") != std::string::npos) {
        Fail("supported foreach lowering should not emit unknown MIR types");
    }
}

void TestForRangeUsesSemaRangeElementType() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func climb(limit: i32) i32 {\n"
        "    for idx in 0..limit {\n"
        "        return idx\n"
        "    }\n"
        "    return 0\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("for-range lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("Local name=idx type=i32") == std::string::npos) {
        Fail("for-range lowering should use the sema-known range element type for loop locals");
    }
    if (dump.find("unknown") != std::string::npos) {
        Fail("supported range lowering should not emit unknown MIR types");
    }
}

void TestKnownSymbolRefsAreTyped() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "const LIMIT: i32 = 3\n"
        "\n"
        "func inc(value: i32) i32 {\n"
        "    return value + LIMIT\n"
        "}\n"
        "\n"
        "func use() i32 {\n"
        "    defer inc(1)\n"
        "    return LIMIT\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("known symbol reference lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("symbol_ref %v") == std::string::npos || dump.find(":i32 target=LIMIT") == std::string::npos) {
        Fail("global symbol references should preserve their value type");
    }
    if (dump.find("proc(i32)->i32 target=inc") == std::string::npos) {
        Fail("function symbol references should preserve procedure types");
    }
    if (dump.find("Local name=$defer_callee0 type=proc(i32)->i32 readonly") == std::string::npos) {
        Fail("deferred callees should preserve procedure types in hidden locals");
    }
    if (dump.find("unknown") != std::string::npos) {
        Fail("supported symbol reference lowering should not emit unknown MIR types");
    }
}

void TestExplicitConversionLowersToConvert() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "distinct UserId = i32\n"
        "\n"
        "func promote(raw: i32) UserId {\n"
        "    return (UserId)(raw)\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("explicit conversion lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("convert_distinct %v") == std::string::npos || dump.find(":UserId target=UserId") == std::string::npos) {
        Fail("distinct explicit conversions should lower to convert_distinct instructions");
    }
}

void TestNumericConversionLowersToConvertNumeric() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func widen(raw: i32) i64 {\n"
        "    return (i64)(raw)\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("numeric conversion lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("convert_numeric %v") == std::string::npos || dump.find(":i64 target=i64") == std::string::npos) {
        Fail("numeric explicit conversions should lower to convert_numeric instructions");
    }
}

void TestArrayToSliceConversionLowersExplicitly() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func view(values: [4]i32) usize {\n"
        "    slice: Slice<i32> = (Slice<i32>)(values)\n"
        "    return slice.len\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("array-to-slice conversion lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("array_to_slice %v") == std::string::npos || dump.find(":Slice<i32> target=Slice<i32>") == std::string::npos) {
        Fail("array-to-slice explicit conversions should lower to array_to_slice instructions");
    }
}

void TestBufferToSliceConversionLowersExplicitly() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func view(values: Buffer<i32>) usize {\n"
        "    slice: Slice<i32> = (Slice<i32>)(values)\n"
        "    return slice.len\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("buffer-to-slice conversion lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("buffer_to_slice %v") == std::string::npos || dump.find(":Slice<i32> target=Slice<i32>") == std::string::npos) {
        Fail("buffer-to-slice explicit conversions should lower to buffer_to_slice instructions");
    }
}

void TestIndexAndSliceLoweringEmitBoundsChecks() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func at(values: [4]i32, idx: usize) i32 {\n"
        "    return values[idx]\n"
        "}\n"
        "\n"
        "func window(values: [4]i32, start: usize, end: usize) Slice<i32> {\n"
        "    return values[start:end]\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("index/slice lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("bounds_check op=index") == std::string::npos) {
        Fail("index lowering should emit explicit bounds_check instructions");
    }
    if (dump.find("bounds_check op=slice") == std::string::npos) {
        Fail("slice lowering should emit explicit bounds_check instructions");
    }
}

void TestPointerToIntConversionLowersExplicitly() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "type I32Ptr = *i32\n"
        "\n"
        "func expose(ptr: *i32) uintptr {\n"
        "    return (uintptr)(ptr)\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("pointer-to-int conversion lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("pointer_to_int %v") == std::string::npos || dump.find(":uintptr target=uintptr") == std::string::npos) {
        Fail("pointer-to-int explicit conversions should lower to pointer_to_int instructions");
    }
}

void TestIntToPointerConversionLowersExplicitly() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func expose(ptr: *i32) uintptr {\n"
        "    return (uintptr)(ptr)\n"
        "}\n"
        "\n"
        "func roundtrip(value: i32) i32 {\n"
        "    raw: uintptr = expose(&value)\n"
        "    ptr: *i32 = (*i32)(raw)\n"
        "    return *ptr\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("int-to-pointer conversion lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("int_to_pointer %v") == std::string::npos || dump.find(":*i32 target=*i32") == std::string::npos) {
        Fail("int-to-pointer explicit conversions should lower to int_to_pointer instructions");
    }
}

void TestScopedDeferRunsBeforeFollowingStatement() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func nested(flag: bool) i32 {\n"
        "    total: i32 = 0\n"
        "    {\n"
        "        defer inner()\n"
        "        if flag {\n"
        "            total = 10\n"
        "        }\n"
        "    }\n"
        "    total = total + 1\n"
        "    return total\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("scoped defer lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    const auto inner_pos = dump.find("target=inner");
    const auto increment_pos = dump.rfind("binary");
    if (inner_pos == std::string::npos) {
        Fail("scoped defer lowering should emit inner cleanup call");
    }
    if (increment_pos == std::string::npos || inner_pos > increment_pos) {
        Fail("scoped defer should run before statements after the nested block");
    }
}

void TestLoopExitUnwindsScopedDefers() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func spin(limit: i32) i32 {\n"
        "    total: i32 = 0\n"
        "    while total < limit {\n"
        "        {\n"
        "            defer tick()\n"
        "            total = total + 1\n"
        "            continue\n"
        "        }\n"
        "    }\n"
        "    return total\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("loop defer lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    const auto tick_pos = dump.find("target=tick");
    const auto exit_pos = dump.find("Block label=loop_exit");
    if (tick_pos == std::string::npos) {
        Fail("continue path should emit deferred tick call");
    }
    if (exit_pos == std::string::npos || tick_pos > exit_pos) {
        Fail("continue path should unwind scoped defers before leaving the loop body");
    }
}

void TestSwitchArmUnwindsScopedDefers() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func choose(flag: i32) i32 {\n"
        "    switch flag {\n"
        "    case 0:\n"
        "        defer hit()\n"
        "        return 10\n"
        "    default:\n"
        "        return 20\n"
        "    }\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("switch defer lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    const auto case_pos = dump.find("Block label=switch_case3");
    const auto hit_pos = dump.find("target=hit", case_pos);
    const auto return_pos = dump.find("terminator return values=", case_pos);
    if (case_pos == std::string::npos || hit_pos == std::string::npos || return_pos == std::string::npos || hit_pos > return_pos) {
        Fail("switch arm defer should run before the arm returns");
    }
}

void TestLoopIterationDefersRunBeforeStep() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func iterate() i32 {\n"
        "    for idx in 0..2 {\n"
        "        defer tick()\n"
        "    }\n"
        "    return 0\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("loop iteration defer lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    const auto body_pos = dump.find("Block label=range_body2");
    const auto tick_pos = dump.find("target=tick", body_pos);
    const auto step_pos = dump.find("terminator branch target=range_step3", body_pos);
    if (body_pos == std::string::npos || tick_pos == std::string::npos || step_pos == std::string::npos || tick_pos > step_pos) {
        Fail("loop iteration defer should run before the loop step");
    }
}

void TestDeferArgumentsAreEvaluatedImmediately() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func demo() i32 {\n"
        "    value: i32 = 1\n"
        "    defer hit(value)\n"
        "    value = 2\n"
        "    return 0\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("defer argument snapshot lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    const auto snapshot_pos = dump.find("store_local target=$defer_arg");
    const auto mutation_pos = dump.find("store_local target=value", snapshot_pos == std::string::npos ? 0 : snapshot_pos + 1);
    const auto replay_load_pos = dump.find("load_local ", mutation_pos == std::string::npos ? 0 : mutation_pos);
    const auto replay_arg_pos = dump.find("target=$defer_arg", mutation_pos == std::string::npos ? 0 : mutation_pos);
    if (snapshot_pos == std::string::npos) {
        Fail("defer lowering should snapshot call arguments into hidden locals");
    }
    if (replay_load_pos == std::string::npos || replay_arg_pos == std::string::npos || replay_arg_pos < replay_load_pos) {
        Fail("deferred call should reload the snapshotted argument at scope exit");
    }
    if (mutation_pos == std::string::npos || !(snapshot_pos < mutation_pos && mutation_pos < replay_arg_pos)) {
        Fail("defer argument snapshot should happen before later mutations and be reused on unwind");
    }
    if (dump.find("Local name=$defer_callee0 type=proc(i32)->void readonly") == std::string::npos) {
        Fail("defer lowering should infer unresolved sink-call procedure types from the call site");
    }
}

void TestStatementCallUsesVoidFallbackForUnresolvedCallee() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func demo() i32 {\n"
        "    hit(1)\n"
        "    return 0\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("statement call lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("symbol_ref %v") == std::string::npos || dump.find("->void target=hit") == std::string::npos) {
        Fail("unresolved sink-call symbol references should carry a void-return procedure shape");
    }
    if (dump.find("call target=hit operands=[") == std::string::npos) {
        Fail("statement calls should lower to side-effect calls without a materialized result value");
    }
}

void TestValidatorRejectsBadBranchTarget() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kBranch,
            .values = {},
            .true_target = "missing",
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject missing block targets");
    }
    if (diagnostics.Render().find("branch targets missing MIR block") == std::string::npos) {
        Fail("validator should explain missing branch target");
    }
}

void TestValidatorRejectsNonBoolCondition() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_cond";
    function.return_types.push_back(mc::sema::NamedType("i32"));
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::IntLiteralType(),
                .op = "1",
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kCondBranch,
            .values = {"%v0"},
            .true_target = "then",
            .false_target = "else",
        },
    });
    function.blocks.push_back({
        .label = "then",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::IntLiteralType(),
                .op = "1",
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v1"},
        },
    });
    function.blocks.push_back({
        .label = "else",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v2",
                .type = mc::sema::IntLiteralType(),
                .op = "0",
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v2"},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject non-bool conditional branches");
    }
    if (diagnostics.Render().find("conditional branch condition must have bool type") == std::string::npos) {
        Fail("validator should explain non-bool branch conditions");
    }
}

void TestValidatorRejectsCallArgumentTypeMismatch() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_call";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::ProcedureType({mc::sema::NamedType("i32")}, {}),
                .target = "callee",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::BoolType(),
                .op = "true",
            },
            {
                .kind = mc::mir::Instruction::Kind::kCall,
                .type = mc::sema::VoidType(),
                .target = "callee",
                .operands = {"%v0", "%v1"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject call argument type mismatches");
    }
    if (diagnostics.Render().find("call argument type mismatch") == std::string::npos) {
        Fail("validator should explain bad call argument types");
    }
}

void TestValidatorRejectsAggregateFieldTypeMismatch() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    module.type_decls.push_back({
        .kind = mc::mir::TypeDecl::Kind::kStruct,
        .name = "Pair",
        .fields = {{"left", mc::sema::NamedType("i32")}},
    });

    mc::mir::Function function;
    function.name = "broken_aggregate";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::BoolType(),
                .op = "true",
            },
            {
                .kind = mc::mir::Instruction::Kind::kAggregateInit,
                .result = "%v1",
                .type = mc::sema::NamedType("Pair"),
                .target = "Pair",
                .operands = {"%v0"},
                .field_names = {"left"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject aggregate field type mismatches");
    }
    if (diagnostics.Render().find("aggregate_init field type mismatch") == std::string::npos) {
        Fail("validator should explain bad aggregate field types");
    }
}

void TestValidatorRejectsGenericConvertThatShouldBeExplicitFamily() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_convert";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConvert,
                .result = "%v1",
                .type = mc::sema::NamedType("i64"),
                .target = "i64",
                .operands = {"%v0"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject generic convert when a dedicated conversion opcode exists");
    }
    if (diagnostics.Render().find("convert must use a dedicated conversion opcode") == std::string::npos) {
        Fail("validator should explain misencoded generic conversions");
    }
}

void TestValidatorRejectsAggregateDuplicateNamedField() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    module.type_decls.push_back({
        .kind = mc::mir::TypeDecl::Kind::kStruct,
        .name = "Pair",
        .fields = {{"left", mc::sema::NamedType("i32")}, {"right", mc::sema::NamedType("i32")}},
    });

    mc::mir::Function function;
    function.name = "broken_duplicate_field";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::NamedType("i32"),
                .op = "2",
            },
            {
                .kind = mc::mir::Instruction::Kind::kAggregateInit,
                .result = "%v2",
                .type = mc::sema::NamedType("Pair"),
                .target = "Pair",
                .operands = {"%v0", "%v1"},
                .field_names = {"left", "left"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject aggregate initializers with duplicate named fields");
    }
    if (diagnostics.Render().find("aggregate_init has duplicate named field") == std::string::npos) {
        Fail("validator should explain duplicate aggregate fields");
    }
}

void TestValidatorRejectsVariantExtractPayloadMismatch() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    module.type_decls.push_back({
        .kind = mc::mir::TypeDecl::Kind::kEnum,
        .name = "Result",
        .variants = {{.name = "Ok", .payload_fields = {{"value", mc::sema::NamedType("i32")}}}, {.name = "Err"}},
    });

    mc::mir::Function function;
    function.name = "broken_variant";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("Result"),
                .op = "selector",
            },
            {
                .kind = mc::mir::Instruction::Kind::kVariantExtract,
                .result = "%v1",
                .type = mc::sema::BoolType(),
                .op = "0",
                .target = "Result.Ok",
                .operands = {"%v0"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject variant extract type mismatches");
    }
    if (diagnostics.Render().find("variant_extract result type mismatch") == std::string::npos) {
        Fail("validator should explain bad variant payload typing");
    }
}

void TestValidatorRejectsBadSymbolRefType() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;

    mc::mir::Function callee;
    callee.name = "callee";
    callee.locals.push_back({
        .name = "value",
        .type = mc::sema::NamedType("i32"),
        .is_parameter = true,
        .is_mutable = false,
    });
    callee.return_types.push_back(mc::sema::NamedType("i32"));
    callee.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kLoadLocal,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .target = "value",
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v0"},
        },
    });
    module.functions.push_back(std::move(callee));

    mc::mir::Function broken;
    broken.name = "broken_symbol";
    broken.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v1",
                .type = mc::sema::BoolType(),
                .target = "callee",
                .target_kind = mc::mir::Instruction::TargetKind::kFunction,
                .target_name = "callee",
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(broken));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject bad symbol reference types");
    }
    if (diagnostics.Render().find("symbol_ref function type mismatch") == std::string::npos) {
        Fail("validator should explain bad function symbol reference types");
    }
}

void TestValidatorRejectsStoreTargetFieldMismatch() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    module.type_decls.push_back({
        .kind = mc::mir::TypeDecl::Kind::kStruct,
        .name = "Pair",
        .fields = {{"left", mc::sema::NamedType("i32")}},
    });

    mc::mir::Function function;
    function.name = "broken_store_target";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::BoolType(),
                .op = "true",
            },
            {
                .kind = mc::mir::Instruction::Kind::kStoreTarget,
                .type = mc::sema::BoolType(),
                .target = "pair.left",
                .target_kind = mc::mir::Instruction::TargetKind::kField,
                .target_name = "left",
                .target_base_type = mc::sema::NamedType("Pair"),
                .operands = {"%v0"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject store_target field mismatches");
    }
    if (diagnostics.Render().find("store_target field type mismatch") == std::string::npos) {
        Fail("validator should explain bad structured store targets");
    }
}

void TestValidatorRejectsBadAddressOfResultType() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_addr";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kUnary,
                .result = "%v1",
                .type = mc::sema::NamedType("i32"),
                .op = "&",
                .operands = {"%v0"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject address-of results that are not pointer-typed");
    }
    if (diagnostics.Render().find("address-of unary must produce pointer type") == std::string::npos) {
        Fail("validator should explain invalid address-of result typing");
    }
}

void TestValidatorRejectsBadDerefOperandType() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_deref";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kUnary,
                .result = "%v1",
                .type = mc::sema::NamedType("i32"),
                .op = "*",
                .operands = {"%v0"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject pointer dereference on non-pointer operands");
    }
    if (diagnostics.Render().find("pointer dereference requires pointer operand") == std::string::npos) {
        Fail("validator should explain invalid dereference operands");
    }
}

void TestValidatorRejectsBadComparisonOperands() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_compare";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::BoolType(),
                .op = "true",
            },
            {
                .kind = mc::mir::Instruction::Kind::kBinary,
                .result = "%v2",
                .type = mc::sema::BoolType(),
                .op = "==",
                .operands = {"%v0", "%v1"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject incompatible comparison operands");
    }
    if (diagnostics.Render().find("comparison requires compatible operand types") == std::string::npos) {
        Fail("validator should explain incompatible comparison operands");
    }
}

void TestValidatorRejectsBadFieldBaseType() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_field";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kField,
                .result = "%v1",
                .type = mc::sema::NamedType("i32"),
                .target = "value",
                .operands = {"%v0"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject field access on non-aggregate bases");
    }
    if (diagnostics.Render().find("field requires named aggregate base") == std::string::npos) {
        Fail("validator should explain invalid field bases");
    }
}

void TestValidatorRejectsBadIndexBaseType() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_index";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::NamedType("usize"),
                .op = "0",
            },
            {
                .kind = mc::mir::Instruction::Kind::kIndex,
                .result = "%v2",
                .type = mc::sema::NamedType("i32"),
                .operands = {"%v0", "%v1"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject indexing on non-indexable bases");
    }
    if (diagnostics.Render().find("index requires array, slice, or buffer base") == std::string::npos) {
        Fail("validator should explain invalid index bases");
    }
}

void TestValidatorRejectsBadSliceBaseType() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_slice";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kSlice,
                .result = "%v1",
                .type = mc::sema::NamedType("Slice"),
                .operands = {"%v0"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject slicing on non-sliceable bases");
    }
    if (diagnostics.Render().find("slice requires array, slice, buffer, or string base") == std::string::npos) {
        Fail("validator should explain invalid slice bases");
    }
}

void TestValidatorRejectsBadSliceBoundType() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_slice_bound";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::ArrayType(mc::sema::NamedType("i32"), "4"),
                .op = "arr",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::BoolType(),
                .op = "true",
            },
            {
                .kind = mc::mir::Instruction::Kind::kSlice,
                .result = "%v2",
                .type = mc::sema::NamedType("Slice"),
                .operands = {"%v0", "%v1"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject non-usize slice bounds");
    }
    if (diagnostics.Render().find("slice bounds must be usize-compatible") == std::string::npos) {
        Fail("validator should explain invalid slice bounds");
    }
}

void TestValidatorRejectsBadBoundsCheckOperands() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_bounds_check";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::BoolType(),
                .op = "true",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::NamedType("usize"),
                .op = "4",
            },
            {
                .kind = mc::mir::Instruction::Kind::kBoundsCheck,
                .op = "index",
                .operands = {"%v0", "%v1"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject non-usize bounds_check operands");
    }
    if (diagnostics.Render().find("bounds_check operands must be usize-compatible") == std::string::npos) {
        Fail("validator should explain invalid bounds_check operands");
    }
}

}  // namespace

int main() {
    TestLoweringProducesDeterministicDump();
    TestIfElseLoweringBranchesToMerge();
    TestVariantSwitchLoweringSucceeds();
    TestForEachAndDeferLoweringSucceed();
    TestForRangeUsesSemaRangeElementType();
    TestKnownSymbolRefsAreTyped();
    TestExplicitConversionLowersToConvert();
    TestNumericConversionLowersToConvertNumeric();
    TestArrayToSliceConversionLowersExplicitly();
    TestBufferToSliceConversionLowersExplicitly();
    TestIndexAndSliceLoweringEmitBoundsChecks();
    TestPointerToIntConversionLowersExplicitly();
    TestIntToPointerConversionLowersExplicitly();
    TestScopedDeferRunsBeforeFollowingStatement();
    TestLoopExitUnwindsScopedDefers();
    TestSwitchArmUnwindsScopedDefers();
    TestLoopIterationDefersRunBeforeStep();
    TestDeferArgumentsAreEvaluatedImmediately();
    TestStatementCallUsesVoidFallbackForUnresolvedCallee();
    TestValidatorRejectsBadBranchTarget();
    TestValidatorRejectsNonBoolCondition();
    TestValidatorRejectsCallArgumentTypeMismatch();
    TestValidatorRejectsAggregateFieldTypeMismatch();
    TestValidatorRejectsGenericConvertThatShouldBeExplicitFamily();
    TestValidatorRejectsAggregateDuplicateNamedField();
    TestValidatorRejectsVariantExtractPayloadMismatch();
    TestValidatorRejectsBadSymbolRefType();
    TestValidatorRejectsStoreTargetFieldMismatch();
    TestValidatorRejectsBadAddressOfResultType();
    TestValidatorRejectsBadDerefOperandType();
    TestValidatorRejectsBadComparisonOperands();
    TestValidatorRejectsBadFieldBaseType();
    TestValidatorRejectsBadIndexBaseType();
    TestValidatorRejectsBadSliceBaseType();
    TestValidatorRejectsBadSliceBoundType();
    TestValidatorRejectsBadBoundsCheckOperands();
    return 0;
}