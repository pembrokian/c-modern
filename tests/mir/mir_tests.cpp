#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

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

mc::mir::LowerResult Lower(const std::string& source,
                          const mc::sema::CheckOptions& options,
                          mc::support::DiagnosticSink& diagnostics) {
    const auto lexed = mc::lex::Lex(source, "<mir-test>", diagnostics);
    const auto parsed = mc::parse::Parse(lexed, "<mir-test>", diagnostics);
    if (!parsed.ok) {
        Fail("source should parse before MIR lowering:\n" + diagnostics.Render());
    }
    const auto checked = mc::sema::CheckProgram(*parsed.source_file, "<mir-test>", options, diagnostics);
    if (!checked.ok) {
        Fail("source should pass semantic checking before MIR lowering:\n" + diagnostics.Render());
    }
    return mc::mir::LowerSourceFile(*parsed.source_file, *checked.module, "<mir-test>", diagnostics);
}

mc::mir::LowerResult Lower(const std::string& source, mc::support::DiagnosticSink& diagnostics) {
    return Lower(source, mc::sema::CheckOptions {}, diagnostics);
}

mc::mir::Module LowerAndValidateModule(const std::string& source) {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(source, diagnostics);
    if (!lowered.ok) {
        Fail("lowering should succeed:\n" + diagnostics.Render());
    }
    if (!mc::mir::ValidateModule(*lowered.module, "<mir-test>", diagnostics)) {
        Fail("lowered module should validate:\n" + diagnostics.Render());
    }
    return *lowered.module;
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
    if (dump.find("variant_match %v1:bool target=Result.Ok target_name=Ok target_base=Result") == std::string::npos ||
        dump.find("variant_extract %v2:i32 op=0 target=Result.Ok target_name=Ok target_base=Result target_index=0") ==
            std::string::npos) {
        Fail("variant switch lowering should preserve structured variant metadata in MIR dumps");
    }
    if (dump.find("unknown") != std::string::npos) {
        Fail("supported variant switch lowering should not emit unknown MIR types");
    }
}

void TestVariantConstructorLoweringSucceeds() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "enum Expr {\n"
        "    Int(value: i32),\n"
        "    Add(left: *Expr, right: *Expr),\n"
        "}\n"
        "\n"
        "func build(left: *Expr, right: *Expr) Expr {\n"
        "    return Expr.Add(left, right)\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("variant constructor lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("variant_init") == std::string::npos) {
        Fail("variant constructor lowering should emit variant_init instructions");
    }
    if (dump.find("target=Expr.Add target_name=Add target_base=Expr target_index=1") == std::string::npos) {
        Fail("variant constructor lowering should preserve variant metadata in MIR dumps");
    }
}

