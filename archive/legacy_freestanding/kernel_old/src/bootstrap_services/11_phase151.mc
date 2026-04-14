import address_space
import bootstrap_state
import bootstrap_audit
import debug
import echo_service
import init
import ipc
import kv_service
import log_service
import mmu
import serial_service
import shell_service
import state
import syscall

const SHELL_COMMAND_ECHO_BYTE0: u8 = 69
const SHELL_COMMAND_ECHO_BYTE1: u8 = 67
const SHELL_COMMAND_LOG_BYTE0: u8 = 76
const SHELL_COMMAND_LOG_APPEND_BYTE1: u8 = 65
const SHELL_COMMAND_LOG_TAIL_BYTE1: u8 = 84
const SHELL_COMMAND_KV_BYTE0: u8 = 75
const SHELL_COMMAND_KV_SET_BYTE1: u8 = 83
const SHELL_COMMAND_KV_GET_BYTE1: u8 = 71

struct LateServiceSystemRuntimeContext {
    identity: state.IdentityConfig
    phase141_shell_pid: u32
    phase141_kv_pid: u32
    init_endpoint_id: u32
    serial_service_endpoint_id: u32
    composition_echo_endpoint_id: u32
    composition_log_endpoint_id: u32
    kv_service_endpoint_id: u32
    shell_service_endpoint_id: u32
    log_service_directory_key: u32
    kv_service_directory_key: u32
    init_root_page_table: usize
    child_root_page_table: usize
    init_image: init.InitImage
    serial_receive_observation: syscall.ReceiveObservation
    services: bootstrap_state.ServiceState
}

struct LateServiceSystemProofInputs {
    phase137_audit: debug.Phase137OptionalDmaOrEquivalentAudit
    contracts: state.ContractState
    runtime: LateServiceSystemRuntimeContext
}

func build_late_service_system_proof_inputs(phase137_audit: debug.Phase137OptionalDmaOrEquivalentAudit, contracts: state.ContractState, runtime_context: LateServiceSystemRuntimeContext) LateServiceSystemProofInputs {
    return LateServiceSystemProofInputs{ phase137_audit: phase137_audit, contracts: contracts, runtime: runtime_context }
}

func shell_command_payload(byte0: u8, byte1: u8, byte2: u8, byte3: u8) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = byte0
    payload[1] = byte1
    payload[2] = byte2
    payload[3] = byte3
    return payload
}

func execute_phase142_rebuilt_system_command(system: Phase150RebuiltSystemState, shell_endpoint_id: u32, request_len: usize, request_payload: [4]u8) Phase150RebuiltSystemCommandResult {
    routed: Phase142ShellCommandRouteResult = execute_phase142_shell_command(system.serial_state, system.shell_state, system.log_state, system.echo_state, system.kv_state, shell_endpoint_id, request_len, request_payload)
    return phase150_rebuilt_system_command_result(phase150_rebuilt_system_state(routed.serial_state, routed.shell_state, routed.log_state, routed.echo_state, routed.kv_state), routed.routing, routed.succeeded)
}

func execute_phase144_rebuilt_system_command(system: Phase150RebuiltSystemState, shell_endpoint_id: u32, request_len: usize, request_payload: [4]u8) Phase150RebuiltSystemCommandResult {
    routed: Phase142ShellCommandRouteResult = execute_phase144_stateful_key_value_shell_command(system.serial_state, system.shell_state, system.log_state, system.echo_state, system.kv_state, shell_endpoint_id, request_len, request_payload)
    return phase150_rebuilt_system_command_result(phase150_rebuilt_system_state(routed.serial_state, routed.shell_state, routed.log_state, routed.echo_state, routed.kv_state), routed.routing, routed.succeeded)
}

func build_late_rebuilt_system_state(inputs: LateServiceSystemProofInputs, serial_endpoint_handle_slot: u32, log_state: log_service.LogServiceState) Phase150RebuiltSystemState {
    return phase150_rebuilt_system_state(serial_service.record_ingress(serial_service.service_state(inputs.runtime.identity.init.pid, serial_endpoint_handle_slot), inputs.runtime.serial_receive_observation), shell_service.service_state(inputs.runtime.phase141_shell_pid, 1, inputs.runtime.composition_echo_endpoint_id, inputs.runtime.composition_log_endpoint_id, inputs.runtime.kv_service_endpoint_id), log_state, inputs.runtime.services.echo_service_state, kv_service.service_state(inputs.runtime.phase141_kv_pid, 1))
}

