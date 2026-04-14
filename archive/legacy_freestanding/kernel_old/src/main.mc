import address_space
import bootstrap_audit
import bootstrap_proofs
import bootstrap_state
import bootstrap_services
import capability
import debug
import echo_service
import ipc
import init
import interrupt
import lifecycle
import kv_service
import log_service
import mmu
import sched
import shell_service
import serial_service
import state
import syscall
import timer
import transfer_service
import uart

const ARCH_ACTOR: u32 = 0
const BOOT_PID: u32 = 1
const BOOT_TID: u32 = 1
const BOOT_TASK_SLOT: u32 = 0
const INIT_PID: u32 = 2
const INIT_TID: u32 = 2
const INIT_TASK_SLOT: u32 = 1
const INIT_ASID: u32 = 1
const CHILD_PID: u32 = 3
const CHILD_TID: u32 = 3
const CHILD_TASK_SLOT: u32 = 2
const CHILD_ASID: u32 = 2
const INIT_ENDPOINT_ID: u32 = 1
const INIT_ENDPOINT_HANDLE_SLOT: u32 = 1
const CHILD_WAIT_HANDLE_SLOT: u32 = 1
const TRANSFER_ENDPOINT_ID: u32 = 2
const TRANSFER_SOURCE_HANDLE_SLOT: u32 = 2
const TRANSFER_RECEIVED_HANDLE_SLOT: u32 = 3
const KERNEL_MAGIC: u32 = 9701
const BOOT_ENTRY_PC: usize = 4096
const BOOT_STACK_TOP: usize = 8192
const INIT_ROOT_PAGE_TABLE: usize = 32768
const CHILD_ROOT_PAGE_TABLE: usize = 49152
const CHILD_EXIT_CODE: i32 = 41
const PHASE124_MARKER_DETAIL: u32 = 124
const PHASE125_MARKER_DETAIL: u32 = 125
const PHASE126_MARKER_DETAIL: u32 = 126
const PHASE128_MARKER_DETAIL: u32 = 128
const PHASE129_MARKER_DETAIL: u32 = 129
const PHASE130_MARKER_DETAIL: u32 = 130
const PHASE131_MARKER_DETAIL: u32 = 131
const PHASE134_MARKER_DETAIL: u32 = 134
const PHASE135_MARKER_DETAIL: u32 = 135
const PHASE136_MARKER_DETAIL: u32 = 136
const PHASE137_MARKER_DETAIL: u32 = 137
const PHASE140_MARKER_DETAIL: u32 = 140
const PHASE141_MARKER_DETAIL: u32 = 141
const PHASE142_MARKER_DETAIL: u32 = 142
const PHASE143_MARKER_DETAIL: u32 = 143
const PHASE144_MARKER_DETAIL: u32 = 144
const PHASE145_MARKER_DETAIL: u32 = 145
const PHASE146_MARKER_DETAIL: u32 = 146
const PHASE147_MARKER_DETAIL: u32 = 147
const PHASE148_MARKER_DETAIL: u32 = 148
const PHASE149_MARKER_DETAIL: u32 = 149
const PHASE150_MARKER_DETAIL: u32 = 150
const PHASE152_MARKER_DETAIL: u32 = 152
const LOG_SERVICE_DIRECTORY_KEY: u32 = 1
const ECHO_SERVICE_DIRECTORY_KEY: u32 = 2
const TRANSFER_SERVICE_DIRECTORY_KEY: u32 = 3
const COMPOSITION_SERVICE_DIRECTORY_KEY: u32 = 4
const KV_SERVICE_DIRECTORY_KEY: u32 = 6
const SHELL_SERVICE_DIRECTORY_KEY: u32 = 7
const COMPOSITION_ECHO_ENDPOINT_ID: u32 = 3
const COMPOSITION_LOG_ENDPOINT_ID: u32 = 4
const SERIAL_SERVICE_ENDPOINT_ID: u32 = TRANSFER_ENDPOINT_ID
const KV_SERVICE_ENDPOINT_ID: u32 = 5
const SHELL_SERVICE_ENDPOINT_ID: u32 = 6
const PHASE141_SHELL_PID: u32 = 4
const PHASE141_KV_PID: u32 = 5
// Shell command prefixes: "EC", "LA", "LT", "KS", and "KG".
const SHELL_COMMAND_ECHO_BYTE0: u8 = 69
const SHELL_COMMAND_ECHO_BYTE1: u8 = 67
const SHELL_COMMAND_LOG_BYTE0: u8 = 76
const SHELL_COMMAND_LOG_APPEND_BYTE1: u8 = 65
const SHELL_COMMAND_LOG_TAIL_BYTE1: u8 = 84
const SHELL_COMMAND_KV_BYTE0: u8 = 75
const SHELL_COMMAND_KV_SET_BYTE1: u8 = 83
const SHELL_COMMAND_KV_GET_BYTE1: u8 = 71
const UART_RECEIVE_VECTOR: u32 = 33
const UART_COMPLETION_VECTOR: u32 = 34
const UART_SOURCE_ACTOR: u32 = 134
// UART ingress frames: "Q1", "Q2", "Q3", "DE", and "DMA!".
const UART_QUEUE_FRAME_ONE_BYTE0: u8 = 81
const UART_QUEUE_FRAME_ONE_BYTE1: u8 = 49
const UART_QUEUE_FRAME_TWO_BYTE0: u8 = 81
const UART_QUEUE_FRAME_TWO_BYTE1: u8 = 50
const UART_QUEUE_FRAME_THREE_BYTE0: u8 = 81
const UART_QUEUE_FRAME_THREE_BYTE1: u8 = 51
const UART_CLOSED_FRAME_BYTE0: u8 = 68
const UART_CLOSED_FRAME_BYTE1: u8 = 69
const UART_COMPLETION_FRAME_BYTE0: u8 = 68
const UART_COMPLETION_FRAME_BYTE1: u8 = 77
const UART_COMPLETION_FRAME_BYTE2: u8 = 65
const UART_COMPLETION_FRAME_BYTE3: u8 = 33
const PHASE124_INTERMEDIARY_PID: u32 = 4
const PHASE124_FINAL_HOLDER_PID: u32 = 5
const PHASE124_CONTROL_HANDLE_SLOT: u32 = 1
const PHASE124_INTERMEDIARY_RECEIVE_HANDLE_SLOT: u32 = 2
const PHASE124_FINAL_RECEIVE_HANDLE_SLOT: u32 = 3

struct LogServiceSnapshot {
    program_capability: capability.CapabilitySlot
    address_space: address_space.AddressSpace
    user_frame: address_space.UserEntryFrame
    state: log_service.LogServiceState
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    ack_observation: syscall.ReceiveObservation
    handshake: log_service.LogHandshakeObservation
    wait_observation: syscall.WaitObservation
}

struct EchoServiceSnapshot {
    program_capability: capability.CapabilitySlot
    address_space: address_space.AddressSpace
    user_frame: address_space.UserEntryFrame
    state: echo_service.EchoServiceState
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    reply_observation: syscall.ReceiveObservation
    exchange: echo_service.EchoExchangeObservation
    wait_observation: syscall.WaitObservation
}

struct TransferServiceSnapshot {
    program_capability: capability.CapabilitySlot
    address_space: address_space.AddressSpace
    user_frame: address_space.UserEntryFrame
    state: transfer_service.TransferServiceState
    spawn_observation: syscall.SpawnObservation
    grant_observation: syscall.ReceiveObservation
    emit_observation: syscall.ReceiveObservation
    transfer: transfer_service.TransferObservation
    wait_observation: syscall.WaitObservation
}

struct SerialServiceSnapshot {
    program_capability: capability.CapabilitySlot
    address_space: address_space.AddressSpace
    user_frame: address_space.UserEntryFrame
    state: serial_service.SerialServiceState
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    failure_observation: bootstrap_services.SerialServiceFailureObservation
    wait_observation: syscall.WaitObservation
}

struct InitRuntimeSnapshot {
    bootstrap_caps: init.BootstrapCapabilitySet
    bootstrap_handoff: init.BootstrapHandoffObservation
    address_space: address_space.AddressSpace
    user_frame: address_space.UserEntryFrame
}

struct ChildExecutionSnapshot {
    translation_root: mmu.TranslationRoot
    address_space: address_space.AddressSpace
    user_frame: address_space.UserEntryFrame
    spawn_observation: syscall.SpawnObservation
    sleep_observation: syscall.SleepObservation
    timer_wake_observation: timer.TimerWakeObservation
    wake_ready_queue: state.ReadyQueue
    pre_exit_wait_observation: syscall.WaitObservation
    exit_wait_observation: syscall.WaitObservation
}

struct KernelExecutionContext {
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    init_handle_table: capability.HandleTable
    child_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    endpoints: ipc.EndpointTable
    ready_queue: state.ReadyQueue
}

struct LateBootLogExpectation {
    index: usize
    detail: u32
    stage_error: i32
    actor_error: i32
    detail_error: i32
}

var KERNEL: state.KernelDescriptor
var PROCESS_SLOTS: [3]state.ProcessSlot
var TASK_SLOTS: [3]state.TaskSlot
var READY_QUEUE: state.ReadyQueue
var BOOT_LOG: state.BootLog
var BOOT_LOG_APPEND_FAILED: u32
var ROOT_CAPABILITY: capability.CapabilitySlot
var INIT_PROGRAM_CAPABILITY: capability.CapabilitySlot
var LOG_SERVICE_SNAPSHOT: LogServiceSnapshot
var ECHO_SERVICE_SNAPSHOT: EchoServiceSnapshot
var TRANSFER_SERVICE_SNAPSHOT: TransferServiceSnapshot
var HANDLE_TABLES: [3]capability.HandleTable
var WAIT_TABLES: [3]capability.WaitTable
var ENDPOINTS: ipc.EndpointTable
var INTERRUPTS: interrupt.InterruptController
var LAST_INTERRUPT_KIND: interrupt.InterruptDispatchKind
var UART_DEVICE: uart.UartDevice
var INIT_RUNTIME_SNAPSHOT: InitRuntimeSnapshot
var CHILD_EXECUTION_SNAPSHOT: ChildExecutionSnapshot
var SYSCALL_GATE: syscall.SyscallGate
var INIT_IMAGE: init.InitImage
var TIMER_STATE: timer.TimerState
var DELIVERED_MESSAGE: ipc.KernelMessage
var RECEIVE_OBSERVATION: syscall.ReceiveObservation
var ATTACHED_RECEIVE_OBSERVATION: syscall.ReceiveObservation
var TRANSFERRED_HANDLE_USE_OBSERVATION: syscall.ReceiveObservation
var SERIAL_SERVICE_SNAPSHOT: SerialServiceSnapshot
var UART_INGRESS: uart.UartIngressObservation

func empty_log_service_snapshot() LogServiceSnapshot {
    return LogServiceSnapshot{ program_capability: capability.empty_slot(), address_space: address_space.empty_space(), user_frame: address_space.empty_frame(), state: log_service.service_state(0, 0), spawn_observation: syscall.empty_spawn_observation(), receive_observation: syscall.empty_receive_observation(), ack_observation: syscall.empty_receive_observation(), handshake: log_service.LogHandshakeObservation{ service_pid: 0, client_pid: 0, endpoint_id: 0, tag: log_service.LogMessageTag.None, request_len: 0, request_byte: 0, ack_byte: 0, request_count: 0, ack_count: 0 }, wait_observation: syscall.empty_wait_observation() }
}