void TestForEachAndDeferLoweringSucceed() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func cleanup() {\n"
        "}\n"
        "\n"
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
    if (dump.find("bounds_check op=index") == std::string::npos) {
        Fail("foreach lowering should emit explicit bounds checks before indexed element loads");
    }
    if (dump.find("target_display=values.len target_base=Slice<i32>") == std::string::npos ||
        dump.find("bounds_check op=index") == std::string::npos ||
        dump.find("target_kind=index target_display=values[index] target_base=Slice<i32>") == std::string::npos ||
        dump.find("target_types=[usize]") == std::string::npos ||
        dump.find("call target=cleanup target_kind=function target_name=cleanup") == std::string::npos) {
        Fail("foreach lowering should preserve structured field, index, and direct-call metadata");
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

void TestImportedModuleSurfaceLowersQualifiedTypesAndTargets() {
    mc::support::DiagnosticSink diagnostics;

    mc::sema::Module imported_mem;
    mc::sema::TypeDeclSummary allocator_type;
    allocator_type.kind = mc::ast::Decl::Kind::kStruct;
    allocator_type.name = "Allocator";
    allocator_type.fields.push_back({"raw", mc::sema::NamedType("uintptr")});
    imported_mem.type_decls.push_back(std::move(allocator_type));
    imported_mem.functions.push_back({
        .name = "default_allocator",
        .return_types = {mc::sema::PointerType(mc::sema::NamedType("Allocator"))},
    });

    std::unordered_map<std::string, mc::sema::Module> imported_modules;
    imported_modules.emplace("mem", std::move(imported_mem));

    mc::sema::CheckOptions options;
    options.imported_modules = &imported_modules;

    const auto lowered = Lower(
        "import mem\n"
        "\n"
        "func expect_alloc(alloc: *mem.Allocator) i32 {\n"
        "    return 0\n"
        "}\n"
        "\n"
        "func main() i32 {\n"
        "    return expect_alloc(mem.default_allocator())\n"
        "}\n",
        options,
        diagnostics);

    if (!lowered.ok) {
        Fail("imported module surface lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("proc()->*mem.Allocator target=mem.default_allocator target_kind=function target_name=mem.default_allocator") ==
            std::string::npos ||
        dump.find("call %v") == std::string::npos ||
        dump.find("target=expect_alloc target_kind=function target_name=expect_alloc") == std::string::npos ||
        dump.find("*mem.Allocator") == std::string::npos) {
        Fail("imported module surface lowering should preserve qualified types and function targets in MIR dumps");
    }
    if (dump.find("unknown") != std::string::npos) {
        Fail("imported module surface lowering should not emit unknown MIR types");
    }
}

void TestImportedModuleVariantMatchLowersQualifiedVariants() {
    mc::support::DiagnosticSink diagnostics;

    mc::sema::Module imported_color;
    mc::sema::TypeDeclSummary color_type;
    color_type.kind = mc::ast::Decl::Kind::kEnum;
    color_type.name = "Color";
    color_type.variants.push_back({.name = "Red", .payload_fields = {}});
    color_type.variants.push_back({.name = "Green", .payload_fields = {}});
    color_type.variants.push_back({.name = "Blue", .payload_fields = {}});
    imported_color.type_decls.push_back(std::move(color_type));
    mc::sema::BuildModuleLookupMaps(imported_color);

    std::unordered_map<std::string, mc::sema::Module> imported_modules;
    imported_modules.emplace("color_module", std::move(imported_color));

    mc::sema::CheckOptions options;
    options.imported_modules = &imported_modules;

    const auto lowered = Lower(
        "import color_module\n"
        "\n"
        "func get_value(c: color_module.Color) i32 {\n"
        "    switch c {\n"
        "    case color_module.Color.Red:\n"
        "        return 1\n"
        "    case color_module.Color.Green:\n"
        "        return 2\n"
        "    case color_module.Color.Blue:\n"
        "        return 3\n"
        "    }\n"
        "    return 0\n"
        "}\n",
        options,
        diagnostics);

    if (!lowered.ok) {
        Fail("imported qualified enum variant matching should lower successfully:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("variant_match") == std::string::npos ||
        dump.find("target=color_module.Color.Red") == std::string::npos ||
        dump.find("target=color_module.Color.Green") == std::string::npos ||
        dump.find("target=color_module.Color.Blue") == std::string::npos) {
        Fail("imported qualified enum variant matching should preserve qualified variant names in MIR");
    }
}

void TestImportedAtomicBoundaryValidationAcceptsQualifiedTypes() {
    mc::support::DiagnosticSink diagnostics;

    mc::sema::Module imported_sync;
    mc::sema::TypeDeclSummary order_type;
    order_type.kind = mc::ast::Decl::Kind::kEnum;
    order_type.name = "MemoryOrder";
    order_type.variants.push_back({.name = "Relaxed", .payload_fields = {}});
    order_type.variants.push_back({.name = "Acquire", .payload_fields = {}});
    order_type.variants.push_back({.name = "Release", .payload_fields = {}});
    imported_sync.type_decls.push_back(std::move(order_type));

    mc::sema::TypeDeclSummary atomic_type;
    atomic_type.kind = mc::ast::Decl::Kind::kStruct;
    atomic_type.name = "Atomic";
    atomic_type.type_params.push_back("T");
    imported_sync.type_decls.push_back(std::move(atomic_type));

    mc::sema::Type atomic_i32 = mc::sema::NamedType("Atomic");
    atomic_i32.subtypes.push_back(mc::sema::NamedType("i32"));
    imported_sync.functions.push_back({
        .name = "atomic_load",
        .param_types = {mc::sema::PointerType(atomic_i32), mc::sema::NamedType("MemoryOrder")},
        .return_types = {mc::sema::NamedType("i32")},
    });
    mc::sema::BuildModuleLookupMaps(imported_sync);

    std::unordered_map<std::string, mc::sema::Module> imported_modules;
    imported_modules.emplace("sync", std::move(imported_sync));

    mc::sema::CheckOptions options;
    options.imported_modules = &imported_modules;

    const auto lowered = Lower(
        "import sync\n"
        "\n"
        "func read(atom: *sync.Atomic<i32>, order: sync.MemoryOrder) i32 {\n"
        "    return sync.atomic_load(atom, order)\n"
        "}\n",
        options,
        diagnostics);

    if (!lowered.ok) {
        Fail("imported atomic boundary lowering should succeed:\n" + diagnostics.Render());
    }
    if (!mc::mir::ValidateModule(*lowered.module, "<mir-test>", diagnostics)) {
        Fail("validator should accept qualified imported atomic boundary types:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("*sync.Atomic<i32>") == std::string::npos || dump.find("Local name=order type=sync.MemoryOrder param readonly") == std::string::npos ||
        dump.find("op=order=order") == std::string::npos) {
        Fail("imported atomic MIR should preserve qualified imported types and order names");
    }
}

void TestGlobalAddressLoweringUsesExplicitLocalAddr() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "const LIMIT: i32 = 3\n"
        "\n"
        "func expose(ptr: *i32) uintptr {\n"
        "    return (uintptr)(ptr)\n"
        "}\n"
        "\n"
        "func use() uintptr {\n"
        "    return expose(&LIMIT)\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("global address lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("local_addr %v0:*i32 target=LIMIT target_kind=global target_name=LIMIT") == std::string::npos &&
        dump.find("local_addr %v1:*i32 target=LIMIT target_kind=global target_name=LIMIT") == std::string::npos) {
        Fail("global address lowering should use explicit local_addr metadata for globals");
    }
    if (dump.find("unary ") != std::string::npos && dump.find(" op=&") != std::string::npos) {
        Fail("global address lowering should not fall back to generic unary '&'");
    }
}

void TestAddressOfFieldFailsCrisplyInMir() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "struct Pair {\n"
        "    left: i32,\n"
        "    right: i32,\n"
        "}\n"
        "\n"
        "func expose(ptr: *i32) uintptr {\n"
        "    return (uintptr)(ptr)\n"
        "}\n"
        "\n"
        "func use(pair: Pair) uintptr {\n"
        "    return expose(&pair.left)\n"
        "}\n",
        diagnostics);

    if (lowered.ok) {
        Fail("address-of field lowering should fail crisply until MIR grows an address-producing field form");
    }
    if (diagnostics.Render().find("not yet supported in MIR: address-of") == std::string::npos) {
        Fail("address-of field lowering should explain the unsupported MIR boundary");
    }
}

void TestMixedBindingOrAssignmentUsesSemanticClassification() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func demo() i32 {\n"
        "    left: i32 = 1\n"
        "    left, right = 2, left\n"
        "    return right\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("mixed binding-or-assignment lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("Local name=right type=i32") == std::string::npos) {
        Fail("mixed binding-or-assignment lowering should create new locals for sema-classified bindings");
    }
    const auto load_pos = dump.find("load_local ");
    const auto left_store_pos = dump.find("store_local target=left", load_pos == std::string::npos ? 0 : load_pos);
    const auto right_store_pos = dump.find("store_local target=right", left_store_pos == std::string::npos ? 0 : left_store_pos);
    if (load_pos == std::string::npos || left_store_pos == std::string::npos || right_store_pos == std::string::npos ||
        !(load_pos < left_store_pos && left_store_pos < right_store_pos)) {
        Fail("mixed binding-or-assignment lowering should evaluate right-hand sides before mutating targets");
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

void TestIndexedStoreLoweringEmitsBoundsChecks() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func write(values: Slice<i32>, idx: usize, value: i32) {\n"
        "    values[idx] = value\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("indexed store lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("bounds_check op=index") == std::string::npos) {
        Fail("indexed store lowering should emit explicit bounds_check instructions");
    }
    if (dump.find("store_target target=values[idx] target_kind=index") == std::string::npos) {
        Fail("indexed store lowering should preserve structured index-store metadata");
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
        "func inner() {\n"
        "}\n"
        "\n"
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
        "func tick() {\n"
        "}\n"
        "\n"
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
        "func hit() {\n"
        "}\n"
        "\n"
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
        "func tick() {\n"
        "}\n"
        "\n"
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
        "func hit(value: i32) {\n"
        "}\n"
        "\n"
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
        Fail("defer lowering should preserve sema-known callee procedure types in hidden locals");
    }
}

void TestUnresolvedCalleeFailsSemanticChecking() {
    mc::support::DiagnosticSink diagnostics;
    const auto lexed = mc::lex::Lex(
        "func demo() i32 {\n"
        "    hit(1)\n"
        "    return 0\n"
        "}\n",
        "<mir-test>",
        diagnostics);

    const auto parsed = mc::parse::Parse(lexed, "<mir-test>", diagnostics);
    if (!parsed.ok) {
        Fail("source should parse before semantic checking:\n" + diagnostics.Render());
    }

    const auto checked = mc::sema::CheckSourceFile(*parsed.source_file, "<mir-test>", diagnostics);
    if (checked.ok) {
        Fail("semantic checking should reject unresolved callees");
    }

    if (diagnostics.Render().find("unknown name: hit") == std::string::npos) {
        Fail("semantic checking should report the unresolved callee name");
    }
}

void TestDivisionAndShiftLoweringEmitExplicitChecks() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func math(value: i32, denom: i32, count: i32) i32 {\n"
        "    left: i32 = value / denom\n"
        "    rem: i32 = value % denom\n"
        "    wide: i32 = value << count\n"
        "    narrow: i32 = value >> count\n"
        "    return left + rem + wide + narrow\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("division and shift lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("div_check op=/") == std::string::npos || dump.find("div_check op=%") == std::string::npos) {
        Fail("division and remainder lowering should emit explicit div_check instructions");
    }
    if (dump.find("shift_check op=<<") == std::string::npos || dump.find("shift_check op=>>") == std::string::npos) {
        Fail("shift lowering should emit explicit shift_check instructions");
    }
}

