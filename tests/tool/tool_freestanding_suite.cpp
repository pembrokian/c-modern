#include <filesystem>
#include <string>

#include "compiler/support/dump_paths.h"
#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::CopyDirectoryTree;
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

void TestPhase85KernelEndpointQueueSmoke(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "endpoint_queue_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc",
              "enum MessageTag {\n"
              "    Empty,\n"
              "    Ping,\n"
              "    Ack,\n"
              "}\n"
              "\n"
              "struct EndpointMessage {\n"
              "    tag: MessageTag\n"
              "    sender: u32\n"
              "    len: usize\n"
              "    payload: [4]u8\n"
              "}\n"
              "\n"
              "struct EndpointQueue {\n"
              "    head: usize\n"
              "    tail: usize\n"
              "    count: usize\n"
              "    slot0: EndpointMessage\n"
              "    slot1: EndpointMessage\n"
              "}\n"
              "\n"
              "func zero_payload() [4]u8 {\n"
              "    payload: [4]u8\n"
              "    payload[0] = 0\n"
              "    payload[1] = 0\n"
              "    payload[2] = 0\n"
              "    payload[3] = 0\n"
              "    return payload\n"
              "}\n"
              "\n"
              "func empty_message() EndpointMessage {\n"
              "    return EndpointMessage{ tag: MessageTag.Empty, sender: 0, len: 0, payload: zero_payload() }\n"
              "}\n"
              "\n"
              "func ping_message(sender: u32) EndpointMessage {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = 79\n"
              "    payload[1] = 75\n"
              "    return EndpointMessage{ tag: MessageTag.Ping, sender: sender, len: 2, payload: payload }\n"
              "}\n"
              "\n"
              "func ack_message(sender: u32) EndpointMessage {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = 33\n"
              "    return EndpointMessage{ tag: MessageTag.Ack, sender: sender, len: 1, payload: payload }\n"
              "}\n"
              "\n"
              "func init_queue() EndpointQueue {\n"
              "    return EndpointQueue{ head: 0, tail: 0, count: 0, slot0: empty_message(), slot1: empty_message() }\n"
              "}\n"
              "\n"
              "func wrap_slot(index: usize) usize {\n"
              "    next: usize = index + 1\n"
              "    if next == 2 {\n"
              "        return 0\n"
              "    }\n"
              "    return next\n"
              "}\n"
              "\n"
              "func enqueue(queue: *EndpointQueue, message: EndpointMessage) bool {\n"
              "    current: EndpointQueue = *queue\n"
              "    if current.count == 2 {\n"
              "        return false\n"
              "    }\n"
              "    head: usize = current.head\n"
              "    tail: usize = current.tail\n"
              "    count: usize = current.count\n"
              "    slot0: EndpointMessage = current.slot0\n"
              "    slot1: EndpointMessage = current.slot1\n"
              "    if tail == 0 {\n"
              "        slot0 = message\n"
              "    } else {\n"
              "        slot1 = message\n"
              "    }\n"
              "    tail = wrap_slot(tail)\n"
              "    count = count + 1\n"
              "    *queue = EndpointQueue{ head: head, tail: tail, count: count, slot0: slot0, slot1: slot1 }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func dequeue(queue: *EndpointQueue, out: *EndpointMessage) bool {\n"
              "    current: EndpointQueue = *queue\n"
              "    if current.count == 0 {\n"
              "        return false\n"
              "    }\n"
              "    head: usize = current.head\n"
              "    tail: usize = current.tail\n"
              "    count: usize = current.count\n"
              "    slot0: EndpointMessage = current.slot0\n"
              "    slot1: EndpointMessage = current.slot1\n"
              "    message: EndpointMessage\n"
              "    if head == 0 {\n"
              "        message = slot0\n"
              "        slot0 = empty_message()\n"
              "    } else {\n"
              "        message = slot1\n"
              "        slot1 = empty_message()\n"
              "    }\n"
              "    head = wrap_slot(head)\n"
              "    count = count - 1\n"
              "    *queue = EndpointQueue{ head: head, tail: tail, count: count, slot0: slot0, slot1: slot1 }\n"
              "    *out = message\n"
              "    return true\n"
              "}\n"
              "\n"
              "func tag_score(tag: MessageTag) i32 {\n"
              "    switch tag {\n"
              "    case MessageTag.Empty:\n"
              "        return 1\n"
              "    case MessageTag.Ping:\n"
              "        return 2\n"
              "    case MessageTag.Ack:\n"
              "        return 4\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    queue: EndpointQueue = init_queue()\n"
              "    if queue.count != 0 {\n"
              "        return 10\n"
              "    }\n"
              "    if !enqueue(&queue, ping_message(7)) {\n"
              "        return 11\n"
              "    }\n"
              "    if !enqueue(&queue, ack_message(9)) {\n"
              "        return 12\n"
              "    }\n"
              "    if enqueue(&queue, ping_message(10)) {\n"
              "        return 13\n"
              "    }\n"
              "\n"
              "    first: EndpointMessage = empty_message()\n"
              "    second: EndpointMessage = empty_message()\n"
              "    if !dequeue(&queue, &first) {\n"
              "        return 14\n"
              "    }\n"
              "    if !dequeue(&queue, &second) {\n"
              "        return 15\n"
              "    }\n"
              "    if dequeue(&queue, &first) {\n"
              "        return 16\n"
              "    }\n"
              "\n"
              "    if tag_score(first.tag) != 2 {\n"
              "        return 17\n"
              "    }\n"
              "    if first.sender != 7 {\n"
              "        return 18\n"
              "    }\n"
              "    if first.len != 2 {\n"
              "        return 19\n"
              "    }\n"
              "    if first.payload[0] != 79 {\n"
              "        return 20\n"
              "    }\n"
              "    if first.payload[1] != 75 {\n"
              "        return 21\n"
              "    }\n"
              "    if tag_score(second.tag) != 4 {\n"
              "        return 22\n"
              "    }\n"
              "    if second.sender != 9 {\n"
              "        return 23\n"
              "    }\n"
              "    if second.len != 1 {\n"
              "        return 24\n"
              "    }\n"
              "    if second.payload[0] != 33 {\n"
              "        return 25\n"
              "    }\n"
              "    if queue.count != 0 {\n"
              "        return 26\n"
              "    }\n"
              "    if queue.head != 0 {\n"
              "        return 27\n"
              "    }\n"
              "    if queue.tail != 0 {\n"
              "        return 28\n"
              "    }\n"
              "    return 85\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase85-endpoint-queue\"\n"
              "default = \"queue\"\n"
              "\n"
              "[targets.queue]\n"
              "kind = \"exe\"\n"
              "package = \"phase85\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.queue.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.queue.runtime]\n"
              "startup = \"bootstrap_main\"\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "endpoint_queue_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "queue",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "endpoint_queue_build_output.txt",
                                                                 "freestanding endpoint queue build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase85 freestanding endpoint queue build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "endpoint_queue_run_output.txt",
                                                             "freestanding endpoint queue run");
    if (!run_outcome.exited || run_outcome.exit_code != 85) {
        Fail("phase85 freestanding endpoint queue run should exit with the queue proof marker:\n" + run_output);
    }

    const std::string queue_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(queue_mir,
                         "TypeDecl kind=enum name=MessageTag",
                         "phase85 endpoint queue MIR should declare the message tag enum");
    ExpectOutputContains(queue_mir,
                         "TypeDecl kind=struct name=EndpointMessage",
                         "phase85 endpoint queue MIR should declare the message struct");
    ExpectOutputContains(queue_mir,
                         "TypeDecl kind=struct name=EndpointQueue",
                         "phase85 endpoint queue MIR should declare the endpoint queue struct");
    ExpectOutputContains(queue_mir,
                         "aggregate_init %v",
                         "phase85 endpoint queue MIR should use aggregate initialization for queue-owned message state");
    ExpectOutputContains(queue_mir,
                         "target_base=[4]u8",
                         "phase85 endpoint queue MIR should still model fixed payload storage through ordinary indexed array operations");
}

void TestPhase86TaskLifecycleKernelProof(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "task_lifecycle_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc",
              "enum TaskState {\n"
              "    Empty,\n"
              "    Created,\n"
              "    Ready,\n"
              "    Running,\n"
              "    Exited,\n"
              "    Reaped,\n"
              "}\n"
              "\n"
              "struct Task {\n"
              "    pid: u32\n"
              "    parent: u32\n"
              "    state: TaskState\n"
              "    exit_code: i32\n"
              "}\n"
              "\n"
              "struct Kernel {\n"
              "    next_pid: u32\n"
              "    running: u32\n"
              "    slot0: Task\n"
              "    slot1: Task\n"
              "}\n"
              "\n"
              "func empty_task() Task {\n"
              "    return Task{ pid: 0, parent: 0, state: TaskState.Empty, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func init_task() Task {\n"
              "    return Task{ pid: 1, parent: 0, state: TaskState.Running, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func init_kernel() Kernel {\n"
              "    return Kernel{ next_pid: 2, running: 1, slot0: init_task(), slot1: empty_task() }\n"
              "}\n"
              "\n"
              "func created_task(pid: u32, parent: u32) Task {\n"
              "    return Task{ pid: pid, parent: parent, state: TaskState.Created, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func with_state(task: Task, state: TaskState) Task {\n"
              "    return Task{ pid: task.pid, parent: task.parent, state: state, exit_code: task.exit_code }\n"
              "}\n"
              "\n"
              "func with_exit(task: Task, state: TaskState, exit_code: i32) Task {\n"
              "    return Task{ pid: task.pid, parent: task.parent, state: state, exit_code: exit_code }\n"
              "}\n"
              "\n"
              "func create_child(kernel: *Kernel, parent_pid: u32) u32 {\n"
              "    current: Kernel = *kernel\n"
              "    if state_score(current.slot1.state) != 1 {\n"
              "        return 0\n"
              "    }\n"
              "    child_pid: u32 = current.next_pid\n"
              "    child: Task = created_task(child_pid, parent_pid)\n"
              "    *kernel = Kernel{ next_pid: child_pid + 1, running: current.running, slot0: current.slot0, slot1: child }\n"
              "    return child_pid\n"
              "}\n"
              "\n"
              "func mark_ready(kernel: *Kernel, child_pid: u32) bool {\n"
              "    current: Kernel = *kernel\n"
              "    child: Task = current.slot1\n"
              "    if child.pid != child_pid {\n"
              "        return false\n"
              "    }\n"
              "    if state_score(child.state) != 2 {\n"
              "        return false\n"
              "    }\n"
              "    child = with_state(child, TaskState.Ready)\n"
              "    *kernel = Kernel{ next_pid: current.next_pid, running: current.running, slot0: current.slot0, slot1: child }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func dispatch_child(kernel: *Kernel, child_pid: u32) bool {\n"
              "    current: Kernel = *kernel\n"
              "    parent: Task = current.slot0\n"
              "    child: Task = current.slot1\n"
              "    if current.running != parent.pid {\n"
              "        return false\n"
              "    }\n"
              "    if child.pid != child_pid {\n"
              "        return false\n"
              "    }\n"
              "    if state_score(child.state) != 4 {\n"
              "        return false\n"
              "    }\n"
              "    parent = with_state(parent, TaskState.Ready)\n"
              "    child = with_state(child, TaskState.Running)\n"
              "    *kernel = Kernel{ next_pid: current.next_pid, running: child.pid, slot0: parent, slot1: child }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func exit_running(kernel: *Kernel, exit_code: i32) bool {\n"
              "    current: Kernel = *kernel\n"
              "    parent: Task = current.slot0\n"
              "    child: Task = current.slot1\n"
              "    if current.running != child.pid {\n"
              "        return false\n"
              "    }\n"
              "    if state_score(child.state) != 8 {\n"
              "        return false\n"
              "    }\n"
              "    parent = with_state(parent, TaskState.Running)\n"
              "    child = with_exit(child, TaskState.Exited, exit_code)\n"
              "    *kernel = Kernel{ next_pid: current.next_pid, running: parent.pid, slot0: parent, slot1: child }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func wait_child(kernel: *Kernel, parent_pid: u32, child_pid: u32, out: *i32) bool {\n"
              "    current: Kernel = *kernel\n"
              "    child: Task = current.slot1\n"
              "    if current.running != parent_pid {\n"
              "        return false\n"
              "    }\n"
              "    if child.pid != child_pid {\n"
              "        return false\n"
              "    }\n"
              "    if child.parent != parent_pid {\n"
              "        return false\n"
              "    }\n"
              "    if state_score(child.state) != 16 {\n"
              "        return false\n"
              "    }\n"
              "    *out = child.exit_code\n"
              "    child = with_state(child, TaskState.Reaped)\n"
              "    *kernel = Kernel{ next_pid: current.next_pid, running: current.running, slot0: current.slot0, slot1: child }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func state_score(state: TaskState) i32 {\n"
              "    switch state {\n"
              "    case TaskState.Empty:\n"
              "        return 1\n"
              "    case TaskState.Created:\n"
              "        return 2\n"
              "    case TaskState.Ready:\n"
              "        return 4\n"
              "    case TaskState.Running:\n"
              "        return 8\n"
              "    case TaskState.Exited:\n"
              "        return 16\n"
              "    case TaskState.Reaped:\n"
              "        return 32\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    kernel: Kernel = init_kernel()\n"
              "    if kernel.running != 1 {\n"
              "        return 10\n"
              "    }\n"
              "    child_pid: u32 = create_child(&kernel, 1)\n"
              "    if child_pid != 2 {\n"
              "        return 11\n"
              "    }\n"
              "    if state_score(kernel.slot1.state) != 2 {\n"
              "        return 12\n"
              "    }\n"
              "    status: i32 = 0\n"
              "    if wait_child(&kernel, 1, child_pid, &status) {\n"
              "        return 13\n"
              "    }\n"
              "    if !mark_ready(&kernel, child_pid) {\n"
              "        return 14\n"
              "    }\n"
              "    if state_score(kernel.slot1.state) != 4 {\n"
              "        return 15\n"
              "    }\n"
              "    if !dispatch_child(&kernel, child_pid) {\n"
              "        return 16\n"
              "    }\n"
              "    if kernel.running != child_pid {\n"
              "        return 17\n"
              "    }\n"
              "    if state_score(kernel.slot0.state) != 4 {\n"
              "        return 18\n"
              "    }\n"
              "    if state_score(kernel.slot1.state) != 8 {\n"
              "        return 19\n"
              "    }\n"
              "    if !exit_running(&kernel, 23) {\n"
              "        return 20\n"
              "    }\n"
              "    if kernel.running != 1 {\n"
              "        return 21\n"
              "    }\n"
              "    if state_score(kernel.slot0.state) != 8 {\n"
              "        return 22\n"
              "    }\n"
              "    if state_score(kernel.slot1.state) != 16 {\n"
              "        return 23\n"
              "    }\n"
              "    if !wait_child(&kernel, 1, child_pid, &status) {\n"
              "        return 24\n"
              "    }\n"
              "    if status != 23 {\n"
              "        return 25\n"
              "    }\n"
              "    if state_score(kernel.slot1.state) != 32 {\n"
              "        return 26\n"
              "    }\n"
              "    if kernel.next_pid != 3 {\n"
              "        return 27\n"
              "    }\n"
              "    return 86\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase86-task-lifecycle\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase86\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "task_lifecycle_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "task_lifecycle_build_output.txt",
                                                                 "freestanding task lifecycle build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase86 freestanding task lifecycle build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "task_lifecycle_run_output.txt",
                                                             "freestanding task lifecycle run");
    if (!run_outcome.exited || run_outcome.exit_code != 86) {
        Fail("phase86 freestanding task lifecycle run should exit with the lifecycle proof marker:\n" + run_output);
    }

    const std::string lifecycle_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(lifecycle_mir,
                         "TypeDecl kind=enum name=TaskState",
                         "phase86 task lifecycle MIR should declare the task state enum");
    ExpectOutputContains(lifecycle_mir,
                         "TypeDecl kind=struct name=Task",
                         "phase86 task lifecycle MIR should declare the task struct");
    ExpectOutputContains(lifecycle_mir,
                         "TypeDecl kind=struct name=Kernel",
                         "phase86 task lifecycle MIR should declare the kernel struct");
    ExpectOutputContains(lifecycle_mir,
                         "aggregate_init %v",
                         "phase86 task lifecycle MIR should use aggregate initialization for task state updates");
    ExpectOutputContains(lifecycle_mir,
                         "Function name=state_score returns=[i32]",
                         "phase86 task lifecycle MIR should keep lifecycle state classification in an ordinary helper");
}

