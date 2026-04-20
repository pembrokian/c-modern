import launcher_service
import program_catalog
import scenario_assert
import scenario_helpers
import scenario_transport
import serial_protocol
import service_effect
import service_state
import syscall

const FAIL_LAUNCHER_SETUP: i32 = 26901
const FAIL_LAUNCHER_LIST: i32 = 26902
const FAIL_LAUNCHER_IDENTIFY_EMPTY: i32 = 26903
const FAIL_LAUNCHER_SELECT_REVIEW_BOARD: i32 = 26904
const FAIL_LAUNCHER_UNINSTALLED_LAUNCH: i32 = 26905
const FAIL_LAUNCHER_STAGE0: i32 = 26906
const FAIL_LAUNCHER_STAGE1: i32 = 26907
const FAIL_LAUNCHER_STAGE2: i32 = 26908
const FAIL_LAUNCHER_MANIFEST: i32 = 26909
const FAIL_LAUNCHER_APPLY_SCHEDULE: i32 = 26910
const FAIL_LAUNCHER_APPLY_WAITING: i32 = 26911
const FAIL_LAUNCHER_APPLY_DONE: i32 = 26912
const FAIL_LAUNCHER_IDENTIFY_INSTALLED: i32 = 26913
const FAIL_LAUNCHER_INSTALLED_MISMATCH: i32 = 26914
const FAIL_LAUNCHER_SELECT_ISSUE_ROLLUP: i32 = 26915
const FAIL_LAUNCHER_LAUNCH: i32 = 26916
const FAIL_LAUNCHER_STATE: i32 = 26917
const FAIL_LAUNCHER_RESTART: i32 = 26918
const FAIL_LAUNCHER_LIST_AFTER_RESTART: i32 = 26919
const FAIL_LAUNCHER_IDENTIFY_AFTER_RESTART: i32 = 26920
const FAIL_LAUNCHER_SELECT_AFTER_RESTART: i32 = 26921
const FAIL_LAUNCHER_RESUMED_LAUNCH: i32 = 26922
const FAIL_LAUNCHER_RESTAGE0: i32 = 26923
const FAIL_LAUNCHER_RESTAGE1: i32 = 26924
const FAIL_LAUNCHER_RESTAGE2: i32 = 26925
const FAIL_LAUNCHER_REMANIFEST: i32 = 26926
const FAIL_LAUNCHER_REAPPLY_SCHEDULE: i32 = 26927
const FAIL_LAUNCHER_REAPPLY_WAITING: i32 = 26928
const FAIL_LAUNCHER_REAPPLY_DONE: i32 = 26929
const FAIL_LAUNCHER_RESTART_INVALIDATED: i32 = 26930
const FAIL_LAUNCHER_SELECT_AFTER_INVALIDATION: i32 = 26931
const FAIL_LAUNCHER_INVALIDATED_LAUNCH: i32 = 26932
const FAIL_LAUNCHER_STORE_RESTART: i32 = 26933
const FAIL_LAUNCHER_IDENTIFY_AFTER_STORE_RESTART: i32 = 26934
const FAIL_LAUNCHER_CLEAR_UPDATE: i32 = 26935

func run_launcher_service_probe() i32 {
    setup := scenario_helpers.scenario_setup_clean_kernel()
    if !setup.ok {
        return FAIL_LAUNCHER_SETUP
    }

    state := setup.state
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_list())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0) {
        return FAIL_LAUNCHER_LIST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, 0, 0, 0, 0) {
        return FAIL_LAUNCHER_IDENTIFY_EMPTY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_REVIEW_BOARD))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0, 0, 0) {
        return FAIL_LAUNCHER_SELECT_REVIEW_BOARD
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_REVIEW_BOARD, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_FRESH) {
        return FAIL_LAUNCHER_UNINSTALLED_LAUNCH
    }
    if launcher_service.launcher_foreground_id(state.launcher.state) != program_catalog.PROGRAM_ID_REVIEW_BOARD {
        return FAIL_LAUNCHER_UNINSTALLED_LAUNCH
    }

    result := scenario_helpers.scenario_apply_issue_rollup_update(&state, 7, 11, 22, 33, FAIL_LAUNCHER_STAGE0)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, program_catalog.PROGRAM_ID_REVIEW_BOARD) {
        return FAIL_LAUNCHER_IDENTIFY_INSTALLED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_REVIEW_BOARD, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 2, launcher_service.LAUNCHER_RESUME_FRESH) {
        return FAIL_LAUNCHER_INSTALLED_MISMATCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0, 0) {
        return FAIL_LAUNCHER_SELECT_ISSUE_ROLLUP
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, program_catalog.PROGRAM_ID_ISSUE_ROLLUP) {
        return FAIL_LAUNCHER_IDENTIFY_INSTALLED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 3, launcher_service.LAUNCHER_RESUME_FRESH) {
        return FAIL_LAUNCHER_LAUNCH
    }

    if launcher_service.launcher_foreground_id(state.launcher.state) != program_catalog.PROGRAM_ID_ISSUE_ROLLUP {
        return FAIL_LAUNCHER_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_state(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_service_state(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, service_state.STATE_CLASS_ORDINARY, service_state.STATE_MODE_RESET, service_state.STATE_PARTICIPATION_NONE, service_state.STATE_POLICY_CLEAR, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 1) {
        return FAIL_LAUNCHER_STATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_LAUNCHER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_list())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0) {
        return FAIL_LAUNCHER_LIST_AFTER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 0) {
        return FAIL_LAUNCHER_IDENTIFY_AFTER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_SELECT_AFTER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_RESUMED) {
        return FAIL_LAUNCHER_RESUMED_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_clear())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_CLEAR_UPDATE
    }

    result = scenario_helpers.scenario_apply_issue_rollup_update(&state, 8, 44, 55, 66, FAIL_LAUNCHER_RESTAGE0)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_LAUNCHER_RESTART_INVALIDATED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_SELECT_AFTER_INVALIDATION
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_INVALIDATED) {
        return FAIL_LAUNCHER_INVALIDATED_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_UPDATE_STORE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_UPDATE_STORE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_LAUNCHER_STORE_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 8, 3, program_catalog.PROGRAM_ID_ISSUE_ROLLUP) {
        return FAIL_LAUNCHER_IDENTIFY_AFTER_STORE_RESTART
    }
    return 0
}