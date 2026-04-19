import boot
import kernel_dispatch
import object_store_service
import scenario_transport
import service_effect
import service_topology
import syscall
import workflow/core

const FAIL_OBJECT_VERSION_SETUP: i32 = 25401
const FAIL_OBJECT_VERSION_CREATE: i32 = 25402
const FAIL_OBJECT_VERSION_ISSUE: i32 = 25403
const FAIL_OBJECT_VERSION_CONSUME: i32 = 25404
const FAIL_OBJECT_VERSION_WAITING: i32 = 25405
const FAIL_OBJECT_VERSION_REPLACE: i32 = 25406
const FAIL_OBJECT_VERSION_CONFLICT: i32 = 25407
const FAIL_OBJECT_VERSION_VALUE: i32 = 25408
const FAIL_OBJECT_VERSION_FETCH: i32 = 25409
const FAIL_OBJECT_VERSION_ACK: i32 = 25410

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

func run_object_version_conflict_probe() i32 {
    if !object_store_service.object_store_persist(object_store_service.object_store_init(service_topology.OBJECT_STORE_SLOT.pid, 1)) {
        return FAIL_OBJECT_VERSION_SETUP
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_create(7, 41))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_OBJECT_VERSION_CREATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_object_update(7, 99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_OBJECT_VERSION_ISSUE
    }
    lease_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_consume_object_update(lease_id))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_OBJECT_VERSION_CONSUME
    }
    update_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_OBJECT_VERSION_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_replace(7, 55))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_OBJECT_VERSION_REPLACE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_OBJECT_CONFLICT, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_OBJECT_VERSION_CONFLICT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 55) {
        return FAIL_OBJECT_VERSION_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, update_id, workflow_core.WORKFLOW_STATE_OBJECT_CONFLICT, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_OBJECT_VERSION_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_OBJECT_VERSION_ACK
    }

    return 0
}