void TestPhase87KernelStaticDataAndArtifactFollowThrough(const std::filesystem::path& source_root,
                                                         const std::filesystem::path& binary_root,
                                                         const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "kernel_static_data_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc",
              "const BOOT_PID: u32 = 1\n"
              "const CHILD_PID: u32 = 2\n"
              "const CHILD_EXIT_CODE: i32 = 23\n"
              "\n"
              "enum TaskState {\n"
              "    Empty,\n"
              "    Created,\n"
              "    Ready,\n"
              "    Running,\n"
              "    Exited,\n"
              "    Reaped,\n"
              "}\n"
              "\n"
              "struct Task {\n"
              "    pid: u32\n"
              "    parent: u32\n"
              "    state: TaskState\n"
              "    exit_code: i32\n"
              "}\n"
              "\n"
              "const EMPTY_TASK_TEMPLATE: Task = Task{ pid: 0, parent: 0, state: TaskState.Empty, exit_code: 0 }\n"
              "const BOOT_TASK_TEMPLATE: Task = Task{ pid: BOOT_PID, parent: 0, state: TaskState.Running, exit_code: 0 }\n"
              "\n"
              "var ACTIVE_TASK: Task\n"
              "var CHILD_TASK: Task\n"
              "var WAIT_STATUS: i32\n"
              "var WAIT_READY: i32\n"
              "\n"
              "func with_state(task: Task, state: TaskState) Task {\n"
              "    return Task{ pid: task.pid, parent: task.parent, state: state, exit_code: task.exit_code }\n"
              "}\n"
              "\n"
              "func with_exit(task: Task, state: TaskState, exit_code: i32) Task {\n"
              "    return Task{ pid: task.pid, parent: task.parent, state: state, exit_code: exit_code }\n"
              "}\n"
              "\n"
              "func state_score(state: TaskState) i32 {\n"
              "    switch state {\n"
              "    case TaskState.Empty:\n"
              "        return 1\n"
              "    case TaskState.Created:\n"
              "        return 2\n"
              "    case TaskState.Ready:\n"
              "        return 4\n"
              "    case TaskState.Running:\n"
              "        return 8\n"
              "    case TaskState.Exited:\n"
              "        return 16\n"
              "    case TaskState.Reaped:\n"
              "        return 32\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func store_i32(ptr: *i32, value: i32) {\n"
              "    *ptr = value\n"
              "}\n"
              "\n"
              "func seed_kernel() {\n"
              "    ACTIVE_TASK = BOOT_TASK_TEMPLATE\n"
              "    CHILD_TASK = EMPTY_TASK_TEMPLATE\n"
              "    WAIT_STATUS = 0\n"
              "    WAIT_READY = 0\n"
              "}\n"
              "\n"
              "func create_child() {\n"
              "    CHILD_TASK = Task{ pid: CHILD_PID, parent: ACTIVE_TASK.pid, state: TaskState.Created, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func mark_ready() {\n"
              "    CHILD_TASK = with_state(CHILD_TASK, TaskState.Ready)\n"
              "}\n"
              "\n"
              "func dispatch_child() {\n"
              "    ACTIVE_TASK = with_state(ACTIVE_TASK, TaskState.Ready)\n"
              "    CHILD_TASK = with_state(CHILD_TASK, TaskState.Running)\n"
              "}\n"
              "\n"
              "func exit_child() {\n"
              "    ACTIVE_TASK = with_state(ACTIVE_TASK, TaskState.Running)\n"
              "    CHILD_TASK = with_exit(CHILD_TASK, TaskState.Exited, CHILD_EXIT_CODE)\n"
              "    store_i32(&WAIT_STATUS, CHILD_TASK.exit_code)\n"
              "    WAIT_READY = 1\n"
              "}\n"
              "\n"
              "func wait_child() bool {\n"
              "    if state_score(CHILD_TASK.state) != 16 {\n"
              "        return false\n"
              "    }\n"
              "    if WAIT_READY != 1 {\n"
              "        return false\n"
              "    }\n"
              "    CHILD_TASK = with_state(CHILD_TASK, TaskState.Reaped)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    if state_score(ACTIVE_TASK.state) != 1 {\n"
              "        return 10\n"
              "    }\n"
              "    if state_score(CHILD_TASK.state) != 1 {\n"
              "        return 11\n"
              "    }\n"
              "    if WAIT_STATUS != 0 {\n"
              "        return 12\n"
              "    }\n"
              "    if WAIT_READY != 0 {\n"
              "        return 13\n"
              "    }\n"
              "\n"
              "    seed_kernel()\n"
              "    if ACTIVE_TASK.pid != BOOT_PID {\n"
              "        return 14\n"
              "    }\n"
              "    if state_score(ACTIVE_TASK.state) != 8 {\n"
              "        return 15\n"
              "    }\n"
              "    create_child()\n"
              "    if CHILD_TASK.pid != CHILD_PID {\n"
              "        return 16\n"
              "    }\n"
              "    if CHILD_TASK.parent != BOOT_PID {\n"
              "        return 17\n"
              "    }\n"
              "    if state_score(CHILD_TASK.state) != 2 {\n"
              "        return 18\n"
              "    }\n"
              "    if wait_child() {\n"
              "        return 19\n"
              "    }\n"
              "    mark_ready()\n"
              "    if state_score(CHILD_TASK.state) != 4 {\n"
              "        return 20\n"
              "    }\n"
              "    dispatch_child()\n"
              "    if state_score(ACTIVE_TASK.state) != 4 {\n"
              "        return 21\n"
              "    }\n"
              "    if state_score(CHILD_TASK.state) != 8 {\n"
              "        return 22\n"
              "    }\n"
              "    exit_child()\n"
              "    if state_score(ACTIVE_TASK.state) != 8 {\n"
              "        return 23\n"
              "    }\n"
              "    if state_score(CHILD_TASK.state) != 16 {\n"
              "        return 24\n"
              "    }\n"
              "    if WAIT_STATUS != CHILD_EXIT_CODE {\n"
              "        return 25\n"
              "    }\n"
              "    if WAIT_READY != 1 {\n"
              "        return 26\n"
              "    }\n"
              "    if !wait_child() {\n"
              "        return 27\n"
              "    }\n"
              "    if state_score(CHILD_TASK.state) != 32 {\n"
              "        return 28\n"
              "    }\n"
              "    return 87\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase87-kernel-static-data\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase87\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "kernel_static_data_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_static_data_build_output.txt",
                                                                 "freestanding kernel static data build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase87 freestanding kernel static data build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    if (!std::filesystem::exists(build_targets.object)) {
        Fail("phase87 freestanding kernel static data build should emit an object artifact");
    }
    if (!std::filesystem::exists(build_targets.executable)) {
        Fail("phase87 freestanding kernel static data build should emit an executable artifact");
    }

    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_static_data_run_output.txt",
                                                             "freestanding kernel static data run");
    if (!run_outcome.exited || run_outcome.exit_code != 87) {
        Fail("phase87 freestanding kernel static data run should exit with the static-data proof marker:\n" + run_output);
    }

    const std::string static_data_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(static_data_mir,
                         "ConstGlobal names=[BOOT_TASK_TEMPLATE] type=Task",
                         "phase87 kernel static-data MIR should retain the boot task aggregate const global");
    ExpectOutputContains(static_data_mir,
                         "ConstGlobal names=[EMPTY_TASK_TEMPLATE] type=Task",
                         "phase87 kernel static-data MIR should retain the empty task aggregate const global");
    ExpectOutputContains(static_data_mir,
                         "VarGlobal names=[ACTIVE_TASK] type=Task",
                         "phase87 kernel static-data MIR should retain the mutable active-task global");
    ExpectOutputContains(static_data_mir,
                         "VarGlobal names=[CHILD_TASK] type=Task",
                         "phase87 kernel static-data MIR should retain the mutable child-task global");
    ExpectOutputContains(static_data_mir,
                         "VarGlobal names=[WAIT_STATUS] type=i32",
                         "phase87 kernel static-data MIR should retain the wait-status global");
    ExpectOutputContains(static_data_mir,
                         "store_target target=ACTIVE_TASK target_kind=global target_name=ACTIVE_TASK",
                         "phase87 kernel static-data MIR should lower global task writes as global targets");
    ExpectOutputContains(static_data_mir,
                         "target=WAIT_STATUS target_kind=global target_name=WAIT_STATUS",
                         "phase87 kernel static-data MIR should preserve explicit global-address lowering for wait-status storage");
}

void TestPhase88KernelBuildIntegrationAudit(const std::filesystem::path& source_root,
                                            const std::filesystem::path& binary_root,
                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "kernel_build_integration_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc",
              "extern(c) func phase88_kernel_delta() i32\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    return 80 + phase88_kernel_delta()\n"
              "}\n");
    WriteFile(project_root / "target/phase88_driver_support.c",
              "int phase88_kernel_delta(void) {\n"
              "    return 8;\n"
              "}\n");

    const std::filesystem::path driver_support_object = project_root / "target/phase88_driver_support.o";
    const auto [driver_support_compile_outcome, driver_support_compile_output] = RunCommandCapture({"xcrun",
                                                                                                    "clang",
                                                                                                    "-target",
                                                                                                    std::string(mc::kBootstrapTriple),
                                                                                                    "-c",
                                                                                                    (project_root / "target/phase88_driver_support.c").generic_string(),
                                                                                                    "-o",
                                                                                                    driver_support_object.generic_string()},
                                                                                                   project_root / "target/phase88_driver_support_compile_output.txt",
                                                                                                   "phase88 driver support object compile");
    if (!driver_support_compile_outcome.exited || driver_support_compile_outcome.exit_code != 0) {
        Fail("phase88 driver support object compile should pass:\n" + driver_support_compile_output);
    }

    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase88-kernel-build-integration\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase88\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n"
              "\n"
              "[targets.kernel.link]\n"
              "inputs = [\"target/phase88_driver_support.o\"]\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "kernel_build_integration_build";
    std::filesystem::remove_all(build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "kernel",
                                       "phase88_kernel_build_output.txt",
                                       "phase88 freestanding kernel build integration build");

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const std::filesystem::path runtime_object =
        build_targets.object.parent_path() /
        (build_targets.object.stem().generic_string() + ".freestanding.bootstrap_main.runtime.o");
    if (!std::filesystem::exists(build_targets.object)) {
        Fail("phase88 freestanding kernel build integration build should emit an entry object artifact");
    }
    if (!std::filesystem::exists(runtime_object)) {
        Fail("phase88 freestanding kernel build integration build should emit a freestanding runtime object artifact");
    }
    if (!std::filesystem::exists(build_targets.executable)) {
        Fail("phase88 freestanding kernel build integration build should emit an executable artifact");
    }

    const auto [driver_run_outcome, driver_run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                                           build_dir / "phase88_driver_link_run_output.txt",
                                                                           "phase88 freestanding driver-linked kernel run");
    if (!driver_run_outcome.exited || driver_run_outcome.exit_code != 88) {
        Fail("phase88 freestanding driver-linked kernel run should exit with the build-integration proof marker:\n" +
             driver_run_output);
    }

    WriteFile(project_root / "target/phase88_manual_support.c",
              "int phase88_kernel_delta(void) {\n"
              "    return 9;\n"
              "}\n");
    const std::filesystem::path manual_support_object = project_root / "target/phase88_manual_support.o";
    const auto [manual_support_compile_outcome, manual_support_compile_output] = RunCommandCapture({"xcrun",
                                                                                                    "clang",
                                                                                                    "-target",
                                                                                                    std::string(mc::kBootstrapTriple),
                                                                                                    "-c",
                                                                                                    (project_root / "target/phase88_manual_support.c").generic_string(),
                                                                                                    "-o",
                                                                                                    manual_support_object.generic_string()},
                                                                                                   project_root / "target/phase88_manual_support_compile_output.txt",
                                                                                                   "phase88 manual support object compile");
    if (!manual_support_compile_outcome.exited || manual_support_compile_outcome.exit_code != 0) {
        Fail("phase88 manual support object compile should pass:\n" + manual_support_compile_output);
    }

    const std::filesystem::path manual_executable = build_dir / "bin" / "phase88_manual_kernel";
    const auto [manual_link_outcome, manual_link_output] = RunCommandCapture({"xcrun",
                                                                              "clang",
                                                                              "-target",
                                                                              std::string(mc::kBootstrapTriple),
                                                                              build_targets.object.generic_string(),
                                                                              manual_support_object.generic_string(),
                                                                              runtime_object.generic_string(),
                                                                              "-o",
                                                                              manual_executable.generic_string()},
                                                                             build_dir / "phase88_manual_link_output.txt",
                                                                             "phase88 manual freestanding kernel link");
    if (!manual_link_outcome.exited || manual_link_outcome.exit_code != 0) {
        Fail("phase88 manual freestanding kernel link should succeed:\n" + manual_link_output);
    }

    const auto [manual_run_outcome, manual_run_output] = RunCommandCapture({manual_executable.generic_string()},
                                                                           build_dir / "phase88_manual_link_run_output.txt",
                                                                           "phase88 manual freestanding kernel run");
    if (!manual_run_outcome.exited || manual_run_outcome.exit_code != 89) {
        Fail("phase88 manual freestanding kernel run should exit with the relinked proof marker:\n" +
             manual_run_output);
    }
}

void TestPhase89InitToLogServiceHandshake(const std::filesystem::path& source_root,
                                          const std::filesystem::path& binary_root,
                                          const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "init_log_service_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc",
              "const INIT_PID: u32 = 1\n"
              "const LOG_PID: u32 = 2\n"
              "const LOG_ENDPOINT_ID: u32 = 7\n"
              "const CAP_RIGHT_SEND: u32 = 1\n"
              "const ACK_BYTE: u8 = 64\n"
              "\n"
              "enum MessageTag {\n"
              "    Empty,\n"
              "    LogOpen,\n"
              "    Ack,\n"
              "}\n"
              "\n"
              "struct EndpointCapability {\n"
              "    endpoint_id: u32\n"
              "    service_pid: u32\n"
              "    rights: u32\n"
              "}\n"
              "\n"
              "struct Message {\n"
              "    tag: MessageTag\n"
              "    sender: u32\n"
              "    recipient: u32\n"
              "    len: usize\n"
              "    payload: [4]u8\n"
              "}\n"
              "\n"
              "struct InitContext {\n"
              "    pid: u32\n"
              "    log_cap: EndpointCapability\n"
              "}\n"
              "\n"
              "struct LogService {\n"
              "    pid: u32\n"
              "    endpoint_id: u32\n"
              "    requests: u32\n"
              "    last_sender: u32\n"
              "    last_len: usize\n"
              "    last_byte: u8\n"
              "}\n"
              "\n"
              "var KERNEL_NEXT_PID: u32\n"
              "var INIT_CONTEXT: InitContext\n"
              "var LOG_SERVICE_STATE: LogService\n"
              "\n"
              "func zero_payload() [4]u8 {\n"
              "    payload: [4]u8\n"
              "    payload[0] = 0\n"
              "    payload[1] = 0\n"
              "    payload[2] = 0\n"
              "    payload[3] = 0\n"
              "    return payload\n"
              "}\n"
              "\n"
              "func empty_message() Message {\n"
              "    return Message{ tag: MessageTag.Empty, sender: 0, recipient: 0, len: 0, payload: zero_payload() }\n"
              "}\n"
              "\n"
              "func tag_score(tag: MessageTag) i32 {\n"
              "    switch tag {\n"
              "    case MessageTag.Empty:\n"
              "        return 1\n"
              "    case MessageTag.LogOpen:\n"
              "        return 2\n"
              "    case MessageTag.Ack:\n"
              "        return 4\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func kernel_boot() {\n"
              "    log_cap: EndpointCapability = EndpointCapability{ endpoint_id: LOG_ENDPOINT_ID, service_pid: LOG_PID, rights: CAP_RIGHT_SEND }\n"
              "    INIT_CONTEXT = InitContext{ pid: INIT_PID, log_cap: log_cap }\n"
              "    LOG_SERVICE_STATE = LogService{ pid: LOG_PID, endpoint_id: LOG_ENDPOINT_ID, requests: 0, last_sender: 0, last_len: 0, last_byte: 0 }\n"
              "    KERNEL_NEXT_PID = 3\n"
              "}\n"
              "\n"
              "func log_open_message(sender: u32, recipient: u32) Message {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = 73\n"
              "    payload[1] = 78\n"
              "    payload[2] = 73\n"
              "    return Message{ tag: MessageTag.LogOpen, sender: sender, recipient: recipient, len: 3, payload: payload }\n"
              "}\n"
              "\n"
              "func ack_message(sender: u32, recipient: u32) Message {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = ACK_BYTE\n"
              "    return Message{ tag: MessageTag.Ack, sender: sender, recipient: recipient, len: 1, payload: payload }\n"
              "}\n"
              "\n"
              "func cap_matches_log_service(cap: EndpointCapability) bool {\n"
              "    if cap.endpoint_id != LOG_SERVICE_STATE.endpoint_id {\n"
              "        return false\n"
              "    }\n"
              "    if cap.service_pid != LOG_SERVICE_STATE.pid {\n"
              "        return false\n"
              "    }\n"
              "    if cap.rights != CAP_RIGHT_SEND {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func request_is_log_open(request: Message) bool {\n"
              "    if request.recipient != LOG_SERVICE_STATE.pid {\n"
              "        return false\n"
              "    }\n"
              "    if tag_score(request.tag) != 2 {\n"
              "        return false\n"
              "    }\n"
              "    if request.len != 3 {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func log_service_call(request: Message) Message {\n"
              "    service: LogService = LOG_SERVICE_STATE\n"
              "    service = LogService{ pid: service.pid, endpoint_id: service.endpoint_id, requests: service.requests + 1, last_sender: request.sender, last_len: request.len, last_byte: request.payload[2] }\n"
              "    LOG_SERVICE_STATE = service\n"
              "    return ack_message(service.pid, request.sender)\n"
              "}\n"
              "\n"
              "func init_main(ctx: InitContext) Message {\n"
              "    if ctx.pid != INIT_PID {\n"
              "        return empty_message()\n"
              "    }\n"
              "    request: Message = log_open_message(ctx.pid, ctx.log_cap.service_pid)\n"
              "    if !cap_matches_log_service(ctx.log_cap) {\n"
              "        return empty_message()\n"
              "    }\n"
              "    if !request_is_log_open(request) {\n"
              "        return empty_message()\n"
              "    }\n"
              "    return log_service_call(request)\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    kernel_boot()\n"
              "    if KERNEL_NEXT_PID != 3 {\n"
              "        return 10\n"
              "    }\n"
              "    if INIT_CONTEXT.pid != INIT_PID {\n"
              "        return 11\n"
              "    }\n"
              "    if INIT_CONTEXT.log_cap.endpoint_id != LOG_ENDPOINT_ID {\n"
              "        return 12\n"
              "    }\n"
              "    if INIT_CONTEXT.log_cap.service_pid != LOG_PID {\n"
              "        return 13\n"
              "    }\n"
              "    if INIT_CONTEXT.log_cap.rights != CAP_RIGHT_SEND {\n"
              "        return 14\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.requests != 0 {\n"
              "        return 15\n"
              "    }\n"
              "    reply: Message = init_main(INIT_CONTEXT)\n"
              "    if tag_score(reply.tag) != 4 {\n"
              "        return 16\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.requests != 1 {\n"
              "        return 17\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.last_sender != INIT_PID {\n"
              "        return 18\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.last_len != 3 {\n"
              "        return 19\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.last_byte != 73 {\n"
              "        return 20\n"
              "    }\n"
              "    if tag_score(reply.tag) != 4 {\n"
              "        return 21\n"
              "    }\n"
              "    if reply.sender != LOG_PID {\n"
              "        return 22\n"
              "    }\n"
              "    if reply.recipient != INIT_PID {\n"
              "        return 23\n"
              "    }\n"
              "    if reply.len != 1 {\n"
              "        return 24\n"
              "    }\n"
              "    if reply.payload[0] != ACK_BYTE {\n"
              "        return 25\n"
              "    }\n"
              "    return 89\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase89-init-log-service\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase89\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "init_log_service_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "init_log_service_build_output.txt",
                                                                 "freestanding init to log-service handshake build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase89 freestanding init-to-log-service build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "init_log_service_run_output.txt",
                                                             "freestanding init to log-service handshake run");
    if (!run_outcome.exited || run_outcome.exit_code != 89) {
        Fail("phase89 freestanding init-to-log-service run should exit with the handshake proof marker:\n" +
             run_output);
    }

    const std::string handshake_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(handshake_mir,
                         "TypeDecl kind=struct name=EndpointCapability",
                         "phase89 handshake MIR should declare the endpoint capability struct");
    ExpectOutputContains(handshake_mir,
                         "TypeDecl kind=struct name=InitContext",
                         "phase89 handshake MIR should declare the init context struct");
    ExpectOutputContains(handshake_mir,
                         "TypeDecl kind=struct name=Message",
                         "phase89 handshake MIR should declare the message struct");
    ExpectOutputContains(handshake_mir,
                         "Function name=init_main returns=[Message]",
                         "phase89 handshake MIR should keep init startup as an ordinary function boundary with an explicit reply value");
    ExpectOutputContains(handshake_mir,
                         "aggregate_init %v",
                         "phase89 handshake MIR should use aggregate initialization for capability and message state");
    ExpectOutputContains(handshake_mir,
                         "target_base=[4]u8",
                         "phase89 handshake MIR should preserve fixed payload storage through ordinary indexed array operations");
}