func empty_echo_service_snapshot() EchoServiceSnapshot {
    return EchoServiceSnapshot{ program_capability: capability.empty_slot(), address_space: address_space.empty_space(), user_frame: address_space.empty_frame(), state: echo_service.service_state(0, 0), spawn_observation: syscall.empty_spawn_observation(), receive_observation: syscall.empty_receive_observation(), reply_observation: syscall.empty_receive_observation(), exchange: echo_service.EchoExchangeObservation{ service_pid: 0, client_pid: 0, endpoint_id: 0, tag: echo_service.EchoMessageTag.None, request_len: 0, request_byte0: 0, request_byte1: 0, reply_len: 0, reply_byte0: 0, reply_byte1: 0, request_count: 0, reply_count: 0 }, wait_observation: syscall.empty_wait_observation() }
}

func empty_transfer_service_snapshot() TransferServiceSnapshot {
    return TransferServiceSnapshot{ program_capability: capability.empty_slot(), address_space: address_space.empty_space(), user_frame: address_space.empty_frame(), state: transfer_service.service_state(0, 0, 0), spawn_observation: syscall.empty_spawn_observation(), grant_observation: syscall.empty_receive_observation(), emit_observation: syscall.empty_receive_observation(), transfer: transfer_service.TransferObservation{ service_pid: 0, client_pid: 0, control_endpoint_id: 0, transferred_endpoint_id: 0, transferred_rights: 0, tag: transfer_service.CapabilityTransferTag.None, grant_len: 0, grant_byte0: 0, grant_byte1: 0, grant_byte2: 0, grant_byte3: 0, emit_len: 0, emit_byte0: 0, emit_byte1: 0, emit_byte2: 0, emit_byte3: 0, grant_count: 0, emit_count: 0 }, wait_observation: syscall.empty_wait_observation() }
}

func empty_serial_service_snapshot() SerialServiceSnapshot {
    return SerialServiceSnapshot{ program_capability: capability.empty_slot(), address_space: address_space.empty_space(), user_frame: address_space.empty_frame(), state: serial_service.service_state(0, 0), spawn_observation: syscall.empty_spawn_observation(), receive_observation: syscall.empty_receive_observation(), failure_observation: bootstrap_services.empty_serial_service_failure_observation(), wait_observation: syscall.empty_wait_observation() }
}

func empty_init_runtime_snapshot() InitRuntimeSnapshot {
    return InitRuntimeSnapshot{ bootstrap_caps: init.empty_bootstrap_capability_set(), bootstrap_handoff: init.BootstrapHandoffObservation{ owner_pid: 0, authority_count: 0, endpoint_handle_slot: 0, program_capability_slot: 0, program_object_id: 0, ambient_root_visible: 0 }, address_space: address_space.empty_space(), user_frame: address_space.empty_frame() }
}

func empty_child_execution_snapshot() ChildExecutionSnapshot {
    return ChildExecutionSnapshot{ translation_root: mmu.empty_translation_root(), address_space: address_space.empty_space(), user_frame: address_space.empty_frame(), spawn_observation: syscall.empty_spawn_observation(), sleep_observation: syscall.empty_sleep_observation(), timer_wake_observation: timer.TimerWakeObservation{ task_id: 0, deadline_tick: 0, wake_tick: 0, wake_count: 0 }, wake_ready_queue: state.empty_queue(), pre_exit_wait_observation: syscall.empty_wait_observation(), exit_wait_observation: syscall.empty_wait_observation() }
}

func reset_kernel_state() {
    KERNEL = state.empty_descriptor()
    PROCESS_SLOTS = state.zero_process_slots()
    TASK_SLOTS = state.zero_task_slots()
    READY_QUEUE = state.empty_queue()
    BOOT_LOG = state.empty_log()
    BOOT_LOG_APPEND_FAILED = 0
    ROOT_CAPABILITY = capability.empty_slot()
    INIT_PROGRAM_CAPABILITY = capability.empty_slot()
    LOG_SERVICE_SNAPSHOT = empty_log_service_snapshot()
    ECHO_SERVICE_SNAPSHOT = empty_echo_service_snapshot()
    TRANSFER_SERVICE_SNAPSHOT = empty_transfer_service_snapshot()
    HANDLE_TABLES = capability.zero_handle_tables()
    WAIT_TABLES = capability.zero_wait_tables()
    ENDPOINTS = ipc.empty_table()
    INTERRUPTS = interrupt.reset_controller()
    LAST_INTERRUPT_KIND = interrupt.InterruptDispatchKind.None
    UART_DEVICE = uart.empty_device()
    INIT_RUNTIME_SNAPSHOT = empty_init_runtime_snapshot()
    CHILD_EXECUTION_SNAPSHOT = empty_child_execution_snapshot()
    SYSCALL_GATE = syscall.gate_closed()
    INIT_IMAGE = init.bootstrap_image()
    TIMER_STATE = timer.empty_timer_state()
    DELIVERED_MESSAGE = ipc.empty_message()
    RECEIVE_OBSERVATION = syscall.empty_receive_observation()
    ATTACHED_RECEIVE_OBSERVATION = syscall.empty_receive_observation()
    TRANSFERRED_HANDLE_USE_OBSERVATION = syscall.empty_receive_observation()
    SERIAL_SERVICE_SNAPSHOT = empty_serial_service_snapshot()
    UART_INGRESS = uart.empty_ingress_observation()
    bootstrap_proofs.reset_late_phase_proofs()
}

func record_boot_stage(stage_value: state.BootStage, detail: u32) {
    append_result: state.BootLogAppendResult = state.append_record(BOOT_LOG, stage_value, ARCH_ACTOR, detail)
    BOOT_LOG = append_result.log
    if append_result.appended == 0 {
        BOOT_LOG_APPEND_FAILED = 1
    }
}

func shell_command_payload(byte0: u8, byte1: u8, byte2: u8, byte3: u8) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = byte0
    payload[1] = byte1
    payload[2] = byte2
    payload[3] = byte3
    return payload
}

func current_kernel_execution_context() KernelExecutionContext {
    return KernelExecutionContext{ gate: SYSCALL_GATE, process_slots: PROCESS_SLOTS, task_slots: TASK_SLOTS, init_handle_table: HANDLE_TABLES[1], child_handle_table: HANDLE_TABLES[2], wait_table: WAIT_TABLES[1], endpoints: ENDPOINTS, ready_queue: READY_QUEUE }
}

func install_kernel_execution_context(next_context: KernelExecutionContext) {
    SYSCALL_GATE = next_context.gate
    PROCESS_SLOTS = next_context.process_slots
    TASK_SLOTS = next_context.task_slots
    HANDLE_TABLES[1] = next_context.init_handle_table
    HANDLE_TABLES[2] = next_context.child_handle_table
    WAIT_TABLES[1] = next_context.wait_table
    ENDPOINTS = next_context.endpoints
    READY_QUEUE = next_context.ready_queue
}

func current_service_runtime_state() bootstrap_services.ServiceRuntimeState {
    kernel_context: KernelExecutionContext = current_kernel_execution_context()
    return bootstrap_services.service_runtime_state(kernel_context.gate, kernel_context.process_slots, kernel_context.task_slots, kernel_context.init_handle_table, kernel_context.child_handle_table, kernel_context.wait_table, kernel_context.endpoints, INIT_IMAGE, kernel_context.ready_queue)
}

func current_log_service_snapshot() bootstrap_services.LogServiceSnapshot {
    return bootstrap_services.LogServiceSnapshot{ program_capability: LOG_SERVICE_SNAPSHOT.program_capability, address_space: LOG_SERVICE_SNAPSHOT.address_space, user_frame: LOG_SERVICE_SNAPSHOT.user_frame, state: LOG_SERVICE_SNAPSHOT.state, spawn_observation: LOG_SERVICE_SNAPSHOT.spawn_observation, receive_observation: LOG_SERVICE_SNAPSHOT.receive_observation, ack_observation: LOG_SERVICE_SNAPSHOT.ack_observation, handshake: LOG_SERVICE_SNAPSHOT.handshake, wait_observation: LOG_SERVICE_SNAPSHOT.wait_observation }
}

func install_log_service_snapshot(snapshot: bootstrap_services.LogServiceSnapshot) {
    LOG_SERVICE_SNAPSHOT = LogServiceSnapshot{ program_capability: snapshot.program_capability, address_space: snapshot.address_space, user_frame: snapshot.user_frame, state: snapshot.state, spawn_observation: snapshot.spawn_observation, receive_observation: snapshot.receive_observation, ack_observation: snapshot.ack_observation, handshake: snapshot.handshake, wait_observation: snapshot.wait_observation }
}

func current_echo_service_snapshot() bootstrap_services.EchoServiceSnapshot {
    return bootstrap_services.EchoServiceSnapshot{ program_capability: ECHO_SERVICE_SNAPSHOT.program_capability, address_space: ECHO_SERVICE_SNAPSHOT.address_space, user_frame: ECHO_SERVICE_SNAPSHOT.user_frame, state: ECHO_SERVICE_SNAPSHOT.state, spawn_observation: ECHO_SERVICE_SNAPSHOT.spawn_observation, receive_observation: ECHO_SERVICE_SNAPSHOT.receive_observation, reply_observation: ECHO_SERVICE_SNAPSHOT.reply_observation, exchange: ECHO_SERVICE_SNAPSHOT.exchange, wait_observation: ECHO_SERVICE_SNAPSHOT.wait_observation }
}

func install_echo_service_snapshot(snapshot: bootstrap_services.EchoServiceSnapshot) {
    ECHO_SERVICE_SNAPSHOT = EchoServiceSnapshot{ program_capability: snapshot.program_capability, address_space: snapshot.address_space, user_frame: snapshot.user_frame, state: snapshot.state, spawn_observation: snapshot.spawn_observation, receive_observation: snapshot.receive_observation, reply_observation: snapshot.reply_observation, exchange: snapshot.exchange, wait_observation: snapshot.wait_observation }
}

func current_transfer_service_snapshot() bootstrap_services.TransferServiceSnapshot {
    return bootstrap_services.TransferServiceSnapshot{ program_capability: TRANSFER_SERVICE_SNAPSHOT.program_capability, address_space: TRANSFER_SERVICE_SNAPSHOT.address_space, user_frame: TRANSFER_SERVICE_SNAPSHOT.user_frame, state: TRANSFER_SERVICE_SNAPSHOT.state, spawn_observation: TRANSFER_SERVICE_SNAPSHOT.spawn_observation, grant_observation: TRANSFER_SERVICE_SNAPSHOT.grant_observation, emit_observation: TRANSFER_SERVICE_SNAPSHOT.emit_observation, transfer: TRANSFER_SERVICE_SNAPSHOT.transfer, wait_observation: TRANSFER_SERVICE_SNAPSHOT.wait_observation }
}

func install_transfer_service_snapshot(snapshot: bootstrap_services.TransferServiceSnapshot) {
    TRANSFER_SERVICE_SNAPSHOT = TransferServiceSnapshot{ program_capability: snapshot.program_capability, address_space: snapshot.address_space, user_frame: snapshot.user_frame, state: snapshot.state, spawn_observation: snapshot.spawn_observation, grant_observation: snapshot.grant_observation, emit_observation: snapshot.emit_observation, transfer: snapshot.transfer, wait_observation: snapshot.wait_observation }
}

