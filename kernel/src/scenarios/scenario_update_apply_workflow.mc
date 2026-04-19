import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall
import update_store_service
import workflow/core

const FAIL_UPDATE_APPLY_SETUP: i32 = 24301
const FAIL_UPDATE_APPLY_STAGE0: i32 = 24302
const FAIL_UPDATE_APPLY_STAGE1: i32 = 24303
const FAIL_UPDATE_APPLY_MANIFEST: i32 = 24304
const FAIL_UPDATE_APPLY_SCHEDULE: i32 = 24305
const FAIL_UPDATE_APPLY_SCHEDULE_SHAPE: i32 = 24306
const FAIL_UPDATE_APPLY_WAITING: i32 = 24307
const FAIL_UPDATE_APPLY_RESTART: i32 = 24308
const FAIL_UPDATE_APPLY_RESUMED: i32 = 24309
const FAIL_UPDATE_APPLY_DONE: i32 = 24310
const FAIL_UPDATE_APPLY_TARGET: i32 = 24311
const FAIL_UPDATE_APPLY_STORE_RESTART: i32 = 24312
const FAIL_UPDATE_APPLY_TARGET_RELOADED: i32 = 24313
const FAIL_UPDATE_APPLY_FETCH: i32 = 24314
const FAIL_UPDATE_APPLY_ACK: i32 = 24315
const FAIL_UPDATE_APPLY_CLEAR: i32 = 24316
const FAIL_UPDATE_APPLY_REJECT_SCHEDULE: i32 = 24317
const FAIL_UPDATE_APPLY_REJECT_SHAPE: i32 = 24318
const FAIL_UPDATE_APPLY_REJECT_WAITING: i32 = 24319
const FAIL_UPDATE_APPLY_REJECT_DONE: i32 = 24320
const FAIL_UPDATE_APPLY_REJECT_FETCH: i32 = 24321
const FAIL_UPDATE_APPLY_REJECT_ACK: i32 = 24322
const FAIL_UPDATE_APPLY_CANCEL_STAGE: i32 = 24323
const FAIL_UPDATE_APPLY_CANCEL_MANIFEST: i32 = 24324
const FAIL_UPDATE_APPLY_CANCEL_SCHEDULE: i32 = 24325
const FAIL_UPDATE_APPLY_CANCEL_SHAPE: i32 = 24326
const FAIL_UPDATE_APPLY_CANCEL_WAITING: i32 = 24327
const FAIL_UPDATE_APPLY_CANCEL_WORKSET_RESTART: i32 = 24328
const FAIL_UPDATE_APPLY_CANCEL_RESTART: i32 = 24329
const FAIL_UPDATE_APPLY_CANCEL_DONE: i32 = 24330
const FAIL_UPDATE_APPLY_CANCEL_TARGET: i32 = 24331
const FAIL_UPDATE_APPLY_CANCEL_FETCH: i32 = 24332
const FAIL_UPDATE_APPLY_CANCEL_ACK: i32 = 24333

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

func expect_applied_target(s: update_store_service.UpdateStoreServiceState, version: u8, len: usize, b0: u8, b1: u8) bool {
    if !update_store_service.update_applied_present(s) {
        return false
    }
    if update_store_service.update_applied_version(s) != version {
        return false
    }
    if update_store_service.update_applied_len(s) != len {
        return false
    }
    if update_store_service.update_applied_byte(s, 0) != b0 {
        return false
    }
    return update_store_service.update_applied_byte(s, 1) == b1
}

func run_update_apply_workflow_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_UPDATE_APPLY_SETUP
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(41))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_STAGE0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(42))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_STAGE1
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_SCHEDULE
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_UPDATE_APPLY_SCHEDULE_SHAPE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_UPDATE_APPLY_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKFLOW, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_UPDATE_APPLY_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return FAIL_UPDATE_APPLY_RESUMED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return FAIL_UPDATE_APPLY_DONE
    }

    if !expect_applied_target(state.update_store.state, 7, 2, 41, 42) {
        return FAIL_UPDATE_APPLY_TARGET
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_UPDATE_STORE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_UPDATE_STORE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_UPDATE_APPLY_STORE_RESTART
    }

    if !expect_applied_target(state.update_store.state, 7, 2, 41, 42) {
        return FAIL_UPDATE_APPLY_TARGET_RELOADED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, apply_id, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_RESUMED, 1) {
        return FAIL_UPDATE_APPLY_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_clear())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_CLEAR
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_REJECT_SCHEDULE
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_UPDATE_APPLY_REJECT_SHAPE
    }
    reject_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(reject_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_UPDATE_APPLY_REJECT_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(reject_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_REJECTED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_UPDATE_APPLY_REJECT_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, reject_id, workflow_core.WORKFLOW_STATE_UPDATE_REJECTED, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_UPDATE_APPLY_REJECT_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_REJECT_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(51))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_CANCEL_STAGE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(8, 1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_CANCEL_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_CANCEL_SCHEDULE
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_UPDATE_APPLY_CANCEL_SHAPE
    }
    cancel_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(cancel_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_UPDATE_APPLY_CANCEL_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_UPDATE_APPLY_CANCEL_WORKSET_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKFLOW, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_UPDATE_APPLY_CANCEL_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(cancel_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_CANCELLED, workflow_core.WORKFLOW_RESTART_CANCELLED) {
        return FAIL_UPDATE_APPLY_CANCEL_DONE
    }

    if !expect_applied_target(state.update_store.state, 7, 2, 41, 42) {
        return FAIL_UPDATE_APPLY_CANCEL_TARGET
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, cancel_id, workflow_core.WORKFLOW_STATE_UPDATE_CANCELLED, workflow_core.WORKFLOW_RESTART_CANCELLED, 2) {
        return FAIL_UPDATE_APPLY_CANCEL_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_APPLY_CANCEL_ACK
    }

    return 0
}