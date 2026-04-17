// Stall consequence probe for Phase 216.
//
// Proves one deterministic lack-of-progress consequence on the queue service.
// When a push is rejected once (Exhausted), the queue records one stall.
// When a push is rejected again while already stalled, WouldBlock is returned.
// On a successful push, the stall count resets to zero.

import boot
import kernel_dispatch
import scenario_transport
import service_effect
import syscall

const FAIL_STALL_FILL_0: i32 = 301
const FAIL_STALL_FILL_1: i32 = 302
const FAIL_STALL_FILL_2: i32 = 303
const FAIL_STALL_FILL_3: i32 = 304
const FAIL_STALL_FIRST_OVERFLOW_STATUS: i32 = 305
const FAIL_STALL_COUNT_AFTER_FIRST: i32 = 306
const FAIL_STALL_SECOND_OVERFLOW_STATUS: i32 = 307
const FAIL_STALL_COUNT_AFTER_SECOND: i32 = 308
const FAIL_STALL_DRAIN_STATUS: i32 = 309
const FAIL_STALL_DRAIN_VALUE: i32 = 310
const FAIL_STALL_PUSH_AFTER_DRAIN_STATUS: i32 = 311
const FAIL_STALL_COUNT_AFTER_RESET: i32 = 312

func run_stall_probe(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(10))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_STALL_FILL_0
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_STALL_FILL_1
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(12))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_STALL_FILL_2
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(13))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_STALL_FILL_3
    }

    // First overflow: stall_count was 0, so Exhausted is returned and stall_count becomes 1.
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return FAIL_STALL_FIRST_OVERFLOW_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_stall_count())
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_STALL_COUNT_AFTER_FIRST
    }

    // Second overflow: stall_count > 0, so WouldBlock is returned (persistent stall consequence).
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.WouldBlock {
        return FAIL_STALL_SECOND_OVERFLOW_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_stall_count())
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_STALL_COUNT_AFTER_SECOND
    }

    // Drain one item to make room.
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_STALL_DRAIN_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 10 {
        return FAIL_STALL_DRAIN_VALUE
    }

    // Successful push resets stall_count to zero.
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(50))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_STALL_PUSH_AFTER_DRAIN_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_stall_count())
    if service_effect.effect_reply_payload_len(effect) != 0 {
        return FAIL_STALL_COUNT_AFTER_RESET
    }

    return 0
}
