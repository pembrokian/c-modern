import address_space
import capability
import echo_service
import init
import interrupt
import kv_service
import log_service
import shell_service
import serial_service
import state
import syscall
import timer
import transfer_service
import uart

struct Phase108ProgramCapContract {
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

struct RunningKernelSliceAudit {
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

struct Phase117MultiServiceBringUpAudit {
    running_slice: RunningKernelSliceAudit
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

struct Phase118DelegatedRequestReplyAudit {
    phase117: Phase117MultiServiceBringUpAudit
    transfer_service_transfer: transfer_service.TransferObservation
    invalidated_source_send_status: syscall.SyscallStatus
    invalidated_source_handle_slot: u32
    retained_receive_handle_slot: u32
    retained_receive_endpoint_id: u32
}

struct Phase119NamespacePressureAudit {
    phase118: Phase118DelegatedRequestReplyAudit
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

struct Phase120RunningSystemSupportAudit {
    phase119: Phase119NamespacePressureAudit
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

struct Phase121KernelImageContractAudit {
    phase120: Phase120RunningSystemSupportAudit
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

struct Phase122TargetSurfaceAudit {
    phase121: Phase121KernelImageContractAudit
    kernel_target_visible: u32
    kernel_runtime_startup_visible: u32
    bootstrap_target_family_visible: u32
    bootstrap_target_family_only_visible: u32
    broader_target_family_visible: u32
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct Phase123NextPlateauAudit {
    phase122: Phase122TargetSurfaceAudit
    running_kernel_truth_visible: u32
    running_system_truth_visible: u32
    kernel_image_truth_visible: u32
    target_surface_truth_visible: u32
    broader_platform_visible: u32
    broad_target_support_visible: u32
    general_loading_visible: u32
    compiler_reopening_visible: u32
}

struct Phase124DelegationChainAudit {
    phase123: Phase123NextPlateauAudit
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

struct Phase125InvalidationAudit {
    phase124: Phase124DelegationChainAudit
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

struct Phase126AuthorityLifetimeAudit {
    phase125: Phase125InvalidationAudit
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

struct Phase128ServiceDeathObservationAudit {
    phase126: Phase126AuthorityLifetimeAudit
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

struct Phase129PartialFailurePropagationAudit {
    phase128: Phase128ServiceDeathObservationAudit
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

struct Phase130ExplicitRestartOrReplacementAudit {
    phase129: Phase129PartialFailurePropagationAudit
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

struct Phase131FanOutCompositionAudit {
    phase130: Phase130ExplicitRestartOrReplacementAudit
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

struct Phase132BackpressureAudit {
    phase131: Phase131FanOutCompositionAudit
    endpoint_id: u32
    sender_pid: u32
    receiver_pid: u32
    sender_tid: u32
    receiver_tid: u32
    sender_task_slot: u32
    receiver_task_slot: u32
    blocked_send_status: syscall.SyscallStatus
    blocked_send_reason: syscall.BlockReason
    blocked_send_task_state: state.TaskState
    blocked_send_queue_depth: usize
    blocked_send_waiter_task_slot: u32
    sender_wake_status: syscall.SyscallStatus
    sender_wake_task_state: state.TaskState
    sender_wake_reason: ipc.EndpointWakeReason
    sender_wake_task_id: u32
    sender_wake_ready_count: usize
    sender_wake_queue_depth: usize
    sender_wake_waiter_task_slot: u32
    blocked_receive_status: syscall.SyscallStatus
    blocked_receive_reason: syscall.BlockReason
    blocked_receive_task_state: state.TaskState
    blocked_receive_queue_depth: usize
    blocked_receive_waiter_task_slot: u32
    receiver_wake_status: syscall.SyscallStatus
    receiver_wake_task_state: state.TaskState
    receiver_wake_reason: ipc.EndpointWakeReason
    receiver_wake_task_id: u32
    receiver_wake_ready_count: usize
    receiver_wake_queue_depth: usize
    receiver_wake_waiter_task_slot: u32
    kernel_policy_visible: u32
    compiler_reopening_visible: u32
}

struct Phase133MessageLifetimeAudit {
    phase132: Phase132BackpressureAudit
    endpoint_id: u32
    attached_endpoint_id: u32
    first_send_status: syscall.SyscallStatus
    source_handle_live_after_first_send: u32
    first_receive_status: syscall.SyscallStatus
    first_receive_byte0: u8
    first_receive_handle_slot: u32
    first_receive_handle_count: usize
    first_received_handle_endpoint_id: u32
    first_retired_slot_state: ipc.MessageState
    first_retired_slot_payload0: u8
    second_send_status: syscall.SyscallStatus
    second_receive_status: syscall.SyscallStatus
    second_receive_byte0: u8
    second_retired_slot_state: ipc.MessageState
    second_retired_slot_payload0: u8
    abort_transfer_send_status: syscall.SyscallStatus
    abort_transfer_source_handle_live_after_send: u32
    abort_transfer_aborted_messages: usize
    abort_transfer_scrubbed_state: ipc.MessageState
    abort_transfer_scrubbed_payload0: u8
    abort_transfer_scrubbed_attached_count: usize
    abort_transfer_scrubbed_attached_endpoint_id: u32
    abort_transfer_scrubbed_source_handle_slot: u32
    close_blocked_send_status: syscall.SyscallStatus
    close_blocked_send_state: state.TaskState
    close_aborted_messages: usize
    close_wake_count: usize
    close_wake_reason: ipc.EndpointWakeReason
    close_wake_task_id: u32
    close_ready_count: usize
    close_task_state: state.TaskState
    closed_send_status: syscall.SyscallStatus
    closed_receive_status: syscall.SyscallStatus
    owner_death_closed_count: usize
    owner_death_aborted_messages: usize
    owner_death_wake_count: usize
    owner_death_wake_reason: ipc.EndpointWakeReason
    owner_death_transfer_source_handle_live_after_send: u32
    owner_death_transfer_scrubbed_state: ipc.MessageState
    owner_death_transfer_scrubbed_payload0: u8
    owner_death_transfer_scrubbed_attached_count: usize
    owner_death_transfer_scrubbed_attached_endpoint_id: u32
    owner_death_transfer_scrubbed_source_handle_slot: u32
    kernel_policy_visible: u32
    compiler_reopening_visible: u32
}

struct Phase134MinimalDeviceServiceHandoffAudit {
    phase131: Phase131FanOutCompositionAudit
    interrupt_vector: u32
    interrupt_source_actor: u32
    interrupt_kind: interrupt.InterruptDispatchKind
    dispatch_handled: u32
    uart_service_endpoint_id: u32
    uart_ack_count: u32
    uart_ingress_count: u32
    uart_received_byte: u8
    published_endpoint_id: u32
    published_source_pid: u32
    published_payload_len: usize
    published_payload_byte0: u8
    published_queued: u32
    published_queue_full: u32
    published_endpoint_valid: u32
    published_endpoint_closed: u32
    serial_service_pid: u32
    serial_receive_status: syscall.SyscallStatus
    serial_wait_status: syscall.SyscallStatus
    serial_received_byte: u8
    serial_ingress_count: usize
    kernel_policy_visible: u32
    driver_framework_visible: u32
    generic_event_bus_visible: u32
    compiler_reopening_visible: u32
}

struct Phase135BufferOwnershipAudit {
    phase131: Phase131FanOutCompositionAudit
    interrupt_vector: u32
    interrupt_source_actor: u32
    interrupt_kind: interrupt.InterruptDispatchKind
    first_dispatch_handled: u32
    second_dispatch_handled: u32
    uart_service_endpoint_id: u32
    first_uart_ack_count: u32
    first_uart_ingress_count: u32
    first_uart_retire_count: u32
    first_staged_payload_len: usize
    first_staged_payload_byte0: u8
    first_staged_payload_byte1: u8
    first_published_endpoint_id: u32
    first_published_source_pid: u32
    first_published_payload_len: usize
    first_published_payload_byte0: u8
    first_published_payload_byte1: u8
    first_published_queued: u32
    first_published_queue_full: u32
    first_published_endpoint_valid: u32
    first_published_endpoint_closed: u32
    first_retired_payload_len: usize
    first_retired_payload_byte0: u8
    first_retired_payload_byte1: u8
    second_uart_ack_count: u32
    second_uart_ingress_count: u32
    second_uart_retire_count: u32
    second_staged_payload_len: usize
    second_staged_payload_byte0: u8
    second_staged_payload_byte1: u8
    second_published_endpoint_id: u32
    second_published_source_pid: u32
    second_published_payload_len: usize
    second_published_payload_byte0: u8
    second_published_payload_byte1: u8
    second_published_queued: u32
    second_published_queue_full: u32
    second_published_endpoint_valid: u32
    second_published_endpoint_closed: u32
    second_retired_payload_len: usize
    second_retired_payload_byte0: u8
    second_retired_payload_byte1: u8
    serial_service_pid: u32
    serial_source_pid: u32
    serial_endpoint_id: u32
    serial_receive_status: syscall.SyscallStatus
    serial_wait_status: syscall.SyscallStatus
    serial_last_payload_len: usize
    serial_last_received_byte: u8
    serial_ingress_count: usize
    serial_log_len: usize
    serial_total_consumed_bytes: usize
    serial_log_byte0: u8
    serial_log_byte1: u8
    serial_log_byte2: u8
    serial_log_byte3: u8
    kernel_policy_visible: u32
    driver_framework_visible: u32
    generic_buffer_pool_visible: u32
    zero_copy_visible: u32
    compiler_reopening_visible: u32
}

struct Phase136DeviceFailureContainmentAudit {
    phase131: Phase131FanOutCompositionAudit
    uart_service_endpoint_id: u32
    malformed_interrupt_kind: interrupt.InterruptDispatchKind
    malformed_dispatch_handled: u32
    malformed_published_queued: u32
    malformed_published_queue_full: u32
    malformed_published_endpoint_valid: u32
    malformed_published_endpoint_closed: u32
    malformed_serial_service_pid: u32
    malformed_serial_tag: serial_service.SerialMessageTag
    malformed_receive_status: syscall.SyscallStatus
    malformed_payload_len: usize
    malformed_received_byte: u8
    malformed_ingress_count: usize
    malformed_service_malformed_count: usize
    malformed_log_len: usize
    malformed_total_consumed_bytes: usize
    queue_one_published_queued: u32
    queue_two_published_queued: u32
    queue_full_dispatch_handled: u32
    queue_full_published_queued: u32
    queue_full_published_queue_full: u32
    queue_full_published_endpoint_valid: u32
    queue_full_published_endpoint_closed: u32
    queue_full_drop_count: u32
    queue_full_failure_kind: uart.UartFailureKind
    uart_ack_count_after_queue_full: u32
    uart_ingress_count_after_queue_full: u32
    uart_retire_count_after_queue_full: u32
    close_endpoint_id: u32
    close_closed: u32
    close_aborted_messages: usize
    close_wake_count: usize
    close_wait_status: syscall.SyscallStatus
    closed_dispatch_handled: u32
    closed_published_queued: u32
    closed_published_queue_full: u32
    closed_published_endpoint_valid: u32
    closed_published_endpoint_closed: u32
    closed_drop_count: u32
    closed_endpoint_closed_drop_count: u32
    closed_failure_kind: uart.UartFailureKind
    kernel_policy_visible: u32
    driver_framework_visible: u32
    retry_framework_visible: u32
    protocol_parsing_in_kernel_visible: u32
    compiler_reopening_visible: u32
}

struct Phase137OptionalDmaOrEquivalentAudit {
    phase136: Phase136DeviceFailureContainmentAudit
    completion_interrupt_kind: interrupt.InterruptDispatchKind
    completion_dispatch_handled: u32
    completion_endpoint_id: u32
    completion_staged_payload_len: usize
    completion_staged_payload0: u8
    completion_staged_payload1: u8
    completion_staged_payload2: u8
    completion_staged_payload3: u8
    completion_published_payload_len: usize
    completion_published_payload0: u8
    completion_published_payload1: u8
    completion_published_payload2: u8
    completion_published_payload3: u8
    completion_retired_payload_len: usize
    completion_retired_payload0: u8
    completion_retired_payload1: u8
    completion_retired_payload2: u8
    completion_retired_payload3: u8
    completion_publish_queued: u32
    completion_publish_queue_full: u32
    completion_publish_endpoint_valid: u32
    completion_publish_endpoint_closed: u32
    completion_ingress_count: u32
    completion_retire_count: u32
    serial_service_pid: u32
    serial_receive_status: syscall.SyscallStatus
    serial_tag: serial_service.SerialMessageTag
    serial_payload_len: usize
    serial_received_byte: u8
    serial_ingress_count: usize
    serial_log_len: usize
    serial_total_consumed_bytes: usize
    serial_log_byte0: u8
    serial_log_byte1: u8
    serial_log_byte2: u8
    serial_log_byte3: u8
    dma_manager_visible: u32
    descriptor_framework_visible: u32
    compiler_reopening_visible: u32
}

struct Phase140SerialIngressComposedServiceGraphAudit {
    phase137: Phase137OptionalDmaOrEquivalentAudit
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

struct Phase141InteractiveServiceSystemScopeFreezeAudit {
    phase140: Phase140SerialIngressComposedServiceGraphAudit
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

struct Phase142SerialShellCommandRoutingAudit {
    phase141: Phase141InteractiveServiceSystemScopeFreezeAudit
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

struct Phase143LongLivedLogServiceAudit {
    phase142: Phase142SerialShellCommandRoutingAudit
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

struct Phase144StatefulKeyValueServiceFollowThroughAudit {
    phase143: Phase143LongLivedLogServiceAudit
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

struct Phase145ServiceRestartFailureAndUsagePressureAudit {
    phase144: Phase144StatefulKeyValueServiceFollowThroughAudit
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
