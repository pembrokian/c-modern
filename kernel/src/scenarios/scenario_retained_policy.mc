// Retained policy probes for explicit keep-versus-clear inspection.

import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall

const FAIL_POLICY_WORKSET_QUERY: i32 = 219
const FAIL_POLICY_WORKSET_RESTART: i32 = 220
const FAIL_POLICY_WORKSET_SUMMARY: i32 = 221
const FAIL_POLICY_AUDIT_QUERY: i32 = 222
const FAIL_POLICY_AUDIT_RESTART: i32 = 223
const FAIL_POLICY_AUDIT_SUMMARY: i32 = 224
const FAIL_POLICY_ECHO_QUERY: i32 = 225
const FAIL_POLICY_ECHO_RESTART: i32 = 226
const FAIL_POLICY_ECHO_SUMMARY: i32 = 227

func run_retained_policy_probe() i32 {
    workset: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_policy(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_policy(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.TARGET_KV, serial_protocol.TARGET_QUEUE, serial_protocol.POLICY_KEEP) {
        return FAIL_POLICY_WORKSET_QUERY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_POLICY_WORKSET_RESTART
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_POLICY_WORKSET_SUMMARY
    }

    audit: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_policy(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_policy(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.TARGET_KV, serial_protocol.TARGET_LOG, serial_protocol.POLICY_KEEP) {
        return FAIL_POLICY_AUDIT_QUERY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_POLICY_AUDIT_RESTART
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_POLICY_AUDIT_SUMMARY
    }

    ordinary: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_policy(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_policy(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.TARGET_ECHO, serial_protocol.PARTICIPANT_NONE, serial_protocol.POLICY_CLEAR) {
        return FAIL_POLICY_ECHO_QUERY
    }
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_POLICY_ECHO_RESTART
    }
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.None, boot.RestartOutcome.OrdinaryReplaced, 2) {
        return FAIL_POLICY_ECHO_SUMMARY
    }

    return 0
}
