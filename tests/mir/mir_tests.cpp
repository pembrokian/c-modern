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

void TestUnsupportedConstructReportsDirectDiagnostic() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func dispatch(value: i32) i32 {\n"
        "    switch value {\n"
        "    case Result.Ok(v):\n"
        "        return 1\n"
        "    default:\n"
        "        return 2\n"
        "    }\n"
        "}\n",
        diagnostics);

    if (lowered.ok) {
        Fail("variant switch lowering should fail until semantic lowering exists");
    }
    if (diagnostics.Render().find("only lowers expression switch cases") == std::string::npos) {
        Fail("unsupported switch pattern should report a direct diagnostic");
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

}  // namespace

int main() {
    TestLoweringProducesDeterministicDump();
    TestIfElseLoweringBranchesToMerge();
    TestUnsupportedConstructReportsDirectDiagnostic();
    TestForEachAndDeferLoweringSucceed();
    TestScopedDeferRunsBeforeFollowingStatement();
    TestLoopExitUnwindsScopedDefers();
    TestSwitchArmUnwindsScopedDefers();
    TestLoopIterationDefersRunBeforeStep();
    TestDeferArgumentsAreEvaluatedImmediately();
    TestValidatorRejectsBadBranchTarget();
    return 0;
}