import address_space
import bootstrap_audit
import bootstrap_proofs
import bootstrap_runtime
import bootstrap_services
import capability
import debug
import echo_service
import ipc
import init
import interrupt
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

var KERNEL: state.KernelDescriptor
var PROCESS_SLOTS: [3]state.ProcessSlot
var TASK_SLOTS: [3]state.TaskSlot
var READY_QUEUE: state.ReadyQueue
var BOOT_LOG: state.BootLog
var BOOT_LOG_APPEND_FAILED: u32
var ROOT_CAPABILITY: capability.CapabilitySlot
var INIT_PROGRAM_CAPABILITY: capability.CapabilitySlot
var LOG_SERVICE_SNAPSHOT: bootstrap_services.LogServiceSnapshot
var ECHO_SERVICE_SNAPSHOT: bootstrap_services.EchoServiceSnapshot
var TRANSFER_SERVICE_SNAPSHOT: bootstrap_services.TransferServiceSnapshot
var HANDLE_TABLES: [3]capability.HandleTable
var WAIT_TABLES: [3]capability.WaitTable
var ENDPOINTS: ipc.EndpointTable
var INTERRUPTS: interrupt.InterruptController
var LAST_INTERRUPT_KIND: interrupt.InterruptDispatchKind
var UART_DEVICE: uart.UartDevice
var INIT_RUNTIME_SNAPSHOT: bootstrap_runtime.InitRuntimeSnapshot
var CHILD_EXECUTION_SNAPSHOT: bootstrap_runtime.ChildExecutionSnapshot
var SYSCALL_GATE: syscall.SyscallGate
var INIT_IMAGE: init.InitImage
var TIMER_STATE: timer.TimerState
var DELIVERED_MESSAGE: ipc.KernelMessage
var RECEIVE_OBSERVATION: syscall.ReceiveObservation
var ATTACHED_RECEIVE_OBSERVATION: syscall.ReceiveObservation
var TRANSFERRED_HANDLE_USE_OBSERVATION: syscall.ReceiveObservation
var SERIAL_SERVICE_SNAPSHOT: bootstrap_services.SerialServiceSnapshot
var UART_INGRESS: uart.UartIngressObservation

func empty_log_service_snapshot() bootstrap_services.LogServiceSnapshot {
    return bootstrap_services.empty_log_service_snapshot()
}

func empty_echo_service_snapshot() bootstrap_services.EchoServiceSnapshot {
    return bootstrap_services.empty_echo_service_snapshot()
}

func empty_transfer_service_snapshot() bootstrap_services.TransferServiceSnapshot {
    return bootstrap_services.empty_transfer_service_snapshot()
}

func empty_serial_service_snapshot() bootstrap_services.SerialServiceSnapshot {
    return bootstrap_services.empty_serial_service_snapshot()
}

func empty_init_runtime_snapshot() bootstrap_runtime.InitRuntimeSnapshot {
    return bootstrap_runtime.empty_init_runtime_snapshot()
}

func empty_child_execution_snapshot() bootstrap_runtime.ChildExecutionSnapshot {
    return bootstrap_runtime.empty_child_execution_snapshot()
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
    result: bootstrap_runtime.InitAddressSpaceResult = bootstrap_runtime.construct_first_user_address_space(INIT_PID, INIT_TID, INIT_ASID, INIT_TASK_SLOT, INIT_ROOT_PAGE_TABLE, INIT_IMAGE, INIT_RUNTIME_SNAPSHOT, PROCESS_SLOTS, TASK_SLOTS)
    if result.succeeded == 0 {
        INIT_RUNTIME_SNAPSHOT = result.snapshot
        return false
    }
    INIT_PROGRAM_CAPABILITY = result.program_capability
    PROCESS_SLOTS = result.process_slots
    TASK_SLOTS = result.task_slots
    READY_QUEUE = result.ready_queue
    INIT_RUNTIME_SNAPSHOT = result.snapshot
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
    result: bootstrap_runtime.FirstUserEntryTransferResult = bootstrap_runtime.transfer_to_first_user_entry(KERNEL, INIT_PID, INIT_TID, INIT_ASID, INIT_RUNTIME_SNAPSHOT)
    KERNEL = result.kernel
    INIT_RUNTIME_SNAPSHOT = result.snapshot
    if result.succeeded == 0 {
        return false
    }
    record_boot_stage(state.BootStage.UserEntryReady, INIT_TID)
    return true
}

