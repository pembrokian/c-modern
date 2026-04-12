import address_space
import capability
import debug
import echo_service
import ipc
import init
import kv_service
import log_service
import serial_service
import mmu
import shell_service
import state
import syscall
import timer
import transfer_service

struct LogServicePhaseAudit {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    ack_observation: syscall.ReceiveObservation
    service_state: log_service.LogServiceState
    handshake: log_service.LogHandshakeObservation
    wait_observation: syscall.WaitObservation
    wait_table: capability.WaitTable
    child_handle_table: capability.HandleTable
    ready_queue: state.ReadyQueue
    child_process: state.ProcessSlot
    child_task: state.TaskSlot
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    init_endpoint_id: u32
    wait_handle_slot: u32
    endpoint_handle_slot: u32
    request_byte: u8
    exit_code: i32
}

struct EchoServicePhaseAudit {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    reply_observation: syscall.ReceiveObservation
    service_state: echo_service.EchoServiceState
    exchange: echo_service.EchoExchangeObservation
    wait_observation: syscall.WaitObservation
    wait_table: capability.WaitTable
    child_handle_table: capability.HandleTable
    ready_queue: state.ReadyQueue
    child_process: state.ProcessSlot
    child_task: state.TaskSlot
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    init_endpoint_id: u32
    wait_handle_slot: u32
    endpoint_handle_slot: u32
    request_byte0: u8
    request_byte1: u8
    exit_code: i32
}

struct TransferServicePhaseAudit {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    spawn_observation: syscall.SpawnObservation
    grant_observation: syscall.ReceiveObservation
    emit_observation: syscall.ReceiveObservation
    service_state: transfer_service.TransferServiceState
    transfer: transfer_service.TransferObservation
    wait_observation: syscall.WaitObservation
    init_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    child_handle_table: capability.HandleTable
    ready_queue: state.ReadyQueue
    child_process: state.ProcessSlot
    child_task: state.TaskSlot
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    init_endpoint_id: u32
    transfer_endpoint_id: u32
    wait_handle_slot: u32
    source_handle_slot: u32
    control_handle_slot: u32
    init_received_handle_slot: u32
    service_received_handle_slot: u32
    grant_byte0: u8
    grant_byte1: u8
    grant_byte2: u8
    grant_byte3: u8
    exit_code: i32
}

struct Phase108ProgramCapContractInputs {
    init_pid: u32
    log_service_program_object_id: u32
    echo_service_program_object_id: u32
    transfer_service_program_object_id: u32
    log_service_wait_handle_slot: u32
    echo_service_wait_handle_slot: u32
    transfer_service_wait_handle_slot: u32
    log_service_exit_code: i32
    echo_service_exit_code: i32
    transfer_service_exit_code: i32
    bootstrap_program_capability: capability.CapabilitySlot
    log_service_program_capability: capability.CapabilitySlot
    echo_service_program_capability: capability.CapabilitySlot
    transfer_service_program_capability: capability.CapabilitySlot
    log_service_spawn: syscall.SpawnObservation
    echo_service_spawn: syscall.SpawnObservation
    transfer_service_spawn: syscall.SpawnObservation
    log_service_wait: syscall.WaitObservation
    echo_service_wait: syscall.WaitObservation
    transfer_service_wait: syscall.WaitObservation
}

