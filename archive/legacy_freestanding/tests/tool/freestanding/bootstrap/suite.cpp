#include <filesystem>
#include <string>

#include "compiler/support/dump_paths.h"
#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;
using namespace mc::tool_tests;
using mc::tool_tests::BuildProjectTargetAndExpectFailure;
using mc::tool_tests::BuildProjectTargetAndExpectSuccess;
using mc::tool_tests::CheckProjectTargetAndExpectFailure;
using mc::tool_tests::RunProjectTargetAndExpectSuccess;

void TestPhase81FreestandingBootstrapAndHal(const std::filesystem::path& source_root,
                                            const std::filesystem::path& binary_root,
                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "freestanding_boot_hal_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase81-freestanding\"\n"
              "default = \"boot\"\n"
              "\n"
              "[targets.boot]\n"
              "kind = \"exe\"\n"
              "package = \"phase81\"\n"
              "root = \"src/boot.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.boot.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.boot.runtime]\n"
              "startup = \"bootstrap_main\"\n"
              "\n"
              "[targets.console]\n"
              "kind = \"exe\"\n"
              "package = \"phase81\"\n"
              "root = \"src/console.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.console.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.console.runtime]\n"
              "startup = \"bootstrap_main\"\n");
    WriteFile(project_root / "src/boot.mc",
              "func bootstrap_main() i32 {\n"
              "    return 81\n"
              "}\n");
    WriteFile(project_root / "src/console.mc",
              "import hal\n"
              "\n"
              "const UART0: uintptr = 4096\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    tx: *i32 = hal.mmio_ptr<i32>(UART0)\n"
              "    hal.volatile_store<i32>(tx, 65)\n"
              "    return hal.volatile_load<i32>(tx)\n"
              "}\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path boot_build_dir = binary_root / "freestanding_boot_build";
    std::filesystem::remove_all(boot_build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       boot_build_dir,
                                       "boot",
                                       "freestanding_boot_build_output.txt",
                                       "freestanding boot build");

    const auto boot_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/boot.mc", boot_build_dir);
    const std::filesystem::path boot_runtime_object =
        boot_targets.object.parent_path() / (boot_targets.object.stem().generic_string() + ".freestanding.bootstrap_main.runtime.o");
    if (!std::filesystem::exists(boot_targets.executable)) {
        Fail("phase81 freestanding boot build should produce an executable");
    }
    if (!std::filesystem::exists(boot_runtime_object)) {
        Fail("phase81 freestanding boot build should produce a freestanding runtime object");
    }

    const auto [boot_outcome, boot_output] = RunCommandCapture({boot_targets.executable.generic_string()},
                                                               boot_build_dir / "freestanding_boot_run_output.txt",
                                                               "freestanding boot executable run");
    if (!boot_outcome.exited || boot_outcome.exit_code != 81) {
        Fail("phase81 freestanding boot executable should exit with the bootstrap marker:\n" + boot_output);
    }

    const std::filesystem::path console_build_dir = binary_root / "freestanding_console_build";
    std::filesystem::remove_all(console_build_dir);

    const auto [console_outcome, console_output] = RunCommandCapture({mc_path.generic_string(),
                                                                      "build",
                                                                      "--project",
                                                                      project_path.generic_string(),
                                                                      "--target",
                                                                      "console",
                                                                      "--build-dir",
                                                                      console_build_dir.generic_string(),
                                                                      "--dump-mir"},
                                                                     console_build_dir / "freestanding_console_build_output.txt",
                                                                     "freestanding hal build");
    if (!console_outcome.exited || console_outcome.exit_code != 0) {
        Fail("phase81 freestanding hal build should succeed:\n" + console_output);
    }

    const auto console_dump_targets = mc::support::ComputeDumpTargets(project_root / "src/console.mc", console_build_dir);
    const auto console_build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/console.mc", console_build_dir);
    const std::string console_mir = ReadFile(console_dump_targets.mir);
    const std::string console_llvm_ir = ReadFile(console_build_targets.llvm_ir);
    ExpectOutputContains(console_mir,
                         "int_to_pointer %v",
                         "phase81 freestanding hal MIR should lower hal.mmio_ptr to int_to_pointer");
    ExpectOutputContains(console_mir,
                         "volatile_store target=hal.volatile_store",
                         "phase81 freestanding hal MIR should lower hal.volatile_store to volatile_store");
    ExpectOutputContains(console_mir,
                         "volatile_load %v",
                         "phase81 freestanding hal MIR should lower hal.volatile_load to volatile_load");
    ExpectOutputContains(console_llvm_ir,
                         "inttoptr i64",
                         "phase81 freestanding hal LLVM IR should preserve the MMIO pointer conversion");
    ExpectOutputContains(console_llvm_ir,
                         "store volatile",
                         "phase81 freestanding hal LLVM IR should preserve volatile stores");
    ExpectOutputContains(console_llvm_ir,
                         "load volatile i32",
                         "phase81 freestanding hal LLVM IR should preserve volatile loads");
}

