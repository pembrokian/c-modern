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

namespace {

void ExpectPhase105BehaviorSlice(std::string_view run_output,
                                 const std::filesystem::path& expected_lines_path) {
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase105 freestanding kernel log-service handshake run should preserve the landed phase105 slice");
}

void ExpectPhase105MirSlice(const std::filesystem::path& mir_path,
                            const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=validate_phase104_contract_hardening returns=[bool]",
            "TypeDecl kind=struct name=log_service.LogServiceState",
            "TypeDecl kind=struct name=log_service.LogHandshakeObservation",
            "Function name=execute_phase105_log_service_handshake returns=[bool]",
            "Function name=log_service.record_open_request returns=[log_service.LogServiceState]",
            "Function name=log_service.observe_handshake returns=[log_service.LogHandshakeObservation]",
            "Function name=log_service.ack_payload returns=[[4]u8]",
            "Function name=validate_phase105_log_service_handshake returns=[bool]",
        },
        expected_projection_path,
        "phase105 merged MIR should preserve the log-service projection");
}

void ExpectPhase106BehaviorSlice(std::string_view run_output,
                                 const std::filesystem::path& expected_lines_path) {
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase106 freestanding kernel echo-service request-reply run should preserve the landed phase106 slice");
}

void ExpectPhase106MirSlice(const std::filesystem::path& mir_path,
                            const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=validate_phase105_log_service_handshake returns=[bool]",
            "TypeDecl kind=struct name=echo_service.EchoServiceState",
            "TypeDecl kind=struct name=echo_service.EchoExchangeObservation",
            "Function name=execute_phase106_echo_service_request_reply returns=[bool]",
            "Function name=echo_service.record_request returns=[echo_service.EchoServiceState]",
            "Function name=echo_service.reply_payload returns=[[4]u8]",
            "Function name=echo_service.observe_exchange returns=[echo_service.EchoExchangeObservation]",
            "Function name=validate_phase106_echo_service_request_reply returns=[bool]",
        },
        expected_projection_path,
        "phase106 merged MIR should preserve the echo-service projection");
}

}  // namespace

void RunFreestandingKernelPhase85EndpointQueueSmoke(const std::filesystem::path& source_root,
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
    MaybeCleanBuildDir(build_dir);

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

    ExpectTextContainsLinesFile(ReadFile(dump_targets.mir),
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase85_endpoint_queue.mir.contains.txt"),
                                "phase85 endpoint queue MIR should preserve the endpoint queue proof slice");
}

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

    ExpectTextContainsLinesFile(ReadFile(dump_targets.mir),
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase86_task_lifecycle.mir.contains.txt"),
                                "phase86 task lifecycle MIR should preserve the lifecycle proof slice");
}

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

    ExpectTextContainsLinesFile(ReadFile(dump_targets.mir),
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase87_static_data.mir.contains.txt"),
                                "phase87 kernel static-data MIR should preserve the static-data proof slice");
}

void RunFreestandingKernelPhase88BuildIntegrationAudit(const std::filesystem::path& source_root,
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
    CompileBootstrapCObjectAndExpectSuccess(project_root / "target/phase88_driver_support.c",
                                            driver_support_object,
                                            project_root / "target/phase88_driver_support_compile_output.txt",
                                            "phase88 driver support object compile");

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
    MaybeCleanBuildDir(build_dir);

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
    CompileBootstrapCObjectAndExpectSuccess(project_root / "target/phase88_manual_support.c",
                                            manual_support_object,
                                            project_root / "target/phase88_manual_support_compile_output.txt",
                                            "phase88 manual support object compile");

    const std::filesystem::path manual_executable = build_dir / "bin" / "phase88_manual_kernel";
    LinkBootstrapObjectsAndExpectSuccess({build_targets.object, manual_support_object, runtime_object},
                                         manual_executable,
                                         build_dir / "phase88_manual_link_output.txt",
                                         "phase88 manual freestanding kernel link");

    const auto [manual_run_outcome, manual_run_output] = RunCommandCapture({manual_executable.generic_string()},
                                                                           build_dir / "phase88_manual_link_run_output.txt",
                                                                           "phase88 manual freestanding kernel run");
    if (!manual_run_outcome.exited || manual_run_outcome.exit_code != 89) {
        Fail("phase88 manual freestanding kernel run should exit with the relinked proof marker:\n" +
             manual_run_output);
    }

}

void RunFreestandingKernelPhase105RealLogServiceHandshake(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto run_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase105_real_log_service_handshake.run.contains.txt");
    const auto mir_projection_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                         "phase105_real_log_service_handshake.mirproj.txt");
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase105_log_service",
                                                               "freestanding kernel phase105 log-service handshake build",
                                                               "freestanding kernel phase105 log-service handshake run");
    ExpectPhase105BehaviorSlice(artifacts.run_output, run_lines_path);
    ExpectPhase105MirSlice(artifacts.dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase106RealEchoServiceRequestReply(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto run_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase106_real_echo_service_request_reply.run.contains.txt");
    const auto mir_projection_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                         "phase106_real_echo_service_request_reply.mirproj.txt");
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase106_echo_service",
                                                               "freestanding kernel phase106 echo-service request-reply build",
                                                               "freestanding kernel phase106 echo-service request-reply run");
    ExpectPhase106BehaviorSlice(artifacts.run_output, run_lines_path);
    ExpectPhase106MirSlice(artifacts.dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelShard1(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    RunFreestandingKernelPhase85EndpointQueueSmoke(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase86TaskLifecycleProof(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase87StaticDataProof(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase88BuildIntegrationAudit(source_root, binary_root, mc_path);

    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_shard1",
                                                               "freestanding kernel shard1 build",
                                                               "freestanding kernel shard1 run");
    ExpectTextContainsLinesFile(artifacts.run_output,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "kernel_shard1.run.contains.txt"),
                                "shard1 freestanding kernel run should preserve the landed shard1 runtime slices");
    ExpectPhase105MirSlice(artifacts.dump_targets.mir,
                           ResolveFreestandingKernelGoldenPath(source_root,
                                                               "phase105_real_log_service_handshake.mirproj.txt"));
    ExpectPhase106MirSlice(artifacts.dump_targets.mir,
                           ResolveFreestandingKernelGoldenPath(source_root,
                                                               "phase106_real_echo_service_request_reply.mirproj.txt"));
}

}  // namespace mc::tool_tests
