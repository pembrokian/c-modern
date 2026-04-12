import address_space
import bootstrap_audit
import bootstrap_proofs
import bootstrap_services
import capability
import debug
import echo_service
import ipc
import init
import interrupt
import lifecycle
import log_service
import mmu
import sched
import state
import syscall
import timer
import transfer_service

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
const PHASE124_MARKER: i32 = 124
const PHASE125_MARKER: i32 = 125
const PHASE125_MARKER_DETAIL: u32 = 125
const PHASE126_MARKER: i32 = 126
const PHASE126_MARKER_DETAIL: u32 = 126
const PHASE128_MARKER: i32 = 128
const PHASE128_MARKER_DETAIL: u32 = 128
const PHASE129_MARKER: i32 = 129
const PHASE129_MARKER_DETAIL: u32 = 129
const PHASE130_MARKER: i32 = 130
const PHASE130_MARKER_DETAIL: u32 = 130
const PHASE131_MARKER: i32 = 131
const PHASE131_MARKER_DETAIL: u32 = 131
const PHASE132_MARKER: i32 = 132
const PHASE132_MARKER_DETAIL: u32 = 132
const PHASE133_MARKER: i32 = 133
const PHASE133_MARKER_DETAIL: u32 = 133
const LOG_SERVICE_DIRECTORY_KEY: u32 = 1
const ECHO_SERVICE_DIRECTORY_KEY: u32 = 2
const TRANSFER_SERVICE_DIRECTORY_KEY: u32 = 3
const COMPOSITION_SERVICE_DIRECTORY_KEY: u32 = 4
const COMPOSITION_ECHO_ENDPOINT_ID: u32 = 3
const COMPOSITION_LOG_ENDPOINT_ID: u32 = 4
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
var BOOT_MARKER_EMITTED: u32
var ROOT_CAPABILITY: capability.CapabilitySlot
var INIT_PROGRAM_CAPABILITY: capability.CapabilitySlot
var LOG_SERVICE_PROGRAM_CAPABILITY: capability.CapabilitySlot
var ECHO_SERVICE_PROGRAM_CAPABILITY: capability.CapabilitySlot
var TRANSFER_SERVICE_PROGRAM_CAPABILITY: capability.CapabilitySlot
var HANDLE_TABLES: [3]capability.HandleTable
var WAIT_TABLES: [3]capability.WaitTable
var ENDPOINTS: ipc.EndpointTable
var INTERRUPTS: interrupt.InterruptController
var LAST_INTERRUPT_KIND: interrupt.InterruptDispatchKind
var INIT_TRANSLATION_ROOT: mmu.TranslationRoot
var CHILD_TRANSLATION_ROOT: mmu.TranslationRoot
var SYSCALL_GATE: syscall.SyscallGate
var INIT_IMAGE: init.InitImage
var INIT_BOOTSTRAP_CAPS: init.BootstrapCapabilitySet
var INIT_BOOTSTRAP_HANDOFF: init.BootstrapHandoffObservation
var ADDRESS_SPACE: address_space.AddressSpace
var USER_FRAME: address_space.UserEntryFrame
var CHILD_ADDRESS_SPACE: address_space.AddressSpace
var CHILD_USER_FRAME: address_space.UserEntryFrame
var LOG_SERVICE_ADDRESS_SPACE: address_space.AddressSpace
var LOG_SERVICE_USER_FRAME: address_space.UserEntryFrame
var ECHO_SERVICE_ADDRESS_SPACE: address_space.AddressSpace
var ECHO_SERVICE_USER_FRAME: address_space.UserEntryFrame
var TRANSFER_SERVICE_ADDRESS_SPACE: address_space.AddressSpace
var TRANSFER_SERVICE_USER_FRAME: address_space.UserEntryFrame
var TIMER_STATE: timer.TimerState
var DELIVERED_MESSAGE: ipc.KernelMessage
var RECEIVE_OBSERVATION: syscall.ReceiveObservation
var ATTACHED_RECEIVE_OBSERVATION: syscall.ReceiveObservation
var TRANSFERRED_HANDLE_USE_OBSERVATION: syscall.ReceiveObservation
var SPAWN_OBSERVATION: syscall.SpawnObservation
var SLEEP_OBSERVATION: syscall.SleepObservation
var TIMER_WAKE_OBSERVATION: timer.TimerWakeObservation
var WAKE_READY_QUEUE: state.ReadyQueue
var PRE_EXIT_WAIT_OBSERVATION: syscall.WaitObservation
var EXIT_WAIT_OBSERVATION: syscall.WaitObservation
var LOG_SERVICE_STATE: log_service.LogServiceState
var LOG_SERVICE_SPAWN_OBSERVATION: syscall.SpawnObservation
var LOG_SERVICE_RECEIVE_OBSERVATION: syscall.ReceiveObservation
var LOG_SERVICE_ACK_OBSERVATION: syscall.ReceiveObservation
var LOG_SERVICE_HANDSHAKE: log_service.LogHandshakeObservation
var LOG_SERVICE_WAIT_OBSERVATION: syscall.WaitObservation
var ECHO_SERVICE_STATE: echo_service.EchoServiceState
var ECHO_SERVICE_SPAWN_OBSERVATION: syscall.SpawnObservation
var ECHO_SERVICE_RECEIVE_OBSERVATION: syscall.ReceiveObservation
var ECHO_SERVICE_REPLY_OBSERVATION: syscall.ReceiveObservation
var ECHO_SERVICE_EXCHANGE: echo_service.EchoExchangeObservation
var ECHO_SERVICE_WAIT_OBSERVATION: syscall.WaitObservation
var TRANSFER_SERVICE_STATE: transfer_service.TransferServiceState
var TRANSFER_SERVICE_SPAWN_OBSERVATION: syscall.SpawnObservation
var TRANSFER_SERVICE_GRANT_OBSERVATION: syscall.ReceiveObservation
var TRANSFER_SERVICE_EMIT_OBSERVATION: syscall.ReceiveObservation
var TRANSFER_SERVICE_TRANSFER: transfer_service.TransferObservation
var TRANSFER_SERVICE_WAIT_OBSERVATION: syscall.WaitObservation

func reset_kernel_state() {
    KERNEL = state.empty_descriptor()
    PROCESS_SLOTS = state.zero_process_slots()
    TASK_SLOTS = state.zero_task_slots()
    READY_QUEUE = state.empty_queue()
    BOOT_LOG = state.empty_log()
    BOOT_LOG_APPEND_FAILED = 0
    BOOT_MARKER_EMITTED = 0
    ROOT_CAPABILITY = capability.empty_slot()
    INIT_PROGRAM_CAPABILITY = capability.empty_slot()
    LOG_SERVICE_PROGRAM_CAPABILITY = capability.empty_slot()
    ECHO_SERVICE_PROGRAM_CAPABILITY = capability.empty_slot()
    TRANSFER_SERVICE_PROGRAM_CAPABILITY = capability.empty_slot()
    HANDLE_TABLES = capability.zero_handle_tables()
    WAIT_TABLES = capability.zero_wait_tables()
    ENDPOINTS = ipc.empty_table()
    INTERRUPTS = interrupt.reset_controller()
    LAST_INTERRUPT_KIND = interrupt.InterruptDispatchKind.None
    INIT_TRANSLATION_ROOT = mmu.empty_translation_root()
    CHILD_TRANSLATION_ROOT = mmu.empty_translation_root()
    SYSCALL_GATE = syscall.gate_closed()
    INIT_IMAGE = init.bootstrap_image()
    INIT_BOOTSTRAP_CAPS = init.empty_bootstrap_capability_set()
    INIT_BOOTSTRAP_HANDOFF = init.BootstrapHandoffObservation{ owner_pid: 0, authority_count: 0, endpoint_handle_slot: 0, program_capability_slot: 0, program_object_id: 0, ambient_root_visible: 0 }
    ADDRESS_SPACE = address_space.empty_space()
    USER_FRAME = address_space.empty_frame()
    CHILD_ADDRESS_SPACE = address_space.empty_space()
    CHILD_USER_FRAME = address_space.empty_frame()
    LOG_SERVICE_ADDRESS_SPACE = address_space.empty_space()
    LOG_SERVICE_USER_FRAME = address_space.empty_frame()
    ECHO_SERVICE_ADDRESS_SPACE = address_space.empty_space()
    ECHO_SERVICE_USER_FRAME = address_space.empty_frame()
    TRANSFER_SERVICE_ADDRESS_SPACE = address_space.empty_space()
    TRANSFER_SERVICE_USER_FRAME = address_space.empty_frame()
    TIMER_STATE = timer.empty_timer_state()
    DELIVERED_MESSAGE = ipc.empty_message()
    RECEIVE_OBSERVATION = syscall.empty_receive_observation()
    ATTACHED_RECEIVE_OBSERVATION = syscall.empty_receive_observation()
    TRANSFERRED_HANDLE_USE_OBSERVATION = syscall.empty_receive_observation()
    SPAWN_OBSERVATION = syscall.empty_spawn_observation()
    SLEEP_OBSERVATION = syscall.empty_sleep_observation()
    TIMER_WAKE_OBSERVATION = timer.TimerWakeObservation{ task_id: 0, deadline_tick: 0, wake_tick: 0, wake_count: 0 }
    WAKE_READY_QUEUE = state.empty_queue()
    PRE_EXIT_WAIT_OBSERVATION = syscall.empty_wait_observation()
    EXIT_WAIT_OBSERVATION = syscall.empty_wait_observation()
    LOG_SERVICE_STATE = log_service.service_state(0, 0)
    LOG_SERVICE_SPAWN_OBSERVATION = syscall.empty_spawn_observation()
    LOG_SERVICE_RECEIVE_OBSERVATION = syscall.empty_receive_observation()
    LOG_SERVICE_ACK_OBSERVATION = syscall.empty_receive_observation()
    LOG_SERVICE_HANDSHAKE = log_service.LogHandshakeObservation{ service_pid: 0, client_pid: 0, endpoint_id: 0, tag: log_service.LogMessageTag.None, request_len: 0, request_byte: 0, ack_byte: 0, request_count: 0, ack_count: 0 }
    LOG_SERVICE_WAIT_OBSERVATION = syscall.empty_wait_observation()
    ECHO_SERVICE_STATE = echo_service.service_state(0, 0)
    ECHO_SERVICE_SPAWN_OBSERVATION = syscall.empty_spawn_observation()
    ECHO_SERVICE_RECEIVE_OBSERVATION = syscall.empty_receive_observation()
    ECHO_SERVICE_REPLY_OBSERVATION = syscall.empty_receive_observation()
    ECHO_SERVICE_EXCHANGE = echo_service.EchoExchangeObservation{ service_pid: 0, client_pid: 0, endpoint_id: 0, tag: echo_service.EchoMessageTag.None, request_len: 0, request_byte0: 0, request_byte1: 0, reply_len: 0, reply_byte0: 0, reply_byte1: 0, request_count: 0, reply_count: 0 }
    ECHO_SERVICE_WAIT_OBSERVATION = syscall.empty_wait_observation()
    TRANSFER_SERVICE_STATE = transfer_service.service_state(0, 0, 0)
    TRANSFER_SERVICE_SPAWN_OBSERVATION = syscall.empty_spawn_observation()
    TRANSFER_SERVICE_GRANT_OBSERVATION = syscall.empty_receive_observation()
    TRANSFER_SERVICE_EMIT_OBSERVATION = syscall.empty_receive_observation()
    TRANSFER_SERVICE_TRANSFER = transfer_service.TransferObservation{ service_pid: 0, client_pid: 0, control_endpoint_id: 0, transferred_endpoint_id: 0, transferred_rights: 0, tag: transfer_service.CapabilityTransferTag.None, grant_len: 0, grant_byte0: 0, grant_byte1: 0, grant_byte2: 0, grant_byte3: 0, emit_len: 0, emit_byte0: 0, emit_byte1: 0, emit_byte2: 0, emit_byte3: 0, grant_count: 0, emit_count: 0 }
    TRANSFER_SERVICE_WAIT_OBSERVATION = syscall.empty_wait_observation()
    bootstrap_proofs.reset_late_phase_proofs()
}

