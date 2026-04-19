import boot
import kernel_dispatch
import launcher_service
import program_catalog
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_state
import service_topology
import syscall
import update_store_service
import workflow/core

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

func expect_workflow(effect: service_effect.Effect, status: syscall.SyscallStatus, state: u8, restart: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == state && payload[1] == restart
}

func run_launcher_service_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_LAUNCHER_SETUP
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_list())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0) {
        return FAIL_LAUNCHER_LIST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, 0, 0, 0, 0) {
        return FAIL_LAUNCHER_IDENTIFY_EMPTY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_REVIEW_BOARD))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0, 0, 0) {
        return FAIL_LAUNCHER_SELECT_REVIEW_BOARD
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_LAUNCHER_UNINSTALLED_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_STAGE0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_STAGE1
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_STAGE2
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_LAUNCHER_APPLY_SCHEDULE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LAUNCHER_APPLY_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LAUNCHER_APPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, program_catalog.PROGRAM_ID_REVIEW_BOARD) {
        return FAIL_LAUNCHER_IDENTIFY_INSTALLED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_LAUNCHER_INSTALLED_MISMATCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_SELECT_ISSUE_ROLLUP
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, program_catalog.PROGRAM_ID_ISSUE_ROLLUP) {
        return FAIL_LAUNCHER_IDENTIFY_INSTALLED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_FRESH) {
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
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0) {
        return FAIL_LAUNCHER_LIST_AFTER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 0) {
        return FAIL_LAUNCHER_IDENTIFY_AFTER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_SELECT_AFTER_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_RESUMED) {
        return FAIL_LAUNCHER_RESUMED_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_clear())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_RESTAGE0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(44))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_RESTAGE0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(55))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_RESTAGE1
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(66))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_RESTAGE2
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(8, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_REMANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_LAUNCHER_REAPPLY_SCHEDULE
    }
    apply_id = service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LAUNCHER_REAPPLY_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LAUNCHER_REAPPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LAUNCHER))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LAUNCHER, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_LAUNCHER_RESTART_INVALIDATED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_SELECT_AFTER_INVALIDATION
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_INVALIDATED) {
        return FAIL_LAUNCHER_INVALIDATED_LAUNCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_UPDATE_STORE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_UPDATE_STORE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_LAUNCHER_STORE_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_identify())
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 8, 3, program_catalog.PROGRAM_ID_ISSUE_ROLLUP) {
        return FAIL_LAUNCHER_IDENTIFY_AFTER_STORE_RESTART
    }
    return 0
}

func run_launcher_manifest_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_LAUNCHER_MANIFEST_SETUP
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_REVIEW_BOARD))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0, 0, 0) {
        return FAIL_LAUNCHER_MANIFEST_WRONG_PROGRAM
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(0))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_LAUNCHER_MANIFEST_WRONG_PROGRAM
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_MANIFEST_STAGE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_MANIFEST_STAGE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_MANIFEST_STAGE2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_LAUNCHER_MANIFEST_RECORD
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_LAUNCHER_MANIFEST_APPLY_SCHEDULE
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LAUNCHER_MANIFEST_APPLY_WAITING
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(apply_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_LAUNCHER_MANIFEST_APPLY_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_ISSUE_ROLLUP))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 0, 0, 0) {
        return FAIL_LAUNCHER_MANIFEST_SELECT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(0))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 11) {
        return FAIL_LAUNCHER_MANIFEST_BYTE0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(1))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 22) {
        return FAIL_LAUNCHER_MANIFEST_BYTE1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_manifest(2))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 33) {
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
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_ISSUE_ROLLUP, 7, 3, 22) {
        return FAIL_LAUNCHER_MANIFEST_AFTER_STORE_RESTART
    }

    return 0
}
