// Service topology: stable endpoint addressing and service-slot records for
// the kernel boot image.  This module owns the boot-wired endpoint IDs and
// the ServiceSlot constants that name the static wiring (endpoint, owner
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
//     received_handle_count.  That is the current explicit transfer-only
//     authority gate, separate from routing by name.
//   - Conclusion: endpoint ids are stable public names that double as
//     routing keys.  They are NOT elevated authority-bearing values.
//     A caller that knows a valid public name may address the service.

const SERIAL_ENDPOINT_ID: u32 = 10
const SHELL_ENDPOINT_ID: u32 = 11
const LOG_ENDPOINT_ID: u32 = 12
const KV_ENDPOINT_ID: u32 = 13
const ECHO_ENDPOINT_ID: u32 = 14
const TRANSFER_ENDPOINT_ID: u32 = 15
const QUEUE_ENDPOINT_ID: u32 = 16
const TICKET_ENDPOINT_ID: u32 = 17
const FILE_ENDPOINT_ID: u32 = 18
const TIMER_ENDPOINT_ID: u32 = 19
const TASK_ENDPOINT_ID: u32 = 20
const JOURNAL_ENDPOINT_ID: u32 = 21
const WORKFLOW_ENDPOINT_ID: u32 = 22

// ServiceSlot records the static wiring for one boot service: which endpoint
// it occupies and which pid owns it.  Both values are fixed at kernel_init
// time and never change.
struct ServiceSlot {
    endpoint: u32
    pid: u32
}

enum ServiceRestartMode {
    None,
    Reset,
    Reload,
}

enum ServiceAuthorityClass {
    None,
    PublicEndpoint,
    TransferOnly,
    RetainedOwner,
    DurableOwner,
    ShellControl,
}

const SERIAL_SLOT: ServiceSlot = { endpoint: SERIAL_ENDPOINT_ID, pid: 1 }
const SHELL_SLOT: ServiceSlot = { endpoint: SHELL_ENDPOINT_ID, pid: 2 }
const LOG_SLOT: ServiceSlot = { endpoint: LOG_ENDPOINT_ID, pid: 3 }
const KV_SLOT: ServiceSlot = { endpoint: KV_ENDPOINT_ID, pid: 4 }
const ECHO_SLOT: ServiceSlot = { endpoint: ECHO_ENDPOINT_ID, pid: 5 }
const TRANSFER_SLOT: ServiceSlot = { endpoint: TRANSFER_ENDPOINT_ID, pid: 6 }
const QUEUE_SLOT: ServiceSlot = { endpoint: QUEUE_ENDPOINT_ID, pid: 7 }
const TICKET_SLOT: ServiceSlot = { endpoint: TICKET_ENDPOINT_ID, pid: 8 }
const FILE_SLOT: ServiceSlot = { endpoint: FILE_ENDPOINT_ID, pid: 9 }
const TIMER_SLOT: ServiceSlot = { endpoint: TIMER_ENDPOINT_ID, pid: 10 }
const TASK_SLOT: ServiceSlot = { endpoint: TASK_ENDPOINT_ID, pid: 11 }
const JOURNAL_SLOT: ServiceSlot = { endpoint: JOURNAL_ENDPOINT_ID, pid: 12 }
const WORKFLOW_SLOT: ServiceSlot = { endpoint: WORKFLOW_ENDPOINT_ID, pid: 13 }

const SERVICE_SLOTS: [13]ServiceSlot = {
    SERIAL_SLOT,
    SHELL_SLOT,
    LOG_SLOT,
    KV_SLOT,
    ECHO_SLOT,
    TRANSFER_SLOT,
    QUEUE_SLOT,
    TICKET_SLOT,
    FILE_SLOT,
    TIMER_SLOT,
    TASK_SLOT,
    JOURNAL_SLOT,
    WORKFLOW_SLOT
}

const SERVICE_RESTART_MODES: [13]ServiceRestartMode = {
    ServiceRestartMode.None,
    ServiceRestartMode.None,
    ServiceRestartMode.Reload,
    ServiceRestartMode.Reload,
    ServiceRestartMode.Reset,
    ServiceRestartMode.Reset,
    ServiceRestartMode.Reload,
    ServiceRestartMode.Reset,
    ServiceRestartMode.Reload,
    ServiceRestartMode.Reload,
    ServiceRestartMode.Reset,
    ServiceRestartMode.Reload,
    ServiceRestartMode.Reload
}

// SERVICE_COUNT is the number of boot-wired services in the static topology.
// Increment this when a new slot constant is added above.
const SERVICE_COUNT: u32 = 13

func service_count() usize {
    return 13
}

func service_slot_at(index: usize) ServiceSlot {
    if index >= service_count() {
        return { endpoint: 0, pid: 0 }
    }
    return SERVICE_SLOTS[index]
}

func service_slot_for_endpoint(endpoint: u32) ServiceSlot {
    for i in 0..service_count() {
        slot: ServiceSlot = SERVICE_SLOTS[i]
        if slot.endpoint == endpoint {
            return slot
        }
    }
    return { endpoint: 0, pid: 0 }
}

func service_slot_is_valid(slot: ServiceSlot) bool {
    if slot.endpoint == 0 {
        return false
    }
    return true
}

// Lifecycle classification stays with the static slot owner.
// init.mc enacts restart, but the topology module names which public services
// restart at all and which ones reload retained state.
func service_restart_mode(endpoint: u32) ServiceRestartMode {
    for i in 0..service_count() {
        slot: ServiceSlot = SERVICE_SLOTS[i]
        if slot.endpoint == endpoint {
            return SERVICE_RESTART_MODES[i]
        }
    }
    return ServiceRestartMode.None
}

func service_can_restart(endpoint: u32) bool {
    if service_restart_mode(endpoint) == ServiceRestartMode.None {
        return false
    }
    return true
}

func service_restart_reloads_state(endpoint: u32) bool {
    if service_restart_mode(endpoint) == ServiceRestartMode.Reload {
        return true
    }
    return false
}

func service_authority_class(endpoint: u32) ServiceAuthorityClass {
    switch endpoint {
    case SHELL_ENDPOINT_ID:
        return ServiceAuthorityClass.ShellControl
    case TRANSFER_ENDPOINT_ID:
        return ServiceAuthorityClass.TransferOnly
    case LOG_ENDPOINT_ID:
        return ServiceAuthorityClass.RetainedOwner
    case KV_ENDPOINT_ID:
        return ServiceAuthorityClass.RetainedOwner
    case QUEUE_ENDPOINT_ID:
        return ServiceAuthorityClass.RetainedOwner
    case FILE_ENDPOINT_ID:
        return ServiceAuthorityClass.RetainedOwner
    case TIMER_ENDPOINT_ID:
        return ServiceAuthorityClass.RetainedOwner
    case JOURNAL_ENDPOINT_ID:
        return ServiceAuthorityClass.DurableOwner
    case WORKFLOW_ENDPOINT_ID:
        return ServiceAuthorityClass.RetainedOwner
    default:
        if endpoint_is_boot_wired(endpoint) {
            return ServiceAuthorityClass.PublicEndpoint
        }
        return ServiceAuthorityClass.None
    }
}

// endpoint_is_boot_wired returns true when the endpoint id names one of the
// boot-wired public services.  This is the explicit addressing
// gate: the dispatcher uses this to separate known public names from unknown
// ids before any service-specific routing.  All boot-wired endpoints are
// publicly addressable.
func endpoint_is_boot_wired(endpoint: u32) bool {
    return service_slot_is_valid(service_slot_for_endpoint(endpoint))
}
