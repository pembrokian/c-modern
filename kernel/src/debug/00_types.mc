import address_space
import capability
import echo_service
import init
import interrupt
import log_service
import state
import syscall
import timer
import transfer_service

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

