import boot
import kernel_dispatch
import object_store_service
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall
import workflow_core

const FAIL_UPDATE_SETUP: i32 = 23501
const FAIL_UPDATE_CREATE: i32 = 23502
const FAIL_UPDATE_SCHEDULE_STATUS: i32 = 23503
const FAIL_UPDATE_SCHEDULE_SHAPE: i32 = 23504
const FAIL_UPDATE_WAITING: i32 = 23505
const FAIL_UPDATE_RESTART: i32 = 23506
const FAIL_UPDATE_RESUMED: i32 = 23507
const FAIL_UPDATE_DONE: i32 = 23508
const FAIL_UPDATE_VALUE: i32 = 23509
const FAIL_UPDATE_FETCH: i32 = 23510
const FAIL_UPDATE_FETCH_PAYLOAD: i32 = 23511
const FAIL_UPDATE_ACK: i32 = 23512
const FAIL_REJECT_SCHEDULE_STATUS: i32 = 23513
const FAIL_REJECT_SCHEDULE_SHAPE: i32 = 23514
const FAIL_REJECT_WAITING: i32 = 23515
const FAIL_REJECT_DONE: i32 = 23516
const FAIL_REJECT_FETCH: i32 = 23517
const FAIL_REJECT_FETCH_PAYLOAD: i32 = 23518
const FAIL_REJECT_ACK: i32 = 23519
const FAIL_CANCEL_SCHEDULE_STATUS: i32 = 23520
const FAIL_CANCEL_SCHEDULE_SHAPE: i32 = 23521
const FAIL_CANCEL_WAITING: i32 = 23522
const FAIL_CANCEL_WORKSET_RESTART: i32 = 23523
const FAIL_CANCEL_RESTART: i32 = 23524
const FAIL_CANCEL_DONE: i32 = 23525
const FAIL_CANCEL_VALUE: i32 = 23526
const FAIL_CANCEL_FETCH: i32 = 23527
const FAIL_CANCEL_FETCH_PAYLOAD: i32 = 23528
const FAIL_CANCEL_ACK: i32 = 23529

func expect_value(effect: service_effect.Effect, value: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return false
    }
    return service_effect.effect_reply_payload(effect)[0] == value
}

func expect_workflow(effect: service_effect.Effect, status: syscall.SyscallStatus, state: u8, restart: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == state && payload[1] == restart
}

func expect_completion(effect: service_effect.Effect, id: u8, state: u8, restart: u8, generation: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == id && payload[1] == state && payload[2] == restart && payload[3] == generation
}

func run_named_object_update_workflow_probe() i32 {
    if !object_store_service.object_store_persist(object_store_service.object_store_init(service_topology.OBJECT_STORE_SLOT.pid, 1)) {
        return FAIL_UPDATE_SETUP
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_create(7, 41))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_CREATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_update(7, 99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_SCHEDULE_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_UPDATE_SCHEDULE_SHAPE
    }
    update_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_UPDATE_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKFLOW, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_UPDATE_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return FAIL_UPDATE_RESUMED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_OBJECT_UPDATED, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return FAIL_UPDATE_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 99) {
        return FAIL_UPDATE_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, update_id, workflow_core.WORKFLOW_STATE_OBJECT_UPDATED, workflow_core.WORKFLOW_RESTART_RESUMED, 1) {
        return FAIL_UPDATE_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_update(9, 55))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_REJECT_SCHEDULE_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_REJECT_SCHEDULE_SHAPE
    }
    reject_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(reject_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_REJECT_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(reject_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_OBJECT_REJECTED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_REJECT_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, reject_id, workflow_core.WORKFLOW_STATE_OBJECT_REJECTED, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_REJECT_FETCH_PAYLOAD
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_REJECT_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_update(7, 12))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_CANCEL_SCHEDULE_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_CANCEL_SCHEDULE_SHAPE
    }
    cancel_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(cancel_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_CANCEL_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_CANCEL_WORKSET_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKFLOW, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_CANCEL_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(cancel_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_OBJECT_CANCELLED, workflow_core.WORKFLOW_RESTART_CANCELLED) {
        return FAIL_CANCEL_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 99) {
        return FAIL_CANCEL_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, cancel_id, workflow_core.WORKFLOW_STATE_OBJECT_CANCELLED, workflow_core.WORKFLOW_RESTART_CANCELLED, 2) {
        return FAIL_CANCEL_FETCH_PAYLOAD
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_CANCEL_ACK
    }

    return 0
}
