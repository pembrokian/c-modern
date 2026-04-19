import boot
import connection_service
import kernel_dispatch
import lease_service
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall
import workflow/core

const FAIL_EXTERNAL_TICKET_OPEN_0: i32 = 24101
const FAIL_EXTERNAL_TICKET_OPEN_1: i32 = 24135
const FAIL_EXTERNAL_TICKET_OPEN_2: i32 = 24136
const FAIL_EXTERNAL_TICKET_OPEN_3: i32 = 24137
const FAIL_EXTERNAL_TICKET_ISSUE_STATUS: i32 = 24102
const FAIL_EXTERNAL_TICKET_ISSUE_SHAPE: i32 = 24103
const FAIL_EXTERNAL_TICKET_LEASE_STATUS: i32 = 24104
const FAIL_EXTERNAL_TICKET_LEASE_SHAPE: i32 = 24105
const FAIL_EXTERNAL_TICKET_RECEIVE_VALID: i32 = 24106
const FAIL_EXTERNAL_TICKET_EXECUTE_VALID: i32 = 24107
const FAIL_EXTERNAL_TICKET_WAITING_VALID: i32 = 24108
const FAIL_EXTERNAL_TICKET_DONE_VALID: i32 = 24109
const FAIL_EXTERNAL_TICKET_FETCH_VALID: i32 = 24110
const FAIL_EXTERNAL_TICKET_ACK_VALID: i32 = 24111
const FAIL_EXTERNAL_TICKET_REISSUE_STATUS: i32 = 24112
const FAIL_EXTERNAL_TICKET_REISSUE_SHAPE: i32 = 24113
const FAIL_EXTERNAL_TICKET_RECEIVE_CONSUMED: i32 = 24114
const FAIL_EXTERNAL_TICKET_EXECUTE_CONSUMED: i32 = 24115
const FAIL_EXTERNAL_TICKET_WAITING_CONSUMED: i32 = 24116
const FAIL_EXTERNAL_TICKET_DONE_CONSUMED: i32 = 24117
const FAIL_EXTERNAL_TICKET_FETCH_CONSUMED: i32 = 24118
const FAIL_EXTERNAL_TICKET_ACK_CONSUMED: i32 = 24119
const FAIL_EXTERNAL_TICKET_RECEIVE_INVALID: i32 = 24120
const FAIL_EXTERNAL_TICKET_EXECUTE_INVALID: i32 = 24121
const FAIL_EXTERNAL_TICKET_WAITING_INVALID: i32 = 24122
const FAIL_EXTERNAL_TICKET_DONE_INVALID: i32 = 24123
const FAIL_EXTERNAL_TICKET_FETCH_INVALID: i32 = 24124
const FAIL_EXTERNAL_TICKET_ACK_INVALID: i32 = 24125
const FAIL_EXTERNAL_TICKET_STALE_ISSUE_STATUS: i32 = 24126
const FAIL_EXTERNAL_TICKET_STALE_ISSUE_SHAPE: i32 = 24127
const FAIL_EXTERNAL_TICKET_RESTART: i32 = 24128
const FAIL_EXTERNAL_TICKET_RECEIVE_STALE: i32 = 24129
const FAIL_EXTERNAL_TICKET_EXECUTE_STALE: i32 = 24130
const FAIL_EXTERNAL_TICKET_WAITING_STALE: i32 = 24131
const FAIL_EXTERNAL_TICKET_DONE_STALE: i32 = 24132
const FAIL_EXTERNAL_TICKET_FETCH_STALE: i32 = 24133
const FAIL_EXTERNAL_TICKET_ACK_STALE: i32 = 24134

