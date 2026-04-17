// Shell-visible compact service-state probes.

import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_state
import syscall

const FAIL_STATE_SERIAL: i32 = 311
const FAIL_STATE_ECHO: i32 = 312
const FAIL_STATE_QUEUE: i32 = 313
const FAIL_STATE_WORKSET: i32 = 314

func run_service_state_probe() i32 {
    fixed: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&fixed, scenario_transport.cmd_lifecycle_state(serial_protocol.TARGET_SERIAL))
    if !scenario_assert.expect_service_state(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_SERIAL, service_state.STATE_CLASS_FIXED, service_state.STATE_MODE_NONE, service_state.STATE_PARTICIPATION_NONE, service_state.STATE_POLICY_NONE, 0, 0) {
        return FAIL_STATE_SERIAL
    }

    ordinary: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return FAIL_STATE_ECHO
    }
    effect = kernel_dispatch.kernel_dispatch_step(&ordinary, scenario_transport.cmd_lifecycle_state(serial_protocol.TARGET_ECHO))
    if !scenario_assert.expect_service_state(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, service_state.STATE_CLASS_ORDINARY, service_state.STATE_MODE_RESET, service_state.STATE_PARTICIPATION_NONE, service_state.STATE_POLICY_CLEAR, 0, 2) {
        return FAIL_STATE_ECHO
    }

    retained: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&retained, scenario_transport.cmd_queue_enqueue(10))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_STATE_QUEUE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&retained, scenario_transport.cmd_queue_enqueue(20))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_STATE_QUEUE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&retained, scenario_transport.cmd_lifecycle_state(serial_protocol.TARGET_QUEUE))
    if !scenario_assert.expect_service_state(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, service_state.STATE_CLASS_RETAINED, service_state.STATE_MODE_RELOAD, service_state.STATE_PARTICIPATION_SERVICE, service_state.STATE_POLICY_KEEP, 2, 1) {
        return FAIL_STATE_QUEUE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&retained, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_QUEUE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_STATE_QUEUE
    }
    effect = kernel_dispatch.kernel_dispatch_step(&retained, scenario_transport.cmd_lifecycle_state(serial_protocol.TARGET_QUEUE))
    if !scenario_assert.expect_service_state(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, service_state.STATE_CLASS_RETAINED, service_state.STATE_MODE_RELOAD, service_state.STATE_PARTICIPATION_SERVICE, service_state.STATE_POLICY_KEEP, 2, 2) {
        return FAIL_STATE_QUEUE
    }

    lane: boot.KernelBootState = boot.kernel_init()
    effect = kernel_dispatch.kernel_dispatch_step(&lane, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_STATE_WORKSET
    }
    effect = kernel_dispatch.kernel_dispatch_step(&lane, scenario_transport.cmd_lifecycle_state(serial_protocol.TARGET_WORKSET))
    if !scenario_assert.expect_service_state(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKSET, service_state.STATE_CLASS_LANE, service_state.STATE_MODE_RELOAD, service_state.STATE_PARTICIPATION_LANE, service_state.STATE_POLICY_KEEP, 0, 2) {
        return FAIL_STATE_WORKSET
    }

    return 0
}