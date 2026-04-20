import boot
import kernel_dispatch
import lease_service
import scenario_assert
import scenario_transport
import service_effect
import service_topology
import serial_protocol
import syscall
import workflow/core

const FAIL_LEASE_SCHEDULE_7: i32 = 23301
const FAIL_LEASE_RUNNING_7: i32 = 23302
const FAIL_LEASE_DONE_7: i32 = 23303
const FAIL_LEASE_ISSUE_7_STATUS: i32 = 23304
const FAIL_LEASE_ISSUE_7_SHAPE: i32 = 23305
const FAIL_LEASE_CONSUME_7_STATUS: i32 = 23306
const FAIL_LEASE_CONSUME_7_PAYLOAD: i32 = 23307
const FAIL_LEASE_SCHEDULE_8: i32 = 23308
const FAIL_LEASE_RUNNING_8: i32 = 23309
const FAIL_LEASE_DONE_8: i32 = 23310
const FAIL_LEASE_ISSUE_8_STATUS: i32 = 23311
const FAIL_LEASE_RESTART_COMPLETION: i32 = 23312
const FAIL_LEASE_STALE_STATUS: i32 = 23313
const FAIL_LEASE_STALE_CODE: i32 = 23314
const FAIL_LEASE_REISSUE_8_STATUS: i32 = 23315
const FAIL_LEASE_CONSUME_8_STATUS: i32 = 23316
const FAIL_LEASE_CONSUME_8_PAYLOAD: i32 = 23317
const FAIL_LEASE_CONSUMED_TWICE_STATUS: i32 = 23318
const FAIL_LEASE_CONSUMED_TWICE_CODE: i32 = 23319

func lease_issue_obs(pid: u32, workflow: u8) syscall.ReceiveObservation {
    return scenario_transport.serial_obs(service_topology.SERIAL_ENDPOINT_ID, pid, serial_protocol.encode_lease_issue(workflow))
}

func lease_consume_obs(pid: u32, id: u8, workflow: u8) syscall.ReceiveObservation {
    return scenario_transport.serial_obs(service_topology.SERIAL_ENDPOINT_ID, pid, serial_protocol.encode_lease_consume(id, workflow))
}

func run_lease_service_probe() i32 {
    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(7, 1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LEASE_SCHEDULE_7
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(7))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_RUNNING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LEASE_RUNNING_7
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(7))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_DONE, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LEASE_DONE_7
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, lease_issue_obs(1, 7))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LEASE_ISSUE_7_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_LEASE_ISSUE_7_SHAPE
    }
    first := service_effect.effect_reply_payload(effect)

    effect = kernel_dispatch.kernel_dispatch_step(&state, lease_consume_obs(99, first[0], 7))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 4 {
        return FAIL_LEASE_CONSUME_7_STATUS
    }
    payload := service_effect.effect_reply_payload(effect)
    if payload[0] != 7 || payload[1] != workflow_core.WORKFLOW_STATE_DONE || payload[2] != workflow_core.WORKFLOW_RESTART_NONE || payload[3] != 1 {
        return FAIL_LEASE_CONSUME_7_PAYLOAD
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(8, 1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LEASE_SCHEDULE_8
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(8))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_RUNNING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LEASE_RUNNING_8
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(8))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_DONE, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LEASE_DONE_8
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, lease_issue_obs(1, 8))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_LEASE_ISSUE_8_STATUS
    }
    second := service_effect.effect_reply_payload(effect)

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_COMPLETION))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_COMPLETION, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_LEASE_RESTART_COMPLETION
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, lease_consume_obs(42, second[0], 8))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument || service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_LEASE_STALE_STATUS
    }
    payload = service_effect.effect_reply_payload(effect)
    if payload[0] != lease_service.LEASE_STALE {
        return FAIL_LEASE_STALE_CODE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, lease_issue_obs(1, 8))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_LEASE_REISSUE_8_STATUS
    }
    third := service_effect.effect_reply_payload(effect)

    effect = kernel_dispatch.kernel_dispatch_step(&state, lease_consume_obs(42, third[0], 8))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 4 {
        return FAIL_LEASE_CONSUME_8_STATUS
    }
    payload = service_effect.effect_reply_payload(effect)
    if payload[0] != 8 || payload[1] != workflow_core.WORKFLOW_STATE_DONE || payload[2] != workflow_core.WORKFLOW_RESTART_NONE || payload[3] != 1 {
        return FAIL_LEASE_CONSUME_8_PAYLOAD
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, lease_consume_obs(42, third[0], 8))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument || service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_LEASE_CONSUMED_TWICE_STATUS
    }
    payload = service_effect.effect_reply_payload(effect)
    if payload[0] != lease_service.LEASE_INVALID {
        return FAIL_LEASE_CONSUMED_TWICE_CODE
    }

    return 0
}