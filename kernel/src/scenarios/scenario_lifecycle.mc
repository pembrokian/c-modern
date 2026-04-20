// Shell-visible lifecycle and identity probes.

import boot
import ipc
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_identity
import service_topology
import shell_service
import syscall

const FAIL_LIFECYCLE_QUEUE_IDENTITY: i32 = 79
const FAIL_LIFECYCLE_QUEUE_CLASS: i32 = 80
const FAIL_LIFECYCLE_ECHO_CLASS: i32 = 81
const FAIL_LIFECYCLE_SERIAL_CLASS: i32 = 82
const FAIL_LIFECYCLE_SERIAL_RESTART: i32 = 83
const FAIL_LIFECYCLE_QUEUE_ENQUEUE_0: i32 = 84
const FAIL_LIFECYCLE_QUEUE_ENQUEUE_1: i32 = 85
const FAIL_LIFECYCLE_QUEUE_RESTART: i32 = 86
const FAIL_LIFECYCLE_QUEUE_IDENTITY_BASE: i32 = 87
const FAIL_LIFECYCLE_QUEUE_IDENTITY_AFTER: i32 = 91
const FAIL_LIFECYCLE_QUEUE_DEQUEUE_0_STATUS: i32 = 92
const FAIL_LIFECYCLE_QUEUE_DEQUEUE_0_VALUE: i32 = 93
const FAIL_LIFECYCLE_QUEUE_DEQUEUE_1_STATUS: i32 = 94
const FAIL_LIFECYCLE_QUEUE_DEQUEUE_1_VALUE: i32 = 95
const FAIL_LIFECYCLE_ECHO_IDENTITY: i32 = 96
const FAIL_LIFECYCLE_ECHO_RESTART: i32 = 97
const FAIL_LIFECYCLE_ECHO_IDENTITY_BASE: i32 = 98
const FAIL_LIFECYCLE_ECHO_IDENTITY_AFTER: i32 = 102
const FAIL_LIFECYCLE_ECHO_STATUS_0: i32 = 103
const FAIL_LIFECYCLE_ECHO_STATUS_1: i32 = 104
const FAIL_LIFECYCLE_ECHO_STATUS_2: i32 = 105
const FAIL_LIFECYCLE_ECHO_STATUS_3: i32 = 106
const FAIL_LIFECYCLE_ECHO_EXHAUSTED: i32 = 107
const FAIL_LIFECYCLE_ECHO_RESTART_2: i32 = 108
const FAIL_LIFECYCLE_ECHO_STATUS_4: i32 = 109
const FAIL_LIFECYCLE_ECHO_PAYLOAD_0: i32 = 110
const FAIL_LIFECYCLE_ECHO_PAYLOAD_1: i32 = 111
const FAIL_LIFECYCLE_LOG_CLASS: i32 = 112
const FAIL_LIFECYCLE_HOSTILE_STATUS: i32 = 113
const FAIL_LIFECYCLE_HOSTILE_LEN: i32 = 114
const FAIL_LIFECYCLE_HOSTILE_REPLY: i32 = 115
const FAIL_LIFECYCLE_HOSTILE_KIND: i32 = 116

