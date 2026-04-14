// Phase 173 SMP model boundary identification.
//
// Named constraints:
//
//   A. Unified dispatch state pointer.
//      kernel_dispatch_step(state: *boot.KernelBootState, ...) takes one
//      mutable pointer to the entire kernel state.  Every dispatch path --
//      leaf, kv, effect-chain -- reads *state, calls a service, then writes
//      the new state back through the same pointer.  Under SMP, two cores
//      calling kernel_dispatch_step with the same pointer concurrently race
//      on the write-back.  The current model relies on single-core sequential
//      composition: each step sees the full effect of the previous step.
//
//   B. Advisory-first compound dispatch.
//      kernel_dispatch_kv executes two dependent dispatches (kv write then
//      advisory log append) and commits both updates to *state as one
//      compound assignment.  Under SMP, another core can observe *state
//      between the two sub-dispatches: kv updated, log not yet.  Single-core
//      sequential dispatch makes this invisible to callers.
//
// Why neither constraint blocks single-core work:
//   Sequential dispatch over a shared *state pointer is exactly the current
//   model's assumption.  Every dispatch observes the full prior state before
//   it writes; no partial state is visible to any caller.
//
// Proof structure:
//
//   smoke_sequential_state_accumulation:
//     Dispatch three writes over the same state pointer.  Verify each step
//     sees the full effect of the prior step.  This is the positive proof of
//     the current model: sequential composition works because the pointer is
//     shared and each step reads the state left by the last.
//
//   smoke_state_pointer_is_the_shared_mutable_root:
//     Take two snapshots of KernelBootState before and after a dispatch.
//     Verify the before-snapshot is unaffected by the dispatch — the state
//     value does not change, only the state through the pointer does.  This
//     makes constraint A concrete: two callers with independent snapshots
//     would each write back their own view, discarding the other's update.
//
//   smoke_advisory_compound_is_invisible_to_caller:
//     Dispatch a kv write when the log is full.  Verify the kv reply is Ok
//     and the log remains at capacity.  The compound (kv + advisory) was
//     committed in one step from the caller's perspective.  Under SMP,
//     the window between the two sub-commits is where constraint B lives.

import boot
import init
import kernel_dispatch
import serial_protocol
import service_effect
import service_topology
import syscall

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// Constraint A proof: sequential dispatch over a shared pointer accumulates
// state correctly.  Each step sees the full effect of the previous one.
// This is the positive proof of the single-core model's assumption.
func smoke_sequential_state_accumulation() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Write three distinct kv entries sequentially.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(1, 11)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(2, 22)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(3, 33)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Count must be 3: each step's write persisted for the next.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_count()))
    if service_effect.effect_reply_payload_len(effect) != 3 {
        return false
    }

    // All three values are readable: sequential accumulation is complete.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_get(1)))
    if service_effect.effect_reply_payload(effect)[1] != 11 {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_get(2)))
    if service_effect.effect_reply_payload(effect)[1] != 22 {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_get(3)))
    if service_effect.effect_reply_payload(effect)[1] != 33 {
        return false
    }

    return true
}

// Constraint A proof: the state value at the call site does not change after
// a dispatch -- only the state through the pointer does.  Two callers with
// independent copies of the state before the dispatch would each write back
// their own view, discarding the other's update.  This is the named race.
func smoke_state_pointer_is_the_shared_mutable_root() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Write an entry so the state is non-trivial.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(7, 77)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Take a snapshot of the state value before the next dispatch.
    // This snapshot is a copy: it holds the state as of after the first write.
    snapshot: boot.KernelBootState = state

    // Dispatch a second write through the state pointer.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(8, 88)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // The live state now has two entries.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_count()))
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }

    // The snapshot still reports one entry -- the value copy was not updated.
    // Under SMP, if a second core held a pointer to its own snapshot and
    // dispatched concurrently, its write-back would discard the first core's
    // second entry.  This is constraint A: the pointer is the shared root.
    effect = kernel_dispatch.kernel_dispatch_step(&snapshot, cmd(serial_protocol.encode_kv_count()))
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return false
    }

    return true
}

// Constraint B proof: kv write when the log is full returns Ok while the
// advisory log append is silently dropped.  From the caller's view the
// compound dispatch is one atomic step.  Under SMP the window between the
// kv commit and the advisory log commit is where constraint B lives.
func smoke_advisory_compound_is_invisible_to_caller() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill the log to capacity.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(10)))
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(20)))
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(30)))
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(40)))

    // Verify log is at capacity.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }

    // kv write when log is full: advisory append is dropped but kv reply is Ok.
    // The caller cannot observe the dropped advisory from the reply alone.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(5, 55)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Log still at 4: the advisory was silently dropped by the compound.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }

    // kv has the entry: the kv half of the compound committed successfully.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_get(5)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 55 {
        return false
    }

    return true
}

func main() i32 {
    if smoke_sequential_state_accumulation() == false {
        return 1
    }
    if smoke_state_pointer_is_the_shared_mutable_root() == false {
        return 2
    }
    if smoke_advisory_compound_is_invisible_to_caller() == false {
        return 3
    }
    return 0
}