func current_serial_service_snapshot() bootstrap_services.SerialServiceSnapshot {
    return bootstrap_services.SerialServiceSnapshot{ program_capability: SERIAL_SERVICE_SNAPSHOT.program_capability, address_space: SERIAL_SERVICE_SNAPSHOT.address_space, user_frame: SERIAL_SERVICE_SNAPSHOT.user_frame, state: SERIAL_SERVICE_SNAPSHOT.state, spawn_observation: SERIAL_SERVICE_SNAPSHOT.spawn_observation, receive_observation: SERIAL_SERVICE_SNAPSHOT.receive_observation, failure_observation: SERIAL_SERVICE_SNAPSHOT.failure_observation, wait_observation: SERIAL_SERVICE_SNAPSHOT.wait_observation }
}

func build_uart_probe_frames() bootstrap_state.UartProbeFrames {
    queue_frame_one: [2]u8
    queue_frame_one[0] = UART_QUEUE_FRAME_ONE_BYTE0
    queue_frame_one[1] = UART_QUEUE_FRAME_ONE_BYTE1

    queue_frame_two: [2]u8
    queue_frame_two[0] = UART_QUEUE_FRAME_TWO_BYTE0
    queue_frame_two[1] = UART_QUEUE_FRAME_TWO_BYTE1

    queue_frame_three: [2]u8
    queue_frame_three[0] = UART_QUEUE_FRAME_THREE_BYTE0
    queue_frame_three[1] = UART_QUEUE_FRAME_THREE_BYTE1

    closed_frame: [2]u8
    closed_frame[0] = UART_CLOSED_FRAME_BYTE0
    closed_frame[1] = UART_CLOSED_FRAME_BYTE1

    completion_frame: [4]u8 = ipc.zero_payload()
    completion_frame[0] = UART_COMPLETION_FRAME_BYTE0
    completion_frame[1] = UART_COMPLETION_FRAME_BYTE1
    completion_frame[2] = UART_COMPLETION_FRAME_BYTE2
    completion_frame[3] = UART_COMPLETION_FRAME_BYTE3

    return bootstrap_state.UartProbeFrames{ queue_frame_one: queue_frame_one, queue_frame_two: queue_frame_two, queue_frame_three: queue_frame_three, closed_frame: closed_frame, completion_frame: completion_frame }
}

func build_mid_phase_proof_runtime_context() bootstrap_proofs.MidPhaseProofRuntimeContext {
    return bootstrap_proofs.MidPhaseProofRuntimeContext{ scheduler_lifecycle_audit: build_scheduler_lifecycle_audit(), late_phase_context: build_late_phase_proof_context(), init_pid: INIT_PID, init_tid: INIT_TID, init_asid: INIT_ASID, init_endpoint_id: INIT_ENDPOINT_ID, init_endpoint_handle_slot: INIT_ENDPOINT_HANDLE_SLOT, init_root_page_table: INIT_ROOT_PAGE_TABLE, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT, child_root_page_table: CHILD_ROOT_PAGE_TABLE, child_exit_code: CHILD_EXIT_CODE, boot_pid: BOOT_PID, boot_tid: BOOT_TID, boot_task_slot: BOOT_TASK_SLOT, boot_entry_pc: BOOT_ENTRY_PC, boot_stack_top: BOOT_STACK_TOP, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, transfer_source_handle_slot: TRANSFER_SOURCE_HANDLE_SLOT, transfer_received_handle_slot: TRANSFER_RECEIVED_HANDLE_SLOT, transfer_service_directory_key: TRANSFER_SERVICE_DIRECTORY_KEY, composition_echo_endpoint_id: COMPOSITION_ECHO_ENDPOINT_ID, composition_log_endpoint_id: COMPOSITION_LOG_ENDPOINT_ID, serial_service_endpoint_id: SERIAL_SERVICE_ENDPOINT_ID, init_image: INIT_IMAGE, arch_actor: ARCH_ACTOR, boot_log_append_failed: BOOT_LOG_APPEND_FAILED, gate: SYSCALL_GATE, process_slots: PROCESS_SLOTS, task_slots: TASK_SLOTS, init_handle_table: HANDLE_TABLES[1], child_handle_table: HANDLE_TABLES[2], wait_table: WAIT_TABLES[1], endpoints: ENDPOINTS, ready_queue: READY_QUEUE, kernel: KERNEL, bootstrap_program_capability: INIT_RUNTIME_SNAPSHOT.bootstrap_caps.program_capability, init_bootstrap_handoff: INIT_RUNTIME_SNAPSHOT.bootstrap_handoff, init_user_frame: INIT_RUNTIME_SNAPSHOT.user_frame, receive_observation: RECEIVE_OBSERVATION, attached_receive_observation: ATTACHED_RECEIVE_OBSERVATION, transferred_handle_use_observation: TRANSFERRED_HANDLE_USE_OBSERVATION, pre_exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.pre_exit_wait_observation, exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.exit_wait_observation, sleep_observation: CHILD_EXECUTION_SNAPSHOT.sleep_observation, timer_wake_observation: CHILD_EXECUTION_SNAPSHOT.timer_wake_observation, last_interrupt_kind: LAST_INTERRUPT_KIND, log_service_snapshot: current_log_service_snapshot(), echo_service_snapshot: current_echo_service_snapshot(), transfer_service_snapshot: current_transfer_service_snapshot(), serial_service_snapshot: current_serial_service_snapshot(), interrupts: INTERRUPTS, uart_device: UART_DEVICE, uart_receive_vector: UART_RECEIVE_VECTOR, uart_completion_vector: UART_COMPLETION_VECTOR, uart_source_actor: UART_SOURCE_ACTOR, uart_frames: build_uart_probe_frames() }
}

func install_mid_phase_proof_result(result: bootstrap_proofs.MidPhaseProofResult) {
    install_log_service_snapshot(result.log_service_snapshot)
    install_echo_service_snapshot(result.echo_service_snapshot)
    install_transfer_service_snapshot(result.transfer_service_snapshot)
    install_serial_service_execution_state(result.serial_state)
    INTERRUPTS = result.interrupts
    LAST_INTERRUPT_KIND = result.last_interrupt_kind
    UART_DEVICE = result.uart_device
    UART_INGRESS = result.uart_ingress
}

func validate_late_phase_boot_log_entry(expectation: LateBootLogExpectation) i32 {
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, expectation.index)) != 16 {
        return expectation.stage_error
    }
    if state.log_actor_at(BOOT_LOG, expectation.index) != ARCH_ACTOR {
        return expectation.actor_error
    }
    if state.log_detail_at(BOOT_LOG, expectation.index) != expectation.detail {
        return expectation.detail_error
    }
    return 0
}

func seed_kernel_descriptor() {
    KERNEL = state.boot_descriptor(KERNEL_MAGIC, BOOT_PID, BOOT_TID)
}

func seed_process_table() {
    slots: [3]state.ProcessSlot = state.zero_process_slots()
    slots[0] = state.boot_process_slot(BOOT_PID, BOOT_TASK_SLOT)
    PROCESS_SLOTS = slots
}

func seed_task_table() {
    slots: [3]state.TaskSlot = state.zero_task_slots()
    slots[0] = state.boot_task_slot(BOOT_TID, BOOT_PID, BOOT_ENTRY_PC, BOOT_STACK_TOP)
    TASK_SLOTS = slots
}

func seed_ready_queue() {
    READY_QUEUE = state.boot_ready_queue(BOOT_TID)
}

func seed_kernel_owners() {
    ROOT_CAPABILITY = capability.bootstrap_root_slot(BOOT_PID)
    INIT_PROGRAM_CAPABILITY = capability.empty_slot()
    HANDLE_TABLES = capability.zero_handle_tables()
    WAIT_TABLES = capability.zero_wait_tables()
    ENDPOINTS = ipc.empty_table()
    INTERRUPTS = interrupt.unmask_timer(INTERRUPTS, 32)
    INTERRUPTS = interrupt.unmask_uart_receive(INTERRUPTS, UART_RECEIVE_VECTOR)
    INTERRUPTS = interrupt.unmask_uart_completion(INTERRUPTS, UART_COMPLETION_VECTOR)
    SYSCALL_GATE = syscall.gate_closed()
}

func validate_seeded_boot_log() bool {
    if BOOT_LOG_APPEND_FAILED != 0 {
        return false
    }
    if BOOT_LOG.count != 2 {
        return false
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 0)) != 1 {
        return false
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 1)) != 2 {
        return false
    }
    return true
}

func validate_seeded_owner_state() bool {
    if !init.bootstrap_image_valid(INIT_IMAGE) {
        return false
    }
    if capability.kind_score(ROOT_CAPABILITY.kind) != 2 {
        return false
    }
    if ROOT_CAPABILITY.owner_pid != BOOT_PID {
        return false
    }
    if capability.kind_score(INIT_PROGRAM_CAPABILITY.kind) != 1 {
        return false
    }
    if HANDLE_TABLES[0].count != 0 {
        return false
    }
    if HANDLE_TABLES[1].count != 0 {
        return false
    }
    if HANDLE_TABLES[2].count != 0 {
        return false
    }
    if WAIT_TABLES[0].count != 0 {
        return false
    }
    if WAIT_TABLES[1].count != 0 {
        return false
    }
    if WAIT_TABLES[2].count != 0 {
        return false
    }
    if ENDPOINTS.count != 0 {
        return false
    }
    if interrupt.state_score(INTERRUPTS.state) != 2 {
        return false
    }
    if INTERRUPTS.timer_vector != 32 {
        return false
    }
    if INTERRUPTS.last_vector != 0 {
        return false
    }
    if INTERRUPTS.last_source_actor != 0 {
        return false
    }
    if INTERRUPTS.entry_count != 0 {
        return false
    }
    if INTERRUPTS.dispatch_count != 0 {
        return false
    }
    if syscall.id_score(SYSCALL_GATE.last_id) != 1 {
        return false
    }
    if syscall.status_score(SYSCALL_GATE.last_status) != 1 {
        return false
    }
    if SYSCALL_GATE.open != 0 {
        return false
    }
    if SYSCALL_GATE.send_count != 0 {
        return false
    }
    if SYSCALL_GATE.receive_count != 0 {
        return false
    }
    if INIT_IMAGE.image_id != 1 {
        return false
    }
    if address_space.state_score(INIT_RUNTIME_SNAPSHOT.address_space.state) != 1 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.user_frame.task_id != 0 {
        return false
    }
    if CHILD_EXECUTION_SNAPSHOT.user_frame.task_id != 0 {
        return false
    }
    return true
}

func validate_seeded_tables() bool {
    if KERNEL.magic != KERNEL_MAGIC {
        return false
    }
    if KERNEL.current_pid != BOOT_PID {
        return false
    }
    if KERNEL.current_tid != BOOT_TID {
        return false
    }
    if KERNEL.active_asid != 0 {
        return false
    }
    if KERNEL.booted != 0 {
        return false
    }
    if KERNEL.user_entry_started != 0 {
        return false
    }
    if PROCESS_SLOTS[0].pid != BOOT_PID {
        return false
    }
    if PROCESS_SLOTS[0].task_slot != BOOT_TASK_SLOT {
        return false
    }
    if PROCESS_SLOTS[0].address_space_id != 0 {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[0].state) != 2 {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[1].state) != 1 {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[2].state) != 1 {
        return false
    }
    if TASK_SLOTS[0].tid != BOOT_TID {
        return false
    }
    if TASK_SLOTS[0].owner_pid != BOOT_PID {
        return false
    }
    if TASK_SLOTS[0].address_space_id != 0 {
        return false
    }
    if TASK_SLOTS[0].entry_pc != BOOT_ENTRY_PC {
        return false
    }
    if TASK_SLOTS[0].stack_top != BOOT_STACK_TOP {
        return false
    }
    if state.task_state_score(TASK_SLOTS[0].state) != 2 {
        return false
    }
    if state.task_state_score(TASK_SLOTS[1].state) != 1 {
        return false
    }
    if state.task_state_score(TASK_SLOTS[2].state) != 1 {
        return false
    }
    if READY_QUEUE.count != 1 {
        return false
    }
    if state.ready_slot_at(READY_QUEUE, 0) != BOOT_TID {
        return false
    }
    if !validate_seeded_owner_state() {
        return false
    }
    return validate_seeded_boot_log()
}

