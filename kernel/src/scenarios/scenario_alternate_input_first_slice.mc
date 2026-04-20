import boot
import input_event
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

const FAIL_ALT_STAGE0: i32 = 28301
const FAIL_ALT_STAGE1: i32 = 28302
const FAIL_ALT_STAGE2: i32 = 28303
const FAIL_ALT_MANIFEST: i32 = 28304
const FAIL_ALT_APPLY_SCHEDULE: i32 = 28305
const FAIL_ALT_APPLY_WAITING: i32 = 28306
const FAIL_ALT_APPLY_DONE: i32 = 28307
const FAIL_ALT_SELECT: i32 = 28308
const FAIL_ALT_LAUNCH: i32 = 28309
const FAIL_ALT_INPUT_KEY: i32 = 28310
const FAIL_ALT_DISPLAY_KEY: i32 = 28311
const FAIL_ALT_INPUT_UNSUPPORTED: i32 = 28312
const FAIL_ALT_DISPLAY_UNSUPPORTED: i32 = 28313

const INPUT_EVENT_POINTER_PROBE: u8 = 80
const INPUT_POINTER_VALUE_PROBE: u8 = 1
const DISPLAY_STATE_STEADY_DEFAULT: [4]u8 = [4]u8{ 83, 84, 68, 89 }

func alternate_input_obs(event: u8, value: u8) syscall.ReceiveObservation {
    return scenario_transport.serial_obs(
        scenario_transport.DEFAULT_SERIAL_ROUTE.endpoint,
        scenario_transport.DEFAULT_SERIAL_ROUTE.pid,
        [4]u8{ serial_protocol.CMD_I, event, value, serial_protocol.CMD_BANG })
}

func run_alternate_input_first_slice_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_ALT_STAGE0
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ALT_STAGE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ALT_STAGE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ALT_STAGE2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ALT_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_ALT_APPLY_SCHEDULE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_ALT_APPLY_WAITING
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_ALT_APPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_ALT_SELECT
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_ALT_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(79))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, 79) {
        return FAIL_ALT_INPUT_KEY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, DISPLAY_STATE_STEADY_DEFAULT) {
        return FAIL_ALT_DISPLAY_KEY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, alternate_input_obs(INPUT_EVENT_POINTER_PROBE, INPUT_POINTER_VALUE_PROBE))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_UNSUPPORTED, INPUT_EVENT_POINTER_PROBE, INPUT_POINTER_VALUE_PROBE) {
        return FAIL_ALT_INPUT_UNSUPPORTED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.display_query_obs())
    if !scenario_assert.expect_display(effect, syscall.SyscallStatus.Ok, DISPLAY_STATE_STEADY_DEFAULT) {
        return FAIL_ALT_DISPLAY_UNSUPPORTED
    }

    return 0
}