void TestPhase90CapabilityHandleTransferProof(const std::filesystem::path& source_root,
                                              const std::filesystem::path& binary_root,
                                              const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "capability_transfer_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc",
              "const INIT_PID: u32 = 1\n"
              "const LOG_PID: u32 = 2\n"
              "const WORKER_PID: u32 = 3\n"
              "const LOG_ENDPOINT_ID: u32 = 7\n"
              "const CAP_RIGHT_SEND: u32 = 1\n"
              "const WORKER_LOG_SLOT: u32 = 5\n"
              "const GRANT_BYTE: u8 = 71\n"
              "const LOG_BYTE: u8 = 90\n"
              "const ACK_BYTE: u8 = 33\n"
              "\n"
              "enum MessageTag {\n"
              "    Empty,\n"
              "    GrantHandle,\n"
              "    LogWrite,\n"
              "    Ack,\n"
              "}\n"
              "\n"
              "struct EndpointCapability {\n"
              "    endpoint_id: u32\n"
              "    service_pid: u32\n"
              "    rights: u32\n"
              "}\n"
              "\n"
              "struct Message {\n"
              "    tag: MessageTag\n"
              "    sender: u32\n"
              "    recipient: u32\n"
              "    handle_slot: u32\n"
              "    len: usize\n"
              "    payload: [4]u8\n"
              "}\n"
              "\n"
              "struct TransferMessage {\n"
              "    tag: MessageTag\n"
              "    sender: u32\n"
              "    recipient: u32\n"
              "    handle_slot: u32\n"
              "    handle_count: usize\n"
              "    transferred_cap: EndpointCapability\n"
              "    payload_len: usize\n"
              "    payload: [4]u8\n"
              "}\n"
              "\n"
              "struct InitContext {\n"
              "    pid: u32\n"
              "    has_log_cap: u32\n"
              "    log_cap: EndpointCapability\n"
              "    worker_pid: u32\n"
              "}\n"
              "\n"
              "struct WorkerContext {\n"
              "    pid: u32\n"
              "    has_log_cap: u32\n"
              "    log_cap: EndpointCapability\n"
              "    received_slot: u32\n"
              "}\n"
              "\n"
              "struct LogService {\n"
              "    pid: u32\n"
              "    endpoint_id: u32\n"
              "    requests: u32\n"
              "    last_sender: u32\n"
              "    last_slot: u32\n"
              "    last_byte: u8\n"
              "}\n"
              "\n"
              "var INIT_CONTEXT: InitContext\n"
              "var WORKER_CONTEXT: WorkerContext\n"
              "var LOG_SERVICE_STATE: LogService\n"
              "\n"
              "func zero_payload() [4]u8 {\n"
              "    payload: [4]u8\n"
              "    payload[0] = 0\n"
              "    payload[1] = 0\n"
              "    payload[2] = 0\n"
              "    payload[3] = 0\n"
              "    return payload\n"
              "}\n"
              "\n"
              "func empty_cap() EndpointCapability {\n"
              "    return EndpointCapability{ endpoint_id: 0, service_pid: 0, rights: 0 }\n"
              "}\n"
              "\n"
              "func empty_message() Message {\n"
              "    return Message{ tag: MessageTag.Empty, sender: 0, recipient: 0, handle_slot: 0, len: 0, payload: zero_payload() }\n"
              "}\n"
              "\n"
              "func empty_transfer_message() TransferMessage {\n"
              "    return TransferMessage{ tag: MessageTag.Empty, sender: 0, recipient: 0, handle_slot: 0, handle_count: 0, transferred_cap: empty_cap(), payload_len: 0, payload: zero_payload() }\n"
              "}\n"
              "\n"
              "func tag_score(tag: MessageTag) i32 {\n"
              "    switch tag {\n"
              "    case MessageTag.Empty:\n"
              "        return 1\n"
              "    case MessageTag.GrantHandle:\n"
              "        return 2\n"
              "    case MessageTag.LogWrite:\n"
              "        return 4\n"
              "    case MessageTag.Ack:\n"
              "        return 8\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func cap_matches_log_service(cap: EndpointCapability) bool {\n"
              "    if cap.endpoint_id != LOG_SERVICE_STATE.endpoint_id {\n"
              "        return false\n"
              "    }\n"
              "    if cap.service_pid != LOG_SERVICE_STATE.pid {\n"
              "        return false\n"
              "    }\n"
              "    if cap.rights != CAP_RIGHT_SEND {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func kernel_boot() {\n"
              "    grant_cap: EndpointCapability = EndpointCapability{ endpoint_id: LOG_ENDPOINT_ID, service_pid: LOG_PID, rights: CAP_RIGHT_SEND }\n"
              "    INIT_CONTEXT = InitContext{ pid: INIT_PID, has_log_cap: 1, log_cap: grant_cap, worker_pid: WORKER_PID }\n"
              "    WORKER_CONTEXT = WorkerContext{ pid: WORKER_PID, has_log_cap: 0, log_cap: empty_cap(), received_slot: 0 }\n"
              "    LOG_SERVICE_STATE = LogService{ pid: LOG_PID, endpoint_id: LOG_ENDPOINT_ID, requests: 0, last_sender: 0, last_slot: 0, last_byte: 0 }\n"
              "}\n"
              "\n"
              "func build_grant_message(sender: u32, recipient: u32, cap: EndpointCapability) TransferMessage {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = GRANT_BYTE\n"
              "    payload[1] = 82\n"
              "    payload[2] = 78\n"
              "    return TransferMessage{ tag: MessageTag.GrantHandle, sender: sender, recipient: recipient, handle_slot: WORKER_LOG_SLOT, handle_count: 1, transferred_cap: cap, payload_len: 3, payload: payload }\n"
              "}\n"
              "\n"
              "func build_log_request(sender: u32, recipient: u32, handle_slot: u32) Message {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = LOG_BYTE\n"
              "    payload[1] = 0\n"
              "    payload[2] = 0\n"
              "    return Message{ tag: MessageTag.LogWrite, sender: sender, recipient: recipient, handle_slot: handle_slot, len: 1, payload: payload }\n"
              "}\n"
              "\n"
              "func build_ack(sender: u32, recipient: u32, handle_slot: u32) Message {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = ACK_BYTE\n"
              "    return Message{ tag: MessageTag.Ack, sender: sender, recipient: recipient, handle_slot: handle_slot, len: 1, payload: payload }\n"
              "}\n"
              "\n"
              "func transfer_log_cap() TransferMessage {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    if ctx.has_log_cap == 0 {\n"
              "        return empty_transfer_message()\n"
              "    }\n"
              "    if !cap_matches_log_service(ctx.log_cap) {\n"
              "        return empty_transfer_message()\n"
              "    }\n"
              "    message: TransferMessage = build_grant_message(ctx.pid, ctx.worker_pid, ctx.log_cap)\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_log_cap: 0, log_cap: empty_cap(), worker_pid: ctx.worker_pid }\n"
              "    return message\n"
              "}\n"
              "\n"
              "func install_transferred_cap(message: TransferMessage) bool {\n"
              "    ctx: WorkerContext = WORKER_CONTEXT\n"
              "    if tag_score(message.tag) != 2 {\n"
              "        return false\n"
              "    }\n"
              "    if message.recipient != ctx.pid {\n"
              "        return false\n"
              "    }\n"
              "    if message.handle_count != 1 {\n"
              "        return false\n"
              "    }\n"
              "    if message.payload_len != 3 {\n"
              "        return false\n"
              "    }\n"
              "    if message.payload[0] != GRANT_BYTE {\n"
              "        return false\n"
              "    }\n"
              "    if !cap_matches_log_service(message.transferred_cap) {\n"
              "        return false\n"
              "    }\n"
              "    WORKER_CONTEXT = WorkerContext{ pid: ctx.pid, has_log_cap: 1, log_cap: message.transferred_cap, received_slot: message.handle_slot }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func log_service_call(cap: EndpointCapability, request: Message) Message {\n"
              "    if !cap_matches_log_service(cap) {\n"
              "        return empty_message()\n"
              "    }\n"
              "    if request.recipient != LOG_SERVICE_STATE.pid {\n"
              "        return empty_message()\n"
              "    }\n"
              "    if tag_score(request.tag) != 4 {\n"
              "        return empty_message()\n"
              "    }\n"
              "    if request.handle_slot != WORKER_LOG_SLOT {\n"
              "        return empty_message()\n"
              "    }\n"
              "    if request.payload[0] != LOG_BYTE {\n"
              "        return empty_message()\n"
              "    }\n"
              "    service: LogService = LOG_SERVICE_STATE\n"
              "    service = LogService{ pid: service.pid, endpoint_id: service.endpoint_id, requests: service.requests + 1, last_sender: request.sender, last_slot: request.handle_slot, last_byte: request.payload[0] }\n"
              "    LOG_SERVICE_STATE = service\n"
              "    return build_ack(service.pid, request.sender, request.handle_slot)\n"
              "}\n"
              "\n"
              "func worker_log_once(ctx: WorkerContext) Message {\n"
              "    if ctx.has_log_cap == 0 {\n"
              "        return empty_message()\n"
              "    }\n"
              "    request: Message = build_log_request(ctx.pid, ctx.log_cap.service_pid, ctx.received_slot)\n"
              "    return log_service_call(ctx.log_cap, request)\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    kernel_boot()\n"
              "    if INIT_CONTEXT.has_log_cap != 1 {\n"
              "        return 10\n"
              "    }\n"
              "    if WORKER_CONTEXT.has_log_cap != 0 {\n"
              "        return 11\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.requests != 0 {\n"
              "        return 12\n"
              "    }\n"
              "    pre_transfer_reply: Message = worker_log_once(WORKER_CONTEXT)\n"
              "    if tag_score(pre_transfer_reply.tag) != 1 {\n"
              "        return 13\n"
              "    }\n"
              "    grant: TransferMessage = transfer_log_cap()\n"
              "    if tag_score(grant.tag) != 2 {\n"
              "        return 14\n"
              "    }\n"
              "    if INIT_CONTEXT.has_log_cap != 0 {\n"
              "        return 15\n"
              "    }\n"
              "    if INIT_CONTEXT.log_cap.endpoint_id != 0 {\n"
              "        return 16\n"
              "    }\n"
              "    second_grant: TransferMessage = transfer_log_cap()\n"
              "    if tag_score(second_grant.tag) != 1 {\n"
              "        return 17\n"
              "    }\n"
              "    if !install_transferred_cap(grant) {\n"
              "        return 18\n"
              "    }\n"
              "    if WORKER_CONTEXT.has_log_cap != 1 {\n"
              "        return 19\n"
              "    }\n"
              "    if WORKER_CONTEXT.received_slot != WORKER_LOG_SLOT {\n"
              "        return 20\n"
              "    }\n"
              "    post_transfer_init_reply: Message = worker_log_once(WorkerContext{ pid: INIT_CONTEXT.pid, has_log_cap: INIT_CONTEXT.has_log_cap, log_cap: INIT_CONTEXT.log_cap, received_slot: WORKER_LOG_SLOT })\n"
              "    if tag_score(post_transfer_init_reply.tag) != 1 {\n"
              "        return 21\n"
              "    }\n"
              "    reply: Message = worker_log_once(WORKER_CONTEXT)\n"
              "    if tag_score(reply.tag) != 8 {\n"
              "        return 22\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.requests != 1 {\n"
              "        return 23\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.last_sender != WORKER_PID {\n"
              "        return 24\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.last_slot != WORKER_LOG_SLOT {\n"
              "        return 25\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.last_byte != LOG_BYTE {\n"
              "        return 26\n"
              "    }\n"
              "    if reply.sender != LOG_PID {\n"
              "        return 27\n"
              "    }\n"
              "    if reply.recipient != WORKER_PID {\n"
              "        return 28\n"
              "    }\n"
              "    if reply.handle_slot != WORKER_LOG_SLOT {\n"
              "        return 29\n"
              "    }\n"
              "    if reply.payload[0] != ACK_BYTE {\n"
              "        return 30\n"
              "    }\n"
              "    return 90\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase90-capability-transfer\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase90\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "capability_transfer_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "capability_transfer_build_output.txt",
                                                                 "freestanding capability-handle transfer build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase90 freestanding capability-handle transfer build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "capability_transfer_run_output.txt",
                                                             "freestanding capability-handle transfer run");
    if (!run_outcome.exited || run_outcome.exit_code != 90) {
        Fail("phase90 freestanding capability-handle transfer run should exit with the transfer proof marker:\n" +
             run_output);
    }

    const std::string transfer_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(transfer_mir,
                         "TypeDecl kind=struct name=TransferMessage",
                         "phase90 transfer MIR should declare the transfer message struct");
    ExpectOutputContains(transfer_mir,
                         "TypeDecl kind=struct name=WorkerContext",
                         "phase90 transfer MIR should declare the receiver context struct");
    ExpectOutputContains(transfer_mir,
                         "Function name=install_transferred_cap returns=[bool]",
                         "phase90 transfer MIR should keep receiver-side handle installation as an ordinary function boundary");
    ExpectOutputContains(transfer_mir,
                         "Function name=transfer_log_cap returns=[TransferMessage]",
                         "phase90 transfer MIR should keep sender-side transfer as an ordinary function boundary");
    ExpectOutputContains(transfer_mir,
                         "aggregate_init %v",
                         "phase90 transfer MIR should use aggregate initialization for capability and transfer state");
    ExpectOutputContains(transfer_mir,
                         "target_base=[4]u8",
                         "phase90 transfer MIR should preserve fixed payload storage through ordinary indexed array operations");
}

