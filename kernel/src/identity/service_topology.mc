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
const LEASE_ENDPOINT_ID: u32 = 23
const COMPLETION_MAILBOX_ENDPOINT_ID: u32 = 24
const OBJECT_STORE_ENDPOINT_ID: u32 = 25
const CONNECTION_ENDPOINT_ID: u32 = 26
const UPDATE_STORE_ENDPOINT_ID: u32 = 27
const LAUNCHER_ENDPOINT_ID: u32 = 28
const DISPLAY_ENDPOINT_ID: u32 = 29

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

struct ServiceDescriptor {
    label: str
    slot: ServiceSlot
    restart: ServiceRestartMode
    authority: ServiceAuthorityClass
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
const LEASE_SLOT: ServiceSlot = { endpoint: LEASE_ENDPOINT_ID, pid: 14 }
const COMPLETION_MAILBOX_SLOT: ServiceSlot = { endpoint: COMPLETION_MAILBOX_ENDPOINT_ID, pid: 15 }
const OBJECT_STORE_SLOT: ServiceSlot = { endpoint: OBJECT_STORE_ENDPOINT_ID, pid: 16 }
const CONNECTION_SLOT: ServiceSlot = { endpoint: CONNECTION_ENDPOINT_ID, pid: 17 }
const UPDATE_STORE_SLOT: ServiceSlot = { endpoint: UPDATE_STORE_ENDPOINT_ID, pid: 18 }
const LAUNCHER_SLOT: ServiceSlot = { endpoint: LAUNCHER_ENDPOINT_ID, pid: 19 }
const DISPLAY_SLOT: ServiceSlot = { endpoint: DISPLAY_ENDPOINT_ID, pid: 20 }

const SERVICE_SLOTS: [20]ServiceSlot = {
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
    WORKFLOW_SLOT,
    LEASE_SLOT,
    COMPLETION_MAILBOX_SLOT,
    OBJECT_STORE_SLOT,
    CONNECTION_SLOT,
    UPDATE_STORE_SLOT,
    LAUNCHER_SLOT,
    DISPLAY_SLOT
}

const INVALID_SERVICE_DESCRIPTOR: ServiceDescriptor = {
    label: "",
    slot: { endpoint: 0, pid: 0 },
    restart: ServiceRestartMode.None,
    authority: ServiceAuthorityClass.None
}

const SERVICE_DESCRIPTORS: [20]ServiceDescriptor = {
    { label: "serial", slot: SERIAL_SLOT, restart: ServiceRestartMode.None, authority: ServiceAuthorityClass.PublicEndpoint },
    { label: "shell", slot: SHELL_SLOT, restart: ServiceRestartMode.None, authority: ServiceAuthorityClass.ShellControl },
    { label: "log", slot: LOG_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.RetainedOwner },
    { label: "kv", slot: KV_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.RetainedOwner },
    { label: "echo", slot: ECHO_SLOT, restart: ServiceRestartMode.Reset, authority: ServiceAuthorityClass.PublicEndpoint },
    { label: "transfer", slot: TRANSFER_SLOT, restart: ServiceRestartMode.Reset, authority: ServiceAuthorityClass.TransferOnly },
    { label: "queue", slot: QUEUE_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.RetainedOwner },
    { label: "ticket", slot: TICKET_SLOT, restart: ServiceRestartMode.Reset, authority: ServiceAuthorityClass.PublicEndpoint },
    { label: "file", slot: FILE_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.RetainedOwner },
    { label: "timer", slot: TIMER_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.RetainedOwner },
    { label: "task", slot: TASK_SLOT, restart: ServiceRestartMode.Reset, authority: ServiceAuthorityClass.PublicEndpoint },
    { label: "journal", slot: JOURNAL_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.DurableOwner },
    { label: "workflow", slot: WORKFLOW_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.RetainedOwner },
    { label: "lease", slot: LEASE_SLOT, restart: ServiceRestartMode.Reset, authority: ServiceAuthorityClass.RetainedOwner },
    { label: "completion_mailbox", slot: COMPLETION_MAILBOX_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.RetainedOwner },
    { label: "object_store", slot: OBJECT_STORE_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.DurableOwner },
    { label: "connection", slot: CONNECTION_SLOT, restart: ServiceRestartMode.Reset, authority: ServiceAuthorityClass.PublicEndpoint },
    { label: "update_store", slot: UPDATE_STORE_SLOT, restart: ServiceRestartMode.Reload, authority: ServiceAuthorityClass.DurableOwner },
    { label: "launcher", slot: LAUNCHER_SLOT, restart: ServiceRestartMode.Reset, authority: ServiceAuthorityClass.PublicEndpoint },
    { label: "display", slot: DISPLAY_SLOT, restart: ServiceRestartMode.Reset, authority: ServiceAuthorityClass.PublicEndpoint }
}

// SERVICE_COUNT is the number of boot-wired services in the static topology.
// Increment this when a new slot constant is added above.
const SERVICE_COUNT: u32 = 20

func service_count() usize {
    return usize(SERVICE_COUNT)
}

func service_descriptor_at(index: usize) ServiceDescriptor {
    if index >= service_count() {
        return INVALID_SERVICE_DESCRIPTOR
    }
    return SERVICE_DESCRIPTORS[index]
}

func service_descriptor_for_endpoint(endpoint: u32) ServiceDescriptor {
    for i in 0..service_count() {
        desc := service_descriptor_at(i)
        if desc.slot.endpoint == endpoint {
            return desc
        }
    }
    return INVALID_SERVICE_DESCRIPTOR
}

func service_slot_at(index: usize) ServiceSlot {
    return service_descriptor_at(index).slot
}

func service_slot_for_endpoint(endpoint: u32) ServiceSlot {
    return service_descriptor_for_endpoint(endpoint).slot
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
    return service_descriptor_for_endpoint(endpoint).restart
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

// Retained-summary participation is a topology fact for boot-wired services:
// retained owners and durable owners carry state across the current admitted
// lifecycle surfaces, while ordinary public endpoints do not.
func service_is_retained(endpoint: u32) bool {
    class: ServiceAuthorityClass = service_authority_class(endpoint)
    if class == ServiceAuthorityClass.RetainedOwner {
        return true
    }
    if class == ServiceAuthorityClass.DurableOwner {
        return true
    }
    return false
}

func service_authority_class(endpoint: u32) ServiceAuthorityClass {
    return service_descriptor_for_endpoint(endpoint).authority
}

func service_label_for_endpoint(endpoint: u32) str {
    return service_descriptor_for_endpoint(endpoint).label
}

// endpoint_is_boot_wired returns true when the endpoint id names one of the
// boot-wired public services.  This is the explicit addressing
// gate: the dispatcher uses this to separate known public names from unknown
// ids before any service-specific routing.  All boot-wired endpoints are
// publicly addressable.
func endpoint_is_boot_wired(endpoint: u32) bool {
    return service_slot_is_valid(service_slot_for_endpoint(endpoint))
}
