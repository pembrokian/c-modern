import boot
import kernel_dispatch
import launcher_service
import program_catalog
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_state
import syscall

const FAIL_LAUNCHER_LIST: i32 = 26801
const FAIL_LAUNCHER_SELECT: i32 = 26802
const FAIL_LAUNCHER_LAUNCH: i32 = 26803
const FAIL_LAUNCHER_STATE: i32 = 26804
const FAIL_LAUNCHER_RESTART: i32 = 26805

func expect_reply(effect: service_effect.Effect, status: syscall.SyscallStatus, len: usize, b0: u8, b1: u8, b2: u8, b3: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != len {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == b0 && payload[1] == b1 && payload[2] == b2 && payload[3] == b3
}

func run_launcher_service_probe() i32 {
    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_list())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0) {
        return FAIL_LAUNCHER_LIST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_REVIEW_BOARD))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0, 0, 0) {
        return FAIL_LAUNCHER_SELECT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 3, program_catalog.PROGRAM_ID_REVIEW_BOARD, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, 0) {
        return FAIL_LAUNCHER_LAUNCH
    }

    if launcher_service.launcher_foreground_id(state.launcher.state) != program_catalog.PROGRAM_ID_REVIEW_BOARD {
        return FAIL_LAUNCHER_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_state(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_service_state(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, service_state.STATE_CLASS_ORDINARY, service_state.STATE_MODE_RESET, service_state.STATE_PARTICIPATION_NONE, service_state.STATE_POLICY_CLEAR, program_catalog.PROGRAM_ID_REVIEW_BOARD, 1) {
        return FAIL_LAUNCHER_STATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_LAUNCHER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_list())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0) {
        return FAIL_LAUNCHER_RESTART
    }
    return 0
}
