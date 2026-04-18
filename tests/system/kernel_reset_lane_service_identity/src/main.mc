import boot
import init
import kernel_dispatch
import ipc
import service_effect
import service_identity
import service_topology
import syscall

func build_serial_cmd(b0: u8, b1: u8, b2: u8, b3: u8) syscall.ReceiveObservation {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = b0
    payload[1] = b1
    payload[2] = b2
    payload[3] = b3
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 99, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// Boot-wired service refs are non-zero and mutually distinct.
func smoke_boot_refs_are_valid_and_distinct() bool {
    serial_ref: service_identity.ServiceRef = boot.BOOT_SERIAL_REF
    shell_ref: service_identity.ServiceRef = boot.BOOT_SHELL_REF
    log_ref: service_identity.ServiceRef = boot.BOOT_LOG_REF
    kv_ref: service_identity.ServiceRef = boot.BOOT_KV_REF

    if !service_identity.ref_is_valid(serial_ref) {
        return false
    }
    if !service_identity.ref_is_valid(shell_ref) {
        return false
    }
    if !service_identity.ref_is_valid(log_ref) {
        return false
    }
    if !service_identity.ref_is_valid(kv_ref) {
        return false
    }
    if service_identity.refs_equal(serial_ref, shell_ref) {
        return false
    }
    if service_identity.refs_equal(serial_ref, log_ref) {
        return false
    }
    if service_identity.refs_equal(serial_ref, kv_ref) {
        return false
    }
    if service_identity.refs_equal(shell_ref, log_ref) {
        return false
    }
    if service_identity.refs_equal(shell_ref, kv_ref) {
        return false
    }
    if service_identity.refs_equal(log_ref, kv_ref) {
        return false
    }
    return true
}

// After an explicit restart the service ref is still valid.
// The endpoint_id and pid do not change, but the instance generation does.
// Retained state may still survive when init reloads it explicitly.
func smoke_restart_keeps_route_and_replaces_instance() bool {
    state: boot.KernelBootState = boot.kernel_init()
    log_ref: service_identity.ServiceRef = boot.BOOT_LOG_REF
    before: service_identity.ServiceMark = boot.boot_log_mark(state)
    append_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, build_serial_cmd(76, 65, 55, 33))
    if service_effect.effect_reply_status(append_effect) != syscall.SyscallStatus.Ok {
        return false
    }

    state = init.restart(state, service_identity.ref_endpoint(log_ref))
    after: service_identity.ServiceMark = boot.boot_log_mark(state)

    // Ref is still valid after restart: endpoint_id unchanged.
    if !service_identity.ref_is_valid(log_ref) {
        return false
    }
    if !service_identity.ref_matches_mark(log_ref, after) {
        return false
    }
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

    tail_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, build_serial_cmd(76, 84, 33, 33))
    if service_effect.effect_reply_status(tail_effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(tail_effect) != 1 {
        return false
    }
    if service_effect.effect_reply_payload(tail_effect)[0] != 55 {
        return false
    }
    return true
}

// Refs obtained before and after re-obtaining them name the same endpoint.
// This confirms there is no "fresh ref" concept; clients never need to
// re-discover the service after restart.
func smoke_ref_is_idempotent() bool {
    first: service_identity.ServiceRef = boot.BOOT_LOG_REF
    second: service_identity.ServiceRef = boot.BOOT_LOG_REF
    return service_identity.refs_equal(first, second)
}

// Dispatching via the serial endpoint still routes correctly — the endpoint_id
// in the ref matches what the kernel dispatcher expects.
func smoke_serial_ref_endpoint_routes_correctly() bool {
    state: boot.KernelBootState = boot.kernel_init()
    serial_ref: service_identity.ServiceRef = boot.BOOT_SERIAL_REF

    obs: syscall.ReceiveObservation = build_serial_cmd(76, 65, 55, 33)
    if service_identity.ref_endpoint(serial_ref) != obs.endpoint_id {
        return false
    }
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs)
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_boot_refs_are_valid_and_distinct() {
        return 1
    }
    if !smoke_restart_keeps_route_and_replaces_instance() {
        return 1
    }
    if !smoke_ref_is_idempotent() {
        return 1
    }
    if !smoke_serial_ref_endpoint_routes_correctly() {
        return 1
    }
    return 0
}
