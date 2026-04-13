import boot
import ipc
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10
const LOG_ENDPOINT_ID: u32 = 12
const KV_ENDPOINT_ID: u32 = 13

func make_observation(endpoint_id: u32, payload_len: usize, b0: u8, b1: u8) syscall.ReceiveObservation {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = b0
    payload[1] = b1
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: 1, payload_len: payload_len, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func main() i32 {
    state: boot.KernelBootState = boot.kernel_init()
    result: boot.DispatchResult

    result = boot.kernel_dispatch_step(state, make_observation(SERIAL_ENDPOINT_ID, 1, 65, 0))
    if result.routed == 0 {
        return 1
    }
    state = result.state

    result = boot.kernel_dispatch_step(state, make_observation(LOG_ENDPOINT_ID, 1, 77, 0))
    if result.routed == 0 {
        return 1
    }
    state = result.state

    result = boot.kernel_dispatch_step(state, make_observation(KV_ENDPOINT_ID, 2, 5, 42))
    if result.routed == 0 {
        return 1
    }
    state = result.state

    result = boot.kernel_dispatch_step(state, make_observation(KV_ENDPOINT_ID, 1, 5, 0))
    if result.routed == 0 {
        return 1
    }
    if result.reply_status != syscall.SyscallStatus.Ok {
        return 1
    }
    if result.reply_payload[1] != 42 {
        return 1
    }

    return 0
}