void TestIntegerArithmeticLowersWithExplicitWrapSemantics() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func ints(left: i32, right: i32) i32 {\n"
        "    sum: i32 = left + right\n"
        "    diff: i32 = sum - right\n"
        "    prod: i32 = diff * right\n"
        "    return prod\n"
        "}\n"
        "\n"
        "func floats(left: f32, right: f32) f32 {\n"
        "    return left + right\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("integer arithmetic lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find(":i32 op=+ arithmetic=wrap") == std::string::npos ||
        dump.find(":i32 op=- arithmetic=wrap") == std::string::npos ||
        dump.find(":i32 op=* arithmetic=wrap") == std::string::npos) {
        Fail("integer arithmetic lowering should emit explicit wrap semantics");
    }
    if (dump.find(":f32 op=+ arithmetic=wrap") != std::string::npos) {
        Fail("floating-point arithmetic should not be marked as wraparound");
    }
}

void TestVolatileAndAtomicCallsLowerExplicitly() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "enum MemoryOrder { Relaxed, Acquire, Release }\n"
        "struct Atomic<T> {}\n"
        "\n"
        "func volatile_load(ptr: *i32) i32 {\n"
        "    return 0\n"
        "}\n"
        "\n"
        "func volatile_store(ptr: *i32, value: i32) {\n"
        "}\n"
        "\n"
        "func atomic_load(ptr: *Atomic<i32>, order: MemoryOrder) i32 {\n"
        "    return 0\n"
        "}\n"
        "\n"
        "func atomic_store(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) {\n"
        "}\n"
        "\n"
        "func atomic_exchange(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) i32 {\n"
        "    return value\n"
        "}\n"
        "\n"
        "func atomic_compare_exchange(ptr: *Atomic<i32>, expected: *i32, desired: i32, success: MemoryOrder, failure: MemoryOrder) bool {\n"
        "    return true\n"
        "}\n"
        "\n"
        "func atomic_fetch_add(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) i32 {\n"
        "    return value\n"
        "}\n"
        "\n"
        "func demo(ptr: *i32, atom: *Atomic<i32>, expected: *i32) bool {\n"
        "    value: i32 = volatile_load(ptr)\n"
        "    volatile_store(ptr, value)\n"
        "    loaded: i32 = atomic_load(atom, MemoryOrder.Acquire)\n"
        "    atomic_store(atom, loaded, MemoryOrder.Release)\n"
        "    swapped: i32 = atomic_exchange(atom, loaded, MemoryOrder.Acquire)\n"
        "    added: i32 = atomic_fetch_add(atom, swapped, MemoryOrder.Release)\n"
        "    return atomic_compare_exchange(atom, expected, added, MemoryOrder.Acquire, MemoryOrder.Relaxed)\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("volatile and atomic lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("volatile_load %v") == std::string::npos || dump.find("volatile_store target=volatile_store") == std::string::npos) {
        Fail("volatile operations should lower to dedicated MIR instructions");
    }
    if (dump.find("atomic_load %v") == std::string::npos || dump.find("atomic_store op=order=") == std::string::npos ||
        dump.find("atomic_exchange %v") == std::string::npos || dump.find("atomic_compare_exchange %v") == std::string::npos ||
        dump.find("atomic_fetch_add %v") == std::string::npos) {
        Fail("atomic operations should lower to dedicated MIR instructions");
    }
    if (dump.find("call target=volatile_load") != std::string::npos || dump.find("call target=atomic_load") != std::string::npos) {
        Fail("dedicated volatile and atomic operations must not lower as generic calls");
    }

    bool saw_atomic_load = false;
    bool saw_atomic_compare_exchange = false;
    for (const auto& function : lowered.module->functions) {
        for (const auto& block : function.blocks) {
            for (const auto& instruction : block.instructions) {
                if (instruction.kind == mc::mir::Instruction::Kind::kAtomicLoad) {
                    saw_atomic_load = true;
                    if (instruction.atomic_order != "MemoryOrder.Acquire" || !instruction.atomic_success_order.empty() ||
                        !instruction.atomic_failure_order.empty() || !instruction.op.empty()) {
                        Fail("atomic_load should carry structured order metadata instead of generic op text");
                    }
                }
                if (instruction.kind == mc::mir::Instruction::Kind::kAtomicCompareExchange) {
                    saw_atomic_compare_exchange = true;
                    if (instruction.atomic_success_order != "MemoryOrder.Acquire" ||
                        instruction.atomic_failure_order != "MemoryOrder.Relaxed" || !instruction.atomic_order.empty() ||
                        !instruction.op.empty()) {
                        Fail("atomic_compare_exchange should carry structured success/failure metadata instead of generic op text");
                    }
                }
            }
        }
    }
    if (!saw_atomic_load || !saw_atomic_compare_exchange) {
        Fail("expected lowered MIR to contain atomic instructions for structured metadata checks");
    }
}

