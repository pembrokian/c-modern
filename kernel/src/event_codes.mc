// Observable event codes for the serial-shell dispatch path.
//
// Owned here so that kernel_dispatch.mc (producer) and
// serial_shell_event_log.mc (consumer) reference the same values
// without either importing the other — the same dual-import pattern
// service_topology.mc uses for endpoint IDs.
//
// Adding a new observable event type means adding one constant here.
// All three former duplication sites (kernel_dispatch emit, event_log
// constants, and test fixture references) become single-point imports.

const EVENT_SERIAL_BUFFERED: u32 = 1
const EVENT_SERIAL_REJECTED: u32 = 2
const EVENT_SHELL_FORWARDED: u32 = 3
const EVENT_SHELL_REPLY_OK: u32 = 4
const EVENT_SHELL_REPLY_INVALID: u32 = 5
const EVENT_SERIAL_CLEARED: u32 = 6
