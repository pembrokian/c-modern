import boot
import connection_service
import completion_mailbox_service
import kernel_dispatch
import primitives
import scenario_transport
import service_effect
import syscall
import workflow/core

const FAIL_FILL_SCHEDULE_BASE: i32 = 24001
const FAIL_FILL_RUNNING_BASE: i32 = 24005
const FAIL_FILL_DONE_BASE: i32 = 24009
const FAIL_EXTERNAL_OPEN: i32 = 24013
const FAIL_EXTERNAL_RECEIVE_FIRST: i32 = 24014
const FAIL_EXTERNAL_ADMIT: i32 = 24015
const FAIL_EXTERNAL_WAITING: i32 = 24016
const FAIL_EXTERNAL_RUNNING: i32 = 24017
const FAIL_EXTERNAL_STALL_FIRST_STATUS: i32 = 24018
const FAIL_EXTERNAL_STALL_FIRST_STATE: i32 = 24019
const FAIL_EXTERNAL_STALL_FIRST_OUTCOME: i32 = 24020
const FAIL_EXTERNAL_RECEIVE_BLOCKED_STATUS: i32 = 24021
const FAIL_EXTERNAL_RECEIVE_BLOCKED_STATE: i32 = 24022
const FAIL_EXTERNAL_STALL_SECOND_STATUS: i32 = 24023
const FAIL_EXTERNAL_STALL_SECOND_STATE: i32 = 24024
const FAIL_EXTERNAL_RELEASE_ACK: i32 = 24025
const FAIL_EXTERNAL_RECOVER: i32 = 24026
const FAIL_EXTERNAL_RECOVER_STATE: i32 = 24027
const FAIL_EXTERNAL_RECEIVE_SECOND: i32 = 24028
const FAIL_EXTERNAL_COUNT_STATUS: i32 = 24029
const FAIL_EXTERNAL_COUNT_LEN: i32 = 24030
const FAIL_EXTERNAL_FETCH_STATUS: i32 = 24031
const FAIL_EXTERNAL_FETCH_ID: i32 = 24032
const FAIL_EXTERNAL_FETCH_STATE: i32 = 24033
const FAIL_EXTERNAL_FETCH_RESTART: i32 = 24034
const FAIL_EXTERNAL_FETCH_GENERATION: i32 = 24035
const FAIL_EXTERNAL_FINAL_ACK: i32 = 24036
const EXTERNAL_RUNNING_TASK_ID: u8 = 5

func expect_schedule(reply: service_effect.Effect, id: u8) bool {
    if service_effect.effect_reply_status(reply) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(reply) != 2 {
        return false
    }
    payload := service_effect.effect_reply_payload(reply)
    return payload[0] == id && payload[1] == workflow_core.WORKFLOW_STATE_WAITING
}

func expect_workflow(reply: service_effect.Effect, status: syscall.SyscallStatus, state: u8, restart: u8, value: u8) bool {
    if service_effect.effect_reply_status(reply) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(reply) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(reply)
    return payload[0] == state && payload[1] == restart && payload[2] == value
}

func expect_reply_shape(reply: service_effect.Effect, status: syscall.SyscallStatus, payload_len: usize) bool {
    return service_effect.effect_reply_status(reply) == status && service_effect.effect_reply_payload_len(reply) == payload_len
}