void TestDeferredBoundaryCallsLowerExplicitly() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "enum MemoryOrder { Relaxed, Acquire, Release }\n"
        "struct Atomic<T> {}\n"
        "\n"
        "func volatile_store(ptr: *i32, value: i32) {\n"
        "}\n"
        "\n"
        "func atomic_store(ptr: *Atomic<i32>, value: i32, order: MemoryOrder) {\n"
        "}\n"
        "\n"
        "func flush(ptr: *i32, atom: *Atomic<i32>) {\n"
        "    defer volatile_store(ptr, 1)\n"
        "    defer atomic_store(atom, 2, MemoryOrder.Release)\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("deferred boundary-call lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("volatile_store target=volatile_store") == std::string::npos ||
        dump.find("atomic_store op=order=MemoryOrder.Release target=atomic_store") == std::string::npos) {
        Fail("deferred boundary calls should lower to dedicated MIR instructions");
    }
    if (dump.find("call target=volatile_store") != std::string::npos || dump.find("call target=atomic_store") != std::string::npos) {
        Fail("deferred boundary calls must not fall back to generic call instructions");
    }
}

void TestMmioPtrCallLowersToIntToPointer() {
    mc::support::DiagnosticSink diagnostics;
    const auto lowered = Lower(
        "func mmio_ptr(addr: uintptr) *i32 {\n"
        "    return (*i32)(addr)\n"
        "}\n"
        "\n"
        "func map(addr: uintptr) *i32 {\n"
        "    return mmio_ptr(addr)\n"
        "}\n",
        diagnostics);

    if (!lowered.ok) {
        Fail("mmio_ptr lowering should succeed:\n" + diagnostics.Render());
    }

    const auto dump = mc::mir::DumpModule(*lowered.module);
    if (dump.find("Function name=map") == std::string::npos || dump.find("int_to_pointer %v") == std::string::npos ||
        dump.find(":*i32 target=*i32") == std::string::npos) {
        Fail("mmio_ptr calls should lower through the dedicated int_to_pointer MIR instruction");
    }
    if (dump.find("call target=mmio_ptr") != std::string::npos) {
        Fail("mmio_ptr calls must not lower as generic calls");
    }
}