struct RunningKernelSliceAuditInputs {
    kernel: state.KernelDescriptor
    init_pid: u32
    init_tid: u32
    init_asid: u32
    child_tid: u32
    child_exit_code: i32
    transfer_endpoint_id: u32
    log_service_request_byte: u8
    echo_service_request_byte0: u8
    echo_service_request_byte1: u8
    log_service_exit_code: i32
    echo_service_exit_code: i32
    transfer_service_exit_code: i32
    init_bootstrap_handoff: init.BootstrapHandoffObservation
    receive_observation: syscall.ReceiveObservation
    attached_receive_observation: syscall.ReceiveObservation
    transferred_handle_use_observation: syscall.ReceiveObservation
    pre_exit_wait_observation: syscall.WaitObservation
    exit_wait_observation: syscall.WaitObservation
    sleep_observation: syscall.SleepObservation
    timer_wake_observation: timer.TimerWakeObservation
    log_service_handshake: log_service.LogHandshakeObservation
    log_service_wait_observation: syscall.WaitObservation
    echo_service_exchange: echo_service.EchoExchangeObservation
    echo_service_wait_observation: syscall.WaitObservation
    transfer_service_transfer: transfer_service.TransferObservation
    transfer_service_wait_observation: syscall.WaitObservation
    phase104_contract_hardened: u32
    phase108_contract_hardened: u32
    init_process: state.ProcessSlot
    init_task: state.TaskSlot
    init_user_frame: address_space.UserEntryFrame
    boot_log_append_failed: u32
}

struct Phase117MultiServiceBringUpAuditInputs {
    running_slice: debug.RunningKernelSliceAudit
    init_endpoint_id: u32
    transfer_endpoint_id: u32
    log_service_program_capability: capability.CapabilitySlot
    echo_service_program_capability: capability.CapabilitySlot
    transfer_service_program_capability: capability.CapabilitySlot
    log_service_spawn: syscall.SpawnObservation
    echo_service_spawn: syscall.SpawnObservation
    transfer_service_spawn: syscall.SpawnObservation
    log_service_wait: syscall.WaitObservation
    echo_service_wait: syscall.WaitObservation
    transfer_service_wait: syscall.WaitObservation
    log_service_handshake: log_service.LogHandshakeObservation
    echo_service_exchange: echo_service.EchoExchangeObservation
    transfer_service_transfer: transfer_service.TransferObservation
}

struct Phase118DelegatedRequestReplyAuditInputs {
    phase117: debug.Phase117MultiServiceBringUpAudit
    transfer_service_transfer: transfer_service.TransferObservation
    invalidated_source_send_status: syscall.SyscallStatus
    invalidated_source_handle_slot: u32
    retained_receive_handle_slot: u32
    retained_receive_endpoint_id: u32
}

struct Phase119NamespacePressureAuditInputs {
    phase118: debug.Phase118DelegatedRequestReplyAudit
    directory_owner_pid: u32
    directory_entry_count: usize
    log_service_key: u32
    echo_service_key: u32
    transfer_service_key: u32
    shared_directory_endpoint_id: u32
    log_service_program_slot: u32
    echo_service_program_slot: u32
    transfer_service_program_slot: u32
    log_service_program_object_id: u32
    echo_service_program_object_id: u32
    transfer_service_program_object_id: u32
    log_service_wait_handle_slot: u32
    echo_service_wait_handle_slot: u32
    transfer_service_wait_handle_slot: u32
    dynamic_namespace_visible: u32
}

