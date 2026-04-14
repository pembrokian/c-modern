// Service topology: stable endpoint addressing constants for the kernel boot
// image.  This module owns the four endpoint IDs and has no service imports,
// so both boot.mc (which constructs services) and shell_service.mc (which
// sends to services) can import it without a circular dependency.
//
// Adding a new boot-wired service means adding one constant here.  All three
// former duplication sites (boot init, shell state, kernel_dispatch routing)
// reference this module directly.

const SERIAL_ENDPOINT_ID: u32 = 10
const SHELL_ENDPOINT_ID: u32 = 11
const LOG_ENDPOINT_ID: u32 = 12
const KV_ENDPOINT_ID: u32 = 13
