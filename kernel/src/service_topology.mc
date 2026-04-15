// Service topology: stable endpoint addressing and service-slot records for
// the kernel boot image.  This module owns the four endpoint IDs and the
// four ServiceSlot constants that name the static wiring (endpoint, owner
// pid) for each boot service.  It has no service imports, so both boot.mc
// (which constructs services) and shell_service.mc (which sends to services)
// can import it without a circular dependency.
//
// Adding a new boot-wired service means adding one endpoint ID constant,
// one slot constant, and incrementing SERVICE_COUNT.  All callers that need
// either the endpoint ID or the owner pid reference this module directly.
//
// Endpoint id classification (Phase 183):
//   - Ownership: service_topology.mc is the sole definer of endpoint id
//     constants.  No other module introduces new endpoint id values.
//   - Routing: kernel_dispatch.mc routes on endpoint ids.  It uses
//     endpoint_is_boot_wired() as the explicit addressing gate: if the
//     caller supplies an id that is not a boot-wired public name, the
//     dispatcher returns InvalidEndpoint without consulting any service.
//   - Public name vs authority: knowing a boot-wired endpoint id is
//     sufficient to address that service.  The dispatcher performs no
//     capability check.  Endpoint ids are stable public names, not
//     authority tokens in the capability sense.
//   - Authority surface: handle transfer lives in
//     syscall.ReceiveObservation.received_handle_slot and
//     received_handle_count.  That is the intended future authority gate,
//     separate from routing by name.
//   - Conclusion: endpoint ids are stable public names that double as
//     routing keys.  They are NOT elevated authority-bearing values.
//     A caller that knows a valid public name may address the service.

const SERIAL_ENDPOINT_ID: u32 = 10
const SHELL_ENDPOINT_ID: u32 = 11
const LOG_ENDPOINT_ID: u32 = 12
const KV_ENDPOINT_ID: u32 = 13
const ECHO_ENDPOINT_ID: u32 = 14
const TRANSFER_ENDPOINT_ID: u32 = 15

// ServiceSlot records the static wiring for one boot service: which endpoint
// it occupies and which pid owns it.  Both values are fixed at kernel_init
// time and never change.
struct ServiceSlot {
    endpoint: u32
    pid: u32
}

const SERIAL_SLOT: ServiceSlot = ServiceSlot{ endpoint: SERIAL_ENDPOINT_ID, pid: 1 }
const SHELL_SLOT: ServiceSlot = ServiceSlot{ endpoint: SHELL_ENDPOINT_ID, pid: 2 }
const LOG_SLOT: ServiceSlot = ServiceSlot{ endpoint: LOG_ENDPOINT_ID, pid: 3 }
const KV_SLOT: ServiceSlot = ServiceSlot{ endpoint: KV_ENDPOINT_ID, pid: 4 }
const ECHO_SLOT: ServiceSlot = ServiceSlot{ endpoint: ECHO_ENDPOINT_ID, pid: 5 }
const TRANSFER_SLOT: ServiceSlot = ServiceSlot{ endpoint: TRANSFER_ENDPOINT_ID, pid: 6 }

// SERVICE_COUNT is the number of boot-wired services in the static topology.
// Increment this when a new slot constant is added above.
const SERVICE_COUNT: u32 = 6

func service_count() usize {
    return 6
}

func service_slot_at(index: usize) ServiceSlot {
    if index == 0 {
        return SERIAL_SLOT
    }
    if index == 1 {
        return SHELL_SLOT
    }
    if index == 2 {
        return LOG_SLOT
    }
    if index == 3 {
        return KV_SLOT
    }
    if index == 4 {
        return ECHO_SLOT
    }
    if index == 5 {
        return TRANSFER_SLOT
    }
    return ServiceSlot{ endpoint: 0, pid: 0 }
}

func service_slot_for_endpoint(endpoint: u32) ServiceSlot {
    if endpoint == SERIAL_ENDPOINT_ID {
        return SERIAL_SLOT
    }
    if endpoint == SHELL_ENDPOINT_ID {
        return SHELL_SLOT
    }
    if endpoint == LOG_ENDPOINT_ID {
        return LOG_SLOT
    }
    if endpoint == KV_ENDPOINT_ID {
        return KV_SLOT
    }
    if endpoint == ECHO_ENDPOINT_ID {
        return ECHO_SLOT
    }
    if endpoint == TRANSFER_ENDPOINT_ID {
        return TRANSFER_SLOT
    }
    return ServiceSlot{ endpoint: 0, pid: 0 }
}

func service_slot_is_valid(slot: ServiceSlot) bool {
    if slot.endpoint == 0 {
        return false
    }
    return true
}

// endpoint_is_boot_wired returns true when the endpoint id names one of the
// five statically wired boot services.  This is the explicit addressing
// gate: the dispatcher uses this to separate known public names from unknown
// ids before any service-specific routing.  All boot-wired endpoints are
// publicly addressable.
func endpoint_is_boot_wired(endpoint: u32) bool {
    if endpoint == SERIAL_ENDPOINT_ID {
        return true
    }
    if endpoint == SHELL_ENDPOINT_ID {
        return true
    }
    if endpoint == LOG_ENDPOINT_ID {
        return true
    }
    if endpoint == KV_ENDPOINT_ID {
        return true
    }
    if endpoint == ECHO_ENDPOINT_ID {
        return true
    }
    if endpoint == TRANSFER_ENDPOINT_ID {
        return true
    }
    return false
}
