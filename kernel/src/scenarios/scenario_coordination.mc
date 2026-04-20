// Coordinated retained restart probes.

import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_identity
import syscall

const FAIL_COORDINATION_QUERY_STATUS: i32 = 147
const FAIL_COORDINATION_QUERY_MODE: i32 = 148
const FAIL_COORDINATION_KV_SET_0: i32 = 149
const FAIL_COORDINATION_KV_SET_1: i32 = 150
const FAIL_COORDINATION_QUEUE_ENQUEUE_0: i32 = 151
const FAIL_COORDINATION_QUEUE_ENQUEUE_1: i32 = 152
const FAIL_COORDINATION_PEEK_BEFORE_STATUS: i32 = 153
const FAIL_COORDINATION_PEEK_BEFORE_VALUE: i32 = 154
const FAIL_COORDINATION_KV_GET_BEFORE_STATUS: i32 = 155
const FAIL_COORDINATION_KV_GET_BEFORE_VALUE: i32 = 156
const FAIL_COORDINATION_RESTART_STATUS: i32 = 157
const FAIL_COORDINATION_KV_IDENTITY_BASE: i32 = 158
const FAIL_COORDINATION_QUEUE_IDENTITY_BASE: i32 = 162
const FAIL_COORDINATION_COUNT_AFTER_STATUS: i32 = 166
const FAIL_COORDINATION_COUNT_AFTER_LEN: i32 = 167
const FAIL_COORDINATION_DEQUEUE_0_STATUS: i32 = 168
const FAIL_COORDINATION_DEQUEUE_0_VALUE: i32 = 169
const FAIL_COORDINATION_KV_GET_0_STATUS: i32 = 170
const FAIL_COORDINATION_KV_GET_0_VALUE: i32 = 171
const FAIL_COORDINATION_DEQUEUE_1_STATUS: i32 = 172
const FAIL_COORDINATION_DEQUEUE_1_VALUE: i32 = 173
const FAIL_COORDINATION_KV_GET_1_STATUS: i32 = 174
const FAIL_COORDINATION_KV_GET_1_VALUE: i32 = 175

func run_retained_coordination_probe(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_query(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_COORDINATION_QUERY_STATUS
    }
    if service_effect.effect_reply_payload(effect)[1] != serial_protocol.LIFECYCLE_RELOAD {
        return FAIL_COORDINATION_QUERY_MODE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_set(30, 111))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_KV_SET_0
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_set(31, 122))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_KV_SET_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(30))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_QUEUE_ENQUEUE_0
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(31))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_QUEUE_ENQUEUE_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_peek())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_PEEK_BEFORE_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 30 {
        return FAIL_COORDINATION_PEEK_BEFORE_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_get(30))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_KV_GET_BEFORE_STATUS
    }
    if service_effect.effect_reply_payload(effect)[1] != 111 {
        return FAIL_COORDINATION_KV_GET_BEFORE_VALUE
    }

    kv_before := boot.boot_kv_mark(*state)
    queue_before := boot.boot_queue_mark(*state)
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_COORDINATION_RESTART_STATUS
    }

    kv_after := boot.boot_kv_mark(*state)
    queue_after := boot.boot_queue_mark(*state)
    kv_id := scenario_assert.expect_restart_identity(kv_before, kv_after, FAIL_COORDINATION_KV_IDENTITY_BASE)
    if kv_id != 0 {
        return kv_id
    }
    queue_id := scenario_assert.expect_restart_identity(queue_before, queue_after, FAIL_COORDINATION_QUEUE_IDENTITY_BASE)
    if queue_id != 0 {
        return queue_id
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_count())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_COUNT_AFTER_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_COORDINATION_COUNT_AFTER_LEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_DEQUEUE_0_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 30 {
        return FAIL_COORDINATION_DEQUEUE_0_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_get(30))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_KV_GET_0_STATUS
    }
    if service_effect.effect_reply_payload(effect)[1] != 111 {
        return FAIL_COORDINATION_KV_GET_0_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_DEQUEUE_1_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 31 {
        return FAIL_COORDINATION_DEQUEUE_1_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_get(31))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COORDINATION_KV_GET_1_STATUS
    }
    if service_effect.effect_reply_payload(effect)[1] != 122 {
        return FAIL_COORDINATION_KV_GET_1_VALUE
    }

    return 0
}