func run_shell_lifecycle_probe(state: *boot.KernelBootState) i32 {
    queue_before: service_identity.ServiceMark = boot.boot_queue_mark(*state)
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_identity(serial_protocol.TARGET_QUEUE))
    if !scenario_assert.expect_identity(effect, syscall.SyscallStatus.Ok, queue_before) {
        return FAIL_LIFECYCLE_QUEUE_IDENTITY
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_query(serial_protocol.TARGET_QUEUE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_LIFECYCLE_QUEUE_CLASS
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_query(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_LIFECYCLE_ECHO_CLASS
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_query(serial_protocol.TARGET_SERIAL))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_SERIAL, serial_protocol.LIFECYCLE_NONE) {
        return FAIL_LIFECYCLE_SERIAL_CLASS
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_SERIAL))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.InvalidArgument, serial_protocol.TARGET_SERIAL, serial_protocol.LIFECYCLE_NONE) {
        return FAIL_LIFECYCLE_SERIAL_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(71))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LIFECYCLE_QUEUE_ENQUEUE_0
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(72))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LIFECYCLE_QUEUE_ENQUEUE_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_QUEUE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_LIFECYCLE_QUEUE_RESTART
    }

    queue_after: service_identity.ServiceMark = boot.boot_queue_mark(*state)
    queue_id: i32 = scenario_assert.expect_restart_identity(queue_before, queue_after, FAIL_LIFECYCLE_QUEUE_IDENTITY_BASE)
    if queue_id != 0 {
        return queue_id
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_identity(serial_protocol.TARGET_QUEUE))
    if !scenario_assert.expect_identity(effect, syscall.SyscallStatus.Ok, queue_after) {
        return FAIL_LIFECYCLE_QUEUE_IDENTITY_AFTER
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LIFECYCLE_QUEUE_DEQUEUE_0_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 71 {
        return FAIL_LIFECYCLE_QUEUE_DEQUEUE_0_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LIFECYCLE_QUEUE_DEQUEUE_1_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != 72 {
        return FAIL_LIFECYCLE_QUEUE_DEQUEUE_1_VALUE
    }

    echo_before: service_identity.ServiceMark = boot.boot_echo_mark(*state)
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_identity(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_identity(effect, syscall.SyscallStatus.Ok, echo_before) {
        return FAIL_LIFECYCLE_ECHO_IDENTITY
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_LIFECYCLE_ECHO_RESTART
    }

    echo_after: service_identity.ServiceMark = boot.boot_echo_mark(*state)
    echo_id: i32 = scenario_assert.expect_restart_identity(echo_before, echo_after, FAIL_LIFECYCLE_ECHO_IDENTITY_BASE)
    if echo_id != 0 {
        return echo_id
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_identity(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_identity(effect, syscall.SyscallStatus.Ok, echo_after) {
        return FAIL_LIFECYCLE_ECHO_IDENTITY_AFTER
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_echo(41, 42))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LIFECYCLE_ECHO_STATUS_0
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_echo(43, 44))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LIFECYCLE_ECHO_STATUS_1
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_echo(45, 46))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LIFECYCLE_ECHO_STATUS_2
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_echo(47, 48))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LIFECYCLE_ECHO_STATUS_3
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_echo(49, 50))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return FAIL_LIFECYCLE_ECHO_EXHAUSTED
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_LIFECYCLE_ECHO_RESTART_2
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_echo(51, 52))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LIFECYCLE_ECHO_STATUS_4
    }
    if service_effect.effect_reply_payload(effect)[0] != 51 {
        return FAIL_LIFECYCLE_ECHO_PAYLOAD_0
    }
    if service_effect.effect_reply_payload(effect)[1] != 52 {
        return FAIL_LIFECYCLE_ECHO_PAYLOAD_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_query(serial_protocol.TARGET_LOG))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LOG, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_LIFECYCLE_LOG_CLASS
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.serial_obs(service_topology.SERIAL_ENDPOINT_ID, 1, ipc.payload_byte(serial_protocol.CMD_X, serial_protocol.CMD_I, serial_protocol.CMD_BANG, serial_protocol.CMD_BANG)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_LIFECYCLE_HOSTILE_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_LIFECYCLE_HOSTILE_LEN
    }
    if service_effect.effect_reply_payload(effect)[0] != shell_service.SHELL_INVALID_REPLY {
        return FAIL_LIFECYCLE_HOSTILE_REPLY
    }
    if service_effect.effect_reply_payload(effect)[1] != shell_service.SHELL_INVALID_COMMAND {
        return FAIL_LIFECYCLE_HOSTILE_KIND
    }

    return 0
}