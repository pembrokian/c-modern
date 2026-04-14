import init
import kv_service
import log_service

func phase152_restart_log_service(system: Phase150RebuiltSystemState, init_pid: u32, log_service_directory_key: u32, shared_control_endpoint_id: u32) Phase150RebuiltSystemRestartResult {
    restart: init.ServiceRestartObservation = init.observe_service_restart(init_pid, log_service_directory_key, system.log_state.owner_pid, system.log_state.owner_pid, shared_control_endpoint_id, 1, 1)
    restarted_log_state: log_service.LogServiceState = log_service.service_state(system.log_state.owner_pid, system.log_state.endpoint_handle_slot)
    return phase150_rebuilt_system_restart_result(phase150_rebuilt_system_state(system.serial_state, system.shell_state, restarted_log_state, system.echo_state, system.kv_state), restart)
}

func execute_phase152_controlled_variation_workflow(serial_state: serial_service.SerialServiceState, shell_state: shell_service.ShellServiceState, log_state: log_service.LogServiceState, echo_state: echo_service.EchoServiceState, kv_state: kv_service.KvServiceState, shell_endpoint_id: u32, init_pid: u32, log_service_directory_key: u32, shared_control_endpoint_id: u32, log_append_payload: [4]u8, log_tail_payload: [4]u8, kv_set_payload: [4]u8, kv_get_payload: [4]u8) Phase152ControlledVariationWorkflowResult {
    system: Phase150RebuiltSystemState = phase150_rebuilt_system_state(serial_state, shell_state, log_state, echo_state, kv_state)
    empty_route: shell_service.ShellRoutingObservation = shell_service.observe_routing(system.shell_state)
    empty_restart: init.ServiceRestartObservation = init.observe_service_restart(0, 0, 0, 0, 0, 0, 0)

    pre_restart_log_append: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(system, shell_endpoint_id, 3, log_append_payload)
    if pre_restart_log_append.succeeded == 0 {
        return phase152_controlled_variation_workflow_result(pre_restart_log_append.system, pre_restart_log_append.routing, empty_route, empty_route, empty_route, empty_route, empty_route, log_service.observe_retention(pre_restart_log_append.system.log_state), log_service.observe_retention(pre_restart_log_append.system.log_state), kv_service.observe_retention(pre_restart_log_append.system.kv_state), empty_restart, 0)
    }

    pre_restart_log_tail: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(pre_restart_log_append.system, shell_endpoint_id, 2, log_tail_payload)
    if pre_restart_log_tail.succeeded == 0 {
        return phase152_controlled_variation_workflow_result(pre_restart_log_tail.system, pre_restart_log_append.routing, pre_restart_log_tail.routing, empty_route, empty_route, empty_route, empty_route, log_service.observe_retention(pre_restart_log_tail.system.log_state), log_service.observe_retention(pre_restart_log_tail.system.log_state), kv_service.observe_retention(pre_restart_log_tail.system.kv_state), empty_restart, 0)
    }

    pre_restart_kv_set: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(pre_restart_log_tail.system, shell_endpoint_id, 4, kv_set_payload)
    if pre_restart_kv_set.succeeded == 0 {
        return phase152_controlled_variation_workflow_result(pre_restart_kv_set.system, pre_restart_log_append.routing, pre_restart_log_tail.routing, pre_restart_kv_set.routing, empty_route, empty_route, empty_route, log_service.observe_retention(pre_restart_kv_set.system.log_state), log_service.observe_retention(pre_restart_kv_set.system.log_state), kv_service.observe_retention(pre_restart_kv_set.system.kv_state), empty_restart, 0)
    }

    pre_restart_kv_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(pre_restart_kv_set.system, shell_endpoint_id, 4, kv_get_payload)
    if pre_restart_kv_get.succeeded == 0 {
        return phase152_controlled_variation_workflow_result(pre_restart_kv_get.system, pre_restart_log_append.routing, pre_restart_log_tail.routing, pre_restart_kv_set.routing, pre_restart_kv_get.routing, empty_route, empty_route, log_service.observe_retention(pre_restart_kv_get.system.log_state), log_service.observe_retention(pre_restart_kv_get.system.log_state), kv_service.observe_retention(pre_restart_kv_get.system.kv_state), empty_restart, 0)
    }

    pre_restart_log_retention: log_service.LogRetentionObservation = log_service.observe_retention(pre_restart_kv_get.system.log_state)
    restarted: Phase150RebuiltSystemRestartResult = phase152_restart_log_service(pre_restart_kv_get.system, init_pid, log_service_directory_key, shared_control_endpoint_id)
    post_restart_log_tail: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(restarted.system, shell_endpoint_id, 2, log_tail_payload)
    if post_restart_log_tail.succeeded == 0 {
        return phase152_controlled_variation_workflow_result(post_restart_log_tail.system, pre_restart_log_append.routing, pre_restart_log_tail.routing, pre_restart_kv_set.routing, pre_restart_kv_get.routing, post_restart_log_tail.routing, empty_route, pre_restart_log_retention, log_service.observe_retention(post_restart_log_tail.system.log_state), kv_service.observe_retention(post_restart_log_tail.system.kv_state), restarted.restart, 0)
    }

    post_restart_kv_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(post_restart_log_tail.system, shell_endpoint_id, 4, kv_get_payload)
    if post_restart_kv_get.succeeded == 0 {
        return phase152_controlled_variation_workflow_result(post_restart_kv_get.system, pre_restart_log_append.routing, pre_restart_log_tail.routing, pre_restart_kv_set.routing, pre_restart_kv_get.routing, post_restart_log_tail.routing, post_restart_kv_get.routing, pre_restart_log_retention, log_service.observe_retention(post_restart_kv_get.system.log_state), kv_service.observe_retention(post_restart_kv_get.system.kv_state), restarted.restart, 0)
    }

    return phase152_controlled_variation_workflow_result(post_restart_kv_get.system, pre_restart_log_append.routing, pre_restart_log_tail.routing, pre_restart_kv_set.routing, pre_restart_kv_get.routing, post_restart_log_tail.routing, post_restart_kv_get.routing, pre_restart_log_retention, log_service.observe_retention(post_restart_kv_get.system.log_state), kv_service.observe_retention(post_restart_kv_get.system.kv_state), restarted.restart, 1)
}