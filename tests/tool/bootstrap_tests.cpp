#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "compiler/ast/ast.h"
#include "compiler/lex/lexer.h"
#include "compiler/parse/parser.h"
#include "compiler/support/diagnostics.h"
#include "compiler/support/dump_paths.h"
#include "compiler/support/source_manager.h"

namespace {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "test failure: " << message << '\n';
        std::exit(1);
    }
}

std::filesystem::path MakeTempFile() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("c_modern_phase2_" + std::to_string(stamp) + ".mc");
    std::ofstream output(path);
    output << "func main() {}\n";
    return path;
}

mc::parse::ParseResult ParseText(const std::string& source, mc::support::DiagnosticSink& diagnostics) {
    const auto lexed = mc::lex::Lex(source, "<test>", diagnostics);
    return mc::parse::Parse(lexed, "<test>", diagnostics);
}

void TestDiagnosticFormatting() {
    const mc::support::Diagnostic diagnostic {
        .file_path = "tests/cases/hello.mc",
        .span = {{3, 7}, {3, 12}},
        .severity = mc::support::DiagnosticSeverity::kError,
        .message = "expected declaration",
    };

    Expect(mc::support::FormatDiagnostic(diagnostic) ==
               "tests/cases/hello.mc:3:7: error: expected declaration",
           "diagnostic rendering should be stable");
}

void TestSourceManagerLoad() {
    const auto temp_file = MakeTempFile();
    mc::support::DiagnosticSink diagnostics;
    mc::support::SourceManager source_manager;

    const auto file_id = source_manager.LoadFile(temp_file, diagnostics);
    Expect(file_id.has_value(), "source manager should load existing files");
    Expect(!diagnostics.HasErrors(), "loading an existing file should not report errors");

    const auto* file = source_manager.GetFile(*file_id);
    Expect(file != nullptr, "loaded file should be retrievable");
    Expect(file->contents.find("func main") != std::string::npos, "loaded contents should match the file");

    std::filesystem::remove(temp_file);
}

void TestDumpPaths() {
    const auto targets = mc::support::ComputeDumpTargets("tests/cases/hello.mc", "build/debug");
    const auto build_targets = mc::support::ComputeBuildArtifactTargets("tests/cases/hello.mc", "build/debug");
    Expect(targets.ast.generic_string() == "build/debug/dumps/ast/tests_cases_hello.mc.ast.txt",
           "ast dump path should be deterministic");
    Expect(targets.mir.generic_string() == "build/debug/dumps/mir/tests_cases_hello.mc.mir.txt",
           "mir dump path should be deterministic");
    Expect(targets.backend.generic_string() == "build/debug/dumps/backend/tests_cases_hello.mc.backend.txt",
        "backend dump path should be deterministic");
    Expect(targets.mci.generic_string() == "build/debug/mci/tests_cases_hello.mc.mci",
           "mci path should be deterministic");
    Expect(build_targets.llvm_ir.generic_string() == "build/debug/dumps/backend/tests_cases_hello.mc.ll",
        "llvm ir path should be deterministic");
    Expect(build_targets.object.generic_string() == "build/debug/obj/tests_cases_hello.mc.o",
        "object path should be deterministic");
    Expect(build_targets.executable.generic_string() == "build/debug/bin/tests_cases_hello.mc",
        "executable path should be deterministic");
}

void TestLexerTracksKeywordsAndSeparators() {
    mc::support::DiagnosticSink diagnostics;
    const auto lexed = mc::lex::Lex("export { main }\nimport io\nfunc main() {}\n", "<test>", diagnostics);
    Expect(lexed.ok, "lexer should accept a basic module");
    Expect(!diagnostics.HasErrors(), "lexer should not emit diagnostics for valid input");
    Expect(lexed.tokens.size() >= 10, "lexer should emit a real token stream");
    Expect(lexed.tokens[0].kind == mc::lex::TokenKind::kExport, "lexer should recognize export keyword");
    Expect(lexed.tokens[4].kind == mc::lex::TokenKind::kNewline, "lexer should preserve newline separators");
}

void TestParserBuildsDeterministicAstDump() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "export { main }\n"
        "import io\n"
        "@trace\n"
        "func main(args: Slice<cstr>) i32 {\n"
        "    cfg = Config{ path: \"out\" }\n"
        "    if args.len == 0 {\n"
        "        return 1\n"
        "    }\n"
        "    return io.write(cfg.path)\n"
        "}\n",
        diagnostics);

    Expect(parsed.ok, "parser should accept a representative Phase 2 module");
    Expect(!diagnostics.HasErrors(), "valid module should not emit diagnostics");

    const auto dump = mc::ast::DumpSourceFile(*parsed.source_file);
    Expect(dump.find("SourceFile") != std::string::npos, "dump should start with SourceFile");
    Expect(dump.find("ExportBlock names=[main]") != std::string::npos, "dump should include export block");
    Expect(dump.find("ImportDecl moduleName=io") != std::string::npos, "dump should include imports");
    Expect(dump.find("FuncDecl name=main") != std::string::npos, "dump should include function declaration");
    Expect(dump.find("AggregateInitExpr") != std::string::npos, "dump should include aggregate initializer expression");
    Expect(dump.find("FieldInit name=path") != std::string::npos, "dump should preserve named aggregate fields");
    Expect(dump.find("IfStmt") != std::string::npos, "dump should include if statement");
}

