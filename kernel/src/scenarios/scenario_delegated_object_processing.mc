import boot
import kernel_dispatch
import lease_service
import object_store_service
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall
import workflow/core

const FAIL_DELEGATED_SETUP: i32 = 23601
const FAIL_DELEGATED_CREATE: i32 = 23602
const FAIL_DELEGATED_INVALID_STATUS: i32 = 23603
const FAIL_DELEGATED_INVALID_CODE: i32 = 23604
const FAIL_DELEGATED_ISSUE_STATUS: i32 = 23605
const FAIL_DELEGATED_ISSUE_SHAPE: i32 = 23606
const FAIL_DELEGATED_CONSUME_STATUS: i32 = 23607
const FAIL_DELEGATED_CONSUME_SHAPE: i32 = 23608
const FAIL_DELEGATED_WAITING: i32 = 23609
const FAIL_DELEGATED_DONE: i32 = 23610
const FAIL_DELEGATED_VALUE: i32 = 23611
const FAIL_DELEGATED_FETCH: i32 = 23612
const FAIL_DELEGATED_ACK: i32 = 23613
const FAIL_DELEGATED_CONSUMED_STATUS: i32 = 23614
const FAIL_DELEGATED_CONSUMED_CODE: i32 = 23615
const FAIL_DELEGATED_STALE_ISSUE_STATUS: i32 = 23616
const FAIL_DELEGATED_RESTART: i32 = 23617
const FAIL_DELEGATED_STALE_STATUS: i32 = 23618
const FAIL_DELEGATED_STALE_CODE: i32 = 23619
const FAIL_DELEGATED_STALE_VALUE: i32 = 23620

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

func run_delegated_named_object_processing_probe() i32 {
    if !object_store_service.object_store_persist(object_store_service.object_store_init(service_topology.OBJECT_STORE_SLOT.pid, 1)) {
        return FAIL_DELEGATED_SETUP
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_create(7, 41))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_DELEGATED_CREATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_consume_object_update(99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument || service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_DELEGATED_INVALID_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != lease_service.LEASE_INVALID {
        return FAIL_DELEGATED_INVALID_CODE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_object_update(7, 99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_DELEGATED_ISSUE_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_DELEGATED_ISSUE_SHAPE
    }
    lease_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_consume_object_update(lease_id))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_DELEGATED_CONSUME_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_DELEGATED_CONSUME_SHAPE
    }
    update_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_DELEGATED_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_OBJECT_UPDATED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_DELEGATED_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 99) {
        return FAIL_DELEGATED_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, update_id, workflow_core.WORKFLOW_STATE_OBJECT_UPDATED, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_DELEGATED_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_DELEGATED_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_consume_object_update(lease_id))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument || service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_DELEGATED_CONSUMED_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != lease_service.LEASE_INVALID {
        return FAIL_DELEGATED_CONSUMED_CODE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_object_update(7, 12))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_DELEGATED_STALE_ISSUE_STATUS
    }
    stale_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_DELEGATED_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_consume_object_update(stale_id))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument || service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_DELEGATED_STALE_STATUS
    }
    if service_effect.effect_reply_payload(effect)[0] != lease_service.LEASE_STALE {
        return FAIL_DELEGATED_STALE_CODE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 99) {
        return FAIL_DELEGATED_STALE_VALUE
    }

    return 0
}