func bootstrap_endpoint_handle_core() {
    result: bootstrap_runtime.EndpointBootstrapResult = bootstrap_runtime.bootstrap_endpoint_handle_core(BOOT_PID, INIT_PID, INIT_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT, HANDLE_TABLES, ENDPOINTS)
    HANDLE_TABLES = result.handle_tables
    ENDPOINTS = result.endpoints
    DELIVERED_MESSAGE = result.delivered_message
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
    result: bootstrap_runtime.BootstrapCapabilityHandoffResult = bootstrap_runtime.handoff_init_bootstrap_capability_set(INIT_PID, INIT_ENDPOINT_HANDLE_SLOT, INIT_PROGRAM_CAPABILITY, INIT_RUNTIME_SNAPSHOT)
    INIT_RUNTIME_SNAPSHOT = result.snapshot
    return result.succeeded != 0
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
    result: bootstrap_runtime.SyscallByteIpcResult = bootstrap_runtime.execute_syscall_byte_ipc(INIT_PID, INIT_ENDPOINT_HANDLE_SLOT, SYSCALL_GATE, HANDLE_TABLES, ENDPOINTS)
    SYSCALL_GATE = result.gate
    HANDLE_TABLES = result.handle_tables
    ENDPOINTS = result.endpoints
    RECEIVE_OBSERVATION = result.observation
    return result.succeeded != 0
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
    result: bootstrap_runtime.TransferEndpointSeedResult = bootstrap_runtime.seed_transfer_endpoint_handle(INIT_PID, TRANSFER_ENDPOINT_ID, TRANSFER_SOURCE_HANDLE_SLOT, HANDLE_TABLES, ENDPOINTS)
    HANDLE_TABLES = result.handle_tables
    ENDPOINTS = result.endpoints
    return result.succeeded != 0
}

func execute_capability_carrying_ipc_transfer() bool {
    result: bootstrap_runtime.CapabilityCarryingIpcTransferResult = bootstrap_runtime.execute_capability_carrying_ipc_transfer(INIT_PID, INIT_ENDPOINT_HANDLE_SLOT, TRANSFER_SOURCE_HANDLE_SLOT, TRANSFER_RECEIVED_HANDLE_SLOT, TRANSFER_ENDPOINT_ID, SYSCALL_GATE, HANDLE_TABLES, ENDPOINTS)
    SYSCALL_GATE = result.gate
    HANDLE_TABLES = result.handle_tables
    ENDPOINTS = result.endpoints
    ATTACHED_RECEIVE_OBSERVATION = result.attached_receive_observation
    TRANSFERRED_HANDLE_USE_OBSERVATION = result.transferred_handle_use_observation
    return result.succeeded != 0
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

func install_service_runtime_state(next_state: bootstrap_services.ServiceRuntimeState) {
    SYSCALL_GATE = next_state.gate
    PROCESS_SLOTS = next_state.process_slots
    TASK_SLOTS = next_state.task_slots
    HANDLE_TABLES[1] = next_state.init_handle_table
    HANDLE_TABLES[2] = next_state.child_handle_table
    WAIT_TABLES[1] = next_state.wait_table
    ENDPOINTS = next_state.endpoints
    READY_QUEUE = next_state.ready_queue
}

func validate_phase104_contract_hardening() bool {
    timer_hardened: u32 = 0
    if bootstrap_audit.validate_timer_hardening_contracts() {
        timer_hardened = 1
    }
    return bootstrap_audit.validate_phase104_contract_hardening(bootstrap_audit.Phase104HardeningAudit{ boot_log_append_failed: BOOT_LOG_APPEND_FAILED, timer_hardened: timer_hardened, bootstrap_layout: bootstrap_audit.BootstrapLayoutAudit{ init_image: INIT_IMAGE, init_root_page_table: INIT_ROOT_PAGE_TABLE }, endpoint_capability: bootstrap_audit.EndpointCapabilityAudit{ init_pid: INIT_PID, init_endpoint_id: INIT_ENDPOINT_ID, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT }, state_hardening: bootstrap_audit.StateHardeningAudit{ boot_tid: BOOT_TID, boot_pid: BOOT_PID, boot_entry_pc: BOOT_ENTRY_PC, boot_stack_top: BOOT_STACK_TOP, child_tid: CHILD_TID, child_pid: CHILD_PID, child_asid: CHILD_ASID, init_image: INIT_IMAGE, arch_actor: ARCH_ACTOR }, syscall_hardening: bootstrap_audit.SyscallHardeningAudit{ init_pid: INIT_PID, init_endpoint_handle_slot: INIT_ENDPOINT_HANDLE_SLOT, init_endpoint_id: INIT_ENDPOINT_ID, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, child_root_page_table: CHILD_ROOT_PAGE_TABLE, boot_pid: BOOT_PID, boot_tid: BOOT_TID, boot_task_slot: BOOT_TASK_SLOT, boot_entry_pc: BOOT_ENTRY_PC, boot_stack_top: BOOT_STACK_TOP, init_image: INIT_IMAGE, transfer_source_handle_slot: TRANSFER_SOURCE_HANDLE_SLOT } })
}

func build_log_service_config() bootstrap_services.LogServiceConfig {
    return bootstrap_services.log_service_config(INIT_PID, CHILD_PID, CHILD_TID, CHILD_ASID, INIT_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT, mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE))
}

