import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import service_effect
import service_topology
import syscall
import update_store_service
import workflow/core

struct ScenarioKernelSetupResult {
    ok: bool
    state: boot.KernelBootState
}

func scenario_setup_clean_kernel() ScenarioKernelSetupResult {
    ok := update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1))
    return ScenarioKernelSetupResult{ ok: ok, state: boot.kernel_init() }
}

func scenario_apply_issue_rollup_update(state: *boot.KernelBootState, version: u8, b0: u8, b1: u8, b2: u8, fail_base: i32) i32 {
    effect := kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_update_stage(b0))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return fail_base
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_update_stage(b1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return fail_base + 1
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_update_stage(b2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return fail_base + 2
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_update_manifest(version, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return fail_base + 3
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_workflow_apply_update())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return fail_base + 4
    }
    apply_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_WAITING, workflow_core.WORKFLOW_RESTART_NONE) {
        return fail_base + 5
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_workflow_query(apply_id))
    if !scenario_assert.expect_workflow_state(effect, syscall.SyscallStatus.Ok, workflow_core.WORKFLOW_STATE_UPDATE_APPLIED, workflow_core.WORKFLOW_RESTART_NONE) {
        return fail_base + 6
    }

    return 0
}