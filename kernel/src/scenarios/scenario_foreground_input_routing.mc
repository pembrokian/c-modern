import boot
import input_event
import kernel_dispatch
import launcher_service
import program_catalog
import scenario_assert
import scenario_transport
import service_effect
import service_topology
import syscall
import update_store_service
import workflow/core

const FAIL_ROUTE_NO_FOREGROUND: i32 = 27601
const FAIL_ROUTE_STAGE0: i32 = 27602
const FAIL_ROUTE_STAGE1: i32 = 27603
const FAIL_ROUTE_STAGE2: i32 = 27604
const FAIL_ROUTE_MANIFEST: i32 = 27605
const FAIL_ROUTE_APPLY_SCHEDULE: i32 = 27606
const FAIL_ROUTE_APPLY_WAITING: i32 = 27607
const FAIL_ROUTE_APPLY_DONE: i32 = 27608
const FAIL_ROUTE_SELECT_ISSUE: i32 = 27609
const FAIL_ROUTE_LAUNCH: i32 = 27610
const FAIL_ROUTE_FOREGROUND: i32 = 27611
const FAIL_ROUTE_SELECT_REVIEW: i32 = 27612
const FAIL_ROUTE_SELECTED: i32 = 27613
const FAIL_ROUTE_DELIVERED: i32 = 27614

func run_foreground_input_routing_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_ROUTE_STAGE0
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(65))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, 0, input_event.INPUT_ROUTE_NO_FOREGROUND, input_event.INPUT_EVENT_KEY, 65) {
        return FAIL_ROUTE_NO_FOREGROUND
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ROUTE_STAGE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ROUTE_STAGE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ROUTE_STAGE2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ROUTE_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_ROUTE_APPLY_SCHEDULE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_ROUTE_APPLY_WAITING
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_ROUTE_APPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_ROUTE_SELECT_ISSUE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ROUTE_LAUNCH
    }
    if launcher_service.launcher_foreground_id(state.launcher.state) != program_catalog.PROGRAM_ID_ISSUE_ROLLUP {
        return FAIL_ROUTE_FOREGROUND
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_REVIEW_BOARD))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_REVIEW_BOARD, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0) {
        return FAIL_ROUTE_SELECT_REVIEW
    }
    if state.launcher.state.selected != program_catalog.PROGRAM_ID_REVIEW_BOARD {
        return FAIL_ROUTE_SELECTED
    }
    if launcher_service.launcher_foreground_id(state.launcher.state) != program_catalog.PROGRAM_ID_ISSUE_ROLLUP {
        return FAIL_ROUTE_FOREGROUND
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(66))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, 66) {
        return FAIL_ROUTE_DELIVERED
    }

    return 0
}