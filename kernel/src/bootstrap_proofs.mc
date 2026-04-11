import address_space
import bootstrap_audit
import bootstrap_services
import capability
import debug
import echo_service
import ipc
import init
import log_service
import state
import syscall
import timer
import transfer_service

struct LatePhaseProofContext {
    init_pid: u32
    init_endpoint_id: u32
    transfer_endpoint_id: u32
    log_service_directory_key: u32
    echo_service_directory_key: u32
    composition_service_directory_key: u32
    phase124_intermediary_pid: u32
    phase124_final_holder_pid: u32
    phase124_control_handle_slot: u32
    phase124_intermediary_receive_handle_slot: u32
    phase124_final_receive_handle_slot: u32
}

var INVALIDATED_SOURCE_SEND_STATUS: syscall.SyscallStatus
var PHASE124_DELEGATOR_INVALIDATED_SEND_STATUS: syscall.SyscallStatus
var PHASE124_INTERMEDIARY_INVALIDATED_SEND_STATUS: syscall.SyscallStatus
var PHASE124_FINAL_SEND_STATUS: syscall.SyscallStatus
var PHASE124_FINAL_SEND_SOURCE_PID: u32
var PHASE124_FINAL_ENDPOINT_QUEUE_DEPTH: usize
var PHASE125_REJECTED_SEND_STATUS: syscall.SyscallStatus
var PHASE125_REJECTED_RECEIVE_STATUS: syscall.SyscallStatus
var PHASE125_SURVIVING_CONTROL_SEND_STATUS: syscall.SyscallStatus
var PHASE125_SURVIVING_CONTROL_SOURCE_PID: u32
var PHASE125_SURVIVING_CONTROL_QUEUE_DEPTH: usize
var PHASE126_REPEAT_LONG_LIVED_SEND_STATUS: syscall.SyscallStatus
var PHASE126_REPEAT_LONG_LIVED_SOURCE_PID: u32
var PHASE126_REPEAT_LONG_LIVED_QUEUE_DEPTH: usize
var PHASE128_OBSERVED_SERVICE_PID: u32
var PHASE128_OBSERVED_SERVICE_KEY: u32
var PHASE128_OBSERVED_WAIT_HANDLE_SLOT: u32
var PHASE128_OBSERVED_EXIT_CODE: i32
var PHASE129_FAILED_SERVICE_PID: u32
var PHASE129_FAILED_WAIT_STATUS: syscall.SyscallStatus
var PHASE129_SURVIVING_SERVICE_PID: u32
var PHASE129_SURVIVING_REPLY_STATUS: syscall.SyscallStatus
var PHASE129_SURVIVING_WAIT_STATUS: syscall.SyscallStatus
var PHASE129_SURVIVING_REPLY_BYTE0: u8
var PHASE129_SURVIVING_REPLY_BYTE1: u8
var PHASE130_REPLACEMENT_SERVICE_PID: u32
var PHASE130_REPLACEMENT_SPAWN_STATUS: syscall.SyscallStatus
var PHASE130_REPLACEMENT_ACK_STATUS: syscall.SyscallStatus
var PHASE130_REPLACEMENT_WAIT_STATUS: syscall.SyscallStatus
var PHASE130_REPLACEMENT_ACK_BYTE: u8
var PHASE130_REPLACEMENT_PROGRAM_OBJECT_ID: u32
var PHASE131_COMPOSITION_STATE: bootstrap_services.CompositionServiceExecutionState

func reset_late_phase_proofs() {
    INVALIDATED_SOURCE_SEND_STATUS = syscall.SyscallStatus.None
    PHASE124_DELEGATOR_INVALIDATED_SEND_STATUS = syscall.SyscallStatus.None
    PHASE124_INTERMEDIARY_INVALIDATED_SEND_STATUS = syscall.SyscallStatus.None
    PHASE124_FINAL_SEND_STATUS = syscall.SyscallStatus.None
    PHASE124_FINAL_SEND_SOURCE_PID = 0
    PHASE124_FINAL_ENDPOINT_QUEUE_DEPTH = 0
    PHASE125_REJECTED_SEND_STATUS = syscall.SyscallStatus.None
    PHASE125_REJECTED_RECEIVE_STATUS = syscall.SyscallStatus.None
    PHASE125_SURVIVING_CONTROL_SEND_STATUS = syscall.SyscallStatus.None
    PHASE125_SURVIVING_CONTROL_SOURCE_PID = 0
    PHASE125_SURVIVING_CONTROL_QUEUE_DEPTH = 0
    PHASE126_REPEAT_LONG_LIVED_SEND_STATUS = syscall.SyscallStatus.None
    PHASE126_REPEAT_LONG_LIVED_SOURCE_PID = 0
    PHASE126_REPEAT_LONG_LIVED_QUEUE_DEPTH = 0
    PHASE128_OBSERVED_SERVICE_PID = 0
    PHASE128_OBSERVED_SERVICE_KEY = 0
    PHASE128_OBSERVED_WAIT_HANDLE_SLOT = 0
    PHASE128_OBSERVED_EXIT_CODE = 0
    PHASE129_FAILED_SERVICE_PID = 0
    PHASE129_FAILED_WAIT_STATUS = syscall.SyscallStatus.None
    PHASE129_SURVIVING_SERVICE_PID = 0
    PHASE129_SURVIVING_REPLY_STATUS = syscall.SyscallStatus.None
    PHASE129_SURVIVING_WAIT_STATUS = syscall.SyscallStatus.None
    PHASE129_SURVIVING_REPLY_BYTE0 = 0
    PHASE129_SURVIVING_REPLY_BYTE1 = 0
    PHASE130_REPLACEMENT_SERVICE_PID = 0
    PHASE130_REPLACEMENT_SPAWN_STATUS = syscall.SyscallStatus.None
    PHASE130_REPLACEMENT_ACK_STATUS = syscall.SyscallStatus.None
    PHASE130_REPLACEMENT_WAIT_STATUS = syscall.SyscallStatus.None
    PHASE130_REPLACEMENT_ACK_BYTE = 0
    PHASE130_REPLACEMENT_PROGRAM_OBJECT_ID = 0
    PHASE131_COMPOSITION_STATE = bootstrap_services.empty_composition_service_execution_state()
}