void TestValidatorRejectsMalformedControlFlowOnLoweredSurface() {
    auto module = LowerAndValidateModule(
        "func choose(flag: bool) i32 {\n"
        "    if flag {\n"
        "        return 1\n"
        "    }\n"
        "    return 0\n"
        "}\n");

    if (module.functions.empty() || module.functions.front().blocks.empty()) {
        Fail("expected lowered function and blocks for control-flow mutation test");
    }

    module.functions.front().blocks.front().terminator.true_target = "missing_phase4_block";

    mc::support::DiagnosticSink diagnostics;
    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject malformed control flow on lowered MIR surface");
    }
    if (diagnostics.Render().find("conditional branch true target missing MIR block") == std::string::npos) {
        Fail("validator should explain malformed control flow on lowered MIR surface");
    }
}

void TestValidatorRejectsIllegalTypedOperationOnLoweredSurface() {
    auto module = LowerAndValidateModule(
        "func add(left: i32, right: i32) i32 {\n"
        "    return left + right\n"
        "}\n");

    bool mutated = false;
    for (auto& function : module.functions) {
        for (auto& block : function.blocks) {
            for (auto& instruction : block.instructions) {
                if (instruction.kind == mc::mir::Instruction::Kind::kBinary && instruction.op == "+") {
                    instruction.arithmetic_semantics = mc::mir::Instruction::ArithmeticSemantics::kNone;
                    mutated = true;
                    break;
                }
            }
            if (mutated) {
                break;
            }
        }
        if (mutated) {
            break;
        }
    }

    if (!mutated) {
        Fail("expected to find integer binary instruction on lowered MIR surface");
    }

    mc::support::DiagnosticSink diagnostics;
    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject illegal typed operations on lowered MIR surface");
    }
    if (diagnostics.Render().find("must record wrap semantics") == std::string::npos) {
        Fail("validator should explain illegal typed operations on lowered MIR surface");
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

void TestValidatorRejectsDivisionWithoutDivCheck() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_div";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "4",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::NamedType("i32"),
                .op = "2",
            },
            {
                .kind = mc::mir::Instruction::Kind::kBinary,
                .result = "%v2",
                .type = mc::sema::NamedType("i32"),
                .op = "/",
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
        Fail("validator should reject division without explicit div_check");
    }
    if (diagnostics.Render().find("must be preceded by matching div_check") == std::string::npos) {
        Fail("validator should explain missing div_check");
    }
}

void TestValidatorRejectsIntegerArithmeticWithoutWrapSemantics() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_wrap";
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
                .kind = mc::mir::Instruction::Kind::kBinary,
                .result = "%v2",
                .type = mc::sema::NamedType("i32"),
                .op = "+",
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
        Fail("validator should reject integer arithmetic without explicit wrap semantics");
    }
    if (diagnostics.Render().find("must record wrap semantics") == std::string::npos) {
        Fail("validator should explain missing wrap semantics");
    }
}

void TestValidatorRejectsWrapSemanticsOnFloatArithmetic() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_float_wrap";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("f32"),
                .op = "1.0",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::NamedType("f32"),
                .op = "2.0",
            },
            {
                .kind = mc::mir::Instruction::Kind::kBinary,
                .result = "%v2",
                .type = mc::sema::NamedType("f32"),
                .op = "+",
                .operands = {"%v0", "%v1"},
                .arithmetic_semantics = mc::mir::Instruction::ArithmeticSemantics::kWrap,
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject wrap semantics on floating-point arithmetic");
    }
    if (diagnostics.Render().find("wrap semantics are only valid") == std::string::npos) {
        Fail("validator should explain invalid wrap semantics");
    }
}

void TestValidatorRejectsGenericVolatileCall() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_volatile_call";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::ProcedureType({mc::sema::PointerType(mc::sema::NamedType("i32"))}, {mc::sema::NamedType("i32")}),
                .target = "volatile_load",
            },
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v1",
                .type = mc::sema::PointerType(mc::sema::NamedType("i32")),
                .target = "ptr",
            },
            {
                .kind = mc::mir::Instruction::Kind::kCall,
                .result = "%v2",
                .type = mc::sema::NamedType("i32"),
                .target = "volatile_load",
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
        Fail("validator should reject generic calls for dedicated volatile operations");
    }
    if (diagnostics.Render().find("dedicated semantic-boundary opcode") == std::string::npos) {
        Fail("validator should explain generic volatile call misuse");
    }
}

void TestValidatorRejectsGenericMmioPtrCall() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_mmio_call";
    function.return_types.push_back(mc::sema::PointerType(mc::sema::NamedType("i32")));
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::ProcedureType({mc::sema::NamedType("uintptr")}, {mc::sema::PointerType(mc::sema::NamedType("i32"))}),
                .target = "mmio_ptr",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::NamedType("uintptr"),
                .op = "0",
            },
            {
                .kind = mc::mir::Instruction::Kind::kCall,
                .result = "%v2",
                .type = mc::sema::PointerType(mc::sema::NamedType("i32")),
                .target = "mmio_ptr",
                .operands = {"%v0", "%v1"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%v2"},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject generic calls for dedicated mmio_ptr lowering");
    }
    if (diagnostics.Render().find("dedicated semantic-boundary opcode") == std::string::npos) {
        Fail("validator should explain generic mmio_ptr call misuse");
    }
}

