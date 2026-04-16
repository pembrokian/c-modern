import service_identity
import service_topology

// Named ServiceRef accessors for the four boot-wired services.
// These refs are stable across restart -- the endpoint_id never changes after
// kernel_init() assigns it. Callers that hold one of these refs may resume
// sending after a service restart without reacquiring the ref.

func boot_serial_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.SERIAL_ENDPOINT_ID)
}

func boot_shell_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.SHELL_ENDPOINT_ID)
}

func boot_log_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.LOG_ENDPOINT_ID)
}

func boot_kv_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.KV_ENDPOINT_ID)
}

func boot_echo_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.ECHO_ENDPOINT_ID)
}

func boot_queue_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.QUEUE_ENDPOINT_ID)
}

func boot_transfer_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.TRANSFER_ENDPOINT_ID)
}

func boot_ticket_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.TICKET_ENDPOINT_ID)
}

func boot_log_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.LOG_ENDPOINT_ID, s.log.state.pid, s.log.generation)
}

func boot_kv_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.KV_ENDPOINT_ID, s.kv.state.pid, s.kv.generation)
}

func boot_queue_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.QUEUE_ENDPOINT_ID, s.queue.state.pid, s.queue.generation)
}

func boot_workset_generation(s: KernelBootState) u32 {
    return s.workset_generation
}

func boot_workset_generation_payload(s: KernelBootState) [4]u8 {
    return service_identity.generation_payload(s.workset_generation)
}

func boot_echo_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.ECHO_ENDPOINT_ID, s.echo.state.pid, s.echo.generation)
}

func boot_transfer_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.TRANSFER_ENDPOINT_ID, s.transfer.state.pid, s.transfer.generation)
}

func boot_ticket_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.TICKET_ENDPOINT_ID, s.ticket.state.pid, s.ticket.generation)
}

func bootmark_for_endpoint(s: KernelBootState, endpoint: u32) service_identity.ServiceMark {
    switch endpoint {
    case service_topology.LOG_ENDPOINT_ID:
        return boot_log_mark(s)
    case service_topology.KV_ENDPOINT_ID:
        return boot_kv_mark(s)
    case service_topology.QUEUE_ENDPOINT_ID:
        return boot_queue_mark(s)
    case service_topology.ECHO_ENDPOINT_ID:
        return boot_echo_mark(s)
    case service_topology.TRANSFER_ENDPOINT_ID:
        return boot_transfer_mark(s)
    case service_topology.TICKET_ENDPOINT_ID:
        return boot_ticket_mark(s)
    default:
        return service_identity.service_mark(endpoint, 0, 0)
    }
}