func phase118_invalidated_source_send_status() syscall.SyscallStatus {
    return INVALIDATED_SOURCE_SEND_STATUS
}

func record_phase118_invalidated_source_send_status(status: syscall.SyscallStatus) {
    INVALIDATED_SOURCE_SEND_STATUS = status
}

func build_phase108_program_cap_contract(init_pid: u32, log_config: bootstrap_services.LogServiceConfig, echo_config: bootstrap_services.EchoServiceConfig, transfer_config: bootstrap_services.TransferServiceConfig, bootstrap_program_capability: capability.CapabilitySlot, log_service_program_capability: capability.CapabilitySlot, echo_service_program_capability: capability.CapabilitySlot, transfer_service_program_capability: capability.CapabilitySlot, log_service_spawn: syscall.SpawnObservation, echo_service_spawn: syscall.SpawnObservation, transfer_service_spawn: syscall.SpawnObservation, log_service_wait: syscall.WaitObservation, echo_service_wait: syscall.WaitObservation, transfer_service_wait: syscall.WaitObservation) debug.Phase108ProgramCapContract {
    return bootstrap_audit.build_phase108_program_cap_contract(bootstrap_audit.Phase108ProgramCapContractInputs{ init_pid: init_pid, log_service_program_object_id: log_config.program_object_id, echo_service_program_object_id: echo_config.program_object_id, transfer_service_program_object_id: transfer_config.program_object_id, log_service_wait_handle_slot: log_config.wait_handle_slot, echo_service_wait_handle_slot: echo_config.wait_handle_slot, transfer_service_wait_handle_slot: transfer_config.wait_handle_slot, log_service_exit_code: log_config.exit_code, echo_service_exit_code: echo_config.exit_code, transfer_service_exit_code: transfer_config.exit_code, bootstrap_program_capability: bootstrap_program_capability, log_service_program_capability: log_service_program_capability, echo_service_program_capability: echo_service_program_capability, transfer_service_program_capability: transfer_service_program_capability, log_service_spawn: log_service_spawn, echo_service_spawn: echo_service_spawn, transfer_service_spawn: transfer_service_spawn, log_service_wait: log_service_wait, echo_service_wait: echo_service_wait, transfer_service_wait: transfer_service_wait })
}

func build_phase109_running_kernel_slice_audit(log_config: bootstrap_services.LogServiceConfig, echo_config: bootstrap_services.EchoServiceConfig, transfer_config: bootstrap_services.TransferServiceConfig, kernel: state.KernelDescriptor, init_pid: u32, init_tid: u32, init_asid: u32, child_tid: u32, child_exit_code: i32, transfer_endpoint_id: u32, init_bootstrap_handoff: init.BootstrapHandoffObservation, receive_observation: syscall.ReceiveObservation, attached_receive_observation: syscall.ReceiveObservation, transferred_handle_use_observation: syscall.ReceiveObservation, pre_exit_wait_observation: syscall.WaitObservation, exit_wait_observation: syscall.WaitObservation, sleep_observation: syscall.SleepObservation, timer_wake_observation: timer.TimerWakeObservation, log_service_handshake: log_service.LogHandshakeObservation, log_service_wait_observation: syscall.WaitObservation, echo_service_exchange: echo_service.EchoExchangeObservation, echo_service_wait_observation: syscall.WaitObservation, transfer_service_transfer: transfer_service.TransferObservation, transfer_service_wait_observation: syscall.WaitObservation, phase104_contract_hardened: u32, phase108_contract_hardened: u32, init_process: state.ProcessSlot, init_task: state.TaskSlot, init_user_frame: address_space.UserEntryFrame, boot_log_append_failed: u32) debug.RunningKernelSliceAudit {
    return bootstrap_audit.build_phase109_running_kernel_slice_audit(bootstrap_audit.RunningKernelSliceAuditInputs{ kernel: kernel, init_pid: init_pid, init_tid: init_tid, init_asid: init_asid, child_tid: child_tid, child_exit_code: child_exit_code, transfer_endpoint_id: transfer_endpoint_id, log_service_request_byte: log_config.request_byte, echo_service_request_byte0: echo_config.request_byte0, echo_service_request_byte1: echo_config.request_byte1, log_service_exit_code: log_config.exit_code, echo_service_exit_code: echo_config.exit_code, transfer_service_exit_code: transfer_config.exit_code, init_bootstrap_handoff: init_bootstrap_handoff, receive_observation: receive_observation, attached_receive_observation: attached_receive_observation, transferred_handle_use_observation: transferred_handle_use_observation, pre_exit_wait_observation: pre_exit_wait_observation, exit_wait_observation: exit_wait_observation, sleep_observation: sleep_observation, timer_wake_observation: timer_wake_observation, log_service_handshake: log_service_handshake, log_service_wait_observation: log_service_wait_observation, echo_service_exchange: echo_service_exchange, echo_service_wait_observation: echo_service_wait_observation, transfer_service_transfer: transfer_service_transfer, transfer_service_wait_observation: transfer_service_wait_observation, phase104_contract_hardened: phase104_contract_hardened, phase108_contract_hardened: phase108_contract_hardened, init_process: init_process, init_task: init_task, init_user_frame: init_user_frame, boot_log_append_failed: boot_log_append_failed })
}

func build_phase117_multi_service_bring_up_audit(running_slice_audit: debug.RunningKernelSliceAudit, init_endpoint_id: u32, transfer_endpoint_id: u32, log_service_program_capability: capability.CapabilitySlot, echo_service_program_capability: capability.CapabilitySlot, transfer_service_program_capability: capability.CapabilitySlot, log_service_spawn: syscall.SpawnObservation, echo_service_spawn: syscall.SpawnObservation, transfer_service_spawn: syscall.SpawnObservation, log_service_wait: syscall.WaitObservation, echo_service_wait: syscall.WaitObservation, transfer_service_wait: syscall.WaitObservation, log_service_handshake: log_service.LogHandshakeObservation, echo_service_exchange: echo_service.EchoExchangeObservation, transfer_service_transfer: transfer_service.TransferObservation) debug.Phase117MultiServiceBringUpAudit {
    return bootstrap_audit.build_phase117_multi_service_bring_up_audit(bootstrap_audit.Phase117MultiServiceBringUpAuditInputs{ running_slice: running_slice_audit, init_endpoint_id: init_endpoint_id, transfer_endpoint_id: transfer_endpoint_id, log_service_program_capability: log_service_program_capability, echo_service_program_capability: echo_service_program_capability, transfer_service_program_capability: transfer_service_program_capability, log_service_spawn: log_service_spawn, echo_service_spawn: echo_service_spawn, transfer_service_spawn: transfer_service_spawn, log_service_wait: log_service_wait, echo_service_wait: echo_service_wait, transfer_service_wait: transfer_service_wait, log_service_handshake: log_service_handshake, echo_service_exchange: echo_service_exchange, transfer_service_transfer: transfer_service_transfer })
}