func build_service_runtime_state() bootstrap_services.ServiceRuntimeState {
    return bootstrap_services.service_runtime_state(SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, HANDLE_TABLES[1], HANDLE_TABLES[2], WAIT_TABLES[1], ENDPOINTS, INIT_IMAGE, READY_QUEUE)
}

func build_log_service_execution_state() bootstrap_services.LogServiceExecutionState {
    return bootstrap_services.log_service_execution_state(LOG_SERVICE_SNAPSHOT, build_service_runtime_state())
}

func execute_phase105_log_service_handshake() bool {
    result: bootstrap_services.LogServiceExecutionResult = bootstrap_services.execute_phase105_log_service_handshake(build_log_service_config(), build_log_service_execution_state())
    install_service_runtime_state(bootstrap_services.service_runtime_state_from_log_execution(result.state))
    LOG_SERVICE_SNAPSHOT = bootstrap_services.log_service_snapshot(result.state)
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
    return bootstrap_services.echo_service_execution_state(ECHO_SERVICE_SNAPSHOT, build_service_runtime_state())
}

func execute_phase106_echo_service_request_reply() bool {
    result: bootstrap_services.EchoServiceExecutionResult = bootstrap_services.execute_phase106_echo_service_request_reply(build_echo_service_config(), build_echo_service_execution_state())
    install_service_runtime_state(bootstrap_services.service_runtime_state_from_echo_execution(result.state))
    ECHO_SERVICE_SNAPSHOT = bootstrap_services.echo_service_snapshot(result.state)
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
    return bootstrap_services.transfer_service_execution_state(TRANSFER_SERVICE_SNAPSHOT, build_service_runtime_state())
}

func execute_phase107_user_to_user_capability_transfer() bool {
    result: bootstrap_services.TransferServiceExecutionResult = bootstrap_services.execute_phase107_user_to_user_capability_transfer(build_transfer_service_config(), build_transfer_service_execution_state())
    install_service_runtime_state(bootstrap_services.service_runtime_state_from_transfer_execution(result.state))
    TRANSFER_SERVICE_SNAPSHOT = bootstrap_services.transfer_service_snapshot(result.state)
    return result.succeeded != 0
}

