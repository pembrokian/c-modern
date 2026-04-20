import kernel_dispatch
import launcher_service
import program_catalog
import scenario_assert
import scenario_helpers
import scenario_transport
import serial_protocol
import service_effect
import syscall

const FAIL_LAUNCHER_DEMO_SETUP: i32 = 27201
const FAIL_LAUNCHER_DEMO_STATUS_EMPTY: i32 = 27202
const FAIL_LAUNCHER_DEMO_STAGE0: i32 = 27203
const FAIL_LAUNCHER_DEMO_STAGE1: i32 = 27204
const FAIL_LAUNCHER_DEMO_STAGE2: i32 = 27205
const FAIL_LAUNCHER_DEMO_MANIFEST: i32 = 27206
const FAIL_LAUNCHER_DEMO_APPLY_SCHEDULE: i32 = 27207
const FAIL_LAUNCHER_DEMO_APPLY_WAITING: i32 = 27208
const FAIL_LAUNCHER_DEMO_APPLY_DONE: i32 = 27209
const FAIL_LAUNCHER_DEMO_STATUS_ACTIVATED: i32 = 27210
const FAIL_LAUNCHER_DEMO_SELECT: i32 = 27211
const FAIL_LAUNCHER_DEMO_LAUNCH_FRESH: i32 = 27212
const FAIL_LAUNCHER_DEMO_STATUS_FRESH: i32 = 27213
const FAIL_LAUNCHER_DEMO_RESTART: i32 = 27214
const FAIL_LAUNCHER_DEMO_STATUS_AFTER_RESTART: i32 = 27215
const FAIL_LAUNCHER_DEMO_LAUNCH_RESUMED: i32 = 27216
const FAIL_LAUNCHER_DEMO_STATUS_RESUMED: i32 = 27217
const FAIL_LAUNCHER_DEMO_CLEAR: i32 = 27218
const FAIL_LAUNCHER_DEMO_RESTAGE0: i32 = 27219
const FAIL_LAUNCHER_DEMO_RESTAGE1: i32 = 27220
const FAIL_LAUNCHER_DEMO_RESTAGE2: i32 = 27221
const FAIL_LAUNCHER_DEMO_REMANIFEST: i32 = 27222
const FAIL_LAUNCHER_DEMO_REAPPLY_SCHEDULE: i32 = 27223
const FAIL_LAUNCHER_DEMO_REAPPLY_WAITING: i32 = 27224
const FAIL_LAUNCHER_DEMO_REAPPLY_DONE: i32 = 27225
const FAIL_LAUNCHER_DEMO_RESTART_INVALIDATED: i32 = 27226
const FAIL_LAUNCHER_DEMO_STATUS_REACTIVATED: i32 = 27227
const FAIL_LAUNCHER_DEMO_LAUNCH_INVALIDATED: i32 = 27228
const FAIL_LAUNCHER_DEMO_STATUS_INVALIDATED: i32 = 27229

func run_launcher_installed_workflow_demo_probe() i32 {
    setup := scenario_helpers.scenario_setup_clean_kernel()
    if !setup.ok {
        return FAIL_LAUNCHER_DEMO_SETUP
    }

    state := setup.state

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, 0, 0, 0, launcher_service.LAUNCHER_STATUS_NONE) {
        return FAIL_LAUNCHER_DEMO_STATUS_EMPTY
    }

    result := scenario_helpers.scenario_apply_issue_rollup_update(&state, 7, 11, 22, 33, FAIL_LAUNCHER_DEMO_STAGE0)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 0, launcher_service.LAUNCHER_STATUS_ACTIVATED) {
        return FAIL_LAUNCHER_DEMO_STATUS_ACTIVATED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_DEMO_SELECT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_FRESH) {
        return FAIL_LAUNCHER_DEMO_LAUNCH_FRESH
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, launcher_service.LAUNCHER_RESUME_FRESH) {
        return FAIL_LAUNCHER_DEMO_STATUS_FRESH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_LAUNCHER_DEMO_RESTART
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 0, launcher_service.LAUNCHER_STATUS_ACTIVATED) {
        return FAIL_LAUNCHER_DEMO_STATUS_AFTER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_DEMO_SELECT
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_RESUMED) {
        return FAIL_LAUNCHER_DEMO_LAUNCH_RESUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, launcher_service.LAUNCHER_RESUME_RESUMED) {
        return FAIL_LAUNCHER_DEMO_STATUS_RESUMED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_clear())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_DEMO_CLEAR
    }
    result = scenario_helpers.scenario_apply_issue_rollup_update(&state, 8, 44, 55, 66, FAIL_LAUNCHER_DEMO_RESTAGE0)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_LAUNCHER_DEMO_RESTART_INVALIDATED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 8, 0, launcher_service.LAUNCHER_STATUS_ACTIVATED) {
        return FAIL_LAUNCHER_DEMO_STATUS_REACTIVATED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_DEMO_SELECT
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_INVALIDATED) {
        return FAIL_LAUNCHER_DEMO_LAUNCH_INVALIDATED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_status())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 8, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, launcher_service.LAUNCHER_RESUME_INVALIDATED) {
        return FAIL_LAUNCHER_DEMO_STATUS_INVALIDATED
    }

    return 0
}