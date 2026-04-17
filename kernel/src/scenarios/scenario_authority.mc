// Shell-visible authority inspection probes.

import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall

const FAIL_AUTHORITY_SERIAL: i32 = 301
const FAIL_AUTHORITY_SHELL: i32 = 302
const FAIL_AUTHORITY_KV: i32 = 303
const FAIL_AUTHORITY_TRANSFER: i32 = 304
const FAIL_AUTHORITY_WORKSET: i32 = 305

func run_authority_probe() i32 {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_authority(serial_protocol.TARGET_SERIAL))
    if !scenario_assert.expect_authority(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_SERIAL, serial_protocol.AUTHORITY_PUBLIC, serial_protocol.AUTHORITY_TRANSFER_NO, serial_protocol.AUTHORITY_SCOPE_NONE) {
        return FAIL_AUTHORITY_SERIAL
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_authority(serial_protocol.TARGET_SHELL))
    if !scenario_assert.expect_authority(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_SHELL, serial_protocol.AUTHORITY_SHELL, serial_protocol.AUTHORITY_TRANSFER_NO, serial_protocol.AUTHORITY_SCOPE_NONE) {
        return FAIL_AUTHORITY_SHELL
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_authority(serial_protocol.TARGET_KV))
    if !scenario_assert.expect_authority(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_KV, serial_protocol.AUTHORITY_RETAINED, serial_protocol.AUTHORITY_TRANSFER_NO, serial_protocol.AUTHORITY_SCOPE_RETAINED) {
        return FAIL_AUTHORITY_KV
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_authority(serial_protocol.TARGET_TRANSFER))
    if !scenario_assert.expect_authority(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_TRANSFER, serial_protocol.AUTHORITY_TRANSFER, serial_protocol.AUTHORITY_TRANSFER_YES, serial_protocol.AUTHORITY_SCOPE_NONE) {
        return FAIL_AUTHORITY_TRANSFER
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_authority(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_authority(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.AUTHORITY_COORDINATED, serial_protocol.AUTHORITY_TRANSFER_NO, serial_protocol.AUTHORITY_SCOPE_COORDINATED) {
        return FAIL_AUTHORITY_WORKSET
    }

    return 0
}