func build_phase118_delegated_request_reply_audit(phase117_audit: debug.Phase117MultiServiceBringUpAudit, transfer_config: bootstrap_services.TransferServiceConfig, init_handle_table: capability.HandleTable, transfer_service_transfer: transfer_service.TransferObservation) debug.Phase118DelegatedRequestReplyAudit {
    retained_receive_endpoint_id: u32 = capability.find_endpoint_for_handle(init_handle_table, transfer_config.init_received_handle_slot)
    return bootstrap_audit.build_phase118_delegated_request_reply_audit(bootstrap_audit.Phase118DelegatedRequestReplyAuditInputs{ phase117: phase117_audit, transfer_service_transfer: transfer_service_transfer, invalidated_source_send_status: INVALIDATED_SOURCE_SEND_STATUS, invalidated_source_handle_slot: transfer_config.source_handle_slot, retained_receive_handle_slot: transfer_config.init_received_handle_slot, retained_receive_endpoint_id: retained_receive_endpoint_id })
}

func build_phase119_namespace_pressure_audit(phase118_audit: debug.Phase118DelegatedRequestReplyAudit, init_pid: u32, init_endpoint_id: u32, log_service_directory_key: u32, echo_service_directory_key: u32, transfer_service_directory_key: u32, log_config: bootstrap_services.LogServiceConfig, echo_config: bootstrap_services.EchoServiceConfig, transfer_config: bootstrap_services.TransferServiceConfig, log_service_spawn: syscall.SpawnObservation, echo_service_spawn: syscall.SpawnObservation, transfer_service_spawn: syscall.SpawnObservation) debug.Phase119NamespacePressureAudit {
    return bootstrap_audit.build_phase119_namespace_pressure_audit(bootstrap_audit.Phase119NamespacePressureAuditInputs{ phase118: phase118_audit, directory_owner_pid: init_pid, directory_entry_count: 3, log_service_key: log_service_directory_key, echo_service_key: echo_service_directory_key, transfer_service_key: transfer_service_directory_key, shared_directory_endpoint_id: init_endpoint_id, log_service_program_slot: log_config.program_slot, echo_service_program_slot: echo_config.program_slot, transfer_service_program_slot: transfer_config.program_slot, log_service_program_object_id: log_config.program_object_id, echo_service_program_object_id: echo_config.program_object_id, transfer_service_program_object_id: transfer_config.program_object_id, log_service_wait_handle_slot: log_service_spawn.wait_handle_slot, echo_service_wait_handle_slot: echo_service_spawn.wait_handle_slot, transfer_service_wait_handle_slot: transfer_service_spawn.wait_handle_slot, dynamic_namespace_visible: 0 })
}

func build_phase120_running_system_support_audit(phase119_audit: debug.Phase119NamespacePressureAudit, init_pid: u32, init_endpoint_id: u32) debug.Phase120RunningSystemSupportAudit {
    return bootstrap_audit.build_phase120_running_system_support_audit(bootstrap_audit.Phase120RunningSystemSupportAuditInputs{ phase119: phase119_audit, service_policy_owner_pid: init_pid, running_service_count: 3, fixed_directory_count: 1, shared_control_endpoint_id: init_endpoint_id, retained_reply_endpoint_id: phase119_audit.phase118.retained_receive_endpoint_id, program_capability_count: 3, wait_handle_count: 3, dynamic_loading_visible: 0, service_manager_visible: 0, dynamic_namespace_visible: 0 })
}

func build_phase121_kernel_image_contract_audit(phase120_audit: debug.Phase120RunningSystemSupportAudit) debug.Phase121KernelImageContractAudit {
    return bootstrap_audit.build_phase121_kernel_image_contract_audit(bootstrap_audit.Phase121KernelImageContractAuditInputs{ phase120: phase120_audit, kernel_manifest_visible: 1, kernel_target_visible: 1, kernel_runtime_startup_visible: 1, bootstrap_target_family_visible: 1, emitted_image_input_visible: 1, linked_kernel_executable_visible: 1, dynamic_loading_visible: 0, service_manager_visible: 0, dynamic_namespace_visible: 0 })
}

func build_phase122_target_surface_audit(phase121_audit: debug.Phase121KernelImageContractAudit) debug.Phase122TargetSurfaceAudit {
    return bootstrap_audit.build_phase122_target_surface_audit(bootstrap_audit.Phase122TargetSurfaceAuditInputs{ phase121: phase121_audit, kernel_target_visible: 1, kernel_runtime_startup_visible: 1, bootstrap_target_family_visible: 1, bootstrap_target_family_only_visible: 1, broader_target_family_visible: 0, dynamic_loading_visible: 0, service_manager_visible: 0, dynamic_namespace_visible: 0 })
}

