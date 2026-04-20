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

const FAIL_COMPOSE_STAGE0: i32 = 28201
const FAIL_COMPOSE_STAGE1: i32 = 28202
const FAIL_COMPOSE_STAGE2: i32 = 28203
const FAIL_COMPOSE_MANIFEST: i32 = 28204
const FAIL_COMPOSE_APPLY_SCHEDULE: i32 = 28205
const FAIL_COMPOSE_APPLY_WAITING: i32 = 28206
const FAIL_COMPOSE_APPLY_DONE: i32 = 28207
const FAIL_COMPOSE_SELECT: i32 = 28208
const FAIL_COMPOSE_LAUNCH: i32 = 28209
const FAIL_COMPOSE_INPUT_STEADY: i32 = 28210
const FAIL_COMPOSE_DISPLAY_STEADY: i32 = 28211
const FAIL_COMPOSE_INPUT_BUSY: i32 = 28212
const FAIL_COMPOSE_DISPLAY_BUSY: i32 = 28213
const FAIL_COMPOSE_INPUT_ATTN: i32 = 28214
const FAIL_COMPOSE_DISPLAY_ATTN: i32 = 28215
const FAIL_COMPOSE_INPUT_RESET: i32 = 28216
const FAIL_COMPOSE_DISPLAY_RESET: i32 = 28217

const DISPLAY_STATE_STEADY_CUSTOM: [4]u8 = [4]u8{ 83, 84, 67, 70 }
const DISPLAY_STATE_BUSY_CUSTOM: [4]u8 = [4]u8{ 66, 85, 67, 70 }
const DISPLAY_STATE_ATTN_CUSTOM: [4]u8 = [4]u8{ 65, 84, 67, 70 }
const DISPLAY_STATE_EMPTY_CUSTOM: [4]u8 = [4]u8{ 69, 77, 67, 70 }

func expect_delivered(effect: service_effect.Effect, value: u8) bool {
    return scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, value)
}

func run_surface_composition_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_COMPOSE_STAGE0
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COMPOSE_STAGE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(4))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COMPOSE_STAGE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(0))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COMPOSE_STAGE2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(1, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COMPOSE_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_COMPOSE_APPLY_SCHEDULE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_COMPOSE_APPLY_WAITING
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_COMPOSE_APPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_COMPOSE_SELECT
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COMPOSE_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(79))
    if !expect_delivered(effect, 79) {
        return FAIL_COMPOSE_INPUT_STEADY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, DISPLAY_STATE_STEADY_CUSTOM) {
        return FAIL_COMPOSE_DISPLAY_STEADY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(79))
    if !expect_delivered(effect, 79) {
        return FAIL_COMPOSE_INPUT_BUSY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, DISPLAY_STATE_BUSY_CUSTOM) {
        return FAIL_COMPOSE_DISPLAY_BUSY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(80))
    if !expect_delivered(effect, 80) {
        return FAIL_COMPOSE_INPUT_ATTN
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, DISPLAY_STATE_ATTN_CUSTOM) {
        return FAIL_COMPOSE_DISPLAY_ATTN
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(82))
    if !expect_delivered(effect, 82) {
        return FAIL_COMPOSE_INPUT_RESET
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, DISPLAY_STATE_EMPTY_CUSTOM) {
        return FAIL_COMPOSE_DISPLAY_RESET
    }

    return 0
}