func build_late_rebuilt_system_state_with_retained_log(inputs: LateServiceSystemProofInputs, serial_endpoint_handle_slot: u32) Phase150RebuiltSystemState {
    return build_late_rebuilt_system_state(inputs, serial_endpoint_handle_slot, inputs.runtime.services.log_service_state)
}

func build_late_rebuilt_system_state_with_fresh_log(inputs: LateServiceSystemProofInputs, serial_endpoint_handle_slot: u32) Phase150RebuiltSystemState {
    return build_late_rebuilt_system_state(inputs, serial_endpoint_handle_slot, log_service.service_state(inputs.runtime.services.log_service_state.owner_pid, inputs.runtime.services.log_service_state.endpoint_handle_slot))
}

func validate_phase152c_serial_behavior(workflow: Phase152ControlledVariationWorkflowResult, shell_endpoint_id: u32) bool {
    if workflow.succeeded == 0 {
        return false
    }
    if workflow.varied_system.serial_state.forward_endpoint_id != shell_endpoint_id {
        return false
    }
    if workflow.post_restart_log_tail_route.endpoint_id != shell_endpoint_id {
        return false
    }
    if workflow.post_restart_kv_get_route.endpoint_id != shell_endpoint_id {
        return false
    }
    if workflow.varied_system.serial_state.forwarded_request_count != 6 {
        return false
    }
    if workflow.varied_system.serial_state.aggregate_reply_count != 6 {
        return false
    }
    if workflow.varied_system.serial_state.aggregate_reply_byte0 != workflow.post_restart_kv_get_route.reply_byte0 || workflow.varied_system.serial_state.aggregate_reply_byte1 != workflow.post_restart_kv_get_route.reply_byte1 {
        return false
    }
    if workflow.post_restart_log_tail_route.reply_byte0 != 0 || workflow.post_restart_log_tail_route.reply_byte1 != 33 {
        return false
    }
    if workflow.post_restart_kv_get_route.reply_byte0 != 86 || workflow.post_restart_kv_get_route.reply_byte1 != 75 {
        return false
    }
    return true
}

