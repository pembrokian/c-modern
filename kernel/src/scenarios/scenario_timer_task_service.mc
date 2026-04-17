// Phase 220: bounded timer and task service pressure probe.

import boot
import kernel_dispatch
import scenario_transport
import serial_protocol
import service_effect
import syscall
import task_service
import timer_service

const FAIL_TIMER_CREATE: i32 = 2200
const FAIL_TIMER_ACTIVE_QUERY: i32 = 2201
const FAIL_TIMER_EXPIRED_LIST: i32 = 2202
const FAIL_TIMER_CANCEL: i32 = 2203
const FAIL_TIMER_CANCELLED_QUERY: i32 = 2204
const FAIL_TIMER_RESTART: i32 = 2205
const FAIL_TIMER_RETAINED: i32 = 2206

const FAIL_TASK_SUBMIT: i32 = 2210
const FAIL_TASK_LIST: i32 = 2211
const FAIL_TASK_CANCEL: i32 = 2212
const FAIL_TASK_CANCELLED_QUERY: i32 = 2213
const FAIL_TASK_FAILED_CLASS: i32 = 2214
const FAIL_TASK_RESTART: i32 = 2215
const FAIL_TASK_RESET_QUERY: i32 = 2216
const FAIL_TASK_RESET_LIST: i32 = 2217

func expect_status(effect: service_effect.Effect, status: syscall.SyscallStatus) bool {
    return service_effect.effect_reply_status(effect) == status
}

func query_state_is(effect: service_effect.Effect, state: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) < 1 {
        return false
    }
    return service_effect.effect_reply_payload(effect)[0] == state
}

func submit_state_is(effect: service_effect.Effect, state: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) < 2 {
        return false
    }
    return service_effect.effect_reply_payload(effect)[1] == state
}

func list_first_is(effect: service_effect.Effect, id: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) == 0 {
        return false
    }
    return service_effect.effect_reply_payload(effect)[0] == id
}

func run_timer_task_probe() i32 {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_timer_create(7, 2))
    if !expect_status(effect, syscall.SyscallStatus.Ok) {
        return FAIL_TIMER_CREATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_timer_query(7))
    if !query_state_is(effect, timer_service.TIMER_STATE_ACTIVE) {
        return FAIL_TIMER_ACTIVE_QUERY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_timer_expired(4))
    if !list_first_is(effect, 7) {
        return FAIL_TIMER_EXPIRED_LIST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_timer_create(8, 3))
    if !expect_status(effect, syscall.SyscallStatus.Ok) {
        return FAIL_TIMER_CREATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_timer_cancel(8))
    if !expect_status(effect, syscall.SyscallStatus.Ok) {
        return FAIL_TIMER_CANCEL
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_timer_query(8))
    if !query_state_is(effect, timer_service.TIMER_STATE_CANCELLED) {
        return FAIL_TIMER_CANCELLED_QUERY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_TIMER))
    if !expect_status(effect, syscall.SyscallStatus.Ok) {
        return FAIL_TIMER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_timer_query(8))
    if !query_state_is(effect, timer_service.TIMER_STATE_CANCELLED) {
        return FAIL_TIMER_RETAINED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_task_submit(55))
    if !expect_status(effect, syscall.SyscallStatus.Ok) {
        return FAIL_TASK_SUBMIT
    }
    task_id: u8 = service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_task_list(4))
    if !list_first_is(effect, task_id) {
        return FAIL_TASK_LIST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_task_cancel(task_id))
    if !expect_status(effect, syscall.SyscallStatus.Ok) {
        return FAIL_TASK_CANCEL
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_task_query(task_id))
    if !query_state_is(effect, task_service.TASK_STATE_CANCELLED) {
        return FAIL_TASK_CANCELLED_QUERY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_task_submit(255))
    if !submit_state_is(effect, task_service.TASK_STATE_FAILED) {
        return FAIL_TASK_FAILED_CLASS
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_TASK))
    if !expect_status(effect, syscall.SyscallStatus.Ok) {
        return FAIL_TASK_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_task_query(task_id))
    if !expect_status(effect, syscall.SyscallStatus.InvalidArgument) {
        return FAIL_TASK_RESET_QUERY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_task_list(4))
    if service_effect.effect_reply_payload_len(effect) != 0 {
        return FAIL_TASK_RESET_LIST
    }

    return 0
}