func record_boot_stage(stage_value: state.BootStage, detail: u32) {
    append_result: state.BootLogAppendResult = state.append_record(BOOT_LOG, stage_value, ARCH_ACTOR, detail)
    BOOT_LOG = append_result.log
    if append_result.appended == 0 {
        BOOT_LOG_APPEND_FAILED = 1
    }
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
    if address_space.state_score(ADDRESS_SPACE.state) != 1 {
        return false
    }
    if USER_FRAME.task_id != 0 {
        return false
    }
    if CHILD_USER_FRAME.task_id != 0 {
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
    INIT_TRANSLATION_ROOT = mmu.bootstrap_translation_root(INIT_ASID, INIT_ROOT_PAGE_TABLE)
    ADDRESS_SPACE = address_space.bootstrap_space(INIT_ASID, INIT_PID, INIT_TRANSLATION_ROOT, INIT_IMAGE.image_base, INIT_IMAGE.image_size, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_base, INIT_IMAGE.stack_size, INIT_IMAGE.stack_top)
    if address_space.state_score(ADDRESS_SPACE.state) != 2 {
        return false
    }
    INIT_PROGRAM_CAPABILITY = capability.bootstrap_init_program_slot(INIT_PID)
    slots[1] = state.init_process_slot(INIT_PID, INIT_TASK_SLOT, INIT_ASID)
    tasks[1] = state.user_task_slot(INIT_TID, INIT_PID, INIT_ASID, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    PROCESS_SLOTS = slots
    TASK_SLOTS = tasks
    READY_QUEUE = state.user_ready_queue(INIT_TID)
    USER_FRAME = address_space.bootstrap_user_frame(ADDRESS_SPACE, INIT_TID)
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
    if address_space.state_score(ADDRESS_SPACE.state) != 2 {
        return false
    }
    if ADDRESS_SPACE.asid != INIT_ASID {
        return false
    }
    if ADDRESS_SPACE.owner_pid != INIT_PID {
        return false
    }
    if ADDRESS_SPACE.translation_root.page_table_base != INIT_ROOT_PAGE_TABLE {
        return false
    }
    if ADDRESS_SPACE.translation_root.owner_asid != INIT_ASID {
        return false
    }
    if mmu.state_score(ADDRESS_SPACE.translation_root.state) != 2 {
        return false
    }
    if ADDRESS_SPACE.mapping_count != 2 {
        return false
    }
    image_mapping_index: usize = address_space.find_mapping_index(ADDRESS_SPACE, address_space.MappingKind.ImageText)
    if image_mapping_index >= ADDRESS_SPACE.mapping_count {
        return false
    }
    stack_mapping_index: usize = address_space.find_mapping_index(ADDRESS_SPACE, address_space.MappingKind.UserStack)
    if stack_mapping_index >= ADDRESS_SPACE.mapping_count {
        return false
    }
    if ADDRESS_SPACE.mappings[image_mapping_index].base != INIT_IMAGE.image_base {
        return false
    }
    if ADDRESS_SPACE.mappings[image_mapping_index].size != INIT_IMAGE.image_size {
        return false
    }
    if ADDRESS_SPACE.mappings[image_mapping_index].writable != 0 {
        return false
    }
    if ADDRESS_SPACE.mappings[image_mapping_index].executable != 1 {
        return false
    }
    if ADDRESS_SPACE.mappings[stack_mapping_index].base != INIT_IMAGE.stack_base {
        return false
    }
    if ADDRESS_SPACE.mappings[stack_mapping_index].size != INIT_IMAGE.stack_size {
        return false
    }
    if ADDRESS_SPACE.mappings[stack_mapping_index].writable != 1 {
        return false
    }
    if ADDRESS_SPACE.mappings[stack_mapping_index].executable != 0 {
        return false
    }
    if USER_FRAME.entry_pc != INIT_IMAGE.entry_pc {
        return false
    }
    if USER_FRAME.stack_top != INIT_IMAGE.stack_top {
        return false
    }
    if USER_FRAME.address_space_id != INIT_ASID {
        return false
    }
    if USER_FRAME.task_id != INIT_TID {
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
    if !address_space.can_activate(ADDRESS_SPACE) {
        return false
    }
    ADDRESS_SPACE = address_space.activate(ADDRESS_SPACE)
    if address_space.state_score(ADDRESS_SPACE.state) != 4 {
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
    INIT_BOOTSTRAP_CAPS = init.install_bootstrap_capability_set(INIT_PID, INIT_ENDPOINT_HANDLE_SLOT, INIT_PROGRAM_CAPABILITY)
    INIT_BOOTSTRAP_HANDOFF = init.observe_bootstrap_handoff(INIT_BOOTSTRAP_CAPS)
    return INIT_BOOTSTRAP_HANDOFF.authority_count == 2
}

func validate_init_bootstrap_capability_handoff() bool {
    if INIT_BOOTSTRAP_CAPS.owner_pid != INIT_PID {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.endpoint_handle_slot != INIT_ENDPOINT_HANDLE_SLOT {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.endpoint_handle_count != 1 {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability_count != 1 {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.wait_handle_count != 0 {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.ambient_root_visible != 0 {
        return false
    }
    if !capability.is_program_capability(INIT_BOOTSTRAP_CAPS.program_capability) {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.owner_pid != INIT_PID {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.slot_id != INIT_PROGRAM_CAPABILITY.slot_id {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.object_id != INIT_PROGRAM_CAPABILITY.object_id {
        return false
    }
    if INIT_BOOTSTRAP_HANDOFF.owner_pid != INIT_PID {
        return false
    }
    if INIT_BOOTSTRAP_HANDOFF.authority_count != 2 {
        return false
    }
    if INIT_BOOTSTRAP_HANDOFF.endpoint_handle_slot != INIT_ENDPOINT_HANDLE_SLOT {
        return false
    }
    if INIT_BOOTSTRAP_HANDOFF.program_capability_slot != INIT_PROGRAM_CAPABILITY.slot_id {
        return false
    }
    if INIT_BOOTSTRAP_HANDOFF.program_object_id != INIT_PROGRAM_CAPABILITY.object_id {
        return false
    }
    if INIT_BOOTSTRAP_HANDOFF.ambient_root_visible != 0 {
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
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], INIT_BOOTSTRAP_CAPS.endpoint_handle_slot) != INIT_ENDPOINT_ID {
        return false
    }
    if WAIT_TABLES[1].count != INIT_BOOTSTRAP_CAPS.wait_handle_count {
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
    return bootstrap_services.LogServiceExecutionState{ program_capability: LOG_SERVICE_PROGRAM_CAPABILITY, gate: SYSCALL_GATE, process_slots: PROCESS_SLOTS, task_slots: TASK_SLOTS, init_handle_table: HANDLE_TABLES[1], child_handle_table: HANDLE_TABLES[2], wait_table: WAIT_TABLES[1], endpoints: ENDPOINTS, init_image: INIT_IMAGE, child_address_space: LOG_SERVICE_ADDRESS_SPACE, child_user_frame: LOG_SERVICE_USER_FRAME, service_state: LOG_SERVICE_STATE, spawn_observation: LOG_SERVICE_SPAWN_OBSERVATION, receive_observation: LOG_SERVICE_RECEIVE_OBSERVATION, ack_observation: LOG_SERVICE_ACK_OBSERVATION, handshake: LOG_SERVICE_HANDSHAKE, wait_observation: LOG_SERVICE_WAIT_OBSERVATION, ready_queue: READY_QUEUE }
}

func install_log_service_execution_state(next_state: bootstrap_services.LogServiceExecutionState) {
    LOG_SERVICE_PROGRAM_CAPABILITY = next_state.program_capability
    SYSCALL_GATE = next_state.gate
    PROCESS_SLOTS = next_state.process_slots
    TASK_SLOTS = next_state.task_slots
    HANDLE_TABLES[1] = next_state.init_handle_table
    HANDLE_TABLES[2] = next_state.child_handle_table
    WAIT_TABLES[1] = next_state.wait_table
    ENDPOINTS = next_state.endpoints
    LOG_SERVICE_ADDRESS_SPACE = next_state.child_address_space
    LOG_SERVICE_USER_FRAME = next_state.child_user_frame
    LOG_SERVICE_STATE = next_state.service_state
    LOG_SERVICE_SPAWN_OBSERVATION = next_state.spawn_observation
    LOG_SERVICE_RECEIVE_OBSERVATION = next_state.receive_observation
    LOG_SERVICE_ACK_OBSERVATION = next_state.ack_observation
    LOG_SERVICE_HANDSHAKE = next_state.handshake
    LOG_SERVICE_WAIT_OBSERVATION = next_state.wait_observation
    READY_QUEUE = next_state.ready_queue
}

func execute_phase105_log_service_handshake() bool {
    result: bootstrap_services.LogServiceExecutionResult = bootstrap_services.execute_phase105_log_service_handshake(build_log_service_config(), build_log_service_execution_state())
    install_log_service_execution_state(result.state)
    return result.succeeded != 0
}

func validate_phase105_log_service_handshake() bool {
    config: bootstrap_services.LogServiceConfig = build_log_service_config()
    return bootstrap_audit.validate_phase105_log_service_handshake(bootstrap_audit.LogServicePhaseAudit{ program_capability: LOG_SERVICE_PROGRAM_CAPABILITY, gate: SYSCALL_GATE, spawn_observation: LOG_SERVICE_SPAWN_OBSERVATION, receive_observation: LOG_SERVICE_RECEIVE_OBSERVATION, ack_observation: LOG_SERVICE_ACK_OBSERVATION, service_state: LOG_SERVICE_STATE, handshake: LOG_SERVICE_HANDSHAKE, wait_observation: LOG_SERVICE_WAIT_OBSERVATION, wait_table: WAIT_TABLES[1], child_handle_table: HANDLE_TABLES[2], ready_queue: READY_QUEUE, child_process: PROCESS_SLOTS[2], child_task: TASK_SLOTS[2], child_address_space: LOG_SERVICE_ADDRESS_SPACE, child_user_frame: LOG_SERVICE_USER_FRAME, init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, init_endpoint_id: INIT_ENDPOINT_ID, wait_handle_slot: config.wait_handle_slot, endpoint_handle_slot: config.endpoint_handle_slot, request_byte: config.request_byte, exit_code: config.exit_code })
}

func build_echo_service_config() bootstrap_services.EchoServiceConfig {
    return bootstrap_services.echo_service_config(INIT_PID, CHILD_PID, CHILD_TID, CHILD_ASID, INIT_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT, mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE))
}

func build_echo_service_execution_state() bootstrap_services.EchoServiceExecutionState {
    return bootstrap_services.EchoServiceExecutionState{ program_capability: ECHO_SERVICE_PROGRAM_CAPABILITY, gate: SYSCALL_GATE, process_slots: PROCESS_SLOTS, task_slots: TASK_SLOTS, init_handle_table: HANDLE_TABLES[1], child_handle_table: HANDLE_TABLES[2], wait_table: WAIT_TABLES[1], endpoints: ENDPOINTS, init_image: INIT_IMAGE, child_address_space: ECHO_SERVICE_ADDRESS_SPACE, child_user_frame: ECHO_SERVICE_USER_FRAME, service_state: ECHO_SERVICE_STATE, spawn_observation: ECHO_SERVICE_SPAWN_OBSERVATION, receive_observation: ECHO_SERVICE_RECEIVE_OBSERVATION, reply_observation: ECHO_SERVICE_REPLY_OBSERVATION, exchange: ECHO_SERVICE_EXCHANGE, wait_observation: ECHO_SERVICE_WAIT_OBSERVATION, ready_queue: READY_QUEUE }
}

func install_echo_service_execution_state(next_state: bootstrap_services.EchoServiceExecutionState) {
    ECHO_SERVICE_PROGRAM_CAPABILITY = next_state.program_capability
    SYSCALL_GATE = next_state.gate
    PROCESS_SLOTS = next_state.process_slots
    TASK_SLOTS = next_state.task_slots
    HANDLE_TABLES[1] = next_state.init_handle_table
    HANDLE_TABLES[2] = next_state.child_handle_table
    WAIT_TABLES[1] = next_state.wait_table
    ENDPOINTS = next_state.endpoints
    ECHO_SERVICE_ADDRESS_SPACE = next_state.child_address_space
    ECHO_SERVICE_USER_FRAME = next_state.child_user_frame
    ECHO_SERVICE_STATE = next_state.service_state
    ECHO_SERVICE_SPAWN_OBSERVATION = next_state.spawn_observation
    ECHO_SERVICE_RECEIVE_OBSERVATION = next_state.receive_observation
    ECHO_SERVICE_REPLY_OBSERVATION = next_state.reply_observation
    ECHO_SERVICE_EXCHANGE = next_state.exchange
    ECHO_SERVICE_WAIT_OBSERVATION = next_state.wait_observation
    READY_QUEUE = next_state.ready_queue
}

func execute_phase106_echo_service_request_reply() bool {
    result: bootstrap_services.EchoServiceExecutionResult = bootstrap_services.execute_phase106_echo_service_request_reply(build_echo_service_config(), build_echo_service_execution_state())
    install_echo_service_execution_state(result.state)
    return result.succeeded != 0
}

func validate_phase106_echo_service_request_reply() bool {
    config: bootstrap_services.EchoServiceConfig = build_echo_service_config()
    return bootstrap_audit.validate_phase106_echo_service_request_reply(bootstrap_audit.EchoServicePhaseAudit{ program_capability: ECHO_SERVICE_PROGRAM_CAPABILITY, gate: SYSCALL_GATE, spawn_observation: ECHO_SERVICE_SPAWN_OBSERVATION, receive_observation: ECHO_SERVICE_RECEIVE_OBSERVATION, reply_observation: ECHO_SERVICE_REPLY_OBSERVATION, service_state: ECHO_SERVICE_STATE, exchange: ECHO_SERVICE_EXCHANGE, wait_observation: ECHO_SERVICE_WAIT_OBSERVATION, wait_table: WAIT_TABLES[1], child_handle_table: HANDLE_TABLES[2], ready_queue: READY_QUEUE, child_process: PROCESS_SLOTS[2], child_task: TASK_SLOTS[2], child_address_space: ECHO_SERVICE_ADDRESS_SPACE, child_user_frame: ECHO_SERVICE_USER_FRAME, init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, init_endpoint_id: INIT_ENDPOINT_ID, wait_handle_slot: config.wait_handle_slot, endpoint_handle_slot: config.endpoint_handle_slot, request_byte0: config.request_byte0, request_byte1: config.request_byte1, exit_code: config.exit_code })
}

func build_transfer_service_config() bootstrap_services.TransferServiceConfig {
    return bootstrap_services.transfer_service_config(INIT_PID, CHILD_PID, CHILD_TID, CHILD_ASID, INIT_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT, mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE), TRANSFER_SOURCE_HANDLE_SLOT, TRANSFER_RECEIVED_HANDLE_SLOT)
}

func build_transfer_service_execution_state() bootstrap_services.TransferServiceExecutionState {
    return bootstrap_services.TransferServiceExecutionState{ program_capability: TRANSFER_SERVICE_PROGRAM_CAPABILITY, gate: SYSCALL_GATE, process_slots: PROCESS_SLOTS, task_slots: TASK_SLOTS, init_handle_table: HANDLE_TABLES[1], child_handle_table: HANDLE_TABLES[2], wait_table: WAIT_TABLES[1], endpoints: ENDPOINTS, init_image: INIT_IMAGE, child_address_space: TRANSFER_SERVICE_ADDRESS_SPACE, child_user_frame: TRANSFER_SERVICE_USER_FRAME, service_state: TRANSFER_SERVICE_STATE, spawn_observation: TRANSFER_SERVICE_SPAWN_OBSERVATION, grant_observation: TRANSFER_SERVICE_GRANT_OBSERVATION, emit_observation: TRANSFER_SERVICE_EMIT_OBSERVATION, transfer: TRANSFER_SERVICE_TRANSFER, wait_observation: TRANSFER_SERVICE_WAIT_OBSERVATION, ready_queue: READY_QUEUE }
}

func install_transfer_service_execution_state(next_state: bootstrap_services.TransferServiceExecutionState) {
    TRANSFER_SERVICE_PROGRAM_CAPABILITY = next_state.program_capability
    SYSCALL_GATE = next_state.gate
    PROCESS_SLOTS = next_state.process_slots
    TASK_SLOTS = next_state.task_slots
    HANDLE_TABLES[1] = next_state.init_handle_table
    HANDLE_TABLES[2] = next_state.child_handle_table
    WAIT_TABLES[1] = next_state.wait_table
    ENDPOINTS = next_state.endpoints
    TRANSFER_SERVICE_ADDRESS_SPACE = next_state.child_address_space
    TRANSFER_SERVICE_USER_FRAME = next_state.child_user_frame
    TRANSFER_SERVICE_STATE = next_state.service_state
    TRANSFER_SERVICE_SPAWN_OBSERVATION = next_state.spawn_observation
    TRANSFER_SERVICE_GRANT_OBSERVATION = next_state.grant_observation
    TRANSFER_SERVICE_EMIT_OBSERVATION = next_state.emit_observation
    TRANSFER_SERVICE_TRANSFER = next_state.transfer
    TRANSFER_SERVICE_WAIT_OBSERVATION = next_state.wait_observation
    READY_QUEUE = next_state.ready_queue
}

func execute_phase107_user_to_user_capability_transfer() bool {
    result: bootstrap_services.TransferServiceExecutionResult = bootstrap_services.execute_phase107_user_to_user_capability_transfer(build_transfer_service_config(), build_transfer_service_execution_state())
    install_transfer_service_execution_state(result.state)
    return result.succeeded != 0
}

func validate_phase107_user_to_user_capability_transfer() bool {
    config: bootstrap_services.TransferServiceConfig = build_transfer_service_config()
    return bootstrap_audit.validate_phase107_user_to_user_capability_transfer(bootstrap_audit.TransferServicePhaseAudit{ program_capability: TRANSFER_SERVICE_PROGRAM_CAPABILITY, gate: SYSCALL_GATE, spawn_observation: TRANSFER_SERVICE_SPAWN_OBSERVATION, grant_observation: TRANSFER_SERVICE_GRANT_OBSERVATION, emit_observation: TRANSFER_SERVICE_EMIT_OBSERVATION, service_state: TRANSFER_SERVICE_STATE, transfer: TRANSFER_SERVICE_TRANSFER, wait_observation: TRANSFER_SERVICE_WAIT_OBSERVATION, init_handle_table: HANDLE_TABLES[1], wait_table: WAIT_TABLES[1], child_handle_table: HANDLE_TABLES[2], ready_queue: READY_QUEUE, child_process: PROCESS_SLOTS[2], child_task: TASK_SLOTS[2], child_address_space: TRANSFER_SERVICE_ADDRESS_SPACE, child_user_frame: TRANSFER_SERVICE_USER_FRAME, init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, init_endpoint_id: INIT_ENDPOINT_ID, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, wait_handle_slot: config.wait_handle_slot, source_handle_slot: config.source_handle_slot, control_handle_slot: config.control_handle_slot, init_received_handle_slot: config.init_received_handle_slot, service_received_handle_slot: config.service_received_handle_slot, grant_byte0: config.grant_byte0, grant_byte1: config.grant_byte1, grant_byte2: config.grant_byte2, grant_byte3: config.grant_byte3, exit_code: config.exit_code })
}

func build_composition_service_config() bootstrap_services.CompositionServiceConfig {
    return bootstrap_services.composition_service_config(INIT_PID, CHILD_PID, CHILD_TID, CHILD_ASID, INIT_ENDPOINT_ID, COMPOSITION_ECHO_ENDPOINT_ID, COMPOSITION_LOG_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT, mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE))
}

func build_late_phase_proof_context() bootstrap_proofs.LatePhaseProofContext {
    return bootstrap_proofs.LatePhaseProofContext{ init_pid: INIT_PID, init_endpoint_id: INIT_ENDPOINT_ID, transfer_endpoint_id: TRANSFER_ENDPOINT_ID, log_service_directory_key: LOG_SERVICE_DIRECTORY_KEY, echo_service_directory_key: ECHO_SERVICE_DIRECTORY_KEY, composition_service_directory_key: COMPOSITION_SERVICE_DIRECTORY_KEY, phase124_intermediary_pid: PHASE124_INTERMEDIARY_PID, phase124_final_holder_pid: PHASE124_FINAL_HOLDER_PID, phase124_control_handle_slot: PHASE124_CONTROL_HANDLE_SLOT, phase124_intermediary_receive_handle_slot: PHASE124_INTERMEDIARY_RECEIVE_HANDLE_SLOT, phase124_final_receive_handle_slot: PHASE124_FINAL_RECEIVE_HANDLE_SLOT }
}

func install_composition_service_runtime_state(next_state: bootstrap_services.CompositionServiceExecutionState) {
    bootstrap_proofs.install_composition_service_execution_state(next_state)
    SYSCALL_GATE = next_state.gate
    PROCESS_SLOTS = next_state.process_slots
    TASK_SLOTS = next_state.task_slots
    HANDLE_TABLES[1] = next_state.init_handle_table
    HANDLE_TABLES[2] = next_state.child_handle_table
    WAIT_TABLES[1] = next_state.wait_table
    ENDPOINTS = next_state.endpoints
    READY_QUEUE = next_state.ready_queue
}

func execute_phase118_invalidated_source_send_probe() bool {
    transfer_config: bootstrap_services.TransferServiceConfig = build_transfer_service_config()
    probe_payload: [4]u8 = ipc.zero_payload()
    probe_payload[0] = transfer_config.grant_byte0
    probe_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, INIT_PID, syscall.build_send_request(transfer_config.source_handle_slot, 1, probe_payload))
    SYSCALL_GATE = probe_result.gate
    HANDLE_TABLES[1] = probe_result.handle_table
    ENDPOINTS = probe_result.endpoints
    bootstrap_proofs.record_phase118_invalidated_source_send_status(probe_result.status)
    return syscall.status_score(bootstrap_proofs.phase118_invalidated_source_send_status()) == 8
}

func build_phase108_program_cap_contract(init_pid: u32, log_config: bootstrap_services.LogServiceConfig, echo_config: bootstrap_services.EchoServiceConfig, transfer_config: bootstrap_services.TransferServiceConfig, bootstrap_program_capability: capability.CapabilitySlot, log_service_program_capability: capability.CapabilitySlot, echo_service_program_capability: capability.CapabilitySlot, transfer_service_program_capability: capability.CapabilitySlot, log_service_spawn: syscall.SpawnObservation, echo_service_spawn: syscall.SpawnObservation, transfer_service_spawn: syscall.SpawnObservation, log_service_wait: syscall.WaitObservation, echo_service_wait: syscall.WaitObservation, transfer_service_wait: syscall.WaitObservation) debug.Phase108ProgramCapContract {
    return bootstrap_proofs.build_phase108_program_cap_contract(init_pid, log_config, echo_config, transfer_config, bootstrap_program_capability, log_service_program_capability, echo_service_program_capability, transfer_service_program_capability, log_service_spawn, echo_service_spawn, transfer_service_spawn, log_service_wait, echo_service_wait, transfer_service_wait)
}

func build_phase132_backpressure_audit(phase131_audit: debug.Phase131FanOutCompositionAudit) debug.Phase132BackpressureAudit {
    local_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    local_endpoints: ipc.EndpointTable = ipc.install_endpoint(ipc.empty_table(), INIT_PID, INIT_ENDPOINT_ID)
    local_task_slots: [3]state.TaskSlot = state.zero_task_slots()
    local_task_slots[1] = state.user_task_slot(INIT_TID, INIT_PID, INIT_ASID, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    local_task_slots[2] = state.user_task_slot(CHILD_TID, CHILD_PID, CHILD_ASID, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    local_ready_queue: state.ReadyQueue = state.empty_queue()
    sender_table: capability.HandleTable = capability.install_endpoint_handle(capability.handle_table_for_owner(INIT_PID), 1, INIT_ENDPOINT_ID, capability.RIGHTS_ENDPOINT_ALL)
    receiver_table: capability.HandleTable = capability.install_endpoint_handle(capability.handle_table_for_owner(CHILD_PID), 1, INIT_ENDPOINT_ID, capability.RIGHTS_ENDPOINT_ALL)

    payload_a: [4]u8 = ipc.zero_payload()
    payload_a[0] = 65
    fill_a: syscall.BackpressureSendResult = syscall.perform_send_with_backpressure(local_gate, sender_table, local_endpoints, local_task_slots, local_ready_queue, INIT_PID, 1, syscall.build_send_request(1, 1, payload_a))
    local_gate = fill_a.gate
    sender_table = fill_a.handle_table
    local_endpoints = fill_a.endpoints
    local_task_slots = fill_a.task_slots
    local_ready_queue = fill_a.ready_queue

    payload_b: [4]u8 = ipc.zero_payload()
    payload_b[0] = 66
    fill_b: syscall.BackpressureSendResult = syscall.perform_send_with_backpressure(local_gate, sender_table, local_endpoints, local_task_slots, local_ready_queue, INIT_PID, 1, syscall.build_send_request(1, 1, payload_b))
    local_gate = fill_b.gate
    sender_table = fill_b.handle_table
    local_endpoints = fill_b.endpoints
    local_task_slots = fill_b.task_slots
    local_ready_queue = fill_b.ready_queue

    payload_c: [4]u8 = ipc.zero_payload()
    payload_c[0] = 67
    blocked_send: syscall.BackpressureSendResult = syscall.perform_send_with_backpressure(local_gate, sender_table, local_endpoints, local_task_slots, local_ready_queue, INIT_PID, 1, syscall.build_send_request(1, 1, payload_c))
    blocked_send_queue_depth: usize = blocked_send.endpoints.slots[0].queued_messages
    blocked_send_waiter_task_slot: u32 = blocked_send.endpoints.slots[0].blocked_sender.task_slot
    local_gate = blocked_send.gate
    local_endpoints = blocked_send.endpoints
    local_task_slots = blocked_send.task_slots
    local_ready_queue = blocked_send.ready_queue

    sender_wake: syscall.BackpressureReceiveResult = syscall.perform_receive_with_backpressure(local_gate, receiver_table, local_endpoints, local_task_slots, local_ready_queue, 2, syscall.build_receive_request(1))
    sender_wake_queue_depth: usize = sender_wake.endpoints.slots[0].queued_messages
    sender_wake_waiter_task_slot: u32 = sender_wake.endpoints.slots[0].blocked_sender.task_slot
    sender_wake_ready_count: usize = sender_wake.ready_queue.count
    sender_wake_task_state: state.TaskState = state.task_slot_at(sender_wake.task_slots, 1).state
    local_gate = sender_wake.gate
    receiver_table = sender_wake.handle_table
    local_endpoints = sender_wake.endpoints
    local_task_slots = sender_wake.task_slots
    local_ready_queue = state.empty_queue()

    drain_receive: syscall.BackpressureReceiveResult = syscall.perform_receive_with_backpressure(local_gate, receiver_table, local_endpoints, local_task_slots, local_ready_queue, 2, syscall.build_receive_request(1))
    local_gate = drain_receive.gate
    receiver_table = drain_receive.handle_table
    local_endpoints = drain_receive.endpoints
    local_task_slots = drain_receive.task_slots
    local_ready_queue = state.empty_queue()

    blocked_receive: syscall.BackpressureReceiveResult = syscall.perform_receive_with_backpressure(local_gate, receiver_table, local_endpoints, local_task_slots, local_ready_queue, 2, syscall.build_receive_request(1))
    blocked_receive_queue_depth: usize = blocked_receive.endpoints.slots[0].queued_messages
    blocked_receive_waiter_task_slot: u32 = blocked_receive.endpoints.slots[0].blocked_receiver.task_slot
    local_gate = blocked_receive.gate
    local_endpoints = blocked_receive.endpoints
    local_task_slots = blocked_receive.task_slots
    local_ready_queue = blocked_receive.ready_queue

    payload_d: [4]u8 = ipc.zero_payload()
    payload_d[0] = 68
    receiver_wake: syscall.BackpressureSendResult = syscall.perform_send_with_backpressure(local_gate, sender_table, local_endpoints, local_task_slots, local_ready_queue, INIT_PID, 1, syscall.build_send_request(1, 1, payload_d))
    receiver_wake_queue_depth: usize = receiver_wake.endpoints.slots[0].queued_messages
    receiver_wake_waiter_task_slot: u32 = receiver_wake.endpoints.slots[0].blocked_receiver.task_slot
    receiver_wake_ready_count: usize = receiver_wake.ready_queue.count
    receiver_wake_task_state: state.TaskState = state.task_slot_at(receiver_wake.task_slots, 2).state

    return debug.Phase132BackpressureAudit{ phase131: phase131_audit, endpoint_id: INIT_ENDPOINT_ID, sender_pid: INIT_PID, receiver_pid: CHILD_PID, sender_tid: INIT_TID, receiver_tid: CHILD_TID, sender_task_slot: 1, receiver_task_slot: 2, blocked_send_status: blocked_send.status, blocked_send_reason: blocked_send.block_reason, blocked_send_task_state: state.task_slot_at(blocked_send.task_slots, 1).state, blocked_send_queue_depth: blocked_send_queue_depth, blocked_send_waiter_task_slot: blocked_send_waiter_task_slot, sender_wake_status: sender_wake.status, sender_wake_task_state: sender_wake_task_state, sender_wake_reason: sender_wake.wake_reason, sender_wake_task_id: sender_wake.wake_task_id, sender_wake_ready_count: sender_wake_ready_count, sender_wake_queue_depth: sender_wake_queue_depth, sender_wake_waiter_task_slot: sender_wake_waiter_task_slot, blocked_receive_status: blocked_receive.status, blocked_receive_reason: blocked_receive.block_reason, blocked_receive_task_state: state.task_slot_at(blocked_receive.task_slots, 2).state, blocked_receive_queue_depth: blocked_receive_queue_depth, blocked_receive_waiter_task_slot: blocked_receive_waiter_task_slot, receiver_wake_status: receiver_wake.status, receiver_wake_task_state: receiver_wake_task_state, receiver_wake_reason: receiver_wake.wake_reason, receiver_wake_task_id: receiver_wake.wake_task_id, receiver_wake_ready_count: receiver_wake_ready_count, receiver_wake_queue_depth: receiver_wake_queue_depth, receiver_wake_waiter_task_slot: receiver_wake_waiter_task_slot, kernel_policy_visible: 0, compiler_reopening_visible: 0 }
}

func build_phase133_message_lifetime_audit(phase132_audit: debug.Phase132BackpressureAudit) debug.Phase133MessageLifetimeAudit {
    local_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    local_endpoints: ipc.EndpointTable = ipc.install_endpoint(ipc.empty_table(), INIT_PID, INIT_ENDPOINT_ID)
    sender_table: capability.HandleTable = capability.handle_table_for_owner(INIT_PID)
    sender_table = capability.install_endpoint_handle(sender_table, 1, INIT_ENDPOINT_ID, capability.RIGHTS_ENDPOINT_ALL)
    sender_table = capability.install_endpoint_handle(sender_table, 2, TRANSFER_ENDPOINT_ID, capability.RIGHTS_ENDPOINT_ALL)
    receiver_table: capability.HandleTable = capability.install_endpoint_handle(capability.handle_table_for_owner(CHILD_PID), 1, INIT_ENDPOINT_ID, capability.RIGHTS_ENDPOINT_ALL)

    first_payload: [4]u8 = ipc.zero_payload()
    first_payload[0] = 84
    first_send: syscall.SendResult = syscall.perform_send(local_gate, sender_table, local_endpoints, INIT_PID, syscall.build_transfer_send_request(1, 1, first_payload, 2))
    source_handle_live_after_first_send: u32 = 0
    if capability.find_endpoint_for_handle(first_send.handle_table, 2) != 0 {
        source_handle_live_after_first_send = 1
    }
    first_receive: syscall.ReceiveResult = syscall.perform_receive(first_send.gate, receiver_table, first_send.endpoints, syscall.build_transfer_receive_request(1, 3))
    first_retired_slot_state: ipc.MessageState = first_receive.endpoints.slots[0].messages[0].state
    first_retired_slot_payload0: u8 = first_receive.endpoints.slots[0].messages[0].payload[0]
    first_received_handle_endpoint_id: u32 = capability.find_endpoint_for_handle(first_receive.handle_table, 3)

    second_payload: [4]u8 = ipc.zero_payload()
    second_payload[0] = 82
    second_send: syscall.SendResult = syscall.perform_send(first_receive.gate, first_send.handle_table, first_receive.endpoints, INIT_PID, syscall.build_send_request(1, 1, second_payload))
    second_receive: syscall.ReceiveResult = syscall.perform_receive(second_send.gate, first_receive.handle_table, second_send.endpoints, syscall.build_receive_request(1))
    second_retired_slot_state: ipc.MessageState = second_receive.endpoints.slots[0].messages[0].state
    second_retired_slot_payload0: u8 = second_receive.endpoints.slots[0].messages[0].payload[0]

    abort_transfer_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    abort_transfer_endpoints: ipc.EndpointTable = ipc.install_endpoint(ipc.empty_table(), INIT_PID, INIT_ENDPOINT_ID)
    abort_transfer_sender_table: capability.HandleTable = capability.handle_table_for_owner(INIT_PID)
    abort_transfer_sender_table = capability.install_endpoint_handle(abort_transfer_sender_table, 1, INIT_ENDPOINT_ID, capability.RIGHTS_ENDPOINT_ALL)
    abort_transfer_sender_table = capability.install_endpoint_handle(abort_transfer_sender_table, 2, TRANSFER_ENDPOINT_ID, capability.RIGHTS_ENDPOINT_ALL)
    abort_transfer_payload: [4]u8 = ipc.zero_payload()
    abort_transfer_payload[0] = 83
    abort_transfer_send: syscall.SendResult = syscall.perform_send(abort_transfer_gate, abort_transfer_sender_table, abort_transfer_endpoints, INIT_PID, syscall.build_transfer_send_request(1, 1, abort_transfer_payload, 2))
    abort_transfer_source_handle_live_after_send: u32 = 0
    if capability.find_endpoint_for_handle(abort_transfer_send.handle_table, 2) != 0 {
        abort_transfer_source_handle_live_after_send = 1
    }
    abort_transfer_closed: ipc.EndpointCloseTransition = ipc.close_runtime_endpoint(abort_transfer_send.endpoints, INIT_ENDPOINT_ID)
    abort_transfer_scrubbed: ipc.KernelMessage = abort_transfer_closed.endpoints.slots[0].messages[0]

    close_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    close_endpoints: ipc.EndpointTable = ipc.install_endpoint(ipc.empty_table(), INIT_PID, INIT_ENDPOINT_ID)
    close_sender_table: capability.HandleTable = capability.install_endpoint_handle(capability.handle_table_for_owner(INIT_PID), 1, INIT_ENDPOINT_ID, capability.RIGHTS_ENDPOINT_ALL)
    close_receiver_table: capability.HandleTable = capability.install_endpoint_handle(capability.handle_table_for_owner(CHILD_PID), 1, INIT_ENDPOINT_ID, capability.RIGHTS_ENDPOINT_ALL)
    close_task_slots: [3]state.TaskSlot = state.zero_task_slots()
    close_task_slots[1] = state.user_task_slot(INIT_TID, INIT_PID, INIT_ASID, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    close_task_slots[2] = state.user_task_slot(CHILD_TID, CHILD_PID, CHILD_ASID, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    close_ready_queue: state.ReadyQueue = state.empty_queue()
    close_fill_a_payload: [4]u8 = ipc.zero_payload()
    close_fill_a_payload[0] = 65
    close_fill_a: syscall.BackpressureSendResult = syscall.perform_send_with_backpressure(close_gate, close_sender_table, close_endpoints, close_task_slots, close_ready_queue, INIT_PID, 1, syscall.build_send_request(1, 1, close_fill_a_payload))
    close_fill_b_payload: [4]u8 = ipc.zero_payload()
    close_fill_b_payload[0] = 66
    close_fill_b: syscall.BackpressureSendResult = syscall.perform_send_with_backpressure(close_fill_a.gate, close_fill_a.handle_table, close_fill_a.endpoints, close_fill_a.task_slots, close_fill_a.ready_queue, INIT_PID, 1, syscall.build_send_request(1, 1, close_fill_b_payload))
    close_block_payload: [4]u8 = ipc.zero_payload()
    close_block_payload[0] = 67
    close_blocked_send: syscall.BackpressureSendResult = syscall.perform_send_with_backpressure(close_fill_b.gate, close_fill_b.handle_table, close_fill_b.endpoints, close_fill_b.task_slots, close_fill_b.ready_queue, INIT_PID, 1, syscall.build_send_request(1, 1, close_block_payload))
    closed: ipc.EndpointCloseTransition = ipc.close_runtime_endpoint(close_blocked_send.endpoints, INIT_ENDPOINT_ID)
    close_wake: ipc.EndpointWake = closed.wakes.wakes[0]
    close_apply: syscall.EndpointWakeApplyResult = syscall.apply_endpoint_wake(close_blocked_send.task_slots, close_blocked_send.ready_queue, close_wake)
    closed_send_payload: [4]u8 = ipc.zero_payload()
    closed_send_payload[0] = 68
    closed_send: syscall.BackpressureSendResult = syscall.perform_send_with_backpressure(close_blocked_send.gate, close_blocked_send.handle_table, closed.endpoints, close_apply.task_slots, close_apply.ready_queue, INIT_PID, 1, syscall.build_send_request(1, 1, closed_send_payload))
    closed_receive: syscall.BackpressureReceiveResult = syscall.perform_receive_with_backpressure(close_blocked_send.gate, close_receiver_table, closed.endpoints, close_apply.task_slots, close_apply.ready_queue, 2, syscall.build_receive_request(1))

    owner_death_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    owner_death_table: ipc.EndpointTable = ipc.install_endpoint(ipc.empty_table(), INIT_PID, 90)
    owner_death_table = ipc.install_endpoint(owner_death_table, INIT_PID, 91)
    owner_death_sender_table: capability.HandleTable = capability.handle_table_for_owner(INIT_PID)
    owner_death_sender_table = capability.install_endpoint_handle(owner_death_sender_table, 1, 90, capability.RIGHTS_ENDPOINT_ALL)
    owner_death_sender_table = capability.install_endpoint_handle(owner_death_sender_table, 2, 91, capability.RIGHTS_ENDPOINT_ALL)
    owner_death_payload: [4]u8 = ipc.zero_payload()
    owner_death_payload[0] = 79
    owner_death_send: syscall.SendResult = syscall.perform_send(owner_death_gate, owner_death_sender_table, owner_death_table, INIT_PID, syscall.build_transfer_send_request(1, 1, owner_death_payload, 2))
    owner_death_transfer_source_handle_live_after_send: u32 = 0
    if capability.find_endpoint_for_handle(owner_death_send.handle_table, 2) != 0 {
        owner_death_transfer_source_handle_live_after_send = 1
    }
    owner_death_waiting: ipc.EndpointTable = ipc.register_runtime_blocked_receiver(owner_death_send.endpoints, 91, 2, CHILD_TID)
    owner_death_closed: ipc.EndpointOwnerCloseTransition = ipc.close_runtime_endpoints_for_owner(owner_death_waiting, INIT_PID)
    owner_death_scrubbed: ipc.KernelMessage = owner_death_closed.endpoints.slots[0].messages[0]
    owner_death_wake_reason: ipc.EndpointWakeReason = ipc.EndpointWakeReason.None
    if owner_death_closed.wakes.count != 0 {
        owner_death_wake_reason = owner_death_closed.wakes.wakes[0].wake_reason
    }

    return debug.Phase133MessageLifetimeAudit{ phase132: phase132_audit, endpoint_id: INIT_ENDPOINT_ID, attached_endpoint_id: TRANSFER_ENDPOINT_ID, first_send_status: first_send.status, source_handle_live_after_first_send: source_handle_live_after_first_send, first_receive_status: first_receive.observation.status, first_receive_byte0: first_receive.observation.payload[0], first_receive_handle_slot: first_receive.observation.received_handle_slot, first_receive_handle_count: first_receive.observation.received_handle_count, first_received_handle_endpoint_id: first_received_handle_endpoint_id, first_retired_slot_state: first_retired_slot_state, first_retired_slot_payload0: first_retired_slot_payload0, second_send_status: second_send.status, second_receive_status: second_receive.observation.status, second_receive_byte0: second_receive.observation.payload[0], second_retired_slot_state: second_retired_slot_state, second_retired_slot_payload0: second_retired_slot_payload0, abort_transfer_send_status: abort_transfer_send.status, abort_transfer_source_handle_live_after_send: abort_transfer_source_handle_live_after_send, abort_transfer_aborted_messages: abort_transfer_closed.aborted_messages, abort_transfer_scrubbed_state: abort_transfer_scrubbed.state, abort_transfer_scrubbed_payload0: abort_transfer_scrubbed.payload[0], abort_transfer_scrubbed_attached_count: abort_transfer_scrubbed.attached_count, abort_transfer_scrubbed_attached_endpoint_id: abort_transfer_scrubbed.attached_endpoint_id, abort_transfer_scrubbed_source_handle_slot: abort_transfer_scrubbed.attached_source_handle_slot, close_blocked_send_status: close_blocked_send.status, close_blocked_send_state: state.task_slot_at(close_blocked_send.task_slots, 1).state, close_aborted_messages: closed.aborted_messages, close_wake_count: closed.wakes.count, close_wake_reason: close_wake.wake_reason, close_wake_task_id: close_wake.task_id, close_ready_count: close_apply.ready_queue.count, close_task_state: state.task_slot_at(close_apply.task_slots, 1).state, closed_send_status: closed_send.status, closed_receive_status: closed_receive.status, owner_death_closed_count: owner_death_closed.closed_count, owner_death_aborted_messages: owner_death_closed.aborted_messages, owner_death_wake_count: owner_death_closed.wakes.count, owner_death_wake_reason: owner_death_wake_reason, owner_death_transfer_source_handle_live_after_send: owner_death_transfer_source_handle_live_after_send, owner_death_transfer_scrubbed_state: owner_death_scrubbed.state, owner_death_transfer_scrubbed_payload0: owner_death_scrubbed.payload[0], owner_death_transfer_scrubbed_attached_count: owner_death_scrubbed.attached_count, owner_death_transfer_scrubbed_attached_endpoint_id: owner_death_scrubbed.attached_endpoint_id, owner_death_transfer_scrubbed_source_handle_slot: owner_death_scrubbed.attached_source_handle_slot, kernel_policy_visible: 0, compiler_reopening_visible: 0 }
}

func execute_spawn_child_process() bool {
    WAIT_TABLES[1] = capability.wait_table_for_owner(INIT_PID)
    CHILD_TRANSLATION_ROOT = mmu.bootstrap_translation_root(CHILD_ASID, CHILD_ROOT_PAGE_TABLE)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(CHILD_WAIT_HANDLE_SLOT)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(SYSCALL_GATE, INIT_PROGRAM_CAPABILITY, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], INIT_IMAGE, spawn_request, CHILD_PID, CHILD_TID, CHILD_ASID, CHILD_TRANSLATION_ROOT)
    SYSCALL_GATE = spawn_result.gate
    INIT_PROGRAM_CAPABILITY = spawn_result.program_capability
    PROCESS_SLOTS = spawn_result.process_slots
    TASK_SLOTS = spawn_result.task_slots
    WAIT_TABLES[1] = spawn_result.wait_table
    CHILD_ADDRESS_SPACE = spawn_result.child_address_space
    CHILD_USER_FRAME = spawn_result.child_frame
    SPAWN_OBSERVATION = spawn_result.observation
    READY_QUEUE = state.user_ready_queue(CHILD_TID)
    return syscall.status_score(SPAWN_OBSERVATION.status) == 2
}

func execute_child_sleep_transition() bool {
    sleep_result: syscall.SleepResult = syscall.perform_sleep(SYSCALL_GATE, TASK_SLOTS, TIMER_STATE, syscall.build_sleep_request(CHILD_TASK_SLOT, 1))
    SYSCALL_GATE = sleep_result.gate
    TASK_SLOTS = sleep_result.task_slots
    TIMER_STATE = sleep_result.timer_state
    SLEEP_OBSERVATION = sleep_result.observation
    READY_QUEUE = state.empty_queue()
    if syscall.status_score(SLEEP_OBSERVATION.status) != 4 {
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
    if SLEEP_OBSERVATION.task_id != CHILD_TID {
        return false
    }
    return SLEEP_OBSERVATION.deadline_tick == 1
}

func execute_child_timer_wake_transition() bool {
    interrupt_entry: interrupt.InterruptEntry = interrupt.arch_enter_interrupt(INTERRUPTS, 32, ARCH_ACTOR)
    INTERRUPTS = interrupt_entry.controller
    dispatch_result: interrupt.InterruptDispatchResult = interrupt.dispatch_interrupt(interrupt_entry)
    INTERRUPTS = dispatch_result.controller
    LAST_INTERRUPT_KIND = dispatch_result.kind
    delivery: timer.TimerInterruptDelivery = timer.deliver_interrupt_tick(TIMER_STATE, 1)
    TIMER_STATE = delivery.timer_state
    TIMER_WAKE_OBSERVATION = delivery.observation
    wake_transition: lifecycle.TaskTransition = lifecycle.ready_task(TASK_SLOTS, 2)
    TASK_SLOTS = wake_transition.task_slots
    READY_QUEUE = state.user_ready_queue(CHILD_TID)
    WAKE_READY_QUEUE = READY_QUEUE
    if TIMER_WAKE_OBSERVATION.task_id != CHILD_TID {
        return false
    }
    if TIMER_WAKE_OBSERVATION.deadline_tick != 1 {
        return false
    }
    if TIMER_WAKE_OBSERVATION.wake_tick != 1 {
        return false
    }
    if TIMER_WAKE_OBSERVATION.wake_count != 1 {
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
    PRE_EXIT_WAIT_OBSERVATION = pre_wait_result.observation
    return syscall.status_score(PRE_EXIT_WAIT_OBSERVATION.status) == 4
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
    EXIT_WAIT_OBSERVATION = wait_result.observation
    READY_QUEUE = state.empty_queue()
    return syscall.status_score(EXIT_WAIT_OBSERVATION.status) == 2
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
    return sched.LifecycleAudit{ init_pid: INIT_PID, child_pid: CHILD_PID, child_tid: CHILD_TID, child_asid: CHILD_ASID, child_translation_root: CHILD_TRANSLATION_ROOT, child_exit_code: CHILD_EXIT_CODE, child_wait_handle_slot: CHILD_WAIT_HANDLE_SLOT, init_entry_pc: INIT_IMAGE.entry_pc, init_stack_top: INIT_IMAGE.stack_top, spawn: SPAWN_OBSERVATION, pre_exit_wait: PRE_EXIT_WAIT_OBSERVATION, exit_wait: EXIT_WAIT_OBSERVATION, sleep: SLEEP_OBSERVATION, timer_wake: TIMER_WAKE_OBSERVATION, timer_state: TIMER_STATE, wake_ready_queue: WAKE_READY_QUEUE, wait_table: WAIT_TABLES[1], child_process: PROCESS_SLOTS[2], child_task: TASK_SLOTS[2], child_address_space: CHILD_ADDRESS_SPACE, child_user_frame: CHILD_USER_FRAME, ready_queue: READY_QUEUE }
}

func bootstrap_main() i32 {
    phase104_contract_hardened: u32 = 0
    phase108_contract_hardened: u32 = 0
    scheduler_contract_hardened: u32 = 0
    lifecycle_contract_hardened: u32 = 0
    capability_contract_hardened: u32 = 0
    ipc_contract_hardened: u32 = 0
    address_space_contract_hardened: u32 = 0
    interrupt_contract_hardened: u32 = 0
    timer_contract_hardened: u32 = 0
    barrier_contract_hardened: u32 = 0
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
    if address_space.state_score(ADDRESS_SPACE.state) != 4 {
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
    if !sched.validate_program_cap_spawn_and_wait(build_scheduler_lifecycle_audit()) {
        return 27
    }
    scheduler_contract_hardened = 1
    if !lifecycle.validate_task_transition_contracts() {
        return 28
    }
    lifecycle_contract_hardened = 1
    if !capability.validate_syscall_capability_boundary() {
        return 29
    }
    capability_contract_hardened = 1
    if !ipc.validate_syscall_ipc_boundary() {
        return 30
    }
    ipc_contract_hardened = 1
    if !address_space.validate_syscall_address_space_boundary() {
        return 31
    }
    address_space_contract_hardened = 1
    if !mmu.validate_address_space_mmu_boundary() {
        return 32
    }
    if !interrupt.validate_interrupt_entry_and_dispatch_boundary() {
        return 33
    }
    interrupt_contract_hardened = 1
    if !timer.validate_interrupt_delivery_boundary() {
        return 34
    }
    timer_contract_hardened = 1
    if !mmu.validate_activation_barrier_boundary() {
        return 35
    }
    barrier_contract_hardened = 1
    if !validate_phase104_contract_hardening() {
        return 36
    }
    phase104_contract_hardened = 1
    if !execute_phase105_log_service_handshake() {
        return 37
    }
    if !validate_phase105_log_service_handshake() {
        return 38
    }
    if !execute_phase106_echo_service_request_reply() {
        return 39
    }
    if !validate_phase106_echo_service_request_reply() {
        return 40
    }
    if !execute_phase107_user_to_user_capability_transfer() {
        return 41
    }
    if !validate_phase107_user_to_user_capability_transfer() {
        return 42
    }
    log_config: bootstrap_services.LogServiceConfig = build_log_service_config()
    echo_config: bootstrap_services.EchoServiceConfig = build_echo_service_config()
    transfer_config: bootstrap_services.TransferServiceConfig = build_transfer_service_config()
    if !debug.validate_phase108_kernel_image_and_program_cap_contracts(build_phase108_program_cap_contract(INIT_PID, log_config, echo_config, transfer_config, INIT_BOOTSTRAP_CAPS.program_capability, LOG_SERVICE_PROGRAM_CAPABILITY, ECHO_SERVICE_PROGRAM_CAPABILITY, TRANSFER_SERVICE_PROGRAM_CAPABILITY, LOG_SERVICE_SPAWN_OBSERVATION, ECHO_SERVICE_SPAWN_OBSERVATION, TRANSFER_SERVICE_SPAWN_OBSERVATION, LOG_SERVICE_WAIT_OBSERVATION, ECHO_SERVICE_WAIT_OBSERVATION, TRANSFER_SERVICE_WAIT_OBSERVATION)) {
        return 43
    }
    phase108_contract_hardened = 1
    running_slice_audit: debug.RunningKernelSliceAudit = bootstrap_proofs.build_phase109_running_kernel_slice_audit(log_config, echo_config, transfer_config, KERNEL, INIT_PID, INIT_TID, INIT_ASID, CHILD_TID, CHILD_EXIT_CODE, TRANSFER_ENDPOINT_ID, INIT_BOOTSTRAP_HANDOFF, RECEIVE_OBSERVATION, ATTACHED_RECEIVE_OBSERVATION, TRANSFERRED_HANDLE_USE_OBSERVATION, PRE_EXIT_WAIT_OBSERVATION, EXIT_WAIT_OBSERVATION, SLEEP_OBSERVATION, TIMER_WAKE_OBSERVATION, LOG_SERVICE_HANDSHAKE, LOG_SERVICE_WAIT_OBSERVATION, ECHO_SERVICE_EXCHANGE, ECHO_SERVICE_WAIT_OBSERVATION, TRANSFER_SERVICE_TRANSFER, TRANSFER_SERVICE_WAIT_OBSERVATION, phase104_contract_hardened, phase108_contract_hardened, PROCESS_SLOTS[1], TASK_SLOTS[1], USER_FRAME, BOOT_LOG_APPEND_FAILED)
    if !debug.validate_phase109_first_running_kernel_slice(running_slice_audit) {
        return 44
    }
    if !debug.validate_phase110_kernel_ownership_split(running_slice_audit, scheduler_contract_hardened) {
        return 45
    }
    if !debug.validate_phase111_scheduler_and_lifecycle_ownership(running_slice_audit, scheduler_contract_hardened, lifecycle_contract_hardened) {
        return 46
    }
    if !debug.validate_phase112_syscall_boundary_thinness(running_slice_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened) {
        return 47
    }
    if !debug.validate_phase113_interrupt_entry_and_generic_dispatch_boundary(running_slice_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, LAST_INTERRUPT_KIND) {
        return 48
    }
    if !debug.validate_phase114_address_space_and_mmu_ownership_split(running_slice_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened) {
        return 49
    }
    if !debug.validate_phase115_timer_ownership_hardening(running_slice_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened) {
        return 50
    }
    if !debug.validate_phase116_mmu_activation_barrier_follow_through(running_slice_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 51
    }
    phase117_audit: debug.Phase117MultiServiceBringUpAudit = bootstrap_proofs.build_phase117_multi_service_bring_up_audit(running_slice_audit, INIT_ENDPOINT_ID, TRANSFER_ENDPOINT_ID, LOG_SERVICE_PROGRAM_CAPABILITY, ECHO_SERVICE_PROGRAM_CAPABILITY, TRANSFER_SERVICE_PROGRAM_CAPABILITY, LOG_SERVICE_SPAWN_OBSERVATION, ECHO_SERVICE_SPAWN_OBSERVATION, TRANSFER_SERVICE_SPAWN_OBSERVATION, LOG_SERVICE_WAIT_OBSERVATION, ECHO_SERVICE_WAIT_OBSERVATION, TRANSFER_SERVICE_WAIT_OBSERVATION, LOG_SERVICE_HANDSHAKE, ECHO_SERVICE_EXCHANGE, TRANSFER_SERVICE_TRANSFER)
    if !debug.validate_phase117_init_orchestrated_multi_service_bring_up(phase117_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 52
    }
    if !execute_phase118_invalidated_source_send_probe() {
        return 53
    }
    phase118_audit: debug.Phase118DelegatedRequestReplyAudit = bootstrap_proofs.build_phase118_delegated_request_reply_audit(phase117_audit, transfer_config, HANDLE_TABLES[1], TRANSFER_SERVICE_TRANSFER)
    if !debug.validate_phase118_request_reply_and_delegation_follow_through(phase118_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 54
    }
    phase119_audit: debug.Phase119NamespacePressureAudit = bootstrap_proofs.build_phase119_namespace_pressure_audit(phase118_audit, INIT_PID, INIT_ENDPOINT_ID, LOG_SERVICE_DIRECTORY_KEY, ECHO_SERVICE_DIRECTORY_KEY, TRANSFER_SERVICE_DIRECTORY_KEY, log_config, echo_config, transfer_config, LOG_SERVICE_SPAWN_OBSERVATION, ECHO_SERVICE_SPAWN_OBSERVATION, TRANSFER_SERVICE_SPAWN_OBSERVATION)
    if !debug.validate_phase119_namespace_pressure_audit(phase119_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 55
    }
    phase120_audit: debug.Phase120RunningSystemSupportAudit = bootstrap_proofs.build_phase120_running_system_support_audit(phase119_audit, INIT_PID, INIT_ENDPOINT_ID)
    if !debug.validate_phase120_running_system_support_statement(phase120_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 56
    }
    phase121_audit: debug.Phase121KernelImageContractAudit = bootstrap_proofs.build_phase121_kernel_image_contract_audit(phase120_audit)
    if !debug.validate_phase121_kernel_image_contract_hardening(phase121_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 57
    }
    phase122_audit: debug.Phase122TargetSurfaceAudit = bootstrap_proofs.build_phase122_target_surface_audit(phase121_audit)
    if !debug.validate_phase122_target_surface_audit(phase122_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 58
    }
    phase123_audit: debug.Phase123NextPlateauAudit = bootstrap_proofs.build_phase123_next_plateau_audit(phase122_audit)
    if !debug.validate_phase123_next_plateau_audit(phase123_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 59
    }
    late_phase_context: bootstrap_proofs.LatePhaseProofContext = build_late_phase_proof_context()
    if !bootstrap_proofs.execute_phase124_delegation_chain_probe(late_phase_context, transfer_config) {
        return 60
    }
    phase124_audit: debug.Phase124DelegationChainAudit = bootstrap_proofs.build_phase124_delegation_chain_audit(late_phase_context, transfer_config, phase123_audit)
    if !debug.validate_phase124_delegation_chain_stress(phase124_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 61
    }
    if !bootstrap_proofs.execute_phase125_invalidation_probe(late_phase_context, transfer_config) {
        return 62
    }
    phase125_audit: debug.Phase125InvalidationAudit = bootstrap_proofs.build_phase125_invalidation_audit(late_phase_context, phase124_audit)
    if !debug.validate_phase125_invalidation_and_rejection_audit(phase125_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 63
    }
    if !bootstrap_proofs.execute_phase126_authority_lifetime_probe(late_phase_context, transfer_config) {
        return 64
    }
    phase126_audit: debug.Phase126AuthorityLifetimeAudit = bootstrap_proofs.build_phase126_authority_lifetime_audit(late_phase_context, phase125_audit)
    if !debug.validate_phase126_authority_lifetime_classification(phase126_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 65
    }
    if !bootstrap_proofs.execute_phase128_service_death_observation_probe(late_phase_context, log_config, LOG_SERVICE_SPAWN_OBSERVATION, LOG_SERVICE_WAIT_OBSERVATION) {
        return 66
    }
    phase128_audit: debug.Phase128ServiceDeathObservationAudit = bootstrap_proofs.build_phase128_service_death_observation_audit(phase126_audit)
    if !debug.validate_phase128_service_death_observation(phase128_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 67
    }
    if !bootstrap_proofs.execute_phase129_partial_failure_propagation_probe(log_config, build_log_service_execution_state(), echo_config) {
        return 68
    }
    phase129_audit: debug.Phase129PartialFailurePropagationAudit = bootstrap_proofs.build_phase129_partial_failure_propagation_audit(late_phase_context, log_config, echo_config, phase128_audit)
    if !debug.validate_phase129_partial_failure_propagation(phase129_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 69
    }
    if !bootstrap_proofs.execute_phase130_explicit_restart_or_replacement_probe(log_config, build_log_service_execution_state()) {
        return 70
    }
    phase130_audit: debug.Phase130ExplicitRestartOrReplacementAudit = bootstrap_proofs.build_phase130_explicit_restart_or_replacement_audit(late_phase_context, log_config, phase129_audit)
    if !debug.validate_phase130_explicit_restart_or_replacement(phase130_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 71
    }
    composition_config: bootstrap_services.CompositionServiceConfig = build_composition_service_config()
    composition_result: bootstrap_services.CompositionServiceExecutionResult = bootstrap_services.execute_phase131_fan_out_composition(composition_config, bootstrap_proofs.build_composition_service_execution_state(SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, HANDLE_TABLES[1], HANDLE_TABLES[2], WAIT_TABLES[1], ENDPOINTS, INIT_IMAGE))
    install_composition_service_runtime_state(composition_result.state)
    if composition_result.succeeded == 0 {
        return 72
    }
    if !bootstrap_proofs.phase131_composition_probe_succeeded() {
        return 72
    }
    phase131_audit: debug.Phase131FanOutCompositionAudit = bootstrap_proofs.build_phase131_fan_out_composition_audit(late_phase_context, composition_config, phase130_audit)
    if !debug.validate_phase131_fan_out_composition(phase131_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 73
    }
    phase132_audit: debug.Phase132BackpressureAudit = build_phase132_backpressure_audit(phase131_audit)
    if !debug.validate_phase132_backpressure_and_blocking(phase132_audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 74
    }
    if !debug.validate_phase133_message_lifetime_and_reuse(build_phase133_message_lifetime_audit(phase132_audit), scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return 75
    }
    BOOT_MARKER_EMITTED = 1
    record_boot_stage(state.BootStage.MarkerEmitted, PHASE133_MARKER_DETAIL)
    if BOOT_MARKER_EMITTED != 1 {
        return 76
    }
    if BOOT_LOG_APPEND_FAILED != 0 {
        return 77
    }
    if BOOT_LOG.count != 5 {
        return 78
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 3)) != 8 {
        return 79
    }
    if state.log_actor_at(BOOT_LOG, 3) != ARCH_ACTOR {
        return 80
    }
    if state.log_detail_at(BOOT_LOG, 3) != INIT_TID {
        return 81
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 4)) != 16 {
        return 82
    }
    if state.log_actor_at(BOOT_LOG, 4) != ARCH_ACTOR {
        return 83
    }
    if state.log_detail_at(BOOT_LOG, 4) != PHASE133_MARKER_DETAIL {
        return 84
    }
    if PROCESS_SLOTS[1].pid != INIT_PID {
        return 85
    }
    if TASK_SLOTS[1].tid != INIT_TID {
        return 86
    }
    if USER_FRAME.task_id != INIT_TID {
        return 87
    }
    return PHASE133_MARKER
}