void TestPhase91EarlyUserSpaceHelperBoundaryAudit(const std::filesystem::path& source_root,
                                                  const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "user_space_helper_boundary_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/user_helpers.mc",
              "const CAP_RIGHT_SEND: u32 = 1\n"
              "const DEFAULT_SLOT: u32 = 5\n"
              "const GRANT_BYTE: u8 = 71\n"
              "const LOG_BYTE: u8 = 90\n"
              "const ACK_BYTE: u8 = 33\n"
              "\n"
              "enum MessageTag {\n"
              "    Empty,\n"
              "    GrantHandle,\n"
              "    LogWrite,\n"
              "    Ack,\n"
              "}\n"
              "\n"
              "struct EndpointCapability {\n"
              "    endpoint_id: u32\n"
              "    service_pid: u32\n"
              "    rights: u32\n"
              "}\n"
              "\n"
              "struct Message {\n"
              "    tag: MessageTag\n"
              "    sender: u32\n"
              "    recipient: u32\n"
              "    handle_slot: u32\n"
              "    len: usize\n"
              "    payload: [4]u8\n"
              "}\n"
              "\n"
              "struct TransferMessage {\n"
              "    tag: MessageTag\n"
              "    sender: u32\n"
              "    recipient: u32\n"
              "    handle_slot: u32\n"
              "    handle_count: usize\n"
              "    transferred_cap: EndpointCapability\n"
              "    payload_len: usize\n"
              "    payload: [4]u8\n"
              "}\n"
              "\n"
              "struct ClientBinding {\n"
              "    owner_pid: u32\n"
              "    has_log_cap: u32\n"
              "    received_slot: u32\n"
              "    log_cap: EndpointCapability\n"
              "}\n"
              "\n"
              "func zero_payload() [4]u8 {\n"
              "    payload: [4]u8\n"
              "    payload[0] = 0\n"
              "    payload[1] = 0\n"
              "    payload[2] = 0\n"
              "    payload[3] = 0\n"
              "    return payload\n"
              "}\n"
              "\n"
              "func empty_cap() EndpointCapability {\n"
              "    return EndpointCapability{ endpoint_id: 0, service_pid: 0, rights: 0 }\n"
              "}\n"
              "\n"
              "func empty_message() Message {\n"
              "    return Message{ tag: MessageTag.Empty, sender: 0, recipient: 0, handle_slot: 0, len: 0, payload: zero_payload() }\n"
              "}\n"
              "\n"
              "func empty_transfer_message() TransferMessage {\n"
              "    return TransferMessage{ tag: MessageTag.Empty, sender: 0, recipient: 0, handle_slot: 0, handle_count: 0, transferred_cap: empty_cap(), payload_len: 0, payload: zero_payload() }\n"
              "}\n"
              "\n"
              "func empty_binding(owner_pid: u32) ClientBinding {\n"
              "    return ClientBinding{ owner_pid: owner_pid, has_log_cap: 0, received_slot: 0, log_cap: empty_cap() }\n"
              "}\n"
              "\n"
              "func bind_log_cap(owner_pid: u32, slot: u32, cap: EndpointCapability) ClientBinding {\n"
              "    return ClientBinding{ owner_pid: owner_pid, has_log_cap: 1, received_slot: slot, log_cap: cap }\n"
              "}\n"
              "\n"
              "func tag_score(tag: MessageTag) i32 {\n"
              "    switch tag {\n"
              "    case MessageTag.Empty:\n"
              "        return 1\n"
              "    case MessageTag.GrantHandle:\n"
              "        return 2\n"
              "    case MessageTag.LogWrite:\n"
              "        return 4\n"
              "    case MessageTag.Ack:\n"
              "        return 8\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func cap_matches_service(cap: EndpointCapability, service_pid: u32, endpoint_id: u32) bool {\n"
              "    if cap.endpoint_id != endpoint_id {\n"
              "        return false\n"
              "    }\n"
              "    if cap.service_pid != service_pid {\n"
              "        return false\n"
              "    }\n"
              "    if cap.rights != CAP_RIGHT_SEND {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func make_transfer_message(sender: u32, recipient: u32, slot: u32, cap: EndpointCapability) TransferMessage {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = GRANT_BYTE\n"
              "    payload[1] = 82\n"
              "    payload[2] = 78\n"
              "    return TransferMessage{ tag: MessageTag.GrantHandle, sender: sender, recipient: recipient, handle_slot: slot, handle_count: 1, transferred_cap: cap, payload_len: 3, payload: payload }\n"
              "}\n"
              "\n"
              "func install_received_handle(recipient_pid: u32, message: TransferMessage, service_pid: u32, endpoint_id: u32) ClientBinding {\n"
              "    if tag_score(message.tag) != 2 {\n"
              "        return empty_binding(recipient_pid)\n"
              "    }\n"
              "    if message.recipient != recipient_pid {\n"
              "        return empty_binding(recipient_pid)\n"
              "    }\n"
              "    if message.handle_count != 1 {\n"
              "        return empty_binding(recipient_pid)\n"
              "    }\n"
              "    if message.payload_len != 3 {\n"
              "        return empty_binding(recipient_pid)\n"
              "    }\n"
              "    if message.payload[0] != GRANT_BYTE {\n"
              "        return empty_binding(recipient_pid)\n"
              "    }\n"
              "    if !cap_matches_service(message.transferred_cap, service_pid, endpoint_id) {\n"
              "        return empty_binding(recipient_pid)\n"
              "    }\n"
              "    return bind_log_cap(recipient_pid, message.handle_slot, message.transferred_cap)\n"
              "}\n"
              "\n"
              "func make_log_request(sender: u32, binding: ClientBinding) Message {\n"
              "    if binding.has_log_cap == 0 {\n"
              "        return empty_message()\n"
              "    }\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = LOG_BYTE\n"
              "    return Message{ tag: MessageTag.LogWrite, sender: sender, recipient: binding.log_cap.service_pid, handle_slot: binding.received_slot, len: 1, payload: payload }\n"
              "}\n"
              "\n"
              "func make_ack(sender: u32, recipient: u32, slot: u32) Message {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = ACK_BYTE\n"
              "    return Message{ tag: MessageTag.Ack, sender: sender, recipient: recipient, handle_slot: slot, len: 1, payload: payload }\n"
              "}\n"
              "\n"
              "func ack_is_valid(reply: Message, sender: u32, recipient: u32, slot: u32) bool {\n"
              "    if tag_score(reply.tag) != 8 {\n"
              "        return false\n"
              "    }\n"
              "    if reply.sender != sender {\n"
              "        return false\n"
              "    }\n"
              "    if reply.recipient != recipient {\n"
              "        return false\n"
              "    }\n"
              "    if reply.handle_slot != slot {\n"
              "        return false\n"
              "    }\n"
              "    if reply.len != 1 {\n"
              "        return false\n"
              "    }\n"
              "    if reply.payload[0] != ACK_BYTE {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n");
    WriteFile(project_root / "src/main.mc",
              "import user_helpers\n"
              "\n"
              "const INIT_PID: u32 = 1\n"
              "const LOG_PID: u32 = 2\n"
              "const WORKER_PID: u32 = 3\n"
              "const LOG_ENDPOINT_ID: u32 = 7\n"
              "\n"
              "struct LogService {\n"
              "    pid: u32\n"
              "    endpoint_id: u32\n"
              "    requests: u32\n"
              "    last_sender: u32\n"
              "    last_slot: u32\n"
              "    last_byte: u8\n"
              "}\n"
              "\n"
              "var INIT_BINDING: user_helpers.ClientBinding\n"
              "var WORKER_BINDING: user_helpers.ClientBinding\n"
              "var LOG_SERVICE_STATE: LogService\n"
              "\n"
              "func kernel_boot() {\n"
              "    grant_cap: user_helpers.EndpointCapability = user_helpers.EndpointCapability{ endpoint_id: LOG_ENDPOINT_ID, service_pid: LOG_PID, rights: user_helpers.CAP_RIGHT_SEND }\n"
              "    INIT_BINDING = user_helpers.bind_log_cap(INIT_PID, user_helpers.DEFAULT_SLOT, grant_cap)\n"
              "    WORKER_BINDING = user_helpers.empty_binding(WORKER_PID)\n"
              "    LOG_SERVICE_STATE = LogService{ pid: LOG_PID, endpoint_id: LOG_ENDPOINT_ID, requests: 0, last_sender: 0, last_slot: 0, last_byte: 0 }\n"
              "}\n"
              "\n"
              "func transfer_log_cap() user_helpers.TransferMessage {\n"
              "    binding: user_helpers.ClientBinding = INIT_BINDING\n"
              "    if binding.has_log_cap == 0 {\n"
              "        return user_helpers.empty_transfer_message()\n"
              "    }\n"
              "    if !user_helpers.cap_matches_service(binding.log_cap, LOG_PID, LOG_ENDPOINT_ID) {\n"
              "        return user_helpers.empty_transfer_message()\n"
              "    }\n"
              "    message: user_helpers.TransferMessage = user_helpers.make_transfer_message(binding.owner_pid, WORKER_PID, binding.received_slot, binding.log_cap)\n"
              "    INIT_BINDING = user_helpers.empty_binding(binding.owner_pid)\n"
              "    return message\n"
              "}\n"
              "\n"
              "func log_service_call(cap: user_helpers.EndpointCapability, request: user_helpers.Message) user_helpers.Message {\n"
              "    if !user_helpers.cap_matches_service(cap, LOG_SERVICE_STATE.pid, LOG_SERVICE_STATE.endpoint_id) {\n"
              "        return user_helpers.empty_message()\n"
              "    }\n"
              "    if request.recipient != LOG_SERVICE_STATE.pid {\n"
              "        return user_helpers.empty_message()\n"
              "    }\n"
              "    if user_helpers.tag_score(request.tag) != 4 {\n"
              "        return user_helpers.empty_message()\n"
              "    }\n"
              "    if request.handle_slot != user_helpers.DEFAULT_SLOT {\n"
              "        return user_helpers.empty_message()\n"
              "    }\n"
              "    if request.payload[0] != user_helpers.LOG_BYTE {\n"
              "        return user_helpers.empty_message()\n"
              "    }\n"
              "    service: LogService = LOG_SERVICE_STATE\n"
              "    service = LogService{ pid: service.pid, endpoint_id: service.endpoint_id, requests: service.requests + 1, last_sender: request.sender, last_slot: request.handle_slot, last_byte: request.payload[0] }\n"
              "    LOG_SERVICE_STATE = service\n"
              "    return user_helpers.make_ack(service.pid, request.sender, request.handle_slot)\n"
              "}\n"
              "\n"
              "func worker_log_once(binding: user_helpers.ClientBinding) user_helpers.Message {\n"
              "    request: user_helpers.Message = user_helpers.make_log_request(binding.owner_pid, binding)\n"
              "    if user_helpers.tag_score(request.tag) != 4 {\n"
              "        return user_helpers.empty_message()\n"
              "    }\n"
              "    return log_service_call(binding.log_cap, request)\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    kernel_boot()\n"
              "    if INIT_BINDING.has_log_cap != 1 {\n"
              "        return 10\n"
              "    }\n"
              "    if WORKER_BINDING.has_log_cap != 0 {\n"
              "        return 11\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.requests != 0 {\n"
              "        return 12\n"
              "    }\n"
              "    pre_transfer_reply: user_helpers.Message = worker_log_once(WORKER_BINDING)\n"
              "    if user_helpers.tag_score(pre_transfer_reply.tag) != 1 {\n"
              "        return 13\n"
              "    }\n"
              "    grant: user_helpers.TransferMessage = transfer_log_cap()\n"
              "    if user_helpers.tag_score(grant.tag) != 2 {\n"
              "        return 14\n"
              "    }\n"
              "    if INIT_BINDING.has_log_cap != 0 {\n"
              "        return 15\n"
              "    }\n"
              "    second_grant: user_helpers.TransferMessage = transfer_log_cap()\n"
              "    if user_helpers.tag_score(second_grant.tag) != 1 {\n"
              "        return 16\n"
              "    }\n"
              "    wrong_install: user_helpers.ClientBinding = user_helpers.install_received_handle(INIT_PID, grant, LOG_PID, LOG_ENDPOINT_ID)\n"
              "    if wrong_install.has_log_cap != 0 {\n"
              "        return 17\n"
              "    }\n"
              "    WORKER_BINDING = user_helpers.install_received_handle(WORKER_PID, grant, LOG_PID, LOG_ENDPOINT_ID)\n"
              "    if WORKER_BINDING.has_log_cap != 1 {\n"
              "        return 18\n"
              "    }\n"
              "    if WORKER_BINDING.received_slot != user_helpers.DEFAULT_SLOT {\n"
              "        return 19\n"
              "    }\n"
              "    post_transfer_init_reply: user_helpers.Message = worker_log_once(INIT_BINDING)\n"
              "    if user_helpers.tag_score(post_transfer_init_reply.tag) != 1 {\n"
              "        return 20\n"
              "    }\n"
              "    reply: user_helpers.Message = worker_log_once(WORKER_BINDING)\n"
              "    if !user_helpers.ack_is_valid(reply, LOG_PID, WORKER_PID, user_helpers.DEFAULT_SLOT) {\n"
              "        return 21\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.requests != 1 {\n"
              "        return 22\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.last_sender != WORKER_PID {\n"
              "        return 23\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.last_slot != user_helpers.DEFAULT_SLOT {\n"
              "        return 24\n"
              "    }\n"
              "    if LOG_SERVICE_STATE.last_byte != user_helpers.LOG_BYTE {\n"
              "        return 25\n"
              "    }\n"
              "    return 91\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase91-user-space-helpers\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase91\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "user_space_helper_boundary_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "user_space_helper_boundary_build_output.txt",
                                                                 "freestanding user-space helper boundary build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase91 freestanding user-space helper boundary build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto main_dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "user_space_helper_boundary_run_output.txt",
                                                             "freestanding user-space helper boundary run");
    if (!run_outcome.exited || run_outcome.exit_code != 91) {
        Fail("phase91 freestanding user-space helper boundary run should exit with the helper-boundary proof marker:\n" +
             run_output);
    }

    const std::string main_mir = ReadFile(main_dump_targets.mir);
    ExpectOutputContains(main_mir,
                         "TypeDecl kind=struct name=user_helpers.ClientBinding",
                         "phase91 main MIR should preserve the imported helper binding type");
    ExpectOutputContains(main_mir,
                         "Function name=transfer_log_cap",
                         "phase91 main MIR should keep destructive transfer policy in the main proof module");
    ExpectOutputContains(main_mir,
                         "Function name=user_helpers.install_received_handle returns=[user_helpers.ClientBinding]",
                         "phase91 merged MIR should keep handle installation inside an ordinary helper function boundary");
    ExpectOutputContains(main_mir,
                         "Function name=user_helpers.make_log_request returns=[user_helpers.Message]",
                         "phase91 merged MIR should keep request construction inside an ordinary helper function boundary");
    ExpectOutputContains(main_mir,
                         "Function name=user_helpers.make_transfer_message returns=[user_helpers.TransferMessage]",
                         "phase91 merged MIR should keep helper-owned transfer construction as an ordinary imported function");
    ExpectOutputContains(main_mir,
                         "aggregate_init %v",
                         "phase91 merged MIR should use aggregate initialization for helper-owned transfer and binding state");
    ExpectOutputContains(main_mir,
                         "target_base=[4]u8",
                         "phase91 merged MIR should preserve fixed payload storage through ordinary indexed array operations");
}

