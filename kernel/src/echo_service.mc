import ipc
import syscall

enum EchoMessageTag {
    None,
    Request,
    Reply,
}

struct EchoServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    last_tag: EchoMessageTag
    request_count: usize
    reply_count: usize
    last_client_pid: u32
    last_endpoint_id: u32
    last_request_len: usize
    last_request_byte0: u8
    last_request_byte1: u8
    last_reply_len: usize
    last_reply_byte0: u8
    last_reply_byte1: u8
}

struct EchoExchangeObservation {
    service_pid: u32
    client_pid: u32
    endpoint_id: u32
    tag: EchoMessageTag
    request_len: usize
    request_byte0: u8
    request_byte1: u8
    reply_len: usize
    reply_byte0: u8
    reply_byte1: u8
    request_count: usize
    reply_count: usize
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) EchoServiceState {
    return EchoServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, last_tag: EchoMessageTag.None, request_count: 0, reply_count: 0, last_client_pid: 0, last_endpoint_id: 0, last_request_len: 0, last_request_byte0: 0, last_request_byte1: 0, last_reply_len: 0, last_reply_byte0: 0, last_reply_byte1: 0 }
}

func record_request(state: EchoServiceState, observation: syscall.ReceiveObservation) EchoServiceState {
    request_byte0: u8 = 0
    request_byte1: u8 = 0
    if observation.payload_len != 0 {
        request_byte0 = observation.payload[0]
    }
    if observation.payload_len == 2 {
        request_byte1 = observation.payload[1]
    }
    return EchoServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: EchoMessageTag.Request, request_count: state.request_count + 1, reply_count: state.reply_count, last_client_pid: observation.source_pid, last_endpoint_id: observation.endpoint_id, last_request_len: observation.payload_len, last_request_byte0: request_byte0, last_request_byte1: request_byte1, last_reply_len: state.last_reply_len, last_reply_byte0: state.last_reply_byte0, last_reply_byte1: state.last_reply_byte1 }
}

func reply_payload(state: EchoServiceState) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = state.last_request_byte0
    payload[1] = state.last_request_byte1
    return payload
}

func record_reply(state: EchoServiceState, observation: syscall.ReceiveObservation) EchoServiceState {
    reply_byte0: u8 = 0
    reply_byte1: u8 = 0
    if observation.payload_len != 0 {
        reply_byte0 = observation.payload[0]
    }
    if observation.payload_len == 2 {
        reply_byte1 = observation.payload[1]
    }
    return EchoServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: EchoMessageTag.Reply, request_count: state.request_count, reply_count: state.reply_count + 1, last_client_pid: state.last_client_pid, last_endpoint_id: state.last_endpoint_id, last_request_len: state.last_request_len, last_request_byte0: state.last_request_byte0, last_request_byte1: state.last_request_byte1, last_reply_len: observation.payload_len, last_reply_byte0: reply_byte0, last_reply_byte1: reply_byte1 }
}

func observe_exchange(state: EchoServiceState) EchoExchangeObservation {
    return EchoExchangeObservation{ service_pid: state.owner_pid, client_pid: state.last_client_pid, endpoint_id: state.last_endpoint_id, tag: state.last_tag, request_len: state.last_request_len, request_byte0: state.last_request_byte0, request_byte1: state.last_request_byte1, reply_len: state.last_reply_len, reply_byte0: state.last_reply_byte0, reply_byte1: state.last_reply_byte1, request_count: state.request_count, reply_count: state.reply_count }
}

func tag_score(tag: EchoMessageTag) i32 {
    switch tag {
    case EchoMessageTag.None:
        return 1
    case EchoMessageTag.Request:
        return 2
    case EchoMessageTag.Reply:
        return 4
    default:
        return 0
    }
    return 0
}