func run_connection_completion_pressure_probe() i32 {
    state := boot.kernel_init()
    reply: service_effect.Effect = service_effect.effect_none()
    payload: [4]u8 = primitives.zero_payload()

    for id in 11..15 {
        reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(u8(id), 1))
        if service_effect.effect_reply_status(reply) != syscall.SyscallStatus.Ok {
            return FAIL_FILL_SCHEDULE_BASE + (id - 11)
        }

        reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(u8(id)))
        if !expect_workflow(reply, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_RUNNING, workflow_core.WORKFLOW_RESTART_NONE, u8(id - 10)) {
            return FAIL_FILL_RUNNING_BASE + (id - 11)
        }

        reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(u8(id)))
        if !expect_workflow(reply, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_DONE, workflow_core.WORKFLOW_RESTART_NONE, 0) {
            return FAIL_FILL_DONE_BASE + (id - 11)
        }
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_open(1, 6))
    if service_effect.effect_reply_status(reply) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_OPEN
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(1, 21))
    if service_effect.effect_reply_status(reply) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_RECEIVE_FIRST
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_execute(1))
    if !expect_schedule(reply, 1) {
        return FAIL_EXTERNAL_ADMIT
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(reply, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE, 0) {
        return FAIL_EXTERNAL_WAITING
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(reply, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_RUNNING, workflow_core.WORKFLOW_RESTART_NONE, EXTERNAL_RUNNING_TASK_ID) {
        return FAIL_EXTERNAL_RUNNING
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(reply, syscall.SyscallStatus.Exhausted, workflow_core.WORKFLOW_STATE_DELIVERING, workflow_core.WORKFLOW_RESTART_NONE, workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED) {
        return FAIL_EXTERNAL_STALL_FIRST_STATUS
    }
    payload = service_effect.effect_reply_payload(reply)
    if payload[0] != workflow_core.WORKFLOW_STATE_DELIVERING {
        return FAIL_EXTERNAL_STALL_FIRST_STATE
    }
    if payload[2] != workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED {
        return FAIL_EXTERNAL_STALL_FIRST_OUTCOME
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(1, 22))
    if !expect_reply_shape(reply, syscall.SyscallStatus.InvalidArgument, 1) {
        return FAIL_EXTERNAL_RECEIVE_BLOCKED_STATUS
    }
    payload = service_effect.effect_reply_payload(reply)
    if payload[0] != connection_service.CONNECTION_STATE_RECEIVED {
        return FAIL_EXTERNAL_RECEIVE_BLOCKED_STATE
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(reply, syscall.SyscallStatus.WouldBlock, workflow_core.WORKFLOW_STATE_DELIVERING, workflow_core.WORKFLOW_RESTART_NONE, workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED) {
        return FAIL_EXTERNAL_STALL_SECOND_STATUS
    }
    payload = service_effect.effect_reply_payload(reply)
    if payload[0] != workflow_core.WORKFLOW_STATE_DELIVERING {
        return FAIL_EXTERNAL_STALL_SECOND_STATE
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(reply) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_RELEASE_ACK
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(reply, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED, workflow_core.WORKFLOW_RESTART_NONE, 0) {
        return FAIL_EXTERNAL_RECOVER
    }
    payload = service_effect.effect_reply_payload(reply)
    if payload[0] != workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED {
        return FAIL_EXTERNAL_RECOVER_STATE
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(1, 22))
    if service_effect.effect_reply_status(reply) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_RECEIVE_SECOND
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_count())
    if !expect_reply_shape(reply, syscall.SyscallStatus.Ok, completion_mailbox_service.MAILBOX_CAPACITY) {
        if service_effect.effect_reply_status(reply) != syscall.SyscallStatus.Ok {
            return FAIL_EXTERNAL_COUNT_STATUS
        }
        return FAIL_EXTERNAL_COUNT_LEN
    }

    for i in 0..3 {
        reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
        if service_effect.effect_reply_status(reply) != syscall.SyscallStatus.Ok {
            return FAIL_EXTERNAL_RELEASE_ACK
        }
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_reply_shape(reply, syscall.SyscallStatus.Ok, 4) {
        return FAIL_EXTERNAL_FETCH_STATUS
    }
    payload = service_effect.effect_reply_payload(reply)
    if payload[0] != 1 {
        return FAIL_EXTERNAL_FETCH_ID
    }
    if payload[1] != workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED {
        return FAIL_EXTERNAL_FETCH_STATE
    }
    if payload[2] != workflow_core.WORKFLOW_RESTART_NONE {
        return FAIL_EXTERNAL_FETCH_RESTART
    }
    if payload[3] != 1 {
        return FAIL_EXTERNAL_FETCH_GENERATION
    }

    reply = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(reply) != syscall.SyscallStatus.Ok {
        return FAIL_EXTERNAL_FINAL_ACK
    }

    return 0
}