func validate_phase107_user_to_user_capability_transfer() bool {
    config: bootstrap_services.TransferServiceConfig = build_transfer_service_config()
    return bootstrap_audit.validate_phase107_user_to_user_capability_transfer(bootstrap_audit.TransferServicePhaseAudit{ program_capability: TRANSFER_SERVICE_SNAPSHOT.program_capability, gate: SYSCALL_GATE, spawn_observation: TRANSFER_SERVICE_SNAPSHOT.spawn_observation, grant_observation: TRANSFER_SERVICE_SNAPSHOT.grant_observation, emit_observation: TRANSFER_SERVICE_SNAPSHOT.emit_observation, service_state: TRANSFER_SERVICE_SNAPSHOT.state, transfer: TRANSFER_SERVICE_SNAPSHOT.transfer, wait_observation: TRANSFER_SERVICE_SNAPSHOT.wait_observation, init_handle_table: HANDLE_TABLES[1], wait_table: WAIT_TABLES[1], child_handle_table: HANDLE_TABLES[2], ready_queue: READY_QUEUE, child_process: PROCESS_SLOTS[2], child_task: TASK_SLOTS[2], child_address_space: TRANSFER_SERVICE_SNAPSHOT.address_space, child_user_frame: TRANSFER_SERVICE_SNAPSHOT.user_frame, init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, init_endpoint_id: INIT_ENDPOINT_ID, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, wait_handle_slot: config.wait_handle_slot, source_handle_slot: config.source_handle_slot, control_handle_slot: config.control_handle_slot, init_received_handle_slot: config.init_received_handle_slot, service_received_handle_slot: config.service_received_handle_slot, grant_byte0: config.grant_byte0, grant_byte1: config.grant_byte1, grant_byte2: config.grant_byte2, grant_byte3: config.grant_byte3, exit_code: config.exit_code })
}

func install_serial_service_execution_state(next_state: bootstrap_services.SerialServiceExecutionState) {
    install_service_runtime_state(bootstrap_services.service_runtime_state_from_serial_execution(next_state))
    SERIAL_SERVICE_SNAPSHOT = bootstrap_services.serial_service_snapshot(next_state)
}

func build_late_phase_proof_context() bootstrap_proofs.LatePhaseProofContext {
    return bootstrap_proofs.LatePhaseProofContext{ init_pid: INIT_PID, init_endpoint_id: INIT_ENDPOINT_ID, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, log_service_directory_key: LOG_SERVICE_DIRECTORY_KEY, echo_service_directory_key: ECHO_SERVICE_DIRECTORY_KEY, composition_service_directory_key: COMPOSITION_SERVICE_DIRECTORY_KEY, phase124_intermediary_pid: PHASE124_INTERMEDIARY_PID, phase124_final_holder_pid: PHASE124_FINAL_HOLDER_PID, phase124_control_handle_slot: PHASE124_CONTROL_HANDLE_SLOT, phase124_intermediary_receive_handle_slot: PHASE124_INTERMEDIARY_RECEIVE_HANDLE_SLOT, phase124_final_receive_handle_slot: PHASE124_FINAL_RECEIVE_HANDLE_SLOT }
}

func install_composition_service_runtime_state(next_state: bootstrap_services.CompositionServiceExecutionState) {
    bootstrap_proofs.install_composition_service_execution_state(next_state)
    install_service_runtime_state(bootstrap_services.service_runtime_state_from_composition_execution(next_state))
}

func build_child_execution_config() bootstrap_runtime.ChildExecutionConfig {
    return bootstrap_runtime.ChildExecutionConfig{ init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_task_slot: CHILD_TASK_SLOT, child_asid: CHILD_ASID, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT, child_root_page_table: CHILD_ROOT_PAGE_TABLE, child_exit_code: CHILD_EXIT_CODE, timer_interrupt_vector: 32, interrupt_actor: ARCH_ACTOR }
}

func install_child_execution_result(next_result: bootstrap_runtime.ChildExecutionResult) {
    INIT_PROGRAM_CAPABILITY = next_result.init_program_capability
    SYSCALL_GATE = next_result.gate
    PROCESS_SLOTS = next_result.process_slots
    TASK_SLOTS = next_result.task_slots
    WAIT_TABLES = next_result.wait_tables
    READY_QUEUE = next_result.ready_queue
    TIMER_STATE = next_result.timer_state
    INTERRUPTS = next_result.interrupts
    LAST_INTERRUPT_KIND = next_result.last_interrupt_kind
    CHILD_EXECUTION_SNAPSHOT = next_result.snapshot
}

func execute_program_cap_spawn_and_wait() bool {
    result: bootstrap_runtime.ChildExecutionResult = bootstrap_runtime.execute_program_cap_spawn_and_wait(build_child_execution_config(), INIT_PROGRAM_CAPABILITY, SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES, READY_QUEUE, TIMER_STATE, INTERRUPTS, LAST_INTERRUPT_KIND, INIT_IMAGE, CHILD_EXECUTION_SNAPSHOT)
    install_child_execution_result(result)
    return result.succeeded != 0
}

