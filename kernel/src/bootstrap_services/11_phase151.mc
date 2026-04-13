import address_space
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

struct LateServiceSystemContractFlags {
    scheduler_contract_hardened: u32
    lifecycle_contract_hardened: u32
    capability_contract_hardened: u32
    ipc_contract_hardened: u32
    address_space_contract_hardened: u32
    interrupt_contract_hardened: u32
    timer_contract_hardened: u32
    barrier_contract_hardened: u32
}

struct LateServiceSystemRuntimeContext {
    boot_pid: u32
    init_pid: u32
    init_tid: u32
    init_asid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    phase141_shell_pid: u32
    phase141_kv_pid: u32
    init_endpoint_id: u32
    serial_service_endpoint_id: u32
    composition_echo_endpoint_id: u32
    composition_log_endpoint_id: u32
    kv_service_endpoint_id: u32
    shell_service_endpoint_id: u32
    kv_service_directory_key: u32
    init_root_page_table: usize
    child_root_page_table: usize
    init_image: init.InitImage
    serial_receive_observation: syscall.ReceiveObservation
    log_service_state: log_service.LogServiceState
    echo_service_state: echo_service.EchoServiceState
}

struct LateServiceSystemProofInputs {
    phase137_audit: debug.Phase137OptionalDmaOrEquivalentAudit
    scheduler_contract_hardened: u32
    lifecycle_contract_hardened: u32
    capability_contract_hardened: u32
    ipc_contract_hardened: u32
    address_space_contract_hardened: u32
    interrupt_contract_hardened: u32
    timer_contract_hardened: u32
    barrier_contract_hardened: u32
    boot_pid: u32
    init_pid: u32
    init_tid: u32
    init_asid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    phase141_shell_pid: u32
    phase141_kv_pid: u32
    init_endpoint_id: u32
    serial_service_endpoint_id: u32
    composition_echo_endpoint_id: u32
    composition_log_endpoint_id: u32
    kv_service_endpoint_id: u32
    shell_service_endpoint_id: u32
    kv_service_directory_key: u32
    init_root_page_table: usize
    child_root_page_table: usize
    init_image: init.InitImage
    serial_receive_observation: syscall.ReceiveObservation
    log_service_state: log_service.LogServiceState
    echo_service_state: echo_service.EchoServiceState
}

func build_late_service_system_proof_inputs(phase137_audit: debug.Phase137OptionalDmaOrEquivalentAudit, contract_flags: LateServiceSystemContractFlags, runtime_context: LateServiceSystemRuntimeContext) LateServiceSystemProofInputs {
    return LateServiceSystemProofInputs{ phase137_audit: phase137_audit, scheduler_contract_hardened: contract_flags.scheduler_contract_hardened, lifecycle_contract_hardened: contract_flags.lifecycle_contract_hardened, capability_contract_hardened: contract_flags.capability_contract_hardened, ipc_contract_hardened: contract_flags.ipc_contract_hardened, address_space_contract_hardened: contract_flags.address_space_contract_hardened, interrupt_contract_hardened: contract_flags.interrupt_contract_hardened, timer_contract_hardened: contract_flags.timer_contract_hardened, barrier_contract_hardened: contract_flags.barrier_contract_hardened, boot_pid: runtime_context.boot_pid, init_pid: runtime_context.init_pid, init_tid: runtime_context.init_tid, init_asid: runtime_context.init_asid, child_pid: runtime_context.child_pid, child_tid: runtime_context.child_tid, child_asid: runtime_context.child_asid, phase141_shell_pid: runtime_context.phase141_shell_pid, phase141_kv_pid: runtime_context.phase141_kv_pid, init_endpoint_id: runtime_context.init_endpoint_id, serial_service_endpoint_id: runtime_context.serial_service_endpoint_id, composition_echo_endpoint_id: runtime_context.composition_echo_endpoint_id, composition_log_endpoint_id: runtime_context.composition_log_endpoint_id, kv_service_endpoint_id: runtime_context.kv_service_endpoint_id, shell_service_endpoint_id: runtime_context.shell_service_endpoint_id, kv_service_directory_key: runtime_context.kv_service_directory_key, init_root_page_table: runtime_context.init_root_page_table, child_root_page_table: runtime_context.child_root_page_table, init_image: runtime_context.init_image, serial_receive_observation: runtime_context.serial_receive_observation, log_service_state: runtime_context.log_service_state, echo_service_state: runtime_context.echo_service_state }
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
    return phase150_rebuilt_system_state(serial_service.record_ingress(serial_service.service_state(inputs.init_pid, serial_endpoint_handle_slot), inputs.serial_receive_observation), shell_service.service_state(inputs.phase141_shell_pid, 1, inputs.composition_echo_endpoint_id, inputs.composition_log_endpoint_id, inputs.kv_service_endpoint_id), log_state, inputs.echo_service_state, kv_service.service_state(inputs.phase141_kv_pid, 1))
}

