import boot
import completion_mailbox_service
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall
import update_store_service
import workflow/core

const FAIL_UPDATE_PRESSURE_SETUP: i32 = 24501
const FAIL_UPDATE_PRESSURE_FILL_SCHEDULE_BASE: i32 = 24502
const FAIL_UPDATE_PRESSURE_FILL_RUNNING_BASE: i32 = 24507
const FAIL_UPDATE_PRESSURE_FILL_DONE_BASE: i32 = 24512
const FAIL_UPDATE_PRESSURE_FULL_COUNT: i32 = 24517
const FAIL_UPDATE_PRESSURE_STAGE0: i32 = 24518
const FAIL_UPDATE_PRESSURE_STAGE1: i32 = 24519
const FAIL_UPDATE_PRESSURE_MANIFEST: i32 = 24520
const FAIL_UPDATE_PRESSURE_ISSUE: i32 = 24521
const FAIL_UPDATE_PRESSURE_ISSUE_SHAPE: i32 = 24522
const FAIL_UPDATE_PRESSURE_SCHEDULE: i32 = 24523
const FAIL_UPDATE_PRESSURE_SCHEDULE_SHAPE: i32 = 24524
const FAIL_UPDATE_PRESSURE_WAITING: i32 = 24525
const FAIL_UPDATE_PRESSURE_EXHAUSTED: i32 = 24526
const FAIL_UPDATE_PRESSURE_EXHAUSTED_OUTCOME: i32 = 24527
const FAIL_UPDATE_PRESSURE_TARGET_PENDING: i32 = 24528
const FAIL_UPDATE_PRESSURE_RESTART: i32 = 24529
const FAIL_UPDATE_PRESSURE_RESUMED: i32 = 24530
const FAIL_UPDATE_PRESSURE_RESUMED_OUTCOME: i32 = 24531
const FAIL_UPDATE_PRESSURE_TARGET_RESUMED: i32 = 24532
const FAIL_UPDATE_PRESSURE_RELEASE: i32 = 24533
const FAIL_UPDATE_PRESSURE_RECOVERED: i32 = 24534
const FAIL_UPDATE_PRESSURE_COUNT_AFTER_RECOVER: i32 = 24535
const FAIL_UPDATE_PRESSURE_DRAIN_0: i32 = 24536
const FAIL_UPDATE_PRESSURE_DRAIN_1: i32 = 24537
const FAIL_UPDATE_PRESSURE_DRAIN_2: i32 = 24538
const FAIL_UPDATE_PRESSURE_FETCH: i32 = 24539
const FAIL_UPDATE_PRESSURE_ACK_FINAL: i32 = 24540

func expect_delivery_outcome(effect: service_effect.Effect, outcome: u8) bool {
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    return service_effect.effect_reply_payload(effect)[2] == outcome
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

func expect_count(effect: service_effect.Effect, count: usize) bool {
    return service_effect.effect_reply_status(effect) == syscall.SyscallStatus.Ok && service_effect.effect_reply_payload_len(effect) == count
}

func run_update_recovery_completion_pressure_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_UPDATE_PRESSURE_SETUP
    }

    state := boot.kernel_init()

    for id in 11..15 {
        schedule_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(u8(id), 1))
        if service_effect.effect_reply_status(schedule_effect) != syscall.SyscallStatus.Ok {
            return FAIL_UPDATE_PRESSURE_FILL_SCHEDULE_BASE + (id - 11)
        }

        running_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(u8(id)))
        if !scenario_assert.expect_workflow_state(running_effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_RUNNING, workflow_core.WORKFLOW_RESTART_NONE) {
            return FAIL_UPDATE_PRESSURE_FILL_RUNNING_BASE + (id - 11)
        }

        done_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(u8(id)))
        if !scenario_assert.expect_workflow_state(done_effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_DONE, workflow_core.WORKFLOW_RESTART_NONE) {
            return FAIL_UPDATE_PRESSURE_FILL_DONE_BASE + (id - 11)
        }
    }

    count_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_count())
    if !expect_count(count_effect, completion_mailbox_service.MAILBOX_CAPACITY) {
        return FAIL_UPDATE_PRESSURE_FULL_COUNT
    }

    stage0_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(41))
    if service_effect.effect_reply_status(stage0_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_STAGE0
    }

    stage1_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(42))
    if service_effect.effect_reply_status(stage1_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_STAGE1
    }

    manifest_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 2))
    if service_effect.effect_reply_status(manifest_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_MANIFEST
    }

    issue_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_installer_apply())
    if service_effect.effect_reply_status(issue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_ISSUE
    }
    if service_effect.effect_reply_payload_len(issue_effect) != 2 {
        return FAIL_UPDATE_PRESSURE_ISSUE_SHAPE
    }
    lease_id := service_effect.effect_reply_payload(issue_effect)[0]

    schedule_apply_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update_lease(lease_id))
    if service_effect.effect_reply_status(schedule_apply_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_SCHEDULE
    }
    if service_effect.effect_reply_payload_len(schedule_apply_effect) != 2 {
        return FAIL_UPDATE_PRESSURE_SCHEDULE_SHAPE
    }
    apply_id := service_effect.effect_reply_payload(schedule_apply_effect)[0]

    waiting_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(waiting_effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_UPDATE_PRESSURE_WAITING
    }

    exhausted_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(exhausted_effect, syscall.SyscallStatus.Exhausted, workflow_core.WORKFLOW_STATE_DELIVERING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_UPDATE_PRESSURE_EXHAUSTED
    }
    if !expect_delivery_outcome(exhausted_effect, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED) {
        return FAIL_UPDATE_PRESSURE_EXHAUSTED_OUTCOME
    }

    if !expect_applied_target(state.update_store.state, 7, 2, 41, 42) {
        return FAIL_UPDATE_PRESSURE_TARGET_PENDING
    }

    restart_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if !scenario_assert.expect_lifecycle(restart_effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKFLOW, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_UPDATE_PRESSURE_RESTART
    }

    resumed_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(resumed_effect, syscall.SyscallStatus.WouldBlock, workflow_core.WORKFLOW_STATE_DELIVERING, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return FAIL_UPDATE_PRESSURE_RESUMED
    }
    if !expect_delivery_outcome(resumed_effect, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED) {
        return FAIL_UPDATE_PRESSURE_RESUMED_OUTCOME
    }

    if !expect_applied_target(state.update_store.state, 7, 2, 41, 42) {
        return FAIL_UPDATE_PRESSURE_TARGET_RESUMED
    }

    release_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(release_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_RELEASE
    }

    recovered_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(recovered_effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return FAIL_UPDATE_PRESSURE_RECOVERED
    }

    recovered_count_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_count())
    if !expect_count(recovered_count_effect, completion_mailbox_service.MAILBOX_CAPACITY) {
        return FAIL_UPDATE_PRESSURE_COUNT_AFTER_RECOVER
    }

    drain0_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(drain0_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_DRAIN_0
    }
    drain1_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(drain1_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_DRAIN_1
    }
    drain2_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(drain2_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_DRAIN_2
    }

    fetch_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !scenario_assert.expect_completion(fetch_effect, apply_id, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_RESUMED, 1) {
        return FAIL_UPDATE_PRESSURE_FETCH
    }

    ack_final_effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(ack_final_effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_PRESSURE_ACK_FINAL
    }

    return 0
}