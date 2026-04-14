import address_space
import bootstrap_audit
import bootstrap_services
import bootstrap_state
import capability
import debug
import echo_service
import interrupt
import ipc
import init
import lifecycle
import log_service
import mmu
import sched
import state
import syscall
import timer
import transfer_service
import uart

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

struct ProofContractFlags {
    scheduler_contract_hardened: u32
    lifecycle_contract_hardened: u32
    capability_contract_hardened: u32
    ipc_contract_hardened: u32
    address_space_contract_hardened: u32
    interrupt_contract_hardened: u32
    timer_contract_hardened: u32
    barrier_contract_hardened: u32
}

struct Phase108To116ProofInputs {
    context: Phase108To116RuntimeContext
    contract_flags: ProofContractFlags
}

struct Phase108To116RuntimeContext {
    init_pid: u32
    init_tid: u32
    init_asid: u32
    child_tid: u32
    child_exit_code: i32
    transfer_endpoint_id: u32
    log_config: bootstrap_services.LogServiceConfig
    echo_config: bootstrap_services.EchoServiceConfig
    transfer_config: bootstrap_services.TransferServiceConfig
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
    kernel: state.KernelDescriptor
    init_bootstrap_handoff: init.BootstrapHandoffObservation
    receive_observation: syscall.ReceiveObservation
    attached_receive_observation: syscall.ReceiveObservation
    transferred_handle_use_observation: syscall.ReceiveObservation
    pre_exit_wait_observation: syscall.WaitObservation
    exit_wait_observation: syscall.WaitObservation
    sleep_observation: syscall.SleepObservation
    timer_wake_observation: timer.TimerWakeObservation
    log_service_handshake: log_service.LogHandshakeObservation
    echo_service_exchange: echo_service.EchoExchangeObservation
    transfer_service_transfer: transfer_service.TransferObservation
    phase104_contract_hardened: u32
    init_process: state.ProcessSlot
    init_task: state.TaskSlot
    init_user_frame: address_space.UserEntryFrame
    boot_log_append_failed: u32
    last_interrupt_kind: interrupt.InterruptDispatchKind
}

struct Phase117To123ProofInputs {
    running_slice_audit: debug.RunningKernelSliceAudit
    context: Phase117To123RuntimeContext
    phase118_probe_succeeded: bool
    contract_flags: ProofContractFlags
}

struct Phase117To123RuntimeContext {
    init_pid: u32
    init_endpoint_id: u32
    transfer_endpoint_id: u32
    log_service_directory_key: u32
    echo_service_directory_key: u32
    transfer_service_directory_key: u32
    log_config: bootstrap_services.LogServiceConfig
    echo_config: bootstrap_services.EchoServiceConfig
    transfer_config: bootstrap_services.TransferServiceConfig
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
    init_handle_table: capability.HandleTable
}

struct Phase131To137ProofInputs {
    context: Phase131To137RuntimeContext
    contract_flags: ProofContractFlags
}

struct Phase131To137RuntimeContext {
    context: LatePhaseProofContext
    composition_config: bootstrap_services.CompositionServiceConfig
    serial_config: bootstrap_services.SerialServiceConfig
    phase130_audit: debug.Phase130ExplicitRestartOrReplacementAudit
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    init_handle_table: capability.HandleTable
    child_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    endpoints: ipc.EndpointTable
    init_image: init.InitImage
    serial_execution_state: bootstrap_services.SerialServiceExecutionState
    interrupts: interrupt.InterruptController
    uart_device: uart.UartDevice
    boot_pid: u32
    uart_receive_vector: u32
    uart_completion_vector: u32
    uart_source_actor: u32
    uart_frames: bootstrap_state.UartProbeFrames
}

struct MidPhaseProofRuntimeContext {
    scheduler_lifecycle_audit: sched.LifecycleAudit
    late_phase_context: LatePhaseProofContext
    init_pid: u32
    init_tid: u32
    init_asid: u32
    init_endpoint_id: u32
    init_endpoint_handle_slot: u32
    init_root_page_table: usize
    child_pid: u32
    child_tid: u32
    child_asid: u32
    child_wait_handle_slot: u32
    child_root_page_table: usize
    child_exit_code: i32
    boot_pid: u32
    boot_tid: u32
    boot_task_slot: u32
    boot_entry_pc: usize
    boot_stack_top: usize
    transfer_endpoint_id: u32
    transfer_source_handle_slot: u32
    transfer_received_handle_slot: u32
    transfer_service_directory_key: u32
    composition_echo_endpoint_id: u32
    composition_log_endpoint_id: u32
    serial_service_endpoint_id: u32
    init_image: init.InitImage
    arch_actor: u32
    boot_log_append_failed: u32
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    init_handle_table: capability.HandleTable
    child_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    endpoints: ipc.EndpointTable
    ready_queue: state.ReadyQueue
    kernel: state.KernelDescriptor
    bootstrap_program_capability: capability.CapabilitySlot
    init_bootstrap_handoff: init.BootstrapHandoffObservation
    init_user_frame: address_space.UserEntryFrame
    receive_observation: syscall.ReceiveObservation
    attached_receive_observation: syscall.ReceiveObservation
    transferred_handle_use_observation: syscall.ReceiveObservation
    pre_exit_wait_observation: syscall.WaitObservation
    exit_wait_observation: syscall.WaitObservation
    sleep_observation: syscall.SleepObservation
    timer_wake_observation: timer.TimerWakeObservation
    last_interrupt_kind: interrupt.InterruptDispatchKind
    log_service_snapshot: bootstrap_services.LogServiceSnapshot
    echo_service_snapshot: bootstrap_services.EchoServiceSnapshot
    transfer_service_snapshot: bootstrap_services.TransferServiceSnapshot
    serial_service_snapshot: bootstrap_services.SerialServiceSnapshot
    interrupts: interrupt.InterruptController
    uart_device: uart.UartDevice
    uart_receive_vector: u32
    uart_completion_vector: u32
    uart_source_actor: u32
    uart_frames: bootstrap_state.UartProbeFrames
}

struct Phase108To116ProofResult {
    status: i32
    running_slice_audit: debug.RunningKernelSliceAudit
}

struct Phase117To123ProofResult {
    status: i32
    phase123_audit: debug.Phase123NextPlateauAudit
}

struct Phase124DelegationChainProbeResult {
    succeeded: bool
    delegator_invalidated_send_status: syscall.SyscallStatus
    intermediary_invalidated_send_status: syscall.SyscallStatus
    final_send_status: syscall.SyscallStatus
    final_send_source_pid: u32
    final_endpoint_queue_depth: usize
}

struct Phase125InvalidationProbeResult {
    succeeded: bool
    rejected_send_status: syscall.SyscallStatus
    rejected_receive_status: syscall.SyscallStatus
    surviving_control_send_status: syscall.SyscallStatus
    surviving_control_source_pid: u32
    surviving_control_queue_depth: usize
}

struct Phase126AuthorityLifetimeProbeResult {
    succeeded: bool
    repeat_long_lived_send_status: syscall.SyscallStatus
    repeat_long_lived_source_pid: u32
    repeat_long_lived_queue_depth: usize
}

struct Phase128ServiceDeathObservationProbeResult {
    succeeded: bool
    observed_service_pid: u32
    observed_service_key: u32
    observed_wait_handle_slot: u32
    observed_exit_code: i32
}

struct Phase129PartialFailurePropagationProbeResult {
    succeeded: bool
    failed_service_pid: u32
    failed_wait_status: syscall.SyscallStatus
    surviving_service_pid: u32
    surviving_reply_status: syscall.SyscallStatus
    surviving_wait_status: syscall.SyscallStatus
    surviving_reply_byte0: u8
    surviving_reply_byte1: u8
}

struct Phase130ExplicitRestartProbeResult {
    succeeded: bool
    replacement_service_pid: u32
    replacement_spawn_status: syscall.SyscallStatus
    replacement_ack_status: syscall.SyscallStatus
    replacement_wait_status: syscall.SyscallStatus
    replacement_ack_byte: u8
    replacement_program_object_id: u32
}

struct Phase124To130ProofResult {
    status: i32
    phase130_audit: debug.Phase130ExplicitRestartOrReplacementAudit
}

struct Phase131To137ProofResult {
    status: i32
    composition_state: bootstrap_services.CompositionServiceExecutionState
    serial_state: bootstrap_services.SerialServiceExecutionState
    interrupts: interrupt.InterruptController
    last_interrupt_kind: interrupt.InterruptDispatchKind
    uart_device: uart.UartDevice
    uart_ingress: uart.UartIngressObservation
    phase137_audit: debug.Phase137OptionalDmaOrEquivalentAudit
}

struct MidPhaseProofResult {
    log_service_snapshot: bootstrap_services.LogServiceSnapshot
    echo_service_snapshot: bootstrap_services.EchoServiceSnapshot
    transfer_service_snapshot: bootstrap_services.TransferServiceSnapshot
    composition_state: bootstrap_services.CompositionServiceExecutionState
    serial_state: bootstrap_services.SerialServiceExecutionState
    interrupts: interrupt.InterruptController
    last_interrupt_kind: interrupt.InterruptDispatchKind
    uart_device: uart.UartDevice
    uart_ingress: uart.UartIngressObservation
    phase137_audit: debug.Phase137OptionalDmaOrEquivalentAudit
    proof_contract_flags: ProofContractFlags
}

struct MidPhaseProofExecutionResult {
    status: i32
    proof_result: MidPhaseProofResult
}

struct LatePhaseProofOutputs {
    invalidated_source_send_status: syscall.SyscallStatus
    phase124_delegator_invalidated_send_status: syscall.SyscallStatus
    phase124_intermediary_invalidated_send_status: syscall.SyscallStatus
    phase124_final_send_status: syscall.SyscallStatus
    phase124_final_send_source_pid: u32
    phase124_final_endpoint_queue_depth: usize
    phase125_rejected_send_status: syscall.SyscallStatus
    phase125_rejected_receive_status: syscall.SyscallStatus
    phase125_surviving_control_send_status: syscall.SyscallStatus
    phase125_surviving_control_source_pid: u32
    phase125_surviving_control_queue_depth: usize
    phase126_repeat_long_lived_send_status: syscall.SyscallStatus
    phase126_repeat_long_lived_source_pid: u32
    phase126_repeat_long_lived_queue_depth: usize
    phase128_observed_service_pid: u32
    phase128_observed_service_key: u32
    phase128_observed_wait_handle_slot: u32
    phase128_observed_exit_code: i32
    phase129_failed_service_pid: u32
    phase129_failed_wait_status: syscall.SyscallStatus
    phase129_surviving_service_pid: u32
    phase129_surviving_reply_status: syscall.SyscallStatus
    phase129_surviving_wait_status: syscall.SyscallStatus
    phase129_surviving_reply_byte0: u8
    phase129_surviving_reply_byte1: u8
    phase130_explicit_restart_audit: debug.Phase130ExplicitRestartOrReplacementAudit
    phase130_replacement_service_pid: u32
    phase130_replacement_spawn_status: syscall.SyscallStatus
    phase130_replacement_ack_status: syscall.SyscallStatus
    phase130_replacement_wait_status: syscall.SyscallStatus
    phase130_replacement_ack_byte: u8
    phase130_replacement_program_object_id: u32
    phase131_composition_state: bootstrap_services.CompositionServiceExecutionState
    phase137_serial_state: bootstrap_services.SerialServiceExecutionState
    phase137_interrupt_controller: interrupt.InterruptController
    phase137_last_interrupt_kind: interrupt.InterruptDispatchKind
    phase137_uart_device: uart.UartDevice
    phase137_uart_ingress: uart.UartIngressObservation
    phase137_optional_dma_audit: debug.Phase137OptionalDmaOrEquivalentAudit
}

var LATE_PHASE_PROOF_OUTPUTS: LatePhaseProofOutputs

