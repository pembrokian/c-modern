import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall
import workflow_core

const FAIL_WORKFLOW_SCHEDULE: i32 = 2312
const FAIL_WORKFLOW_WAITING: i32 = 2313
const FAIL_WORKFLOW_RESTART: i32 = 2314
const FAIL_WORKFLOW_RESUMED: i32 = 2315
const FAIL_WORKFLOW_RUNNING: i32 = 2316
const FAIL_WORKFLOW_DONE: i32 = 2317
const FAIL_WORKFLOW_FAILURE_SCHEDULE: i32 = 2318
const FAIL_WORKFLOW_FAILED: i32 = 2319
const FAIL_WORKFLOW_CANCEL_SCHEDULE: i32 = 2320
const FAIL_WORKFLOW_WORKSET_RESTART: i32 = 2321
const FAIL_WORKFLOW_CANCEL_RESTART: i32 = 2322
const FAIL_WORKFLOW_CANCELLED: i32 = 2323

func expect_workflow(effect: service_effect.Effect, state: u8, restart: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == state && payload[1] == restart
}

func run_workflow_service_probe() i32 {
    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(7, 2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_WORKFLOW_SCHEDULE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(7))
    if !expect_workflow(effect, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_WORKFLOW_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKFLOW, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_WORKFLOW_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(7))
    if !expect_workflow(effect, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return FAIL_WORKFLOW_RESUMED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(7))
    if !expect_workflow(effect, workflow_core.WORKFLOW_STATE_RUNNING, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return FAIL_WORKFLOW_RUNNING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(7))
    if !expect_workflow(effect, workflow_core.WORKFLOW_STATE_DONE, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return FAIL_WORKFLOW_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(255, 1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_WORKFLOW_FAILURE_SCHEDULE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(255))
    if !expect_workflow(effect, workflow_core.WORKFLOW_STATE_FAILED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_WORKFLOW_FAILED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(56, 2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_WORKFLOW_CANCEL_SCHEDULE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_WORKFLOW_WORKSET_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKFLOW, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_WORKFLOW_CANCEL_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(56))
    if !expect_workflow(effect, workflow_core.WORKFLOW_STATE_CANCELLED, workflow_core.WORKFLOW_RESTART_CANCELLED) {
        return FAIL_WORKFLOW_CANCELLED
    }

    return 0
}