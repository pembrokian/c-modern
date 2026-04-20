import boot
import input_event
import journal_service
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

const FAIL_REVIEW_BOARD_PERSIST_SETUP: i32 = 28701
const FAIL_REVIEW_BOARD_PERSIST_SELECT0: i32 = 28702
const FAIL_REVIEW_BOARD_PERSIST_LAUNCH0: i32 = 28703
const FAIL_REVIEW_BOARD_PERSIST_INPUT0: i32 = 28704
const FAIL_REVIEW_BOARD_PERSIST_INPUT1: i32 = 28705
const FAIL_REVIEW_BOARD_PERSIST_INPUT2: i32 = 28706
const FAIL_REVIEW_BOARD_PERSIST_INPUT3: i32 = 28707
const FAIL_REVIEW_BOARD_PERSIST_INPUT4: i32 = 28708
const FAIL_REVIEW_BOARD_PERSIST_INPUT5: i32 = 28709
const FAIL_REVIEW_BOARD_PERSIST_INPUT6: i32 = 28710
const FAIL_REVIEW_BOARD_PERSIST_INPUT7: i32 = 28711
const FAIL_REVIEW_BOARD_PERSIST_DISPLAY0: i32 = 28712
const FAIL_REVIEW_BOARD_PERSIST_SELECT1: i32 = 28713
const FAIL_REVIEW_BOARD_PERSIST_LAUNCH1: i32 = 28714
const FAIL_REVIEW_BOARD_PERSIST_DISPLAY1: i32 = 28715

const DISPLAY_STATE_FOCUS_ESCALATE_STATUS: [4]u8 = [4]u8{ 70, 69, 70, 83 }

func display_query_obs() syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.DISPLAY_ENDPOINT_ID, source_pid: 1, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: { 0, 0, 0, 0 } }
}

func expect_display(effect: service_effect.Effect, cells: [4]u8) bool {
    return scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, cells[0], cells[1], cells[2], cells[3])
}

func expect_delivered(effect: service_effect.Effect, value: u8) bool {
    return scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_REVIEW_BOARD, input_event.INPUT_ROUTE_DELIVERED, input_event.INPUT_EVENT_KEY, value)
}

func launch_review_board(state: *boot.KernelBootState, fail_select: i32, fail_launch: i32) i32 {
    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_select(program_catalog.PROGRAM_ID_REVIEW_BOARD))
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 2, program_catalog.PROGRAM_ID_REVIEW_BOARD, 0, 0, 0) {
        return fail_select
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_launcher_launch())
    if !scenario_assert.expect_reply(effect, syscall.SyscallStatus.Ok, 4, program_catalog.PROGRAM_ID_REVIEW_BOARD, program_catalog.PROGRAM_KIND_CODE_HOSTED_EXE, 1, launcher_service.LAUNCHER_RESUME_FRESH) {
        return fail_launch
    }
    return 0
}

func input_key(state: *boot.KernelBootState, key: u8, fail: i32) i32 {
    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_input_key(key))
    if !expect_delivered(effect, key) {
        return fail
    }
    return 0
}

func run_review_board_state_persistence_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_REVIEW_BOARD_PERSIST_SETUP
    }
    if !journal_service.journal_persist(journal_service.journal_init(service_topology.JOURNAL_SLOT.pid, 1)) {
        return FAIL_REVIEW_BOARD_PERSIST_SETUP
    }

    state := boot.kernel_init()

    result := launch_review_board(&state, FAIL_REVIEW_BOARD_PERSIST_SELECT0, FAIL_REVIEW_BOARD_PERSIST_LAUNCH0)
    if result != 0 {
        return result
    }

    result = input_key(&state, review_board_ui.REVIEW_BOARD_KEY_OPEN, FAIL_REVIEW_BOARD_PERSIST_INPUT0)
    if result != 0 {
        return result
    }
    result = input_key(&state, review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION, FAIL_REVIEW_BOARD_PERSIST_INPUT1)
    if result != 0 {
        return result
    }
    result = input_key(&state, review_board_ui.REVIEW_BOARD_KEY_FOCUS, FAIL_REVIEW_BOARD_PERSIST_INPUT2)
    if result != 0 {
        return result
    }
    result = input_key(&state, review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION, FAIL_REVIEW_BOARD_PERSIST_INPUT3)
    if result != 0 {
        return result
    }
    result = input_key(&state, review_board_ui.REVIEW_BOARD_KEY_URGENT, FAIL_REVIEW_BOARD_PERSIST_INPUT4)
    if result != 0 {
        return result
    }
    result = input_key(&state, review_board_ui.REVIEW_BOARD_KEY_URGENT, FAIL_REVIEW_BOARD_PERSIST_INPUT5)
    if result != 0 {
        return result
    }
    result = input_key(&state, review_board_ui.REVIEW_BOARD_KEY_URGENT, FAIL_REVIEW_BOARD_PERSIST_INPUT6)
    if result != 0 {
        return result
    }
    result = input_key(&state, review_board_ui.REVIEW_BOARD_KEY_TOGGLE_REGION, FAIL_REVIEW_BOARD_PERSIST_INPUT7)
    if result != 0 {
        return result
    }

    effect := kernel_dispatch.kernel_dispatch_step(&state, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_FOCUS_ESCALATE_STATUS) {
        return FAIL_REVIEW_BOARD_PERSIST_DISPLAY0
    }

    restarted := boot.kernel_init()

    result = launch_review_board(&restarted, FAIL_REVIEW_BOARD_PERSIST_SELECT1, FAIL_REVIEW_BOARD_PERSIST_LAUNCH1)
    if result != 0 {
        return result
    }

    effect = kernel_dispatch.kernel_dispatch_step(&restarted, display_query_obs())
    if !expect_display(effect, DISPLAY_STATE_FOCUS_ESCALATE_STATUS) {
        return FAIL_REVIEW_BOARD_PERSIST_DISPLAY1
    }

    return 0
}