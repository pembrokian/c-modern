// Retained/restart comparison probes for restart outcomes and retained participation.

import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall

const FAIL_COMPARE_FRESH_LOG: i32 = 227
const FAIL_COMPARE_FRESH_WORKSET: i32 = 228
const FAIL_COMPARE_ORDINARY_STATUS: i32 = 229
const FAIL_COMPARE_ORDINARY_AFTER: i32 = 230
const FAIL_COMPARE_RETAINED_STATUS: i32 = 231
const FAIL_COMPARE_RETAINED_AFTER: i32 = 232
const FAIL_COMPARE_WORKSET_STATUS: i32 = 233
const FAIL_COMPARE_WORKSET_AFTER: i32 = 234
const FAIL_COMPARE_KV_AFTER_WORKSET: i32 = 235
const FAIL_COMPARE_AUDIT_STATUS: i32 = 236
const FAIL_COMPARE_AUDIT_AFTER: i32 = 237
const FAIL_COMPARE_LOG_AFTER_AUDIT: i32 = 238

func run_retained_restart_identity_probe() i32 {
    fresh: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&fresh, scenario_transport.cmd_lifecycle_compare(serial_protocol.TARGET_LOG))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.None, 1) {
        return FAIL_COMPARE_FRESH_LOG
    }
    effect = kernel_dispatch.kernel_dispatch_step(&fresh, scenario_transport.cmd_lifecycle_compare(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.None, 1) {
        return FAIL_COMPARE_FRESH_WORKSET
    }

    ordinary: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_COMPARE_ORDINARY_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_compare(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.None, boot.RestartOutcome.OrdinaryReplaced, 2) {
        return FAIL_COMPARE_ORDINARY_AFTER
    }

    retained: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&retained, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LOG))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LOG, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_COMPARE_RETAINED_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&retained, scenario_transport.cmd_lifecycle_compare(serial_protocol.TARGET_LOG))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.RetainedReloaded, 2) {
        return FAIL_COMPARE_RETAINED_AFTER
    }

    workset: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_COMPARE_WORKSET_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_compare(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_COMPARE_WORKSET_AFTER
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_compare(serial_protocol.TARGET_KV))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_COMPARE_KV_AFTER_WORKSET
    }

    audit: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_COMPARE_AUDIT_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_compare(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_COMPARE_AUDIT_AFTER
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_compare(serial_protocol.TARGET_LOG))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_COMPARE_LOG_AFTER_AUDIT
    }

    return 0
}