import boot
import journal_service
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall

const FAIL_JOURNAL_CLEAR_A: i32 = 2300
const FAIL_JOURNAL_CLEAR_B: i32 = 2301
const FAIL_JOURNAL_APPEND_0: i32 = 2302
const FAIL_JOURNAL_APPEND_1: i32 = 2303
const FAIL_JOURNAL_REPLAY_0: i32 = 2304
const FAIL_JOURNAL_RESTART: i32 = 2305
const FAIL_JOURNAL_REPLAY_1: i32 = 2306
const FAIL_JOURNAL_APPEND_2: i32 = 2307
const FAIL_JOURNAL_REPLAY_2: i32 = 2308
const FAIL_JOURNAL_STATE_COUNT: i32 = 2309
const FAIL_JOURNAL_STATE_VALUE_0: i32 = 2310
const FAIL_JOURNAL_STATE_VALUE_1: i32 = 2311

func expect_replay(effect: service_effect.Effect, count: usize, first: u8, second: u8, third: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != count {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    if count > 0 && payload[0] != first {
        return false
    }
    if count > 1 && payload[1] != second {
        return false
    }
    if count > 2 && payload[2] != third {
        return false
    }
    return true
}

func run_journal_service_probe() i32 {
    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_journal_clear(journal_service.JOURNAL_LANE_A))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_JOURNAL_CLEAR_A
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_journal_clear(journal_service.JOURNAL_LANE_B))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_JOURNAL_CLEAR_B
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_journal_append(journal_service.JOURNAL_LANE_A, 11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_JOURNAL_APPEND_0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_journal_append(journal_service.JOURNAL_LANE_A, 12))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_JOURNAL_APPEND_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_journal_replay(journal_service.JOURNAL_LANE_A))
    if !expect_replay(effect, 2, 11, 12, 0) {
        return FAIL_JOURNAL_REPLAY_0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_JOURNAL))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_JOURNAL, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_JOURNAL_RESTART
    }
    if journal_service.journal_count(state.journal.state) != 2 {
        return FAIL_JOURNAL_STATE_COUNT
    }
    if state.journal.state.lane0.data[0] != 11 {
        return FAIL_JOURNAL_STATE_VALUE_0
    }
    if state.journal.state.lane0.data[1] != 12 {
        return FAIL_JOURNAL_STATE_VALUE_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_journal_replay(journal_service.JOURNAL_LANE_A))
    if !expect_replay(effect, 2, 11, 12, 0) {
        return FAIL_JOURNAL_REPLAY_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_journal_append(journal_service.JOURNAL_LANE_A, 13))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_JOURNAL_APPEND_2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_journal_replay(journal_service.JOURNAL_LANE_A))
    if !expect_replay(effect, 3, 11, 12, 13) {
        return FAIL_JOURNAL_REPLAY_2
    }

    return 0
}