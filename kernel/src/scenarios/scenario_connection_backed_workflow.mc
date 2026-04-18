import boot
import connection_service
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall
import workflow_core

const FAIL_EXECUTE_OPEN: i32 = 23901
const FAIL_EXECUTE_RECEIVE: i32 = 23902
const FAIL_EXECUTE_ADMIT: i32 = 23903
const FAIL_EXECUTE_WAITING: i32 = 23904
const FAIL_EXECUTE_RUNNING: i32 = 23905
const FAIL_EXECUTE_DONE: i32 = 23906
const FAIL_EXECUTE_FETCH: i32 = 23907
const FAIL_EXECUTE_ACK: i32 = 23908
const FAIL_CANCEL_RECEIVE: i32 = 23909
const FAIL_CANCEL_ADMIT: i32 = 23910
const FAIL_CANCEL_CLOSE: i32 = 23911
const FAIL_CANCEL_DONE: i32 = 23912
const FAIL_CANCEL_FETCH: i32 = 23913
const FAIL_CANCEL_ACK: i32 = 23914
const FAIL_RESTART_OPEN: i32 = 23915
const FAIL_RESTART_RECEIVE: i32 = 23916
const FAIL_RESTART_ADMIT: i32 = 23917
const FAIL_RESTART_CONNECTION: i32 = 23918
const FAIL_RESTART_WORKFLOW: i32 = 23919
const FAIL_RESTART_DONE: i32 = 23920
const FAIL_RESTART_FETCH: i32 = 23921
const FAIL_RESTART_ACK: i32 = 23922

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

func run_connection_backed_workflow_probe() i32 {
    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_open(1, 5))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXECUTE_OPEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(1, 11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXECUTE_RECEIVE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_execute(1))
    if !expect_schedule(effect, 1) {
        return FAIL_EXECUTE_ADMIT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXECUTE_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_RUNNING, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXECUTE_RUNNING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_EXECUTE_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, 1, workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_EXECUTE_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_EXECUTE_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(1, 12))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_CANCEL_RECEIVE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_execute(1))
    if !expect_schedule(effect, 2) {
        return FAIL_CANCEL_ADMIT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_close(1, connection_service.CONNECTION_CLOSE_DISCONNECT))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_CANCEL_CLOSE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(2))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_CONNECTION_CANCELLED, workflow_core.WORKFLOW_RESTART_NONE) {
        return FAIL_CANCEL_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, 2, workflow_core.WORKFLOW_STATE_CONNECTION_CANCELLED, workflow_core.WORKFLOW_RESTART_NONE, 1) {
        return FAIL_CANCEL_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_CANCEL_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_open(2, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_OPEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(2, 13))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_RECEIVE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_execute(2))
    if !expect_schedule(effect, 3) {
        return FAIL_RESTART_ADMIT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_CONNECTION))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_CONNECTION, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_RESTART_CONNECTION
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKFLOW, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_RESTART_WORKFLOW
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(3))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_CONNECTION_RESTART_CANCELLED, workflow_core.WORKFLOW_RESTART_CANCELLED) {
        return FAIL_RESTART_DONE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, 3, workflow_core.WORKFLOW_STATE_CONNECTION_RESTART_CANCELLED, workflow_core.WORKFLOW_RESTART_CANCELLED, 1) {
        return FAIL_RESTART_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_ACK
    }

    return 0
}