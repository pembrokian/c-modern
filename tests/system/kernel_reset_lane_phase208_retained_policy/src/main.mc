import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall

func smoke_retained_policy_matches_restart_outcomes() bool {
    workset: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_policy(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_policy(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.TARGET_KV, serial_protocol.TARGET_QUEUE, serial_protocol.POLICY_KEEP) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return false
    }

    audit: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_policy(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_policy(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.TARGET_KV, serial_protocol.TARGET_LOG, serial_protocol.POLICY_KEEP) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return false
    }

    ordinary: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_policy(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_policy(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.TARGET_ECHO, serial_protocol.PARTICIPANT_NONE, serial_protocol.POLICY_CLEAR) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.None, boot.RestartOutcome.OrdinaryReplaced, 2) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_retained_policy_matches_restart_outcomes() {
        return 1
    }
    return 0
}