void TestPhase82FreestandingArtifactAndEntryHardening(const std::filesystem::path& source_root,
                                                      const std::filesystem::path& binary_root,
                                                      const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "freestanding_link_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc",
              "extern(c) func phase82_support_value() i32\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    return phase82_support_value()\n"
              "}\n");
    WriteFile(project_root / "target/phase82_support.c",
              "int phase82_support_value(void) {\n"
              "    return 82;\n"
              "}\n");

    const std::filesystem::path support_object = project_root / "target/phase82_support.o";
    const auto [support_compile_outcome, support_compile_output] = RunCommandCapture({"xcrun",
                                                                                      "clang",
                                                                                      "-target",
                                                                                      std::string(mc::kBootstrapTriple),
                                                                                      "-c",
                                                                                      (project_root / "target/phase82_support.c").generic_string(),
                                                                                      "-o",
                                                                                      support_object.generic_string()},
                                                                                     project_root / "target/support_compile_output.txt",
                                                                                     "support object compile");
    if (!support_compile_outcome.exited || support_compile_outcome.exit_code != 0) {
        Fail("phase82 support object compile should pass:\n" + support_compile_output);
    }

    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase82-freestanding\"\n"
              "default = \"boot\"\n"
              "\n"
              "[targets.boot]\n"
              "kind = \"exe\"\n"
              "package = \"phase82\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.boot.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.boot.runtime]\n"
              "startup = \"bootstrap_main\"\n"
              "\n"
              "[targets.boot.link]\n"
              "inputs = [\"target/phase82_support.o\"]\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "freestanding_link_build";
    std::filesystem::remove_all(build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "boot",
                                       "freestanding_link_build_output.txt",
                                       "freestanding explicit link input build");

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "freestanding_link_run_output.txt",
                                                             "freestanding explicit link input run");
    if (!run_outcome.exited || run_outcome.exit_code != 82) {
        Fail("phase82 freestanding explicit link input run should exit with the support object result:\n" + run_output);
    }

    const std::filesystem::path missing_target_project_root = binary_root / "freestanding_missing_target_project";
    std::filesystem::remove_all(missing_target_project_root);
    WriteFile(missing_target_project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase82-missing-target\"\n"
              "default = \"boot\"\n"
              "\n"
              "[targets.boot]\n"
              "kind = \"exe\"\n"
              "package = \"phase82\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "\n"
              "[targets.boot.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.boot.runtime]\n"
              "startup = \"bootstrap_main\"\n");
    WriteFile(missing_target_project_root / "src/main.mc",
              "func bootstrap_main() i32 {\n"
              "    return 0\n"
              "}\n");

    const std::string missing_target_output = BuildProjectTargetAndExpectFailure(mc_path,
                                                                                 missing_target_project_root / "build.toml",
                                                                                 binary_root / "freestanding_missing_target_build",
                                                                                 "boot",
                                                                                 "freestanding_missing_target_output.txt",
                                                                                 "freestanding missing explicit target");
    ExpectOutputContains(missing_target_output,
                         "must declare an explicit freestanding target",
                         "phase82 freestanding builds should reject missing explicit target identity");

    const std::filesystem::path missing_link_project_root = binary_root / "freestanding_missing_link_input_project";
    std::filesystem::remove_all(missing_link_project_root);
    WriteFile(missing_link_project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase82-missing-link-input\"\n"
              "default = \"boot\"\n"
              "\n"
              "[targets.boot]\n"
              "kind = \"exe\"\n"
              "package = \"phase82\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.boot.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.boot.runtime]\n"
              "startup = \"bootstrap_main\"\n"
              "\n"
              "[targets.boot.link]\n"
              "inputs = [\"target/missing_support.o\"]\n");
    WriteFile(missing_link_project_root / "src/main.mc",
              "extern(c) func phase82_support_value() i32\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    return phase82_support_value()\n"
              "}\n");

    const std::string missing_link_output = BuildProjectTargetAndExpectFailure(mc_path,
                                                                               missing_link_project_root / "build.toml",
                                                                               binary_root / "freestanding_missing_link_input_build",
                                                                               "boot",
                                                                               "freestanding_missing_link_input_output.txt",
                                                                               "freestanding missing explicit link input");
    ExpectOutputContains(missing_link_output,
                         "could not read explicit link input",
                         "phase82 freestanding builds should reject missing explicit link inputs");
}