void TestValidatorRejectsAtomicLoadBadOrderType() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_atomic_order";
    mc::sema::Type atomic_i32 = mc::sema::NamedType("Atomic");
    atomic_i32.subtypes.push_back(mc::sema::NamedType("i32"));
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::PointerType(atomic_i32),
                .target = "atom",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::BoolType(),
                .op = "true",
            },
            {
                .kind = mc::mir::Instruction::Kind::kAtomicLoad,
                .result = "%v2",
                .type = mc::sema::NamedType("i32"),
                .target = "atomic_load",
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
        Fail("validator should reject atomic_load with non-MemoryOrder operand");
    }
    if (diagnostics.Render().find("MemoryOrder operand") == std::string::npos) {
        Fail("validator should explain invalid atomic order operands");
    }
}

void TestValidatorRejectsShiftWithoutShiftCheck() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_shift";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "4",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kBinary,
                .result = "%v2",
                .type = mc::sema::NamedType("i32"),
                .op = "<<",
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
        Fail("validator should reject shift without explicit shift_check");
    }
    if (diagnostics.Render().find("must be preceded by matching shift_check") == std::string::npos) {
        Fail("validator should explain missing shift_check");
    }
}

void TestValidatorRejectsConvertNumericBadTargetMetadata() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_convert_numeric_metadata";
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
                .kind = mc::mir::Instruction::Kind::kConvertNumeric,
                .result = "%v1",
                .type = mc::sema::NamedType("i64"),
                .target = "i32",
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
        Fail("validator should reject convert_numeric target metadata mismatches");
    }
    if (diagnostics.Render().find("convert_numeric target metadata must match result type") == std::string::npos) {
        Fail("validator should explain bad convert_numeric target metadata");
    }
}

void TestValidatorRejectsAtomicCompareExchangeMissingOrderMetadata() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_atomic_cx_orders";
    mc::sema::Type atomic_i32 = mc::sema::NamedType("Atomic");
    atomic_i32.subtypes.push_back(mc::sema::NamedType("i32"));
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::PointerType(atomic_i32),
                .target = "atom",
                .target_kind = mc::mir::Instruction::TargetKind::kOther,
            },
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v1",
                .type = mc::sema::PointerType(mc::sema::NamedType("i32")),
                .target = "expected_ptr",
                .target_kind = mc::mir::Instruction::TargetKind::kOther,
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v2",
                .type = mc::sema::NamedType("i32"),
                .op = "7",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v3",
                .type = mc::sema::NamedType("MemoryOrder"),
                .op = "seq_cst",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v4",
                .type = mc::sema::NamedType("MemoryOrder"),
                .op = "seq_cst",
            },
            {
                .kind = mc::mir::Instruction::Kind::kAtomicCompareExchange,
                .result = "%v5",
                .type = mc::sema::BoolType(),
                .target = "atomic_compare_exchange",
                .operands = {"%v0", "%v1", "%v2", "%v3", "%v4"},
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject atomic_compare_exchange without order metadata");
    }
    if (diagnostics.Render().find("must record success/failure order metadata") == std::string::npos) {
        Fail("validator should explain missing compare-exchange order metadata");
    }
}

void TestValidatorRejectsBufferNewNonAllocatorOperand() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_buffer_new_allocator";
    mc::sema::Type buffer_i32 = mc::sema::NamedType("Buffer");
    buffer_i32.subtypes.push_back(mc::sema::NamedType("i32"));
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::PointerType(mc::sema::NamedType("i32")),
                .target = "not_alloc",
                .target_kind = mc::mir::Instruction::TargetKind::kOther,
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::NamedType("usize"),
                .op = "4",
            },
            {
                .kind = mc::mir::Instruction::Kind::kBufferNew,
                .result = "%v2",
                .type = mc::sema::PointerType(buffer_i32),
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
        Fail("validator should reject buffer_new with non-Allocator operand");
    }
    if (diagnostics.Render().find("buffer_new allocator operand must be *Allocator") == std::string::npos) {
        Fail("validator should explain invalid buffer_new allocator operands");
    }
}

void TestValidatorRejectsBufferFreeNonBufferOperand() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_buffer_free_operand";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::PointerType(mc::sema::NamedType("i32")),
                .target = "value_ptr",
                .target_kind = mc::mir::Instruction::TargetKind::kOther,
            },
            {
                .kind = mc::mir::Instruction::Kind::kBufferFree,
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
        Fail("validator should reject buffer_free with non-Buffer operand");
    }
    if (diagnostics.Render().find("buffer_free requires *Buffer<T> operand") == std::string::npos) {
        Fail("validator should explain invalid buffer_free operands");
    }
}

void TestValidatorRejectsArenaNewNonArenaOperand() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_arena_new_operand";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::PointerType(mc::sema::NamedType("i32")),
                .target = "value_ptr",
                .target_kind = mc::mir::Instruction::TargetKind::kOther,
            },
            {
                .kind = mc::mir::Instruction::Kind::kArenaNew,
                .result = "%v1",
                .type = mc::sema::PointerType(mc::sema::NamedType("i32")),
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
        Fail("validator should reject arena_new with non-Arena operand");
    }
    if (diagnostics.Render().find("arena_new requires Arena operand") == std::string::npos) {
        Fail("validator should explain invalid arena_new operands");
    }
}

void TestValidatorRejectsSliceFromBufferAfterLoweringBoundary() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_slice_from_buffer";
    mc::sema::Type buffer_i32 = mc::sema::NamedType("Buffer");
    buffer_i32.subtypes.push_back(mc::sema::NamedType("i32"));
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::PointerType(buffer_i32),
                .target = "buf",
                .target_kind = mc::mir::Instruction::TargetKind::kOther,
            },
            {
                .kind = mc::mir::Instruction::Kind::kSliceFromBuffer,
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
        Fail("validator should reject slice_from_buffer after the lowering boundary");
    }
    if (diagnostics.Render().find("slice_from_buffer should lower") == std::string::npos) {
        Fail("validator should explain the slice_from_buffer boundary requirement");
    }
}