func architecture_entry() bool {
    reset_kernel_state()
    // Proof-only sequencing: reset is recorded before seeded-table validation runs.
    record_boot_stage(state.BootStage.Reset, KERNEL_MAGIC)
    seed_kernel_descriptor()
    seed_process_table()
    seed_task_table()
    seed_ready_queue()
    seed_kernel_owners()
    // Proof-only sequencing: the second boot-log record marks seeded tables.
    record_boot_stage(state.BootStage.TablesSeeded, BOOT_TID)
    if !validate_seeded_tables() {
        return false
    }
    KERNEL = state.mark_booted(KERNEL)
    return true
}

func construct_first_user_address_space() bool {
    if !init.bootstrap_image_valid(INIT_IMAGE) {
        return false
    }
    slots: [3]state.ProcessSlot = PROCESS_SLOTS
    tasks: [3]state.TaskSlot = TASK_SLOTS
    new_address_space: address_space.AddressSpace = address_space.bootstrap_space(INIT_ASID, INIT_PID, mmu.bootstrap_translation_root(INIT_ASID, INIT_ROOT_PAGE_TABLE), INIT_IMAGE.image_base, INIT_IMAGE.image_size, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_base, INIT_IMAGE.stack_size, INIT_IMAGE.stack_top)
    INIT_RUNTIME_SNAPSHOT = InitRuntimeSnapshot{ bootstrap_caps: INIT_RUNTIME_SNAPSHOT.bootstrap_caps, bootstrap_handoff: INIT_RUNTIME_SNAPSHOT.bootstrap_handoff, address_space: new_address_space, user_frame: INIT_RUNTIME_SNAPSHOT.user_frame }
    if address_space.state_score(INIT_RUNTIME_SNAPSHOT.address_space.state) != 2 {
        return false
    }
    INIT_PROGRAM_CAPABILITY = capability.bootstrap_init_program_slot(INIT_PID)
    slots[1] = state.init_process_slot(INIT_PID, INIT_TASK_SLOT, INIT_ASID)
    tasks[1] = state.user_task_slot(INIT_TID, INIT_PID, INIT_ASID, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    PROCESS_SLOTS = slots
    TASK_SLOTS = tasks
    READY_QUEUE = state.user_ready_queue(INIT_TID)
    new_user_frame: address_space.UserEntryFrame = address_space.bootstrap_user_frame(new_address_space, INIT_TID)
    INIT_RUNTIME_SNAPSHOT = InitRuntimeSnapshot{ bootstrap_caps: INIT_RUNTIME_SNAPSHOT.bootstrap_caps, bootstrap_handoff: INIT_RUNTIME_SNAPSHOT.bootstrap_handoff, address_space: INIT_RUNTIME_SNAPSHOT.address_space, user_frame: new_user_frame }
    record_boot_stage(state.BootStage.AddressSpaceReady, INIT_ASID)
    return BOOT_LOG_APPEND_FAILED == 0
}

func validate_first_user_entry_ready() bool {
    if KERNEL.booted != 1 {
        return false
    }
    if KERNEL.user_entry_started != 0 {
        return false
    }
    if PROCESS_SLOTS[1].pid != INIT_PID {
        return false
    }
    if PROCESS_SLOTS[1].task_slot != INIT_TASK_SLOT {
        return false
    }
    if PROCESS_SLOTS[1].address_space_id != INIT_ASID {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[1].state) != 4 {
        return false
    }
    if TASK_SLOTS[1].tid != INIT_TID {
        return false
    }
    if TASK_SLOTS[1].owner_pid != INIT_PID {
        return false
    }
    if TASK_SLOTS[1].address_space_id != INIT_ASID {
        return false
    }
    if TASK_SLOTS[1].entry_pc != INIT_IMAGE.entry_pc {
        return false
    }
    if TASK_SLOTS[1].stack_top != INIT_IMAGE.stack_top {
        return false
    }
    if state.task_state_score(TASK_SLOTS[1].state) != 4 {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[2].state) != 1 {
        return false
    }
    if state.task_state_score(TASK_SLOTS[2].state) != 1 {
        return false
    }
    if !capability.is_program_capability(INIT_PROGRAM_CAPABILITY) {
        return false
    }
    if INIT_PROGRAM_CAPABILITY.owner_pid != INIT_PID {
        return false
    }
    if address_space.state_score(INIT_RUNTIME_SNAPSHOT.address_space.state) != 2 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.asid != INIT_ASID {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.owner_pid != INIT_PID {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.translation_root.page_table_base != INIT_ROOT_PAGE_TABLE {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.translation_root.owner_asid != INIT_ASID {
        return false
    }
    if mmu.state_score(INIT_RUNTIME_SNAPSHOT.address_space.translation_root.state) != 2 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.mapping_count != 2 {
        return false
    }
    image_mapping_index: usize = address_space.find_mapping_index(INIT_RUNTIME_SNAPSHOT.address_space, address_space.MappingKind.ImageText)
    if image_mapping_index >= INIT_RUNTIME_SNAPSHOT.address_space.mapping_count {
        return false
    }
    stack_mapping_index: usize = address_space.find_mapping_index(INIT_RUNTIME_SNAPSHOT.address_space, address_space.MappingKind.UserStack)
    if stack_mapping_index >= INIT_RUNTIME_SNAPSHOT.address_space.mapping_count {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.mappings[image_mapping_index].base != INIT_IMAGE.image_base {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.mappings[image_mapping_index].size != INIT_IMAGE.image_size {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.mappings[image_mapping_index].writable != 0 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.mappings[image_mapping_index].executable != 1 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.mappings[stack_mapping_index].base != INIT_IMAGE.stack_base {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.mappings[stack_mapping_index].size != INIT_IMAGE.stack_size {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.mappings[stack_mapping_index].writable != 1 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.address_space.mappings[stack_mapping_index].executable != 0 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.user_frame.entry_pc != INIT_IMAGE.entry_pc {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.user_frame.stack_top != INIT_IMAGE.stack_top {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.user_frame.address_space_id != INIT_ASID {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.user_frame.task_id != INIT_TID {
        return false
    }
    if READY_QUEUE.count != 1 {
        return false
    }
    if state.ready_slot_at(READY_QUEUE, 0) != INIT_TID {
        return false
    }
    if BOOT_LOG.count != 3 {
        return false
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 2)) != 4 {
        return false
    }
    if state.log_detail_at(BOOT_LOG, 2) != INIT_ASID {
        return false
    }
    return true
}

func transfer_to_first_user_entry() bool {
    if !construct_first_user_address_space() {
        return false
    }
    if !validate_first_user_entry_ready() {
        return false
    }
    if !address_space.can_activate(INIT_RUNTIME_SNAPSHOT.address_space) {
        return false
    }
    activated_address_space: address_space.AddressSpace = address_space.activate(INIT_RUNTIME_SNAPSHOT.address_space)
    INIT_RUNTIME_SNAPSHOT = InitRuntimeSnapshot{ bootstrap_caps: INIT_RUNTIME_SNAPSHOT.bootstrap_caps, bootstrap_handoff: INIT_RUNTIME_SNAPSHOT.bootstrap_handoff, address_space: activated_address_space, user_frame: INIT_RUNTIME_SNAPSHOT.user_frame }
    if address_space.state_score(INIT_RUNTIME_SNAPSHOT.address_space.state) != 4 {
        return false
    }
    KERNEL = state.start_user_entry(KERNEL, INIT_PID, INIT_TID, INIT_ASID)
    record_boot_stage(state.BootStage.UserEntryReady, INIT_TID)
    return true
}

func bootstrap_endpoint_handle_core() {
    handle_tables: [3]capability.HandleTable = HANDLE_TABLES
    endpoints: ipc.EndpointTable = ENDPOINTS

    endpoints = ipc.install_endpoint(endpoints, INIT_PID, INIT_ENDPOINT_ID)
    handle_tables[1] = capability.handle_table_for_owner(INIT_PID)
    handle_tables[1] = capability.install_endpoint_handle(handle_tables[1], INIT_ENDPOINT_HANDLE_SLOT, INIT_ENDPOINT_ID, 7)
    endpoints = ipc.enqueue_message(endpoints, 0, ipc.bootstrap_init_message(BOOT_PID, INIT_ENDPOINT_ID))
    DELIVERED_MESSAGE = ipc.mark_delivered(ipc.peek_head_message(endpoints, 0))
    endpoints = ipc.consume_head_message(endpoints, 0)

    HANDLE_TABLES = handle_tables
    ENDPOINTS = endpoints
}

func validate_endpoint_handle_core() bool {
    if KERNEL.booted != 1 {
        return false
    }
    if KERNEL.current_pid != INIT_PID {
        return false
    }
    if KERNEL.current_tid != INIT_TID {
        return false
    }
    if KERNEL.active_asid != INIT_ASID {
        return false
    }
    if KERNEL.user_entry_started != 1 {
        return false
    }
    if ENDPOINTS.count != 1 {
        return false
    }
    if ENDPOINTS.slots[0].endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if ENDPOINTS.slots[0].owner_pid != INIT_PID {
        return false
    }
    if ENDPOINTS.slots[0].active != 1 {
        return false
    }
    if ENDPOINTS.slots[0].queued_messages != 0 {
        return false
    }
    if ENDPOINTS.slots[0].head != 0 {
        return false
    }
    if ENDPOINTS.slots[0].tail != 0 {
        return false
    }
    if ipc.message_state_score(ENDPOINTS.slots[0].messages[0].state) != 1 {
        return false
    }
    if ipc.message_state_score(ENDPOINTS.slots[0].messages[1].state) != 1 {
        return false
    }
    if HANDLE_TABLES[0].count != 0 {
        return false
    }
    if HANDLE_TABLES[2].count != 0 {
        return false
    }
    if HANDLE_TABLES[1].owner_pid != INIT_PID {
        return false
    }
    if HANDLE_TABLES[1].count != 1 {
        return false
    }
    if HANDLE_TABLES[1].slots[0].slot_id != INIT_ENDPOINT_HANDLE_SLOT {
        return false
    }
    if HANDLE_TABLES[1].slots[0].owner_pid != INIT_PID {
        return false
    }
    if HANDLE_TABLES[1].slots[0].endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if HANDLE_TABLES[1].slots[0].rights != 7 {
        return false
    }
    if capability.handle_state_score(HANDLE_TABLES[1].slots[0].state) != 2 {
        return false
    }
    if DELIVERED_MESSAGE.message_id != 1 {
        return false
    }
    if DELIVERED_MESSAGE.source_pid != BOOT_PID {
        return false
    }
    if DELIVERED_MESSAGE.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if DELIVERED_MESSAGE.len != 4 {
        return false
    }
    if ipc.message_state_score(DELIVERED_MESSAGE.state) != 4 {
        return false
    }
    if DELIVERED_MESSAGE.payload[0] != 73 {
        return false
    }
    if DELIVERED_MESSAGE.payload[1] != 78 {
        return false
    }
    if DELIVERED_MESSAGE.payload[2] != 73 {
        return false
    }
    if DELIVERED_MESSAGE.payload[3] != 84 {
        return false
    }
    if WAIT_TABLES[1].count != 0 {
        return false
    }
    return true
}

func handoff_init_bootstrap_capability_set() bool {
    new_bootstrap_caps: init.BootstrapCapabilitySet = init.install_bootstrap_capability_set(INIT_PID, INIT_ENDPOINT_HANDLE_SLOT, INIT_PROGRAM_CAPABILITY)
    new_bootstrap_handoff: init.BootstrapHandoffObservation = init.observe_bootstrap_handoff(new_bootstrap_caps)
    INIT_RUNTIME_SNAPSHOT = InitRuntimeSnapshot{ bootstrap_caps: new_bootstrap_caps, bootstrap_handoff: new_bootstrap_handoff, address_space: INIT_RUNTIME_SNAPSHOT.address_space, user_frame: INIT_RUNTIME_SNAPSHOT.user_frame }
    return INIT_RUNTIME_SNAPSHOT.bootstrap_handoff.authority_count == 2
}

func validate_init_bootstrap_capability_handoff() bool {
    if INIT_RUNTIME_SNAPSHOT.bootstrap_caps.owner_pid != INIT_PID {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_caps.endpoint_handle_slot != INIT_ENDPOINT_HANDLE_SLOT {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_caps.endpoint_handle_count != 1 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_caps.program_capability_count != 1 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_caps.wait_handle_count != 0 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_caps.ambient_root_visible != 0 {
        return false
    }
    if !capability.is_program_capability(INIT_RUNTIME_SNAPSHOT.bootstrap_caps.program_capability) {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_caps.program_capability.owner_pid != INIT_PID {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_caps.program_capability.slot_id != INIT_PROGRAM_CAPABILITY.slot_id {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_caps.program_capability.object_id != INIT_PROGRAM_CAPABILITY.object_id {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_handoff.owner_pid != INIT_PID {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_handoff.authority_count != 2 {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_handoff.endpoint_handle_slot != INIT_ENDPOINT_HANDLE_SLOT {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_handoff.program_capability_slot != INIT_PROGRAM_CAPABILITY.slot_id {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_handoff.program_object_id != INIT_PROGRAM_CAPABILITY.object_id {
        return false
    }
    if INIT_RUNTIME_SNAPSHOT.bootstrap_handoff.ambient_root_visible != 0 {
        return false
    }
    if capability.kind_score(ROOT_CAPABILITY.kind) != 2 {
        return false
    }
    if ROOT_CAPABILITY.owner_pid != BOOT_PID {
        return false
    }
    if HANDLE_TABLES[1].count != 1 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], INIT_RUNTIME_SNAPSHOT.bootstrap_caps.endpoint_handle_slot) != INIT_ENDPOINT_ID {
        return false
    }
    if WAIT_TABLES[1].count != INIT_RUNTIME_SNAPSHOT.bootstrap_caps.wait_handle_count {
        return false
    }
    return true
}

func execute_syscall_byte_ipc() bool {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = 83
    payload[1] = 89
    payload[2] = 83
    payload[3] = 67

    SYSCALL_GATE = syscall.open_gate(SYSCALL_GATE)
    send_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, INIT_PID, syscall.build_send_request(INIT_ENDPOINT_HANDLE_SLOT, 4, payload))
    SYSCALL_GATE = send_result.gate
    HANDLE_TABLES[1] = send_result.handle_table
    ENDPOINTS = send_result.endpoints
    if syscall.status_score(send_result.status) != 2 {
        return false
    }

    receive_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, syscall.build_receive_request(INIT_ENDPOINT_HANDLE_SLOT))
    SYSCALL_GATE = receive_result.gate
    HANDLE_TABLES[1] = receive_result.handle_table
    ENDPOINTS = receive_result.endpoints
    RECEIVE_OBSERVATION = receive_result.observation
    if syscall.status_score(RECEIVE_OBSERVATION.status) != 2 {
        return false
    }

    would_block_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, syscall.build_receive_request(INIT_ENDPOINT_HANDLE_SLOT))
    SYSCALL_GATE = would_block_result.gate
    HANDLE_TABLES[1] = would_block_result.handle_table
    ENDPOINTS = would_block_result.endpoints
    if syscall.status_score(would_block_result.observation.status) != 4 {
        return false
    }
    return syscall.block_reason_score(would_block_result.observation.block_reason) == 4
}

func validate_syscall_byte_ipc() bool {
    if SYSCALL_GATE.open != 1 {
        return false
    }
    if syscall.id_score(SYSCALL_GATE.last_id) != 4 {
        return false
    }
    if syscall.status_score(SYSCALL_GATE.last_status) != 4 {
        return false
    }
    if SYSCALL_GATE.send_count != 1 {
        return false
    }
    if SYSCALL_GATE.receive_count != 1 {
        return false
    }
    if ENDPOINTS.count != 1 {
        return false
    }
    if ENDPOINTS.slots[0].queued_messages != 0 {
        return false
    }
    if RECEIVE_OBSERVATION.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if RECEIVE_OBSERVATION.source_pid != INIT_PID {
        return false
    }
    if RECEIVE_OBSERVATION.payload_len != 4 {
        return false
    }
    if RECEIVE_OBSERVATION.payload[0] != 83 {
        return false
    }
    if RECEIVE_OBSERVATION.payload[1] != 89 {
        return false
    }
    if RECEIVE_OBSERVATION.payload[2] != 83 {
        return false
    }
    if RECEIVE_OBSERVATION.payload[3] != 67 {
        return false
    }
    if syscall.status_score(RECEIVE_OBSERVATION.status) != 2 {
        return false
    }
    if syscall.block_reason_score(RECEIVE_OBSERVATION.block_reason) != 1 {
        return false
    }
    return true
}

func seed_transfer_endpoint_handle() bool {
    handle_tables: [3]capability.HandleTable = HANDLE_TABLES
    endpoints: ipc.EndpointTable = ENDPOINTS

    endpoints = ipc.install_endpoint(endpoints, INIT_PID, TRANSFER_ENDPOINT_ID)
    handle_tables[1] = capability.install_endpoint_handle(handle_tables[1], TRANSFER_SOURCE_HANDLE_SLOT, TRANSFER_ENDPOINT_ID, 7)

    HANDLE_TABLES = handle_tables
    ENDPOINTS = endpoints

    if ENDPOINTS.count != 2 {
        return false
    }
    if ENDPOINTS.slots[1].endpoint_id != TRANSFER_ENDPOINT_ID {
        return false
    }
    if ENDPOINTS.slots[1].owner_pid != INIT_PID {
        return false
    }
    if ENDPOINTS.slots[1].active != 1 {
        return false
    }
    if HANDLE_TABLES[1].count != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_SOURCE_HANDLE_SLOT) != TRANSFER_ENDPOINT_ID {
        return false
    }
    if capability.find_rights_for_handle(HANDLE_TABLES[1], TRANSFER_SOURCE_HANDLE_SLOT) != 7 {
        return false
    }
    return true
}

func execute_capability_carrying_ipc_transfer() bool {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = 67
    payload[1] = 65
    payload[2] = 80
    payload[3] = 83

    transfer_send_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, INIT_PID, syscall.build_transfer_send_request(INIT_ENDPOINT_HANDLE_SLOT, 4, payload, TRANSFER_SOURCE_HANDLE_SLOT))
    SYSCALL_GATE = transfer_send_result.gate
    HANDLE_TABLES[1] = transfer_send_result.handle_table
    ENDPOINTS = transfer_send_result.endpoints
    if syscall.status_score(transfer_send_result.status) != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_SOURCE_HANDLE_SLOT) != 0 {
        return false
    }

    transfer_receive_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, syscall.build_transfer_receive_request(INIT_ENDPOINT_HANDLE_SLOT, TRANSFER_RECEIVED_HANDLE_SLOT))
    SYSCALL_GATE = transfer_receive_result.gate
    HANDLE_TABLES[1] = transfer_receive_result.handle_table
    ENDPOINTS = transfer_receive_result.endpoints
    ATTACHED_RECEIVE_OBSERVATION = transfer_receive_result.observation
    if syscall.status_score(ATTACHED_RECEIVE_OBSERVATION.status) != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_RECEIVED_HANDLE_SLOT) != TRANSFER_ENDPOINT_ID {
        return false
    }

    follow_payload: [4]u8 = ipc.zero_payload()
    follow_payload[0] = 77
    follow_payload[1] = 79
    follow_payload[2] = 86
    follow_payload[3] = 69

    follow_send_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, INIT_PID, syscall.build_send_request(TRANSFER_RECEIVED_HANDLE_SLOT, 4, follow_payload))
    SYSCALL_GATE = follow_send_result.gate
    HANDLE_TABLES[1] = follow_send_result.handle_table
    ENDPOINTS = follow_send_result.endpoints
    if syscall.status_score(follow_send_result.status) != 2 {
        return false
    }

    follow_receive_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, syscall.build_receive_request(TRANSFER_RECEIVED_HANDLE_SLOT))
    SYSCALL_GATE = follow_receive_result.gate
    HANDLE_TABLES[1] = follow_receive_result.handle_table
    ENDPOINTS = follow_receive_result.endpoints
    TRANSFERRED_HANDLE_USE_OBSERVATION = follow_receive_result.observation
    return syscall.status_score(TRANSFERRED_HANDLE_USE_OBSERVATION.status) == 2
}

func validate_capability_carrying_ipc_transfer() bool {
    if SYSCALL_GATE.open != 1 {
        return false
    }
    if syscall.id_score(SYSCALL_GATE.last_id) != 4 {
        return false
    }
    if syscall.status_score(SYSCALL_GATE.last_status) != 2 {
        return false
    }
    if SYSCALL_GATE.send_count != 3 {
        return false
    }
    if SYSCALL_GATE.receive_count != 3 {
        return false
    }
    if ENDPOINTS.count != 2 {
        return false
    }
    if ENDPOINTS.slots[0].queued_messages != 0 {
        return false
    }
    if ENDPOINTS.slots[1].queued_messages != 0 {
        return false
    }
    if HANDLE_TABLES[1].count != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], INIT_ENDPOINT_HANDLE_SLOT) != INIT_ENDPOINT_ID {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_SOURCE_HANDLE_SLOT) != 0 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_RECEIVED_HANDLE_SLOT) != TRANSFER_ENDPOINT_ID {
        return false
    }
    if capability.find_rights_for_handle(HANDLE_TABLES[1], TRANSFER_RECEIVED_HANDLE_SLOT) != 7 {
        return false
    }
    if syscall.status_score(ATTACHED_RECEIVE_OBSERVATION.status) != 2 {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.source_pid != INIT_PID {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.payload_len != 4 {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.received_handle_slot != TRANSFER_RECEIVED_HANDLE_SLOT {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.received_handle_count != 1 {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.payload[0] != 67 {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.payload[1] != 65 {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.payload[2] != 80 {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.payload[3] != 83 {
        return false
    }
    if syscall.status_score(TRANSFERRED_HANDLE_USE_OBSERVATION.status) != 2 {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.endpoint_id != TRANSFER_ENDPOINT_ID {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.source_pid != INIT_PID {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.payload_len != 4 {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.received_handle_slot != 0 {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.received_handle_count != 0 {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.payload[0] != 77 {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.payload[1] != 79 {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.payload[2] != 86 {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.payload[3] != 69 {
        return false
    }
    return true
}

func validate_timer_hardening_contracts() bool {
    return bootstrap_audit.validate_timer_hardening_contracts()
}

func validate_bootstrap_layout_contracts() bool {
    return bootstrap_audit.validate_bootstrap_layout_contracts(bootstrap_audit.BootstrapLayoutAudit{ init_image: INIT_IMAGE, init_root_page_table: INIT_ROOT_PAGE_TABLE })
}

func validate_endpoint_and_capability_contracts() bool {
    return bootstrap_audit.validate_endpoint_and_capability_contracts(bootstrap_audit.EndpointCapabilityAudit{ init_pid: INIT_PID, init_endpoint_id: INIT_ENDPOINT_ID, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT })
}

func validate_state_hardening_contracts() bool {
    return bootstrap_audit.validate_state_hardening_contracts(bootstrap_audit.StateHardeningAudit{ boot_tid: BOOT_TID, boot_pid: BOOT_PID, boot_entry_pc: BOOT_ENTRY_PC, boot_stack_top: BOOT_STACK_TOP, child_tid: CHILD_TID, child_pid: CHILD_PID, child_asid: CHILD_ASID, init_image: INIT_IMAGE, arch_actor: ARCH_ACTOR })
}

func validate_syscall_contract_hardening() bool {
    return bootstrap_audit.validate_syscall_contract_hardening(bootstrap_audit.SyscallHardeningAudit{ init_pid: INIT_PID, init_endpoint_handle_slot: INIT_ENDPOINT_HANDLE_SLOT, init_endpoint_id: INIT_ENDPOINT_ID, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, child_root_page_table: CHILD_ROOT_PAGE_TABLE, boot_pid: BOOT_PID, boot_tid: BOOT_TID, boot_task_slot: BOOT_TASK_SLOT, boot_entry_pc: BOOT_ENTRY_PC, boot_stack_top: BOOT_STACK_TOP, init_image: INIT_IMAGE, transfer_source_handle_slot: TRANSFER_SOURCE_HANDLE_SLOT })
}

func validate_phase104_contract_hardening() bool {
    timer_hardened: u32 = 0
    if validate_timer_hardening_contracts() {
        timer_hardened = 1
    }
    return bootstrap_audit.validate_phase104_contract_hardening(bootstrap_audit.Phase104HardeningAudit{ boot_log_append_failed: BOOT_LOG_APPEND_FAILED, timer_hardened: timer_hardened, bootstrap_layout: bootstrap_audit.BootstrapLayoutAudit{ init_image: INIT_IMAGE, init_root_page_table: INIT_ROOT_PAGE_TABLE }, endpoint_capability: bootstrap_audit.EndpointCapabilityAudit{ init_pid: INIT_PID, init_endpoint_id: INIT_ENDPOINT_ID, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT }, state_hardening: bootstrap_audit.StateHardeningAudit{ boot_tid: BOOT_TID, boot_pid: BOOT_PID, boot_entry_pc: BOOT_ENTRY_PC, boot_stack_top: BOOT_STACK_TOP, child_tid: CHILD_TID, child_pid: CHILD_PID, child_asid: CHILD_ASID, init_image: INIT_IMAGE, arch_actor: ARCH_ACTOR }, syscall_hardening: bootstrap_audit.SyscallHardeningAudit{ init_pid: INIT_PID, init_endpoint_handle_slot: INIT_ENDPOINT_HANDLE_SLOT, init_endpoint_id: INIT_ENDPOINT_ID, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, child_root_page_table: CHILD_ROOT_PAGE_TABLE, boot_pid: BOOT_PID, boot_tid: BOOT_TID, boot_task_slot: BOOT_TASK_SLOT, boot_entry_pc: BOOT_ENTRY_PC, boot_stack_top: BOOT_STACK_TOP, init_image: INIT_IMAGE, transfer_source_handle_slot: TRANSFER_SOURCE_HANDLE_SLOT } })
}

func build_log_service_config() bootstrap_services.LogServiceConfig {
    return bootstrap_services.log_service_config(INIT_PID, CHILD_PID, CHILD_TID, CHILD_ASID, INIT_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT, mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE))
}

func build_log_service_execution_state() bootstrap_services.LogServiceExecutionState {
    return bootstrap_services.log_service_execution_state(current_log_service_snapshot(), current_service_runtime_state())
}

func install_log_service_execution_state(next_state: bootstrap_services.LogServiceExecutionState) {
    install_kernel_execution_context(KernelExecutionContext{ gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: next_state.endpoints, ready_queue: next_state.ready_queue })
    install_log_service_snapshot(bootstrap_services.log_service_snapshot(next_state))
}

func execute_phase105_log_service_handshake() bool {
    result: bootstrap_services.LogServiceExecutionResult = bootstrap_services.execute_phase105_log_service_handshake(build_log_service_config(), build_log_service_execution_state())
    install_log_service_execution_state(result.state)
    return result.succeeded != 0
}

func validate_phase105_log_service_handshake() bool {
    config: bootstrap_services.LogServiceConfig = build_log_service_config()
    return bootstrap_audit.validate_phase105_log_service_handshake(bootstrap_audit.LogServicePhaseAudit{ program_capability: LOG_SERVICE_SNAPSHOT.program_capability, gate: SYSCALL_GATE, spawn_observation: LOG_SERVICE_SNAPSHOT.spawn_observation, receive_observation: LOG_SERVICE_SNAPSHOT.receive_observation, ack_observation: LOG_SERVICE_SNAPSHOT.ack_observation, service_state: LOG_SERVICE_SNAPSHOT.state, handshake: LOG_SERVICE_SNAPSHOT.handshake, wait_observation: LOG_SERVICE_SNAPSHOT.wait_observation, wait_table: WAIT_TABLES[1], child_handle_table: HANDLE_TABLES[2], ready_queue: READY_QUEUE, child_process: PROCESS_SLOTS[2], child_task: TASK_SLOTS[2], child_address_space: LOG_SERVICE_SNAPSHOT.address_space, child_user_frame: LOG_SERVICE_SNAPSHOT.user_frame, init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, init_endpoint_id: INIT_ENDPOINT_ID, wait_handle_slot: config.wait_handle_slot, endpoint_handle_slot: config.endpoint_handle_slot, request_byte: config.request_byte, exit_code: config.exit_code })
}

func build_echo_service_config() bootstrap_services.EchoServiceConfig {
    return bootstrap_services.echo_service_config(INIT_PID, CHILD_PID, CHILD_TID, CHILD_ASID, INIT_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT, mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE))
}

func build_echo_service_execution_state() bootstrap_services.EchoServiceExecutionState {
    return bootstrap_services.echo_service_execution_state(current_echo_service_snapshot(), current_service_runtime_state())
}

func install_echo_service_execution_state(next_state: bootstrap_services.EchoServiceExecutionState) {
    install_kernel_execution_context(KernelExecutionContext{ gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: next_state.endpoints, ready_queue: next_state.ready_queue })
    install_echo_service_snapshot(bootstrap_services.echo_service_snapshot(next_state))
}

func execute_phase106_echo_service_request_reply() bool {
    result: bootstrap_services.EchoServiceExecutionResult = bootstrap_services.execute_phase106_echo_service_request_reply(build_echo_service_config(), build_echo_service_execution_state())
    install_echo_service_execution_state(result.state)
    return result.succeeded != 0
}

func validate_phase106_echo_service_request_reply() bool {
    config: bootstrap_services.EchoServiceConfig = build_echo_service_config()
    return bootstrap_audit.validate_phase106_echo_service_request_reply(bootstrap_audit.EchoServicePhaseAudit{ program_capability: ECHO_SERVICE_SNAPSHOT.program_capability, gate: SYSCALL_GATE, spawn_observation: ECHO_SERVICE_SNAPSHOT.spawn_observation, receive_observation: ECHO_SERVICE_SNAPSHOT.receive_observation, reply_observation: ECHO_SERVICE_SNAPSHOT.reply_observation, service_state: ECHO_SERVICE_SNAPSHOT.state, exchange: ECHO_SERVICE_SNAPSHOT.exchange, wait_observation: ECHO_SERVICE_SNAPSHOT.wait_observation, wait_table: WAIT_TABLES[1], child_handle_table: HANDLE_TABLES[2], ready_queue: READY_QUEUE, child_process: PROCESS_SLOTS[2], child_task: TASK_SLOTS[2], child_address_space: ECHO_SERVICE_SNAPSHOT.address_space, child_user_frame: ECHO_SERVICE_SNAPSHOT.user_frame, init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, init_endpoint_id: INIT_ENDPOINT_ID, wait_handle_slot: config.wait_handle_slot, endpoint_handle_slot: config.endpoint_handle_slot, request_byte0: config.request_byte0, request_byte1: config.request_byte1, exit_code: config.exit_code })
}

func build_transfer_service_config() bootstrap_services.TransferServiceConfig {
    return bootstrap_services.transfer_service_config(INIT_PID, CHILD_PID, CHILD_TID, CHILD_ASID, INIT_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT, mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE), TRANSFER_SOURCE_HANDLE_SLOT, TRANSFER_RECEIVED_HANDLE_SLOT)
}

func build_transfer_service_execution_state() bootstrap_services.TransferServiceExecutionState {
    return bootstrap_services.transfer_service_execution_state(current_transfer_service_snapshot(), current_service_runtime_state())
}

func install_transfer_service_execution_state(next_state: bootstrap_services.TransferServiceExecutionState) {
    install_kernel_execution_context(KernelExecutionContext{ gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: next_state.endpoints, ready_queue: next_state.ready_queue })
    install_transfer_service_snapshot(bootstrap_services.transfer_service_snapshot(next_state))
}

func execute_phase107_user_to_user_capability_transfer() bool {
    result: bootstrap_services.TransferServiceExecutionResult = bootstrap_services.execute_phase107_user_to_user_capability_transfer(build_transfer_service_config(), build_transfer_service_execution_state())
    install_transfer_service_execution_state(result.state)
    return result.succeeded != 0
}

func validate_phase107_user_to_user_capability_transfer() bool {
    config: bootstrap_services.TransferServiceConfig = build_transfer_service_config()
    return bootstrap_audit.validate_phase107_user_to_user_capability_transfer(bootstrap_audit.TransferServicePhaseAudit{ program_capability: TRANSFER_SERVICE_SNAPSHOT.program_capability, gate: SYSCALL_GATE, spawn_observation: TRANSFER_SERVICE_SNAPSHOT.spawn_observation, grant_observation: TRANSFER_SERVICE_SNAPSHOT.grant_observation, emit_observation: TRANSFER_SERVICE_SNAPSHOT.emit_observation, service_state: TRANSFER_SERVICE_SNAPSHOT.state, transfer: TRANSFER_SERVICE_SNAPSHOT.transfer, wait_observation: TRANSFER_SERVICE_SNAPSHOT.wait_observation, init_handle_table: HANDLE_TABLES[1], wait_table: WAIT_TABLES[1], child_handle_table: HANDLE_TABLES[2], ready_queue: READY_QUEUE, child_process: PROCESS_SLOTS[2], child_task: TASK_SLOTS[2], child_address_space: TRANSFER_SERVICE_SNAPSHOT.address_space, child_user_frame: TRANSFER_SERVICE_SNAPSHOT.user_frame, init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, init_endpoint_id: INIT_ENDPOINT_ID, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, wait_handle_slot: config.wait_handle_slot, source_handle_slot: config.source_handle_slot, control_handle_slot: config.control_handle_slot, init_received_handle_slot: config.init_received_handle_slot, service_received_handle_slot: config.service_received_handle_slot, grant_byte0: config.grant_byte0, grant_byte1: config.grant_byte1, grant_byte2: config.grant_byte2, grant_byte3: config.grant_byte3, exit_code: config.exit_code })
}

func build_composition_service_config() bootstrap_services.CompositionServiceConfig {
    return bootstrap_services.composition_service_config(INIT_PID, CHILD_PID, CHILD_TID, CHILD_ASID, INIT_ENDPOINT_ID, COMPOSITION_ECHO_ENDPOINT_ID, COMPOSITION_LOG_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT, mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE))
}

func build_serial_service_config() bootstrap_services.SerialServiceConfig {
    return bootstrap_services.serial_service_config(INIT_PID, CHILD_PID, CHILD_TID, CHILD_ASID, SERIAL_SERVICE_ENDPOINT_ID, mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE))
}

func build_serial_service_execution_state() bootstrap_services.SerialServiceExecutionState {
    return bootstrap_services.serial_service_execution_state(current_serial_service_snapshot(), current_service_runtime_state())
}

func install_serial_service_execution_state(next_state: bootstrap_services.SerialServiceExecutionState) {
    install_kernel_execution_context(KernelExecutionContext{ gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: next_state.endpoints, ready_queue: next_state.ready_queue })
    SERIAL_SERVICE_SNAPSHOT = SerialServiceSnapshot{ program_capability: next_state.program_capability, address_space: next_state.child_address_space, user_frame: next_state.child_user_frame, state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, failure_observation: next_state.failure_observation, wait_observation: next_state.wait_observation }
}

func build_late_phase_proof_context() bootstrap_proofs.LatePhaseProofContext {
    return bootstrap_proofs.LatePhaseProofContext{ init_pid: INIT_PID, init_endpoint_id: INIT_ENDPOINT_ID, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, log_service_directory_key: LOG_SERVICE_DIRECTORY_KEY, echo_service_directory_key: ECHO_SERVICE_DIRECTORY_KEY, composition_service_directory_key: COMPOSITION_SERVICE_DIRECTORY_KEY, phase124_intermediary_pid: PHASE124_INTERMEDIARY_PID, phase124_final_holder_pid: PHASE124_FINAL_HOLDER_PID, phase124_control_handle_slot: PHASE124_CONTROL_HANDLE_SLOT, phase124_intermediary_receive_handle_slot: PHASE124_INTERMEDIARY_RECEIVE_HANDLE_SLOT, phase124_final_receive_handle_slot: PHASE124_FINAL_RECEIVE_HANDLE_SLOT }
}

func install_composition_service_runtime_state(next_state: bootstrap_services.CompositionServiceExecutionState) {
    bootstrap_proofs.install_composition_service_execution_state(next_state)
    install_kernel_execution_context(KernelExecutionContext{ gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: next_state.endpoints, ready_queue: next_state.ready_queue })
}

func execute_spawn_child_process() bool {
    WAIT_TABLES[1] = capability.wait_table_for_owner(INIT_PID)
    new_translation_root: mmu.TranslationRoot = mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(CHILD_WAIT_HANDLE_SLOT)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(SYSCALL_GATE, INIT_PROGRAM_CAPABILITY, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], INIT_IMAGE, spawn_request, CHILD_PID, CHILD_TID, CHILD_ASID, new_translation_root)
    SYSCALL_GATE = spawn_result.gate
    INIT_PROGRAM_CAPABILITY = spawn_result.program_capability
    PROCESS_SLOTS = spawn_result.process_slots
    TASK_SLOTS = spawn_result.task_slots
    WAIT_TABLES[1] = spawn_result.wait_table
    CHILD_EXECUTION_SNAPSHOT = ChildExecutionSnapshot{ translation_root: new_translation_root, address_space: spawn_result.child_address_space, user_frame: spawn_result.child_frame, spawn_observation: spawn_result.observation, sleep_observation: CHILD_EXECUTION_SNAPSHOT.sleep_observation, timer_wake_observation: CHILD_EXECUTION_SNAPSHOT.timer_wake_observation, wake_ready_queue: CHILD_EXECUTION_SNAPSHOT.wake_ready_queue, pre_exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.pre_exit_wait_observation, exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.exit_wait_observation }
    READY_QUEUE = state.user_ready_queue(CHILD_TID)
    return syscall.status_score(CHILD_EXECUTION_SNAPSHOT.spawn_observation.status) == 2
}

func execute_child_sleep_transition() bool {
    sleep_result: syscall.SleepResult = syscall.perform_sleep(SYSCALL_GATE, TASK_SLOTS, TIMER_STATE, syscall.build_sleep_request(CHILD_TASK_SLOT, 1))
    SYSCALL_GATE = sleep_result.gate
    TASK_SLOTS = sleep_result.task_slots
    TIMER_STATE = sleep_result.timer_state
    CHILD_EXECUTION_SNAPSHOT = ChildExecutionSnapshot{ translation_root: CHILD_EXECUTION_SNAPSHOT.translation_root, address_space: CHILD_EXECUTION_SNAPSHOT.address_space, user_frame: CHILD_EXECUTION_SNAPSHOT.user_frame, spawn_observation: CHILD_EXECUTION_SNAPSHOT.spawn_observation, sleep_observation: sleep_result.observation, timer_wake_observation: CHILD_EXECUTION_SNAPSHOT.timer_wake_observation, wake_ready_queue: CHILD_EXECUTION_SNAPSHOT.wake_ready_queue, pre_exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.pre_exit_wait_observation, exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.exit_wait_observation }
    READY_QUEUE = state.empty_queue()
    if syscall.status_score(CHILD_EXECUTION_SNAPSHOT.sleep_observation.status) != 4 {
        return false
    }
    if syscall.id_score(SYSCALL_GATE.last_id) != 32 {
        return false
    }
    if syscall.status_score(SYSCALL_GATE.last_status) != 4 {
        return false
    }
    if state.task_state_score(TASK_SLOTS[2].state) != 8 {
        return false
    }
    if CHILD_EXECUTION_SNAPSHOT.sleep_observation.task_id != CHILD_TID {
        return false
    }
    return CHILD_EXECUTION_SNAPSHOT.sleep_observation.deadline_tick == 1
}

func execute_child_timer_wake_transition() bool {
    interrupt_entry: interrupt.InterruptEntry = interrupt.arch_enter_interrupt(INTERRUPTS, 32, ARCH_ACTOR)
    INTERRUPTS = interrupt_entry.controller
    dispatch_result: interrupt.InterruptDispatchResult = interrupt.dispatch_interrupt(interrupt_entry)
    INTERRUPTS = dispatch_result.controller
    LAST_INTERRUPT_KIND = dispatch_result.kind
    delivery: timer.TimerInterruptDelivery = timer.deliver_interrupt_tick(TIMER_STATE, 1)
    TIMER_STATE = delivery.timer_state
    CHILD_EXECUTION_SNAPSHOT = ChildExecutionSnapshot{ translation_root: CHILD_EXECUTION_SNAPSHOT.translation_root, address_space: CHILD_EXECUTION_SNAPSHOT.address_space, user_frame: CHILD_EXECUTION_SNAPSHOT.user_frame, spawn_observation: CHILD_EXECUTION_SNAPSHOT.spawn_observation, sleep_observation: CHILD_EXECUTION_SNAPSHOT.sleep_observation, timer_wake_observation: delivery.observation, wake_ready_queue: CHILD_EXECUTION_SNAPSHOT.wake_ready_queue, pre_exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.pre_exit_wait_observation, exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.exit_wait_observation }
    wake_transition: lifecycle.TaskTransition = lifecycle.ready_task(TASK_SLOTS, 2)
    TASK_SLOTS = wake_transition.task_slots
    READY_QUEUE = state.user_ready_queue(CHILD_TID)
    CHILD_EXECUTION_SNAPSHOT = ChildExecutionSnapshot{ translation_root: CHILD_EXECUTION_SNAPSHOT.translation_root, address_space: CHILD_EXECUTION_SNAPSHOT.address_space, user_frame: CHILD_EXECUTION_SNAPSHOT.user_frame, spawn_observation: CHILD_EXECUTION_SNAPSHOT.spawn_observation, sleep_observation: CHILD_EXECUTION_SNAPSHOT.sleep_observation, timer_wake_observation: CHILD_EXECUTION_SNAPSHOT.timer_wake_observation, wake_ready_queue: READY_QUEUE, pre_exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.pre_exit_wait_observation, exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.exit_wait_observation }
    if CHILD_EXECUTION_SNAPSHOT.timer_wake_observation.task_id != CHILD_TID {
        return false
    }
    if CHILD_EXECUTION_SNAPSHOT.timer_wake_observation.deadline_tick != 1 {
        return false
    }
    if CHILD_EXECUTION_SNAPSHOT.timer_wake_observation.wake_tick != 1 {
        return false
    }
    if CHILD_EXECUTION_SNAPSHOT.timer_wake_observation.wake_count != 1 {
        return false
    }
    if interrupt.dispatch_kind_score(LAST_INTERRUPT_KIND) != 2 {
        return false
    }
    if TIMER_STATE.monotonic_tick != 1 {
        return false
    }
    if TIMER_STATE.wake_count != 1 {
        return false
    }
    if TIMER_STATE.count != 0 {
        return false
    }
    return state.task_state_score(TASK_SLOTS[2].state) == 4
}

func execute_child_pre_exit_wait() bool {
    pre_wait_result: syscall.WaitResult = syscall.perform_wait(SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], syscall.build_wait_request(CHILD_WAIT_HANDLE_SLOT))
    SYSCALL_GATE = pre_wait_result.gate
    PROCESS_SLOTS = pre_wait_result.process_slots
    TASK_SLOTS = pre_wait_result.task_slots
    WAIT_TABLES[1] = pre_wait_result.wait_table
    CHILD_EXECUTION_SNAPSHOT = ChildExecutionSnapshot{ translation_root: CHILD_EXECUTION_SNAPSHOT.translation_root, address_space: CHILD_EXECUTION_SNAPSHOT.address_space, user_frame: CHILD_EXECUTION_SNAPSHOT.user_frame, spawn_observation: CHILD_EXECUTION_SNAPSHOT.spawn_observation, sleep_observation: CHILD_EXECUTION_SNAPSHOT.sleep_observation, timer_wake_observation: CHILD_EXECUTION_SNAPSHOT.timer_wake_observation, wake_ready_queue: CHILD_EXECUTION_SNAPSHOT.wake_ready_queue, pre_exit_wait_observation: pre_wait_result.observation, exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.exit_wait_observation }
    return syscall.status_score(CHILD_EXECUTION_SNAPSHOT.pre_exit_wait_observation.status) == 4
}

func simulate_child_exit() {
    processes: [3]state.ProcessSlot = PROCESS_SLOTS
    tasks: [3]state.TaskSlot = TASK_SLOTS
    wait_tables: [3]capability.WaitTable = WAIT_TABLES
    processes = lifecycle.exit_process(processes, 2)
    tasks = lifecycle.exit_task(tasks, 2)
    wait_tables[1] = capability.mark_wait_handle_exited(wait_tables[1], CHILD_PID, CHILD_EXIT_CODE)
    PROCESS_SLOTS = processes
    TASK_SLOTS = tasks
    WAIT_TABLES = wait_tables
    READY_QUEUE = state.empty_queue()
}

func execute_child_post_exit_wait() bool {
    wait_result: syscall.WaitResult = syscall.perform_wait(SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], syscall.build_wait_request(CHILD_WAIT_HANDLE_SLOT))
    SYSCALL_GATE = wait_result.gate
    PROCESS_SLOTS = wait_result.process_slots
    TASK_SLOTS = wait_result.task_slots
    WAIT_TABLES[1] = wait_result.wait_table
    CHILD_EXECUTION_SNAPSHOT = ChildExecutionSnapshot{ translation_root: CHILD_EXECUTION_SNAPSHOT.translation_root, address_space: CHILD_EXECUTION_SNAPSHOT.address_space, user_frame: CHILD_EXECUTION_SNAPSHOT.user_frame, spawn_observation: CHILD_EXECUTION_SNAPSHOT.spawn_observation, sleep_observation: CHILD_EXECUTION_SNAPSHOT.sleep_observation, timer_wake_observation: CHILD_EXECUTION_SNAPSHOT.timer_wake_observation, wake_ready_queue: CHILD_EXECUTION_SNAPSHOT.wake_ready_queue, pre_exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.pre_exit_wait_observation, exit_wait_observation: wait_result.observation }
    READY_QUEUE = state.empty_queue()
    return syscall.status_score(CHILD_EXECUTION_SNAPSHOT.exit_wait_observation.status) == 2
}

func execute_program_cap_spawn_and_wait() bool {
    if !execute_spawn_child_process() {
        return false
    }
    if !execute_child_sleep_transition() {
        return false
    }
    if !execute_child_timer_wake_transition() {
        return false
    }
    if !execute_child_pre_exit_wait() {
        return false
    }
    simulate_child_exit()
    return execute_child_post_exit_wait()
}

func build_scheduler_lifecycle_audit() sched.LifecycleAudit {
    return sched.LifecycleAudit{ init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, child_translation_root: CHILD_EXECUTION_SNAPSHOT.translation_root, child_exit_code: CHILD_EXIT_CODE, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT, init_entry_pc: INIT_IMAGE.entry_pc, init_stack_top: INIT_IMAGE.stack_top, spawn: CHILD_EXECUTION_SNAPSHOT.spawn_observation, pre_exit_wait: CHILD_EXECUTION_SNAPSHOT.pre_exit_wait_observation, exit_wait: CHILD_EXECUTION_SNAPSHOT.exit_wait_observation, sleep: CHILD_EXECUTION_SNAPSHOT.sleep_observation, timer_wake: CHILD_EXECUTION_SNAPSHOT.timer_wake_observation, timer_state: TIMER_STATE, wake_ready_queue: CHILD_EXECUTION_SNAPSHOT.wake_ready_queue, wait_table: WAIT_TABLES[1], child_process: PROCESS_SLOTS[2], child_task: TASK_SLOTS[2], child_address_space: CHILD_EXECUTION_SNAPSHOT.address_space, child_user_frame: CHILD_EXECUTION_SNAPSHOT.user_frame, ready_queue: READY_QUEUE }
}

func emit_late_phase_markers() {
    // Phase 151 is intentionally absent; phase 152 proves the stabilized rebuilt-system pattern.
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE140_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE141_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE142_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE143_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE144_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE145_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE146_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE147_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE148_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE149_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE150_MARKER_DETAIL)
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE152_MARKER_DETAIL)
}

func validate_late_phase_boot_log() i32 {
    if BOOT_LOG_APPEND_FAILED != 0 {
        return 109
    }
    if BOOT_LOG.count < 16 {
        return 108
    }
    if BOOT_LOG.count != 16 {
        return 110
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 3)) != 8 {
        return 111
    }
    if state.log_actor_at(BOOT_LOG, 3) != ARCH_ACTOR {
        return 112
    }
    if state.log_detail_at(BOOT_LOG, 3) != INIT_TID {
        return 113
    }
    phase140_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 4, detail: PHASE140_MARKER_DETAIL, stage_error: 114, actor_error: 115, detail_error: 116 })
    if phase140_status != 0 {
        return phase140_status
    }
    phase141_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 5, detail: PHASE141_MARKER_DETAIL, stage_error: 117, actor_error: 118, detail_error: 119 })
    if phase141_status != 0 {
        return phase141_status
    }
    phase142_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 6, detail: PHASE142_MARKER_DETAIL, stage_error: 120, actor_error: 121, detail_error: 122 })
    if phase142_status != 0 {
        return phase142_status
    }
    phase143_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 7, detail: PHASE143_MARKER_DETAIL, stage_error: 123, actor_error: 124, detail_error: 125 })
    if phase143_status != 0 {
        return phase143_status
    }
    phase144_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 8, detail: PHASE144_MARKER_DETAIL, stage_error: 126, actor_error: 127, detail_error: 128 })
    if phase144_status != 0 {
        return phase144_status
    }
    phase145_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 9, detail: PHASE145_MARKER_DETAIL, stage_error: 129, actor_error: 130, detail_error: 131 })
    if phase145_status != 0 {
        return phase145_status
    }
    phase146_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 10, detail: PHASE146_MARKER_DETAIL, stage_error: 132, actor_error: 133, detail_error: 134 })
    if phase146_status != 0 {
        return phase146_status
    }
    phase147_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 11, detail: PHASE147_MARKER_DETAIL, stage_error: 135, actor_error: 136, detail_error: 137 })
    if phase147_status != 0 {
        return phase147_status
    }
    phase148_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 12, detail: PHASE148_MARKER_DETAIL, stage_error: 138, actor_error: 139, detail_error: 140 })
    if phase148_status != 0 {
        return phase148_status
    }
    phase149_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 13, detail: PHASE149_MARKER_DETAIL, stage_error: 141, actor_error: 142, detail_error: 143 })
    if phase149_status != 0 {
        return phase149_status
    }
    phase150_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 14, detail: PHASE150_MARKER_DETAIL, stage_error: 144, actor_error: 145, detail_error: 146 })
    if phase150_status != 0 {
        return phase150_status
    }
    phase152_status: i32 = validate_late_phase_boot_log_entry(LateBootLogExpectation{ index: 15, detail: PHASE152_MARKER_DETAIL, stage_error: 147, actor_error: 148, detail_error: 149 })
    if phase152_status != 0 {
        return phase152_status
    }
    return 0
}

