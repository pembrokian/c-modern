#include <filesystem>
#include <string>

#include "compiler/support/dump_paths.h"
#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;
void RunFreestandingKernelPhase87StaticDataProof(const std::filesystem::path& source_root,
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
    MaybeCleanBuildDir(build_dir);

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

}  // namespace mc::tool_tests
