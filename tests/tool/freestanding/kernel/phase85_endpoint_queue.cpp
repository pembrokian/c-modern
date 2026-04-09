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

}  // namespace mc::tool_tests
