import boot
import input_event
import kernel_dispatch
import launcher_service
import program_catalog
import review_board_ui
import scenario_assert
import scenario_transport
import service_effect
import service_topology
import syscall
import update_store_service

const FAIL_REVIEW_BOARD_SETUP: i32 = 28601
const FAIL_REVIEW_BOARD_SELECT: i32 = 28602
const FAIL_REVIEW_BOARD_LAUNCH: i32 = 28603
const FAIL_REVIEW_BOARD_DISPLAY_INIT: i32 = 28604
const FAIL_REVIEW_BOARD_INPUT_OPEN: i32 = 28605
const FAIL_REVIEW_BOARD_DISPLAY_PAUSE: i32 = 28606
const FAIL_REVIEW_BOARD_INPUT_STATUS: i32 = 28607
const FAIL_REVIEW_BOARD_DISPLAY_STATUS: i32 = 28608
const FAIL_REVIEW_BOARD_INPUT_MODE: i32 = 28609
const FAIL_REVIEW_BOARD_DISPLAY_MODE: i32 = 28610
const FAIL_REVIEW_BOARD_INPUT_BODY: i32 = 28611
const FAIL_REVIEW_BOARD_DISPLAY_BODY: i32 = 28612
const FAIL_REVIEW_BOARD_INPUT_URGENT0: i32 = 28613
const FAIL_REVIEW_BOARD_INPUT_URGENT1: i32 = 28614
const FAIL_REVIEW_BOARD_INPUT_URGENT2: i32 = 28615
const FAIL_REVIEW_BOARD_DISPLAY_ESCALATE: i32 = 28616

const DISPLAY_STATE_AUDIT_STABLE_BODY: [4]u8 = [4]u8{ 65, 83, 65, 66 }
const DISPLAY_STATE_AUDIT_PAUSE_BODY: [4]u8 = [4]u8{ 65, 80, 65, 66 }
const DISPLAY_STATE_AUDIT_PAUSE_STATUS: [4]u8 = [4]u8{ 65, 80, 65, 83 }
const DISPLAY_STATE_FOCUS_NORMAL_STATUS: [4]u8 = [4]u8{ 70, 78, 70, 83 }
const DISPLAY_STATE_FOCUS_NORMAL_BODY: [4]u8 = [4]u8{ 70, 78, 70, 66 }
const DISPLAY_STATE_FOCUS_ESCALATE_BODY: [4]u8 = [4]u8{ 70, 69, 70, 66 }

func display_query_obs() syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.DISPLAY_ENDPOINT_ID, source_pid: 1, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: { 0, 0, 0, 0 } }
}

func expect_display(effect: service_effect.Effect, cells: [4]u8) bool {
    return scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, cells[0], cells[1], cells[2], cells[3])
}

func expect_delivered(effect: service_effect.Effect, value: u8) bool {
    return scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_REVIEW_BOARD, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, value)
}

func run_review_board_ui_proof_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_REVIEW_BOARD_SETUP
    }

    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_REVIEW_BOARD))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0, 0, 0) {
        return FAIL_REVIEW_BOARD_SELECT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_REVIEW_BOARD, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_FRESH) {
        return FAIL_REVIEW_BOARD_LAUNCH
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_AUDIT_STABLE_BODY) {
        return FAIL_REVIEW_BOARD_DISPLAY_INIT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(review_board_ui.REVIEW_BOARD_KEY_OPEN))
    if !expect_delivered(effect, review_board_ui.REVIEW_BOARD_KEY_OPEN) {
        return FAIL_REVIEW_BOARD_INPUT_OPEN
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_AUDIT_PAUSE_BODY) {
        return FAIL_REVIEW_BOARD_DISPLAY_PAUSE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION))
    if !expect_delivered(effect, review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION) {
        return FAIL_REVIEW_BOARD_INPUT_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_AUDIT_PAUSE_STATUS) {
        return FAIL_REVIEW_BOARD_DISPLAY_STATUS
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(review_board_ui.REVIEW_BOARD_KEY_FOCUS))
    if !expect_delivered(effect, review_board_ui.REVIEW_BOARD_KEY_FOCUS) {
        return FAIL_REVIEW_BOARD_INPUT_MODE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_FOCUS_NORMAL_STATUS) {
        return FAIL_REVIEW_BOARD_DISPLAY_MODE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION))
    if !expect_delivered(effect, review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION) {
        return FAIL_REVIEW_BOARD_INPUT_BODY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_FOCUS_NORMAL_BODY) {
        return FAIL_REVIEW_BOARD_DISPLAY_BODY
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(review_board_ui.REVIEW_BOARD_KEY_URGENT))
    if !expect_delivered(effect, review_board_ui.REVIEW_BOARD_KEY_URGENT) {
        return FAIL_REVIEW_BOARD_INPUT_URGENT0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(review_board_ui.REVIEW_BOARD_KEY_URGENT))
    if !expect_delivered(effect, review_board_ui.REVIEW_BOARD_KEY_URGENT) {
        return FAIL_REVIEW_BOARD_INPUT_URGENT1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_input_key(review_board_ui.REVIEW_BOARD_KEY_URGENT))
    if !expect_delivered(effect, review_board_ui.REVIEW_BOARD_KEY_URGENT) {
        return FAIL_REVIEW_BOARD_INPUT_URGENT2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_FOCUS_ESCALATE_BODY) {
        return FAIL_REVIEW_BOARD_DISPLAY_ESCALATE
    }

    return 0
}