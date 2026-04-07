#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "compiler/ast/ast.h"
#include "compiler/lex/lexer.h"
#include "compiler/mci/mci.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/sema/const_eval.h"
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

void TestMciRoundTrip() {
    mc::sema::Module module;
    module.imports.push_back("helper_dep");
    mc::sema::FunctionSignature function;
    function.name = "answer";
    function.param_types = {mc::sema::PointerType(mc::sema::NamedType("i32"))};
    function.param_is_noalias = {true};
    function.return_types = {mc::sema::NamedType("i32"), mc::sema::RangeType(mc::sema::NamedType("i32"))};
    module.functions.push_back(std::move(function));

    mc::sema::FunctionSignature tuple_user;
    tuple_user.name = "tuple_user";
    tuple_user.param_types = {mc::sema::TupleType({mc::sema::NamedType("i32"), mc::sema::NamedType("i32")})};
    tuple_user.param_is_noalias = {false};
    tuple_user.return_types = {mc::sema::TupleType({mc::sema::NamedType("i32"), mc::sema::NamedType("i32")})};
    module.functions.push_back(std::move(tuple_user));

    mc::sema::TypeDeclSummary type_decl;
    type_decl.kind = mc::ast::Decl::Kind::kStruct;
    type_decl.name = "Pair";
    type_decl.type_params = {"T"};
    type_decl.fields = {{"left", mc::sema::NamedType("T")}, {"right", mc::sema::PointerType(mc::sema::NamedType("T"))}};
    type_decl.layout = {
        .valid = true,
        .size = 8,
        .alignment = 4,
        .field_offsets = {0, 4},
    };
    module.type_decls.push_back(type_decl);

    mc::sema::GlobalSummary global;
    global.is_const = true;
    global.names = {"answer_seed"};
    global.type = mc::sema::NamedType("i32");
    global.constant_values = {mc::sema::ConstValue {
        .kind = mc::sema::ConstValue::Kind::kInteger,
        .integer_value = 42,
        .text = "42",
    }};
    global.zero_initialized_values = {false};
    module.globals.push_back(std::move(global));

    mc::sema::GlobalSummary aggregate_global;
    aggregate_global.is_const = true;
    aggregate_global.names = {"default_pair"};
    aggregate_global.type = mc::sema::NamedType("Pair");
    aggregate_global.type.subtypes = {mc::sema::NamedType("i32")};
    mc::sema::ConstValue aggregate_value;
    aggregate_value.kind = mc::sema::ConstValue::Kind::kAggregate;
    aggregate_value.field_names = {"left", "right"};
    aggregate_value.elements = {
        mc::sema::MakeConstValue(static_cast<std::int64_t>(7)),
        mc::sema::ConstValue {
            .kind = mc::sema::ConstValue::Kind::kNil,
            .text = "nil",
        },
    };
    aggregate_value.text = mc::sema::RenderConstValue(aggregate_value);
    aggregate_global.constant_values = {aggregate_value};
    aggregate_global.zero_initialized_values = {false};
    module.globals.push_back(std::move(aggregate_global));

    mc::support::DiagnosticSink diagnostics;
    const auto temp_path = std::filesystem::temp_directory_path() / "c_modern_phase7_roundtrip.mci";
    const mc::mci::InterfaceArtifact artifact {
        .target_identity = "arm64-apple-darwin25.4.0",
        .package_identity = "phase7-tests",
        .module_name = "helper",
        .module_kind = mc::ast::SourceFile::ModuleKind::kOrdinary,
        .source_path = "tests/tool/phase7_project/src/helper.mc",
        .module = module,
    };

    Expect(mc::mci::WriteInterfaceArtifact(temp_path, artifact, diagnostics),
           "mci writer should emit deterministic interface artifacts");
    Expect(!diagnostics.HasErrors(), "mci writer should not report errors for valid interface artifacts");

    const auto loaded = mc::mci::LoadInterfaceArtifact(temp_path, diagnostics);
    Expect(loaded.has_value(), "mci loader should read interface artifacts back");
    Expect(!diagnostics.HasErrors(), "mci loader should accept its own canonical encoding");
        Expect(loaded->package_identity == "phase7-tests", "mci loader should preserve the package identity");
    Expect(loaded->module_name == "helper", "mci loader should preserve the module name");
        Expect(loaded->module_kind == mc::ast::SourceFile::ModuleKind::kOrdinary,
            "mci loader should preserve the module kind");
    Expect(loaded->module.imports.size() == 1 && loaded->module.imports[0] == "helper_dep",
           "mci loader should preserve import lists");
        Expect(loaded->module.functions.size() == 2 && loaded->module.functions[0].name == "answer",
           "mci loader should preserve exported function signatures");
        Expect(loaded->module.functions[0].param_is_noalias.size() == 1 && loaded->module.functions[0].param_is_noalias[0],
            "mci loader should preserve exported function parameter attributes");
        Expect(loaded->module.functions[0].return_types.size() == 2 && loaded->module.functions[0].return_types[1] == mc::sema::RangeType(mc::sema::NamedType("i32")),
            "mci loader should preserve range return types");
        Expect(loaded->module.functions.size() == 2 && loaded->module.functions[1].param_types.size() == 1 &&
             loaded->module.functions[1].param_types[0] == mc::sema::TupleType({mc::sema::NamedType("i32"), mc::sema::NamedType("i32")}) &&
             loaded->module.functions[1].return_types.size() == 1 &&
             loaded->module.functions[1].return_types[0] == mc::sema::TupleType({mc::sema::NamedType("i32"), mc::sema::NamedType("i32")}),
            "mci loader should preserve tuple parameter and return types");
    Expect(loaded->module.type_decls.size() == 1 && loaded->module.type_decls[0].name == "Pair",
           "mci loader should preserve exported type declarations");
        Expect(loaded->module.type_decls[0].type_params.size() == 1 && loaded->module.type_decls[0].type_params[0] == "T",
            "mci loader should preserve generic type parameters");
        Expect(loaded->module.type_decls[0].fields.size() == 2 && loaded->module.type_decls[0].fields[0].second == mc::sema::NamedType("T") &&
             loaded->module.type_decls[0].fields[1].second == mc::sema::PointerType(mc::sema::NamedType("T")),
            "mci loader should preserve generic field types");
        Expect(loaded->module.globals.size() == 2,
               "mci loader should preserve exported global count");
        Expect(loaded->module.globals[0].constant_values.size() == 1 && loaded->module.globals[0].constant_values[0].has_value() &&
                   loaded->module.globals[0].constant_values[0]->kind == mc::sema::ConstValue::Kind::kInteger &&
                   loaded->module.globals[0].constant_values[0]->integer_value == 42 &&
                   loaded->module.globals[0].zero_initialized_values.size() == 1 && !loaded->module.globals[0].zero_initialized_values[0],
               "mci loader should preserve exported scalar compile-time constant values");
        Expect(loaded->module.globals[1].constant_values.size() == 1 && loaded->module.globals[1].constant_values[0].has_value() &&
                   loaded->module.globals[1].constant_values[0]->kind == mc::sema::ConstValue::Kind::kAggregate &&
                   loaded->module.globals[1].constant_values[0]->field_names.size() == 2 &&
                   loaded->module.globals[1].constant_values[0]->field_names[0] == "left" &&
                   loaded->module.globals[1].constant_values[0]->field_names[1] == "right" &&
                   loaded->module.globals[1].constant_values[0]->elements.size() == 2 &&
                   loaded->module.globals[1].constant_values[0]->elements[0].kind == mc::sema::ConstValue::Kind::kInteger &&
                   loaded->module.globals[1].constant_values[0]->elements[0].integer_value == 7 &&
                   loaded->module.globals[1].constant_values[0]->elements[1].kind == mc::sema::ConstValue::Kind::kNil,
               "mci loader should preserve exported structural compile-time constant values");
    Expect(!loaded->interface_hash.empty(), "mci loader should preserve interface hashes");

    std::filesystem::remove(temp_path);
}