void TestPhase83NarrowHalAdmissionAndConsoleProof(const std::filesystem::path& source_root,
                                                  const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "hal_console_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc",
              "import hal\n"
              "\n"
              "extern(c) func phase83_uart_status_addr() uintptr\n"
              "extern(c) func phase83_uart_data_addr() uintptr\n"
              "extern(c) func phase83_uart_reset()\n"
              "extern(c) func phase83_uart_commit()\n"
              "extern(c) func phase83_uart_result() i32\n"
              "\n"
              "const UART_TX_READY: u32 = 1\n"
              "\n"
              "func uart_status_ptr() *u32 {\n"
              "    return hal.mmio_ptr<u32>(phase83_uart_status_addr())\n"
              "}\n"
              "\n"
              "func uart_data_ptr() *u32 {\n"
              "    return hal.mmio_ptr<u32>(phase83_uart_data_addr())\n"
              "}\n"
              "\n"
              "func uart_tx_ready() bool {\n"
              "    status: u32 = hal.volatile_load<u32>(uart_status_ptr())\n"
              "    return status == UART_TX_READY\n"
              "}\n"
              "\n"
              "func uart_write_byte(b: u8) {\n"
              "    while !uart_tx_ready() {\n"
              "    }\n"
              "    value: u32 = (u32)(b)\n"
              "    hal.volatile_store<u32>(uart_data_ptr(), value)\n"
              "    phase83_uart_commit()\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    phase83_uart_reset()\n"
              "    bytes: [2]u8\n"
              "    bytes[0] = 79\n"
              "    bytes[1] = 75\n"
              "    idx: usize = 0\n"
              "    while idx < 2 {\n"
              "        uart_write_byte(bytes[idx])\n"
              "        idx = idx + 1\n"
              "    }\n"
              "    return phase83_uart_result()\n"
              "}\n");
    WriteFile(project_root / "target/phase83_uart_support.c",
              "#include <stdint.h>\n"
              "\n"
              "static volatile uint32_t phase83_uart_status = 1;\n"
              "static volatile uint32_t phase83_uart_data = 0;\n"
              "static uint8_t phase83_uart_log[4] = {0, 0, 0, 0};\n"
              "static uint32_t phase83_uart_count = 0;\n"
              "\n"
              "uintptr_t phase83_uart_status_addr(void) {\n"
              "    return (uintptr_t)(&phase83_uart_status);\n"
              "}\n"
              "\n"
              "uintptr_t phase83_uart_data_addr(void) {\n"
              "    return (uintptr_t)(&phase83_uart_data);\n"
              "}\n"
              "\n"
              "void phase83_uart_reset(void) {\n"
              "    phase83_uart_status = 1;\n"
              "    phase83_uart_data = 0;\n"
              "    phase83_uart_count = 0;\n"
              "    phase83_uart_log[0] = 0;\n"
              "    phase83_uart_log[1] = 0;\n"
              "    phase83_uart_log[2] = 0;\n"
              "    phase83_uart_log[3] = 0;\n"
              "}\n"
              "\n"
              "void phase83_uart_commit(void) {\n"
              "    if (phase83_uart_count < 4) {\n"
              "        phase83_uart_log[phase83_uart_count] = (uint8_t)(phase83_uart_data & 0xffu);\n"
              "    }\n"
              "    phase83_uart_count += 1;\n"
              "    phase83_uart_status = 1;\n"
              "}\n"
              "\n"
              "int phase83_uart_result(void) {\n"
              "    if (phase83_uart_count != 2) {\n"
              "        return (int)phase83_uart_count;\n"
              "    }\n"
              "    if (phase83_uart_log[0] != 'O') {\n"
              "        return 10;\n"
              "    }\n"
              "    if (phase83_uart_log[1] != 'K') {\n"
              "        return 11;\n"
              "    }\n"
              "    return 83;\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase83-hal-console\"\n"
              "default = \"console\"\n"
              "\n"
              "[targets.console]\n"
              "kind = \"exe\"\n"
              "package = \"phase83\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.console.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.console.runtime]\n"
              "startup = \"bootstrap_main\"\n"
              "\n"
              "[targets.console.link]\n"
              "inputs = [\"target/phase83_uart_support.o\"]\n");

    const std::filesystem::path support_object = project_root / "target/phase83_uart_support.o";
    const auto [support_compile_outcome, support_compile_output] = RunCommandCapture({"xcrun",
                                                                                      "clang",
                                                                                      "-target",
                                                                                      std::string(mc::kBootstrapTriple),
                                                                                      "-c",
                                                                                      (project_root / "target/phase83_uart_support.c").generic_string(),
                                                                                      "-o",
                                                                                      support_object.generic_string()},
                                                                                     project_root / "target/uart_support_compile_output.txt",
                                                                                     "uart support object compile");
    if (!support_compile_outcome.exited || support_compile_outcome.exit_code != 0) {
        Fail("phase83 uart support object compile should pass:\n" + support_compile_output);
    }

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "hal_console_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "console",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "hal_console_build_output.txt",
                                                                 "freestanding hal console build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase83 freestanding hal console build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "hal_console_run_output.txt",
                                                             "freestanding hal console run");
    if (!run_outcome.exited || run_outcome.exit_code != 83) {
        Fail("phase83 freestanding hal console run should exit with the console proof marker:\n" + run_output);
    }

    const std::string console_mir = ReadFile(dump_targets.mir);
    const std::string console_llvm_ir = ReadFile(build_targets.llvm_ir);
    ExpectOutputContains(console_mir,
                         "Function name=uart_write_byte",
                         "phase83 freestanding hal MIR should include the polling helper");
    ExpectOutputContains(console_mir,
                         "Block label=loop_cond",
                         "phase83 freestanding hal MIR should preserve the polling loop shape");
    ExpectOutputContains(console_mir,
                         "int_to_pointer %v",
                         "phase83 freestanding hal MIR should lower hal.mmio_ptr to int_to_pointer");
    ExpectOutputContains(console_mir,
                         "volatile_load %v",
                         "phase83 freestanding hal MIR should lower status reads to volatile_load");
    ExpectOutputContains(console_mir,
                         "volatile_store target=hal.volatile_store",
                         "phase83 freestanding hal MIR should lower data writes to volatile_store");
    ExpectOutputContains(console_llvm_ir,
                         "inttoptr i64",
                         "phase83 freestanding hal LLVM IR should preserve the MMIO pointer conversion");
    ExpectOutputContains(console_llvm_ir,
                         "load volatile i32",
                         "phase83 freestanding hal LLVM IR should preserve volatile status loads");
    ExpectOutputContains(console_llvm_ir,
                         "store volatile i32",
                         "phase83 freestanding hal LLVM IR should preserve volatile data stores");

}

