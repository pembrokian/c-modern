import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall
import update_store_service
import workflow/core

const FAIL_INSTALLER_SETUP: i32 = 24401
const FAIL_INSTALLER_STAGE0: i32 = 24402
const FAIL_INSTALLER_STAGE1: i32 = 24403
const FAIL_INSTALLER_MANIFEST: i32 = 24404
const FAIL_INSTALLER_ISSUE_STATUS: i32 = 24405
const FAIL_INSTALLER_ISSUE_SHAPE: i32 = 24406
const FAIL_INSTALLER_SCHEDULE_STATUS: i32 = 24407
const FAIL_INSTALLER_SCHEDULE_SHAPE: i32 = 24408
const FAIL_INSTALLER_WAITING: i32 = 24409
const FAIL_INSTALLER_DONE: i32 = 24410
const FAIL_INSTALLER_TARGET: i32 = 24411
const FAIL_INSTALLER_FETCH: i32 = 24412
const FAIL_INSTALLER_ACK: i32 = 24413
const FAIL_INSTALLER_CONSUMED_SCHEDULE: i32 = 24414
const FAIL_INSTALLER_CONSUMED_WAITING: i32 = 24415
const FAIL_INSTALLER_CONSUMED_DONE: i32 = 24416
const FAIL_INSTALLER_CONSUMED_FETCH: i32 = 24417
const FAIL_INSTALLER_CONSUMED_ACK: i32 = 24418
const FAIL_INSTALLER_INVALID_SCHEDULE: i32 = 24419
const FAIL_INSTALLER_INVALID_WAITING: i32 = 24420
const FAIL_INSTALLER_INVALID_DONE: i32 = 24421
const FAIL_INSTALLER_INVALID_FETCH: i32 = 24422
const FAIL_INSTALLER_INVALID_ACK: i32 = 24423
const FAIL_INSTALLER_STALE_ISSUE: i32 = 24424
const FAIL_INSTALLER_STALE_SCHEDULE: i32 = 24425
const FAIL_INSTALLER_STALE_WAITING: i32 = 24426
const FAIL_INSTALLER_STALE_RESTART: i32 = 24427
const FAIL_INSTALLER_STALE_DONE: i32 = 24428
const FAIL_INSTALLER_STALE_FETCH: i32 = 24429
const FAIL_INSTALLER_STALE_ACK: i32 = 24430
const FAIL_INSTALLER_STALE_TARGET: i32 = 24431
const FAIL_INSTALLER_LEASE_RESET: i32 = 24432
const FAIL_INSTALLER_COMPLETION_RESET: i32 = 24433
const FAIL_INSTALLER_WORKFLOW_RESET: i32 = 24434
const FAIL_INSTALLER_UPDATE_STORE_RESET: i32 = 24435

func run_delegated_installer_authority_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(7, 1)) {
        return FAIL_INSTALLER_SETUP
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LEASE))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_LEASE_RESET
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_COMPLETION))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_COMPLETION_RESET
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_WORKFLOW_RESET
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_UPDATE_STORE))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_UPDATE_STORE_RESET
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(41))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_STAGE0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(42))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_STAGE1
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_installer_apply())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_ISSUE_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_INSTALLER_ISSUE_SHAPE
    }
    lease_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update_lease(lease_id))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_SCHEDULE_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_INSTALLER_SCHEDULE_SHAPE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_INSTALLER_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_INSTALLER_DONE
    }

    store := update_store_service.update_store_load(7, 1)
    if !update_store_service.update_applied_present(store) || update_store_service.update_applied_version(store) != 7 || update_store_service.update_applied_len(store) != 2 || update_store_service.update_applied_byte(store, 0) != 41 || update_store_service.update_applied_byte(store, 1) != 42 {
        return FAIL_INSTALLER_TARGET
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !scenario_assert.expect_completion(effect, apply_id, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_INSTALLER_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_installer_apply())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_INSTALLER_STALE_ISSUE
    }
    stale_lease_id := service_effect.effect_reply_payload(effect)[0]
    if stale_lease_id == 255 {
        return FAIL_INSTALLER_STALE_ISSUE
    }
    invalid_lease_id := stale_lease_id + 1

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update_lease(lease_id))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_INSTALLER_CONSUMED_SCHEDULE
    }
    consumed_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(consumed_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_INSTALLER_CONSUMED_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(consumed_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_LEASE_CONSUMED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_INSTALLER_CONSUMED_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !scenario_assert.expect_completion(effect, consumed_id, workflow_core.WORKFLOW_STATE_UPDATE_LEASE_CONSUMED, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_INSTALLER_CONSUMED_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_CONSUMED_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update_lease(invalid_lease_id))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_INSTALLER_INVALID_SCHEDULE
    }
    invalid_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(invalid_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_INSTALLER_INVALID_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(invalid_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_LEASE_INVALID, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_INSTALLER_INVALID_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !scenario_assert.expect_completion(effect, invalid_id, workflow_core.WORKFLOW_STATE_UPDATE_LEASE_INVALID, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_INSTALLER_INVALID_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_INVALID_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update_lease(stale_lease_id))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_INSTALLER_STALE_SCHEDULE
    }
    stale_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(stale_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_INSTALLER_STALE_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_INSTALLER_STALE_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(stale_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_LEASE_STALE, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_INSTALLER_STALE_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !scenario_assert.expect_completion(effect, stale_id, workflow_core.WORKFLOW_STATE_UPDATE_LEASE_STALE, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_INSTALLER_STALE_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_INSTALLER_STALE_ACK
    }

    store = update_store_service.update_store_load(7, 1)
    if !update_store_service.update_applied_present(store) || update_store_service.update_applied_version(store) != 7 || update_store_service.update_applied_len(store) != 2 || update_store_service.update_applied_byte(store, 0) != 41 || update_store_service.update_applied_byte(store, 1) != 42 {
        return FAIL_INSTALLER_STALE_TARGET
    }

    return 0
}