func execute_late_service_system_proofs(mid_phase_result: bootstrap_proofs.MidPhaseProofResult) i32 {
    proof_contracts: state.ContractState = state.contract_state(mid_phase_result.proof_contract_flags.scheduler_contract_hardened, mid_phase_result.proof_contract_flags.lifecycle_contract_hardened, mid_phase_result.proof_contract_flags.capability_contract_hardened, mid_phase_result.proof_contract_flags.ipc_contract_hardened, mid_phase_result.proof_contract_flags.address_space_contract_hardened, mid_phase_result.proof_contract_flags.interrupt_contract_hardened, mid_phase_result.proof_contract_flags.timer_contract_hardened, mid_phase_result.proof_contract_flags.barrier_contract_hardened)
    runtime_context: bootstrap_services.LateServiceSystemRuntimeContext = bootstrap_services.LateServiceSystemRuntimeContext{ identity: state.identity_config(state.task_identity(BOOT_PID, BOOT_TID, 0), state.task_identity(INIT_PID, INIT_TID, INIT_ASID), state.task_identity(CHILD_PID, CHILD_TID, CHILD_ASID)), phase141_shell_pid: PHASE141_SHELL_PID, phase141_kv_pid: PHASE141_KV_PID, init_endpoint_id: INIT_ENDPOINT_ID, serial_service_endpoint_id: SERIAL_SERVICE_ENDPOINT_ID, composition_echo_endpoint_id: COMPOSITION_ECHO_ENDPOINT_ID, composition_log_endpoint_id: COMPOSITION_LOG_ENDPOINT_ID, kv_service_endpoint_id: KV_SERVICE_ENDPOINT_ID, shell_service_endpoint_id: SHELL_SERVICE_ENDPOINT_ID, log_service_directory_key: LOG_SERVICE_DIRECTORY_KEY, kv_service_directory_key: KV_SERVICE_DIRECTORY_KEY, init_root_page_table: INIT_ROOT_PAGE_TABLE, child_root_page_table: CHILD_ROOT_PAGE_TABLE, init_image: INIT_IMAGE, serial_receive_observation: SERIAL_SERVICE_SNAPSHOT.receive_observation, services: bootstrap_state.service_state(LOG_SERVICE_SNAPSHOT.state, ECHO_SERVICE_SNAPSHOT.state, SERIAL_SERVICE_SNAPSHOT.state, kv_service.service_state(PHASE141_KV_PID, 1), shell_service.service_state(PHASE141_SHELL_PID, 1, COMPOSITION_ECHO_ENDPOINT_ID, COMPOSITION_LOG_ENDPOINT_ID, KV_SERVICE_ENDPOINT_ID)) }
    return bootstrap_services.execute_late_service_system_proofs(bootstrap_services.build_late_service_system_proof_inputs(mid_phase_result.phase137_audit, proof_contracts, runtime_context))
}