func build_late_rebuilt_system_state_with_retained_log(inputs: LateServiceSystemProofInputs, serial_endpoint_handle_slot: u32) Phase150RebuiltSystemState {
    return build_late_rebuilt_system_state(inputs, serial_endpoint_handle_slot, inputs.log_service_state)
}

func build_late_rebuilt_system_state_with_fresh_log(inputs: LateServiceSystemProofInputs, serial_endpoint_handle_slot: u32) Phase150RebuiltSystemState {
    return build_late_rebuilt_system_state(inputs, serial_endpoint_handle_slot, log_service.service_state(inputs.log_service_state.owner_pid, inputs.log_service_state.endpoint_handle_slot))
}

func execute_late_service_system_proofs(inputs: LateServiceSystemProofInputs) i32 {
    phase140_serial_config: SerialServiceConfig = serial_service_composition_config(inputs.boot_pid, inputs.init_pid, inputs.init_tid, inputs.init_asid, inputs.serial_service_endpoint_id, inputs.init_endpoint_id, mmu.bootstrap_translation_root(inputs.init_asid, inputs.init_root_page_table))
    phase140_serial_state: serial_service.SerialServiceState = serial_service.record_ingress(serial_service.service_state(inputs.init_pid, phase140_serial_config.endpoint_handle_slot), inputs.serial_receive_observation)
    phase140_serial_execution: SerialServiceExecutionState = SerialServiceExecutionState{ program_capability: capability.empty_slot(), gate: syscall.open_gate(syscall.gate_closed()), process_slots: state.zero_process_slots(), task_slots: state.zero_task_slots(), init_handle_table: capability.empty_handle_table(), child_handle_table: capability.handle_table_for_owner(inputs.init_pid), wait_table: capability.empty_wait_table(), endpoints: ipc.empty_table(), init_image: inputs.init_image, child_address_space: address_space.empty_space(), child_user_frame: address_space.empty_frame(), service_state: phase140_serial_state, spawn_observation: syscall.empty_spawn_observation(), receive_observation: inputs.serial_receive_observation, ingress: serial_service.observe_ingress(phase140_serial_state), composition: serial_service.observe_composition(phase140_serial_state), failure_observation: empty_serial_service_failure_observation(), wait_observation: syscall.empty_wait_observation(), ready_queue: state.empty_queue() }
    phase140_composition_config: CompositionServiceConfig = composition_service_config(inputs.init_pid, inputs.child_pid, inputs.child_tid, inputs.child_asid, inputs.init_endpoint_id, inputs.composition_echo_endpoint_id, inputs.composition_log_endpoint_id, phase140_serial_config.composition_handle_slot, mmu.bootstrap_translation_root(inputs.child_asid, inputs.child_root_page_table))
    phase140_composition_execution: CompositionServiceExecutionState = empty_composition_service_execution_state()
    phase140_composition_execution = CompositionServiceExecutionState{ program_capability: phase140_composition_execution.program_capability, gate: phase140_composition_execution.gate, process_slots: phase140_composition_execution.process_slots, task_slots: phase140_composition_execution.task_slots, init_handle_table: phase140_composition_execution.init_handle_table, child_handle_table: capability.handle_table_for_owner(inputs.child_pid), wait_table: phase140_composition_execution.wait_table, endpoints: phase140_composition_execution.endpoints, init_image: inputs.init_image, child_address_space: phase140_composition_execution.child_address_space, child_user_frame: phase140_composition_execution.child_user_frame, echo_peer_state: phase140_composition_execution.echo_peer_state, log_peer_state: phase140_composition_execution.log_peer_state, spawn_observation: phase140_composition_execution.spawn_observation, request_receive_observation: phase140_composition_execution.request_receive_observation, echo_fanout_observation: phase140_composition_execution.echo_fanout_observation, echo_peer_receive_observation: phase140_composition_execution.echo_peer_receive_observation, echo_reply_send_observation: phase140_composition_execution.echo_reply_send_observation, echo_reply_observation: phase140_composition_execution.echo_reply_observation, log_fanout_observation: phase140_composition_execution.log_fanout_observation, log_peer_receive_observation: phase140_composition_execution.log_peer_receive_observation, log_ack_send_observation: phase140_composition_execution.log_ack_send_observation, log_ack_observation: phase140_composition_execution.log_ack_observation, aggregate_reply_send_observation: phase140_composition_execution.aggregate_reply_send_observation, aggregate_reply_observation: phase140_composition_execution.aggregate_reply_observation, wait_observation: phase140_composition_execution.wait_observation, observation: phase140_composition_execution.observation, ready_queue: phase140_composition_execution.ready_queue }
    phase140_result: Phase140SerialIngressCompositionResult = execute_phase140_serial_ingress_composed_service_graph(phase140_serial_config, phase140_composition_config, phase140_serial_execution, phase140_composition_execution)
    if phase140_result.succeeded == 0 {
        return 95
    }
    phase140_audit: debug.Phase140SerialIngressComposedServiceGraphAudit = bootstrap_audit.build_phase140_serial_ingress_composed_service_graph_audit(bootstrap_audit.Phase140SerialIngressComposedServiceGraphAuditInputs{ phase137: inputs.phase137_audit, serial_service_pid: phase140_result.serial_state.composition.service_pid, composition_service_pid: phase140_result.composition_state.observation.service_pid, serial_forward_endpoint_id: phase140_result.serial_state.composition.forward_endpoint_id, serial_forward_status: phase140_result.serial_state.composition.forward_status, serial_forward_request_len: phase140_result.serial_state.composition.forwarded_request_len, serial_forward_request_byte0: phase140_result.serial_state.composition.forwarded_request_byte0, serial_forward_request_byte1: phase140_result.serial_state.composition.forwarded_request_byte1, serial_forward_count: phase140_result.serial_state.composition.forwarded_request_count, composition_request_receive_status: phase140_result.composition_state.request_receive_observation.status, composition_request_source_pid: phase140_result.composition_state.request_receive_observation.source_pid, composition_request_len: phase140_result.composition_state.observation.request_len, composition_request_byte0: phase140_result.composition_state.observation.request_byte0, composition_request_byte1: phase140_result.composition_state.observation.request_byte1, composition_control_endpoint_id: phase140_composition_config.control_endpoint_id, composition_echo_endpoint_id: phase140_composition_config.echo_endpoint_id, composition_log_endpoint_id: phase140_composition_config.log_endpoint_id, composition_echo_fanout_status: phase140_result.composition_state.echo_fanout_observation.status, composition_echo_fanout_endpoint_id: phase140_result.composition_state.echo_fanout_observation.endpoint_id, composition_log_fanout_status: phase140_result.composition_state.log_fanout_observation.status, composition_log_fanout_endpoint_id: phase140_result.composition_state.log_fanout_observation.endpoint_id, composition_aggregate_reply_status: phase140_result.composition_state.aggregate_reply_observation.status, composition_outbound_edge_count: phase140_result.composition_state.observation.outbound_edge_count, composition_aggregate_reply_count: phase140_result.composition_state.observation.aggregate_reply_count, serial_aggregate_reply_status: phase140_result.serial_state.composition.aggregate_reply_status, serial_aggregate_reply_len: phase140_result.serial_state.composition.aggregate_reply_len, serial_aggregate_reply_byte0: phase140_result.serial_state.composition.aggregate_reply_byte0, serial_aggregate_reply_byte1: phase140_result.serial_state.composition.aggregate_reply_byte1, serial_aggregate_reply_byte2: phase140_result.serial_state.composition.aggregate_reply_byte2, serial_aggregate_reply_byte3: phase140_result.serial_state.composition.aggregate_reply_byte3, serial_aggregate_reply_count: phase140_result.serial_state.composition.aggregate_reply_count, kernel_broker_visible: 0, dynamic_routing_visible: 0, general_service_graph_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase140_serial_ingress_composed_service_graph(phase140_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 96
    }

    phase141_request_len: usize = 4
    phase141_request_payload: [4]u8 = shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 33, 33)
    phase141_serial_state: serial_service.SerialServiceState = serial_service.record_ingress(serial_service.service_state(inputs.init_pid, phase140_serial_config.endpoint_handle_slot), inputs.serial_receive_observation)
    phase141_serial_state = serial_service.record_forward(phase141_serial_state, inputs.shell_service_endpoint_id, syscall.SyscallStatus.Ok, phase141_request_len, phase141_request_payload)
    phase141_shell_state: shell_service.ShellServiceState = shell_service.record_request(shell_service.service_state(inputs.phase141_shell_pid, 1, inputs.composition_echo_endpoint_id, inputs.composition_log_endpoint_id, inputs.kv_service_endpoint_id), phase141_serial_state.owner_pid, inputs.shell_service_endpoint_id, phase141_request_len, phase141_request_payload)
    phase141_log_state: log_service.LogServiceState = inputs.log_service_state
    phase141_echo_state: echo_service.EchoServiceState = inputs.echo_service_state
    phase141_kv_state: kv_service.KvServiceState = kv_service.record_set(kv_service.service_state(inputs.phase141_kv_pid, 1), inputs.phase141_shell_pid, inputs.kv_service_endpoint_id, 75, 86)
    phase141_reply_payload: [4]u8 = shell_service.reply_payload(phase141_shell_state, phase141_echo_state, phase141_log_state, phase141_kv_state)
    phase141_shell_state = shell_service.record_reply(phase141_shell_state, syscall.SyscallStatus.Ok, 2, phase141_reply_payload)
    phase141_audit: debug.Phase141InteractiveServiceSystemScopeFreezeAudit = bootstrap_audit.build_phase141_interactive_service_system_scope_freeze_audit(bootstrap_audit.Phase141InteractiveServiceSystemScopeFreezeAuditInputs{ phase140: phase140_audit, serial_service_pid: phase141_serial_state.owner_pid, shell_service_pid: phase141_shell_state.owner_pid, kv_service_pid: phase141_kv_state.owner_pid, serial_forward_endpoint_id: phase141_serial_state.forward_endpoint_id, serial_forward_status: phase141_serial_state.forward_status, serial_forward_request_len: phase141_serial_state.forwarded_request_len, serial_forward_request_byte0: phase141_serial_state.forwarded_request_byte0, serial_forward_request_byte1: phase141_serial_state.forwarded_request_byte1, shell_tag: phase141_shell_state.last_tag, shell_request_len: phase141_shell_state.last_request_len, shell_request_byte0: phase141_shell_state.last_byte0, shell_request_byte1: phase141_shell_state.last_byte1, shell_request_byte2: phase141_shell_state.last_byte2, shell_endpoint_id: inputs.shell_service_endpoint_id, echo_endpoint_id: inputs.composition_echo_endpoint_id, log_endpoint_id: inputs.composition_log_endpoint_id, kv_endpoint_id: inputs.kv_service_endpoint_id, echo_route_count: phase141_shell_state.echo_route_count, log_route_count: phase141_shell_state.log_route_count, kv_route_count: phase141_shell_state.kv_route_count, invalid_command_count: phase141_shell_state.invalid_command_count, shell_reply_status: phase141_shell_state.reply_status, shell_reply_len: phase141_shell_state.reply_len, shell_reply_byte0: phase141_shell_state.reply_byte0, shell_reply_byte1: phase141_shell_state.reply_byte1, shell_reply_byte2: phase141_shell_state.reply_byte2, shell_reply_byte3: phase141_shell_state.reply_byte3, fixed_vocabulary_visible: 1, kernel_broker_visible: 0, dynamic_routing_visible: 0, general_shell_framework_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase141_interactive_service_system_scope_freeze(phase141_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 97
    }

    phase142_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_retained_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase142_echo_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_ECHO_BYTE0, SHELL_COMMAND_ECHO_BYTE1, 72, 73))
    phase142_log_append_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_echo_result.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 80, 33))
    phase142_log_tail_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_log_append_result.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 33, 33))
    phase142_kv_set_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_log_tail_result.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 86))
    phase142_kv_get_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_kv_set_result.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase142_invalid_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase142_kv_get_result.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(88, 88, 63, 63))
    phase142_audit: debug.Phase142SerialShellCommandRoutingAudit = bootstrap_audit.build_phase142_serial_shell_command_routing_audit(bootstrap_audit.Phase142SerialShellCommandRoutingAuditInputs{ phase141: phase141_audit, serial_service_pid: phase142_invalid_result.system.serial_state.owner_pid, shell_service_pid: phase142_invalid_result.system.shell_state.owner_pid, kv_service_pid: phase142_invalid_result.system.kv_state.owner_pid, serial_forward_endpoint_id: phase142_invalid_result.system.serial_state.forward_endpoint_id, serial_forward_status: phase142_invalid_result.system.serial_state.forward_status, serial_forward_count: phase142_invalid_result.system.serial_state.forwarded_request_count, serial_final_reply_status: phase142_invalid_result.system.serial_state.aggregate_reply_status, serial_final_reply_len: phase142_invalid_result.system.serial_state.aggregate_reply_len, serial_final_reply_byte0: phase142_invalid_result.system.serial_state.aggregate_reply_byte0, serial_final_reply_count: phase142_invalid_result.system.serial_state.aggregate_reply_count, echo_route: phase142_echo_result.routing, log_append_route: phase142_log_append_result.routing, log_tail_route: phase142_log_tail_result.routing, kv_set_route: phase142_kv_set_result.routing, kv_get_route: phase142_kv_get_result.routing, invalid_route: phase142_invalid_result.routing, compact_command_encoding_visible: 1, malformed_command_visible: 1, general_shell_framework_visible: 0, payload_width_reopened_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase142_serial_shell_command_routing(phase142_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 98
    }

    phase143_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase143_append_a: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 65, 33))
    phase143_append_b: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_append_a.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 66, 33))
    phase143_append_c: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_append_b.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 67, 33))
    phase143_append_d: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_append_c.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 68, 33))
    phase143_overflow_append: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_append_d.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 69, 33))
    phase143_tail_result: Phase150RebuiltSystemCommandResult = execute_phase142_rebuilt_system_command(phase143_overflow_append.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 33, 33))
    phase143_audit: debug.Phase143LongLivedLogServiceAudit = bootstrap_audit.build_phase143_long_lived_log_service_audit(bootstrap_audit.Phase143LongLivedLogServiceAuditInputs{ phase142: phase142_audit, log_service_pid: phase143_tail_result.system.log_state.owner_pid, shell_service_pid: phase143_tail_result.system.shell_state.owner_pid, overflow_append_route: phase143_overflow_append.routing, tail_route: phase143_tail_result.routing, retention: log_service.observe_retention(phase143_tail_result.system.log_state), bounded_retention_visible: 1, explicit_overwrite_policy_visible: 1, durable_persistence_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase143_long_lived_log_service(phase143_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 99
    }

    phase144_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase144_missing_get: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase144_initial_set: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_missing_get.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 86))
    phase144_hit_get: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_initial_set.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase144_overwrite_set: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_hit_get.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 87))
    phase144_overwrite_get: Phase150RebuiltSystemCommandResult = execute_phase144_rebuilt_system_command(phase144_overwrite_set.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase144_audit: debug.Phase144StatefulKeyValueServiceFollowThroughAudit = bootstrap_audit.build_phase144_stateful_key_value_service_follow_through_audit(bootstrap_audit.Phase144StatefulKeyValueServiceFollowThroughAuditInputs{ phase143: phase143_audit, kv_service_pid: phase144_overwrite_get.system.kv_state.owner_pid, shell_service_pid: phase144_overwrite_get.system.shell_state.owner_pid, log_service_pid: phase144_overwrite_set.system.log_state.owner_pid, missing_get_route: phase144_missing_get.routing, initial_set_route: phase144_initial_set.routing, hit_get_route: phase144_hit_get.routing, overwrite_set_route: phase144_overwrite_set.routing, overwrite_get_route: phase144_overwrite_get.routing, kv_retention: kv_service.observe_retention(phase144_overwrite_get.system.kv_state), write_log_retention: log_service.observe_retention(phase144_overwrite_set.system.log_state), bounded_retention_visible: 1, missing_key_visible: 1, explicit_overwrite_visible: 1, fixed_write_log_visible: 1, durable_persistence_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase144_stateful_key_value_service_follow_through(phase144_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 100
    }

    phase145_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase145_pre_failure_set: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase145_system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 86))
    phase145_pre_failure_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase145_pre_failure_set.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))

    phase145_pre_failure_retention: kv_service.KvRetentionObservation = kv_service.observe_retention(phase145_pre_failure_get.system.kv_state)
    phase145_failed_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase150_mark_kv_unavailable(phase145_pre_failure_get.system), inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))

    phase145_restart: Phase150RebuiltSystemRestartResult = phase150_restart_kv_service(phase145_failed_get.system, inputs.init_pid, inputs.kv_service_directory_key, inputs.init_endpoint_id)

    phase145_restarted_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase145_restart.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    phase145_audit: debug.Phase145ServiceRestartFailureAndUsagePressureAudit = bootstrap_audit.build_phase145_service_restart_failure_and_usage_pressure_audit(bootstrap_audit.Phase145ServiceRestartFailureAndUsagePressureAuditInputs{ phase144: phase144_audit, kv_service_pid: phase145_restarted_get.system.kv_state.owner_pid, shell_service_pid: phase145_restarted_get.system.shell_state.owner_pid, log_service_pid: phase145_restarted_get.system.log_state.owner_pid, init_policy_owner_pid: inputs.init_pid, pre_failure_set_route: phase145_pre_failure_set.routing, pre_failure_get_route: phase145_pre_failure_get.routing, failed_get_route: phase145_failed_get.routing, restarted_get_route: phase145_restarted_get.routing, pre_failure_retention: phase145_pre_failure_retention, post_restart_retention: kv_service.observe_retention(phase145_restarted_get.system.kv_state), restart: phase145_restart.restart, event_log_retention: log_service.observe_retention(phase145_restarted_get.system.log_state), shell_failure_visible: 1, init_restart_visible: 1, post_restart_state_reset_visible: 1, manual_retry_visible: 1, kernel_supervision_visible: 0, durable_persistence_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase145_service_restart_failure_and_usage_pressure(phase145_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 101
    }

    phase146_audit: debug.Phase146ServiceShapeConsolidationAudit = bootstrap_audit.build_phase146_service_shape_consolidation_audit(bootstrap_audit.Phase146ServiceShapeConsolidationAuditInputs{ phase145: phase145_audit, serial_service_pid: phase145_restarted_get.system.serial_state.owner_pid, shell_service_pid: phase145_restarted_get.system.shell_state.owner_pid, log_service_pid: phase145_restarted_get.system.log_state.owner_pid, kv_service_pid: phase145_restarted_get.system.kv_state.owner_pid, init_policy_owner_pid: inputs.init_pid, serial_forward: serial_service.observe_composition(phase145_restarted_get.system.serial_state), shell_route: phase145_restarted_get.routing, log_retention: log_service.observe_retention(phase145_restarted_get.system.log_state), kv_retention: kv_service.observe_retention(phase145_restarted_get.system.kv_state), restart: phase145_restart.restart, serial_endpoint_handle_slot: phase145_restarted_get.system.serial_state.endpoint_handle_slot, shell_endpoint_handle_slot: phase145_restarted_get.system.shell_state.endpoint_handle_slot, log_endpoint_handle_slot: phase145_restarted_get.system.log_state.endpoint_handle_slot, kv_endpoint_handle_slot: phase145_restarted_get.system.kv_state.endpoint_handle_slot, fixed_boot_wiring_visible: 1, service_local_request_reply_visible: 1, shell_syntax_boundary_visible: 1, service_semantic_boundary_visible: 1, explicit_failure_exposure_visible: 1, init_owned_restart_visible: 1, intentional_shell_difference_visible: 1, intentional_stateful_difference_visible: 1, dynamic_service_framework_visible: 0, dynamic_registration_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase146_service_shape_consolidation(phase146_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 102
    }

    phase147_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)

    phase147_log_append: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_system, inputs.shell_service_endpoint_id, 3, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 76, 0))
    phase147_log_tail: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_log_append.system, inputs.shell_service_endpoint_id, 2, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 0, 0))
    phase147_kv_set: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_log_tail.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 81, 90))
    phase147_kv_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_kv_set.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 81, 0))
    phase147_repeated_log_tail: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_kv_get.system, inputs.shell_service_endpoint_id, 2, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 0, 0))
    phase147_repeated_kv_get: Phase150RebuiltSystemCommandResult = execute_phase150_rebuilt_system_command(phase147_repeated_log_tail.system, inputs.shell_service_endpoint_id, 4, shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 81, 0))
    phase147_audit: debug.Phase147IpcShapeAudit = bootstrap_audit.build_phase147_ipc_shape_audit(bootstrap_audit.Phase147IpcShapeAuditInputs{ phase146: phase146_audit, serial_service_pid: phase147_repeated_kv_get.system.serial_state.owner_pid, shell_service_pid: phase147_repeated_kv_get.system.shell_state.owner_pid, log_service_pid: phase147_repeated_kv_get.system.log_state.owner_pid, kv_service_pid: phase147_repeated_kv_get.system.kv_state.owner_pid, serial_observation: serial_service.observe_composition(phase147_repeated_kv_get.system.serial_state), log_append_route: phase147_log_append.routing, log_tail_route: phase147_log_tail.routing, kv_set_route: phase147_kv_set.routing, kv_get_route: phase147_kv_get.routing, repeated_log_tail_route: phase147_repeated_log_tail.routing, repeated_kv_get_route: phase147_repeated_kv_get.routing, log_retention: log_service.observe_retention(phase147_repeated_kv_get.system.log_state), kv_retention: kv_service.observe_retention(phase147_repeated_kv_get.system.kv_state), request_construction_sufficient_visible: 1, reply_shape_service_local_visible: 1, compact_encoding_sufficient_visible: 1, service_to_service_observation_visible: 1, generic_message_bus_visible: 0, dynamic_payload_typing_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase147_ipc_shape_audit_under_real_usage(phase147_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 103
    }

    phase148_audit: debug.Phase148AuthorityErgonomicsAudit = bootstrap_audit.build_phase148_authority_ergonomics_audit(bootstrap_audit.Phase148AuthorityErgonomicsAuditInputs{ phase147: phase147_audit, serial_endpoint_handle_slot: phase147_repeated_kv_get.system.serial_state.endpoint_handle_slot, shell_endpoint_handle_slot: phase147_repeated_kv_get.system.shell_state.endpoint_handle_slot, log_endpoint_handle_slot: phase147_repeated_kv_get.system.log_state.endpoint_handle_slot, kv_endpoint_handle_slot: phase147_repeated_kv_get.system.kv_state.endpoint_handle_slot, explicit_slot_authority_visible: 1, retained_state_authority_local_visible: 1, restart_handoff_explicit_visible: 1, repeated_authority_ceremony_stable_visible: 1, narrow_capability_helper_visible: 1, overscoped_helper_visible: 0, ambient_authority_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase148_authority_ergonomics_under_reuse(phase148_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 104
    }

    phase149_audit: debug.Phase149RestartSemanticsFirstClassPatternAudit = bootstrap_audit.build_phase149_restart_semantics_first_class_pattern_audit(bootstrap_audit.Phase149RestartSemanticsFirstClassPatternAuditInputs{ phase148: phase148_audit, shell_service_pid: phase145_audit.shell_service_pid, kv_service_pid: phase145_audit.kv_service_pid, log_service_pid: phase145_audit.log_service_pid, init_policy_owner_pid: phase145_audit.init_policy_owner_pid, pre_failure_get_route: phase145_audit.pre_failure_get_route, failed_get_route: phase145_audit.failed_get_route, restarted_get_route: phase145_audit.restarted_get_route, post_restart_retention: phase145_audit.post_restart_retention, restart: phase145_audit.restart, event_log_retention: phase145_audit.event_log_retention, caller_retry_obligation_visible: 1, retry_reissues_same_request_visible: 1, request_identity_not_tracked_visible: 1, post_restart_reset_state_visible: 1, init_owned_restart_policy_visible: 1, transparent_retry_visible: 0, kernel_supervision_visible: 0, durable_recovery_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase149_restart_semantics_first_class_pattern(phase149_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 105
    }

    phase150_system: Phase150RebuiltSystemState = build_late_rebuilt_system_state_with_fresh_log(inputs, phase140_serial_config.endpoint_handle_slot)
    phase150_workflow: Phase150RebuiltSystemWorkflowResult = execute_phase150_rebuilt_system_workflow(phase150_system.serial_state, phase150_system.shell_state, phase150_system.log_state, phase150_system.echo_state, phase150_system.kv_state, inputs.shell_service_endpoint_id, inputs.init_pid, inputs.kv_service_directory_key, inputs.init_endpoint_id, shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_APPEND_BYTE1, 76, 0), shell_command_payload(SHELL_COMMAND_LOG_BYTE0, SHELL_COMMAND_LOG_TAIL_BYTE1, 0, 0), shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_SET_BYTE1, 75, 86), shell_command_payload(SHELL_COMMAND_KV_BYTE0, SHELL_COMMAND_KV_GET_BYTE1, 75, 33))
    if phase150_workflow.succeeded == 0 {
        return 106
    }

    phase150_audit: debug.Phase150OneSystemRebuiltCleanlyAudit = bootstrap_audit.build_phase150_one_system_rebuilt_cleanly_audit(bootstrap_audit.Phase150OneSystemRebuiltCleanlyAuditInputs{ phase149: phase149_audit, serial_service_pid: phase150_workflow.rebuilt_system.serial_state.owner_pid, shell_service_pid: phase150_workflow.rebuilt_system.shell_state.owner_pid, log_service_pid: phase150_workflow.rebuilt_system.log_state.owner_pid, kv_service_pid: phase150_workflow.rebuilt_system.kv_state.owner_pid, log_append_route: phase150_workflow.log_append_route, log_tail_route: phase150_workflow.log_tail_route, pre_failure_set_route: phase150_workflow.pre_failure_set_route, pre_failure_get_route: phase150_workflow.pre_failure_get_route, failed_get_route: phase150_workflow.failed_get_route, restarted_get_route: phase150_workflow.restarted_get_route, pre_failure_retention: phase150_workflow.pre_failure_retention, post_restart_retention: phase150_workflow.post_restart_retention, restart: phase150_workflow.restart, event_log_retention: phase150_workflow.event_log_retention, rebuilt_system_helper_visible: 1, owner_local_system_state_visible: 1, manual_state_threading_removed_visible: 1, dynamic_service_framework_visible: 0, compiler_reopening_visible: 0 })
    if !debug.validate_phase150_one_system_rebuilt_cleanly(phase150_audit, inputs.scheduler_contract_hardened, inputs.lifecycle_contract_hardened, inputs.capability_contract_hardened, inputs.ipc_contract_hardened, inputs.address_space_contract_hardened, inputs.interrupt_contract_hardened, inputs.timer_contract_hardened, inputs.barrier_contract_hardened) {
        return 107
    }

    return 0
}