void TestPhase92UserSpaceLifecycleFollowThrough(const std::filesystem::path& source_root,
                                                const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "user_space_lifecycle_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/user_lifecycle.mc",
              "const CAP_RIGHT_LAUNCH: u32 = 1\n"
              "\n"
              "enum ProcessState {\n"
              "    Empty,\n"
              "    Created,\n"
              "    Ready,\n"
              "    Running,\n"
              "    Exited,\n"
              "    Observed,\n"
              "}\n"
              "\n"
              "struct ProgramCapability {\n"
              "    image_id: u32\n"
              "    owner_pid: u32\n"
              "    rights: u32\n"
              "}\n"
              "\n"
              "struct WaitHandle {\n"
              "    parent_pid: u32\n"
              "    child_pid: u32\n"
              "    active: u32\n"
              "}\n"
              "\n"
              "struct ChildProcess {\n"
              "    pid: u32\n"
              "    parent_pid: u32\n"
              "    image_id: u32\n"
              "    state: ProcessState\n"
              "    ready_seen: u32\n"
              "    exit_code: i32\n"
              "}\n"
              "\n"
              "struct WaitObservation {\n"
              "    ok: u32\n"
              "    child_pid: u32\n"
              "    exit_code: i32\n"
              "}\n"
              "\n"
              "func empty_program_cap() ProgramCapability {\n"
              "    return ProgramCapability{ image_id: 0, owner_pid: 0, rights: 0 }\n"
              "}\n"
              "\n"
              "func launchable_program_cap(owner_pid: u32, image_id: u32) ProgramCapability {\n"
              "    return ProgramCapability{ image_id: image_id, owner_pid: owner_pid, rights: CAP_RIGHT_LAUNCH }\n"
              "}\n"
              "\n"
              "func program_is_launchable(cap: ProgramCapability, owner_pid: u32, image_id: u32) bool {\n"
              "    if cap.image_id != image_id {\n"
              "        return false\n"
              "    }\n"
              "    if cap.owner_pid != owner_pid {\n"
              "        return false\n"
              "    }\n"
              "    if cap.rights != CAP_RIGHT_LAUNCH {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func empty_wait_handle() WaitHandle {\n"
              "    return WaitHandle{ parent_pid: 0, child_pid: 0, active: 0 }\n"
              "}\n"
              "\n"
              "func active_wait_handle(parent_pid: u32, child_pid: u32) WaitHandle {\n"
              "    return WaitHandle{ parent_pid: parent_pid, child_pid: child_pid, active: 1 }\n"
              "}\n"
              "\n"
              "func empty_child() ChildProcess {\n"
              "    return ChildProcess{ pid: 0, parent_pid: 0, image_id: 0, state: ProcessState.Empty, ready_seen: 0, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func created_child(parent_pid: u32, child_pid: u32, image_id: u32) ChildProcess {\n"
              "    return ChildProcess{ pid: child_pid, parent_pid: parent_pid, image_id: image_id, state: ProcessState.Created, ready_seen: 0, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func mark_ready(child: ChildProcess) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Ready, ready_seen: 1, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func mark_running(child: ChildProcess) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Running, ready_seen: child.ready_seen, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func mark_exited(child: ChildProcess, exit_code: i32) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Exited, ready_seen: child.ready_seen, exit_code: exit_code }\n"
              "}\n"
              "\n"
              "func mark_observed(child: ChildProcess) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Observed, ready_seen: child.ready_seen, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func empty_observation() WaitObservation {\n"
              "    return WaitObservation{ ok: 0, child_pid: 0, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func observe_exit(handle: WaitHandle, child: ChildProcess) WaitObservation {\n"
              "    if handle.active == 0 {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    if handle.parent_pid != child.parent_pid {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    if handle.child_pid != child.pid {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    if state_score(child.state) != 16 {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    return WaitObservation{ ok: 1, child_pid: child.pid, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func state_score(state: ProcessState) i32 {\n"
              "    switch state {\n"
              "    case ProcessState.Empty:\n"
              "        return 1\n"
              "    case ProcessState.Created:\n"
              "        return 2\n"
              "    case ProcessState.Ready:\n"
              "        return 4\n"
              "    case ProcessState.Running:\n"
              "        return 8\n"
              "    case ProcessState.Exited:\n"
              "        return 16\n"
              "    case ProcessState.Observed:\n"
              "        return 32\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n");
    WriteFile(project_root / "src/main.mc",
              "import user_lifecycle\n"
              "\n"
              "const INIT_PID: u32 = 1\n"
              "const CHILD_PID: u32 = 2\n"
              "const CHILD_IMAGE_ID: u32 = 77\n"
              "const CHILD_EXIT_CODE: i32 = 29\n"
              "\n"
              "struct InitContext {\n"
              "    pid: u32\n"
              "    has_program_cap: u32\n"
              "    program_cap: user_lifecycle.ProgramCapability\n"
              "    has_wait_handle: u32\n"
              "    wait_handle: user_lifecycle.WaitHandle\n"
              "    child_ready: u32\n"
              "    observed_exit: i32\n"
              "}\n"
              "\n"
              "var INIT_CONTEXT: InitContext\n"
              "var CHILD_CONTEXT: user_lifecycle.ChildProcess\n"
              "\n"
              "func kernel_boot() {\n"
              "    cap: user_lifecycle.ProgramCapability = user_lifecycle.launchable_program_cap(INIT_PID, CHILD_IMAGE_ID)\n"
              "    INIT_CONTEXT = InitContext{ pid: INIT_PID, has_program_cap: 1, program_cap: cap, has_wait_handle: 0, wait_handle: user_lifecycle.empty_wait_handle(), child_ready: 0, observed_exit: 0 }\n"
              "    CHILD_CONTEXT = user_lifecycle.empty_child()\n"
              "}\n"
              "\n"
              "func launch_child() bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    if ctx.has_program_cap == 0 {\n"
              "        return false\n"
              "    }\n"
              "    if !user_lifecycle.program_is_launchable(ctx.program_cap, ctx.pid, CHILD_IMAGE_ID) {\n"
              "        return false\n"
              "    }\n"
              "    CHILD_CONTEXT = user_lifecycle.created_child(ctx.pid, CHILD_PID, ctx.program_cap.image_id)\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_program_cap: 0, program_cap: user_lifecycle.empty_program_cap(), has_wait_handle: 1, wait_handle: user_lifecycle.active_wait_handle(ctx.pid, CHILD_PID), child_ready: 0, observed_exit: 0 }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func child_report_ready() bool {\n"
              "    child: user_lifecycle.ChildProcess = CHILD_CONTEXT\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    if user_lifecycle.state_score(child.state) != 2 {\n"
              "        return false\n"
              "    }\n"
              "    CHILD_CONTEXT = user_lifecycle.mark_ready(child)\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_program_cap: ctx.has_program_cap, program_cap: ctx.program_cap, has_wait_handle: ctx.has_wait_handle, wait_handle: ctx.wait_handle, child_ready: 1, observed_exit: ctx.observed_exit }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func dispatch_child() bool {\n"
              "    child: user_lifecycle.ChildProcess = CHILD_CONTEXT\n"
              "    if user_lifecycle.state_score(child.state) != 4 {\n"
              "        return false\n"
              "    }\n"
              "    CHILD_CONTEXT = user_lifecycle.mark_running(child)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func child_exit() bool {\n"
              "    child: user_lifecycle.ChildProcess = CHILD_CONTEXT\n"
              "    if user_lifecycle.state_score(child.state) != 8 {\n"
              "        return false\n"
              "    }\n"
              "    CHILD_CONTEXT = user_lifecycle.mark_exited(child, CHILD_EXIT_CODE)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func observe_child_exit() bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    observation: user_lifecycle.WaitObservation = user_lifecycle.observe_exit(ctx.wait_handle, CHILD_CONTEXT)\n"
              "    if observation.ok == 0 {\n"
              "        return false\n"
              "    }\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_program_cap: ctx.has_program_cap, program_cap: ctx.program_cap, has_wait_handle: 0, wait_handle: user_lifecycle.empty_wait_handle(), child_ready: ctx.child_ready, observed_exit: observation.exit_code }\n"
              "    CHILD_CONTEXT = user_lifecycle.mark_observed(CHILD_CONTEXT)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    kernel_boot()\n"
              "    if INIT_CONTEXT.has_program_cap != 1 {\n"
              "        return 10\n"
              "    }\n"
              "    if INIT_CONTEXT.has_wait_handle != 0 {\n"
              "        return 11\n"
              "    }\n"
              "    if !launch_child() {\n"
              "        return 12\n"
              "    }\n"
              "    if INIT_CONTEXT.has_program_cap != 0 {\n"
              "        return 13\n"
              "    }\n"
              "    if INIT_CONTEXT.has_wait_handle != 1 {\n"
              "        return 14\n"
              "    }\n"
              "    if CHILD_CONTEXT.image_id != CHILD_IMAGE_ID {\n"
              "        return 15\n"
              "    }\n"
              "    if user_lifecycle.state_score(CHILD_CONTEXT.state) != 2 {\n"
              "        return 16\n"
              "    }\n"
              "    if launch_child() {\n"
              "        return 17\n"
              "    }\n"
              "    pre_exit: user_lifecycle.WaitObservation = user_lifecycle.observe_exit(INIT_CONTEXT.wait_handle, CHILD_CONTEXT)\n"
              "    if pre_exit.ok != 0 {\n"
              "        return 18\n"
              "    }\n"
              "    wrong_parent: user_lifecycle.WaitObservation = user_lifecycle.observe_exit(user_lifecycle.active_wait_handle(99, CHILD_PID), CHILD_CONTEXT)\n"
              "    if wrong_parent.ok != 0 {\n"
              "        return 19\n"
              "    }\n"
              "    if !child_report_ready() {\n"
              "        return 20\n"
              "    }\n"
              "    if INIT_CONTEXT.child_ready != 1 {\n"
              "        return 21\n"
              "    }\n"
              "    if CHILD_CONTEXT.ready_seen != 1 {\n"
              "        return 22\n"
              "    }\n"
              "    if user_lifecycle.state_score(CHILD_CONTEXT.state) != 4 {\n"
              "        return 23\n"
              "    }\n"
              "    if !dispatch_child() {\n"
              "        return 24\n"
              "    }\n"
              "    if user_lifecycle.state_score(CHILD_CONTEXT.state) != 8 {\n"
              "        return 25\n"
              "    }\n"
              "    if !child_exit() {\n"
              "        return 26\n"
              "    }\n"
              "    if user_lifecycle.state_score(CHILD_CONTEXT.state) != 16 {\n"
              "        return 27\n"
              "    }\n"
              "    if !observe_child_exit() {\n"
              "        return 28\n"
              "    }\n"
              "    if INIT_CONTEXT.observed_exit != CHILD_EXIT_CODE {\n"
              "        return 29\n"
              "    }\n"
              "    if INIT_CONTEXT.has_wait_handle != 0 {\n"
              "        return 30\n"
              "    }\n"
              "    if user_lifecycle.state_score(CHILD_CONTEXT.state) != 32 {\n"
              "        return 31\n"
              "    }\n"
              "    if observe_child_exit() {\n"
              "        return 32\n"
              "    }\n"
              "    return 92\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase92-user-space-lifecycle\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase92\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "user_space_lifecycle_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "user_space_lifecycle_build_output.txt",
                                                                 "freestanding user-space lifecycle build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase92 freestanding user-space lifecycle build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "user_space_lifecycle_run_output.txt",
                                                             "freestanding user-space lifecycle run");
    if (!run_outcome.exited || run_outcome.exit_code != 92) {
        Fail("phase92 freestanding user-space lifecycle run should exit with the lifecycle proof marker:\n" +
             run_output);
    }

    const std::string lifecycle_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(lifecycle_mir,
                         "TypeDecl kind=struct name=user_lifecycle.ProgramCapability",
                         "phase92 merged MIR should preserve the imported program-capability type");
    ExpectOutputContains(lifecycle_mir,
                         "TypeDecl kind=struct name=user_lifecycle.WaitHandle",
                         "phase92 merged MIR should preserve the imported wait-handle type");
    ExpectOutputContains(lifecycle_mir,
                         "TypeDecl kind=struct name=user_lifecycle.ChildProcess",
                         "phase92 merged MIR should preserve the imported child-process type");
    ExpectOutputContains(lifecycle_mir,
                         "Function name=launch_child returns=[bool]",
                         "phase92 merged MIR should keep launch policy in the root proof module");
    ExpectOutputContains(lifecycle_mir,
                         "Function name=user_lifecycle.observe_exit returns=[user_lifecycle.WaitObservation]",
                         "phase92 merged MIR should keep exit observation inside an ordinary helper function boundary");
    ExpectOutputContains(lifecycle_mir,
                         "Function name=user_lifecycle.launchable_program_cap returns=[user_lifecycle.ProgramCapability]",
                         "phase92 merged MIR should keep launchable capability construction inside an ordinary helper function boundary");
    ExpectOutputContains(lifecycle_mir,
                         "aggregate_init %v",
                         "phase92 merged MIR should use aggregate initialization for lifecycle state");
    ExpectOutputContains(lifecycle_mir,
                         "variant_match",
                         "phase92 merged MIR should lower lifecycle state classification through ordinary enum matching");
}