void TestPhase138FreestandingFatalPrimitive(const std::filesystem::path& source_root,
                                            const std::filesystem::path& binary_root,
                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "freestanding_panic_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase138-freestanding\"\n"
              "default = \"boot\"\n"
              "\n"
              "[targets.boot]\n"
              "kind = \"exe\"\n"
              "package = \"phase138\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.boot.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.boot.runtime]\n"
              "startup = \"bootstrap_main\"\n");
    WriteFile(project_root / "src/main.mc",
              "func bootstrap_main() i32 {\n"
              "    ok: bool = false\n"
              "    if !ok {\n"
              "        panic(138)\n"
              "    }\n"
              "    return 0\n"
              "}\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "freestanding_panic_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "boot",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "freestanding_panic_build_output.txt",
                                                                 "freestanding panic build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase138 freestanding panic build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const std::string panic_mir = ReadFile(dump_targets.mir);
    const std::string panic_llvm_ir = ReadFile(build_targets.llvm_ir);
    ExpectOutputContains(panic_mir,
                         "terminator panic value=",
                         "phase138 freestanding MIR should preserve the explicit panic terminator");
    ExpectOutputContains(panic_llvm_ir,
                         "define private void @__mc_panic(i64 %code)",
                         "phase138 freestanding LLVM IR should emit the shared panic helper");
    ExpectOutputContains(panic_llvm_ir,
                         "call void @__mc_panic(i64 138)",
                         "phase138 freestanding LLVM IR should preserve the fatal fault code");
    if (panic_llvm_ir.find("call void @exit(i32") != std::string::npos) {
        Fail("phase138 freestanding panic helper should not route through hosted exit support");
    }
}

}  // namespace

namespace mc::tool_tests {

void RunFreestandingBootstrapToolSuite(const std::filesystem::path& source_root,
                                       const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    TestPhase81FreestandingBootstrapAndHal(source_root, binary_root, mc_path);
    TestPhase82FreestandingArtifactAndEntryHardening(source_root, binary_root, mc_path);
    TestPhase83NarrowHalAdmissionAndConsoleProof(source_root, binary_root, mc_path);
    TestPhase138FreestandingFatalPrimitive(source_root, binary_root, mc_path);
}

}  // namespace mc::tool_tests
