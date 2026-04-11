import address_space
import capability
import debug
import echo_service
import endpoint
import init
import log_service
import mmu
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

