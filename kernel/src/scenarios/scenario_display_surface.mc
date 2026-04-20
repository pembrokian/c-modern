import boot
import kernel_dispatch
import program_catalog
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall
import update_store_service
import workflow/core

const FAIL_DISPLAY_STAGE0: i32 = 27501
const FAIL_DISPLAY_STAGE1: i32 = 27502
const FAIL_DISPLAY_STAGE2: i32 = 27503
const FAIL_DISPLAY_MANIFEST: i32 = 27504
const FAIL_DISPLAY_APPLY_SCHEDULE: i32 = 27505
const FAIL_DISPLAY_APPLY_WAITING: i32 = 27506
const FAIL_DISPLAY_APPLY_DONE: i32 = 27507
const FAIL_DISPLAY_BLANK: i32 = 27508
const FAIL_DISPLAY_SELECT: i32 = 27509
const FAIL_DISPLAY_LAUNCH: i32 = 27510
const FAIL_DISPLAY_PRESENT: i32 = 27511
const FAIL_DISPLAY_QUERY: i32 = 27512
const FAIL_DISPLAY_RESTART: i32 = 27513
const FAIL_DISPLAY_CLEARED: i32 = 27514

func run_display_surface_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_DISPLAY_STAGE0
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_BLANK) {
        return FAIL_DISPLAY_BLANK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_DISPLAY_STAGE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_DISPLAY_STAGE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_DISPLAY_STAGE2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_DISPLAY_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_DISPLAY_APPLY_SCHEDULE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_DISPLAY_APPLY_WAITING
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_DISPLAY_APPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_DISPLAY_SELECT
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_DISPLAY_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_FRESH) {
        return FAIL_DISPLAY_PRESENT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_query(serial_protocol.TARGET_DISPLAY))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, serial_protocol.TARGET_DISPLAY, serial_protocol.LIFECYCLE_RESET, 0, 0) {
        return FAIL_DISPLAY_QUERY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_DISPLAY))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, serial_protocol.TARGET_DISPLAY, serial_protocol.LIFECYCLE_RESET, 0, 0) {
        return FAIL_DISPLAY_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_BLANK) {
        return FAIL_DISPLAY_CLEARED
    }

    return 0
}