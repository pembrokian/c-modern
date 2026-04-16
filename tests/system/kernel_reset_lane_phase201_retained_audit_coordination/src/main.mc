import boot
import kernel_dispatch
import serial_protocol
import service_effect
import service_identity
import service_topology
import syscall

const KV_WRITE_LOG_MARKER: u8 = 75

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
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

func expect_restart_identity(before: service_identity.ServiceMark, after: service_identity.ServiceMark) bool {
    if !service_identity.marks_same_endpoint(before, after) {
        return false
    }
    if !service_identity.marks_same_pid(before, after) {
        return false
    }
    if service_identity.marks_same_instance(before, after) {
        return false
    }
    if service_identity.mark_generation(after) != service_identity.mark_generation(before) + 1 {
        return false
    }
    return true
}

func log_tail_is(effect: service_effect.Effect, value0: u8, value1: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != value0 {
        return false
    }
    if payload[1] != value1 {
        return false
    }
    return true
}

func kv_value_is(effect: service_effect.Effect, key: u8, value: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != key {
        return false
    }
    if payload[1] != value {
        return false
    }
    return true
}

func smoke_retained_audit_restart_preserves_kv_and_log_truth() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_query(serial_protocol.TARGET_AUDIT)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(40, 131)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(41, 142)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if !log_tail_is(effect, KV_WRITE_LOG_MARKER, KV_WRITE_LOG_MARKER) {
        return false
    }

    kv_before: service_identity.ServiceMark = boot.boot_kv_mark(state)
    log_before: service_identity.ServiceMark = boot.boot_log_mark(state)
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_AUDIT)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }

    if !expect_restart_identity(kv_before, boot.boot_kv_mark(state)) {
        return false
    }
    if !expect_restart_identity(log_before, boot.boot_log_mark(state)) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_get(40)))
    if !kv_value_is(effect, 40, 131) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_get(41)))
    if !kv_value_is(effect, 41, 142) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if !log_tail_is(effect, KV_WRITE_LOG_MARKER, KV_WRITE_LOG_MARKER) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_retained_audit_restart_preserves_kv_and_log_truth() {
        return 1
    }
    return 0
}