func expect_schedule(effect: service_effect.Effect, id: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == id && payload[1] == workflow_core.WORKFLOW_STATE_WAITING
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

func expect_completion(effect: service_effect.Effect, id: u8, state: u8, restart: u8, generation: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == id && payload[1] == state && payload[2] == restart && payload[3] == generation
}

func expect_two_byte_reply(effect: service_effect.Effect) bool {
    return service_effect.effect_reply_status(effect) == syscall.SyscallStatus.Ok && service_effect.effect_reply_payload_len(effect) == 2
}

func delegated_request(lease_id: u8) u8 {
    return connection_service.CONNECTION_EXTERNAL_TICKET_BASE + lease_id
}

func run_delegated_external_request_probe() i32 {
    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_open(0, 6))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_OPEN_0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_open(1, 6))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_OPEN_1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_open(2, 6))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_OPEN_2
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_open(3, 6))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_OPEN_3
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_ticket_issue())
    if !expect_two_byte_reply(effect) {
        return FAIL_EXTERNAL_TICKET_ISSUE_STATUS
    }
    ticket := service_effect.effect_reply_payload(effect)
    if ticket[0] == 0 || ticket[1] == 0 {
        return FAIL_EXTERNAL_TICKET_ISSUE_SHAPE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_external_ticket(ticket[0], ticket[1]))
    if !expect_two_byte_reply(effect) {
        return FAIL_EXTERNAL_TICKET_LEASE_STATUS
    }
    lease := service_effect.effect_reply_payload(effect)
    if lease[0] == 0 {
        return FAIL_EXTERNAL_TICKET_LEASE_SHAPE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(0, delegated_request(lease[0])))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_RECEIVE_VALID
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_execute(0))
    if !expect_schedule(effect, 1) {
        return FAIL_EXTERNAL_TICKET_EXECUTE_VALID
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXTERNAL_TICKET_WAITING_VALID
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_CONNECTION_TICKET_USED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXTERNAL_TICKET_DONE_VALID
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, 1, workflow_core.WORKFLOW_STATE_CONNECTION_TICKET_USED, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_EXTERNAL_TICKET_FETCH_VALID
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_ACK_VALID
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_external_ticket(ticket[0], ticket[1]))
    if !expect_two_byte_reply(effect) {
        return FAIL_EXTERNAL_TICKET_REISSUE_STATUS
    }
    consumed_lease := service_effect.effect_reply_payload(effect)
    if consumed_lease[0] == 0 {
        return FAIL_EXTERNAL_TICKET_REISSUE_SHAPE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(1, delegated_request(consumed_lease[0])))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_RECEIVE_CONSUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_execute(1))
    if !expect_schedule(effect, 2) {
        return FAIL_EXTERNAL_TICKET_EXECUTE_CONSUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(2))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXTERNAL_TICKET_WAITING_CONSUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(2))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_CONNECTION_TICKET_CONSUMED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXTERNAL_TICKET_DONE_CONSUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, 2, workflow_core.WORKFLOW_STATE_CONNECTION_TICKET_CONSUMED, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_EXTERNAL_TICKET_FETCH_CONSUMED
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_ACK_CONSUMED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(2, delegated_request(99)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_RECEIVE_INVALID
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_execute(2))
    if !expect_schedule(effect, 3) {
        return FAIL_EXTERNAL_TICKET_EXECUTE_INVALID
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(3))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXTERNAL_TICKET_WAITING_INVALID
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(3))
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, 3, workflow_core.WORKFLOW_STATE_CONNECTION_TICKET_INVALID, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_EXTERNAL_TICKET_FETCH_INVALID
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_ACK_INVALID
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_ticket_issue())
    if !expect_two_byte_reply(effect) {
        return FAIL_EXTERNAL_TICKET_STALE_ISSUE_STATUS
    }
    stale_ticket := service_effect.effect_reply_payload(effect)
    if stale_ticket[0] == 0 || stale_ticket[1] == 0 {
        return FAIL_EXTERNAL_TICKET_STALE_ISSUE_SHAPE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_external_ticket(stale_ticket[0], stale_ticket[1]))
    if !expect_two_byte_reply(effect) {
        return FAIL_EXTERNAL_TICKET_STALE_ISSUE_STATUS
    }
    stale_lease := service_effect.effect_reply_payload(effect)

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_TICKET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_TICKET, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_EXTERNAL_TICKET_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(3, delegated_request(stale_lease[0])))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_RECEIVE_STALE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_execute(3))
    if !expect_schedule(effect, 4) {
        return FAIL_EXTERNAL_TICKET_EXECUTE_STALE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(4))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXTERNAL_TICKET_WAITING_STALE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(4))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_CONNECTION_TICKET_STALE, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXTERNAL_TICKET_DONE_STALE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, 4, workflow_core.WORKFLOW_STATE_CONNECTION_TICKET_STALE, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_EXTERNAL_TICKET_FETCH_STALE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_TICKET_ACK_STALE
    }

    return 0
}