void TestValidatorRejectsStoreTargetIndexMissingAuxType() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_store_index_metadata";
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
                .kind = mc::mir::Instruction::Kind::kStoreTarget,
                .type = mc::sema::NamedType("i32"),
                .target = "values[idx]",
                .target_kind = mc::mir::Instruction::TargetKind::kIndex,
                .target_base_type = mc::sema::ArrayType(mc::sema::NamedType("i32"), "4"),
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
        Fail("validator should reject store_target index operations without aux index metadata");
    }
    if (diagnostics.Render().find("must carry exactly one index type") == std::string::npos) {
        Fail("validator should explain missing store_target index metadata");
    }
}

void TestValidatorRejectsKnownFunctionSymbolRefMissingTargetName() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;

    mc::mir::Function callee;
    callee.name = "callee";
    callee.blocks.push_back({
        .label = "entry",
        .instructions = {},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(callee));

    mc::mir::Function function;
    function.name = "broken_missing_symbol_target_name";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::ProcedureType({}, {}),
                .target = "callee",
                .target_kind = mc::mir::Instruction::TargetKind::kFunction,
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject known function symbol_ref instructions without target_name metadata");
    }
    if (diagnostics.Render().find("must record target_name metadata") == std::string::npos) {
        Fail("validator should explain missing function symbol_ref target_name metadata");
    }
}

void TestValidatorRejectsDirectCallMissingTargetName() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;

    mc::mir::Function callee;
    callee.name = "callee";
    callee.blocks.push_back({
        .label = "entry",
        .instructions = {},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(callee));

    mc::mir::Function function;
    function.name = "broken_direct_call_target_name";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kSymbolRef,
                .result = "%v0",
                .type = mc::sema::ProcedureType({}, {}),
                .target = "callee",
                .target_kind = mc::mir::Instruction::TargetKind::kFunction,
                .target_name = "callee",
            },
            {
                .kind = mc::mir::Instruction::Kind::kCall,
                .type = mc::sema::VoidType(),
                .target = "callee",
                .target_kind = mc::mir::Instruction::TargetKind::kFunction,
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
        Fail("validator should reject direct call instructions without target_name metadata");
    }
    if (diagnostics.Render().find("call must record target_name metadata") == std::string::npos) {
        Fail("validator should explain missing direct-call target metadata");
    }
}

void TestValidatorRejectsUnterminatedUnreachableBlock() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_dead_block";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kBranch,
            .values = {},
            .true_target = "exit",
        },
    });
    function.blocks.push_back({
        .label = "exit",
        .instructions = {},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    function.blocks.push_back({
        .label = "dead",
        .instructions = {},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kNone,
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject unterminated unreachable blocks");
    }
    if (diagnostics.Render().find("unterminated MIR block") == std::string::npos) {
        Fail("validator should explain unterminated unreachable blocks");
    }
}

void TestValidatorRejectsBranchTerminatorWithValues() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "broken_branch_values";
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kBranch,
            .values = {"%v0"},
            .true_target = "exit",
        },
    });
    function.blocks.push_back({
        .label = "exit",
        .instructions = {},
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject branch terminators that carry values");
    }
    if (diagnostics.Render().find("branch terminator must not carry values") == std::string::npos) {
        Fail("validator should explain invalid branch terminator payloads");
    }
}

// CFG: entry --cond--> then | merge
//       then -----------> merge
// %v1 is defined only in `then`; `merge` uses it as an operand.
// `then` does not dominate `merge`, so this must be rejected.
void TestValidatorRejectsSSADominanceViolation() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "bad_dom";
    function.return_types.push_back(mc::sema::NamedType("i32"));
    // entry: define %cond (bool), cond-branch to then/merge
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%cond",
                .type = mc::sema::BoolType(),
                .op = "true",
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kCondBranch,
            .values = {"%cond"},
            .true_target = "then",
            .false_target = "merge",
        },
    });
    // then: define %v1, branch to merge
    function.blocks.push_back({
        .label = "then",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v1",
                .type = mc::sema::NamedType("i32"),
                .op = "42",
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kBranch,
            .true_target = "merge",
        },
    });
    // merge: uses %v1 — dominance violation
    function.blocks.push_back({
        .label = "merge",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%zero",
                .type = mc::sema::NamedType("i32"),
                .op = "0",
            },
            {
                .kind = mc::mir::Instruction::Kind::kBinary,
                .result = "%result",
                .type = mc::sema::NamedType("i32"),
                .op = "+",
                .operands = {"%v1", "%zero"},
                .arithmetic_semantics = mc::mir::Instruction::ArithmeticSemantics::kWrap,
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%result"},
        },
    });
    module.functions.push_back(std::move(function));

    if (mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should reject SSA dominance violation");
    }
    if (diagnostics.Render().find("SSA dominance violation") == std::string::npos) {
        Fail("validator should name the SSA dominance violation: " + diagnostics.Render());
    }
}

