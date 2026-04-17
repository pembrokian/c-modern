// Retained-versus-durable boundary probe.

import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import syscall

const FAIL_DURABILITY_SERIAL: i32 = 321
const FAIL_DURABILITY_QUEUE: i32 = 322
const FAIL_DURABILITY_WORKSET: i32 = 323
const FAIL_DURABILITY_JOURNAL: i32 = 324

func run_retained_durable_boundary_probe() i32 {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_durability(serial_protocol.TARGET_SERIAL))
    if !scenario_assert.expect_durability(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_SERIAL) {
        return FAIL_DURABILITY_SERIAL
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_durability(serial_protocol.TARGET_QUEUE))
    if !scenario_assert.expect_durability(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE) {
        return FAIL_DURABILITY_QUEUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_durability(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_durability(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET) {
        return FAIL_DURABILITY_WORKSET
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_durability(serial_protocol.TARGET_JOURNAL))
    if !scenario_assert.expect_durability(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_JOURNAL) {
        return FAIL_DURABILITY_JOURNAL
    }

    return 0
}