func execute_late_service_system_proofs(inputs: LateServiceSystemProofInputs) i32 {
    contracts: state.ContractState = inputs.contracts
    runtime: LateServiceSystemRuntimeContext = inputs.runtime
    identity: state.IdentityConfig = runtime.identity
    services: bootstrap_state.ServiceState = runtime.services
    phase140_serial_config: SerialServiceConfig = serial_service_composition_config(inputs.runtime.identity.boot.pid, inputs.runtime.identity.init.pid, inputs.runtime.identity.init.tid, inputs.runtime.identity.init.asid, inputs.runtime.serial_service_endpoint_id, inputs.runtime.init_endpoint_id, mmu.bootstrap_translation_root(inputs.runtime.identity.init.asid, inputs.runtime.init_root_page_table))
    phase140_serial_state: serial_service.SerialServiceState = serial_service.record_ingress(serial_service.service_state(inputs.runtime.identity.init.pid, phase140_serial_config.endpoint_handle_slot), inputs.runtime.serial_receive_observation)
    phase140_serial_execution: SerialServiceExecutionState = SerialServiceExecutionState{ program_capability: capability.empty_slot(), gate: syscall.open_gate(syscall.gate_closed()), process_slots: state.zero_process_slots(), task_slots: state.zero_task_slots(), init_handle_table: capability.empty_handle_table(), child_handle_table: capability.handle_table_for_owner(inputs.runtime.identity.init.pid), wait_table: capability.empty_wait_table(), endpoints: ipc.empty_table(), init_image: inputs.runtime.init_image, child_address_space: address_space.empty_space(), child_user_frame: address_space.empty_frame(), service_state: phase140_serial_state, spawn_observation: syscall.empty_spawn_observation(), receive_observation: inputs.runtime.serial_receive_observation, failure_observation: empty_serial_service_failure_observation(), wait_observation: syscall.empty_wait_observation(), ready_queue: state.empty_queue() }
    phase140_composition_config: CompositionServiceConfig = composition_service_config(inputs.runtime.identity.init.pid, inputs.runtime.identity.child.pid, inputs.runtime.identity.child.tid, inputs.runtime.identity.child.asid, inputs.runtime.init_endpoint_id, inputs.runtime.composition_echo_endpoint_id, inputs.runtime.composition_log_endpoint_id, phase140_serial_config.composition_handle_slot, mmu.bootstrap_translation_root(inputs.runtime.identity.child.asid, inputs.runtime.child_root_page_table))
    phase140_composition_execution: CompositionServiceExecutionState = empty_composition_service_execution_state()
    phase140_composition_execution = CompositionServiceExecutionState{ program_capability: phase140_composition_execution.program_capability, gate: phase140_composition_execution.gate, process_slots: phase140_composition_execution.process_slots, task_slots: phase140_composition_execution.task_slots, init_handle_table: phase140_composition_execution.init_handle_table, child_handle_table: capability.handle_table_for_owner(identity.child.pid), wait_table: phase140_composition_execution.wait_table, endpoints: phase140_composition_execution.endpoints, init_image: runtime.init_image, child_address_space: phase140_composition_execution.child_address_space, child_user_frame: phase140_composition_execution.child_user_frame, echo_peer_state: phase140_composition_execution.echo_peer_state, log_peer_state: phase140_composition_execution.log_peer_state, spawn_observation: phase140_composition_execution.spawn_observation, request_receive_observation: phase140_composition_execution.request_receive_observation, echo_fanout_observation: phase140_composition_execution.echo_fanout_observation, echo_peer_receive_observation: phase140_composition_execution.echo_peer_receive_observation, echo_reply_send_observation: phase140_composition_execution.echo_reply_send_observation, echo_reply_observation: phase140_composition_execution.echo_reply_observation, log_fanout_observation: phase140_composition_execution.log_fanout_observation, log_peer_receive_observation: phase140_composition_execution.log_peer_receive_observation, log_ack_send_observation: phase140_composition_execution.log_ack_send_observation, log_ack_observation: phase140_composition_execution.log_ack_observation, aggregate_reply_send_observation: phase140_composition_execution.aggregate_reply_send_observation, aggregate_reply_observation: phase140_composition_execution.aggregate_reply_observation, wait_observation: phase140_composition_execution.wait_observation, observation: phase140_composition_execution.observation, ready_queue: phase140_composition_execution.ready_queue }
    phase140_result: Phase140SerialIngressCompositionResult = execute_phase140_serial_ingress_composed_service_graph(phase140_serial_config, phase140_composition_config, phase140_serial_execution, phase140_composition_execution)
    if phase140_result.succeeded == 0 {
        return 95
    }
    phase140_audit: debug.Phase140SerialIngressComposedServiceGraphAudit = bootstrap_audit.build_phase140_serial_ingress_composed_service_graph_audit(bootstrap_audit.Phase140SerialIngressComposedServiceGraphAuditInputs{ phase137: inputs.phase137_audit, serial_service_pid: phase140_result.serial_state.service_state.owner_pid, composition_service_pid: phase140_result.composition_state.observation.service_pid, serial_forward_endpoint_id: phase140_result.serial_state.service_state.forward_endpoint_id, serial_forward_status: phase140_result.serial_state.service_state.forward_status, serial_forward_request_len: phase140_result.serial_state.service_state.forwarded_request_len, serial_forward_request_byte0: phase140_result.serial_state.service_state.forwarded_request_byte0, serial_forward_request_byte1: phase140_result.serial_state.service_state.forwarded_request_byte1, serial_forward_count: phase140_result.serial_state.service_state.forwarded_request_count, composition_request_receive_status: phase140_result.composition_state.request_receive_observation.status, composition_request_source_pid: phase140_result.composition_state.request_receive_observation.source_pid, composition_request_len: phase140_result.composition_state.observation.request_len, composition_request_byte0: phase140_result.composition_state.observation.request_byte0, composition_request_byte1: phase140_result.composition_state.observation.request_byte1, composition_control_endpoint_id: phase140_composition_config.control_endpoint_id, composition_echo_endpoint_id: phase140_composition_config.echo_endpoint_id, composition_log_endpoint_id: phase140_composition_config.log_endpoint_id, composition_echo_fanout_status: phase140_result.composition_state.echo_fanout_observation.status, composition_echo_fanout_endpoint_id: phase140_result.composition_state.echo_fanout_observation.endpoint_id, composition_log_fanout_status: phase140_result.composition_state.log_fanout_observation.status, composition_log_fanout_endpoint_id: phase140_result.composition_state.log_fanout_observation.endpoint_id, composition_aggregate_reply_status: phase140_result.composition_state.aggregate_reply_observation.status, composition_outbound_edge_count: phase140_result.composition_state.observation.outbound_edge_count, composition_aggregate_reply_count: phase140_result.composition_state.observation.aggregate_reply_count, serial_aggregate_reply_status: phase140_result.serial_state.service_state.aggregate_reply_status, serial_aggregate_reply_len: phase140_result.serial_state.service_state.aggregate_reply_len, serial_aggregate_reply_byte0: phase140_result.serial_state.service_state.aggregate_reply_byte0, serial_aggregate_reply_byte1: phase140_result.serial_state.service_state.aggregate_reply_byte1, serial_aggregate_reply_byte2: phase140_result.serial_state.service_state.aggregate_reply_byte2, serial_aggregate_reply_byte3: phase140_result.serial_state.service_state.aggregate_reply_byte3, serial_aggregate_reply_count: phase140_result.serial_state.service_state.aggregate_reply_count, kernel_broker_visible: 0, dynamic_routing_visible: 0, general_service_graph_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase140_serial_ingress_composed_service_graph(phase140_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 96
    }

    phase141_request_len: usize = 4
    phase141_request_payload: [4]u8 = shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 33, 33)
    phase141_serial_state: serial_service.SerialServiceState = serial_service.record_ingress(serial_service.service_state(identity.init.pid, phase140_serial_config.endpoint_handle_slot), runtime.serial_receive_observation)
    phase141_serial_state = serial_service.record_forward(phase141_serial_state, runtime.shell_service_endpoint_id, syscall.SyscallStatus.Ok, phase141_request_len, phase141_request_payload)
    phase141_shell_state: shell_service.ShellServiceState = shell_service.record_request(shell_service.service_state(runtime.phase141_shell_pid, 1, runtime.composition_echo_endpoint_id, runtime.composition_log_endpoint_id, runtime.kv_service_endpoint_id), phase141_serial_state.owner_pid, runtime.shell_service_endpoint_id, phase141_request_len, phase141_request_payload)
    phase141_log_state: log_service.LogServiceState = services.log_service_state
    phase141_echo_state: echo_service.EchoServiceState = services.echo_service_state
    phase141_kv_state: kv_service.KvServiceState = kv_service.record_set(kv_service.service_state(runtime.phase141_kv_pid, 1), runtime.phase141_shell_pid, runtime.kv_service_endpoint_id, 75, 86)
    phase141_reply_payload: [4]u8 = shell_service.reply_payload(phase141_shell_state, phase141_echo_state, phase141_log_state, phase141_kv_state)
    phase141_shell_state = shell_service.record_reply(phase141_shell_state, syscall.SyscallStatus.Ok, 2, phase141_reply_payload)
    phase141_audit: debug.Phase141InteractiveServiceSystemScopeFreezeAudit = bootstrap_audit.build_phase141_interactive_service_system_scope_freeze_audit(bootstrap_audit.Phase141InteractiveServiceSystemScopeFreezeAuditInputs{ phase140: phase140_audit, serial_service_pid: phase141_serial_state.owner_pid, shell_service_pid: phase141_shell_state.owner_pid, kv_service_pid: phase141_kv_state.owner_pid, serial_forward_endpoint_id: phase141_serial_state.forward_endpoint_id, serial_forward_status: phase141_serial_state.forward_status, serial_forward_request_len: phase141_serial_state.forwarded_request_len, serial_forward_request_byte0: phase141_serial_state.forwarded_request_byte0, serial_forward_request_byte1: phase141_serial_state.forwarded_request_byte1, shell_tag: phase141_shell_state.last_tag, shell_request_len: phase141_shell_state.last_request_len, shell_request_byte0: phase141_shell_state.last_byte0, shell_request_byte1: phase141_shell_state.last_byte1, shell_request_byte2: phase141_shell_state.last_byte2, shell_endpoint_id: runtime.shell_service_endpoint_id, echo_endpoint_id: runtime.composition_echo_endpoint_id, log_endpoint_id: runtime.composition_log_endpoint_id, kv_endpoint_id: runtime.kv_service_endpoint_id, echo_route_count: phase141_shell_state.echo_route_count, log_route_count: phase141_shell_state.log_route_count, kv_route_count: phase141_shell_state.kv_route_count, invalid_command_count: phase141_shell_state.invalid_command_count, shell_reply_status: phase141_shell_state.reply_status, shell_reply_len: phase141_shell_state.reply_len, shell_reply_byte0: phase141_shell_state.reply_byte0, shell_reply_byte1: phase141_shell_state.reply_byte1, shell_reply_byte2: phase141_shell_state.reply_byte2, shell_reply_byte3: phase141_shell_state.reply_byte3, fixed_vocabulary_visible: 1, kernel_broker_visible: 0, dynamic_routing_visible: 0, general_shell_framework_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase141_interactive_service_system_scope_freeze(phase141_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 97
    }

    phase142_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_retained_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase142_echo_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_ECHO_BYTE0, SHELL_COMMAND_ECHO_BYTE1, 72, 73))
    phase142_log_append_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_echo_result.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 80, 33))
    phase142_log_tail_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_log_append_result.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 33, 33))
    phase142_kv_set_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_log_tail_result.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 86))
    phase142_kv_get_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_kv_set_result.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase142_invalid_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_kv_get_result.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(88, 88, 63, 63))
    phase142_audit: debug.Phase142SerialShellCommandRoutingAudit = bootstrap_audit.build_phase142_serial_shell_command_routing_audit(bootstrap_audit.Phase142SerialShellCommandRoutingAuditInputs{ phase141: phase141_audit, serial_service_pid: phase142_invalid_result.system.serial_state.owner_pid, shell_service_pid: phase142_invalid_result.system.shell_state.owner_pid, kv_service_pid: phase142_invalid_result.system.kv_state.owner_pid, serial_forward_endpoint_id: phase142_invalid_result.system.serial_state.forward_endpoint_id, serial_forward_status: phase142_invalid_result.system.serial_state.forward_status, serial_forward_count: phase142_invalid_result.system.serial_state.forwarded_request_count, serial_final_reply_status: phase142_invalid_result.system.serial_state.aggregate_reply_status, serial_final_reply_len: phase142_invalid_result.system.serial_state.aggregate_reply_len, serial_final_reply_byte0: phase142_invalid_result.system.serial_state.aggregate_reply_byte0, serial_final_reply_count: phase142_invalid_result.system.serial_state.aggregate_reply_count, echo_route: phase142_echo_result.routing, log_append_route: phase142_log_append_result.routing, log_tail_route: phase142_log_tail_result.routing, kv_set_route: phase142_kv_set_result.routing, kv_get_route: phase142_kv_get_result.routing, invalid_route: phase142_invalid_result.routing, compact_command_encoding_visible: 1, malformed_command_visible: 1, general_shell_framework_visible: 0, payload_width_reopened_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase142_serial_shell_command_routing(phase142_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 98
    }

    phase143_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase143_append_a: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 65, 33))
    phase143_append_b: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_append_a.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 66, 33))
    phase143_append_c: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_append_b.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 67, 33))
    phase143_append_d: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_append_c.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 68, 33))
    phase143_overflow_append: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_append_d.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 69, 33))
    phase143_tail_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_overflow_append.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 33, 33))
    phase143_audit: debug.Phase143LongLivedLogServiceAudit = bootstrap_audit.build_phase143_long_lived_log_service_audit(bootstrap_audit.Phase143LongLivedLogServiceAuditInputs{ phase142: phase142_audit, log_service_pid: phase143_tail_result.system.log_state.owner_pid, shell_service_pid: phase143_tail_result.system.shell_state.owner_pid, overflow_append_route: phase143_overflow_append.routing, tail_route: phase143_tail_result.routing, retention: log_service.observe_retention(phase143_tail_result.system.log_state), bounded_retention_visible: 1, explicit_overwrite_policy_visible: 1, durable_persistence_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase143_long_lived_log_service(phase143_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 99
    }

    phase144_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase144_missing_get: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase144_initial_set: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_missing_get.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 86))
    phase144_hit_get: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_initial_set.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase144_overwrite_set: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_hit_get.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 87))
    phase144_overwrite_get: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_overwrite_set.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase144_audit: debug.Phase144StatefulKeyValueServiceFollowThroughAudit = bootstrap_audit.build_phase144_stateful_key_value_service_follow_through_audit(bootstrap_audit.Phase144StatefulKeyValueServiceFollowThroughAuditInputs{ phase143: phase143_audit, kv_service_pid: phase144_overwrite_get.system.kv_state.owner_pid, shell_service_pid: phase144_overwrite_get.system.shell_state.owner_pid, log_service_pid: phase144_overwrite_set.system.log_state.owner_pid, missing_get_route: phase144_missing_get.routing, initial_set_route: phase144_initial_set.routing, hit_get_route: phase144_hit_get.routing, overwrite_set_route: phase144_overwrite_set.routing, overwrite_get_route: phase144_overwrite_get.routing, kv_retention: kv_service.observe_retention(phase144_overwrite_get.system.kv_state), write_log_retention: log_service.observe_retention(phase144_overwrite_set.system.log_state), bounded_retention_visible: 1, missing_key_visible: 1, explicit_overwrite_visible: 1, fixed_write_log_visible: 1, durable_persistence_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase144_stateful_key_value_service_follow_through(phase144_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 100
    }

    phase145_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase145_pre_failure_set: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase145_system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 86))
    phase145_pre_failure_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase145_pre_failure_set.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))

    phase145_pre_failure_retention: kv_service.KvRetentionObservation = kv_service.observe_retention(phase145_pre_failure_get.system.kv_state)
    phase145_failed_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase150_mark_kv_unavailable(phase145_pre_failure_get.system), runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))

    phase145_restart: Phase150RebuiltSystemRestartResult = phase150_restart_kv_service(phase145_failed_get.system, identity.init.pid, runtime.kv_service_directory_key, runtime.init_endpoint_id)

    phase145_restarted_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase145_restart.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase145_audit: debug.Phase145ServiceRestartFailureAndUsagePressureAudit = bootstrap_audit.build_phase145_service_restart_failure_and_usage_pressure_audit(bootstrap_audit.Phase145ServiceRestartFailureAndUsagePressureAuditInputs{ phase144: phase144_audit, kv_service_pid: phase145_restarted_get.system.kv_state.owner_pid, shell_service_pid: phase145_restarted_get.system.shell_state.owner_pid, log_service_pid: phase145_restarted_get.system.log_state.owner_pid, init_policy_owner_pid: identity.init.pid, pre_failure_set_route: phase145_pre_failure_set.routing, pre_failure_get_route: phase145_pre_failure_get.routing, failed_get_route: phase145_failed_get.routing, restarted_get_route: phase145_restarted_get.routing, pre_failure_retention: phase145_pre_failure_retention, post_restart_retention: kv_service.observe_retention(phase145_restarted_get.system.kv_state), restart: phase145_restart.restart, event_log_retention: log_service.observe_retention(phase145_restarted_get.system.log_state), shell_failure_visible: 1, init_restart_visible: 1, post_restart_state_reset_visible: 1, manual_retry_visible: 1, kernel_supervision_visible: 0, durable_persistence_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase145_service_restart_failure_and_usage_pressure(phase145_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 101
    }

    phase146_audit: debug.Phase146ServiceShapeConsolidationAudit = bootstrap_audit.build_phase146_service_shape_consolidation_audit(bootstrap_audit.Phase146ServiceShapeConsolidationAuditInputs{ phase145: phase145_audit, serial_service_pid: phase145_restarted_get.system.serial_state.owner_pid, shell_service_pid: phase145_restarted_get.system.shell_state.owner_pid, log_service_pid: phase145_restarted_get.system.log_state.owner_pid, kv_service_pid: phase145_restarted_get.system.kv_state.owner_pid, init_policy_owner_pid: identity.init.pid, serial_forward: serial_service.observe_composition(phase145_restarted_get.system.serial_state), shell_route: phase145_restarted_get.routing, log_retention: log_service.observe_retention(phase145_restarted_get.system.log_state), kv_retention: kv_service.observe_retention(phase145_restarted_get.system.kv_state), restart: phase145_restart.restart, serial_endpoint_handle_slot: phase145_restarted_get.system.serial_state.endpoint_handle_slot, shell_endpoint_handle_slot: phase145_restarted_get.system.shell_state.endpoint_handle_slot, log_endpoint_handle_slot: phase145_restarted_get.system.log_state.endpoint_handle_slot, kv_endpoint_handle_slot: phase145_restarted_get.system.kv_state.endpoint_handle_slot, fixed_boot_wiring_visible: 1, service_local_request_reply_visible: 1, shell_syntax_boundary_visible: 1, service_semantic_boundary_visible: 1, explicit_failure_exposure_visible: 1, init_owned_restart_visible: 1, intentional_shell_difference_visible: 1, intentional_stateful_difference_visible: 1, dynamic_service_framework_visible: 0, dynamic_registration_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase146_service_shape_consolidation(phase146_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 102
    }

    phase147_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase147_log_append: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_system, runtime.shell_service_endpoint_id, 3, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 76, 0))
    phase147_log_tail: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_log_append.system, runtime.shell_service_endpoint_id, 2, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 0, 0))
    phase147_kv_set: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_log_tail.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 81, 90))
    phase147_kv_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_kv_set.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 81, 0))
    phase147_repeated_log_tail: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_kv_get.system, runtime.shell_service_endpoint_id, 2, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 0, 0))
    phase147_repeated_kv_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_repeated_log_tail.system, runtime.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 81, 0))
    phase147_audit: debug.Phase147IpcShapeAudit = bootstrap_audit.build_phase147_ipc_shape_audit(bootstrap_audit.Phase147IpcShapeAuditInputs{ phase146: phase146_audit, serial_service_pid: phase147_repeated_kv_get.system.serial_state.owner_pid, shell_service_pid: phase147_repeated_kv_get.system.shell_state.owner_pid, log_service_pid: phase147_repeated_kv_get.system.log_state.owner_pid, kv_service_pid: phase147_repeated_kv_get.system.kv_state.owner_pid, serial_observation: serial_service.observe_composition(phase147_repeated_kv_get.system.serial_state), log_append_route: phase147_log_append.routing, log_tail_route: phase147_log_tail.routing, kv_set_route: phase147_kv_set.routing, kv_get_route: phase147_kv_get.routing, repeated_log_tail_route: phase147_repeated_log_tail.routing, repeated_kv_get_route: phase147_repeated_kv_get.routing, log_retention: log_service.observe_retention(phase147_repeated_kv_get.system.log_state), kv_retention: kv_service.observe_retention(phase147_repeated_kv_get.system.kv_state), request_construction_sufficient_visible: 1, reply_shape_service_local_visible: 1, compact_encoding_sufficient_visible: 1, service_to_service_observation_visible: 1, generic_message_bus_visible: 0, dynamic_payload_typing_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase147_ipc_shape_audit_under_real_usage(phase147_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 103
    }

    phase148_audit: debug.Phase148AuthorityErgonomicsAudit = bootstrap_audit.build_phase148_authority_ergonomics_audit(bootstrap_audit.Phase148AuthorityErgonomicsAuditInputs{ phase147: phase147_audit, serial_endpoint_handle_slot: phase147_repeated_kv_get.system.serial_state.endpoint_handle_slot, shell_endpoint_handle_slot: phase147_repeated_kv_get.system.shell_state.endpoint_handle_slot, log_endpoint_handle_slot: phase147_repeated_kv_get.system.log_state.endpoint_handle_slot, kv_endpoint_handle_slot: phase147_repeated_kv_get.system.kv_state.endpoint_handle_slot, explicit_slot_authority_visible: 1, retained_state_authority_local_visible: 1, restart_handoff_explicit_visible: 1, repeated_authority_ceremony_stable_visible: 1, narrow_capability_helper_visible: 1, overscoped_helper_visible: 0, ambient_authority_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase148_authority_ergonomics_under_reuse(phase148_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 104
    }

    phase149_audit: debug.Phase149RestartSemanticsFirstClassPatternAudit = bootstrap_audit.build_phase149_restart_semantics_first_class_pattern_audit(bootstrap_audit.Phase149RestartSemanticsFirstClassPatternAuditInputs{ phase148: phase148_audit, shell_service_pid: phase145_audit.shell_service_pid, kv_service_pid: phase145_audit.kv_service_pid, log_service_pid: phase145_audit.log_service_pid, init_policy_owner_pid: phase145_audit.init_policy_owner_pid, pre_failure_get_route: phase145_audit.pre_failure_get_route, failed_get_route: phase145_audit.failed_get_route, restarted_get_route: phase145_audit.restarted_get_route, post_restart_retention: phase145_audit.post_restart_retention, restart: phase145_audit.restart, event_log_retention: phase145_audit.event_log_retention, caller_retry_obligation_visible: 1, retry_reissues_same_request_visible: 1, request_identity_not_tracked_visible: 1, post_restart_reset_state_visible: 1, init_owned_restart_policy_visible: 1, transparent_retry_visible: 0, kernel_supervision_visible: 0, durable_recovery_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase149_restart_semantics_first_class_pattern(phase149_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 105
    }

    phase150_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)
    phase150_workflow: Phase150RebuiltSystemWorkflowResult = execute_phase150_rebuilt_system_workflow(phase150_system.serial_state, phase150_system.shell_state, phase150_system.log_state, phase150_system.echo_state, phase150_system.kv_state, runtime.shell_service_endpoint_id, identity.init.pid, runtime.kv_service_directory_key, runtime.init_endpoint_id, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 76, 0), shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 0, 0), shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 86), shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    if phase150_workflow.succeeded == 0 {
        return 106
    }

    phase150_audit: debug.Phase150OneSystemRebuiltCleanlyAudit = bootstrap_audit.build_phase150_one_system_rebuilt_cleanly_audit(bootstrap_audit.Phase150OneSystemRebuiltCleanlyAuditInputs{ phase149: phase149_audit, serial_service_pid: phase150_workflow.rebuilt_system.serial_state.owner_pid, shell_service_pid: phase150_workflow.rebuilt_system.shell_state.owner_pid, log_service_pid: phase150_workflow.rebuilt_system.log_state.owner_pid, kv_service_pid: phase150_workflow.rebuilt_system.kv_state.owner_pid, log_append_route: phase150_workflow.log_append_route, log_tail_route: phase150_workflow.log_tail_route, pre_failure_set_route: phase150_workflow.pre_failure_set_route, pre_failure_get_route: phase150_workflow.pre_failure_get_route, failed_get_route: phase150_workflow.failed_get_route, restarted_get_route: phase150_workflow.restarted_get_route, pre_failure_retention: phase150_workflow.pre_failure_retention, post_restart_retention: phase150_workflow.post_restart_retention, restart: phase150_workflow.restart, event_log_retention: phase150_workflow.event_log_retention, rebuilt_system_helper_visible: 1, owner_local_system_state_visible: 1, manual_state_threading_removed_visible: 1, dynamic_service_framework_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase150_one_system_rebuilt_cleanly(phase150_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 107
    }

    phase152_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)
    phase152_workflow: Phase152ControlledVariationWorkflowResult = execute_phase152_controlled_variation_workflow(phase152_system.serial_state, phase152_system.shell_state, phase152_system.log_state, phase152_system.echo_state, phase152_system.kv_state, runtime.shell_service_endpoint_id, identity.init.pid, runtime.log_service_directory_key, runtime.init_endpoint_id, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 86, 0), shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 0, 0), shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 86), shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 0))
    if phase152_workflow.succeeded == 0 {
        return 108
    }
    if !validate_phase152c_serial_behavior(phase152_workflow, runtime.shell_service_endpoint_id) {
        return 109
    }

    phase152_audit: debug.Phase152PatternStabilityAudit = bootstrap_audit.build_phase152_pattern_stability_audit(bootstrap_audit.Phase152PatternStabilityAuditInputs{ phase150: phase150_audit, serial_service_pid: phase152_workflow.varied_system.serial_state.owner_pid, shell_service_pid: phase152_workflow.varied_system.shell_state.owner_pid, log_service_pid: phase152_workflow.varied_system.log_state.owner_pid, kv_service_pid: phase152_workflow.varied_system.kv_state.owner_pid, pre_restart_log_append_route: phase152_workflow.pre_restart_log_append_route, pre_restart_log_tail_route: phase152_workflow.pre_restart_log_tail_route, pre_restart_kv_set_route: phase152_workflow.pre_restart_kv_set_route, pre_restart_kv_get_route: phase152_workflow.pre_restart_kv_get_route, post_restart_log_tail_route: phase152_workflow.post_restart_log_tail_route, post_restart_kv_get_route: phase152_workflow.post_restart_kv_get_route, pre_restart_log_retention: phase152_workflow.pre_restart_log_retention, post_restart_log_retention: phase152_workflow.post_restart_log_retention, post_restart_kv_retention: phase152_workflow.post_restart_kv_retention, restart: phase152_workflow.restart, rebuilt_system_helper_survives_visible: 1, shell_command_shape_survives_visible: 1, unaffected_peer_state_survives_visible: 1, restarted_service_state_resets_visible: 1, restart_evidence_moves_outside_restarted_owner_visible: 1, partial_generalization_visible: 1, full_generalization_visible: 0, pattern_failure_visible: 0, dynamic_service_framework_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase152_pattern_stability(phase152_audit, contracts.scheduler, contracts.lifecycle, contracts.capability, contracts.ipc, contracts.address_space, contracts.interrupt, contracts.timer, contracts.barrier) {
        return 110
    }

    return 0
}