func build_scheduler_lifecycle_audit() sched.LifecycleAudit {
    return bootstrap_runtime.build_scheduler_lifecycle_audit(build_child_execution_config(), INIT_IMAGE, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES, READY_QUEUE, TIMER_STATE, CHILD_EXECUTION_SNAPSHOT)
}

func emit_late_phase_markers() {
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
}

func validate_late_phase_boot_log() i32 {
    if BOOT_LOG_APPEND_FAILED != 0 {
        return 109
    }
    if BOOT_LOG.count < 15 {
        return 108
    }
    if BOOT_LOG.count != 15 {
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
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 4)) != 16 {
        return 114
    }
    if state.log_actor_at(BOOT_LOG, 4) != ARCH_ACTOR {
        return 115
    }
    if state.log_detail_at(BOOT_LOG, 4) != PHASE140_MARKER_DETAIL {
        return 116
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 5)) != 16 {
        return 117
    }
    if state.log_actor_at(BOOT_LOG, 5) != ARCH_ACTOR {
        return 118
    }
    if state.log_detail_at(BOOT_LOG, 5) != PHASE141_MARKER_DETAIL {
        return 119
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 6)) != 16 {
        return 120
    }
    if state.log_actor_at(BOOT_LOG, 6) != ARCH_ACTOR {
        return 121
    }
    if state.log_detail_at(BOOT_LOG, 6) != PHASE142_MARKER_DETAIL {
        return 122
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 7)) != 16 {
        return 123
    }
    if state.log_actor_at(BOOT_LOG, 7) != ARCH_ACTOR {
        return 124
    }
    if state.log_detail_at(BOOT_LOG, 7) != PHASE143_MARKER_DETAIL {
        return 125
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 8)) != 16 {
        return 126
    }
    if state.log_actor_at(BOOT_LOG, 8) != ARCH_ACTOR {
        return 127
    }
    if state.log_detail_at(BOOT_LOG, 8) != PHASE144_MARKER_DETAIL {
        return 128
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 9)) != 16 {
        return 129
    }
    if state.log_actor_at(BOOT_LOG, 9) != ARCH_ACTOR {
        return 130
    }
    if state.log_detail_at(BOOT_LOG, 9) != PHASE145_MARKER_DETAIL {
        return 131
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 10)) != 16 {
        return 132
    }
    if state.log_actor_at(BOOT_LOG, 10) != ARCH_ACTOR {
        return 133
    }
    if state.log_detail_at(BOOT_LOG, 10) != PHASE146_MARKER_DETAIL {
        return 134
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 11)) != 16 {
        return 135
    }
    if state.log_actor_at(BOOT_LOG, 11) != ARCH_ACTOR {
        return 136
    }
    if state.log_detail_at(BOOT_LOG, 11) != PHASE147_MARKER_DETAIL {
        return 137
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 12)) != 16 {
        return 138
    }
    if state.log_actor_at(BOOT_LOG, 12) != ARCH_ACTOR {
        return 139
    }
    if state.log_detail_at(BOOT_LOG, 12) != PHASE148_MARKER_DETAIL {
        return 140
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 13)) != 16 {
        return 141
    }
    if state.log_actor_at(BOOT_LOG, 13) != ARCH_ACTOR {
        return 142
    }
    if state.log_detail_at(BOOT_LOG, 13) != PHASE149_MARKER_DETAIL {
        return 143
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 14)) != 16 {
        return 144
    }
    if state.log_actor_at(BOOT_LOG, 14) != ARCH_ACTOR {
        return 145
    }
    if state.log_detail_at(BOOT_LOG, 14) != PHASE150_MARKER_DETAIL {
        return 146
    }
    return 0
}