func empty_late_phase_proof_outputs() LatePhaseProofOutputs {
    explicit_restart_audit: debug.Phase130ExplicitRestartOrReplacementAudit = debug.empty_phase130_explicit_restart_audit()
    optional_dma_audit: debug.Phase137OptionalDmaOrEquivalentAudit = debug.empty_phase137_optional_dma_audit()
    return LatePhaseProofOutputs{ invalidated_source_send_status: syscall.SyscallStatus.None, phase124_delegator_invalidated_send_status: syscall.SyscallStatus.None, phase124_intermediary_invalidated_send_status: syscall.SyscallStatus.None, phase124_final_send_status: syscall.SyscallStatus.None, phase124_final_send_source_pid: 0, phase124_final_endpoint_queue_depth: 0, phase125_rejected_send_status: syscall.SyscallStatus.None, phase125_rejected_receive_status: syscall.SyscallStatus.None, phase125_surviving_control_send_status: syscall.SyscallStatus.None, phase125_surviving_control_source_pid: 0, phase125_surviving_control_queue_depth: 0, phase126_repeat_long_lived_send_status: syscall.SyscallStatus.None, phase126_repeat_long_lived_source_pid: 0, phase126_repeat_long_lived_queue_depth: 0, phase128_observed_service_pid: 0, phase128_observed_service_key: 0, phase128_observed_wait_handle_slot: 0, phase128_observed_exit_code: 0, phase129_failed_service_pid: 0, phase129_failed_wait_status: syscall.SyscallStatus.None, phase129_surviving_service_pid: 0, phase129_surviving_reply_status: syscall.SyscallStatus.None, phase129_surviving_wait_status: syscall.SyscallStatus.None, phase129_surviving_reply_byte0: 0, phase129_surviving_reply_byte1: 0, phase130_explicit_restart_audit: explicit_restart_audit, phase130_replacement_service_pid: 0, phase130_replacement_spawn_status: syscall.SyscallStatus.None, phase130_replacement_ack_status: syscall.SyscallStatus.None, phase130_replacement_wait_status: syscall.SyscallStatus.None, phase130_replacement_ack_byte: 0, phase130_replacement_program_object_id: 0, phase131_composition_state: bootstrap_services.empty_composition_service_execution_state(), phase137_serial_state: bootstrap_services.empty_serial_service_execution_state(), phase137_interrupt_controller: interrupt.reset_controller(), phase137_last_interrupt_kind: interrupt.InterruptDispatchKind.None, phase137_uart_device: uart.empty_device(), phase137_uart_ingress: uart.empty_ingress_observation(), phase137_optional_dma_audit: optional_dma_audit }
}

func empty_proof_contract_flags() ProofContractFlags {
    return ProofContractFlags{ scheduler_contract_hardened: 0, lifecycle_contract_hardened: 0, capability_contract_hardened: 0, ipc_contract_hardened: 0, address_space_contract_hardened: 0, interrupt_contract_hardened: 0, timer_contract_hardened: 0, barrier_contract_hardened: 0 }
}

func reset_late_phase_proofs() {
    LATE_PHASE_PROOF_OUTPUTS = empty_late_phase_proof_outputs()
}

func install_composition_service_execution_state(next_state: bootstrap_services.CompositionServiceExecutionState) {
    LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state = next_state
}

func mid_phase_execution_result(status: i32, log_service_snapshot: bootstrap_services.LogServiceSnapshot, echo_service_snapshot: bootstrap_services.EchoServiceSnapshot, transfer_service_snapshot: bootstrap_services.TransferServiceSnapshot, proof_contract_flags: ProofContractFlags) MidPhaseProofExecutionResult {
    return MidPhaseProofExecutionResult{ status: status, proof_result: MidPhaseProofResult{ log_service_snapshot: log_service_snapshot, echo_service_snapshot: echo_service_snapshot, transfer_service_snapshot: transfer_service_snapshot, composition_state: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state, serial_state: LATE_PHASE_PROOF_OUTPUTS.phase137_serial_state, interrupts: LATE_PHASE_PROOF_OUTPUTS.phase137_interrupt_controller, last_interrupt_kind: LATE_PHASE_PROOF_OUTPUTS.phase137_last_interrupt_kind, uart_device: LATE_PHASE_PROOF_OUTPUTS.phase137_uart_device, uart_ingress: LATE_PHASE_PROOF_OUTPUTS.phase137_uart_ingress, phase137_audit: LATE_PHASE_PROOF_OUTPUTS.phase137_optional_dma_audit, proof_contract_flags: proof_contract_flags } }
}

func build_phase109_running_kernel_slice_audit(log_config: bootstrap_services.LogServiceConfig, echo_config: bootstrap_services.EchoServiceConfig, transfer_config: bootstrap_services.TransferServiceConfig, kernel: state.KernelDescriptor, init_pid: u32, init_tid: u32, init_asid: u32, child_tid: u32, child_exit_code: i32, transfer_endpoint_id: u32, init_bootstrap_handoff: init.BootstrapHandoffObservation, receive_observation: syscall.ReceiveObservation, attached_receive_observation: syscall.ReceiveObservation, transferred_handle_use_observation: syscall.ReceiveObservation, pre_exit_wait_observation: syscall.WaitObservation, exit_wait_observation: syscall.WaitObservation, sleep_observation: syscall.SleepObservation, timer_wake_observation: timer.TimerWakeObservation, log_service_handshake: log_service.LogHandshakeObservation, log_service_wait_observation: syscall.WaitObservation, echo_service_exchange: echo_service.EchoExchangeObservation, echo_service_wait_observation: syscall.WaitObservation, transfer_service_transfer: transfer_service.TransferObservation, transfer_service_wait_observation: syscall.WaitObservation, phase104_contract_hardened: u32, phase108_contract_hardened: u32, init_process: state.ProcessSlot, init_task: state.TaskSlot, init_user_frame: address_space.UserEntryFrame, boot_log_append_failed: u32) debug.RunningKernelSliceAudit {
    return bootstrap_audit.build_phase109_running_kernel_slice_audit(bootstrap_audit.RunningKernelSliceAuditInputs{ kernel: kernel, init_pid: init_pid, init_tid: init_tid, init_asid: init_asid, child_tid: child_tid, child_exit_code: child_exit_code, transfer_endpoint_id: transfer_endpoint_id, log_service_request_byte: log_config.request_byte, echo_service_request_byte0: echo_config.request_byte0, echo_service_request_byte1: echo_config.request_byte1, log_service_exit_code: log_config.exit_code, echo_service_exit_code: echo_config.exit_code, transfer_service_exit_code: transfer_config.exit_code, init_bootstrap_handoff: init_bootstrap_handoff, receive_observation: receive_observation, attached_receive_observation: attached_receive_observation, transferred_handle_use_observation: transferred_handle_use_observation, pre_exit_wait_observation: pre_exit_wait_observation, exit_wait_observation: exit_wait_observation, sleep_observation: sleep_observation, timer_wake_observation: timer_wake_observation, log_service_handshake: log_service_handshake, log_service_wait_observation: log_service_wait_observation, echo_service_exchange: echo_service_exchange, echo_service_wait_observation: echo_service_wait_observation, transfer_service_transfer: transfer_service_transfer, transfer_service_wait_observation: transfer_service_wait_observation, phase104_contract_hardened: phase104_contract_hardened, phase108_contract_hardened: phase108_contract_hardened, init_process: init_process, init_task: init_task, init_user_frame: init_user_frame, boot_log_append_failed: boot_log_append_failed })
}

func build_log_service_config(runtime_context: MidPhaseProofRuntimeContext) bootstrap_services.LogServiceConfig {
    return bootstrap_services.log_service_config(runtime_context.init_pid, runtime_context.child_pid, runtime_context.child_tid, runtime_context.child_asid, runtime_context.init_endpoint_id, runtime_context.init_endpoint_handle_slot, mmu.bootstrap_translation_root(runtime_context.child_asid, runtime_context.child_root_page_table))
}

func build_echo_service_config(runtime_context: MidPhaseProofRuntimeContext) bootstrap_services.EchoServiceConfig {
    return bootstrap_services.echo_service_config(runtime_context.init_pid, runtime_context.child_pid, runtime_context.child_tid, runtime_context.child_asid, runtime_context.init_endpoint_id, runtime_context.init_endpoint_handle_slot, mmu.bootstrap_translation_root(runtime_context.child_asid, runtime_context.child_root_page_table))
}

func build_transfer_service_config(runtime_context: MidPhaseProofRuntimeContext) bootstrap_services.TransferServiceConfig {
    return bootstrap_services.transfer_service_config(runtime_context.init_pid, runtime_context.child_pid, runtime_context.child_tid, runtime_context.child_asid, runtime_context.init_endpoint_id, runtime_context.init_endpoint_handle_slot, mmu.bootstrap_translation_root(runtime_context.child_asid, runtime_context.child_root_page_table), runtime_context.transfer_source_handle_slot, runtime_context.transfer_received_handle_slot)
}

func build_composition_service_config(runtime_context: MidPhaseProofRuntimeContext) bootstrap_services.CompositionServiceConfig {
    return bootstrap_services.composition_service_config(runtime_context.init_pid, runtime_context.child_pid, runtime_context.child_tid, runtime_context.child_asid, runtime_context.init_endpoint_id, runtime_context.composition_echo_endpoint_id, runtime_context.composition_log_endpoint_id, runtime_context.init_endpoint_handle_slot, mmu.bootstrap_translation_root(runtime_context.child_asid, runtime_context.child_root_page_table))
}

func build_serial_service_config(runtime_context: MidPhaseProofRuntimeContext) bootstrap_services.SerialServiceConfig {
    return bootstrap_services.serial_service_config(runtime_context.init_pid, runtime_context.child_pid, runtime_context.child_tid, runtime_context.child_asid, runtime_context.serial_service_endpoint_id, mmu.bootstrap_translation_root(runtime_context.child_asid, runtime_context.child_root_page_table))
}

func build_service_runtime_state(gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, init_handle_table: capability.HandleTable, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, endpoints: ipc.EndpointTable, init_image: init.InitImage, ready_queue: state.ReadyQueue) bootstrap_services.ServiceRuntimeState {
    return bootstrap_services.service_runtime_state(gate, process_slots, task_slots, init_handle_table, child_handle_table, wait_table, endpoints, init_image, ready_queue)
}

func build_log_service_execution_state(snapshot: bootstrap_services.LogServiceSnapshot, gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, init_handle_table: capability.HandleTable, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, endpoints: ipc.EndpointTable, init_image: init.InitImage, ready_queue: state.ReadyQueue) bootstrap_services.LogServiceExecutionState {
    return bootstrap_services.log_service_execution_state(snapshot, build_service_runtime_state(gate, process_slots, task_slots, init_handle_table, child_handle_table, wait_table, endpoints, init_image, ready_queue))
}

func build_echo_service_execution_state(snapshot: bootstrap_services.EchoServiceSnapshot, gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, init_handle_table: capability.HandleTable, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, endpoints: ipc.EndpointTable, init_image: init.InitImage, ready_queue: state.ReadyQueue) bootstrap_services.EchoServiceExecutionState {
    return bootstrap_services.echo_service_execution_state(snapshot, build_service_runtime_state(gate, process_slots, task_slots, init_handle_table, child_handle_table, wait_table, endpoints, init_image, ready_queue))
}

func build_transfer_service_execution_state(snapshot: bootstrap_services.TransferServiceSnapshot, gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, init_handle_table: capability.HandleTable, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, endpoints: ipc.EndpointTable, init_image: init.InitImage, ready_queue: state.ReadyQueue) bootstrap_services.TransferServiceExecutionState {
    return bootstrap_services.transfer_service_execution_state(snapshot, build_service_runtime_state(gate, process_slots, task_slots, init_handle_table, child_handle_table, wait_table, endpoints, init_image, ready_queue))
}

func build_serial_service_execution_state(snapshot: bootstrap_services.SerialServiceSnapshot, gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, init_handle_table: capability.HandleTable, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, endpoints: ipc.EndpointTable, init_image: init.InitImage, ready_queue: state.ReadyQueue) bootstrap_services.SerialServiceExecutionState {
    return bootstrap_services.serial_service_execution_state(snapshot, build_service_runtime_state(gate, process_slots, task_slots, init_handle_table, child_handle_table, wait_table, endpoints, init_image, ready_queue))
}

