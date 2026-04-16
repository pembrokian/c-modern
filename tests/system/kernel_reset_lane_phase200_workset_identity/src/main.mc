import boot
import kernel_dispatch
import serial_protocol
import service_effect
import service_identity
import service_topology
import syscall

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func expect_generation_payload(effect: service_effect.Effect, generation: u32) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = service_identity.generation_payload(generation)
    if payload[0] != expected[0] {
        return false
    }
    if payload[1] != expected[1] {
        return false
    }
    if payload[2] != expected[2] {
        return false
    }
    if payload[3] != expected[3] {
        return false
    }
    return true
}

func expect_lifecycle(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, mode: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != target {
        return false
    }
    if payload[1] != mode {
        return false
    }
    return true
}

func smoke_workset_identity_query_tracks_only_coordinated_restart() bool {
    state: boot.KernelBootState = boot.kernel_init()
    before_generation: u32 = boot.boot_workset_generation(state)
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_identity(serial_protocol.TARGET_WORKSET)))
    if !expect_generation_payload(effect, before_generation) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_WORKSET)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }

    after_generation: u32 = boot.boot_workset_generation(state)
    if after_generation != before_generation + 1 {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_identity(serial_protocol.TARGET_WORKSET)))
    if !expect_generation_payload(effect, after_generation) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_KV)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_KV, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_identity(serial_protocol.TARGET_WORKSET)))
    if !expect_generation_payload(effect, after_generation) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_workset_identity_query_tracks_only_coordinated_restart() {
        return 1
    }
    return 0
}