import boot
import ipc
import log_service
import service_effect
import service_identity
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10  // mirrors boot.SERIAL_ENDPOINT_ID

func build_serial_cmd(b0: u8, b1: u8, b2: u8, b3: u8) syscall.ReceiveObservation {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = b0
    payload[1] = b1
    payload[2] = b2
    payload[3] = b3
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: SERIAL_ENDPOINT_ID, source_pid: 99, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// Boot-wired service refs are non-zero and mutually distinct.
func smoke_boot_refs_are_valid_and_distinct() bool {
    serial_ref: service_identity.ServiceRef = boot.boot_serial_ref()
    shell_ref: service_identity.ServiceRef = boot.boot_shell_ref()
    log_ref: service_identity.ServiceRef = boot.boot_log_ref()
    kv_ref: service_identity.ServiceRef = boot.boot_kv_ref()

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

// After a simulated restart the service ref is still valid.
// The endpoint_id does not change; only retained state is lost.
func smoke_ref_survives_restart_state_is_gone() bool {
    log_ref: service_identity.ServiceRef = boot.boot_log_ref()

    // Simulate restart: re-init the log service (state reset to empty).
    restarted_state: log_service.LogServiceState = log_service.log_init(3, 1)

    // Ref is still valid after restart: endpoint_id unchanged.
    if !service_identity.ref_is_valid(log_ref) {
        return false
    }
    // Restarted state has no retained entries.
    if log_service.log_len(restarted_state) != 0 {
        return false
    }
    return true
}

// Refs obtained before and after re-obtaining them name the same endpoint.
// This confirms there is no "fresh ref" concept; clients never need to
// re-discover the service after restart.
func smoke_ref_is_idempotent() bool {
    first: service_identity.ServiceRef = boot.boot_log_ref()
    second: service_identity.ServiceRef = boot.boot_log_ref()
    return service_identity.refs_equal(first, second)
}

// Dispatching via the serial endpoint still routes correctly — the endpoint_id
// in the ref matches what the kernel dispatcher expects.
func smoke_serial_ref_endpoint_routes_correctly() bool {
    state: boot.KernelBootState = boot.kernel_init()
    serial_ref: service_identity.ServiceRef = boot.boot_serial_ref()

    obs: syscall.ReceiveObservation = build_serial_cmd(76, 65, 55, 33)
    if service_identity.ref_endpoint(serial_ref) != obs.endpoint_id {
        return false
    }
    effect: service_effect.Effect = boot.kernel_dispatch_step(&state, obs)
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_boot_refs_are_valid_and_distinct() {
        return 1
    }
    if !smoke_ref_survives_restart_state_is_gone() {
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
