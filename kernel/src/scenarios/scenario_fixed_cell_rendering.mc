import boot
import input_event
import kernel_dispatch
import program_catalog
import scenario_assert
import scenario_transport
import service_effect
import service_topology
import syscall
import update_store_service
import workflow/core

const FAIL_RENDER_STAGE0: i32 = 27801
const FAIL_RENDER_STAGE1: i32 = 27802
const FAIL_RENDER_STAGE2: i32 = 27803
const FAIL_RENDER_MANIFEST: i32 = 27804
const FAIL_RENDER_APPLY_SCHEDULE: i32 = 27805
const FAIL_RENDER_APPLY_WAITING: i32 = 27806
const FAIL_RENDER_APPLY_DONE: i32 = 27807
const FAIL_RENDER_BLANK: i32 = 27808
const FAIL_RENDER_SELECT: i32 = 27809
const FAIL_RENDER_LAUNCH: i32 = 27810
const FAIL_RENDER_EMPTY: i32 = 27811
const FAIL_RENDER_STEADY_DELIVERED: i32 = 27812
const FAIL_RENDER_STEADY: i32 = 27813
const FAIL_RENDER_BUSY_DELIVERED: i32 = 27814
const FAIL_RENDER_BUSY: i32 = 27815
const FAIL_RENDER_ATTN_DELIVERED: i32 = 27816
const FAIL_RENDER_ATTN: i32 = 27817
const FAIL_RENDER_RESET_DELIVERED: i32 = 27818
const FAIL_RENDER_RESET: i32 = 27819

func run_fixed_cell_rendering_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_RENDER_STAGE0
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_BLANK) {
        return FAIL_RENDER_BLANK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_RENDER_STAGE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_RENDER_STAGE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_RENDER_STAGE2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_RENDER_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_RENDER_APPLY_SCHEDULE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_RENDER_APPLY_WAITING
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_RENDER_APPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_RENDER_SELECT
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_RENDER_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_FRESH) {
        return FAIL_RENDER_EMPTY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(79))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, 79) {
        return FAIL_RENDER_STEADY_DELIVERED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_STEADY) {
        return FAIL_RENDER_STEADY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(79))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, 79) {
        return FAIL_RENDER_BUSY_DELIVERED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_BUSY) {
        return FAIL_RENDER_BUSY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(80))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, 80) {
        return FAIL_RENDER_ATTN_DELIVERED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_ATTN) {
        return FAIL_RENDER_ATTN
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(82))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, 82) {
        return FAIL_RENDER_RESET_DELIVERED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, scenario_assert.DISPLAY_STATE_EMPTY) {
        return FAIL_RENDER_RESET
    }

    return 0
}