func execute_mid_phase_proofs() i32 {
    mid_phase_execution: bootstrap_proofs.MidPhaseProofExecutionResult = bootstrap_proofs.execute_mid_phase_proofs(build_mid_phase_proof_runtime_context())
    if mid_phase_execution.status != 0 {
        return mid_phase_execution.status
    }

    install_mid_phase_proof_result(mid_phase_execution.proof_result)
    return execute_late_service_system_proofs(mid_phase_execution.proof_result)
}

func bootstrap_main() i32 {
    if !architecture_entry() {
        return 10
    }
    if !transfer_to_first_user_entry() {
        return 11
    }
    bootstrap_endpoint_handle_core()
    if KERNEL.booted != 1 {
        return 12
    }
    if KERNEL.current_pid != INIT_PID {
        return 13
    }
    if KERNEL.current_tid != INIT_TID {
        return 14
    }
    if KERNEL.active_asid != INIT_ASID {
        return 15
    }
    if KERNEL.user_entry_started != 1 {
        return 16
    }
    if address_space.state_score(INIT_RUNTIME_SNAPSHOT.address_space.state) != 4 {
        return 17
    }
    if !validate_endpoint_handle_core() {
        return 18
    }
    if !handoff_init_bootstrap_capability_set() {
        return 19
    }
    if !validate_init_bootstrap_capability_handoff() {
        return 20
    }
    if !execute_syscall_byte_ipc() {
        return 21
    }
    if !validate_syscall_byte_ipc() {
        return 22
    }
    if !seed_transfer_endpoint_handle() {
        return 23
    }
    if !execute_capability_carrying_ipc_transfer() {
        return 24
    }
    if !validate_capability_carrying_ipc_transfer() {
        return 25
    }
    if !execute_program_cap_spawn_and_wait() {
        return 26
    }
    mid_phase_status: i32 = execute_mid_phase_proofs()
    if mid_phase_status != 0 {
        return mid_phase_status
    }

    emit_late_phase_markers()
    late_phase_boot_log_status: i32 = validate_late_phase_boot_log()
    if late_phase_boot_log_status != 0 {
        return late_phase_boot_log_status
    }
    if PROCESS_SLOTS[1].pid != INIT_PID {
        return 147
    }
    if TASK_SLOTS[1].tid != INIT_TID {
        return 148
    }
    if INIT_RUNTIME_SNAPSHOT.user_frame.task_id != INIT_TID {
        return 149
    }
    return 0
}
