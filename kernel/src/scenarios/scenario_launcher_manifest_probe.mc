import kernel_dispatch
import program_catalog
import scenario_assert
import scenario_helpers
import scenario_transport
import serial_protocol
import service_effect
import syscall

const FAIL_LAUNCHER_MANIFEST_SETUP: i32 = 27101
const FAIL_LAUNCHER_MANIFEST_WRONG_PROGRAM: i32 = 27102
const FAIL_LAUNCHER_MANIFEST_STAGE0: i32 = 27103
const FAIL_LAUNCHER_MANIFEST_STAGE1: i32 = 27104
const FAIL_LAUNCHER_MANIFEST_STAGE2: i32 = 27105
const FAIL_LAUNCHER_MANIFEST_RECORD: i32 = 27106
const FAIL_LAUNCHER_MANIFEST_APPLY_SCHEDULE: i32 = 27107
const FAIL_LAUNCHER_MANIFEST_APPLY_WAITING: i32 = 27108
const FAIL_LAUNCHER_MANIFEST_APPLY_DONE: i32 = 27109
const FAIL_LAUNCHER_MANIFEST_SELECT: i32 = 27110
const FAIL_LAUNCHER_MANIFEST_BYTE0: i32 = 27111
const FAIL_LAUNCHER_MANIFEST_BYTE1: i32 = 27112
const FAIL_LAUNCHER_MANIFEST_BYTE2: i32 = 27113
const FAIL_LAUNCHER_MANIFEST_RANGE: i32 = 27114
const FAIL_LAUNCHER_MANIFEST_STORE_RESTART: i32 = 27115
const FAIL_LAUNCHER_MANIFEST_AFTER_STORE_RESTART: i32 = 27116

func run_launcher_manifest_probe() i32 {
    setup := scenario_helpers.scenario_setup_clean_kernel()
    if !setup.ok {
        return FAIL_LAUNCHER_MANIFEST_SETUP
    }

    state := setup.state

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_REVIEW_BOARD))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0, 0, 0) {
        return FAIL_LAUNCHER_MANIFEST_WRONG_PROGRAM
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(0))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_LAUNCHER_MANIFEST_WRONG_PROGRAM
    }

    result := scenario_helpers.scenario_apply_issue_rollup_update(&state, 7, 11, 22, 33, FAIL_LAUNCHER_MANIFEST_STAGE0)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_MANIFEST_SELECT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(0))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 11) {
        return FAIL_LAUNCHER_MANIFEST_BYTE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(1))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 22) {
        return FAIL_LAUNCHER_MANIFEST_BYTE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(2))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 33) {
        return FAIL_LAUNCHER_MANIFEST_BYTE2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_LAUNCHER_MANIFEST_RANGE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_UPDATE_STORE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_UPDATE_STORE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_LAUNCHER_MANIFEST_STORE_RESTART
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(1))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 22) {
        return FAIL_LAUNCHER_MANIFEST_AFTER_STORE_RESTART
    }

    return 0
}