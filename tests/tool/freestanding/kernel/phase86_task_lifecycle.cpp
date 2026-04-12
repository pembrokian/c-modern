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
void RunFreestandingKernelPhase86TaskLifecycleProof(const std::filesystem::path& source_root,
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

}  // namespace mc::tool_tests
