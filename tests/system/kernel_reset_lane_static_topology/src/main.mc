// Phase 170: Static service topology first acknowledgment.
//
// Proves that the static wiring of the boot services is now named
// in one place (service_topology.ServiceSlot) rather than split between
// endpoint ID constants in service_topology.mc and PID constants in boot.mc.
//
// Three checks:
//   A. Slot endpoint fields match the raw endpoint ID constants.
//   B. SERVICE_COUNT equals the number of wired slots.
//   C. kernel_init() produces working state that accepts a round-trip
//      command — confirming that slot-based init is functionally identical
//      to the former hard-coded PID init.

import boot
import kernel_dispatch
import serial_protocol
import service_effect
import service_topology
import syscall

// A: each slot's endpoint field must equal the corresponding endpoint ID
// constant.  This proves the slot records are consistent with the existing
// direct-constant users.
func smoke_slot_endpoints_match_constants() bool {
    serial_slot: service_topology.ServiceSlot = service_topology.SERIAL_SLOT
    shell_slot: service_topology.ServiceSlot = service_topology.SHELL_SLOT
    log_slot: service_topology.ServiceSlot = service_topology.LOG_SLOT
    kv_slot: service_topology.ServiceSlot = service_topology.KV_SLOT
    echo_slot: service_topology.ServiceSlot = service_topology.ECHO_SLOT
    transfer_slot: service_topology.ServiceSlot = service_topology.TRANSFER_SLOT
    queue_slot: service_topology.ServiceSlot = service_topology.QUEUE_SLOT
    ticket_slot: service_topology.ServiceSlot = service_topology.TICKET_SLOT
    file_slot: service_topology.ServiceSlot = service_topology.FILE_SLOT
    timer_slot: service_topology.ServiceSlot = service_topology.TIMER_SLOT
    task_slot: service_topology.ServiceSlot = service_topology.TASK_SLOT

    if serial_slot.endpoint != service_topology.SERIAL_ENDPOINT_ID {
        return false
    }
    if shell_slot.endpoint != service_topology.SHELL_ENDPOINT_ID {
        return false
    }
    if log_slot.endpoint != service_topology.LOG_ENDPOINT_ID {
        return false
    }
    if kv_slot.endpoint != service_topology.KV_ENDPOINT_ID {
        return false
    }
    if echo_slot.endpoint != service_topology.ECHO_ENDPOINT_ID {
        return false
    }
    if transfer_slot.endpoint != service_topology.TRANSFER_ENDPOINT_ID {
        return false
    }
    if queue_slot.endpoint != service_topology.QUEUE_ENDPOINT_ID {
        return false
    }
    if ticket_slot.endpoint != service_topology.TICKET_ENDPOINT_ID {
        return false
    }
    if file_slot.endpoint != service_topology.FILE_ENDPOINT_ID {
        return false
    }
    if timer_slot.endpoint != service_topology.TIMER_ENDPOINT_ID {
        return false
    }
    if task_slot.endpoint != service_topology.TASK_ENDPOINT_ID {
        return false
    }
    return true
}

// B: SERVICE_COUNT must equal 12 — the number of boot-wired slots.
func smoke_service_count_is_twelve() bool {
    if service_topology.SERVICE_COUNT != 12 {
        return false
    }
    return true
}

// C: kernel_init() uses the slot PIDs under the hood; confirm a basic
// log-append round-trip still returns Ok.
func smoke_init_and_round_trip() bool {
    state: boot.KernelBootState = boot.kernel_init()
    obs: syscall.ReceiveObservation = syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: serial_protocol.encode_log_append(7) }
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs)
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    return true
}

func main() i32 {
    if smoke_slot_endpoints_match_constants() == false {
        return 1
    }
    if smoke_service_count_is_twelve() == false {
        return 2
    }
    if smoke_init_and_round_trip() == false {
        return 3
    }
    return 0
}
