// Phase 183: Endpoint ownership and authority audit.
//
// Proves the endpoint id classification established by Phase 183:
//   - endpoint ids are stable public names that double as routing keys
//   - knowing a boot-wired endpoint id is sufficient to address a service
//   - there is no capability check at the dispatch gateway
//   - unknown endpoint ids return InvalidEndpoint at the explicit guard
//   - ServiceRef names the same endpoint id that the dispatcher routes on

import boot
import ipc
import kernel_dispatch
import service_effect
import service_identity
import service_topology
import syscall

func obs_for(endpoint: u32) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint, source_pid: 1, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: ipc.zero_payload() }
}

// All six boot-wired endpoints are recognized as public names.
func check_all_boot_endpoints_are_named() bool {
    if !service_topology.endpoint_is_boot_wired(service_topology.SERIAL_ENDPOINT_ID) {
        return false
    }
    if !service_topology.endpoint_is_boot_wired(service_topology.SHELL_ENDPOINT_ID) {
        return false
    }
    if !service_topology.endpoint_is_boot_wired(service_topology.LOG_ENDPOINT_ID) {
        return false
    }
    if !service_topology.endpoint_is_boot_wired(service_topology.KV_ENDPOINT_ID) {
        return false
    }
    if !service_topology.endpoint_is_boot_wired(service_topology.ECHO_ENDPOINT_ID) {
        return false
    }
    if !service_topology.endpoint_is_boot_wired(service_topology.TRANSFER_ENDPOINT_ID) {
        return false
    }
    return true
}

// Unknown endpoint ids return false from endpoint_is_boot_wired.
func check_unknown_endpoint_not_named() bool {
    if service_topology.endpoint_is_boot_wired(0) {
        return false
    }
    if service_topology.endpoint_is_boot_wired(99) {
        return false
    }
    if service_topology.endpoint_is_boot_wired(255) {
        return false
    }
    return true
}

// Unknown endpoint at the dispatch gateway returns InvalidEndpoint.
// The guard is explicit: no capability check, just name recognition.
func check_unknown_endpoint_returns_invalid_endpoint() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs_for(99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidEndpoint {
        return false
    }
    return true
}

// A caller that knows the log endpoint id can address log directly.
// No capability check is performed.  The endpoint id is the complete
// public address.  A zero-payload message is interpreted as a tail query
// and returns Ok.
func check_known_endpoint_publicly_addressable() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs_for(service_topology.LOG_ENDPOINT_ID))
    if service_effect.effect_reply_status(effect) == syscall.SyscallStatus.InvalidEndpoint {
        return false
    }
    return true
}

// ServiceRef.endpoint_id is the same value the dispatcher routes on.
// The ref is a naming surface: it holds a public name, not a capability.
// Any valid ServiceRef names a boot-wired endpoint.
func check_service_ref_names_boot_wired_endpoint() bool {
    log_ref: service_identity.ServiceRef = boot.boot_log_ref()
    kv_ref: service_identity.ServiceRef = boot.boot_kv_ref()
    echo_ref: service_identity.ServiceRef = boot.boot_echo_ref()
    transfer_ref: service_identity.ServiceRef = boot.boot_transfer_ref()

    if service_identity.ref_endpoint(log_ref) != service_topology.LOG_ENDPOINT_ID {
        return false
    }
    if service_identity.ref_endpoint(kv_ref) != service_topology.KV_ENDPOINT_ID {
        return false
    }
    if service_identity.ref_endpoint(echo_ref) != service_topology.ECHO_ENDPOINT_ID {
        return false
    }
    if service_identity.ref_endpoint(transfer_ref) != service_topology.TRANSFER_ENDPOINT_ID {
        return false
    }
    if !service_topology.endpoint_is_boot_wired(service_identity.ref_endpoint(log_ref)) {
        return false
    }
    if !service_topology.endpoint_is_boot_wired(service_identity.ref_endpoint(kv_ref)) {
        return false
    }
    if !service_topology.endpoint_is_boot_wired(service_identity.ref_endpoint(transfer_ref)) {
        return false
    }
    return true
}

func main() i32 {
    if !check_all_boot_endpoints_are_named() {
        return 1
    }
    if !check_unknown_endpoint_not_named() {
        return 2
    }
    if !check_unknown_endpoint_returns_invalid_endpoint() {
        return 3
    }
    if !check_known_endpoint_publicly_addressable() {
        return 4
    }
    if !check_service_ref_names_boot_wired_endpoint() {
        return 5
    }
    return 0
}
