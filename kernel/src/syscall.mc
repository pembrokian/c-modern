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
    // received_handle_slot and received_handle_count are reserved for future
    // capability-passing. They must be zero-initialised and ignored by all
    // current services.
    received_handle_slot: u32
    received_handle_count: usize
    payload: [4]u8
}

func empty_receive_observation() ReceiveObservation {
    return ReceiveObservation{ status: SyscallStatus.None, block_reason: BlockReason.None, endpoint_id: 0, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: primitives.zero_payload() }
}