func validate_phase104_contract_hardening(runtime_context: MidPhaseProofRuntimeContext, timer_hardened: u32) bool {
    return bootstrap_audit.validate_phase104_contract_hardening(bootstrap_audit.Phase104HardeningAudit{ boot_log_append_failed: runtime_context.boot_log_append_failed, timer_hardened: timer_hardened, bootstrap_layout: bootstrap_audit.BootstrapLayoutAudit{ init_image: runtime_context.init_image, init_root_page_table: runtime_context.init_root_page_table }, endpoint_capability: bootstrap_audit.EndpointCapabilityAudit{ init_pid: runtime_context.init_pid, init_endpoint_id: runtime_context.init_endpoint_id, transfer_endpoint_id: runtime_context.transfer_endpoint_id, child_wait_handle_slot: runtime_context.child_wait_handle_slot }, state_hardening: bootstrap_audit.StateHardeningAudit{ boot_tid: runtime_context.boot_tid, boot_pid: runtime_context.boot_pid, boot_entry_pc: runtime_context.boot_entry_pc, boot_stack_top: runtime_context.boot_stack_top, child_tid: runtime_context.child_tid, child_pid: runtime_context.child_pid, child_asid: runtime_context.child_asid, init_image: runtime_context.init_image, arch_actor: runtime_context.arch_actor }, syscall_hardening: bootstrap_audit.SyscallHardeningAudit{ init_pid: runtime_context.init_pid, init_endpoint_handle_slot: runtime_context.init_endpoint_handle_slot, init_endpoint_id: runtime_context.init_endpoint_id, child_wait_handle_slot: runtime_context.child_wait_handle_slot, child_pid: runtime_context.child_pid, child_tid: runtime_context.child_tid, child_asid: runtime_context.child_asid, child_root_page_table: runtime_context.child_root_page_table, boot_pid: runtime_context.boot_pid, boot_tid: runtime_context.boot_tid, boot_task_slot: runtime_context.boot_task_slot, boot_entry_pc: runtime_context.boot_entry_pc, boot_stack_top: runtime_context.boot_stack_top, init_image: runtime_context.init_image, transfer_source_handle_slot: runtime_context.transfer_source_handle_slot } })
}

func validate_phase105_log_service_handshake(runtime_context: MidPhaseProofRuntimeContext, config: bootstrap_services.LogServiceConfig, snapshot: bootstrap_services.LogServiceSnapshot, gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, ready_queue: state.ReadyQueue) bool {
    return bootstrap_audit.validate_phase105_log_service_handshake(bootstrap_audit.LogServicePhaseAudit{ program_capability: snapshot.program_capability, gate: gate, spawn_observation: snapshot.spawn_observation, receive_observation: snapshot.receive_observation, ack_observation: snapshot.ack_observation, service_state: snapshot.state, handshake: snapshot.handshake, wait_observation: snapshot.wait_observation, wait_table: wait_table, child_handle_table: child_handle_table, ready_queue: ready_queue, child_process: process_slots[2], child_task: task_slots[2], child_address_space: snapshot.address_space, child_user_frame: snapshot.user_frame, init_pid: runtime_context.init_pid, child_pid: runtime_context.child_pid, child_tid: runtime_context.child_tid, child_asid: runtime_context.child_asid, init_endpoint_id: runtime_context.init_endpoint_id, wait_handle_slot: config.wait_handle_slot, endpoint_handle_slot: config.endpoint_handle_slot, request_byte: config.request_byte, exit_code: config.exit_code })
}

func validate_phase106_echo_service_request_reply(runtime_context: MidPhaseProofRuntimeContext, config: bootstrap_services.EchoServiceConfig, snapshot: bootstrap_services.EchoServiceSnapshot, gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, ready_queue: state.ReadyQueue) bool {
    return bootstrap_audit.validate_phase106_echo_service_request_reply(bootstrap_audit.EchoServicePhaseAudit{ program_capability: snapshot.program_capability, gate: gate, spawn_observation: snapshot.spawn_observation, receive_observation: snapshot.receive_observation, reply_observation: snapshot.reply_observation, service_state: snapshot.state, exchange: snapshot.exchange, wait_observation: snapshot.wait_observation, wait_table: wait_table, child_handle_table: child_handle_table, ready_queue: ready_queue, child_process: process_slots[2], child_task: task_slots[2], child_address_space: snapshot.address_space, child_user_frame: snapshot.user_frame, init_pid: runtime_context.init_pid, child_pid: runtime_context.child_pid, child_tid: runtime_context.child_tid, child_asid: runtime_context.child_asid, init_endpoint_id: runtime_context.init_endpoint_id, wait_handle_slot: config.wait_handle_slot, endpoint_handle_slot: config.endpoint_handle_slot, request_byte0: config.request_byte0, request_byte1: config.request_byte1, exit_code: config.exit_code })
}

func validate_phase107_user_to_user_capability_transfer(runtime_context: MidPhaseProofRuntimeContext, config: bootstrap_services.TransferServiceConfig, snapshot: bootstrap_services.TransferServiceSnapshot, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, init_handle_table: capability.HandleTable, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, ready_queue: state.ReadyQueue, gate: syscall.SyscallGate) bool {
    return bootstrap_audit.validate_phase107_user_to_user_capability_transfer(bootstrap_audit.TransferServicePhaseAudit{ program_capability: snapshot.program_capability, gate: gate, spawn_observation: snapshot.spawn_observation, grant_observation: snapshot.grant_observation, emit_observation: snapshot.emit_observation, service_state: snapshot.state, transfer: snapshot.transfer, wait_observation: snapshot.wait_observation, init_handle_table: init_handle_table, wait_table: wait_table, child_handle_table: child_handle_table, ready_queue: ready_queue, child_process: process_slots[2], child_task: task_slots[2], child_address_space: snapshot.address_space, child_user_frame: snapshot.user_frame, init_pid: runtime_context.init_pid, child_pid: runtime_context.child_pid, child_tid: runtime_context.child_tid, child_asid: runtime_context.child_asid, init_endpoint_id: runtime_context.init_endpoint_id, transfer_endpoint_id: runtime_context.transfer_endpoint_id, wait_handle_slot: config.wait_handle_slot, source_handle_slot: config.source_handle_slot, control_handle_slot: config.control_handle_slot, init_received_handle_slot: config.init_received_handle_slot, service_received_handle_slot: config.service_received_handle_slot, grant_byte0: config.grant_byte0, grant_byte1: config.grant_byte1, grant_byte2: config.grant_byte2, grant_byte3: config.grant_byte3, exit_code: config.exit_code })
}