func build_late_service_system_runtime_context() bootstrap_services.LateServiceSystemRuntimeContext {
    return bootstrap_services.LateServiceSystemRuntimeContext{ boot_pid: BOOT_PID, init_pid: INIT_PID, init_tid: INIT_TID, init_asid: INIT_ASID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, phase141_shell_pid: PHASE141_SHELL_PID, phase141_kv_pid: PHASE141_KV_PID, init_endpoint_id: INIT_ENDPOINT_ID, serial_service_endpoint_id: SERIAL_SERVICE_ENDPOINT_ID, composition_echo_endpoint_id: COMPOSITION_ECHO_ENDPOINT_ID, composition_log_endpoint_id: COMPOSITION_LOG_ENDPOINT_ID, kv_service_endpoint_id: KV_SERVICE_ENDPOINT_ID, shell_service_endpoint_id: SHELL_SERVICE_ENDPOINT_ID, kv_service_directory_key: KV_SERVICE_DIRECTORY_KEY, init_root_page_table: INIT_ROOT_PAGE_TABLE, child_root_page_table: CHILD_ROOT_PAGE_TABLE, init_image: INIT_IMAGE, serial_receive_observation: SERIAL_SERVICE_SNAPSHOT.receive_observation, log_service_state: LOG_SERVICE_SNAPSHOT.state, echo_service_state: ECHO_SERVICE_SNAPSHOT.state }
}

func build_mid_phase_proof_runtime_context() bootstrap_proofs.MidPhaseProofRuntimeContext {
    return bootstrap_proofs.MidPhaseProofRuntimeContext{ scheduler_lifecycle_audit: build_scheduler_lifecycle_audit(), late_phase_context: build_late_phase_proof_context(), init_pid: INIT_PID, init_tid: INIT_TID, init_asid: INIT_ASID, init_endpoint_id: INIT_ENDPOINT_ID, init_endpoint_handle_slot: INIT_ENDPOINT_HANDLE_SLOT, init_root_page_table: INIT_ROOT_PAGE_TABLE, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT, child_root_page_table: CHILD_ROOT_PAGE_TABLE, child_exit_code: CHILD_EXIT_CODE, boot_pid: BOOT_PID, boot_tid: BOOT_TID, boot_task_slot: BOOT_TASK_SLOT, boot_entry_pc: BOOT_ENTRY_PC, boot_stack_top: BOOT_STACK_TOP, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, transfer_source_handle_slot: TRANSFER_SOURCE_HANDLE_SLOT, transfer_received_handle_slot: TRANSFER_RECEIVED_HANDLE_SLOT, transfer_service_directory_key: TRANSFER_SERVICE_DIRECTORY_KEY, composition_echo_endpoint_id: COMPOSITION_ECHO_ENDPOINT_ID, composition_log_endpoint_id: COMPOSITION_LOG_ENDPOINT_ID, serial_service_endpoint_id: SERIAL_SERVICE_ENDPOINT_ID, init_image: INIT_IMAGE, arch_actor: ARCH_ACTOR, boot_log_append_failed: BOOT_LOG_APPEND_FAILED, gate: SYSCALL_GATE, process_slots: PROCESS_SLOTS, task_slots: TASK_SLOTS, init_handle_table: HANDLE_TABLES[1], child_handle_table: HANDLE_TABLES[2], wait_table: WAIT_TABLES[1], endpoints: ENDPOINTS, ready_queue: READY_QUEUE, kernel: KERNEL, bootstrap_program_capability: INIT_RUNTIME_SNAPSHOT.bootstrap_caps.program_capability, init_bootstrap_handoff: INIT_RUNTIME_SNAPSHOT.bootstrap_handoff, init_user_frame: INIT_RUNTIME_SNAPSHOT.user_frame, receive_observation: RECEIVE_OBSERVATION, attached_receive_observation: ATTACHED_RECEIVE_OBSERVATION, transferred_handle_use_observation: TRANSFERRED_HANDLE_USE_OBSERVATION, pre_exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.pre_exit_wait_observation, exit_wait_observation: CHILD_EXECUTION_SNAPSHOT.exit_wait_observation, sleep_observation: CHILD_EXECUTION_SNAPSHOT.sleep_observation, timer_wake_observation: CHILD_EXECUTION_SNAPSHOT.timer_wake_observation, last_interrupt_kind: LAST_INTERRUPT_KIND, log_service_snapshot: LOG_SERVICE_SNAPSHOT, echo_service_snapshot: ECHO_SERVICE_SNAPSHOT, transfer_service_snapshot: TRANSFER_SERVICE_SNAPSHOT, serial_service_snapshot: SERIAL_SERVICE_SNAPSHOT, interrupts: INTERRUPTS, uart_device: UART_DEVICE, uart_receive_vector: UART_RECEIVE_VECTOR, uart_completion_vector: UART_COMPLETION_VECTOR, uart_source_actor: UART_SOURCE_ACTOR, uart_queue_frame_one_byte0: UART_QUEUE_FRAME_ONE_BYTE0, uart_queue_frame_one_byte1: UART_QUEUE_FRAME_ONE_BYTE1, uart_queue_frame_two_byte0: UART_QUEUE_FRAME_TWO_BYTE0, uart_queue_frame_two_byte1: UART_QUEUE_FRAME_TWO_BYTE1, uart_queue_frame_three_byte0: UART_QUEUE_FRAME_THREE_BYTE0, uart_queue_frame_three_byte1: UART_QUEUE_FRAME_THREE_BYTE1, uart_closed_frame_byte0: UART_CLOSED_FRAME_BYTE0, uart_closed_frame_byte1: UART_CLOSED_FRAME_BYTE1, uart_completion_frame_byte0: UART_COMPLETION_FRAME_BYTE0, uart_completion_frame_byte1: UART_COMPLETION_FRAME_BYTE1, uart_completion_frame_byte2: UART_COMPLETION_FRAME_BYTE2, uart_completion_frame_byte3: UART_COMPLETION_FRAME_BYTE3 }
}

