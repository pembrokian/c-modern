import boot
import kernel_dispatch
import serial_protocol
import service_effect
import service_topology
import syscall

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func expect_lifecycle(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, mode: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != target {
        return false
    }
    if payload[1] != mode {
        return false
    }
    return true
}

func expect_summary(effect: service_effect.Effect, participation: boot.RetainedSummaryParticipation, outcome: boot.RestartOutcome, generation: u32) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = boot.summary_payload(participation, outcome, generation)
    if payload[0] != expected[0] {
        return false
    }
    if payload[1] != expected[1] {
        return false
    }
    if payload[2] != expected[2] {
        return false
    }
    if payload[3] != expected[3] {
        return false
    }
    return true
}

func smoke_retained_summary_tracks_restart_outcomes() bool {
    effect: service_effect.Effect

    fresh: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&fresh, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_LOG)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.None, 1) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&fresh, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_WORKSET)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.None, 1) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&fresh, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_AUDIT)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.None, 1) {
        return false
    }

    ordinary: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_ECHO)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_ECHO)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.None, boot.RestartOutcome.OrdinaryReplaced, 2) {
        return false
    }

    retained: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&retained, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_LOG)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LOG, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&retained, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_LOG)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.RetainedReloaded, 2) {
        return false
    }

    workset: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&workset, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_WORKSET)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_WORKSET)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_KV)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_QUEUE)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return false
    }

    audit: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&audit, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_AUDIT)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_AUDIT)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_LOG)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, cmd(serial_protocol.encode_lifecycle_summary(serial_protocol.TARGET_KV)))
    if !expect_summary(effect, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_retained_summary_tracks_restart_outcomes() {
        return 1
    }
    return 0
}