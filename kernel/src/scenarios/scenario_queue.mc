// Queue observability probes.

import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_identity
import syscall

const FAIL_QUEUE_COUNT_EMPTY_STATUS: i32 = 117
const FAIL_QUEUE_COUNT_EMPTY_LEN: i32 = 118
const FAIL_QUEUE_PEEK_EMPTY_STATUS: i32 = 119
const FAIL_QUEUE_ENQUEUE_0_STATUS: i32 = 120
const FAIL_QUEUE_ENQUEUE_1_STATUS: i32 = 121
const FAIL_QUEUE_COUNT_FILLED_STATUS: i32 = 122
const FAIL_QUEUE_COUNT_FILLED_LEN: i32 = 123
const FAIL_QUEUE_PEEK_FIRST_STATUS: i32 = 124
const FAIL_QUEUE_PEEK_FIRST_LEN: i32 = 125
const FAIL_QUEUE_PEEK_FIRST_VALUE: i32 = 126
const FAIL_QUEUE_PEEK_REPEAT_STATUS: i32 = 127
const FAIL_QUEUE_PEEK_REPEAT_VALUE: i32 = 128
const FAIL_QUEUE_DEQUEUE_FIRST_STATUS: i32 = 129
const FAIL_QUEUE_DEQUEUE_FIRST_VALUE: i32 = 130
const FAIL_QUEUE_PEEK_SECOND_STATUS: i32 = 131
const FAIL_QUEUE_PEEK_SECOND_VALUE: i32 = 132
const FAIL_QUEUE_RESTART_STATUS: i32 = 133
const FAIL_QUEUE_RESTART_IDENTITY_BASE: i32 = 134
const FAIL_QUEUE_RESTART_COUNT_STATUS: i32 = 138
const FAIL_QUEUE_RESTART_COUNT_LEN: i32 = 139
const FAIL_QUEUE_RESTART_PEEK_STATUS: i32 = 140
const FAIL_QUEUE_RESTART_PEEK_VALUE: i32 = 141
const FAIL_QUEUE_RESTART_DEQUEUE_STATUS: i32 = 142
const FAIL_QUEUE_RESTART_DEQUEUE_VALUE: i32 = 143
const FAIL_QUEUE_FINAL_COUNT_STATUS: i32 = 144
const FAIL_QUEUE_FINAL_COUNT_LEN: i32 = 145
const FAIL_QUEUE_FINAL_PEEK_STATUS: i32 = 146
const FAIL_QUEUE_PRESSURE_FILL_0: i32 = 147
const FAIL_QUEUE_PRESSURE_FILL_1: i32 = 148
const FAIL_QUEUE_PRESSURE_FILL_2: i32 = 149
const FAIL_QUEUE_PRESSURE_FILL_3: i32 = 150
const FAIL_QUEUE_PRESSURE_OVERFLOW_STATUS: i32 = 151
const FAIL_QUEUE_PRESSURE_COUNT_STATUS: i32 = 152
const FAIL_QUEUE_PRESSURE_COUNT_LEN: i32 = 153
const FAIL_QUEUE_PRESSURE_PEEK_STATUS: i32 = 154
const FAIL_QUEUE_PRESSURE_PEEK_LEN: i32 = 155
const FAIL_QUEUE_PRESSURE_PEEK_VALUE: i32 = 156
const FAIL_QUEUE_PRESSURE_DEQUEUE_STATUS: i32 = 157
const FAIL_QUEUE_PRESSURE_DEQUEUE_LEN: i32 = 158
const FAIL_QUEUE_PRESSURE_DEQUEUE_VALUE: i32 = 159
const FAIL_QUEUE_PRESSURE_POST_COUNT_STATUS: i32 = 160
const FAIL_QUEUE_PRESSURE_POST_COUNT_LEN: i32 = 161

func run_queue_observability_probe(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_count())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_COUNT_EMPTY_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 0 {
        return FAIL_QUEUE_COUNT_EMPTY_LEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_peek())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_QUEUE_PEEK_EMPTY_STATUS
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(81))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_ENQUEUE_0_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(82))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_ENQUEUE_1_STATUS
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_count())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_COUNT_FILLED_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_QUEUE_COUNT_FILLED_LEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_peek())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PEEK_FIRST_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_QUEUE_PEEK_FIRST_LEN
    }
    if service_effect.effect_reply_payload(effect)[0] != 81 {
        return FAIL_QUEUE_PEEK_FIRST_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_peek())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PEEK_REPEAT_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 81 {
        return FAIL_QUEUE_PEEK_REPEAT_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_DEQUEUE_FIRST_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 81 {
        return FAIL_QUEUE_DEQUEUE_FIRST_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_peek())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PEEK_SECOND_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 82 {
        return FAIL_QUEUE_PEEK_SECOND_VALUE
    }

    queue_before: service_identity.ServiceMark = boot.boot_queue_mark(*state)
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_QUEUE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_QUEUE_RESTART_STATUS
    }

    queue_after: service_identity.ServiceMark = boot.boot_queue_mark(*state)
    queue_before_endpoint: u32 = service_identity.mark_endpoint(queue_before)
    queue_before_pid: u32 = service_identity.mark_pid(queue_before)
    queue_before_generation: u32 = service_identity.mark_generation(queue_before)
    queue_after_endpoint: u32 = service_identity.mark_endpoint(queue_after)
    queue_after_pid: u32 = service_identity.mark_pid(queue_after)
    queue_after_generation: u32 = service_identity.mark_generation(queue_after)
    queue_id: i32 = scenario_assert.expect_restart_identity(queue_before_endpoint, queue_before_pid, queue_before_generation, queue_after_endpoint, queue_after_pid, queue_after_generation, FAIL_QUEUE_RESTART_IDENTITY_BASE)
    if queue_id != 0 {
        return queue_id
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_count())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_RESTART_COUNT_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_QUEUE_RESTART_COUNT_LEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_peek())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_RESTART_PEEK_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 82 {
        return FAIL_QUEUE_RESTART_PEEK_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_RESTART_DEQUEUE_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 82 {
        return FAIL_QUEUE_RESTART_DEQUEUE_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_count())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_FINAL_COUNT_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 0 {
        return FAIL_QUEUE_FINAL_COUNT_LEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_peek())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_QUEUE_FINAL_PEEK_STATUS
    }

    return 0
}

func run_queue_pressure_probe(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(21))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PRESSURE_FILL_0
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PRESSURE_FILL_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(23))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PRESSURE_FILL_2
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(24))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PRESSURE_FILL_3
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return FAIL_QUEUE_PRESSURE_OVERFLOW_STATUS
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_count())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PRESSURE_COUNT_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return FAIL_QUEUE_PRESSURE_COUNT_LEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_peek())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PRESSURE_PEEK_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_QUEUE_PRESSURE_PEEK_LEN
    }
    if service_effect.effect_reply_payload(effect)[0] != 21 {
        return FAIL_QUEUE_PRESSURE_PEEK_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PRESSURE_DEQUEUE_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_QUEUE_PRESSURE_DEQUEUE_LEN
    }
    if service_effect.effect_reply_payload(effect)[0] != 21 {
        return FAIL_QUEUE_PRESSURE_DEQUEUE_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_count())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_QUEUE_PRESSURE_POST_COUNT_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 3 {
        return FAIL_QUEUE_PRESSURE_POST_COUNT_LEN
    }

    return 0
}