struct Phase120RunningSystemSupportAuditInputs {
    phase119: debug.Phase119NamespacePressureAudit
    service_policy_owner_pid: u32
    running_service_count: usize
    fixed_directory_count: usize
    shared_control_endpoint_id: u32
    retained_reply_endpoint_id: u32
    program_capability_count: usize
    wait_handle_count: usize
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct Phase121KernelImageContractAuditInputs {
    phase120: debug.Phase120RunningSystemSupportAudit
    kernel_manifest_visible: u32
    kernel_target_visible: u32
    kernel_runtime_startup_visible: u32
    bootstrap_target_family_visible: u32
    emitted_image_input_visible: u32
    linked_kernel_executable_visible: u32
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct Phase122TargetSurfaceAuditInputs {
    phase121: debug.Phase121KernelImageContractAudit
    kernel_target_visible: u32
    kernel_runtime_startup_visible: u32
    bootstrap_target_family_visible: u32
    bootstrap_target_family_only_visible: u32
    broader_target_family_visible: u32
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct Phase123NextPlateauAuditInputs {
    phase122: debug.Phase122TargetSurfaceAudit
    running_kernel_truth_visible: u32
    running_system_truth_visible: u32
    kernel_image_truth_visible: u32
    target_surface_truth_visible: u32
    broader_platform_visible: u32
    broad_target_support_visible: u32
    general_loading_visible: u32
    compiler_reopening_visible: u32
}

struct Phase124DelegationChainAuditInputs {
    phase123: debug.Phase123NextPlateauAudit
    delegator_pid: u32
    intermediary_pid: u32
    final_holder_pid: u32
    control_endpoint_id: u32
    delegated_endpoint_id: u32
    delegator_source_handle_slot: u32
    intermediary_receive_handle_slot: u32
    final_receive_handle_slot: u32
    first_invalidated_send_status: syscall.SyscallStatus
    second_invalidated_send_status: syscall.SyscallStatus
    final_send_status: syscall.SyscallStatus
    final_send_source_pid: u32
    final_endpoint_queue_depth: usize
    ambient_authority_visible: u32
    compiler_reopening_visible: u32
}

struct Phase125InvalidationAuditInputs {
    phase124: debug.Phase124DelegationChainAudit
    invalidated_holder_pid: u32
    control_endpoint_id: u32
    invalidated_endpoint_id: u32
    invalidated_handle_slot: u32
    rejected_send_status: syscall.SyscallStatus
    rejected_receive_status: syscall.SyscallStatus
    surviving_control_send_status: syscall.SyscallStatus
    surviving_control_source_pid: u32
    surviving_control_queue_depth: usize
    authority_loss_visible: u32
    broader_revocation_visible: u32
    compiler_reopening_visible: u32
}

struct Phase126AuthorityLifetimeAuditInputs {
    phase125: debug.Phase125InvalidationAudit
    classified_holder_pid: u32
    long_lived_endpoint_id: u32
    short_lived_endpoint_id: u32
    long_lived_handle_slot: u32
    short_lived_handle_slot: u32
    repeat_long_lived_send_status: syscall.SyscallStatus
    repeat_long_lived_source_pid: u32
    repeat_long_lived_queue_depth: usize
    long_lived_class_visible: u32
    short_lived_class_visible: u32
    broader_lifetime_framework_visible: u32
    compiler_reopening_visible: u32
}

struct Phase128ServiceDeathObservationAuditInputs {
    phase126: debug.Phase126AuthorityLifetimeAudit
    observed_service_pid: u32
    observed_service_key: u32
    observed_wait_handle_slot: u32
    observed_exit_code: i32
    fixed_directory_entry_count: usize
    service_death_visible: u32
    kernel_supervision_visible: u32
    service_restart_visible: u32
    broader_failure_framework_visible: u32
    compiler_reopening_visible: u32
}

struct Phase129PartialFailurePropagationAuditInputs {
    phase128: debug.Phase128ServiceDeathObservationAudit
    failed_service_pid: u32
    failed_service_key: u32
    failed_wait_handle_slot: u32
    failed_wait_status: syscall.SyscallStatus
    surviving_service_pid: u32
    surviving_service_key: u32
    surviving_wait_handle_slot: u32
    surviving_reply_status: syscall.SyscallStatus
    surviving_wait_status: syscall.SyscallStatus
    surviving_reply_byte0: u8
    surviving_reply_byte1: u8
    shared_control_endpoint_id: u32
    directory_entry_count: usize
    partial_failure_visible: u32
    kernel_recovery_visible: u32
    service_rebinding_visible: u32
    broader_failure_framework_visible: u32
    compiler_reopening_visible: u32
}

struct Phase130ExplicitRestartOrReplacementAuditInputs {
    phase129: debug.Phase129PartialFailurePropagationAudit
    replacement_policy_owner_pid: u32
    replacement_service_pid: u32
    replacement_service_key: u32
    replacement_wait_handle_slot: u32
    replacement_program_slot: u32
    replacement_program_object_id: u32
    replacement_spawn_status: syscall.SyscallStatus
    replacement_ack_status: syscall.SyscallStatus
    replacement_wait_status: syscall.SyscallStatus
    replacement_ack_byte: u8
    shared_control_endpoint_id: u32
    directory_entry_count: usize
    explicit_restart_or_replacement_visible: u32
    kernel_supervision_visible: u32
    service_rebinding_visible: u32
    broader_failure_framework_visible: u32
    compiler_reopening_visible: u32
}

struct Phase131FanOutCompositionAuditInputs {
    phase130: debug.Phase130ExplicitRestartOrReplacementAudit
    composition_policy_owner_pid: u32
    composition_service_pid: u32
    composition_service_key: u32
    composition_wait_handle_slot: u32
    fixed_directory_entry_count: usize
    control_endpoint_id: u32
    echo_endpoint_id: u32
    log_endpoint_id: u32
    request_receive_status: syscall.SyscallStatus
    echo_fanout_status: syscall.SyscallStatus
    echo_fanout_endpoint_id: u32
    log_fanout_status: syscall.SyscallStatus
    log_fanout_endpoint_id: u32
    echo_reply_status: syscall.SyscallStatus
    log_ack_status: syscall.SyscallStatus
    aggregate_reply_status: syscall.SyscallStatus
    composition_wait_status: syscall.SyscallStatus
    aggregate_reply_byte0: u8
    aggregate_reply_byte1: u8
    aggregate_reply_byte2: u8
    aggregate_reply_byte3: u8
    explicit_composition_visible: u32
    kernel_broker_visible: u32
    dynamic_namespace_visible: u32
    compiler_reopening_visible: u32
}

struct Phase140SerialIngressComposedServiceGraphAuditInputs {
    phase137: debug.Phase137OptionalDmaOrEquivalentAudit
    serial_service_pid: u32
    composition_service_pid: u32
    serial_forward_endpoint_id: u32
    serial_forward_status: syscall.SyscallStatus
    serial_forward_request_len: usize
    serial_forward_request_byte0: u8
    serial_forward_request_byte1: u8
    serial_forward_count: usize
    composition_request_receive_status: syscall.SyscallStatus
    composition_request_source_pid: u32
    composition_request_len: usize
    composition_request_byte0: u8
    composition_request_byte1: u8
    composition_control_endpoint_id: u32
    composition_echo_endpoint_id: u32
    composition_log_endpoint_id: u32
    composition_echo_fanout_status: syscall.SyscallStatus
    composition_echo_fanout_endpoint_id: u32
    composition_log_fanout_status: syscall.SyscallStatus
    composition_log_fanout_endpoint_id: u32
    composition_aggregate_reply_status: syscall.SyscallStatus
    composition_outbound_edge_count: usize
    composition_aggregate_reply_count: usize
    serial_aggregate_reply_status: syscall.SyscallStatus
    serial_aggregate_reply_len: usize
    serial_aggregate_reply_byte0: u8
    serial_aggregate_reply_byte1: u8
    serial_aggregate_reply_byte2: u8
    serial_aggregate_reply_byte3: u8
    serial_aggregate_reply_count: usize
    kernel_broker_visible: u32
    dynamic_routing_visible: u32
    general_service_graph_visible: u32
    compiler_reopening_visible: u32
}

struct Phase141InteractiveServiceSystemScopeFreezeAuditInputs {
    phase140: debug.Phase140SerialIngressComposedServiceGraphAudit
    serial_service_pid: u32
    shell_service_pid: u32
    kv_service_pid: u32
    serial_forward_endpoint_id: u32
    serial_forward_status: syscall.SyscallStatus
    serial_forward_request_len: usize
    serial_forward_request_byte0: u8
    serial_forward_request_byte1: u8
    shell_tag: shell_service.ShellCommandTag
    shell_request_len: usize
    shell_request_byte0: u8
    shell_request_byte1: u8
    shell_request_byte2: u8
    shell_endpoint_id: u32
    echo_endpoint_id: u32
    log_endpoint_id: u32
    kv_endpoint_id: u32
    echo_route_count: usize
    log_route_count: usize
    kv_route_count: usize
    invalid_command_count: usize
    shell_reply_status: syscall.SyscallStatus
    shell_reply_len: usize
    shell_reply_byte0: u8
    shell_reply_byte1: u8
    shell_reply_byte2: u8
    shell_reply_byte3: u8
    fixed_vocabulary_visible: u32
    kernel_broker_visible: u32
    dynamic_routing_visible: u32
    general_shell_framework_visible: u32
    compiler_reopening_visible: u32
}

struct Phase142SerialShellCommandRoutingAuditInputs {
    phase141: debug.Phase141InteractiveServiceSystemScopeFreezeAudit
    serial_service_pid: u32
    shell_service_pid: u32
    kv_service_pid: u32
    serial_forward_endpoint_id: u32
    serial_forward_status: syscall.SyscallStatus
    serial_forward_count: usize
    serial_final_reply_status: syscall.SyscallStatus
    serial_final_reply_len: usize
    serial_final_reply_byte0: u8
    serial_final_reply_count: usize
    echo_route: shell_service.ShellRoutingObservation
    log_append_route: shell_service.ShellRoutingObservation
    log_tail_route: shell_service.ShellRoutingObservation
    kv_set_route: shell_service.ShellRoutingObservation
    kv_get_route: shell_service.ShellRoutingObservation
    invalid_route: shell_service.ShellRoutingObservation
    compact_command_encoding_visible: u32
    malformed_command_visible: u32
    general_shell_framework_visible: u32
    payload_width_reopened_visible: u32
    compiler_reopening_visible: u32
}

struct Phase143LongLivedLogServiceAuditInputs {
    phase142: debug.Phase142SerialShellCommandRoutingAudit
    log_service_pid: u32
    shell_service_pid: u32
    overflow_append_route: shell_service.ShellRoutingObservation
    tail_route: shell_service.ShellRoutingObservation
    retention: log_service.LogRetentionObservation
    bounded_retention_visible: u32
    explicit_overwrite_policy_visible: u32
    durable_persistence_visible: u32
    compiler_reopening_visible: u32
}

struct Phase144StatefulKeyValueServiceFollowThroughAuditInputs {
    phase143: debug.Phase143LongLivedLogServiceAudit
    kv_service_pid: u32
    shell_service_pid: u32
    log_service_pid: u32
    missing_get_route: shell_service.ShellRoutingObservation
    initial_set_route: shell_service.ShellRoutingObservation
    hit_get_route: shell_service.ShellRoutingObservation
    overwrite_set_route: shell_service.ShellRoutingObservation
    overwrite_get_route: shell_service.ShellRoutingObservation
    kv_retention: kv_service.KvRetentionObservation
    write_log_retention: log_service.LogRetentionObservation
    bounded_retention_visible: u32
    missing_key_visible: u32
    explicit_overwrite_visible: u32
    fixed_write_log_visible: u32
    durable_persistence_visible: u32
    compiler_reopening_visible: u32
}

struct Phase145ServiceRestartFailureAndUsagePressureAuditInputs {
    phase144: debug.Phase144StatefulKeyValueServiceFollowThroughAudit
    kv_service_pid: u32
    shell_service_pid: u32
    log_service_pid: u32
    init_policy_owner_pid: u32
    pre_failure_set_route: shell_service.ShellRoutingObservation
    pre_failure_get_route: shell_service.ShellRoutingObservation
    failed_get_route: shell_service.ShellRoutingObservation
    restarted_get_route: shell_service.ShellRoutingObservation
    pre_failure_retention: kv_service.KvRetentionObservation
    post_restart_retention: kv_service.KvRetentionObservation
    restart: init.ServiceRestartObservation
    event_log_retention: log_service.LogRetentionObservation
    shell_failure_visible: u32
    init_restart_visible: u32
    post_restart_state_reset_visible: u32
    manual_retry_visible: u32
    kernel_supervision_visible: u32
    durable_persistence_visible: u32
    compiler_reopening_visible: u32
}

struct Phase146ServiceShapeConsolidationAuditInputs {
    phase145: debug.Phase145ServiceRestartFailureAndUsagePressureAudit
    serial_service_pid: u32
    shell_service_pid: u32
    log_service_pid: u32
    kv_service_pid: u32
    init_policy_owner_pid: u32
    serial_forward: serial_service.SerialCompositionObservation
    shell_route: shell_service.ShellRoutingObservation
    log_retention: log_service.LogRetentionObservation
    kv_retention: kv_service.KvRetentionObservation
    restart: init.ServiceRestartObservation
    serial_endpoint_handle_slot: u32
    shell_endpoint_handle_slot: u32
    log_endpoint_handle_slot: u32
    kv_endpoint_handle_slot: u32
    fixed_boot_wiring_visible: u32
    service_local_request_reply_visible: u32
    shell_syntax_boundary_visible: u32
    service_semantic_boundary_visible: u32
    explicit_failure_exposure_visible: u32
    init_owned_restart_visible: u32
    intentional_shell_difference_visible: u32
    intentional_stateful_difference_visible: u32
    dynamic_service_framework_visible: u32
    dynamic_registration_visible: u32
    compiler_reopening_visible: u32
}

struct Phase147IpcShapeAuditInputs {
    phase146: debug.Phase146ServiceShapeConsolidationAudit
    serial_service_pid: u32
    shell_service_pid: u32
    log_service_pid: u32
    kv_service_pid: u32
    serial_observation: serial_service.SerialCompositionObservation
    log_append_route: shell_service.ShellRoutingObservation
    log_tail_route: shell_service.ShellRoutingObservation
    kv_set_route: shell_service.ShellRoutingObservation
    kv_get_route: shell_service.ShellRoutingObservation
    repeated_log_tail_route: shell_service.ShellRoutingObservation
    repeated_kv_get_route: shell_service.ShellRoutingObservation
    log_retention: log_service.LogRetentionObservation
    kv_retention: kv_service.KvRetentionObservation
    request_construction_sufficient_visible: u32
    reply_shape_service_local_visible: u32
    compact_encoding_sufficient_visible: u32
    service_to_service_observation_visible: u32
    generic_message_bus_visible: u32
    dynamic_payload_typing_visible: u32
    compiler_reopening_visible: u32
}

struct BootstrapLayoutAudit {
    init_image: init.InitImage
    init_root_page_table: usize
}

struct EndpointCapabilityAudit {
    init_pid: u32
    init_endpoint_id: u32
    transfer_endpoint_id: u32
    child_wait_handle_slot: u32
}

struct StateHardeningAudit {
    boot_tid: u32
    boot_pid: u32
    boot_entry_pc: usize
    boot_stack_top: usize
    child_tid: u32
    child_pid: u32
    child_asid: u32
    init_image: init.InitImage
    arch_actor: u32
}

struct SyscallHardeningAudit {
    init_pid: u32
    init_endpoint_handle_slot: u32
    init_endpoint_id: u32
    child_wait_handle_slot: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    child_root_page_table: usize
    boot_pid: u32
    boot_tid: u32
    boot_task_slot: u32
    boot_entry_pc: usize
    boot_stack_top: usize
    init_image: init.InitImage
    transfer_source_handle_slot: u32
}

struct Phase104HardeningAudit {
    boot_log_append_failed: u32
    timer_hardened: u32
    bootstrap_layout: BootstrapLayoutAudit
    endpoint_capability: EndpointCapabilityAudit
    state_hardening: StateHardeningAudit
    syscall_hardening: SyscallHardeningAudit
}

