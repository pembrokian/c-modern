import boot
import kernel_dispatch
import scenario_transport
import serial_protocol
import service_effect
import syscall
import workflow_core

func expect_state(effect: service_effect.Effect, state: u8, restart: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == state && payload[1] == restart
}

func smoke_workflow_progress_and_restart_boundary() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(1, 2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_state(effect, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_state(effect, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_state(effect, workflow_core.WORKFLOW_STATE_RUNNING, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(1))
    if !expect_state(effect, workflow_core.WORKFLOW_STATE_DONE, workflow_core.WORKFLOW_RESTART_RESUMED) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(2, 2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKSET))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(2))
    if !expect_state(effect, workflow_core.WORKFLOW_STATE_CANCELLED, workflow_core.WORKFLOW_RESTART_CANCELLED) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_workflow_progress_and_restart_boundary() {
        return 1
    }
    return 0
}