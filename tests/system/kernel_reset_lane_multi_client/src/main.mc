// Phase 156 multi-client interaction audit.
//
// This fixture proves the ordering and state-sharing contracts that become
// visible when two independent clients interact with the same service path.
//
// Key findings:
//
//   Shared state (no per-client isolation):
//     kv_service and log_service hold one state value that is global to the
//     service.  A write from client A is immediately visible to client B on
//     the very next dispatch step.  There is no per-client key namespace,
//     per-client log buffer, or session concept.
//
//   Reply routing by call ordering, not by source_pid:
//     The reply is the return value of kernel_dispatch_step.  Whoever owns
//     that return value owns the reply.  source_pid flows through the message
//     struct for observability but plays no role in routing the reply back.
//     If two clients were truly concurrent, a reply mailbox keyed on
//     source_pid would be required.  That is the first assumption that would
//     fail under real concurrency.
//
//   Sequential ordering wins:
//     Whichever dispatch step runs last wins.  A writes 3=10, then B writes
//     3=20; A then reads 3 and sees 20 — last writer wins, with no
//     read-your-writes isolation.
//
//   Backpressure is per service, not per client:
//     A fill from client A leaves no room for client B.  The Exhausted reply
//     is returned to whoever submits the overflowing step, regardless of pid.

import boot
import kernel_dispatch
import ipc
import kv_service
import log_service
import service_effect
import service_topology
import syscall


func obs(pid: u32, b0: u8, b1: u8, b2: u8, b3: u8) syscall.ReceiveObservation {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = b0
    payload[1] = b1
    payload[2] = b2
    payload[3] = b3
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: pid, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// Client A writes kv[7]=11.  Client B reads kv[7].  B sees A's value: shared
// state, no per-client namespace.
func smoke_shared_kv_state() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Client A: KS 7 11
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(1, 75, 83, 7, 11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Client B: KG 7
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(99, 75, 71, 7, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 11 {
        return false
    }
    return true
}

// Client A writes 3=10.  Client B writes 3=20.  Client A reads 3 and sees 20.
// Last writer wins; no per-client isolation.
func smoke_last_writer_wins() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // A: KS 3 10
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(1, 75, 83, 3, 10))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // B: KS 3 20  (overwrites A's value)
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(99, 75, 83, 3, 20))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // A: KG 3 — sees B's value, not its own
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(1, 75, 71, 3, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 20 {
        return false
    }
    return true
}

// Client A fills the log.  Client B's append gets Exhausted.  Backpressure is
// per service, not per client.
func smoke_backpressure_is_per_service() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Client A fills all 4 slots.  LA <byte> !
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(1, 76, 65, 1, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(1, 76, 65, 2, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(1, 76, 65, 3, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(1, 76, 65, 4, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Client B appends: log is full regardless of who asks.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(99, 76, 65, 5, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }
    if log_service.log_len(state.log.state) != 4 {
        return false
    }
    return true
}

// Client A appends to the log.  Client B's tail sees A's entry.  Log state is
// globally shared; no per-client append buffer exists.
func smoke_shared_log_state() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Client A: LA <65> !
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(1, 76, 65, 65, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Client B: LT ! !
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(99, 76, 84, 33, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 65 {
        return false
    }
    return true
}

// Reply routing: the effect returned by kernel_dispatch_step is the reply for
// whoever submitted that step.  source_pid does not route the reply; it just
// flows through for observability.  Two clients submitting steps in sequence
// each receive their reply as the return value of their own step.
func smoke_reply_is_return_value_of_step() bool {
    state: boot.KernelBootState = boot.kernel_init()

    // A writes, B reads in alternation.  Each gets the right reply from its
    // own step — no cross-contamination.
    effect_a_write: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs(1, 75, 83, 9, 7))
    if service_effect.effect_reply_status(effect_a_write) != syscall.SyscallStatus.Ok {
        return false
    }

    effect_b_read: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs(99, 75, 71, 9, 33))
    if service_effect.effect_reply_status(effect_b_read) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect_b_read)[1] != 7 {
        return false
    }

    // A's earlier write reply is still Ok; B's read reply reflects the write.
    if service_effect.effect_reply_status(effect_a_write) != syscall.SyscallStatus.Ok {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_shared_kv_state() {
        return 1
    }
    if !smoke_last_writer_wins() {
        return 2
    }
    if !smoke_backpressure_is_per_service() {
        return 3
    }
    if !smoke_shared_log_state() {
        return 4
    }
    if !smoke_reply_is_return_value_of_step() {
        return 5
    }
    return 0
}
