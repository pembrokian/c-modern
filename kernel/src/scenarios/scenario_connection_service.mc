import boot
import connection_service
import init
import kernel_dispatch
import scenario_transport
import service_effect
import service_topology
import syscall

const FAIL_CONNECTION_OPEN: i32 = 23801
const FAIL_CONNECTION_RECEIVE: i32 = 23802
const FAIL_CONNECTION_SEND: i32 = 23803
const FAIL_CONNECTION_CLOSE: i32 = 23804
const FAIL_CONNECTION_DISCONNECTED: i32 = 23805
const FAIL_CONNECTION_TIMEOUT: i32 = 23806
const FAIL_CONNECTION_RESTART: i32 = 23807

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

func run_connection_service_probe() i32 {
    state := boot.kernel_init()

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_open(1, 2))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, 1, connection_service.CONNECTION_STATE_OPEN, 0, 0) {
        return FAIL_CONNECTION_OPEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(1, 44))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 3, 1, 44, connection_service.CONNECTION_STATE_RECEIVED, 0) {
        return FAIL_CONNECTION_RECEIVE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_send(1, 99))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 4, 1, 44, 99, connection_service.CONNECTION_STATE_REPLIED) {
        return FAIL_CONNECTION_SEND
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_close(1, connection_service.CONNECTION_CLOSE_DISCONNECT))
    if !expect_reply(effect, syscall.SyscallStatus.Ok, 2, 1, connection_service.CONNECTION_STATE_DISCONNECTED, 0, 0) {
        return FAIL_CONNECTION_CLOSE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(1, 55))
    if !expect_reply(effect, syscall.SyscallStatus.Closed, 1, connection_service.CONNECTION_STATE_DISCONNECTED, 0, 0, 0) {
        return FAIL_CONNECTION_DISCONNECTED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_open(2, 1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_CONNECTION_TIMEOUT
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_receive(2, 12))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_CONNECTION_TIMEOUT
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_connection_send(2, 13))
    if !expect_reply(effect, syscall.SyscallStatus.WouldBlock, 1, connection_service.CONNECTION_STATE_TIMED_OUT, 0, 0, 0) {
        return FAIL_CONNECTION_TIMEOUT
    }

    restarted := boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&restarted, scenario_transport.cmd_connection_open(3, 2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_CONNECTION_RESTART
    }
    restarted = init.restart(restarted, service_topology.CONNECTION_ENDPOINT_ID)
    if connection_service.connection_state_code(restarted.connection.state, 3) != connection_service.CONNECTION_STATE_INVALID {
        return FAIL_CONNECTION_RESTART
    }

    return 0
}