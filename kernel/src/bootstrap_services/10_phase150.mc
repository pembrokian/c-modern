import init
import kv_service
import log_service

func execute_phase150_rebuilt_system_command(system: Phase150RebuiltSystemState, shell_endpoint_id: u32, request_len: usize, request_payload: [4]u8) Phase150RebuiltSystemCommandResult {
    routed: Phase142ShellCommandRouteResult = execute_phase145_service_restart_shell_command(system.serial_state, system.shell_state, system.log_state, system.echo_state, system.kv_state, shell_endpoint_id, request_len, request_payload)
    return phase150_rebuilt_system_command_result(phase150_rebuilt_system_state(routed.serial_state, routed.shell_state, routed.log_state, routed.echo_state, routed.kv_state), routed.routing, routed.succeeded)
}

func phase150_mark_kv_unavailable(system: Phase150RebuiltSystemState) Phase150RebuiltSystemState {
    return phase150_rebuilt_system_state(system.serial_state, system.shell_state, system.log_state, system.echo_state, kv_service.mark_unavailable(system.kv_state))
}

func phase150_restart_kv_service(system: Phase150RebuiltSystemState, init_pid: u32, kv_service_directory_key: u32, shared_control_endpoint_id: u32) Phase150RebuiltSystemRestartResult {
    restart: init.ServiceRestartObservation = init.observe_service_restart(init_pid, kv_service_directory_key, system.kv_state.owner_pid, system.kv_state.owner_pid, shared_control_endpoint_id, 1, 1)
    restarted_kv_state: kv_service.KvServiceState = kv_service.restart_service(system.kv_state)
    restarted_log_state: log_service.LogServiceState = append_phase145_restart_observation(system.log_state, init_pid, system.shell_state.log_endpoint_id)
    return phase150_rebuilt_system_restart_result(phase150_rebuilt_system_state(system.serial_state, system.shell_state, restarted_log_state, system.echo_state, restarted_kv_state), restart)
}

func execute_phase150_rebuilt_system_workflow(serial_state: serial_service.SerialServiceState, shell_state: shell_service.ShellServiceState, log_state: log_service.LogServiceState, echo_state: echo_service.EchoServiceState, kv_state: kv_service.KvServiceState, shell_endpoint_id: u32, init_pid: u32, kv_service_directory_key: u32, shared_control_endpoint_id: u32, log_append_payload: [4]u8, log_tail_payload: [4]u8, pre_failure_set_payload: [4]u8, pre_failure_get_payload: [4]u8) Phase150RebuiltSystemWorkflowResult {
    system: Phase150RebuiltSystemState = phase150_rebuilt_system_state(serial_state, shell_state, log_state, echo_state, kv_state)
    empty_route: shell_service.ShellRoutingObservation = shell_service.observe_routing(system.shell_state)

    log_append: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(system, shell_endpoint_id, 3, log_append_payload)
    if log_append.succeeded == 0 {
        return phase150_rebuilt_system_workflow_result(log_append.system, log_append.routing, empty_route, empty_route, empty_route, empty_route, empty_route, kv_service.observe_retention(log_append.system.kv_state), kv_service.observe_retention(log_append.system.kv_state), init.observe_service_restart(0, 0, 0, 0, 0, 0, 0), log_service.observe_retention(log_append.system.log_state), 0)
    }

    log_tail: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(log_append.system, shell_endpoint_id, 2, log_tail_payload)
    if log_tail.succeeded == 0 {
        return phase150_rebuilt_system_workflow_result(log_tail.system, log_append.routing, log_tail.routing, empty_route, empty_route, empty_route, empty_route, kv_service.observe_retention(log_tail.system.kv_state), kv_service.observe_retention(log_tail.system.kv_state), init.observe_service_restart(0, 0, 0, 0, 0, 0, 0), log_service.observe_retention(log_tail.system.log_state), 0)
    }

    pre_failure_set: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(log_tail.system, shell_endpoint_id, 4, pre_failure_set_payload)
    if pre_failure_set.succeeded == 0 {
        return phase150_rebuilt_system_workflow_result(pre_failure_set.system, log_append.routing, log_tail.routing, pre_failure_set.routing, empty_route, empty_route, empty_route, kv_service.observe_retention(pre_failure_set.system.kv_state), kv_service.observe_retention(pre_failure_set.system.kv_state), init.observe_service_restart(0, 0, 0, 0, 0, 0, 0), log_service.observe_retention(pre_failure_set.system.log_state), 0)
    }

    pre_failure_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(pre_failure_set.system, shell_endpoint_id, 4, pre_failure_get_payload)
    if pre_failure_get.succeeded == 0 {
        return phase150_rebuilt_system_workflow_result(pre_failure_get.system, log_append.routing, log_tail.routing, pre_failure_set.routing, pre_failure_get.routing, empty_route, empty_route, kv_service.observe_retention(pre_failure_get.system.kv_state), kv_service.observe_retention(pre_failure_get.system.kv_state), init.observe_service_restart(0, 0, 0, 0, 0, 0, 0), log_service.observe_retention(pre_failure_get.system.log_state), 0)
    }

    pre_failure_retention: kv_service.KvRetentionObservation = kv_service.observe_retention(pre_failure_get.system.kv_state)
    unavailable_system: Phase150RebuiltSystemState = phase150_mark_kv_unavailable(pre_failure_get.system)
    failed_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(unavailable_system, shell_endpoint_id, 4, pre_failure_get_payload)
    if failed_get.succeeded == 0 {
        return phase150_rebuilt_system_workflow_result(failed_get.system, log_append.routing, log_tail.routing, pre_failure_set.routing, pre_failure_get.routing, failed_get.routing, empty_route, pre_failure_retention, kv_service.observe_retention(failed_get.system.kv_state), init.observe_service_restart(0, 0, 0, 0, 0, 0, 0), log_service.observe_retention(failed_get.system.log_state), 0)
    }

    restarted: Phase150RebuiltSystemRestartResult = phase150_restart_kv_service(failed_get.system, init_pid, kv_service_directory_key, shared_control_endpoint_id)
    restarted_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(restarted.system, shell_endpoint_id, 4, pre_failure_get_payload)
    if restarted_get.succeeded == 0 {
        return phase150_rebuilt_system_workflow_result(restarted_get.system, log_append.routing, log_tail.routing, pre_failure_set.routing, pre_failure_get.routing, failed_get.routing, restarted_get.routing, pre_failure_retention, kv_service.observe_retention(restarted_get.system.kv_state), restarted.restart, log_service.observe_retention(restarted_get.system.log_state), 0)
    }

    return phase150_rebuilt_system_workflow_result(restarted_get.system, log_append.routing, log_tail.routing, pre_failure_set.routing, pre_failure_get.routing, failed_get.routing, restarted_get.routing, pre_failure_retention, kv_service.observe_retention(restarted_get.system.kv_state), restarted.restart, log_service.observe_retention(restarted_get.system.log_state), 1)
}