void TestSemaLookupFallsBackToModuleContents() {
    mc::sema::Module module;
    module.functions.push_back({.name = "first"});
    module.type_decls.push_back({.kind = mc::ast::Decl::Kind::kStruct, .name = "First"});
    mc::sema::BuildModuleLookupMaps(module);

    module.functions.push_back({.name = "late_add"});
    module.type_decls.push_back({.kind = mc::ast::Decl::Kind::kStruct, .name = "LateAdd"});

    Expect(mc::sema::FindFunctionSignature(module, "late_add") != nullptr,
           "function lookup should stay correct when declaration vectors grow after cache rebuild");
    Expect(mc::sema::FindTypeDecl(module, "LateAdd") != nullptr,
           "type lookup should stay correct when declaration vectors grow after cache rebuild");
}

void TestLexerTracksKeywordsAndSeparators() {
    mc::support::DiagnosticSink diagnostics;
    const auto lexed = mc::lex::Lex("import io\nfunc main() {}\n", "<test>", diagnostics);
    Expect(lexed.ok, "lexer should accept a basic module");
    Expect(!diagnostics.HasErrors(), "lexer should not emit diagnostics for valid input");
    Expect(lexed.tokens.size() >= 7, "lexer should emit a real token stream");
    Expect(lexed.tokens[0].kind == mc::lex::TokenKind::kImport, "lexer should recognize import keyword");
    Expect(lexed.tokens[2].kind == mc::lex::TokenKind::kNewline, "lexer should preserve newline separators");
}