// CFG: entry --cond--> then | else_blk; then --> exit; else_blk --> exit
// %v0 is defined in entry (which dominates all blocks); both successors may use it.
void TestValidatorAcceptsDominatingBlockValue() {
    mc::support::DiagnosticSink diagnostics;
    mc::mir::Module module;
    mc::mir::Function function;
    function.name = "good_dom";
    function.return_types.push_back(mc::sema::NamedType("i32"));
    // entry: define %cond and %v0, cond-branch
    function.blocks.push_back({
        .label = "entry",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%cond",
                .type = mc::sema::BoolType(),
                .op = "true",
            },
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%v0",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kCondBranch,
            .values = {"%cond"},
            .true_target = "then",
            .false_target = "else_blk",
        },
    });
    // then: use %v0 (dominated by entry — OK)
    function.blocks.push_back({
        .label = "then",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%one",
                .type = mc::sema::NamedType("i32"),
                .op = "1",
            },
            {
                .kind = mc::mir::Instruction::Kind::kBinary,
                .result = "%then_res",
                .type = mc::sema::NamedType("i32"),
                .op = "+",
                .operands = {"%v0", "%one"},
                .arithmetic_semantics = mc::mir::Instruction::ArithmeticSemantics::kWrap,
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%then_res"},
        },
    });
    // else_blk: use %v0 (dominated by entry — OK)
    function.blocks.push_back({
        .label = "else_blk",
        .instructions = {
            {
                .kind = mc::mir::Instruction::Kind::kConst,
                .result = "%two",
                .type = mc::sema::NamedType("i32"),
                .op = "2",
            },
            {
                .kind = mc::mir::Instruction::Kind::kBinary,
                .result = "%else_res",
                .type = mc::sema::NamedType("i32"),
                .op = "+",
                .operands = {"%v0", "%two"},
                .arithmetic_semantics = mc::mir::Instruction::ArithmeticSemantics::kWrap,
            },
        },
        .terminator = {
            .kind = mc::mir::Terminator::Kind::kReturn,
            .values = {"%else_res"},
        },
    });
    module.functions.push_back(std::move(function));

    if (!mc::mir::ValidateModule(module, "<mir-test>", diagnostics)) {
        Fail("validator should accept value uses dominated by definition: " + diagnostics.Render());
    }
}

}  // namespace

int main() {
    TestLoweringProducesDeterministicDump();
    TestIfElseLoweringBranchesToMerge();
    TestVariantSwitchLoweringSucceeds();
    TestVariantConstructorLoweringSucceeds();
    TestForEachAndDeferLoweringSucceed();
    TestForRangeUsesSemaRangeElementType();
    TestKnownSymbolRefsAreTyped();
    TestImportedModuleSurfaceLowersQualifiedTypesAndTargets();
    TestImportedModuleVariantMatchLowersQualifiedVariants();
    TestImportedAtomicBoundaryValidationAcceptsQualifiedTypes();
    TestGlobalAddressLoweringUsesExplicitLocalAddr();
    TestAddressOfFieldFailsCrisplyInMir();
    TestMixedBindingOrAssignmentUsesSemanticClassification();
    TestExplicitConversionLowersToConvert();
    TestNumericConversionLowersToConvertNumeric();
    TestArrayToSliceConversionLowersExplicitly();
    TestBufferToSliceConversionLowersExplicitly();
    TestIndexAndSliceLoweringEmitBoundsChecks();
    TestIndexedStoreLoweringEmitsBoundsChecks();
    TestPointerToIntConversionLowersExplicitly();
    TestIntToPointerConversionLowersExplicitly();
    TestScopedDeferRunsBeforeFollowingStatement();
    TestLoopExitUnwindsScopedDefers();
    TestSwitchArmUnwindsScopedDefers();
    TestLoopIterationDefersRunBeforeStep();
    TestDeferArgumentsAreEvaluatedImmediately();
    TestUnresolvedCalleeFailsSemanticChecking();
    TestDivisionAndShiftLoweringEmitExplicitChecks();
    TestIntegerArithmeticLowersWithExplicitWrapSemantics();
    TestVolatileAndAtomicCallsLowerExplicitly();
    TestDeferredBoundaryCallsLowerExplicitly();
    TestMmioPtrCallLowersToIntToPointer();
    TestValidatorRejectsMalformedControlFlowOnLoweredSurface();
    TestValidatorRejectsIllegalTypedOperationOnLoweredSurface();
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
    TestValidatorRejectsDivisionWithoutDivCheck();
    TestValidatorRejectsIntegerArithmeticWithoutWrapSemantics();
    TestValidatorRejectsWrapSemanticsOnFloatArithmetic();
    TestValidatorRejectsGenericVolatileCall();
    TestValidatorRejectsGenericMmioPtrCall();
    TestValidatorRejectsAtomicLoadBadOrderType();
    TestValidatorRejectsShiftWithoutShiftCheck();
    TestValidatorRejectsConvertNumericBadTargetMetadata();
    TestValidatorRejectsAtomicCompareExchangeMissingOrderMetadata();
    TestValidatorRejectsBufferNewNonAllocatorOperand();
    TestValidatorRejectsBufferFreeNonBufferOperand();
    TestValidatorRejectsArenaNewNonArenaOperand();
    TestValidatorRejectsSliceFromBufferAfterLoweringBoundary();
    TestValidatorRejectsStoreTargetIndexMissingAuxType();
    TestValidatorRejectsKnownFunctionSymbolRefMissingTargetName();
    TestValidatorRejectsDirectCallMissingTargetName();
    TestValidatorRejectsUnterminatedUnreachableBlock();
    TestValidatorRejectsBranchTerminatorWithValues();
    TestValidatorRejectsSSADominanceViolation();
    TestValidatorAcceptsDominatingBlockValue();
    return 0;
}