void TestParserHandlesTypesAndLoopForms() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "struct Buffer<T> {\n"
        "    ptr: *T\n"
        "    len: usize\n"
        "}\n"
        "\n"
        "func walk(values: Slice<i32>) i32 {\n"
        "    total: i32\n"
        "    for index, value in values {\n"
        "        total = total + value\n"
        "    }\n"
        "    for idx in 0..10 {\n"
        "        total = total + idx\n"
        "    }\n"
        "    while total < 100 {\n"
        "        total = total + 1\n"
        "    }\n"
        "    return total\n"
        "}\n",
        diagnostics);

    Expect(parsed.ok, "parser should accept generic types and loop forms");
    const auto dump = mc::ast::DumpSourceFile(*parsed.source_file);
    Expect(dump.find("StructDecl name=Buffer") != std::string::npos, "dump should include struct declaration");
    Expect(dump.find("PointerType") != std::string::npos, "dump should include pointer types");
    Expect(dump.find("ForEachIndexStmt") != std::string::npos, "dump should include index foreach loop");
    Expect(dump.find("ForRangeStmt") != std::string::npos, "dump should include range loop");
    Expect(dump.find("WhileStmt") != std::string::npos, "dump should include while loop");
}

void TestParserReportsMalformedInput() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "func broken(a: i32 {\n"
        "    return a\n"
        "}\n",
        diagnostics);

    Expect(!parsed.ok, "parser should reject malformed input");
    Expect(diagnostics.HasErrors(), "parser should report malformed input");
    Expect(diagnostics.Render().find("expected ')' after parameter list") != std::string::npos,
           "parser should report a direct syntax diagnostic");
}

void TestParserRejectsArrayLiteralCallArgumentWithoutHanging() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "func view(values: [4]i32) i32 {\n"
        "    return 0\n"
        "}\n"
        "\n"
        "func main() i32 {\n"
        "    return view([4]i32{ 1, 2, 3, 4 })\n"
        "}\n",
        diagnostics);

    Expect(!parsed.ok, "parser should reject unsupported array literal call arguments rather than hanging");
    Expect(diagnostics.HasErrors(), "parser should report diagnostics for unsupported array literal call arguments");
    Expect(diagnostics.Render().find("expected expression") != std::string::npos,
           "parser should report the unexpected array literal token directly");
}

void TestParserHandlesMultiTargetAssignment() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "func swap() {\n"
        "    left = 1\n"
        "    pair = Pair{ right: 2 }\n"
        "    left, pair.right = pair.right, left\n"
        "}\n",
        diagnostics);

    Expect(parsed.ok, "parser should accept multi-target assignment");
    const auto dump = mc::ast::DumpSourceFile(*parsed.source_file);
    Expect(dump.find("AssignStmt") != std::string::npos, "dump should include assignment statement");
    Expect(dump.find("targets:") != std::string::npos, "assignment dump should distinguish targets");
    Expect(dump.find("values:") != std::string::npos, "assignment dump should distinguish values");
}

void TestParserPreservesBindingAssignmentAmbiguity() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "func demo() {\n"
        "    left = right\n"
        "    first, second = second, first\n"
        "}\n",
        diagnostics);

    Expect(parsed.ok, "parser should accept bare identifier-list equals forms");
    const auto dump = mc::ast::DumpSourceFile(*parsed.source_file);
    Expect(dump.find("BindingOrAssignStmt") != std::string::npos,
           "dump should preserve binding-versus-assignment ambiguity explicitly");
    Expect(dump.find("NamePattern names=[first, second]") != std::string::npos,
           "ambiguous node should preserve the identifier list");
}

}  // namespace

int main() {
    TestDiagnosticFormatting();
    TestSourceManagerLoad();
    TestDumpPaths();
    TestLexerTracksKeywordsAndSeparators();
    TestParserBuildsDeterministicAstDump();
    TestParserHandlesTypesAndLoopForms();
    TestParserReportsMalformedInput();
    TestParserRejectsArrayLiteralCallArgumentWithoutHanging();
    TestParserHandlesMultiTargetAssignment();
    TestParserPreservesBindingAssignmentAmbiguity();
    return 0;
}