void TestParserBuildsDeterministicAstDump() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "import io\n"
        "@trace\n"
        "func main(args: Slice<cstr>) i32 {\n"
        "    cfg = Config{ path: \"out\" }\n"
        "    if args.len == 0 {\n"
        "        return 1\n"
        "    }\n"
        "    if io.write(cfg.path) != 0 {\n"
        "        return 1\n"
        "    }\n"
        "    return 0\n"
        "}\n",
        diagnostics);

    Expect(parsed.ok, "parser should accept a representative Phase 2 module");
    Expect(!diagnostics.HasErrors(), "valid module should not emit diagnostics");

    const auto dump = mc::ast::DumpSourceFile(*parsed.source_file);
    Expect(dump.find("SourceFile") != std::string::npos, "dump should start with SourceFile");
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
        "    total = id<i32>(total)\n"
        "    return total\n"
        "}\n"
        "\n"
        "func id<T>(value: T) T {\n"
        "    return value\n"
        "}\n",
        diagnostics);

    Expect(parsed.ok, "parser should accept generic types and loop forms");
    const auto dump = mc::ast::DumpSourceFile(*parsed.source_file);
    Expect(dump.find("StructDecl name=Buffer") != std::string::npos, "dump should include struct declaration");
    Expect(dump.find("PointerType") != std::string::npos, "dump should include pointer types");
    Expect(dump.find("typeArgs:") != std::string::npos, "dump should include call type arguments");
    Expect(dump.find("ForEachIndexStmt") != std::string::npos, "dump should include index foreach loop");
    Expect(dump.find("ForInStmt") != std::string::npos, "dump should include deferred for-in loop");
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
    TestMciRoundTrip();
    TestSemaLookupFallsBackToModuleContents();
    TestLexerTracksKeywordsAndSeparators();
    TestParserBuildsDeterministicAstDump();
    TestParserHandlesTypesAndLoopForms();
    TestParserReportsMalformedInput();
    TestParserRejectsArrayLiteralCallArgumentWithoutHanging();
    TestParserHandlesMultiTargetAssignment();
    TestParserPreservesBindingAssignmentAmbiguity();
    return 0;
}