func build_phase123_next_plateau_audit(phase122_audit: debug.Phase122TargetSurfaceAudit) debug.Phase123NextPlateauAudit {
    return bootstrap_audit.build_phase123_next_plateau_audit(bootstrap_audit.Phase123NextPlateauAuditInputs{ phase122: phase122_audit, running_kernel_truth_visible: 1, running_system_truth_visible: 1, kernel_image_truth_visible: 1, target_surface_truth_visible: 1, broader_platform_visible: 0, broad_target_support_visible: 0, general_loading_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase124_delegation_chain_audit(context: LatePhaseProofContext, transfer_config: bootstrap_services.TransferServiceConfig, phase123_audit: debug.Phase123NextPlateauAudit) debug.Phase124DelegationChainAudit {
    return bootstrap_audit.build_phase124_delegation_chain_audit(bootstrap_audit.Phase124DelegationChainAuditInputs{ phase123: phase123_audit, delegator_pid: context.init_pid, intermediary_pid: context.phase124_intermediary_pid, final_holder_pid: context.phase124_final_holder_pid, control_endpoint_id: context.init_endpoint_id, delegated_endpoint_id: context.transfer_endpoint_id, delegator_source_handle_slot: transfer_config.init_received_handle_slot, intermediary_receive_handle_slot: context.phase124_intermediary_receive_handle_slot, final_receive_handle_slot: context.phase124_final_receive_handle_slot, first_invalidated_send_status: PHASE124_DELEGATOR_INVALIDATED_SEND_STATUS, second_invalidated_send_status: PHASE124_INTERMEDIARY_INVALIDATED_SEND_STATUS, final_send_status: PHASE124_FINAL_SEND_STATUS, final_send_source_pid: PHASE124_FINAL_SEND_SOURCE_PID, final_endpoint_queue_depth: PHASE124_FINAL_ENDPOINT_QUEUE_DEPTH, ambient_authority_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase125_invalidation_audit(context: LatePhaseProofContext, phase124_audit: debug.Phase124DelegationChainAudit) debug.Phase125InvalidationAudit {
    return bootstrap_audit.build_phase125_invalidation_audit(bootstrap_audit.Phase125InvalidationAuditInputs{ phase124: phase124_audit, invalidated_holder_pid: context.phase124_final_holder_pid, control_endpoint_id: context.init_endpoint_id, invalidated_endpoint_id: context.transfer_endpoint_id, invalidated_handle_slot: context.phase124_final_receive_handle_slot, rejected_send_status: PHASE125_REJECTED_SEND_STATUS, rejected_receive_status: PHASE125_REJECTED_RECEIVE_STATUS, surviving_control_send_status: PHASE125_SURVIVING_CONTROL_SEND_STATUS, surviving_control_source_pid: PHASE125_SURVIVING_CONTROL_SOURCE_PID, surviving_control_queue_depth: PHASE125_SURVIVING_CONTROL_QUEUE_DEPTH, authority_loss_visible: 1, broader_revocation_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase126_authority_lifetime_audit(context: LatePhaseProofContext, phase125_audit: debug.Phase125InvalidationAudit) debug.Phase126AuthorityLifetimeAudit {
    return bootstrap_audit.build_phase126_authority_lifetime_audit(bootstrap_audit.Phase126AuthorityLifetimeAuditInputs{ phase125: phase125_audit, classified_holder_pid: context.phase124_final_holder_pid, long_lived_endpoint_id: context.init_endpoint_id, short_lived_endpoint_id: context.transfer_endpoint_id, long_lived_handle_slot: context.phase124_control_handle_slot, short_lived_handle_slot: context.phase124_final_receive_handle_slot, repeat_long_lived_send_status: PHASE126_REPEAT_LONG_LIVED_SEND_STATUS, repeat_long_lived_source_pid: PHASE126_REPEAT_LONG_LIVED_SOURCE_PID, repeat_long_lived_queue_depth: PHASE126_REPEAT_LONG_LIVED_QUEUE_DEPTH, long_lived_class_visible: 1, short_lived_class_visible: 1, broader_lifetime_framework_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase128_service_death_observation_audit(phase126_audit: debug.Phase126AuthorityLifetimeAudit) debug.Phase128ServiceDeathObservationAudit {
    return bootstrap_audit.build_phase128_service_death_observation_audit(bootstrap_audit.Phase128ServiceDeathObservationAuditInputs{ phase126: phase126_audit, observed_service_pid: PHASE128_OBSERVED_SERVICE_PID, observed_service_key: PHASE128_OBSERVED_SERVICE_KEY, observed_wait_handle_slot: PHASE128_OBSERVED_WAIT_HANDLE_SLOT, observed_exit_code: PHASE128_OBSERVED_EXIT_CODE, fixed_directory_entry_count: 3, service_death_visible: 1, kernel_supervision_visible: 0, service_restart_visible: 0, broader_failure_framework_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase129_partial_failure_propagation_audit(context: LatePhaseProofContext, log_config: bootstrap_services.LogServiceConfig, echo_config: bootstrap_services.EchoServiceConfig, phase128_audit: debug.Phase128ServiceDeathObservationAudit) debug.Phase129PartialFailurePropagationAudit {
    return bootstrap_audit.build_phase129_partial_failure_propagation_audit(bootstrap_audit.Phase129PartialFailurePropagationAuditInputs{ phase128: phase128_audit, failed_service_pid: PHASE129_FAILED_SERVICE_PID, failed_service_key: context.log_service_directory_key, failed_wait_handle_slot: log_config.wait_handle_slot, failed_wait_status: PHASE129_FAILED_WAIT_STATUS, surviving_service_pid: PHASE129_SURVIVING_SERVICE_PID, surviving_service_key: context.echo_service_directory_key, surviving_wait_handle_slot: echo_config.wait_handle_slot, surviving_reply_status: PHASE129_SURVIVING_REPLY_STATUS, surviving_wait_status: PHASE129_SURVIVING_WAIT_STATUS, surviving_reply_byte0: PHASE129_SURVIVING_REPLY_BYTE0, surviving_reply_byte1: PHASE129_SURVIVING_REPLY_BYTE1, shared_control_endpoint_id: context.init_endpoint_id, directory_entry_count: 3, partial_failure_visible: 1, kernel_recovery_visible: 0, service_rebinding_visible: 0, broader_failure_framework_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase130_explicit_restart_or_replacement_audit(context: LatePhaseProofContext, log_config: bootstrap_services.LogServiceConfig, phase129_audit: debug.Phase129PartialFailurePropagationAudit) debug.Phase130ExplicitRestartOrReplacementAudit {
    return bootstrap_audit.build_phase130_explicit_restart_or_replacement_audit(bootstrap_audit.Phase130ExplicitRestartOrReplacementAuditInputs{ phase129: phase129_audit, replacement_policy_owner_pid: context.init_pid, replacement_service_pid: PHASE130_REPLACEMENT_SERVICE_PID, replacement_service_key: context.log_service_directory_key, replacement_wait_handle_slot: log_config.wait_handle_slot, replacement_program_slot: log_config.program_slot, replacement_program_object_id: PHASE130_REPLACEMENT_PROGRAM_OBJECT_ID, replacement_spawn_status: PHASE130_REPLACEMENT_SPAWN_STATUS, replacement_ack_status: PHASE130_REPLACEMENT_ACK_STATUS, replacement_wait_status: PHASE130_REPLACEMENT_WAIT_STATUS, replacement_ack_byte: PHASE130_REPLACEMENT_ACK_BYTE, shared_control_endpoint_id: context.init_endpoint_id, directory_entry_count: 3, explicit_restart_or_replacement_visible: 1, kernel_supervision_visible: 0, service_rebinding_visible: 0, broader_failure_framework_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase131_fan_out_composition_audit(context: LatePhaseProofContext, composition_config: bootstrap_services.CompositionServiceConfig, phase130_audit: debug.Phase130ExplicitRestartOrReplacementAudit) debug.Phase131FanOutCompositionAudit {
    return bootstrap_audit.build_phase131_fan_out_composition_audit(bootstrap_audit.Phase131FanOutCompositionAuditInputs{ phase130: phase130_audit, composition_policy_owner_pid: context.init_pid, composition_service_pid: PHASE131_COMPOSITION_STATE.wait_observation.child_pid, composition_service_key: context.composition_service_directory_key, composition_wait_handle_slot: composition_config.wait_handle_slot, fixed_directory_entry_count: 4, control_endpoint_id: composition_config.control_endpoint_id, echo_endpoint_id: composition_config.echo_endpoint_id, log_endpoint_id: composition_config.log_endpoint_id, request_receive_status: PHASE131_COMPOSITION_STATE.request_receive_observation.status, echo_fanout_status: PHASE131_COMPOSITION_STATE.echo_fanout_observation.status, echo_fanout_endpoint_id: PHASE131_COMPOSITION_STATE.echo_fanout_observation.endpoint_id, log_fanout_status: PHASE131_COMPOSITION_STATE.log_fanout_observation.status, log_fanout_endpoint_id: PHASE131_COMPOSITION_STATE.log_fanout_observation.endpoint_id, echo_reply_status: PHASE131_COMPOSITION_STATE.echo_reply_observation.status, log_ack_status: PHASE131_COMPOSITION_STATE.log_ack_observation.status, aggregate_reply_status: PHASE131_COMPOSITION_STATE.aggregate_reply_observation.status, composition_wait_status: PHASE131_COMPOSITION_STATE.wait_observation.status, aggregate_reply_byte0: PHASE131_COMPOSITION_STATE.observation.aggregate_reply_byte0, aggregate_reply_byte1: PHASE131_COMPOSITION_STATE.observation.aggregate_reply_byte1, aggregate_reply_byte2: PHASE131_COMPOSITION_STATE.observation.aggregate_reply_byte2, aggregate_reply_byte3: PHASE131_COMPOSITION_STATE.observation.aggregate_reply_byte3, explicit_composition_visible: 1, kernel_broker_visible: 0, dynamic_namespace_visible: 0, compiler_reopening_visible: 0 })
}

func build_composition_service_execution_state(gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, init_handle_table: capability.HandleTable, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, endpoints: ipc.EndpointTable, init_image: init.InitImage) bootstrap_services.CompositionServiceExecutionState {
    return bootstrap_services.CompositionServiceExecutionState{ program_capability: PHASE131_COMPOSITION_STATE.program_capability, gate: gate, process_slots: process_slots, task_slots: task_slots, init_handle_table: init_handle_table, child_handle_table: child_handle_table, wait_table: wait_table, endpoints: endpoints, init_image: init_image, child_address_space: PHASE131_COMPOSITION_STATE.child_address_space, child_user_frame: PHASE131_COMPOSITION_STATE.child_user_frame, echo_peer_state: PHASE131_COMPOSITION_STATE.echo_peer_state, log_peer_state: PHASE131_COMPOSITION_STATE.log_peer_state, spawn_observation: PHASE131_COMPOSITION_STATE.spawn_observation, request_receive_observation: PHASE131_COMPOSITION_STATE.request_receive_observation, echo_fanout_observation: PHASE131_COMPOSITION_STATE.echo_fanout_observation, echo_peer_receive_observation: PHASE131_COMPOSITION_STATE.echo_peer_receive_observation, echo_reply_send_observation: PHASE131_COMPOSITION_STATE.echo_reply_send_observation, echo_reply_observation: PHASE131_COMPOSITION_STATE.echo_reply_observation, log_fanout_observation: PHASE131_COMPOSITION_STATE.log_fanout_observation, log_peer_receive_observation: PHASE131_COMPOSITION_STATE.log_peer_receive_observation, log_ack_send_observation: PHASE131_COMPOSITION_STATE.log_ack_send_observation, log_ack_observation: PHASE131_COMPOSITION_STATE.log_ack_observation, aggregate_reply_send_observation: PHASE131_COMPOSITION_STATE.aggregate_reply_send_observation, aggregate_reply_observation: PHASE131_COMPOSITION_STATE.aggregate_reply_observation, wait_observation: PHASE131_COMPOSITION_STATE.wait_observation, observation: PHASE131_COMPOSITION_STATE.observation, ready_queue: PHASE131_COMPOSITION_STATE.ready_queue }
}

func install_composition_service_execution_state(next_state: bootstrap_services.CompositionServiceExecutionState) {
    PHASE131_COMPOSITION_STATE = next_state
}

func phase131_composition_probe_succeeded() bool {
    if PHASE131_COMPOSITION_STATE.observation.outbound_edge_count != 2 {
        return false
    }
    if PHASE131_COMPOSITION_STATE.observation.aggregate_reply_count != 1 {
        return false
    }
    return PHASE131_COMPOSITION_STATE.observation.aggregate_reply_byte3 == 2
}

func execute_phase124_delegation_chain_probe(context: LatePhaseProofContext, transfer_config: bootstrap_services.TransferServiceConfig) bool {
    local_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    local_endpoints: ipc.EndpointTable = ipc.empty_table()
    local_endpoints = ipc.install_endpoint(local_endpoints, context.init_pid, context.init_endpoint_id)
    local_endpoints = ipc.install_endpoint(local_endpoints, context.init_pid, context.transfer_endpoint_id)

    init_table: capability.HandleTable = capability.handle_table_for_owner(context.init_pid)
    init_table = capability.install_endpoint_handle(init_table, context.phase124_control_handle_slot, context.init_endpoint_id, 3)
    init_table = capability.install_endpoint_handle(init_table, transfer_config.init_received_handle_slot, context.transfer_endpoint_id, 5)

    intermediary_table: capability.HandleTable = capability.handle_table_for_owner(context.phase124_intermediary_pid)
    intermediary_table = capability.install_endpoint_handle(intermediary_table, context.phase124_control_handle_slot, context.init_endpoint_id, 3)

    final_table: capability.HandleTable = capability.handle_table_for_owner(context.phase124_final_holder_pid)
    final_table = capability.install_endpoint_handle(final_table, context.phase124_control_handle_slot, context.init_endpoint_id, 3)

    grant_payload: [4]u8 = ipc.zero_payload()
    grant_payload[0] = 67
    grant_payload[1] = 72
    grant_payload[2] = 65
    grant_payload[3] = 73
    first_send: syscall.SendResult = syscall.perform_send(local_gate, init_table, local_endpoints, context.init_pid, syscall.build_transfer_send_request(context.phase124_control_handle_slot, 4, grant_payload, transfer_config.init_received_handle_slot))
    init_table = first_send.handle_table
    local_endpoints = first_send.endpoints
    local_gate = first_send.gate
    if syscall.status_score(first_send.status) != 2 {
        return false
    }

    first_receive: syscall.ReceiveResult = syscall.perform_receive(local_gate, intermediary_table, local_endpoints, syscall.build_transfer_receive_request(context.phase124_control_handle_slot, context.phase124_intermediary_receive_handle_slot))
    intermediary_table = first_receive.handle_table
    local_endpoints = first_receive.endpoints
    local_gate = first_receive.gate
    if syscall.status_score(first_receive.observation.status) != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(intermediary_table, context.phase124_intermediary_receive_handle_slot) != context.transfer_endpoint_id {
        return false
    }

    init_invalidated_send: syscall.SendResult = syscall.perform_send(local_gate, init_table, local_endpoints, context.init_pid, syscall.build_send_request(transfer_config.init_received_handle_slot, 4, grant_payload))
    PHASE124_DELEGATOR_INVALIDATED_SEND_STATUS = init_invalidated_send.status
    if syscall.status_score(PHASE124_DELEGATOR_INVALIDATED_SEND_STATUS) != 8 {
        return false
    }

    second_send: syscall.SendResult = syscall.perform_send(local_gate, intermediary_table, local_endpoints, context.phase124_intermediary_pid, syscall.build_transfer_send_request(context.phase124_control_handle_slot, 4, grant_payload, context.phase124_intermediary_receive_handle_slot))
    intermediary_table = second_send.handle_table
    local_endpoints = second_send.endpoints
    local_gate = second_send.gate
    if syscall.status_score(second_send.status) != 2 {
        return false
    }

    second_receive: syscall.ReceiveResult = syscall.perform_receive(local_gate, final_table, local_endpoints, syscall.build_transfer_receive_request(context.phase124_control_handle_slot, context.phase124_final_receive_handle_slot))
    final_table = second_receive.handle_table
    local_endpoints = second_receive.endpoints
    local_gate = second_receive.gate
    if syscall.status_score(second_receive.observation.status) != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(final_table, context.phase124_final_receive_handle_slot) != context.transfer_endpoint_id {
        return false
    }

    intermediary_invalidated_send: syscall.SendResult = syscall.perform_send(local_gate, intermediary_table, local_endpoints, context.phase124_intermediary_pid, syscall.build_send_request(context.phase124_intermediary_receive_handle_slot, 4, grant_payload))
    PHASE124_INTERMEDIARY_INVALIDATED_SEND_STATUS = intermediary_invalidated_send.status
    if syscall.status_score(PHASE124_INTERMEDIARY_INVALIDATED_SEND_STATUS) != 8 {
        return false
    }

    final_payload: [4]u8 = ipc.zero_payload()
    final_payload[0] = 72
    final_payload[1] = 79
    final_payload[2] = 80
    final_payload[3] = 50
    final_send: syscall.SendResult = syscall.perform_send(local_gate, final_table, local_endpoints, context.phase124_final_holder_pid, syscall.build_send_request(context.phase124_final_receive_handle_slot, 4, final_payload))
    PHASE124_FINAL_SEND_STATUS = final_send.status
    PHASE124_FINAL_SEND_SOURCE_PID = final_send.endpoints.slots[1].messages[0].source_pid
    PHASE124_FINAL_ENDPOINT_QUEUE_DEPTH = final_send.endpoints.slots[1].queued_messages
    if syscall.status_score(PHASE124_FINAL_SEND_STATUS) != 2 {
        return false
    }
    return PHASE124_FINAL_ENDPOINT_QUEUE_DEPTH == 1
}

func execute_phase125_invalidation_probe(context: LatePhaseProofContext, transfer_config: bootstrap_services.TransferServiceConfig) bool {
    local_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    local_endpoints: ipc.EndpointTable = ipc.empty_table()
    local_endpoints = ipc.install_endpoint(local_endpoints, context.init_pid, context.init_endpoint_id)
    local_endpoints = ipc.install_endpoint(local_endpoints, context.init_pid, context.transfer_endpoint_id)

    init_table: capability.HandleTable = capability.handle_table_for_owner(context.init_pid)
    init_table = capability.install_endpoint_handle(init_table, context.phase124_control_handle_slot, context.init_endpoint_id, 3)
    init_table = capability.install_endpoint_handle(init_table, transfer_config.init_received_handle_slot, context.transfer_endpoint_id, 5)

    intermediary_table: capability.HandleTable = capability.handle_table_for_owner(context.phase124_intermediary_pid)
    intermediary_table = capability.install_endpoint_handle(intermediary_table, context.phase124_control_handle_slot, context.init_endpoint_id, 3)

    final_table: capability.HandleTable = capability.handle_table_for_owner(context.phase124_final_holder_pid)
    final_table = capability.install_endpoint_handle(final_table, context.phase124_control_handle_slot, context.init_endpoint_id, 3)

    grant_payload: [4]u8 = ipc.zero_payload()
    grant_payload[0] = 67
    grant_payload[1] = 72
    grant_payload[2] = 65
    grant_payload[3] = 73

    first_send: syscall.SendResult = syscall.perform_send(local_gate, init_table, local_endpoints, context.init_pid, syscall.build_transfer_send_request(context.phase124_control_handle_slot, 4, grant_payload, transfer_config.init_received_handle_slot))
    init_table = first_send.handle_table
    local_endpoints = first_send.endpoints
    local_gate = first_send.gate
    if syscall.status_score(first_send.status) != 2 {
        return false
    }

    first_receive: syscall.ReceiveResult = syscall.perform_receive(local_gate, intermediary_table, local_endpoints, syscall.build_transfer_receive_request(context.phase124_control_handle_slot, context.phase124_intermediary_receive_handle_slot))
    intermediary_table = first_receive.handle_table
    local_endpoints = first_receive.endpoints
    local_gate = first_receive.gate
    if syscall.status_score(first_receive.observation.status) != 2 {
        return false
    }

    second_send: syscall.SendResult = syscall.perform_send(local_gate, intermediary_table, local_endpoints, context.phase124_intermediary_pid, syscall.build_transfer_send_request(context.phase124_control_handle_slot, 4, grant_payload, context.phase124_intermediary_receive_handle_slot))
    intermediary_table = second_send.handle_table
    local_endpoints = second_send.endpoints
    local_gate = second_send.gate
    if syscall.status_score(second_send.status) != 2 {
        return false
    }

    second_receive: syscall.ReceiveResult = syscall.perform_receive(local_gate, final_table, local_endpoints, syscall.build_transfer_receive_request(context.phase124_control_handle_slot, context.phase124_final_receive_handle_slot))
    final_table = second_receive.handle_table
    local_endpoints = second_receive.endpoints
    local_gate = second_receive.gate
    if syscall.status_score(second_receive.observation.status) != 2 {
        return false
    }

    invalidated_final_table: capability.HandleTable = capability.remove_handle(final_table, context.phase124_final_receive_handle_slot)
    if !capability.handle_remove_succeeded(final_table, invalidated_final_table, context.phase124_final_receive_handle_slot) {
        return false
    }
    final_table = invalidated_final_table

    rejected_send: syscall.SendResult = syscall.perform_send(local_gate, final_table, local_endpoints, context.phase124_final_holder_pid, syscall.build_send_request(context.phase124_final_receive_handle_slot, 4, grant_payload))
    PHASE125_REJECTED_SEND_STATUS = rejected_send.status
    if syscall.status_score(PHASE125_REJECTED_SEND_STATUS) != 8 {
        return false
    }

    rejected_receive: syscall.ReceiveResult = syscall.perform_receive(local_gate, final_table, local_endpoints, syscall.build_receive_request(context.phase124_final_receive_handle_slot))
    PHASE125_REJECTED_RECEIVE_STATUS = rejected_receive.observation.status
    if syscall.status_score(PHASE125_REJECTED_RECEIVE_STATUS) != 8 {
        return false
    }

    control_payload: [4]u8 = ipc.zero_payload()
    control_payload[0] = 76
    control_payload[1] = 79
    control_payload[2] = 83
    control_payload[3] = 84
    surviving_control_send: syscall.SendResult = syscall.perform_send(local_gate, final_table, local_endpoints, context.phase124_final_holder_pid, syscall.build_send_request(context.phase124_control_handle_slot, 4, control_payload))
    PHASE125_SURVIVING_CONTROL_SEND_STATUS = surviving_control_send.status
    PHASE125_SURVIVING_CONTROL_SOURCE_PID = surviving_control_send.endpoints.slots[0].messages[0].source_pid
    PHASE125_SURVIVING_CONTROL_QUEUE_DEPTH = surviving_control_send.endpoints.slots[0].queued_messages
    if syscall.status_score(PHASE125_SURVIVING_CONTROL_SEND_STATUS) != 2 {
        return false
    }
    return PHASE125_SURVIVING_CONTROL_QUEUE_DEPTH == 1
}

func execute_phase126_authority_lifetime_probe(context: LatePhaseProofContext, transfer_config: bootstrap_services.TransferServiceConfig) bool {
    if !execute_phase125_invalidation_probe(context, transfer_config) {
        return false
    }

    local_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    local_endpoints: ipc.EndpointTable = ipc.empty_table()
    local_endpoints = ipc.install_endpoint(local_endpoints, context.phase124_final_holder_pid, context.init_endpoint_id)

    final_table: capability.HandleTable = capability.handle_table_for_owner(context.phase124_final_holder_pid)
    final_table = capability.install_endpoint_handle(final_table, context.phase124_control_handle_slot, context.init_endpoint_id, 3)

    control_payload: [4]u8 = ipc.zero_payload()
    control_payload[0] = 76
    control_payload[1] = 79
    control_payload[2] = 83
    control_payload[3] = 84
    surviving_control_send: syscall.SendResult = syscall.perform_send(local_gate, final_table, local_endpoints, context.phase124_final_holder_pid, syscall.build_send_request(context.phase124_control_handle_slot, 4, control_payload))
    final_table = surviving_control_send.handle_table
    local_endpoints = surviving_control_send.endpoints
    local_gate = surviving_control_send.gate
    if syscall.status_score(surviving_control_send.status) != 2 {
        return false
    }
    if surviving_control_send.endpoints.slots[0].queued_messages != 1 {
        return false
    }

    repeat_control_send: syscall.SendResult = syscall.perform_send(local_gate, final_table, local_endpoints, context.phase124_final_holder_pid, syscall.build_send_request(context.phase124_control_handle_slot, 4, control_payload))
    PHASE126_REPEAT_LONG_LIVED_SEND_STATUS = repeat_control_send.status
    PHASE126_REPEAT_LONG_LIVED_SOURCE_PID = repeat_control_send.endpoints.slots[0].messages[1].source_pid
    PHASE126_REPEAT_LONG_LIVED_QUEUE_DEPTH = repeat_control_send.endpoints.slots[0].queued_messages
    if syscall.status_score(PHASE126_REPEAT_LONG_LIVED_SEND_STATUS) != 2 {
        return false
    }
    return PHASE126_REPEAT_LONG_LIVED_QUEUE_DEPTH == 2
}

func execute_phase128_service_death_observation_probe(context: LatePhaseProofContext, log_config: bootstrap_services.LogServiceConfig, log_spawn_observation: syscall.SpawnObservation, log_wait_observation: syscall.WaitObservation) bool {
    if syscall.status_score(log_wait_observation.status) != 2 {
        return false
    }
    PHASE128_OBSERVED_SERVICE_PID = log_wait_observation.child_pid
    PHASE128_OBSERVED_SERVICE_KEY = context.log_service_directory_key
    PHASE128_OBSERVED_WAIT_HANDLE_SLOT = log_wait_observation.wait_handle_slot
    PHASE128_OBSERVED_EXIT_CODE = log_wait_observation.exit_code
    if PHASE128_OBSERVED_SERVICE_PID != log_spawn_observation.child_pid {
        return false
    }
    if PHASE128_OBSERVED_WAIT_HANDLE_SLOT != log_config.wait_handle_slot {
        return false
    }
    return PHASE128_OBSERVED_EXIT_CODE == log_config.exit_code
}

func execute_phase129_partial_failure_propagation_probe(log_config: bootstrap_services.LogServiceConfig, log_execution_state: bootstrap_services.LogServiceExecutionState, echo_config: bootstrap_services.EchoServiceConfig) bool {
    log_result: bootstrap_services.LogServiceExecutionResult = bootstrap_services.execute_phase105_log_service_handshake(log_config, log_execution_state)
    if log_result.succeeded == 0 {
        return false
    }
    PHASE129_FAILED_SERVICE_PID = log_result.state.wait_observation.child_pid
    PHASE129_FAILED_WAIT_STATUS = log_result.state.wait_observation.status
    if PHASE129_FAILED_SERVICE_PID == 0 {
        return false
    }
    if syscall.status_score(PHASE129_FAILED_WAIT_STATUS) != 2 {
        return false
    }

    echo_execution: bootstrap_services.EchoServiceExecutionState = bootstrap_services.EchoServiceExecutionState{ program_capability: capability.empty_slot(), gate: log_result.state.gate, process_slots: log_result.state.process_slots, task_slots: log_result.state.task_slots, init_handle_table: log_result.state.init_handle_table, child_handle_table: log_result.state.child_handle_table, wait_table: log_result.state.wait_table, endpoints: log_result.state.endpoints, init_image: log_result.state.init_image, child_address_space: address_space.empty_space(), child_user_frame: address_space.empty_frame(), service_state: echo_service.service_state(0, 0), spawn_observation: syscall.empty_spawn_observation(), receive_observation: syscall.empty_receive_observation(), reply_observation: syscall.empty_receive_observation(), exchange: echo_service.EchoExchangeObservation{ service_pid: 0, client_pid: 0, endpoint_id: 0, tag: echo_service.EchoMessageTag.None, request_len: 0, request_byte0: 0, request_byte1: 0, reply_len: 0, reply_byte0: 0, reply_byte1: 0, request_count: 0, reply_count: 0 }, wait_observation: syscall.empty_wait_observation(), ready_queue: log_result.state.ready_queue }
    echo_result: bootstrap_services.EchoServiceExecutionResult = bootstrap_services.execute_phase106_echo_service_request_reply(echo_config, echo_execution)
    if echo_result.succeeded == 0 {
        return false
    }
    PHASE129_SURVIVING_SERVICE_PID = echo_result.state.wait_observation.child_pid
    PHASE129_SURVIVING_REPLY_STATUS = echo_result.state.reply_observation.status
    PHASE129_SURVIVING_WAIT_STATUS = echo_result.state.wait_observation.status
    PHASE129_SURVIVING_REPLY_BYTE0 = echo_result.state.reply_observation.payload[0]
    PHASE129_SURVIVING_REPLY_BYTE1 = echo_result.state.reply_observation.payload[1]
    if PHASE129_SURVIVING_SERVICE_PID == 0 {
        return false
    }
    if syscall.status_score(PHASE129_SURVIVING_REPLY_STATUS) != 2 {
        return false
    }
    if syscall.status_score(PHASE129_SURVIVING_WAIT_STATUS) != 2 {
        return false
    }
    if PHASE129_SURVIVING_REPLY_BYTE0 != echo_config.request_byte0 {
        return false
    }
    return PHASE129_SURVIVING_REPLY_BYTE1 == echo_config.request_byte1
}

func execute_phase130_explicit_restart_or_replacement_probe(log_config: bootstrap_services.LogServiceConfig, log_execution_state: bootstrap_services.LogServiceExecutionState) bool {
    initial_result: bootstrap_services.LogServiceExecutionResult = bootstrap_services.execute_phase105_log_service_handshake(log_config, log_execution_state)
    if initial_result.succeeded == 0 {
        return false
    }
    if syscall.status_score(initial_result.state.wait_observation.status) != 2 {
        return false
    }

    replacement_result: bootstrap_services.LogServiceExecutionResult = bootstrap_services.execute_phase105_log_service_handshake(log_config, initial_result.state)
    if replacement_result.succeeded == 0 {
        return false
    }
    PHASE130_REPLACEMENT_SERVICE_PID = replacement_result.state.wait_observation.child_pid
    PHASE130_REPLACEMENT_SPAWN_STATUS = replacement_result.state.spawn_observation.status
    PHASE130_REPLACEMENT_ACK_STATUS = replacement_result.state.ack_observation.status
    PHASE130_REPLACEMENT_WAIT_STATUS = replacement_result.state.wait_observation.status
    PHASE130_REPLACEMENT_ACK_BYTE = replacement_result.state.ack_observation.payload[0]
    PHASE130_REPLACEMENT_PROGRAM_OBJECT_ID = log_config.program_object_id
    if PHASE130_REPLACEMENT_SERVICE_PID == 0 {
        return false
    }
    if syscall.status_score(PHASE130_REPLACEMENT_SPAWN_STATUS) != 2 {
        return false
    }
    if syscall.status_score(PHASE130_REPLACEMENT_ACK_STATUS) != 2 {
        return false
    }
    if syscall.status_score(PHASE130_REPLACEMENT_WAIT_STATUS) != 2 {
        return false
    }
    return PHASE130_REPLACEMENT_ACK_BYTE == log_service.ack_payload()[0]
}