func execute_mid_phase_proofs(runtime_context: MidPhaseProofRuntimeContext) MidPhaseProofExecutionResult {
    LATE_PHASE_PROOF_OUTPUTS = empty_late_phase_proof_outputs()

    local_log_snapshot: bootstrap_services.LogServiceSnapshot = runtime_context.log_service_snapshot
    local_echo_snapshot: bootstrap_services.EchoServiceSnapshot = runtime_context.echo_service_snapshot
    local_transfer_snapshot: bootstrap_services.TransferServiceSnapshot = runtime_context.transfer_service_snapshot

    phase104_contract_hardened: u32 = 0
    scheduler_contract_hardened: u32 = 0
    lifecycle_contract_hardened: u32 = 0
    capability_contract_hardened: u32 = 0
    ipc_contract_hardened: u32 = 0
    address_space_contract_hardened: u32 = 0
    interrupt_contract_hardened: u32 = 0
    timer_contract_hardened: u32 = 0
    barrier_contract_hardened: u32 = 0

    if !sched.validate_program_cap_spawn_and_wait(runtime_context.scheduler_lifecycle_audit) {
        return mid_phase_execution_result(27, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    scheduler_contract_hardened = 1
    if !lifecycle.validate_task_transition_contracts() {
        return mid_phase_execution_result(28, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    lifecycle_contract_hardened = 1
    if !capability.validate_syscall_capability_boundary() {
        return mid_phase_execution_result(29, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    capability_contract_hardened = 1
    if !ipc.validate_syscall_ipc_boundary() {
        return mid_phase_execution_result(30, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    ipc_contract_hardened = 1
    if !address_space.validate_syscall_address_space_boundary() {
        return mid_phase_execution_result(31, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    address_space_contract_hardened = 1
    if !mmu.validate_address_space_mmu_boundary() {
        return mid_phase_execution_result(32, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    if !interrupt.validate_interrupt_entry_and_dispatch_boundary() {
        return mid_phase_execution_result(33, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    interrupt_contract_hardened = 1
    if !timer.validate_interrupt_delivery_boundary() {
        return mid_phase_execution_result(34, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    timer_contract_hardened = 1
    if !mmu.validate_activation_barrier_boundary() {
        return mid_phase_execution_result(35, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    barrier_contract_hardened = 1
    if !validate_phase104_contract_hardening(runtime_context, timer_contract_hardened) {
        return mid_phase_execution_result(36, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    phase104_contract_hardened = 1

    local_gate: syscall.SyscallGate = runtime_context.gate
    local_process_slots: [3]state.ProcessSlot = runtime_context.process_slots
    local_task_slots: [3]state.TaskSlot = runtime_context.task_slots
    local_init_handle_table: capability.HandleTable = runtime_context.init_handle_table
    local_child_handle_table: capability.HandleTable = runtime_context.child_handle_table
    local_wait_table: capability.WaitTable = runtime_context.wait_table
    local_endpoints: ipc.EndpointTable = runtime_context.endpoints
    local_ready_queue: state.ReadyQueue = runtime_context.ready_queue
    log_config: bootstrap_services.LogServiceConfig = build_log_service_config(runtime_context)
    echo_config: bootstrap_services.EchoServiceConfig = build_echo_service_config(runtime_context)
    transfer_config: bootstrap_services.TransferServiceConfig = build_transfer_service_config(runtime_context)

    log_result: bootstrap_services.LogServiceExecutionResult = bootstrap_services.execute_phase105_log_service_handshake(log_config, build_log_service_execution_state(local_log_snapshot, local_gate, local_process_slots, local_task_slots, local_init_handle_table, local_child_handle_table, local_wait_table, local_endpoints, runtime_context.init_image, local_ready_queue))
    if log_result.succeeded == 0 {
        return mid_phase_execution_result(37, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    local_gate = log_result.state.gate
    local_process_slots = log_result.state.process_slots
    local_task_slots = log_result.state.task_slots
    local_init_handle_table = log_result.state.init_handle_table
    local_child_handle_table = log_result.state.child_handle_table
    local_wait_table = log_result.state.wait_table
    local_endpoints = log_result.state.endpoints
    local_ready_queue = log_result.state.ready_queue
    local_log_snapshot = bootstrap_services.log_service_snapshot(log_result.state)
    if !validate_phase105_log_service_handshake(runtime_context, log_config, local_log_snapshot, local_gate, local_process_slots, local_task_slots, local_child_handle_table, local_wait_table, local_ready_queue) {
        return mid_phase_execution_result(38, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }

    echo_result: bootstrap_services.EchoServiceExecutionResult = bootstrap_services.execute_phase106_echo_service_request_reply(echo_config, build_echo_service_execution_state(local_echo_snapshot, local_gate, local_process_slots, local_task_slots, local_init_handle_table, local_child_handle_table, local_wait_table, local_endpoints, runtime_context.init_image, local_ready_queue))
    if echo_result.succeeded == 0 {
        return mid_phase_execution_result(39, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    local_gate = echo_result.state.gate
    local_process_slots = echo_result.state.process_slots
    local_task_slots = echo_result.state.task_slots
    local_init_handle_table = echo_result.state.init_handle_table
    local_child_handle_table = echo_result.state.child_handle_table
    local_wait_table = echo_result.state.wait_table
    local_endpoints = echo_result.state.endpoints
    local_ready_queue = echo_result.state.ready_queue
    local_echo_snapshot = bootstrap_services.echo_service_snapshot(echo_result.state)
    if !validate_phase106_echo_service_request_reply(runtime_context, echo_config, local_echo_snapshot, local_gate, local_process_slots, local_task_slots, local_child_handle_table, local_wait_table, local_ready_queue) {
        return mid_phase_execution_result(40, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }

    transfer_result: bootstrap_services.TransferServiceExecutionResult = bootstrap_services.execute_phase107_user_to_user_capability_transfer(transfer_config, build_transfer_service_execution_state(local_transfer_snapshot, local_gate, local_process_slots, local_task_slots, local_init_handle_table, local_child_handle_table, local_wait_table, local_endpoints, runtime_context.init_image, local_ready_queue))
    if transfer_result.succeeded == 0 {
        return mid_phase_execution_result(41, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }
    local_gate = transfer_result.state.gate
    local_process_slots = transfer_result.state.process_slots
    local_task_slots = transfer_result.state.task_slots
    local_init_handle_table = transfer_result.state.init_handle_table
    local_child_handle_table = transfer_result.state.child_handle_table
    local_wait_table = transfer_result.state.wait_table
    local_endpoints = transfer_result.state.endpoints
    local_ready_queue = transfer_result.state.ready_queue
    local_transfer_snapshot = bootstrap_services.transfer_service_snapshot(transfer_result.state)
    if !validate_phase107_user_to_user_capability_transfer(runtime_context, transfer_config, local_transfer_snapshot, local_process_slots, local_task_slots, local_init_handle_table, local_child_handle_table, local_wait_table, local_ready_queue, local_gate) {
        return mid_phase_execution_result(42, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, empty_proof_contract_flags())
    }

    proof_contract_flags: ProofContractFlags = ProofContractFlags{ scheduler_contract_hardened: scheduler_contract_hardened, lifecycle_contract_hardened: lifecycle_contract_hardened, capability_contract_hardened: capability_contract_hardened, ipc_contract_hardened: ipc_contract_hardened, address_space_contract_hardened: address_space_contract_hardened, interrupt_contract_hardened: interrupt_contract_hardened, timer_contract_hardened: timer_contract_hardened, barrier_contract_hardened: barrier_contract_hardened }
    phase108_to_116_result: Phase108To116ProofResult = execute_phase108_to_116_proofs(Phase108To116ProofInputs{ context: Phase108To116RuntimeContext{ init_pid: runtime_context.init_pid, init_tid: runtime_context.init_tid, init_asid: runtime_context.init_asid, child_tid: runtime_context.child_tid, child_exit_code: runtime_context.child_exit_code, transfer_endpoint_id: runtime_context.transfer_endpoint_id, log_config: log_config, echo_config: echo_config, transfer_config: transfer_config, bootstrap_program_capability: runtime_context.bootstrap_program_capability, log_service_program_capability: local_log_snapshot.program_capability, echo_service_program_capability: local_echo_snapshot.program_capability, transfer_service_program_capability: local_transfer_snapshot.program_capability, log_service_spawn: local_log_snapshot.spawn_observation, echo_service_spawn: local_echo_snapshot.spawn_observation, transfer_service_spawn: local_transfer_snapshot.spawn_observation, log_service_wait: local_log_snapshot.wait_observation, echo_service_wait: local_echo_snapshot.wait_observation, transfer_service_wait: local_transfer_snapshot.wait_observation, kernel: runtime_context.kernel, init_bootstrap_handoff: runtime_context.init_bootstrap_handoff, receive_observation: runtime_context.receive_observation, attached_receive_observation: runtime_context.attached_receive_observation, transferred_handle_use_observation: runtime_context.transferred_handle_use_observation, pre_exit_wait_observation: runtime_context.pre_exit_wait_observation, exit_wait_observation: runtime_context.exit_wait_observation, sleep_observation: runtime_context.sleep_observation, timer_wake_observation: runtime_context.timer_wake_observation, log_service_handshake: local_log_snapshot.handshake, echo_service_exchange: local_echo_snapshot.exchange, transfer_service_transfer: local_transfer_snapshot.transfer, phase104_contract_hardened: phase104_contract_hardened, init_process: local_process_slots[1], init_task: local_task_slots[1], init_user_frame: runtime_context.init_user_frame, boot_log_append_failed: runtime_context.boot_log_append_failed, last_interrupt_kind: runtime_context.last_interrupt_kind }, contract_flags: proof_contract_flags })
    if phase108_to_116_result.status != 0 {
        return mid_phase_execution_result(phase108_to_116_result.status, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, proof_contract_flags)
    }

    probe_payload: [4]u8 = ipc.zero_payload()
    probe_payload[0] = transfer_config.grant_byte0
    probe_result: syscall.SendResult = syscall.perform_send(local_gate, local_init_handle_table, local_endpoints, runtime_context.init_pid, syscall.build_send_request(transfer_config.source_handle_slot, 1, probe_payload))
    local_gate = probe_result.gate
    local_init_handle_table = probe_result.handle_table
    local_endpoints = probe_result.endpoints
    LATE_PHASE_PROOF_OUTPUTS.invalidated_source_send_status = probe_result.status

    phase117_to_123_result: Phase117To123ProofResult = execute_phase117_to_123_proofs(Phase117To123ProofInputs{ running_slice_audit: phase108_to_116_result.running_slice_audit, context: Phase117To123RuntimeContext{ init_pid: runtime_context.init_pid, init_endpoint_id: runtime_context.init_endpoint_id, transfer_endpoint_id: runtime_context.transfer_endpoint_id, log_service_directory_key: runtime_context.late_phase_context.log_service_directory_key, echo_service_directory_key: runtime_context.late_phase_context.echo_service_directory_key, transfer_service_directory_key: runtime_context.transfer_service_directory_key, log_config: log_config, echo_config: echo_config, transfer_config: transfer_config, log_service_program_capability: local_log_snapshot.program_capability, echo_service_program_capability: local_echo_snapshot.program_capability, transfer_service_program_capability: local_transfer_snapshot.program_capability, log_service_spawn: local_log_snapshot.spawn_observation, echo_service_spawn: local_echo_snapshot.spawn_observation, transfer_service_spawn: local_transfer_snapshot.spawn_observation, log_service_wait: local_log_snapshot.wait_observation, echo_service_wait: local_echo_snapshot.wait_observation, transfer_service_wait: local_transfer_snapshot.wait_observation, log_service_handshake: local_log_snapshot.handshake, echo_service_exchange: local_echo_snapshot.exchange, transfer_service_transfer: local_transfer_snapshot.transfer, init_handle_table: local_init_handle_table }, phase118_probe_succeeded: syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.invalidated_source_send_status) == 8, contract_flags: proof_contract_flags })
    if phase117_to_123_result.status != 0 {
        return mid_phase_execution_result(phase117_to_123_result.status, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, proof_contract_flags)
    }

    phase123_audit: debug.Phase123NextPlateauAudit = phase117_to_123_result.phase123_audit
    phase124_to_130_status: i32 = execute_phase124_to_130_proofs(runtime_context.late_phase_context, transfer_config, log_config, echo_config, phase123_audit, local_log_snapshot.spawn_observation, local_log_snapshot.wait_observation, build_log_service_execution_state(local_log_snapshot, local_gate, local_process_slots, local_task_slots, local_init_handle_table, local_child_handle_table, local_wait_table, local_endpoints, runtime_context.init_image, local_ready_queue), proof_contract_flags)
    if phase124_to_130_status != 0 {
        return mid_phase_execution_result(phase124_to_130_status, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, proof_contract_flags)
    }

    phase130_audit: debug.Phase130ExplicitRestartOrReplacementAudit = LATE_PHASE_PROOF_OUTPUTS.phase130_explicit_restart_audit
    phase131_to_137_status: i32 = execute_phase131_to_137_proofs(Phase131To137ProofInputs{ context: Phase131To137RuntimeContext{ context: runtime_context.late_phase_context, composition_config: build_composition_service_config(runtime_context), serial_config: build_serial_service_config(runtime_context), phase130_audit: phase130_audit, gate: local_gate, process_slots: local_process_slots, task_slots: local_task_slots, init_handle_table: local_init_handle_table, child_handle_table: local_child_handle_table, wait_table: local_wait_table, endpoints: local_endpoints, init_image: runtime_context.init_image, serial_execution_state: build_serial_service_execution_state(runtime_context.serial_service_snapshot, local_gate, local_process_slots, local_task_slots, local_init_handle_table, local_child_handle_table, local_wait_table, local_endpoints, runtime_context.init_image, local_ready_queue), interrupts: runtime_context.interrupts, uart_device: runtime_context.uart_device, boot_pid: runtime_context.boot_pid, uart_receive_vector: runtime_context.uart_receive_vector, uart_completion_vector: runtime_context.uart_completion_vector, uart_source_actor: runtime_context.uart_source_actor, uart_frames: runtime_context.uart_frames }, contract_flags: proof_contract_flags })
    if phase131_to_137_status != 0 {
        return mid_phase_execution_result(phase131_to_137_status, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, proof_contract_flags)
    }

    return mid_phase_execution_result(0, local_log_snapshot, local_echo_snapshot, local_transfer_snapshot, proof_contract_flags)
}

func execute_phase108_to_116_proofs(inputs: Phase108To116ProofInputs) Phase108To116ProofResult {
    phase109_audit: debug.RunningKernelSliceAudit = build_phase109_running_kernel_slice_audit(inputs.context.log_config, inputs.context.echo_config, inputs.context.transfer_config, inputs.context.kernel, inputs.context.init_pid, inputs.context.init_tid, inputs.context.init_asid, inputs.context.child_tid, inputs.context.child_exit_code, inputs.context.transfer_endpoint_id, inputs.context.init_bootstrap_handoff, inputs.context.receive_observation, inputs.context.attached_receive_observation, inputs.context.transferred_handle_use_observation, inputs.context.pre_exit_wait_observation, inputs.context.exit_wait_observation, inputs.context.sleep_observation, inputs.context.timer_wake_observation, inputs.context.log_service_handshake, inputs.context.log_service_wait, inputs.context.echo_service_exchange, inputs.context.echo_service_wait, inputs.context.transfer_service_transfer, inputs.context.transfer_service_wait, inputs.context.phase104_contract_hardened, 1, inputs.context.init_process, inputs.context.init_task, inputs.context.init_user_frame, inputs.context.boot_log_append_failed)
    if !debug.validate_phase108_kernel_image_and_program_cap_contracts(bootstrap_audit.build_phase108_program_cap_contract(bootstrap_audit.Phase108ProgramCapContractInputs{ init_pid: inputs.context.init_pid, log_service_program_object_id: inputs.context.log_config.program_object_id, echo_service_program_object_id: inputs.context.echo_config.program_object_id, transfer_service_program_object_id: inputs.context.transfer_config.program_object_id, log_service_wait_handle_slot: inputs.context.log_config.wait_handle_slot, echo_service_wait_handle_slot: inputs.context.echo_config.wait_handle_slot, transfer_service_wait_handle_slot: inputs.context.transfer_config.wait_handle_slot, log_service_exit_code: inputs.context.log_config.exit_code, echo_service_exit_code: inputs.context.echo_config.exit_code, transfer_service_exit_code: inputs.context.transfer_config.exit_code, bootstrap_program_capability: inputs.context.bootstrap_program_capability, log_service_program_capability: inputs.context.log_service_program_capability, echo_service_program_capability: inputs.context.echo_service_program_capability, transfer_service_program_capability: inputs.context.transfer_service_program_capability, log_service_spawn: inputs.context.log_service_spawn, echo_service_spawn: inputs.context.echo_service_spawn, transfer_service_spawn: inputs.context.transfer_service_spawn, log_service_wait: inputs.context.log_service_wait, echo_service_wait: inputs.context.echo_service_wait, transfer_service_wait: inputs.context.transfer_service_wait })) {
        return Phase108To116ProofResult{ status: 43, running_slice_audit: phase109_audit }
    }

    if !debug.validate_phase109_first_running_kernel_slice(phase109_audit) {
        return Phase108To116ProofResult{ status: 44, running_slice_audit: phase109_audit }
    }
    if !debug.validate_phase110_kernel_ownership_split(phase109_audit, inputs.contract_flags.scheduler_contract_hardened) {
        return Phase108To116ProofResult{ status: 45, running_slice_audit: phase109_audit }
    }
    if !debug.validate_phase111_scheduler_and_lifecycle_ownership(phase109_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened) {
        return Phase108To116ProofResult{ status: 46, running_slice_audit: phase109_audit }
    }
    if !debug.validate_phase112_syscall_boundary_thinness(phase109_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened) {
        return Phase108To116ProofResult{ status: 47, running_slice_audit: phase109_audit }
    }
    if !debug.validate_phase113_interrupt_entry_and_generic_dispatch_boundary(phase109_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.context.last_interrupt_kind) {
        return Phase108To116ProofResult{ status: 48, running_slice_audit: phase109_audit }
    }
    if !debug.validate_phase114_address_space_and_mmu_ownership_split(phase109_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened) {
        return Phase108To116ProofResult{ status: 49, running_slice_audit: phase109_audit }
    }
    if !debug.validate_phase115_timer_ownership_hardening(phase109_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened) {
        return Phase108To116ProofResult{ status: 50, running_slice_audit: phase109_audit }
    }
    if !debug.validate_phase116_mmu_activation_barrier_follow_through(phase109_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return Phase108To116ProofResult{ status: 51, running_slice_audit: phase109_audit }
    }

    return Phase108To116ProofResult{ status: 0, running_slice_audit: phase109_audit }
}

func build_phase117_multi_service_bring_up_audit(running_slice_audit: debug.RunningKernelSliceAudit, init_endpoint_id: u32, transfer_endpoint_id: u32, log_service_program_capability: capability.CapabilitySlot, echo_service_program_capability: capability.CapabilitySlot, transfer_service_program_capability: capability.CapabilitySlot, log_service_spawn: syscall.SpawnObservation, echo_service_spawn: syscall.SpawnObservation, transfer_service_spawn: syscall.SpawnObservation, log_service_wait: syscall.WaitObservation, echo_service_wait: syscall.WaitObservation, transfer_service_wait: syscall.WaitObservation, log_service_handshake: log_service.LogHandshakeObservation, echo_service_exchange: echo_service.EchoExchangeObservation, transfer_service_transfer: transfer_service.TransferObservation) debug.Phase117MultiServiceBringUpAudit {
    return bootstrap_audit.build_phase117_multi_service_bring_up_audit(bootstrap_audit.Phase117MultiServiceBringUpAuditInputs{ running_slice: running_slice_audit, init_endpoint_id: init_endpoint_id, transfer_endpoint_id: transfer_endpoint_id, log_service_program_capability: log_service_program_capability, echo_service_program_capability: echo_service_program_capability, transfer_service_program_capability: transfer_service_program_capability, log_service_spawn: log_service_spawn, echo_service_spawn: echo_service_spawn, transfer_service_spawn: transfer_service_spawn, log_service_wait: log_service_wait, echo_service_wait: echo_service_wait, transfer_service_wait: transfer_service_wait, log_service_handshake: log_service_handshake, echo_service_exchange: echo_service_exchange, transfer_service_transfer: transfer_service_transfer })
}

func build_phase118_delegated_request_reply_audit(phase117_audit: debug.Phase117MultiServiceBringUpAudit, transfer_config: bootstrap_services.TransferServiceConfig, init_handle_table: capability.HandleTable, transfer_service_transfer: transfer_service.TransferObservation, invalidated_source_send_status: syscall.SyscallStatus) debug.Phase118DelegatedRequestReplyAudit {
    retained_receive_endpoint_id: u32 = capability.find_endpoint_for_handle(init_handle_table, transfer_config.init_received_handle_slot)
    return bootstrap_audit.build_phase118_delegated_request_reply_audit(bootstrap_audit.Phase118DelegatedRequestReplyAuditInputs{ phase117: phase117_audit, transfer_service_transfer: transfer_service_transfer, invalidated_source_send_status: invalidated_source_send_status, invalidated_source_handle_slot: transfer_config.source_handle_slot, retained_receive_handle_slot: transfer_config.init_received_handle_slot, retained_receive_endpoint_id: retained_receive_endpoint_id })
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

func execute_phase117_to_123_proofs(inputs: Phase117To123ProofInputs) Phase117To123ProofResult {
    phase117_audit: debug.Phase117MultiServiceBringUpAudit = build_phase117_multi_service_bring_up_audit(inputs.running_slice_audit, inputs.context.init_endpoint_id, inputs.context.transfer_endpoint_id, inputs.context.log_service_program_capability, inputs.context.echo_service_program_capability, inputs.context.transfer_service_program_capability, inputs.context.log_service_spawn, inputs.context.echo_service_spawn, inputs.context.transfer_service_spawn, inputs.context.log_service_wait, inputs.context.echo_service_wait, inputs.context.transfer_service_wait, inputs.context.log_service_handshake, inputs.context.echo_service_exchange, inputs.context.transfer_service_transfer)
    phase118_audit: debug.Phase118DelegatedRequestReplyAudit = build_phase118_delegated_request_reply_audit(phase117_audit, inputs.context.transfer_config, inputs.context.init_handle_table, inputs.context.transfer_service_transfer, LATE_PHASE_PROOF_OUTPUTS.invalidated_source_send_status)
    phase119_audit: debug.Phase119NamespacePressureAudit = build_phase119_namespace_pressure_audit(phase118_audit, inputs.context.init_pid, inputs.context.init_endpoint_id, inputs.context.log_service_directory_key, inputs.context.echo_service_directory_key, inputs.context.transfer_service_directory_key, inputs.context.log_config, inputs.context.echo_config, inputs.context.transfer_config, inputs.context.log_service_spawn, inputs.context.echo_service_spawn, inputs.context.transfer_service_spawn)
    phase120_audit: debug.Phase120RunningSystemSupportAudit = build_phase120_running_system_support_audit(phase119_audit, inputs.context.init_pid, inputs.context.init_endpoint_id)
    phase121_audit: debug.Phase121KernelImageContractAudit = build_phase121_kernel_image_contract_audit(phase120_audit)
    phase122_audit: debug.Phase122TargetSurfaceAudit = build_phase122_target_surface_audit(phase121_audit)
    phase123_audit: debug.Phase123NextPlateauAudit = build_phase123_next_plateau_audit(phase122_audit)
    if !debug.validate_phase117_init_orchestrated_multi_service_bring_up(phase117_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return Phase117To123ProofResult{ status: 52, phase123_audit: phase123_audit }
    }
    if !inputs.phase118_probe_succeeded {
        return Phase117To123ProofResult{ status: 53, phase123_audit: phase123_audit }
    }
    if !debug.validate_phase118_request_reply_and_delegation_follow_through(phase118_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return Phase117To123ProofResult{ status: 54, phase123_audit: phase123_audit }
    }
    if !debug.validate_phase119_namespace_pressure_audit(phase119_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return Phase117To123ProofResult{ status: 55, phase123_audit: phase123_audit }
    }
    if !debug.validate_phase120_running_system_support_statement(phase120_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return Phase117To123ProofResult{ status: 56, phase123_audit: phase123_audit }
    }
    if !debug.validate_phase121_kernel_image_contract_hardening(phase121_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return Phase117To123ProofResult{ status: 57, phase123_audit: phase123_audit }
    }
    if !debug.validate_phase122_target_surface_audit(phase122_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return Phase117To123ProofResult{ status: 58, phase123_audit: phase123_audit }
    }
    if !debug.validate_phase123_next_plateau_audit(phase123_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return Phase117To123ProofResult{ status: 59, phase123_audit: phase123_audit }
    }

    return Phase117To123ProofResult{ status: 0, phase123_audit: phase123_audit }
}

func build_phase124_delegation_chain_audit(context: LatePhaseProofContext, transfer_config: bootstrap_services.TransferServiceConfig, phase123_audit: debug.Phase123NextPlateauAudit) debug.Phase124DelegationChainAudit {
    return bootstrap_audit.build_phase124_delegation_chain_audit(bootstrap_audit.Phase124DelegationChainAuditInputs{ phase123: phase123_audit, delegator_pid: context.init_pid, intermediary_pid: context.phase124_intermediary_pid, final_holder_pid: context.phase124_final_holder_pid, control_endpoint_id: context.init_endpoint_id, delegated_endpoint_id: context.transfer_endpoint_id, delegator_source_handle_slot: transfer_config.init_received_handle_slot, intermediary_receive_handle_slot: context.phase124_intermediary_receive_handle_slot, final_receive_handle_slot: context.phase124_final_receive_handle_slot, first_invalidated_send_status: LATE_PHASE_PROOF_OUTPUTS.phase124_delegator_invalidated_send_status, second_invalidated_send_status: LATE_PHASE_PROOF_OUTPUTS.phase124_intermediary_invalidated_send_status, final_send_status: LATE_PHASE_PROOF_OUTPUTS.phase124_final_send_status, final_send_source_pid: LATE_PHASE_PROOF_OUTPUTS.phase124_final_send_source_pid, final_endpoint_queue_depth: LATE_PHASE_PROOF_OUTPUTS.phase124_final_endpoint_queue_depth, ambient_authority_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase125_invalidation_audit(context: LatePhaseProofContext, phase124_audit: debug.Phase124DelegationChainAudit) debug.Phase125InvalidationAudit {
    return bootstrap_audit.build_phase125_invalidation_audit(bootstrap_audit.Phase125InvalidationAuditInputs{ phase124: phase124_audit, invalidated_holder_pid: context.phase124_final_holder_pid, control_endpoint_id: context.init_endpoint_id, invalidated_endpoint_id: context.transfer_endpoint_id, invalidated_handle_slot: context.phase124_final_receive_handle_slot, rejected_send_status: LATE_PHASE_PROOF_OUTPUTS.phase125_rejected_send_status, rejected_receive_status: LATE_PHASE_PROOF_OUTPUTS.phase125_rejected_receive_status, surviving_control_send_status: LATE_PHASE_PROOF_OUTPUTS.phase125_surviving_control_send_status, surviving_control_source_pid: LATE_PHASE_PROOF_OUTPUTS.phase125_surviving_control_source_pid, surviving_control_queue_depth: LATE_PHASE_PROOF_OUTPUTS.phase125_surviving_control_queue_depth, authority_loss_visible: 1, broader_revocation_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase126_authority_lifetime_audit(context: LatePhaseProofContext, phase125_audit: debug.Phase125InvalidationAudit) debug.Phase126AuthorityLifetimeAudit {
    return bootstrap_audit.build_phase126_authority_lifetime_audit(bootstrap_audit.Phase126AuthorityLifetimeAuditInputs{ phase125: phase125_audit, classified_holder_pid: context.phase124_final_holder_pid, long_lived_endpoint_id: context.init_endpoint_id, short_lived_endpoint_id: context.transfer_endpoint_id, long_lived_handle_slot: context.phase124_control_handle_slot, short_lived_handle_slot: context.phase124_final_receive_handle_slot, repeat_long_lived_send_status: LATE_PHASE_PROOF_OUTPUTS.phase126_repeat_long_lived_send_status, repeat_long_lived_source_pid: LATE_PHASE_PROOF_OUTPUTS.phase126_repeat_long_lived_source_pid, repeat_long_lived_queue_depth: LATE_PHASE_PROOF_OUTPUTS.phase126_repeat_long_lived_queue_depth, long_lived_class_visible: 1, short_lived_class_visible: 1, broader_lifetime_framework_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase128_service_death_observation_audit(phase126_audit: debug.Phase126AuthorityLifetimeAudit) debug.Phase128ServiceDeathObservationAudit {
    return bootstrap_audit.build_phase128_service_death_observation_audit(bootstrap_audit.Phase128ServiceDeathObservationAuditInputs{ phase126: phase126_audit, observed_service_pid: LATE_PHASE_PROOF_OUTPUTS.phase128_observed_service_pid, observed_service_key: LATE_PHASE_PROOF_OUTPUTS.phase128_observed_service_key, observed_wait_handle_slot: LATE_PHASE_PROOF_OUTPUTS.phase128_observed_wait_handle_slot, observed_exit_code: LATE_PHASE_PROOF_OUTPUTS.phase128_observed_exit_code, fixed_directory_entry_count: 3, service_death_visible: 1, kernel_supervision_visible: 0, service_restart_visible: 0, broader_failure_framework_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase129_partial_failure_propagation_audit(context: LatePhaseProofContext, log_config: bootstrap_services.LogServiceConfig, echo_config: bootstrap_services.EchoServiceConfig, phase128_audit: debug.Phase128ServiceDeathObservationAudit) debug.Phase129PartialFailurePropagationAudit {
    return bootstrap_audit.build_phase129_partial_failure_propagation_audit(bootstrap_audit.Phase129PartialFailurePropagationAuditInputs{ phase128: phase128_audit, failed_service_pid: LATE_PHASE_PROOF_OUTPUTS.phase129_failed_service_pid, failed_service_key: context.log_service_directory_key, failed_wait_handle_slot: log_config.wait_handle_slot, failed_wait_status: LATE_PHASE_PROOF_OUTPUTS.phase129_failed_wait_status, surviving_service_pid: LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_service_pid, surviving_service_key: context.echo_service_directory_key, surviving_wait_handle_slot: echo_config.wait_handle_slot, surviving_reply_status: LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_reply_status, surviving_wait_status: LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_wait_status, surviving_reply_byte0: LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_reply_byte0, surviving_reply_byte1: LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_reply_byte1, shared_control_endpoint_id: context.init_endpoint_id, directory_entry_count: 3, partial_failure_visible: 1, kernel_recovery_visible: 0, service_rebinding_visible: 0, broader_failure_framework_visible: 0, compiler_reopening_visible: 0 })
}

func build_phase130_explicit_restart_or_replacement_audit(context: LatePhaseProofContext, log_config: bootstrap_services.LogServiceConfig, phase129_audit: debug.Phase129PartialFailurePropagationAudit) debug.Phase130ExplicitRestartOrReplacementAudit {
    return bootstrap_audit.build_phase130_explicit_restart_or_replacement_audit(bootstrap_audit.Phase130ExplicitRestartOrReplacementAuditInputs{ phase129: phase129_audit, replacement_policy_owner_pid: context.init_pid, replacement_service_pid: LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_service_pid, replacement_service_key: context.log_service_directory_key, replacement_wait_handle_slot: log_config.wait_handle_slot, replacement_program_slot: log_config.program_slot, replacement_program_object_id: LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_program_object_id, replacement_spawn_status: LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_spawn_status, replacement_ack_status: LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_ack_status, replacement_wait_status: LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_wait_status, replacement_ack_byte: LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_ack_byte, shared_control_endpoint_id: context.init_endpoint_id, directory_entry_count: 3, explicit_restart_or_replacement_visible: 1, kernel_supervision_visible: 0, service_rebinding_visible: 0, broader_failure_framework_visible: 0, compiler_reopening_visible: 0 })
}

func execute_phase124_to_130_proofs(context: LatePhaseProofContext, transfer_config: bootstrap_services.TransferServiceConfig, log_config: bootstrap_services.LogServiceConfig, echo_config: bootstrap_services.EchoServiceConfig, phase123_audit: debug.Phase123NextPlateauAudit, log_service_spawn_observation: syscall.SpawnObservation, log_service_wait_observation: syscall.WaitObservation, log_execution_state: bootstrap_services.LogServiceExecutionState, contract_flags: ProofContractFlags) i32 {
    if !execute_phase124_delegation_chain_probe(context, transfer_config) {
        return 60
    }
    phase124_audit: debug.Phase124DelegationChainAudit = build_phase124_delegation_chain_audit(context, transfer_config, phase123_audit)
    if !debug.validate_phase124_delegation_chain_stress(phase124_audit, contract_flags.scheduler_contract_hardened, contract_flags.lifecycle_contract_hardened, contract_flags.capability_contract_hardened, contract_flags.ipc_contract_hardened, contract_flags.address_space_contract_hardened, contract_flags.interrupt_contract_hardened, contract_flags.timer_contract_hardened, contract_flags.barrier_contract_hardened) {
        return 61
    }
    if !execute_phase125_invalidation_probe(context, transfer_config) {
        return 62
    }
    phase125_audit: debug.Phase125InvalidationAudit = build_phase125_invalidation_audit(context, phase124_audit)
    if !debug.validate_phase125_invalidation_and_rejection_audit(phase125_audit, contract_flags.scheduler_contract_hardened, contract_flags.lifecycle_contract_hardened, contract_flags.capability_contract_hardened, contract_flags.ipc_contract_hardened, contract_flags.address_space_contract_hardened, contract_flags.interrupt_contract_hardened, contract_flags.timer_contract_hardened, contract_flags.barrier_contract_hardened) {
        return 63
    }
    if !execute_phase126_authority_lifetime_probe(context, transfer_config) {
        return 64
    }
    phase126_audit: debug.Phase126AuthorityLifetimeAudit = build_phase126_authority_lifetime_audit(context, phase125_audit)
    if !debug.validate_phase126_authority_lifetime_classification(phase126_audit, contract_flags.scheduler_contract_hardened, contract_flags.lifecycle_contract_hardened, contract_flags.capability_contract_hardened, contract_flags.ipc_contract_hardened, contract_flags.address_space_contract_hardened, contract_flags.interrupt_contract_hardened, contract_flags.timer_contract_hardened, contract_flags.barrier_contract_hardened) {
        return 65
    }
    if !execute_phase128_service_death_observation_probe(context, log_config, log_service_spawn_observation, log_service_wait_observation) {
        return 66
    }
    phase128_audit: debug.Phase128ServiceDeathObservationAudit = build_phase128_service_death_observation_audit(phase126_audit)
    if !debug.validate_phase128_service_death_observation(phase128_audit, contract_flags.scheduler_contract_hardened, contract_flags.lifecycle_contract_hardened, contract_flags.capability_contract_hardened, contract_flags.ipc_contract_hardened, contract_flags.address_space_contract_hardened, contract_flags.interrupt_contract_hardened, contract_flags.timer_contract_hardened, contract_flags.barrier_contract_hardened) {
        return 67
    }
    if !execute_phase129_partial_failure_propagation_probe(log_config, log_execution_state, echo_config) {
        return 68
    }
    phase129_audit: debug.Phase129PartialFailurePropagationAudit = build_phase129_partial_failure_propagation_audit(context, log_config, echo_config, phase128_audit)
    if !debug.validate_phase129_partial_failure_propagation(phase129_audit, contract_flags.scheduler_contract_hardened, contract_flags.lifecycle_contract_hardened, contract_flags.capability_contract_hardened, contract_flags.ipc_contract_hardened, contract_flags.address_space_contract_hardened, contract_flags.interrupt_contract_hardened, contract_flags.timer_contract_hardened, contract_flags.barrier_contract_hardened) {
        return 69
    }
    if !execute_phase130_explicit_restart_or_replacement_probe(log_config, log_execution_state) {
        return 70
    }
    phase130_audit: debug.Phase130ExplicitRestartOrReplacementAudit = build_phase130_explicit_restart_or_replacement_audit(context, log_config, phase129_audit)
    if !debug.validate_phase130_explicit_restart_or_replacement(phase130_audit, contract_flags.scheduler_contract_hardened, contract_flags.lifecycle_contract_hardened, contract_flags.capability_contract_hardened, contract_flags.ipc_contract_hardened, contract_flags.address_space_contract_hardened, contract_flags.interrupt_contract_hardened, contract_flags.timer_contract_hardened, contract_flags.barrier_contract_hardened) {
        return 71
    }
    LATE_PHASE_PROOF_OUTPUTS.phase130_explicit_restart_audit = phase130_audit
    return 0
}

func build_phase131_fan_out_composition_audit(context: LatePhaseProofContext, composition_config: bootstrap_services.CompositionServiceConfig, phase130_audit: debug.Phase130ExplicitRestartOrReplacementAudit) debug.Phase131FanOutCompositionAudit {
    return bootstrap_audit.build_phase131_fan_out_composition_audit(bootstrap_audit.Phase131FanOutCompositionAuditInputs{ phase130: phase130_audit, composition_policy_owner_pid: context.init_pid, composition_service_pid: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.wait_observation.child_pid, composition_service_key: context.composition_service_directory_key, composition_wait_handle_slot: composition_config.wait_handle_slot, fixed_directory_entry_count: 4, control_endpoint_id: composition_config.control_endpoint_id, echo_endpoint_id: composition_config.echo_endpoint_id, log_endpoint_id: composition_config.log_endpoint_id, request_receive_status: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.request_receive_observation.status, echo_fanout_status: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.echo_fanout_observation.status, echo_fanout_endpoint_id: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.echo_fanout_observation.endpoint_id, log_fanout_status: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.log_fanout_observation.status, log_fanout_endpoint_id: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.log_fanout_observation.endpoint_id, echo_reply_status: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.echo_reply_observation.status, log_ack_status: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.log_ack_observation.status, aggregate_reply_status: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.aggregate_reply_observation.status, composition_wait_status: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.wait_observation.status, aggregate_reply_byte0: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.observation.aggregate_reply_byte0, aggregate_reply_byte1: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.observation.aggregate_reply_byte1, aggregate_reply_byte2: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.observation.aggregate_reply_byte2, aggregate_reply_byte3: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.observation.aggregate_reply_byte3, explicit_composition_visible: 1, kernel_broker_visible: 0, dynamic_namespace_visible: 0, compiler_reopening_visible: 0 })
}

func build_composition_service_execution_state(gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, init_handle_table: capability.HandleTable, child_handle_table: capability.HandleTable, wait_table: capability.WaitTable, endpoints: ipc.EndpointTable, init_image: init.InitImage) bootstrap_services.CompositionServiceExecutionState {
    return bootstrap_services.CompositionServiceExecutionState{ program_capability: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.program_capability, gate: gate, process_slots: process_slots, task_slots: task_slots, init_handle_table: init_handle_table, child_handle_table: child_handle_table, wait_table: wait_table, endpoints: endpoints, init_image: init_image, child_address_space: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.child_address_space, child_user_frame: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.child_user_frame, echo_peer_state: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.echo_peer_state, log_peer_state: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.log_peer_state, spawn_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.spawn_observation, request_receive_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.request_receive_observation, echo_fanout_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.echo_fanout_observation, echo_peer_receive_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.echo_peer_receive_observation, echo_reply_send_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.echo_reply_send_observation, echo_reply_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.echo_reply_observation, log_fanout_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.log_fanout_observation, log_peer_receive_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.log_peer_receive_observation, log_ack_send_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.log_ack_send_observation, log_ack_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.log_ack_observation, aggregate_reply_send_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.aggregate_reply_send_observation, aggregate_reply_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.aggregate_reply_observation, wait_observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.wait_observation, observation: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.observation, ready_queue: LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.ready_queue }
}

func serial_execution_with_endpoints(execution: bootstrap_services.SerialServiceExecutionState, endpoints: ipc.EndpointTable) bootstrap_services.SerialServiceExecutionState {
    return bootstrap_services.SerialServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, failure_observation: execution.failure_observation, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func phase131_composition_probe_succeeded() bool {
    if LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.observation.outbound_edge_count != 2 {
        return false
    }
    if LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.observation.aggregate_reply_count != 1 {
        return false
    }
    return LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state.observation.aggregate_reply_byte3 == 2
}

func execute_phase131_to_137_proofs(inputs: Phase131To137ProofInputs) i32 {
    composition_result: bootstrap_services.CompositionServiceExecutionResult = bootstrap_services.execute_phase131_fan_out_composition(inputs.context.composition_config, build_composition_service_execution_state(inputs.context.gate, inputs.context.process_slots, inputs.context.task_slots, inputs.context.init_handle_table, inputs.context.child_handle_table, inputs.context.wait_table, inputs.context.endpoints, inputs.context.init_image))
    LATE_PHASE_PROOF_OUTPUTS.phase131_composition_state = composition_result.state
    if composition_result.succeeded == 0 {
        return 72
    }
    if !phase131_composition_probe_succeeded() {
        return 72
    }

    phase131_audit: debug.Phase131FanOutCompositionAudit = build_phase131_fan_out_composition_audit(inputs.context.context, inputs.context.composition_config, inputs.context.phase130_audit)
    if !debug.validate_phase131_fan_out_composition(phase131_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return 73
    }

    local_serial_state: bootstrap_services.SerialServiceExecutionState = bootstrap_services.seed_serial_service_program_capability(inputs.context.serial_config, inputs.context.serial_execution_state)
    if !capability.is_program_capability(local_serial_state.program_capability) {
        return 74
    }
    serial_spawn: bootstrap_services.SerialServiceExecutionResult = bootstrap_services.spawn_serial_service(inputs.context.serial_config, local_serial_state)
    local_serial_state = serial_spawn.state
    if serial_spawn.succeeded == 0 {
        return 75
    }
    if !ipc.validate_runtime_frame_copy_boundary() {
        return 76
    }

    local_interrupts: interrupt.InterruptController = inputs.context.interrupts
    local_last_interrupt_kind: interrupt.InterruptDispatchKind = interrupt.InterruptDispatchKind.None
    local_uart_device: uart.UartDevice = uart.configure_receive(inputs.context.uart_device, inputs.context.uart_receive_vector, inputs.context.serial_config.serial_endpoint_id)

    malformed_uart_payload: [4]u8 = ipc.zero_payload()
    malformed_uart_payload[0] = 255
    local_uart_device = uart.stage_receive_frame(local_uart_device, 1, malformed_uart_payload)
    malformed_uart_entry: interrupt.InterruptEntry = interrupt.arch_enter_interrupt(local_interrupts, inputs.context.uart_receive_vector, inputs.context.uart_source_actor)
    malformed_uart_dispatch: interrupt.InterruptDispatchResult = interrupt.dispatch_interrupt(malformed_uart_entry)
    local_interrupts = malformed_uart_dispatch.controller
    local_last_interrupt_kind = malformed_uart_dispatch.kind
    malformed_uart_result: uart.UartInterruptResult = uart.handle_receive_interrupt(local_uart_device, malformed_uart_entry, malformed_uart_dispatch, local_serial_state.endpoints, inputs.context.boot_pid)
    local_uart_device = malformed_uart_result.device
    local_serial_state = serial_execution_with_endpoints(local_serial_state, malformed_uart_result.endpoints)
    if malformed_uart_result.handled == 0 {
        return 77
    }
    if !ipc.runtime_publish_succeeded(malformed_uart_result.observation.publish) {
        return 78
    }

    serial_malformed_receive: bootstrap_services.SerialServiceExecutionResult = bootstrap_services.execute_serial_service_receive(inputs.context.serial_config, local_serial_state)
    local_serial_state = serial_malformed_receive.state
    if serial_malformed_receive.succeeded == 0 {
        return 79
    }

    queue_uart_payload_one: [4]u8 = ipc.zero_payload()
    queue_uart_payload_one[0] = inputs.context.uart_frames.queue_frame_one[0]
    queue_uart_payload_one[1] = inputs.context.uart_frames.queue_frame_one[1]
    local_uart_device = uart.stage_receive_frame(local_uart_device, 2, queue_uart_payload_one)
    queue_uart_entry_one: interrupt.InterruptEntry = interrupt.arch_enter_interrupt(local_interrupts, inputs.context.uart_receive_vector, inputs.context.uart_source_actor)
    queue_uart_dispatch_one: interrupt.InterruptDispatchResult = interrupt.dispatch_interrupt(queue_uart_entry_one)
    local_interrupts = queue_uart_dispatch_one.controller
    local_last_interrupt_kind = queue_uart_dispatch_one.kind
    queue_uart_result_one: uart.UartInterruptResult = uart.handle_receive_interrupt(local_uart_device, queue_uart_entry_one, queue_uart_dispatch_one, local_serial_state.endpoints, inputs.context.boot_pid)
    local_uart_device = queue_uart_result_one.device
    local_serial_state = serial_execution_with_endpoints(local_serial_state, queue_uart_result_one.endpoints)
    if queue_uart_result_one.handled == 0 {
        return 80
    }
    if !ipc.runtime_publish_succeeded(queue_uart_result_one.observation.publish) {
        return 81
    }

    queue_uart_payload_two: [4]u8 = ipc.zero_payload()
    queue_uart_payload_two[0] = inputs.context.uart_frames.queue_frame_two[0]
    queue_uart_payload_two[1] = inputs.context.uart_frames.queue_frame_two[1]
    local_uart_device = uart.stage_receive_frame(local_uart_device, 2, queue_uart_payload_two)
    queue_uart_entry_two: interrupt.InterruptEntry = interrupt.arch_enter_interrupt(local_interrupts, inputs.context.uart_receive_vector, inputs.context.uart_source_actor)
    queue_uart_dispatch_two: interrupt.InterruptDispatchResult = interrupt.dispatch_interrupt(queue_uart_entry_two)
    local_interrupts = queue_uart_dispatch_two.controller
    local_last_interrupt_kind = queue_uart_dispatch_two.kind
    queue_uart_result_two: uart.UartInterruptResult = uart.handle_receive_interrupt(local_uart_device, queue_uart_entry_two, queue_uart_dispatch_two, local_serial_state.endpoints, inputs.context.boot_pid)
    local_uart_device = queue_uart_result_two.device
    local_serial_state = serial_execution_with_endpoints(local_serial_state, queue_uart_result_two.endpoints)
    if queue_uart_result_two.handled == 0 {
        return 82
    }
    if !ipc.runtime_publish_succeeded(queue_uart_result_two.observation.publish) {
        return 83
    }

    queue_uart_payload_three: [4]u8 = ipc.zero_payload()
    queue_uart_payload_three[0] = inputs.context.uart_frames.queue_frame_three[0]
    queue_uart_payload_three[1] = inputs.context.uart_frames.queue_frame_three[1]
    local_uart_device = uart.stage_receive_frame(local_uart_device, 2, queue_uart_payload_three)
    queue_full_uart_entry: interrupt.InterruptEntry = interrupt.arch_enter_interrupt(local_interrupts, inputs.context.uart_receive_vector, inputs.context.uart_source_actor)
    queue_full_uart_dispatch: interrupt.InterruptDispatchResult = interrupt.dispatch_interrupt(queue_full_uart_entry)
    local_interrupts = queue_full_uart_dispatch.controller
    local_last_interrupt_kind = queue_full_uart_dispatch.kind
    queue_full_uart_result: uart.UartInterruptResult = uart.handle_receive_interrupt(local_uart_device, queue_full_uart_entry, queue_full_uart_dispatch, local_serial_state.endpoints, inputs.context.boot_pid)
    local_uart_device = queue_full_uart_result.device
    local_serial_state = serial_execution_with_endpoints(local_serial_state, queue_full_uart_result.endpoints)
    if queue_full_uart_result.handled == 0 {
        return 84
    }
    if queue_full_uart_result.observation.publish.queue_full == 0 {
        return 85
    }

    serial_failure_close: bootstrap_services.SerialServiceExecutionResult = bootstrap_services.close_serial_service_after_failure(inputs.context.serial_config, local_serial_state)
    local_serial_state = serial_failure_close.state
    if serial_failure_close.succeeded == 0 {
        return 86
    }

    closed_uart_payload: [4]u8 = ipc.zero_payload()
    closed_uart_payload[0] = inputs.context.uart_frames.closed_frame[0]
    closed_uart_payload[1] = inputs.context.uart_frames.closed_frame[1]
    local_uart_device = uart.stage_receive_frame(local_uart_device, 2, closed_uart_payload)
    closed_uart_entry: interrupt.InterruptEntry = interrupt.arch_enter_interrupt(local_interrupts, inputs.context.uart_receive_vector, inputs.context.uart_source_actor)
    closed_uart_dispatch: interrupt.InterruptDispatchResult = interrupt.dispatch_interrupt(closed_uart_entry)
    local_interrupts = closed_uart_dispatch.controller
    local_last_interrupt_kind = closed_uart_dispatch.kind
    closed_uart_result: uart.UartInterruptResult = uart.handle_receive_interrupt(local_uart_device, closed_uart_entry, closed_uart_dispatch, local_serial_state.endpoints, inputs.context.boot_pid)
    local_uart_device = closed_uart_result.device
    local_serial_state = serial_execution_with_endpoints(local_serial_state, closed_uart_result.endpoints)
    if closed_uart_result.handled == 0 {
        return 87
    }
    if closed_uart_result.observation.publish.endpoint_closed == 0 {
        return 88
    }

    phase136_audit: debug.Phase136DeviceFailureContainmentAudit = debug.Phase136DeviceFailureContainmentAudit{ phase131: phase131_audit, uart_service_endpoint_id: malformed_uart_result.observation.service_endpoint_id, malformed_interrupt_kind: malformed_uart_dispatch.kind, malformed_dispatch_handled: malformed_uart_dispatch.handled, malformed_published_queued: malformed_uart_result.observation.publish.queued, malformed_published_queue_full: malformed_uart_result.observation.publish.queue_full, malformed_published_endpoint_valid: malformed_uart_result.observation.publish.endpoint_valid, malformed_published_endpoint_closed: malformed_uart_result.observation.publish.endpoint_closed, malformed_serial_service_pid: local_serial_state.ingress.service_pid, malformed_serial_tag: local_serial_state.ingress.tag, malformed_receive_status: local_serial_state.receive_observation.status, malformed_payload_len: local_serial_state.ingress.payload_len, malformed_received_byte: local_serial_state.ingress.received_byte, malformed_ingress_count: local_serial_state.ingress.ingress_count, malformed_service_malformed_count: local_serial_state.ingress.malformed_ingress_count, malformed_log_len: local_serial_state.ingress.log_len, malformed_total_consumed_bytes: local_serial_state.ingress.total_consumed_bytes, queue_one_published_queued: queue_uart_result_one.observation.publish.queued, queue_two_published_queued: queue_uart_result_two.observation.publish.queued, queue_full_dispatch_handled: queue_full_uart_dispatch.handled, queue_full_published_queued: queue_full_uart_result.observation.publish.queued, queue_full_published_queue_full: queue_full_uart_result.observation.publish.queue_full, queue_full_published_endpoint_valid: queue_full_uart_result.observation.publish.endpoint_valid, queue_full_published_endpoint_closed: queue_full_uart_result.observation.publish.endpoint_closed, queue_full_drop_count: queue_full_uart_result.observation.dropped_count, queue_full_failure_kind: queue_full_uart_result.observation.failure_kind, uart_ack_count_after_queue_full: queue_full_uart_result.observation.ack_count, uart_ingress_count_after_queue_full: queue_full_uart_result.observation.ingress_count, uart_retire_count_after_queue_full: queue_full_uart_result.observation.retire_count, close_endpoint_id: local_serial_state.failure_observation.endpoint_id, close_closed: local_serial_state.failure_observation.closed, close_aborted_messages: local_serial_state.failure_observation.aborted_messages, close_wake_count: local_serial_state.failure_observation.wake_count, close_wait_status: local_serial_state.wait_observation.status, closed_dispatch_handled: closed_uart_dispatch.handled, closed_published_queued: closed_uart_result.observation.publish.queued, closed_published_queue_full: closed_uart_result.observation.publish.queue_full, closed_published_endpoint_valid: closed_uart_result.observation.publish.endpoint_valid, closed_published_endpoint_closed: closed_uart_result.observation.publish.endpoint_closed, closed_drop_count: closed_uart_result.observation.dropped_count, closed_endpoint_closed_drop_count: closed_uart_result.observation.endpoint_closed_drop_count, closed_failure_kind: closed_uart_result.observation.failure_kind, kernel_policy_visible: 0, driver_framework_visible: 0, retry_framework_visible: 0, protocol_parsing_in_kernel_visible: 0, compiler_reopening_visible: 0 }
    if !debug.validate_phase136_device_failure_containment(phase136_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return 89
    }

    respawn_serial_state: bootstrap_services.SerialServiceExecutionState = bootstrap_services.seed_serial_service_program_capability(inputs.context.serial_config, local_serial_state)
    serial_respawn: bootstrap_services.SerialServiceExecutionResult = bootstrap_services.spawn_serial_service(inputs.context.serial_config, respawn_serial_state)
    local_serial_state = serial_respawn.state
    if serial_respawn.succeeded == 0 {
        return 90
    }

    local_uart_device = uart.configure_receive(local_uart_device, inputs.context.uart_receive_vector, inputs.context.serial_config.serial_endpoint_id)
    local_uart_device = uart.configure_receive_completion(local_uart_device, inputs.context.uart_completion_vector)
    completion_payload: [4]u8 = ipc.zero_payload()
    completion_payload[0] = inputs.context.uart_frames.completion_frame[0]
    completion_payload[1] = inputs.context.uart_frames.completion_frame[1]
    completion_payload[2] = inputs.context.uart_frames.completion_frame[2]
    completion_payload[3] = inputs.context.uart_frames.completion_frame[3]
    local_uart_device = uart.stage_receive_completion_frame(local_uart_device, 4, completion_payload)
    completion_entry: interrupt.InterruptEntry = interrupt.arch_enter_interrupt(local_interrupts, inputs.context.uart_completion_vector, inputs.context.uart_source_actor)
    completion_dispatch: interrupt.InterruptDispatchResult = interrupt.dispatch_interrupt(completion_entry)
    local_interrupts = completion_dispatch.controller
    local_last_interrupt_kind = completion_dispatch.kind
    completion_result: uart.UartCompletionInterruptResult = uart.handle_receive_completion_interrupt(local_uart_device, completion_entry, completion_dispatch, local_serial_state.endpoints, inputs.context.boot_pid)
    local_uart_device = completion_result.device
    local_serial_state = serial_execution_with_endpoints(local_serial_state, completion_result.endpoints)
    if completion_result.handled == 0 {
        return 91
    }
    if !ipc.runtime_publish_succeeded(completion_result.observation.publish) {
        return 92
    }

    serial_completion_receive: bootstrap_services.SerialServiceExecutionResult = bootstrap_services.execute_serial_service_receive(inputs.context.serial_config, local_serial_state)
    local_serial_state = serial_completion_receive.state
    if serial_completion_receive.succeeded == 0 {
        return 93
    }

    phase137_audit: debug.Phase137OptionalDmaOrEquivalentAudit = debug.Phase137OptionalDmaOrEquivalentAudit{ phase136: phase136_audit, completion_interrupt_kind: completion_dispatch.kind, completion_dispatch_handled: completion_dispatch.handled, completion_endpoint_id: completion_result.observation.service_endpoint_id, completion_staged_payload_len: completion_result.observation.staged_payload_len, completion_staged_payload0: completion_result.observation.staged_payload0, completion_staged_payload1: completion_result.observation.staged_payload1, completion_staged_payload2: completion_result.observation.staged_payload2, completion_staged_payload3: completion_result.observation.staged_payload3, completion_published_payload_len: completion_result.observation.published_payload_len, completion_published_payload0: completion_result.observation.published_payload0, completion_published_payload1: completion_result.observation.published_payload1, completion_published_payload2: completion_result.observation.published_payload2, completion_published_payload3: completion_result.observation.published_payload3, completion_retired_payload_len: completion_result.observation.retired_payload_len, completion_retired_payload0: completion_result.observation.retired_payload0, completion_retired_payload1: completion_result.observation.retired_payload1, completion_retired_payload2: completion_result.observation.retired_payload2, completion_retired_payload3: completion_result.observation.retired_payload3, completion_publish_queued: completion_result.observation.publish.queued, completion_publish_queue_full: completion_result.observation.publish.queue_full, completion_publish_endpoint_valid: completion_result.observation.publish.endpoint_valid, completion_publish_endpoint_closed: completion_result.observation.publish.endpoint_closed, completion_ingress_count: completion_result.observation.completion_ingress_count, completion_retire_count: completion_result.observation.completion_retire_count, serial_service_pid: local_serial_state.ingress.service_pid, serial_receive_status: local_serial_state.receive_observation.status, serial_tag: local_serial_state.ingress.tag, serial_payload_len: local_serial_state.ingress.payload_len, serial_received_byte: local_serial_state.ingress.received_byte, serial_ingress_count: local_serial_state.ingress.ingress_count, serial_log_len: local_serial_state.ingress.log_len, serial_total_consumed_bytes: local_serial_state.ingress.total_consumed_bytes, serial_log_byte0: local_serial_state.ingress.log_byte0, serial_log_byte1: local_serial_state.ingress.log_byte1, serial_log_byte2: local_serial_state.ingress.log_byte2, serial_log_byte3: local_serial_state.ingress.log_byte3, dma_manager_visible: 0, descriptor_framework_visible: 0, compiler_reopening_visible: 0 }
    if !debug.validate_phase137_optional_dma_or_equivalent_follow_through(phase137_audit, inputs.contract_flags.scheduler_contract_hardened, inputs.contract_flags.lifecycle_contract_hardened, inputs.contract_flags.capability_contract_hardened, inputs.contract_flags.ipc_contract_hardened, inputs.contract_flags.address_space_contract_hardened, inputs.contract_flags.interrupt_contract_hardened, inputs.contract_flags.timer_contract_hardened, inputs.contract_flags.barrier_contract_hardened) {
        return 94
    }

    LATE_PHASE_PROOF_OUTPUTS.phase137_serial_state = local_serial_state
    LATE_PHASE_PROOF_OUTPUTS.phase137_interrupt_controller = local_interrupts
    LATE_PHASE_PROOF_OUTPUTS.phase137_last_interrupt_kind = local_last_interrupt_kind
    LATE_PHASE_PROOF_OUTPUTS.phase137_uart_device = local_uart_device
    LATE_PHASE_PROOF_OUTPUTS.phase137_uart_ingress = closed_uart_result.observation
    LATE_PHASE_PROOF_OUTPUTS.phase137_optional_dma_audit = phase137_audit
    return 0
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
        panic(1251)
    }

    first_receive: syscall.ReceiveResult = syscall.perform_receive(local_gate, intermediary_table, local_endpoints, syscall.build_transfer_receive_request(context.phase124_control_handle_slot, context.phase124_intermediary_receive_handle_slot))
    intermediary_table = first_receive.handle_table
    local_endpoints = first_receive.endpoints
    local_gate = first_receive.gate
    if syscall.status_score(first_receive.observation.status) != 2 {
        panic(1252)
    }
    if capability.find_endpoint_for_handle(intermediary_table, context.phase124_intermediary_receive_handle_slot) != context.transfer_endpoint_id {
        return false
    }

    init_invalidated_send: syscall.SendResult = syscall.perform_send(local_gate, init_table, local_endpoints, context.init_pid, syscall.build_send_request(transfer_config.init_received_handle_slot, 4, grant_payload))
    LATE_PHASE_PROOF_OUTPUTS.phase124_delegator_invalidated_send_status = init_invalidated_send.status
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase124_delegator_invalidated_send_status) != 8 {
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
    LATE_PHASE_PROOF_OUTPUTS.phase124_intermediary_invalidated_send_status = intermediary_invalidated_send.status
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase124_intermediary_invalidated_send_status) != 8 {
        return false
    }

    final_payload: [4]u8 = ipc.zero_payload()
    final_payload[0] = 72
    final_payload[1] = 79
    final_payload[2] = 80
    final_payload[3] = 50
    final_send: syscall.SendResult = syscall.perform_send(local_gate, final_table, local_endpoints, context.phase124_final_holder_pid, syscall.build_send_request(context.phase124_final_receive_handle_slot, 4, final_payload))
    LATE_PHASE_PROOF_OUTPUTS.phase124_final_send_status = final_send.status
    LATE_PHASE_PROOF_OUTPUTS.phase124_final_send_source_pid = final_send.endpoints.slots[1].messages[0].source_pid
    LATE_PHASE_PROOF_OUTPUTS.phase124_final_endpoint_queue_depth = final_send.endpoints.slots[1].queued_messages
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase124_final_send_status) != 2 {
        return false
    }
    return LATE_PHASE_PROOF_OUTPUTS.phase124_final_endpoint_queue_depth == 1
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
        panic(1253)
    }

    invalidated_final_table: capability.HandleTable = capability.remove_handle(final_table, context.phase124_final_receive_handle_slot)
    if !capability.handle_remove_succeeded(final_table, invalidated_final_table, context.phase124_final_receive_handle_slot) {
        panic(1254)
    }
    final_table = invalidated_final_table

    rejected_send: syscall.SendResult = syscall.perform_send(local_gate, final_table, local_endpoints, context.phase124_final_holder_pid, syscall.build_send_request(context.phase124_final_receive_handle_slot, 4, grant_payload))
    LATE_PHASE_PROOF_OUTPUTS.phase125_rejected_send_status = rejected_send.status
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase125_rejected_send_status) != 8 {
        panic(1255)
    }

    rejected_receive: syscall.ReceiveResult = syscall.perform_receive(local_gate, final_table, local_endpoints, syscall.build_receive_request(context.phase124_final_receive_handle_slot))
    LATE_PHASE_PROOF_OUTPUTS.phase125_rejected_receive_status = rejected_receive.observation.status
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase125_rejected_receive_status) != 8 {
        return false
    }

    control_payload: [4]u8 = ipc.zero_payload()
    control_payload[0] = 76
    control_payload[1] = 79
    control_payload[2] = 83
    control_payload[3] = 84
    surviving_control_send: syscall.SendResult = syscall.perform_send(local_gate, final_table, local_endpoints, context.phase124_final_holder_pid, syscall.build_send_request(context.phase124_control_handle_slot, 4, control_payload))
    LATE_PHASE_PROOF_OUTPUTS.phase125_surviving_control_send_status = surviving_control_send.status
    LATE_PHASE_PROOF_OUTPUTS.phase125_surviving_control_source_pid = surviving_control_send.endpoints.slots[0].messages[0].source_pid
    LATE_PHASE_PROOF_OUTPUTS.phase125_surviving_control_queue_depth = surviving_control_send.endpoints.slots[0].queued_messages
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase125_surviving_control_send_status) != 2 {
        return false
    }
    return LATE_PHASE_PROOF_OUTPUTS.phase125_surviving_control_queue_depth == 1
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
    LATE_PHASE_PROOF_OUTPUTS.phase126_repeat_long_lived_send_status = repeat_control_send.status
    LATE_PHASE_PROOF_OUTPUTS.phase126_repeat_long_lived_source_pid = repeat_control_send.endpoints.slots[0].messages[1].source_pid
    LATE_PHASE_PROOF_OUTPUTS.phase126_repeat_long_lived_queue_depth = repeat_control_send.endpoints.slots[0].queued_messages
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase126_repeat_long_lived_send_status) != 2 {
        return false
    }
    return LATE_PHASE_PROOF_OUTPUTS.phase126_repeat_long_lived_queue_depth == 2
}

func execute_phase128_service_death_observation_probe(context: LatePhaseProofContext, log_config: bootstrap_services.LogServiceConfig, log_spawn_observation: syscall.SpawnObservation, log_wait_observation: syscall.WaitObservation) bool {
    if syscall.status_score(log_wait_observation.status) != 2 {
        return false
    }
    LATE_PHASE_PROOF_OUTPUTS.phase128_observed_service_pid = log_wait_observation.child_pid
    LATE_PHASE_PROOF_OUTPUTS.phase128_observed_service_key = context.log_service_directory_key
    LATE_PHASE_PROOF_OUTPUTS.phase128_observed_wait_handle_slot = log_wait_observation.wait_handle_slot
    LATE_PHASE_PROOF_OUTPUTS.phase128_observed_exit_code = log_wait_observation.exit_code
    if LATE_PHASE_PROOF_OUTPUTS.phase128_observed_service_pid != log_spawn_observation.child_pid {
        return false
    }
    if LATE_PHASE_PROOF_OUTPUTS.phase128_observed_wait_handle_slot != log_config.wait_handle_slot {
        return false
    }
    return LATE_PHASE_PROOF_OUTPUTS.phase128_observed_exit_code == log_config.exit_code
}

func execute_phase129_partial_failure_propagation_probe(log_config: bootstrap_services.LogServiceConfig, log_execution_state: bootstrap_services.LogServiceExecutionState, echo_config: bootstrap_services.EchoServiceConfig) bool {
    log_result: bootstrap_services.LogServiceExecutionResult = bootstrap_services.execute_phase105_log_service_handshake(log_config, log_execution_state)
    if log_result.succeeded == 0 {
        return false
    }
    LATE_PHASE_PROOF_OUTPUTS.phase129_failed_service_pid = log_result.state.wait_observation.child_pid
    LATE_PHASE_PROOF_OUTPUTS.phase129_failed_wait_status = log_result.state.wait_observation.status
    if LATE_PHASE_PROOF_OUTPUTS.phase129_failed_service_pid == 0 {
        return false
    }
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase129_failed_wait_status) != 2 {
        return false
    }

    echo_execution: bootstrap_services.EchoServiceExecutionState = bootstrap_services.EchoServiceExecutionState{ program_capability: capability.empty_slot(), gate: log_result.state.gate, process_slots: log_result.state.process_slots, task_slots: log_result.state.task_slots, init_handle_table: log_result.state.init_handle_table, child_handle_table: log_result.state.child_handle_table, wait_table: log_result.state.wait_table, endpoints: log_result.state.endpoints, init_image: log_result.state.init_image, child_address_space: address_space.empty_space(), child_user_frame: address_space.empty_frame(), service_state: echo_service.service_state(0, 0), spawn_observation: syscall.empty_spawn_observation(), receive_observation: syscall.empty_receive_observation(), reply_observation: syscall.empty_receive_observation(), exchange: echo_service.EchoExchangeObservation{ service_pid: 0, client_pid: 0, endpoint_id: 0, tag: echo_service.EchoMessageTag.None, request_len: 0, request_byte0: 0, request_byte1: 0, reply_len: 0, reply_byte0: 0, reply_byte1: 0, request_count: 0, reply_count: 0 }, wait_observation: syscall.empty_wait_observation(), ready_queue: log_result.state.ready_queue }
    echo_result: bootstrap_services.EchoServiceExecutionResult = bootstrap_services.execute_phase106_echo_service_request_reply(echo_config, echo_execution)
    if echo_result.succeeded == 0 {
        return false
    }
    LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_service_pid = echo_result.state.wait_observation.child_pid
    LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_reply_status = echo_result.state.reply_observation.status
    LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_wait_status = echo_result.state.wait_observation.status
    LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_reply_byte0 = echo_result.state.reply_observation.payload[0]
    LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_reply_byte1 = echo_result.state.reply_observation.payload[1]
    if LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_service_pid == 0 {
        return false
    }
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_reply_status) != 2 {
        return false
    }
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_wait_status) != 2 {
        return false
    }
    if LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_reply_byte0 != echo_config.request_byte0 {
        return false
    }
    return LATE_PHASE_PROOF_OUTPUTS.phase129_surviving_reply_byte1 == echo_config.request_byte1
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
    LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_service_pid = replacement_result.state.wait_observation.child_pid
    LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_spawn_status = replacement_result.state.spawn_observation.status
    LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_ack_status = replacement_result.state.ack_observation.status
    LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_wait_status = replacement_result.state.wait_observation.status
    LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_ack_byte = replacement_result.state.ack_observation.payload[0]
    LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_program_object_id = log_config.program_object_id
    if LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_service_pid == 0 {
        return false
    }
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_spawn_status) != 2 {
        return false
    }
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_ack_status) != 2 {
        return false
    }
    if syscall.status_score(LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_wait_status) != 2 {
        return false
    }
    return LATE_PHASE_PROOF_OUTPUTS.phase130_replacement_ack_byte == log_service.ack_payload()[0]
}