void TestPhase93TimerWakeProof(const std::filesystem::path& source_root,
                               const std::filesystem::path& binary_root,
                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "timer_wake_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/user_timer.mc",
              "const CAP_RIGHT_TIMER: u32 = 1\n"
              "\n"
              "enum TimerState {\n"
              "    Idle,\n"
              "    Armed,\n"
              "    Fired,\n"
              "    Delivered,\n"
              "}\n"
              "\n"
              "enum TaskState {\n"
              "    Empty,\n"
              "    Ready,\n"
              "    BlockedOnTimer,\n"
              "    Woken,\n"
              "    Running,\n"
              "    Exited,\n"
              "}\n"
              "\n"
              "struct TimerCapability {\n"
              "    timer_id: u32\n"
              "    owner_pid: u32\n"
              "    rights: u32\n"
              "}\n"
              "\n"
              "struct TimerDevice {\n"
              "    timer_id: u32\n"
              "    deadline_tick: u32\n"
              "    state: TimerState\n"
              "}\n"
              "\n"
              "struct WakeEndpoint {\n"
              "    owner_pid: u32\n"
              "    waiting_pid: u32\n"
              "    active: u32\n"
              "    wake_reason: u32\n"
              "}\n"
              "\n"
              "struct WakeObservation {\n"
              "    ok: u32\n"
              "    waiting_pid: u32\n"
              "    wake_reason: u32\n"
              "}\n"
              "\n"
              "struct WorkerTask {\n"
              "    pid: u32\n"
              "    state: TaskState\n"
              "    wake_count: u32\n"
              "    last_wake_reason: u32\n"
              "}\n"
              "\n"
              "func empty_timer_cap() TimerCapability {\n"
              "    return TimerCapability{ timer_id: 0, owner_pid: 0, rights: 0 }\n"
              "}\n"
              "\n"
              "func timer_cap(owner_pid: u32, timer_id: u32) TimerCapability {\n"
              "    return TimerCapability{ timer_id: timer_id, owner_pid: owner_pid, rights: CAP_RIGHT_TIMER }\n"
              "}\n"
              "\n"
              "func timer_cap_is_usable(cap: TimerCapability, owner_pid: u32, timer_id: u32) bool {\n"
              "    if cap.timer_id != timer_id {\n"
              "        return false\n"
              "    }\n"
              "    if cap.owner_pid != owner_pid {\n"
              "        return false\n"
              "    }\n"
              "    if cap.rights != CAP_RIGHT_TIMER {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func idle_timer(timer_id: u32) TimerDevice {\n"
              "    return TimerDevice{ timer_id: timer_id, deadline_tick: 0, state: TimerState.Idle }\n"
              "}\n"
              "\n"
              "func arm_timer(device: TimerDevice, deadline_tick: u32) TimerDevice {\n"
              "    return TimerDevice{ timer_id: device.timer_id, deadline_tick: deadline_tick, state: TimerState.Armed }\n"
              "}\n"
              "\n"
              "func tick_timer(device: TimerDevice, now_tick: u32) TimerDevice {\n"
              "    if timer_state_score(device.state) != 2 {\n"
              "        return device\n"
              "    }\n"
              "    if now_tick != device.deadline_tick {\n"
              "        return device\n"
              "    }\n"
              "    return TimerDevice{ timer_id: device.timer_id, deadline_tick: device.deadline_tick, state: TimerState.Fired }\n"
              "}\n"
              "\n"
              "func mark_delivered(device: TimerDevice) TimerDevice {\n"
              "    return TimerDevice{ timer_id: device.timer_id, deadline_tick: device.deadline_tick, state: TimerState.Delivered }\n"
              "}\n"
              "\n"
              "func empty_endpoint(owner_pid: u32, waiting_pid: u32) WakeEndpoint {\n"
              "    return WakeEndpoint{ owner_pid: owner_pid, waiting_pid: waiting_pid, active: 0, wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func armed_endpoint(owner_pid: u32, waiting_pid: u32) WakeEndpoint {\n"
              "    return WakeEndpoint{ owner_pid: owner_pid, waiting_pid: waiting_pid, active: 1, wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func deliver_wake(endpoint: WakeEndpoint, wake_reason: u32) WakeEndpoint {\n"
              "    if endpoint.active == 0 {\n"
              "        return endpoint\n"
              "    }\n"
              "    return WakeEndpoint{ owner_pid: endpoint.owner_pid, waiting_pid: endpoint.waiting_pid, active: endpoint.active, wake_reason: wake_reason }\n"
              "}\n"
              "\n"
              "func clear_endpoint(owner_pid: u32, waiting_pid: u32) WakeEndpoint {\n"
              "    return WakeEndpoint{ owner_pid: owner_pid, waiting_pid: waiting_pid, active: 0, wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func empty_observation() WakeObservation {\n"
              "    return WakeObservation{ ok: 0, waiting_pid: 0, wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func observe_wake(owner_pid: u32, endpoint: WakeEndpoint) WakeObservation {\n"
              "    if endpoint.active == 0 {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    if endpoint.owner_pid != owner_pid {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    if endpoint.wake_reason == 0 {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    return WakeObservation{ ok: 1, waiting_pid: endpoint.waiting_pid, wake_reason: endpoint.wake_reason }\n"
              "}\n"
              "\n"
              "func empty_worker(pid: u32) WorkerTask {\n"
              "    return WorkerTask{ pid: pid, state: TaskState.Empty, wake_count: 0, last_wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func ready_worker(pid: u32) WorkerTask {\n"
              "    return WorkerTask{ pid: pid, state: TaskState.Ready, wake_count: 0, last_wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func blocked_worker(task: WorkerTask) WorkerTask {\n"
              "    return WorkerTask{ pid: task.pid, state: TaskState.BlockedOnTimer, wake_count: task.wake_count, last_wake_reason: task.last_wake_reason }\n"
              "}\n"
              "\n"
              "func woken_worker(task: WorkerTask, wake_reason: u32) WorkerTask {\n"
              "    return WorkerTask{ pid: task.pid, state: TaskState.Woken, wake_count: task.wake_count + 1, last_wake_reason: wake_reason }\n"
              "}\n"
              "\n"
              "func running_worker(task: WorkerTask) WorkerTask {\n"
              "    return WorkerTask{ pid: task.pid, state: TaskState.Running, wake_count: task.wake_count, last_wake_reason: task.last_wake_reason }\n"
              "}\n"
              "\n"
              "func exited_worker(task: WorkerTask) WorkerTask {\n"
              "    return WorkerTask{ pid: task.pid, state: TaskState.Exited, wake_count: task.wake_count, last_wake_reason: task.last_wake_reason }\n"
              "}\n"
              "\n"
              "func timer_state_score(state: TimerState) i32 {\n"
              "    switch state {\n"
              "    case TimerState.Idle:\n"
              "        return 1\n"
              "    case TimerState.Armed:\n"
              "        return 2\n"
              "    case TimerState.Fired:\n"
              "        return 4\n"
              "    case TimerState.Delivered:\n"
              "        return 8\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func task_score(state: TaskState) i32 {\n"
              "    switch state {\n"
              "    case TaskState.Empty:\n"
              "        return 1\n"
              "    case TaskState.Ready:\n"
              "        return 2\n"
              "    case TaskState.BlockedOnTimer:\n"
              "        return 4\n"
              "    case TaskState.Woken:\n"
              "        return 8\n"
              "    case TaskState.Running:\n"
              "        return 16\n"
              "    case TaskState.Exited:\n"
              "        return 32\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n");
    WriteFile(project_root / "src/main.mc",
              "import user_timer\n"
              "\n"
              "const INIT_PID: u32 = 1\n"
              "const WORKER_PID: u32 = 2\n"
              "const TIMER_ID: u32 = 7\n"
              "const DEADLINE_TICK: u32 = 9\n"
              "const WAKE_REASON_TIMER: u32 = 41\n"
              "\n"
              "struct InitContext {\n"
              "    pid: u32\n"
              "    has_timer_cap: u32\n"
              "    timer_cap: user_timer.TimerCapability\n"
              "    wake_seen: u32\n"
              "    last_wake_reason: u32\n"
              "}\n"
              "\n"
              "var INIT_CONTEXT: InitContext\n"
              "var TIMER_DEVICE: user_timer.TimerDevice\n"
              "var WAKE_ENDPOINT: user_timer.WakeEndpoint\n"
              "var WORKER_TASK: user_timer.WorkerTask\n"
              "\n"
              "func kernel_boot() {\n"
              "    INIT_CONTEXT = InitContext{ pid: INIT_PID, has_timer_cap: 1, timer_cap: user_timer.timer_cap(INIT_PID, TIMER_ID), wake_seen: 0, last_wake_reason: 0 }\n"
              "    TIMER_DEVICE = user_timer.idle_timer(TIMER_ID)\n"
              "    WAKE_ENDPOINT = user_timer.empty_endpoint(INIT_PID, WORKER_PID)\n"
              "    WORKER_TASK = user_timer.ready_worker(WORKER_PID)\n"
              "}\n"
              "\n"
              "func block_worker_until_tick(deadline_tick: u32) bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    if ctx.has_timer_cap == 0 {\n"
              "        return false\n"
              "    }\n"
              "    if !user_timer.timer_cap_is_usable(ctx.timer_cap, ctx.pid, TIMER_ID) {\n"
              "        return false\n"
              "    }\n"
              "    if user_timer.task_score(WORKER_TASK.state) != 2 {\n"
              "        return false\n"
              "    }\n"
              "    TIMER_DEVICE = user_timer.arm_timer(TIMER_DEVICE, deadline_tick)\n"
              "    WAKE_ENDPOINT = user_timer.armed_endpoint(ctx.pid, WORKER_PID)\n"
              "    WORKER_TASK = user_timer.blocked_worker(WORKER_TASK)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func advance_clock(now_tick: u32) bool {\n"
              "    if user_timer.timer_state_score(TIMER_DEVICE.state) != 2 {\n"
              "        return false\n"
              "    }\n"
              "    TIMER_DEVICE = user_timer.tick_timer(TIMER_DEVICE, now_tick)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func deliver_timer_interrupt() bool {\n"
              "    if user_timer.timer_state_score(TIMER_DEVICE.state) != 4 {\n"
              "        return false\n"
              "    }\n"
              "    if user_timer.task_score(WORKER_TASK.state) != 4 {\n"
              "        return false\n"
              "    }\n"
              "    WAKE_ENDPOINT = user_timer.deliver_wake(WAKE_ENDPOINT, WAKE_REASON_TIMER)\n"
              "    TIMER_DEVICE = user_timer.mark_delivered(TIMER_DEVICE)\n"
              "    WORKER_TASK = user_timer.woken_worker(WORKER_TASK, WAKE_REASON_TIMER)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func observe_timer_wake() bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    observation: user_timer.WakeObservation = user_timer.observe_wake(ctx.pid, WAKE_ENDPOINT)\n"
              "    if observation.ok == 0 {\n"
              "        return false\n"
              "    }\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_timer_cap: ctx.has_timer_cap, timer_cap: ctx.timer_cap, wake_seen: 1, last_wake_reason: observation.wake_reason }\n"
              "    WAKE_ENDPOINT = user_timer.clear_endpoint(ctx.pid, WORKER_PID)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func dispatch_worker_after_wake() bool {\n"
              "    if user_timer.task_score(WORKER_TASK.state) != 8 {\n"
              "        return false\n"
              "    }\n"
              "    WORKER_TASK = user_timer.running_worker(WORKER_TASK)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func complete_worker() bool {\n"
              "    if user_timer.task_score(WORKER_TASK.state) != 16 {\n"
              "        return false\n"
              "    }\n"
              "    WORKER_TASK = user_timer.exited_worker(WORKER_TASK)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    kernel_boot()\n"
              "    if INIT_CONTEXT.has_timer_cap != 1 {\n"
              "        return 10\n"
              "    }\n"
              "    if user_timer.timer_state_score(TIMER_DEVICE.state) != 1 {\n"
              "        return 11\n"
              "    }\n"
              "    if user_timer.task_score(WORKER_TASK.state) != 2 {\n"
              "        return 12\n"
              "    }\n"
              "    pre_arm: user_timer.WakeObservation = user_timer.observe_wake(INIT_CONTEXT.pid, WAKE_ENDPOINT)\n"
              "    if pre_arm.ok != 0 {\n"
              "        return 13\n"
              "    }\n"
              "    if !block_worker_until_tick(DEADLINE_TICK) {\n"
              "        return 14\n"
              "    }\n"
              "    if user_timer.task_score(WORKER_TASK.state) != 4 {\n"
              "        return 15\n"
              "    }\n"
              "    if user_timer.timer_state_score(TIMER_DEVICE.state) != 2 {\n"
              "        return 16\n"
              "    }\n"
              "    if block_worker_until_tick(DEADLINE_TICK) {\n"
              "        return 17\n"
              "    }\n"
              "    before_fire: user_timer.WakeObservation = user_timer.observe_wake(INIT_CONTEXT.pid, WAKE_ENDPOINT)\n"
              "    if before_fire.ok != 0 {\n"
              "        return 18\n"
              "    }\n"
              "    if !advance_clock(DEADLINE_TICK - 1) {\n"
              "        return 19\n"
              "    }\n"
              "    if user_timer.timer_state_score(TIMER_DEVICE.state) != 2 {\n"
              "        return 20\n"
              "    }\n"
              "    if deliver_timer_interrupt() {\n"
              "        return 21\n"
              "    }\n"
              "    if !advance_clock(DEADLINE_TICK) {\n"
              "        return 22\n"
              "    }\n"
              "    if user_timer.timer_state_score(TIMER_DEVICE.state) != 4 {\n"
              "        return 23\n"
              "    }\n"
              "    wrong_owner: user_timer.WakeObservation = user_timer.observe_wake(99, WAKE_ENDPOINT)\n"
              "    if wrong_owner.ok != 0 {\n"
              "        return 24\n"
              "    }\n"
              "    if !deliver_timer_interrupt() {\n"
              "        return 25\n"
              "    }\n"
              "    if user_timer.timer_state_score(TIMER_DEVICE.state) != 8 {\n"
              "        return 26\n"
              "    }\n"
              "    if user_timer.task_score(WORKER_TASK.state) != 8 {\n"
              "        return 27\n"
              "    }\n"
              "    if WORKER_TASK.wake_count != 1 {\n"
              "        return 28\n"
              "    }\n"
              "    if WORKER_TASK.last_wake_reason != WAKE_REASON_TIMER {\n"
              "        return 29\n"
              "    }\n"
              "    if !observe_timer_wake() {\n"
              "        return 30\n"
              "    }\n"
              "    if INIT_CONTEXT.wake_seen != 1 {\n"
              "        return 31\n"
              "    }\n"
              "    if INIT_CONTEXT.last_wake_reason != WAKE_REASON_TIMER {\n"
              "        return 32\n"
              "    }\n"
              "    if observe_timer_wake() {\n"
              "        return 33\n"
              "    }\n"
              "    if !dispatch_worker_after_wake() {\n"
              "        return 34\n"
              "    }\n"
              "    if user_timer.task_score(WORKER_TASK.state) != 16 {\n"
              "        return 35\n"
              "    }\n"
              "    if !complete_worker() {\n"
              "        return 36\n"
              "    }\n"
              "    if user_timer.task_score(WORKER_TASK.state) != 32 {\n"
              "        return 37\n"
              "    }\n"
              "    if deliver_timer_interrupt() {\n"
              "        return 38\n"
              "    }\n"
              "    return 93\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase93-timer-wake\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase93\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "timer_wake_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "timer_wake_build_output.txt",
                                                                 "freestanding timer wake build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase93 freestanding timer wake build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "timer_wake_run_output.txt",
                                                             "freestanding timer wake run");
    if (!run_outcome.exited || run_outcome.exit_code != 93) {
        Fail("phase93 freestanding timer wake run should exit with the timer-wake proof marker:\n" +
             run_output);
    }

    const std::string timer_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(timer_mir,
                         "TypeDecl kind=struct name=user_timer.TimerCapability",
                         "phase93 merged MIR should preserve the imported timer-capability type");
    ExpectOutputContains(timer_mir,
                         "TypeDecl kind=struct name=user_timer.TimerDevice",
                         "phase93 merged MIR should preserve the imported timer-device type");
    ExpectOutputContains(timer_mir,
                         "TypeDecl kind=struct name=user_timer.WakeEndpoint",
                         "phase93 merged MIR should preserve the imported wake-endpoint type");
    ExpectOutputContains(timer_mir,
                         "Function name=deliver_timer_interrupt returns=[bool]",
                         "phase93 merged MIR should keep timer-delivery policy in the root proof module");
    ExpectOutputContains(timer_mir,
                         "Function name=user_timer.arm_timer returns=[user_timer.TimerDevice]",
                         "phase93 merged MIR should keep timer arming inside an ordinary helper function boundary");
    ExpectOutputContains(timer_mir,
                         "Function name=user_timer.observe_wake returns=[user_timer.WakeObservation]",
                         "phase93 merged MIR should keep wake observation inside an ordinary helper function boundary");
    ExpectOutputContains(timer_mir,
                         "aggregate_init %v",
                         "phase93 merged MIR should use aggregate initialization for timer and wake state");
    ExpectOutputContains(timer_mir,
                         "variant_match",
                         "phase93 merged MIR should lower timer and task state classification through ordinary enum matching");
}