func install_mid_phase_proof_result(next_result: bootstrap_proofs.MidPhaseProofResult) {
    LOG_SERVICE_SNAPSHOT = next_result.log_service_snapshot
    ECHO_SERVICE_SNAPSHOT = next_result.echo_service_snapshot
    TRANSFER_SERVICE_SNAPSHOT = next_result.transfer_service_snapshot
    install_composition_service_runtime_state(next_result.composition_state)
    install_serial_service_execution_state(next_result.serial_state)
    INTERRUPTS = next_result.interrupts
    LAST_INTERRUPT_KIND = next_result.last_interrupt_kind
    UART_DEVICE = next_result.uart_device
    UART_INGRESS = next_result.uart_ingress
}

func execute_late_service_system_proofs(phase137_audit: debug.Phase137OptionalDmaOrEquivalentAudit, proof_contract_flags: bootstrap_proofs.ProofContractFlags) i32 {
    contract_flags: bootstrap_services.LateServiceSystemContractFlags = bootstrap_services.LateServiceSystemContractFlags{ scheduler_contract_hardened: proof_contract_flags.scheduler_contract_hardened, lifecycle_contract_hardened: proof_contract_flags.lifecycle_contract_hardened, capability_contract_hardened: proof_contract_flags.capability_contract_hardened, ipc_contract_hardened: proof_contract_flags.ipc_contract_hardened, address_space_contract_hardened: proof_contract_flags.address_space_contract_hardened, interrupt_contract_hardened: proof_contract_flags.interrupt_contract_hardened, timer_contract_hardened: proof_contract_flags.timer_contract_hardened, barrier_contract_hardened: proof_contract_flags.barrier_contract_hardened }
    return bootstrap_services.execute_late_service_system_proofs(bootstrap_services.build_late_service_system_proof_inputs(phase137_audit, contract_flags, build_late_service_system_runtime_context()))
}

func execute_mid_phase_proofs() i32 {
    status: i32 = bootstrap_proofs.execute_mid_phase_proofs(build_mid_phase_proof_runtime_context())
    if status != 0 {
        return status
    }
    result: bootstrap_proofs.MidPhaseProofResult = bootstrap_proofs.mid_phase_proof_result()
    install_mid_phase_proof_result(result)
    return execute_late_service_system_proofs(result.phase137_audit, result.proof_contract_flags)
}

func execute_bootstrap_entry_sequence() i32 {
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
    return 0
}

func validate_bootstrap_final_identity() i32 {
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

func bootstrap_main() i32 {
    bootstrap_entry_status: i32 = execute_bootstrap_entry_sequence()
    if bootstrap_entry_status != 0 {
        return bootstrap_entry_status
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
    final_identity_status: i32 = validate_bootstrap_final_identity()
    if final_identity_status != 0 {
        return final_identity_status
    }
    return 0
}
