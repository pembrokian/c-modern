// Retained summary probes for restart outcomes and retained participation.

import boot
import kernel_dispatch
import object_store_service
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall

const FAIL_SUMMARY_FRESH_LOG: i32 = 207
const FAIL_SUMMARY_FRESH_WORKSET: i32 = 208
const FAIL_SUMMARY_ORDINARY_STATUS: i32 = 209
const FAIL_SUMMARY_ORDINARY_AFTER: i32 = 210
const FAIL_SUMMARY_RETAINED_STATUS: i32 = 211
const FAIL_SUMMARY_RETAINED_AFTER: i32 = 212
const FAIL_SUMMARY_WORKSET_STATUS: i32 = 213
const FAIL_SUMMARY_WORKSET_AFTER: i32 = 214
const FAIL_SUMMARY_KV_AFTER_WORKSET: i32 = 215
const FAIL_SUMMARY_AUDIT_STATUS: i32 = 216
const FAIL_SUMMARY_AUDIT_AFTER: i32 = 217
const FAIL_SUMMARY_LOG_AFTER_AUDIT: i32 = 218
const FAIL_SUMMARY_OBJECT_STORE_STATUS: i32 = 219
const FAIL_SUMMARY_OBJECT_STORE_AFTER: i32 = 220

func run_retained_summary_probe() i32 {
    if !object_store_service.object_store_persist(object_store_service.object_store_init(service_topology.OBJECT_STORE_SLOT.pid, 1)) {
        return FAIL_SUMMARY_OBJECT_STORE_STATUS
    }

    durable: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&durable, scenario_transport.cmd_object_create(1, 55))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_SUMMARY_OBJECT_STORE_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&durable, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_OBJECT_STORE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_OBJECT_STORE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_SUMMARY_OBJECT_STORE_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&durable, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_OBJECT_STORE))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.DurableReloaded, 2) {
        return FAIL_SUMMARY_OBJECT_STORE_AFTER
    }

    fresh: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&fresh, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_LOG))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.None, 1) {
        return FAIL_SUMMARY_FRESH_LOG
    }
    effect = kernel_dispatch.kernel_dispatch_step(&fresh, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.None, 1) {
        return FAIL_SUMMARY_FRESH_WORKSET
    }

    ordinary: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_SUMMARY_ORDINARY_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.None, boot.RestartOutcome.OrdinaryReplaced, 2) {
        return FAIL_SUMMARY_ORDINARY_AFTER
    }

    retained: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&retained, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_LOG))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LOG, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_SUMMARY_RETAINED_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&retained, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_LOG))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.RetainedReloaded, 2) {
        return FAIL_SUMMARY_RETAINED_AFTER
    }

    workset: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_SUMMARY_WORKSET_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_SUMMARY_WORKSET_AFTER
    }
    effect = kernel_dispatch.kernel_dispatch_step(&workset, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_KV))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_SUMMARY_KV_AFTER_WORKSET
    }

    audit: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_SUMMARY_AUDIT_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Lane, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_SUMMARY_AUDIT_AFTER
    }
    effect = kernel_dispatch.kernel_dispatch_step(&audit, scenario_transport.cmd_lifecycle_summary(serial_protocol.TARGET_LOG))
    if !scenario_assert.expect_summary(effect, syscall.SyscallStatus.Ok, boot.RetainedSummaryParticipation.Service, boot.RestartOutcome.CoordinatedRetainedReloaded, 2) {
        return FAIL_SUMMARY_LOG_AFTER_AUDIT
    }

    return 0
}