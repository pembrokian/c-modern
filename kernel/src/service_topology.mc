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

const SERIAL_ENDPOINT_ID: u32 = 10
const SHELL_ENDPOINT_ID: u32 = 11
const LOG_ENDPOINT_ID: u32 = 12
const KV_ENDPOINT_ID: u32 = 13

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

// SERVICE_COUNT is the number of boot-wired services in the static topology.
// Increment this when a new slot constant is added above.
const SERVICE_COUNT: u32 = 4
