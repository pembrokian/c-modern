import primitives
import service_effect
import syscall

const LOG_BUFFER_CAPACITY: usize = 4

struct LogServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    retained: [4]u8
    len: usize
}

struct LogReply {
    status: syscall.SyscallStatus
    payload_len: usize
    payload: [4]u8
}

struct LogHandleResult {
    state: LogServiceState
    effect: service_effect.Effect
}

func log_init(owner_pid: u32, endpoint_handle_slot: u32) LogServiceState {
    return LogServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, retained: primitives.zero_payload(), len: 0 }
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) LogServiceState {
    return log_init(owner_pid, endpoint_handle_slot)
}

func log_on_append(state: LogServiceState, observation: syscall.ReceiveObservation) LogServiceState {
    if observation.payload_len == 0 {
        return state
    }
    if state.len >= LOG_BUFFER_CAPACITY {
        // Intentional silent drop: buffer is fixed-capacity for bootstrap.
        // A full buffer returns the state unchanged. The caller receives an
        // Ok reply from handle() regardless — overflow is not signalled.
        return state
    }

    next_retained: [4]u8 = state.retained
    next_retained[state.len] = observation.payload[0]
    return LogServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, retained: next_retained, len: state.len + 1 }
}

func log_on_tail(state: LogServiceState) LogReply {
    payload: [4]u8 = primitives.zero_payload()
    if state.len == 0 {
        return LogReply{ status: syscall.SyscallStatus.Ok, payload_len: 0, payload: payload }
    }
    payload[0] = state.retained[0]
    if state.len > 1 {
        payload[1] = state.retained[1]
    }
    if state.len > 2 {
        payload[2] = state.retained[2]
    }
    if state.len > 3 {
        payload[3] = state.retained[3]
    }
    return LogReply{ status: syscall.SyscallStatus.Ok, payload_len: state.len, payload: payload }
}

func handle(state: LogServiceState, msg: service_effect.Message) LogHandleResult {
    if msg.payload_len == 0 {
        reply: LogReply = log_on_tail(state)
        return LogHandleResult{ state: state, effect: service_effect.effect_reply(reply.status, reply.payload_len, reply.payload) }
    }
    new_state: LogServiceState = log_on_append(state, syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: msg.endpoint_id, source_pid: msg.source_pid, payload_len: msg.payload_len, received_handle_slot: 0, received_handle_count: 0, payload: msg.payload })
    return LogHandleResult{ state: new_state, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func debug_retained_len(state: LogServiceState) usize {
    return state.len
}