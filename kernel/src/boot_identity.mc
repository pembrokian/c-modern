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
    return service_identity.service_mark(service_topology.LOG_ENDPOINT_ID, s.log.state.pid, s.log.generation, s.log.generation_payload)
}

func boot_kv_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.KV_ENDPOINT_ID, s.kv.state.pid, s.kv.generation, s.kv.generation_payload)
}

func boot_queue_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.QUEUE_ENDPOINT_ID, s.queue.state.pid, s.queue.generation, s.queue.generation_payload)
}

func boot_echo_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.ECHO_ENDPOINT_ID, s.echo.state.pid, s.echo.generation, s.echo.generation_payload)
}

func boot_transfer_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.TRANSFER_ENDPOINT_ID, s.transfer.state.pid, s.transfer.generation, s.transfer.generation_payload)
}

func boot_ticket_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.TICKET_ENDPOINT_ID, s.ticket.state.pid, s.ticket.generation, s.ticket.generation_payload)
}

func bootmark_for_endpoint(s: KernelBootState, endpoint: u32) service_identity.ServiceMark {
    if endpoint == service_topology.LOG_ENDPOINT_ID {
        return boot_log_mark(s)
    }
    if endpoint == service_topology.KV_ENDPOINT_ID {
        return boot_kv_mark(s)
    }
    if endpoint == service_topology.QUEUE_ENDPOINT_ID {
        return boot_queue_mark(s)
    }
    if endpoint == service_topology.ECHO_ENDPOINT_ID {
        return boot_echo_mark(s)
    }
    if endpoint == service_topology.TRANSFER_ENDPOINT_ID {
        return boot_transfer_mark(s)
    }
    if endpoint == service_topology.TICKET_ENDPOINT_ID {
        return boot_ticket_mark(s)
    }
    return service_identity.service_mark(endpoint, 0, 0, service_identity.generation_zero_payload())
}