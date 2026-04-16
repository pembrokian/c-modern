import primitives

enum SyscallStatus {
    None,
    Ok,
    WouldBlock,
    InvalidHandle,
    InvalidEndpoint,
    Closed,
    InvalidCapability,
    Exhausted,
    InvalidArgument,
}

enum BlockReason {
    None,
    EndpointQueueFull,
    EndpointQueueEmpty,
    WaitPending,
    TimerPending,
}

struct ReceiveObservation {
    status: SyscallStatus
    block_reason: BlockReason
    endpoint_id: u32
    source_pid: u32
    payload_len: usize
    // received_handle_slot and received_handle_count carry the explicit
    // transfer-only authority path. Current dispatch still rejects any
    // non-zero value on ordinary named traffic; the only admitted exception
    // is the transfer endpoint.
    received_handle_slot: u32
    received_handle_count: usize
    payload: [4]u8
}

func empty_receive_observation() ReceiveObservation {
    return ReceiveObservation{ status: SyscallStatus.None, block_reason: BlockReason.None, endpoint_id: 0, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: primitives.zero_payload() }
}

func observation_has_received_handle(obs: ReceiveObservation) bool {
    if obs.received_handle_slot != 0 {
        return true
    }
    if obs.received_handle_count != 0 {
        return true
    }
    return false
}
