// Shell-visible coordinated retained workset identity probes.

import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall

const FAIL_WORKSET_IDENTITY_BEFORE: i32 = 176
const FAIL_WORKSET_IDENTITY_RESTART_STATUS: i32 = 177
const FAIL_WORKSET_IDENTITY_AFTER: i32 = 178
const FAIL_WORKSET_IDENTITY_COORDINATED_STEP: i32 = 179
const FAIL_WORKSET_IDENTITY_SINGLE_RESTART_STATUS: i32 = 180
const FAIL_WORKSET_IDENTITY_SINGLE_RESTART_STABLE: i32 = 181

func run_workset_identity_probe(state: *boot.KernelBootState) i32 {
    before_generation: u32 = boot.boot_workset_generation(*state)
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_identity(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_generation_payload(effect, syscall.SyscallStatus.Ok, before_generation) {
        return FAIL_WORKSET_IDENTITY_BEFORE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_WORKSET_IDENTITY_RESTART_STATUS
    }

    after_generation: u32 = boot.boot_workset_generation(*state)
    if after_generation != before_generation + 1 {
        return FAIL_WORKSET_IDENTITY_COORDINATED_STEP
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_identity(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_generation_payload(effect, syscall.SyscallStatus.Ok, after_generation) {
        return FAIL_WORKSET_IDENTITY_AFTER
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_KV))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_KV, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_WORKSET_IDENTITY_SINGLE_RESTART_STATUS
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_identity(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_generation_payload(effect, syscall.SyscallStatus.Ok, after_generation) {
        return FAIL_WORKSET_IDENTITY_SINGLE_RESTART_STABLE
    }

    return 0
}