void TestPhase94FirstSystemDemoIntegration(const std::filesystem::path& source_root,
                                           const std::filesystem::path& binary_root,
                                           const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "first_system_demo_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/system_demo.mc",
              "const CAP_RIGHT_SEND: u32 = 1\n"
              "const CAP_RIGHT_LAUNCH: u32 = 2\n"
              "const CAP_RIGHT_TIMER: u32 = 4\n"
              "\n"
              "enum ProcessState {\n"
              "    Empty,\n"
              "    Created,\n"
              "    Ready,\n"
              "    BlockedOnTimer,\n"
              "    Woken,\n"
              "    WaitingReply,\n"
              "    Replied,\n"
              "    Exited,\n"
              "    Observed,\n"
              "}\n"
              "\n"
              "enum MessageTag {\n"
              "    Empty,\n"
              "    EchoRequest,\n"
              "    EchoReply,\n"
              "}\n"
              "\n"
              "enum TimerState {\n"
              "    Idle,\n"
              "    Armed,\n"
              "    Fired,\n"
              "    Delivered,\n"
              "}\n"
              "\n"
              "struct ProgramCapability {\n"
              "    image_id: u32\n"
              "    owner_pid: u32\n"
              "    rights: u32\n"
              "}\n"
              "\n"
              "struct EndpointCapability {\n"
              "    endpoint_id: u32\n"
              "    service_pid: u32\n"
              "    rights: u32\n"
              "}\n"
              "\n"
              "struct TimerCapability {\n"
              "    timer_id: u32\n"
              "    owner_pid: u32\n"
              "    rights: u32\n"
              "}\n"
              "\n"
              "struct WaitHandle {\n"
              "    parent_pid: u32\n"
              "    child_pid: u32\n"
              "    active: u32\n"
              "}\n"
              "\n"
              "struct Message {\n"
              "    tag: MessageTag\n"
              "    sender: u32\n"
              "    recipient: u32\n"
              "    handle_slot: u32\n"
              "    len: usize\n"
              "    payload: [4]u8\n"
              "}\n"
              "\n"
              "struct ChildProcess {\n"
              "    pid: u32\n"
              "    parent_pid: u32\n"
              "    image_id: u32\n"
              "    state: ProcessState\n"
              "    inbox: Message\n"
              "    handled: u32\n"
              "    wake_count: u32\n"
              "    last_byte: u8\n"
              "    exit_code: i32\n"
              "}\n"
              "\n"
              "struct WaitObservation {\n"
              "    ok: u32\n"
              "    child_pid: u32\n"
              "    exit_code: i32\n"
              "}\n"
              "\n"
              "struct TimerDevice {\n"
              "    timer_id: u32\n"
              "    deadline_tick: u32\n"
              "    state: TimerState\n"
              "}\n"
              "\n"
              "struct WakeEndpoint {\n"
              "    owner_pid: u32\n"
              "    waiting_pid: u32\n"
              "    active: u32\n"
              "    wake_reason: u32\n"
              "}\n"
              "\n"
              "struct WakeObservation {\n"
              "    ok: u32\n"
              "    waiting_pid: u32\n"
              "    wake_reason: u32\n"
              "}\n"
              "\n"
              "struct LogEntry {\n"
              "    code: u8\n"
              "    actor_pid: u32\n"
              "}\n"
              "\n"
              "struct LogBuffer {\n"
              "    count: usize\n"
              "    entries: [10]LogEntry\n"
              "}\n"
              "\n"
              "func zero_payload() [4]u8 {\n"
              "    payload: [4]u8\n"
              "    payload[0] = 0\n"
              "    payload[1] = 0\n"
              "    payload[2] = 0\n"
              "    payload[3] = 0\n"
              "    return payload\n"
              "}\n"
              "\n"
              "func empty_message() Message {\n"
              "    return Message{ tag: MessageTag.Empty, sender: 0, recipient: 0, handle_slot: 0, len: 0, payload: zero_payload() }\n"
              "}\n"
              "\n"
              "func make_echo_request(sender: u32, recipient: u32, slot: u32, byte: u8) Message {\n"
              "    payload: [4]u8 = zero_payload()\n"
              "    payload[0] = byte\n"
              "    return Message{ tag: MessageTag.EchoRequest, sender: sender, recipient: recipient, handle_slot: slot, len: 1, payload: payload }\n"
              "}\n"
              "\n"
              "func make_echo_reply(request: Message) Message {\n"
              "    return Message{ tag: MessageTag.EchoReply, sender: request.recipient, recipient: request.sender, handle_slot: request.handle_slot, len: request.len, payload: request.payload }\n"
              "}\n"
              "\n"
              "func tag_score(tag: MessageTag) i32 {\n"
              "    switch tag {\n"
              "    case MessageTag.Empty:\n"
              "        return 1\n"
              "    case MessageTag.EchoRequest:\n"
              "        return 2\n"
              "    case MessageTag.EchoReply:\n"
              "        return 4\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func empty_program_cap() ProgramCapability {\n"
              "    return ProgramCapability{ image_id: 0, owner_pid: 0, rights: 0 }\n"
              "}\n"
              "\n"
              "func launchable_program_cap(owner_pid: u32, image_id: u32) ProgramCapability {\n"
              "    return ProgramCapability{ image_id: image_id, owner_pid: owner_pid, rights: CAP_RIGHT_LAUNCH }\n"
              "}\n"
              "\n"
              "func program_is_launchable(cap: ProgramCapability, owner_pid: u32, image_id: u32) bool {\n"
              "    if cap.image_id != image_id {\n"
              "        return false\n"
              "    }\n"
              "    if cap.owner_pid != owner_pid {\n"
              "        return false\n"
              "    }\n"
              "    if cap.rights != CAP_RIGHT_LAUNCH {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func empty_endpoint_cap() EndpointCapability {\n"
              "    return EndpointCapability{ endpoint_id: 0, service_pid: 0, rights: 0 }\n"
              "}\n"
              "\n"
              "func send_cap(service_pid: u32, endpoint_id: u32) EndpointCapability {\n"
              "    return EndpointCapability{ endpoint_id: endpoint_id, service_pid: service_pid, rights: CAP_RIGHT_SEND }\n"
              "}\n"
              "\n"
              "func cap_matches_service(cap: EndpointCapability, service_pid: u32, endpoint_id: u32) bool {\n"
              "    if cap.endpoint_id != endpoint_id {\n"
              "        return false\n"
              "    }\n"
              "    if cap.service_pid != service_pid {\n"
              "        return false\n"
              "    }\n"
              "    if cap.rights != CAP_RIGHT_SEND {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func empty_timer_cap() TimerCapability {\n"
              "    return TimerCapability{ timer_id: 0, owner_pid: 0, rights: 0 }\n"
              "}\n"
              "\n"
              "func timer_cap(owner_pid: u32, timer_id: u32) TimerCapability {\n"
              "    return TimerCapability{ timer_id: timer_id, owner_pid: owner_pid, rights: CAP_RIGHT_TIMER }\n"
              "}\n"
              "\n"
              "func timer_cap_is_usable(cap: TimerCapability, owner_pid: u32, timer_id: u32) bool {\n"
              "    if cap.timer_id != timer_id {\n"
              "        return false\n"
              "    }\n"
              "    if cap.owner_pid != owner_pid {\n"
              "        return false\n"
              "    }\n"
              "    if cap.rights != CAP_RIGHT_TIMER {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func empty_wait_handle() WaitHandle {\n"
              "    return WaitHandle{ parent_pid: 0, child_pid: 0, active: 0 }\n"
              "}\n"
              "\n"
              "func active_wait_handle(parent_pid: u32, child_pid: u32) WaitHandle {\n"
              "    return WaitHandle{ parent_pid: parent_pid, child_pid: child_pid, active: 1 }\n"
              "}\n"
              "\n"
              "func empty_child(pid: u32) ChildProcess {\n"
              "    return ChildProcess{ pid: pid, parent_pid: 0, image_id: 0, state: ProcessState.Empty, inbox: empty_message(), handled: 0, wake_count: 0, last_byte: 0, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func created_child(parent_pid: u32, child_pid: u32, image_id: u32) ChildProcess {\n"
              "    return ChildProcess{ pid: child_pid, parent_pid: parent_pid, image_id: image_id, state: ProcessState.Created, inbox: empty_message(), handled: 0, wake_count: 0, last_byte: 0, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func mark_ready(child: ChildProcess) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Ready, inbox: child.inbox, handled: child.handled, wake_count: child.wake_count, last_byte: child.last_byte, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func mark_blocked(child: ChildProcess) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.BlockedOnTimer, inbox: child.inbox, handled: child.handled, wake_count: child.wake_count, last_byte: child.last_byte, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func mark_woken(child: ChildProcess, wake_reason: u8) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Woken, inbox: child.inbox, handled: child.handled, wake_count: child.wake_count + 1, last_byte: wake_reason, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func mark_waiting_reply(child: ChildProcess) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.WaitingReply, inbox: child.inbox, handled: child.handled, wake_count: child.wake_count, last_byte: child.last_byte, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func mark_replied(child: ChildProcess, reply_byte: u8) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Replied, inbox: empty_message(), handled: child.handled, wake_count: child.wake_count, last_byte: reply_byte, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func deliver_message(child: ChildProcess, message: Message) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: child.state, inbox: message, handled: child.handled, wake_count: child.wake_count, last_byte: child.last_byte, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func record_handled(child: ChildProcess, last_byte: u8) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Ready, inbox: empty_message(), handled: child.handled + 1, wake_count: child.wake_count, last_byte: last_byte, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func mark_exited(child: ChildProcess, exit_code: i32) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Exited, inbox: child.inbox, handled: child.handled, wake_count: child.wake_count, last_byte: child.last_byte, exit_code: exit_code }\n"
              "}\n"
              "\n"
              "func mark_observed(child: ChildProcess) ChildProcess {\n"
              "    return ChildProcess{ pid: child.pid, parent_pid: child.parent_pid, image_id: child.image_id, state: ProcessState.Observed, inbox: child.inbox, handled: child.handled, wake_count: child.wake_count, last_byte: child.last_byte, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func empty_observation() WaitObservation {\n"
              "    return WaitObservation{ ok: 0, child_pid: 0, exit_code: 0 }\n"
              "}\n"
              "\n"
              "func observe_exit(handle: WaitHandle, child: ChildProcess) WaitObservation {\n"
              "    if handle.active == 0 {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    if handle.parent_pid != child.parent_pid {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    if handle.child_pid != child.pid {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    if state_score(child.state) != 256 {\n"
              "        return empty_observation()\n"
              "    }\n"
              "    return WaitObservation{ ok: 1, child_pid: child.pid, exit_code: child.exit_code }\n"
              "}\n"
              "\n"
              "func idle_timer(timer_id: u32) TimerDevice {\n"
              "    return TimerDevice{ timer_id: timer_id, deadline_tick: 0, state: TimerState.Idle }\n"
              "}\n"
              "\n"
              "func arm_timer(device: TimerDevice, deadline_tick: u32) TimerDevice {\n"
              "    return TimerDevice{ timer_id: device.timer_id, deadline_tick: deadline_tick, state: TimerState.Armed }\n"
              "}\n"
              "\n"
              "func tick_timer(device: TimerDevice, now_tick: u32) TimerDevice {\n"
              "    if timer_state_score(device.state) != 2 {\n"
              "        return device\n"
              "    }\n"
              "    if now_tick != device.deadline_tick {\n"
              "        return device\n"
              "    }\n"
              "    return TimerDevice{ timer_id: device.timer_id, deadline_tick: device.deadline_tick, state: TimerState.Fired }\n"
              "}\n"
              "\n"
              "func mark_delivered(device: TimerDevice) TimerDevice {\n"
              "    return TimerDevice{ timer_id: device.timer_id, deadline_tick: device.deadline_tick, state: TimerState.Delivered }\n"
              "}\n"
              "\n"
              "func empty_endpoint(owner_pid: u32, waiting_pid: u32) WakeEndpoint {\n"
              "    return WakeEndpoint{ owner_pid: owner_pid, waiting_pid: waiting_pid, active: 0, wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func armed_endpoint(owner_pid: u32, waiting_pid: u32) WakeEndpoint {\n"
              "    return WakeEndpoint{ owner_pid: owner_pid, waiting_pid: waiting_pid, active: 1, wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func deliver_wake(endpoint: WakeEndpoint, wake_reason: u32) WakeEndpoint {\n"
              "    if endpoint.active == 0 {\n"
              "        return endpoint\n"
              "    }\n"
              "    return WakeEndpoint{ owner_pid: endpoint.owner_pid, waiting_pid: endpoint.waiting_pid, active: endpoint.active, wake_reason: wake_reason }\n"
              "}\n"
              "\n"
              "func clear_endpoint(owner_pid: u32, waiting_pid: u32) WakeEndpoint {\n"
              "    return WakeEndpoint{ owner_pid: owner_pid, waiting_pid: waiting_pid, active: 0, wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func empty_wake_observation() WakeObservation {\n"
              "    return WakeObservation{ ok: 0, waiting_pid: 0, wake_reason: 0 }\n"
              "}\n"
              "\n"
              "func observe_wake(owner_pid: u32, endpoint: WakeEndpoint) WakeObservation {\n"
              "    if endpoint.active == 0 {\n"
              "        return empty_wake_observation()\n"
              "    }\n"
              "    if endpoint.owner_pid != owner_pid {\n"
              "        return empty_wake_observation()\n"
              "    }\n"
              "    if endpoint.wake_reason == 0 {\n"
              "        return empty_wake_observation()\n"
              "    }\n"
              "    return WakeObservation{ ok: 1, waiting_pid: endpoint.waiting_pid, wake_reason: endpoint.wake_reason }\n"
              "}\n"
              "\n"
              "func empty_log_entry() LogEntry {\n"
              "    return LogEntry{ code: 0, actor_pid: 0 }\n"
              "}\n"
              "\n"
              "func zero_log_entries() [10]LogEntry {\n"
              "    entries: [10]LogEntry\n"
              "    entries[0] = empty_log_entry()\n"
              "    entries[1] = empty_log_entry()\n"
              "    entries[2] = empty_log_entry()\n"
              "    entries[3] = empty_log_entry()\n"
              "    entries[4] = empty_log_entry()\n"
              "    entries[5] = empty_log_entry()\n"
              "    entries[6] = empty_log_entry()\n"
              "    entries[7] = empty_log_entry()\n"
              "    entries[8] = empty_log_entry()\n"
              "    entries[9] = empty_log_entry()\n"
              "    return entries\n"
              "}\n"
              "\n"
              "func empty_log_buffer() LogBuffer {\n"
              "    return LogBuffer{ count: 0, entries: zero_log_entries() }\n"
              "}\n"
              "\n"
              "func append_log(buffer: LogBuffer, code: u8, actor_pid: u32) LogBuffer {\n"
              "    if buffer.count >= 10 {\n"
              "        return buffer\n"
              "    }\n"
              "    entries: [10]LogEntry = buffer.entries\n"
              "    entries[buffer.count] = LogEntry{ code: code, actor_pid: actor_pid }\n"
              "    return LogBuffer{ count: buffer.count + 1, entries: entries }\n"
              "}\n"
              "\n"
              "func log_code_at(buffer: LogBuffer, index: usize) u8 {\n"
              "    if index >= buffer.count {\n"
              "        return 0\n"
              "    }\n"
              "    return buffer.entries[index].code\n"
              "}\n"
              "\n"
              "func log_actor_at(buffer: LogBuffer, index: usize) u32 {\n"
              "    if index >= buffer.count {\n"
              "        return 0\n"
              "    }\n"
              "    return buffer.entries[index].actor_pid\n"
              "}\n"
              "\n"
              "func state_score(state: ProcessState) i32 {\n"
              "    switch state {\n"
              "    case ProcessState.Empty:\n"
              "        return 1\n"
              "    case ProcessState.Created:\n"
              "        return 2\n"
              "    case ProcessState.Ready:\n"
              "        return 4\n"
              "    case ProcessState.BlockedOnTimer:\n"
              "        return 8\n"
              "    case ProcessState.Woken:\n"
              "        return 16\n"
              "    case ProcessState.WaitingReply:\n"
              "        return 32\n"
              "    case ProcessState.Replied:\n"
              "        return 64\n"
              "    case ProcessState.Exited:\n"
              "        return 256\n"
              "    case ProcessState.Observed:\n"
              "        return 512\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func timer_state_score(state: TimerState) i32 {\n"
              "    switch state {\n"
              "    case TimerState.Idle:\n"
              "        return 1\n"
              "    case TimerState.Armed:\n"
              "        return 2\n"
              "    case TimerState.Fired:\n"
              "        return 4\n"
              "    case TimerState.Delivered:\n"
              "        return 8\n"
              "    default:\n"
              "        return 0\n"
              "    }\n"
              "    return 0\n"
              "}\n");
    WriteFile(project_root / "src/main.mc",
              "import system_demo\n"
              "\n"
              "const KERNEL_PID: u32 = 0\n"
              "const INIT_PID: u32 = 1\n"
              "const LOG_PID: u32 = 2\n"
              "const ECHO_PID: u32 = 3\n"
              "const CLIENT_PID: u32 = 4\n"
              "const LOG_IMAGE_ID: u32 = 101\n"
              "const ECHO_IMAGE_ID: u32 = 102\n"
              "const CLIENT_IMAGE_ID: u32 = 103\n"
              "const LOG_ENDPOINT_ID: u32 = 7\n"
              "const ECHO_ENDPOINT_ID: u32 = 8\n"
              "const CLIENT_ECHO_SLOT: u32 = 5\n"
              "const TIMER_ID: u32 = 9\n"
              "const DEADLINE_TICK: u32 = 12\n"
              "const WAKE_REASON_TIMER: u32 = 41\n"
              "const ECHO_BYTE: u8 = 90\n"
              "const LOG_BOOT: u8 = 1\n"
              "const LOG_INIT: u8 = 2\n"
              "const LOG_LOG_READY: u8 = 3\n"
              "const LOG_ECHO_READY: u8 = 4\n"
              "const LOG_CLIENT_READY: u8 = 5\n"
              "const LOG_CLIENT_WAKE: u8 = 6\n"
              "const LOG_ECHO_REQUEST: u8 = 7\n"
              "const LOG_ECHO_REPLY: u8 = 8\n"
              "const LOG_CLIENT_EXIT: u8 = 9\n"
              "const CLIENT_EXIT_CODE: i32 = 33\n"
              "\n"
              "struct InitContext {\n"
              "    pid: u32\n"
              "    has_log_program: u32\n"
              "    log_program: system_demo.ProgramCapability\n"
              "    has_echo_program: u32\n"
              "    echo_program: system_demo.ProgramCapability\n"
              "    has_client_program: u32\n"
              "    client_program: system_demo.ProgramCapability\n"
              "    has_log_cap: u32\n"
              "    log_cap: system_demo.EndpointCapability\n"
              "    has_echo_cap: u32\n"
              "    echo_cap: system_demo.EndpointCapability\n"
              "    has_timer_cap: u32\n"
              "    timer_cap: system_demo.TimerCapability\n"
              "    has_client_wait: u32\n"
              "    client_wait: system_demo.WaitHandle\n"
              "    wake_seen: u32\n"
              "    observed_client_exit: i32\n"
              "}\n"
              "\n"
              "var INIT_CONTEXT: InitContext\n"
              "var LOG_SERVICE: system_demo.ChildProcess\n"
              "var ECHO_SERVICE: system_demo.ChildProcess\n"
              "var CLIENT_PROCESS: system_demo.ChildProcess\n"
              "var TIMER_DEVICE: system_demo.TimerDevice\n"
              "var WAKE_ENDPOINT: system_demo.WakeEndpoint\n"
              "var LOG_BUFFER: system_demo.LogBuffer\n"
              "var CLIENT_HAS_ECHO_CAP: u32\n"
              "var CLIENT_ECHO_CAP: system_demo.EndpointCapability\n"
              "\n"
              "func kernel_boot() {\n"
              "    INIT_CONTEXT = InitContext{ pid: INIT_PID, has_log_program: 1, log_program: system_demo.launchable_program_cap(INIT_PID, LOG_IMAGE_ID), has_echo_program: 1, echo_program: system_demo.launchable_program_cap(INIT_PID, ECHO_IMAGE_ID), has_client_program: 1, client_program: system_demo.launchable_program_cap(INIT_PID, CLIENT_IMAGE_ID), has_log_cap: 1, log_cap: system_demo.send_cap(LOG_PID, LOG_ENDPOINT_ID), has_echo_cap: 1, echo_cap: system_demo.send_cap(ECHO_PID, ECHO_ENDPOINT_ID), has_timer_cap: 1, timer_cap: system_demo.timer_cap(INIT_PID, TIMER_ID), has_client_wait: 0, client_wait: system_demo.empty_wait_handle(), wake_seen: 0, observed_client_exit: 0 }\n"
              "    LOG_SERVICE = system_demo.empty_child(LOG_PID)\n"
              "    ECHO_SERVICE = system_demo.empty_child(ECHO_PID)\n"
              "    CLIENT_PROCESS = system_demo.empty_child(CLIENT_PID)\n"
              "    TIMER_DEVICE = system_demo.idle_timer(TIMER_ID)\n"
              "    WAKE_ENDPOINT = system_demo.empty_endpoint(INIT_PID, CLIENT_PID)\n"
              "    LOG_BUFFER = system_demo.empty_log_buffer()\n"
              "    LOG_BUFFER = system_demo.append_log(LOG_BUFFER, LOG_BOOT, KERNEL_PID)\n"
              "    CLIENT_HAS_ECHO_CAP = 0\n"
              "    CLIENT_ECHO_CAP = system_demo.empty_endpoint_cap()\n"
              "}\n"
              "\n"
              "func record_bootstrap_log(code: u8, actor_pid: u32) bool {\n"
              "    if system_demo.state_score(LOG_SERVICE.state) != 4 {\n"
              "        return false\n"
              "    }\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    if ctx.has_log_cap == 0 {\n"
              "        return false\n"
              "    }\n"
              "    if !system_demo.cap_matches_service(ctx.log_cap, LOG_PID, LOG_ENDPOINT_ID) {\n"
              "        return false\n"
              "    }\n"
              "    LOG_BUFFER = system_demo.append_log(LOG_BUFFER, code, actor_pid)\n"
              "    LOG_SERVICE = system_demo.record_handled(LOG_SERVICE, code)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func init_start() {\n"
              "    LOG_BUFFER = system_demo.append_log(LOG_BUFFER, LOG_INIT, INIT_PID)\n"
              "}\n"
              "\n"
              "func launch_log_service() bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    if ctx.has_log_program == 0 {\n"
              "        return false\n"
              "    }\n"
              "    if !system_demo.program_is_launchable(ctx.log_program, ctx.pid, LOG_IMAGE_ID) {\n"
              "        return false\n"
              "    }\n"
              "    LOG_SERVICE = system_demo.mark_ready(system_demo.created_child(ctx.pid, LOG_PID, ctx.log_program.image_id))\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_log_program: 0, log_program: system_demo.empty_program_cap(), has_echo_program: ctx.has_echo_program, echo_program: ctx.echo_program, has_client_program: ctx.has_client_program, client_program: ctx.client_program, has_log_cap: ctx.has_log_cap, log_cap: ctx.log_cap, has_echo_cap: ctx.has_echo_cap, echo_cap: ctx.echo_cap, has_timer_cap: ctx.has_timer_cap, timer_cap: ctx.timer_cap, has_client_wait: ctx.has_client_wait, client_wait: ctx.client_wait, wake_seen: ctx.wake_seen, observed_client_exit: ctx.observed_client_exit }\n"
              "    return record_bootstrap_log(LOG_LOG_READY, LOG_PID)\n"
              "}\n"
              "\n"
              "func launch_echo_service() bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    if ctx.has_echo_program == 0 {\n"
              "        return false\n"
              "    }\n"
              "    if !system_demo.program_is_launchable(ctx.echo_program, ctx.pid, ECHO_IMAGE_ID) {\n"
              "        return false\n"
              "    }\n"
              "    ECHO_SERVICE = system_demo.mark_ready(system_demo.created_child(ctx.pid, ECHO_PID, ctx.echo_program.image_id))\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_log_program: ctx.has_log_program, log_program: ctx.log_program, has_echo_program: 0, echo_program: system_demo.empty_program_cap(), has_client_program: ctx.has_client_program, client_program: ctx.client_program, has_log_cap: ctx.has_log_cap, log_cap: ctx.log_cap, has_echo_cap: ctx.has_echo_cap, echo_cap: ctx.echo_cap, has_timer_cap: ctx.has_timer_cap, timer_cap: ctx.timer_cap, has_client_wait: ctx.has_client_wait, client_wait: ctx.client_wait, wake_seen: ctx.wake_seen, observed_client_exit: ctx.observed_client_exit }\n"
              "    return record_bootstrap_log(LOG_ECHO_READY, ECHO_PID)\n"
              "}\n"
              "\n"
              "func launch_client() bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    if ctx.has_client_program == 0 {\n"
              "        return false\n"
              "    }\n"
              "    if ctx.has_echo_cap == 0 {\n"
              "        return false\n"
              "    }\n"
              "    if !system_demo.program_is_launchable(ctx.client_program, ctx.pid, CLIENT_IMAGE_ID) {\n"
              "        return false\n"
              "    }\n"
              "    if !system_demo.cap_matches_service(ctx.echo_cap, ECHO_PID, ECHO_ENDPOINT_ID) {\n"
              "        return false\n"
              "    }\n"
              "    CLIENT_PROCESS = system_demo.mark_ready(system_demo.created_child(ctx.pid, CLIENT_PID, ctx.client_program.image_id))\n"
              "    CLIENT_HAS_ECHO_CAP = 1\n"
              "    CLIENT_ECHO_CAP = ctx.echo_cap\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_log_program: ctx.has_log_program, log_program: ctx.log_program, has_echo_program: ctx.has_echo_program, echo_program: ctx.echo_program, has_client_program: 0, client_program: system_demo.empty_program_cap(), has_log_cap: ctx.has_log_cap, log_cap: ctx.log_cap, has_echo_cap: 0, echo_cap: system_demo.empty_endpoint_cap(), has_timer_cap: ctx.has_timer_cap, timer_cap: ctx.timer_cap, has_client_wait: 1, client_wait: system_demo.active_wait_handle(ctx.pid, CLIENT_PID), wake_seen: ctx.wake_seen, observed_client_exit: ctx.observed_client_exit }\n"
              "    return record_bootstrap_log(LOG_CLIENT_READY, CLIENT_PID)\n"
              "}\n"
              "\n"
              "func block_client_until_tick(deadline_tick: u32) bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    if ctx.has_timer_cap == 0 {\n"
              "        return false\n"
              "    }\n"
              "    if !system_demo.timer_cap_is_usable(ctx.timer_cap, ctx.pid, TIMER_ID) {\n"
              "        return false\n"
              "    }\n"
              "    if system_demo.state_score(CLIENT_PROCESS.state) != 4 {\n"
              "        return false\n"
              "    }\n"
              "    TIMER_DEVICE = system_demo.arm_timer(TIMER_DEVICE, deadline_tick)\n"
              "    WAKE_ENDPOINT = system_demo.armed_endpoint(ctx.pid, CLIENT_PID)\n"
              "    CLIENT_PROCESS = system_demo.mark_blocked(CLIENT_PROCESS)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func advance_clock(now_tick: u32) bool {\n"
              "    if system_demo.timer_state_score(TIMER_DEVICE.state) != 2 {\n"
              "        return false\n"
              "    }\n"
              "    TIMER_DEVICE = system_demo.tick_timer(TIMER_DEVICE, now_tick)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func deliver_timer_interrupt() bool {\n"
              "    if system_demo.timer_state_score(TIMER_DEVICE.state) != 4 {\n"
              "        return false\n"
              "    }\n"
              "    if system_demo.state_score(CLIENT_PROCESS.state) != 8 {\n"
              "        return false\n"
              "    }\n"
              "    WAKE_ENDPOINT = system_demo.deliver_wake(WAKE_ENDPOINT, WAKE_REASON_TIMER)\n"
              "    TIMER_DEVICE = system_demo.mark_delivered(TIMER_DEVICE)\n"
              "    CLIENT_PROCESS = system_demo.mark_woken(CLIENT_PROCESS, LOG_CLIENT_WAKE)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func observe_client_wake() bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    observation: system_demo.WakeObservation = system_demo.observe_wake(ctx.pid, WAKE_ENDPOINT)\n"
              "    if observation.ok == 0 {\n"
              "        return false\n"
              "    }\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_log_program: ctx.has_log_program, log_program: ctx.log_program, has_echo_program: ctx.has_echo_program, echo_program: ctx.echo_program, has_client_program: ctx.has_client_program, client_program: ctx.client_program, has_log_cap: ctx.has_log_cap, log_cap: ctx.log_cap, has_echo_cap: ctx.has_echo_cap, echo_cap: ctx.echo_cap, has_timer_cap: ctx.has_timer_cap, timer_cap: ctx.timer_cap, has_client_wait: ctx.has_client_wait, client_wait: ctx.client_wait, wake_seen: 1, observed_client_exit: ctx.observed_client_exit }\n"
              "    WAKE_ENDPOINT = system_demo.clear_endpoint(ctx.pid, CLIENT_PID)\n"
              "    return record_bootstrap_log(LOG_CLIENT_WAKE, CLIENT_PID)\n"
              "}\n"
              "\n"
              "func client_send_request() bool {\n"
              "    if CLIENT_HAS_ECHO_CAP == 0 {\n"
              "        return false\n"
              "    }\n"
              "    if !system_demo.cap_matches_service(CLIENT_ECHO_CAP, ECHO_PID, ECHO_ENDPOINT_ID) {\n"
              "        return false\n"
              "    }\n"
              "    if system_demo.state_score(CLIENT_PROCESS.state) != 16 {\n"
              "        return false\n"
              "    }\n"
              "    request: system_demo.Message = system_demo.make_echo_request(CLIENT_PID, ECHO_PID, CLIENT_ECHO_SLOT, ECHO_BYTE)\n"
              "    ECHO_SERVICE = system_demo.deliver_message(ECHO_SERVICE, request)\n"
              "    CLIENT_PROCESS = system_demo.mark_waiting_reply(CLIENT_PROCESS)\n"
              "    return record_bootstrap_log(LOG_ECHO_REQUEST, CLIENT_PID)\n"
              "}\n"
              "\n"
              "func echo_service_reply() bool {\n"
              "    request: system_demo.Message = ECHO_SERVICE.inbox\n"
              "    if system_demo.state_score(ECHO_SERVICE.state) != 4 {\n"
              "        return false\n"
              "    }\n"
              "    if system_demo.tag_score(request.tag) != 2 {\n"
              "        return false\n"
              "    }\n"
              "    if request.recipient != ECHO_PID {\n"
              "        return false\n"
              "    }\n"
              "    if request.handle_slot != CLIENT_ECHO_SLOT {\n"
              "        return false\n"
              "    }\n"
              "    reply: system_demo.Message = system_demo.make_echo_reply(request)\n"
              "    CLIENT_PROCESS = system_demo.deliver_message(CLIENT_PROCESS, reply)\n"
              "    ECHO_SERVICE = system_demo.record_handled(ECHO_SERVICE, request.payload[0])\n"
              "    return record_bootstrap_log(LOG_ECHO_REPLY, ECHO_PID)\n"
              "}\n"
              "\n"
              "func client_observe_reply() bool {\n"
              "    reply: system_demo.Message = CLIENT_PROCESS.inbox\n"
              "    if system_demo.state_score(CLIENT_PROCESS.state) != 32 {\n"
              "        return false\n"
              "    }\n"
              "    if system_demo.tag_score(reply.tag) != 4 {\n"
              "        return false\n"
              "    }\n"
              "    if reply.sender != ECHO_PID {\n"
              "        return false\n"
              "    }\n"
              "    if reply.recipient != CLIENT_PID {\n"
              "        return false\n"
              "    }\n"
              "    if reply.payload[0] != ECHO_BYTE {\n"
              "        return false\n"
              "    }\n"
              "    CLIENT_PROCESS = system_demo.mark_replied(CLIENT_PROCESS, reply.payload[0])\n"
              "    return true\n"
              "}\n"
              "\n"
              "func client_exit() bool {\n"
              "    if system_demo.state_score(CLIENT_PROCESS.state) != 64 {\n"
              "        return false\n"
              "    }\n"
              "    CLIENT_PROCESS = system_demo.mark_exited(CLIENT_PROCESS, CLIENT_EXIT_CODE)\n"
              "    return record_bootstrap_log(LOG_CLIENT_EXIT, CLIENT_PID)\n"
              "}\n"
              "\n"
              "func observe_client_exit() bool {\n"
              "    ctx: InitContext = INIT_CONTEXT\n"
              "    observation: system_demo.WaitObservation = system_demo.observe_exit(ctx.client_wait, CLIENT_PROCESS)\n"
              "    if observation.ok == 0 {\n"
              "        return false\n"
              "    }\n"
              "    INIT_CONTEXT = InitContext{ pid: ctx.pid, has_log_program: ctx.has_log_program, log_program: ctx.log_program, has_echo_program: ctx.has_echo_program, echo_program: ctx.echo_program, has_client_program: ctx.has_client_program, client_program: ctx.client_program, has_log_cap: ctx.has_log_cap, log_cap: ctx.log_cap, has_echo_cap: ctx.has_echo_cap, echo_cap: ctx.echo_cap, has_timer_cap: ctx.has_timer_cap, timer_cap: ctx.timer_cap, has_client_wait: 0, client_wait: system_demo.empty_wait_handle(), wake_seen: ctx.wake_seen, observed_client_exit: observation.exit_code }\n"
              "    CLIENT_PROCESS = system_demo.mark_observed(CLIENT_PROCESS)\n"
              "    return true\n"
              "}\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    kernel_boot()\n"
              "    if LOG_BUFFER.count != 1 {\n"
              "        return 10\n"
              "    }\n"
              "    init_start()\n"
              "    if LOG_BUFFER.count != 2 {\n"
              "        return 11\n"
              "    }\n"
              "    if !launch_log_service() {\n"
              "        return 12\n"
              "    }\n"
              "    if !launch_echo_service() {\n"
              "        return 13\n"
              "    }\n"
              "    if !launch_client() {\n"
              "        return 14\n"
              "    }\n"
              "    if CLIENT_HAS_ECHO_CAP != 1 {\n"
              "        return 15\n"
              "    }\n"
              "    if INIT_CONTEXT.has_echo_cap != 0 {\n"
              "        return 16\n"
              "    }\n"
              "    if system_demo.state_score(CLIENT_PROCESS.state) != 4 {\n"
              "        return 17\n"
              "    }\n"
              "    if !block_client_until_tick(DEADLINE_TICK) {\n"
              "        return 18\n"
              "    }\n"
              "    if !advance_clock(DEADLINE_TICK - 1) {\n"
              "        return 19\n"
              "    }\n"
              "    if system_demo.timer_state_score(TIMER_DEVICE.state) != 2 {\n"
              "        return 20\n"
              "    }\n"
              "    if !advance_clock(DEADLINE_TICK) {\n"
              "        return 21\n"
              "    }\n"
              "    if !deliver_timer_interrupt() {\n"
              "        return 22\n"
              "    }\n"
              "    if !observe_client_wake() {\n"
              "        return 23\n"
              "    }\n"
              "    if INIT_CONTEXT.wake_seen != 1 {\n"
              "        return 24\n"
              "    }\n"
              "    if CLIENT_PROCESS.wake_count != 1 {\n"
              "        return 25\n"
              "    }\n"
              "    if !client_send_request() {\n"
              "        return 26\n"
              "    }\n"
              "    if !echo_service_reply() {\n"
              "        return 27\n"
              "    }\n"
              "    if !client_observe_reply() {\n"
              "        return 28\n"
              "    }\n"
              "    if CLIENT_PROCESS.last_byte != ECHO_BYTE {\n"
              "        return 29\n"
              "    }\n"
              "    if !client_exit() {\n"
              "        return 30\n"
              "    }\n"
              "    pre_exit: system_demo.WaitObservation = system_demo.observe_exit(system_demo.active_wait_handle(99, CLIENT_PID), CLIENT_PROCESS)\n"
              "    if pre_exit.ok != 0 {\n"
              "        return 31\n"
              "    }\n"
              "    if !observe_client_exit() {\n"
              "        return 32\n"
              "    }\n"
              "    if INIT_CONTEXT.observed_client_exit != CLIENT_EXIT_CODE {\n"
              "        return 33\n"
              "    }\n"
              "    if INIT_CONTEXT.has_client_wait != 0 {\n"
              "        return 34\n"
              "    }\n"
              "    if system_demo.state_score(CLIENT_PROCESS.state) != 512 {\n"
              "        return 35\n"
              "    }\n"
              "    if LOG_SERVICE.handled != 7 {\n"
              "        return 36\n"
              "    }\n"
              "    if ECHO_SERVICE.handled != 1 {\n"
              "        return 37\n"
              "    }\n"
              "    if LOG_BUFFER.count != 9 {\n"
              "        return 38\n"
              "    }\n"
              "    if system_demo.log_code_at(LOG_BUFFER, 0) != LOG_BOOT {\n"
              "        return 39\n"
              "    }\n"
              "    if system_demo.log_code_at(LOG_BUFFER, 1) != LOG_INIT {\n"
              "        return 40\n"
              "    }\n"
              "    if system_demo.log_code_at(LOG_BUFFER, 2) != LOG_LOG_READY {\n"
              "        return 41\n"
              "    }\n"
              "    if system_demo.log_code_at(LOG_BUFFER, 3) != LOG_ECHO_READY {\n"
              "        return 42\n"
              "    }\n"
              "    if system_demo.log_code_at(LOG_BUFFER, 4) != LOG_CLIENT_READY {\n"
              "        return 43\n"
              "    }\n"
              "    if system_demo.log_code_at(LOG_BUFFER, 5) != LOG_CLIENT_WAKE {\n"
              "        return 44\n"
              "    }\n"
              "    if system_demo.log_code_at(LOG_BUFFER, 6) != LOG_ECHO_REQUEST {\n"
              "        return 45\n"
              "    }\n"
              "    if system_demo.log_code_at(LOG_BUFFER, 7) != LOG_ECHO_REPLY {\n"
              "        return 46\n"
              "    }\n"
              "    if system_demo.log_code_at(LOG_BUFFER, 8) != LOG_CLIENT_EXIT {\n"
              "        return 47\n"
              "    }\n"
              "    if system_demo.log_actor_at(LOG_BUFFER, 0) != KERNEL_PID {\n"
              "        return 48\n"
              "    }\n"
              "    if system_demo.log_actor_at(LOG_BUFFER, 8) != CLIENT_PID {\n"
              "        return 49\n"
              "    }\n"
              "    return 94\n"
              "}\n");
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase94-first-system-demo\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase94\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "first_system_demo_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "first_system_demo_build_output.txt",
                                                                 "freestanding first-system demo build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase94 freestanding first-system demo build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "first_system_demo_run_output.txt",
                                                             "freestanding first-system demo run");
    if (!run_outcome.exited || run_outcome.exit_code != 94) {
        Fail("phase94 freestanding first-system demo run should exit with the first-system proof marker:\n" +
             run_output);
    }

    const std::string demo_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(demo_mir,
                         "TypeDecl kind=struct name=system_demo.LogBuffer",
                         "phase94 merged MIR should preserve the imported log-buffer type");
    ExpectOutputContains(demo_mir,
                         "TypeDecl kind=struct name=system_demo.TimerDevice",
                         "phase94 merged MIR should preserve the imported timer-device type");
    ExpectOutputContains(demo_mir,
                         "TypeDecl kind=struct name=system_demo.Message",
                         "phase94 merged MIR should preserve the imported request-reply message type");
    ExpectOutputContains(demo_mir,
                         "Function name=launch_client returns=[bool]",
                         "phase94 merged MIR should keep launch and capability routing policy in the root proof module");
    ExpectOutputContains(demo_mir,
                         "Function name=system_demo.append_log returns=[system_demo.LogBuffer]",
                         "phase94 merged MIR should keep milestone logging inside an ordinary helper function boundary");
    ExpectOutputContains(demo_mir,
                         "Function name=system_demo.make_echo_request returns=[system_demo.Message]",
                         "phase94 merged MIR should keep request construction inside an ordinary helper function boundary");
    ExpectOutputContains(demo_mir,
                         "Function name=system_demo.observe_wake returns=[system_demo.WakeObservation]",
                         "phase94 merged MIR should keep wake observation inside an ordinary helper function boundary");
    ExpectOutputContains(demo_mir,
                         "aggregate_init %v",
                         "phase94 merged MIR should use aggregate initialization for integrated system state");
    ExpectOutputContains(demo_mir,
                         "variant_match",
                         "phase94 merged MIR should lower system-state classification through ordinary enum matching");
}

}  // namespace
namespace mc::tool_tests {

void RunPhase7FreestandingSuite(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path suite_root = binary_root / "tool" / "freestanding";

    TestPhase81FreestandingBootstrapAndHal(source_root, suite_root, mc_path);
    TestPhase82FreestandingArtifactAndEntryHardening(source_root, suite_root, mc_path);
    TestPhase83NarrowHalAdmissionAndConsoleProof(source_root, suite_root, mc_path);
    TestPhase85KernelEndpointQueueSmoke(source_root, suite_root, mc_path);
    TestPhase86TaskLifecycleKernelProof(source_root, suite_root, mc_path);
    TestPhase87KernelStaticDataAndArtifactFollowThrough(source_root, suite_root, mc_path);
    TestPhase88KernelBuildIntegrationAudit(source_root, suite_root, mc_path);
    TestPhase89InitToLogServiceHandshake(source_root, suite_root, mc_path);
    TestPhase90CapabilityHandleTransferProof(source_root, suite_root, mc_path);
    TestPhase91EarlyUserSpaceHelperBoundaryAudit(source_root, suite_root, mc_path);
    TestPhase92UserSpaceLifecycleFollowThrough(source_root, suite_root, mc_path);
    TestPhase93TimerWakeProof(source_root, suite_root, mc_path);
    TestPhase94FirstSystemDemoIntegration(source_root, suite_root, mc_path);
}

}  // namespace mc::tool_tests
