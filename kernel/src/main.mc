import address_space
import capability
import echo_service
import endpoint
import init
import interrupt
import log_service
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
const LOG_SERVICE_PROGRAM_SLOT: u32 = 3
const LOG_SERVICE_PROGRAM_OBJECT_ID: u32 = 2
const LOG_SERVICE_WAIT_HANDLE_SLOT: u32 = 2
const LOG_SERVICE_ENDPOINT_HANDLE_SLOT: u32 = 1
const LOG_SERVICE_REQUEST_BYTE: u8 = 76
const LOG_SERVICE_EXIT_CODE: i32 = 52
const ECHO_SERVICE_PROGRAM_SLOT: u32 = 4
const ECHO_SERVICE_PROGRAM_OBJECT_ID: u32 = 3
const ECHO_SERVICE_WAIT_HANDLE_SLOT: u32 = 3
const ECHO_SERVICE_ENDPOINT_HANDLE_SLOT: u32 = 1
const ECHO_SERVICE_REQUEST_BYTE0: u8 = 69
const ECHO_SERVICE_REQUEST_BYTE1: u8 = 67
const ECHO_SERVICE_EXIT_CODE: i32 = 53
const TRANSFER_SERVICE_PROGRAM_SLOT: u32 = 5
const TRANSFER_SERVICE_PROGRAM_OBJECT_ID: u32 = 4
const TRANSFER_SERVICE_WAIT_HANDLE_SLOT: u32 = 4
const TRANSFER_SERVICE_CONTROL_HANDLE_SLOT: u32 = 1
const TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT: u32 = 2
const TRANSFER_SERVICE_GRANT_BYTE0: u8 = 71
const TRANSFER_SERVICE_GRANT_BYTE1: u8 = 73
const TRANSFER_SERVICE_GRANT_BYTE2: u8 = 86
const TRANSFER_SERVICE_GRANT_BYTE3: u8 = 69
const TRANSFER_SERVICE_EXIT_CODE: i32 = 54
const PHASE109_MARKER: i32 = 109

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
var ENDPOINTS: endpoint.EndpointTable
var INTERRUPTS: interrupt.InterruptController
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
var DELIVERED_MESSAGE: endpoint.KernelMessage
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
    ENDPOINTS = endpoint.empty_table()
    INTERRUPTS = interrupt.reset_controller()
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
    DELIVERED_MESSAGE = endpoint.empty_message()
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
    ENDPOINTS = endpoint.empty_table()
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
    ADDRESS_SPACE = address_space.bootstrap_space(INIT_ASID, INIT_PID, INIT_ROOT_PAGE_TABLE, INIT_IMAGE.image_base, INIT_IMAGE.image_size, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_base, INIT_IMAGE.stack_size, INIT_IMAGE.stack_top)
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
    if ADDRESS_SPACE.root_page_table != INIT_ROOT_PAGE_TABLE {
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
    endpoints: endpoint.EndpointTable = ENDPOINTS

    endpoints = endpoint.install_endpoint(endpoints, INIT_PID, INIT_ENDPOINT_ID)
    handle_tables[1] = capability.handle_table_for_owner(INIT_PID)
    handle_tables[1] = capability.install_endpoint_handle(handle_tables[1], INIT_ENDPOINT_HANDLE_SLOT, INIT_ENDPOINT_ID, 7)
    endpoints = endpoint.enqueue_message(endpoints, 0, endpoint.bootstrap_init_message(BOOT_PID, INIT_ENDPOINT_ID))
    DELIVERED_MESSAGE = endpoint.mark_delivered(endpoint.peek_head_message(endpoints, 0))
    endpoints = endpoint.consume_head_message(endpoints, 0)

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
    if endpoint.message_state_score(ENDPOINTS.slots[0].messages[0].state) != 1 {
        return false
    }
    if endpoint.message_state_score(ENDPOINTS.slots[0].messages[1].state) != 1 {
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
    if endpoint.message_state_score(DELIVERED_MESSAGE.state) != 4 {
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
    payload: [4]u8 = endpoint.zero_payload()
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
    endpoints: endpoint.EndpointTable = ENDPOINTS

    endpoints = endpoint.install_endpoint(endpoints, INIT_PID, TRANSFER_ENDPOINT_ID)
    handle_tables[1] = capability.install_endpoint_handle(handle_tables[1], TRANSFER_SOURCE_HANDLE_SLOT, TRANSFER_ENDPOINT_ID, 5)

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
    if capability.find_rights_for_handle(HANDLE_TABLES[1], TRANSFER_SOURCE_HANDLE_SLOT) != 5 {
        return false
    }
    return true
}

func execute_capability_carrying_ipc_transfer() bool {
    payload: [4]u8 = endpoint.zero_payload()
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

    follow_payload: [4]u8 = endpoint.zero_payload()
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
    if capability.find_rights_for_handle(HANDLE_TABLES[1], TRANSFER_RECEIVED_HANDLE_SLOT) != 5 {
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
    premature_timer: timer.TimerState = timer.empty_timer_state()
    premature_timer = timer.arm_sleep(premature_timer, 41, 5)
    premature_consumed: timer.TimerState = timer.consume_wake(premature_timer, 41)
    if premature_consumed.count != 1 {
        return false
    }
    if timer.find_sleep_index(premature_consumed, 41) != 0 {
        return false
    }
    if timer.sleep_state_score(premature_consumed.sleepers[0].state) != 2 {
        return false
    }

    dual_timer: timer.TimerState = timer.empty_timer_state()
    dual_timer = timer.arm_sleep(dual_timer, 51, 1)
    dual_timer = timer.arm_sleep(dual_timer, 52, 1)
    dual_timer = timer.advance_tick(dual_timer, 1)
    if timer.has_fired_sleeper(dual_timer) == 0 {
        return false
    }
    first_wake: timer.TimerWakeResult = timer.wake_fired_sleepers(dual_timer)
    if first_wake.observation.task_id != 51 {
        return false
    }
    dual_timer = timer.consume_wake(first_wake.timer_state, first_wake.observation.task_id)
    if dual_timer.count != 1 {
        return false
    }
    if timer.has_fired_sleeper(dual_timer) == 0 {
        return false
    }
    second_wake: timer.TimerWakeResult = timer.wake_fired_sleepers(dual_timer)
    if second_wake.observation.task_id != 52 {
        return false
    }
    if second_wake.observation.wake_count != 2 {
        return false
    }
    dual_timer = timer.consume_wake(second_wake.timer_state, second_wake.observation.task_id)
    if dual_timer.count != 0 {
        return false
    }
    return timer.has_fired_sleeper(dual_timer) == 0
}

func validate_bootstrap_layout_contracts() bool {
    if !init.bootstrap_image_valid(INIT_IMAGE) {
        return false
    }
    invalid_image: init.InitImage = init.InitImage{ image_id: 9, image_base: 65536, image_size: 4096, entry_pc: 70000, stack_base: 67584, stack_top: 71680, stack_size: 4096 }
    if init.bootstrap_image_valid(invalid_image) {
        return false
    }
    invalid_space: address_space.AddressSpace = address_space.bootstrap_space(3, 7, INIT_ROOT_PAGE_TABLE, invalid_image.image_base, invalid_image.image_size, invalid_image.entry_pc, invalid_image.stack_base, invalid_image.stack_size, invalid_image.stack_top)
    if address_space.state_score(invalid_space.state) != 1 {
        return false
    }
    if address_space.can_activate(address_space.empty_space()) {
        return false
    }
    still_empty: address_space.AddressSpace = address_space.activate(address_space.empty_space())
    return address_space.state_score(still_empty.state) == 1
}

func validate_endpoint_and_capability_contracts() bool {
    payload: [4]u8 = endpoint.zero_payload()
    local_endpoints: endpoint.EndpointTable = endpoint.empty_table()
    local_endpoints = endpoint.install_endpoint(local_endpoints, INIT_PID, INIT_ENDPOINT_ID)
    enqueue_one: endpoint.EndpointTable = endpoint.enqueue_message(local_endpoints, 0, endpoint.byte_message(2, INIT_PID, INIT_ENDPOINT_ID, 0, payload))
    if !endpoint.enqueue_succeeded(local_endpoints, enqueue_one, 0) {
        return false
    }
    enqueue_two: endpoint.EndpointTable = endpoint.enqueue_message(enqueue_one, 0, endpoint.byte_message(3, INIT_PID, INIT_ENDPOINT_ID, 0, payload))
    if !endpoint.enqueue_succeeded(enqueue_one, enqueue_two, 0) {
        return false
    }
    enqueue_three: endpoint.EndpointTable = endpoint.enqueue_message(enqueue_two, 0, endpoint.byte_message(4, INIT_PID, INIT_ENDPOINT_ID, 0, payload))
    if endpoint.enqueue_succeeded(enqueue_two, enqueue_three, 0) {
        return false
    }

    local_handles: capability.HandleTable = capability.handle_table_for_owner(INIT_PID)
    installed_handle: capability.HandleTable = capability.install_endpoint_handle(local_handles, 1, INIT_ENDPOINT_ID, 7)
    if !capability.handle_install_succeeded(local_handles, installed_handle, 1) {
        return false
    }
    duplicate_handle: capability.HandleTable = capability.install_endpoint_handle(installed_handle, 1, TRANSFER_ENDPOINT_ID, 7)
    if capability.handle_install_succeeded(installed_handle, duplicate_handle, 1) {
        return false
    }
    invalid_rights_handle: capability.HandleTable = capability.install_endpoint_handle(local_handles, 2, TRANSFER_ENDPOINT_ID, 8)
    if capability.handle_install_succeeded(local_handles, invalid_rights_handle, 2) {
        return false
    }
    if capability.attenuate_endpoint_rights(8) != 0 {
        return false
    }
    if capability.find_transfer_rights_for_handle(installed_handle, 1) != 7 {
        return false
    }

    local_waits: capability.WaitTable = capability.wait_table_for_owner(INIT_PID)
    installed_wait: capability.WaitTable = capability.install_wait_handle(local_waits, CHILD_WAIT_HANDLE_SLOT, CHILD_PID)
    if !capability.wait_install_succeeded(local_waits, installed_wait, CHILD_WAIT_HANDLE_SLOT) {
        return false
    }
    duplicate_wait: capability.WaitTable = capability.install_wait_handle(installed_wait, CHILD_WAIT_HANDLE_SLOT, CHILD_PID)
    return !capability.wait_install_succeeded(installed_wait, duplicate_wait, CHILD_WAIT_HANDLE_SLOT)
}

func validate_state_hardening_contracts() bool {
    boot_task: state.TaskSlot = state.boot_task_slot(BOOT_TID, BOOT_PID, BOOT_ENTRY_PC, BOOT_STACK_TOP)
    if state.can_block_task(boot_task) {
        return false
    }
    if state.task_state_score(state.blocked_task_slot(boot_task).state) != 2 {
        return false
    }
    ready_task: state.TaskSlot = state.user_task_slot(CHILD_TID, CHILD_PID, CHILD_ASID, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    if !state.can_block_task(ready_task) {
        return false
    }
    if state.task_state_score(state.blocked_task_slot(ready_task).state) != 8 {
        return false
    }

    local_log: state.BootLog = state.empty_log()
    append0: state.BootLogAppendResult = state.append_record(local_log, state.BootStage.Reset, ARCH_ACTOR, 1)
    append1: state.BootLogAppendResult = state.append_record(append0.log, state.BootStage.TablesSeeded, ARCH_ACTOR, 2)
    append2: state.BootLogAppendResult = state.append_record(append1.log, state.BootStage.AddressSpaceReady, ARCH_ACTOR, 3)
    append3: state.BootLogAppendResult = state.append_record(append2.log, state.BootStage.UserEntryReady, ARCH_ACTOR, 4)
    append4: state.BootLogAppendResult = state.append_record(append3.log, state.BootStage.MarkerEmitted, ARCH_ACTOR, 5)
    append5: state.BootLogAppendResult = state.append_record(append4.log, state.BootStage.Halted, ARCH_ACTOR, 6)
    overflow: state.BootLogAppendResult = state.append_record(append5.log, state.BootStage.Halted, ARCH_ACTOR, 7)
    if overflow.appended != 0 {
        return false
    }
    return overflow.log.count == 6
}

func validate_syscall_contract_hardening() bool {
    local_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    payload: [4]u8 = endpoint.zero_payload()
    payload[0] = 81
    payload[1] = 85
    payload[2] = 69
    payload[3] = 85

    local_handles: capability.HandleTable = capability.handle_table_for_owner(INIT_PID)
    local_handles = capability.install_endpoint_handle(local_handles, INIT_ENDPOINT_HANDLE_SLOT, INIT_ENDPOINT_ID, 5)
    local_endpoints: endpoint.EndpointTable = endpoint.empty_table()
    local_endpoints = endpoint.install_endpoint(local_endpoints, INIT_PID, INIT_ENDPOINT_ID)

    send_one: syscall.SendResult = syscall.perform_send(local_gate, local_handles, local_endpoints, INIT_PID, syscall.build_send_request(INIT_ENDPOINT_HANDLE_SLOT, 4, payload))
    send_two: syscall.SendResult = syscall.perform_send(send_one.gate, send_one.handle_table, send_one.endpoints, INIT_PID, syscall.build_send_request(INIT_ENDPOINT_HANDLE_SLOT, 4, payload))
    send_block: syscall.SendResult = syscall.perform_send(send_two.gate, send_two.handle_table, send_two.endpoints, INIT_PID, syscall.build_send_request(INIT_ENDPOINT_HANDLE_SLOT, 4, payload))
    if syscall.status_score(send_block.status) != 4 {
        return false
    }
    if syscall.block_reason_score(send_block.block_reason) != 2 {
        return false
    }
    if send_block.endpoints.slots[0].queued_messages != 2 {
        return false
    }

    receive_one: syscall.ReceiveResult = syscall.perform_receive(send_block.gate, send_block.handle_table, send_block.endpoints, syscall.build_receive_request(INIT_ENDPOINT_HANDLE_SLOT))
    receive_two: syscall.ReceiveResult = syscall.perform_receive(receive_one.gate, receive_one.handle_table, receive_one.endpoints, syscall.build_receive_request(INIT_ENDPOINT_HANDLE_SLOT))
    receive_block: syscall.ReceiveResult = syscall.perform_receive(receive_two.gate, receive_two.handle_table, receive_two.endpoints, syscall.build_receive_request(INIT_ENDPOINT_HANDLE_SLOT))
    if syscall.status_score(receive_block.observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(receive_block.observation.block_reason) != 4 {
        return false
    }

    local_waits: capability.WaitTable = capability.wait_table_for_owner(INIT_PID)
    local_waits = capability.install_wait_handle(local_waits, CHILD_WAIT_HANDLE_SLOT, CHILD_PID)
    wait_block: syscall.WaitResult = syscall.perform_wait(local_gate, state.zero_process_slots(), state.zero_task_slots(), local_waits, syscall.build_wait_request(CHILD_WAIT_HANDLE_SLOT))
    if syscall.status_score(wait_block.observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(wait_block.observation.block_reason) != 8 {
        return false
    }

    sleep_tasks: [3]state.TaskSlot = state.zero_task_slots()
    sleep_tasks[1] = state.user_task_slot(CHILD_TID, CHILD_PID, CHILD_ASID, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    sleep_block: syscall.SleepResult = syscall.perform_sleep(local_gate, sleep_tasks, timer.empty_timer_state(), syscall.build_sleep_request(1, 2))
    if syscall.status_score(sleep_block.observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(sleep_block.observation.block_reason) != 16 {
        return false
    }

    spawn_process_slots: [3]state.ProcessSlot = state.zero_process_slots()
    spawn_process_slots[0] = state.boot_process_slot(BOOT_PID, BOOT_TASK_SLOT)
    spawn_process_slots[2] = state.init_process_slot(9, 2, 9)
    spawn_task_slots: [3]state.TaskSlot = state.zero_task_slots()
    spawn_task_slots[0] = state.boot_task_slot(BOOT_TID, BOOT_PID, BOOT_ENTRY_PC, BOOT_STACK_TOP)
    spawn_task_slots[2] = state.user_task_slot(9, 9, 9, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    spawn_waits: capability.WaitTable = capability.wait_table_for_owner(INIT_PID)
    local_program_capability: capability.CapabilitySlot = capability.bootstrap_init_program_slot(INIT_PID)
    local_spawn: syscall.SpawnResult = syscall.perform_spawn(local_gate, local_program_capability, spawn_process_slots, spawn_task_slots, spawn_waits, INIT_IMAGE, syscall.build_spawn_request(CHILD_WAIT_HANDLE_SLOT), CHILD_PID, CHILD_TID, CHILD_ASID, CHILD_ROOT_PAGE_TABLE)
    if syscall.status_score(local_spawn.observation.status) != 2 {
        return false
    }
    if local_spawn.process_slots[1].pid != CHILD_PID {
        return false
    }
    if local_spawn.process_slots[2].pid != 9 {
        return false
    }
    if local_spawn.task_slots[1].tid != CHILD_TID {
        return false
    }
    if local_spawn.task_slots[2].tid != 9 {
        return false
    }
    if !capability.wait_handle_installed(local_spawn.wait_table, CHILD_WAIT_HANDLE_SLOT) {
        return false
    }

    transfer_handles: capability.HandleTable = capability.handle_table_for_owner(INIT_PID)
    transfer_handles = capability.install_endpoint_handle(transfer_handles, INIT_ENDPOINT_HANDLE_SLOT, INIT_ENDPOINT_ID, 5)
    transfer_handles = capability.install_endpoint_handle(transfer_handles, TRANSFER_SOURCE_HANDLE_SLOT, TRANSFER_ENDPOINT_ID, 5)
    transfer_endpoints: endpoint.EndpointTable = endpoint.empty_table()
    transfer_endpoints = endpoint.install_endpoint(transfer_endpoints, INIT_PID, INIT_ENDPOINT_ID)
    transfer_send: syscall.SendResult = syscall.perform_send(local_gate, transfer_handles, transfer_endpoints, INIT_PID, syscall.build_transfer_send_request(INIT_ENDPOINT_HANDLE_SLOT, 4, payload, TRANSFER_SOURCE_HANDLE_SLOT))
    if capability.find_endpoint_for_handle(transfer_send.handle_table, TRANSFER_SOURCE_HANDLE_SLOT) != 0 {
        return false
    }
    blocked_receive: syscall.ReceiveResult = syscall.perform_receive(transfer_send.gate, transfer_send.handle_table, transfer_send.endpoints, syscall.build_transfer_receive_request(INIT_ENDPOINT_HANDLE_SLOT, INIT_ENDPOINT_HANDLE_SLOT))
    if syscall.status_score(blocked_receive.observation.status) != 8 {
        return false
    }
    if blocked_receive.endpoints.slots[0].queued_messages != 1 {
        return false
    }
    return capability.find_endpoint_for_handle(blocked_receive.handle_table, TRANSFER_SOURCE_HANDLE_SLOT) == 0
}

func validate_phase104_contract_hardening() bool {
    if BOOT_LOG_APPEND_FAILED != 0 {
        return false
    }
    if !validate_timer_hardening_contracts() {
        return false
    }
    if !validate_bootstrap_layout_contracts() {
        return false
    }
    if !validate_endpoint_and_capability_contracts() {
        return false
    }
    if !validate_state_hardening_contracts() {
        return false
    }
    return validate_syscall_contract_hardening()
}

func seed_log_service_program_capability() bool {
    LOG_SERVICE_PROGRAM_CAPABILITY = capability.CapabilitySlot{ slot_id: LOG_SERVICE_PROGRAM_SLOT, owner_pid: INIT_PID, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: LOG_SERVICE_PROGRAM_OBJECT_ID }
    return capability.is_program_capability(LOG_SERVICE_PROGRAM_CAPABILITY)
}

func spawn_log_service() bool {
    WAIT_TABLES[1] = capability.wait_table_for_owner(INIT_PID)
    HANDLE_TABLES[2] = capability.handle_table_for_owner(CHILD_PID)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(LOG_SERVICE_WAIT_HANDLE_SLOT)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(SYSCALL_GATE, LOG_SERVICE_PROGRAM_CAPABILITY, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], INIT_IMAGE, spawn_request, CHILD_PID, CHILD_TID, CHILD_ASID, CHILD_ROOT_PAGE_TABLE)
    SYSCALL_GATE = spawn_result.gate
    LOG_SERVICE_PROGRAM_CAPABILITY = spawn_result.program_capability
    PROCESS_SLOTS = spawn_result.process_slots
    TASK_SLOTS = spawn_result.task_slots
    WAIT_TABLES[1] = spawn_result.wait_table
    LOG_SERVICE_ADDRESS_SPACE = spawn_result.child_address_space
    LOG_SERVICE_USER_FRAME = spawn_result.child_frame
    LOG_SERVICE_SPAWN_OBSERVATION = spawn_result.observation
    if syscall.status_score(LOG_SERVICE_SPAWN_OBSERVATION.status) != 2 {
        return false
    }

    HANDLE_TABLES[2] = capability.install_endpoint_handle(HANDLE_TABLES[2], LOG_SERVICE_ENDPOINT_HANDLE_SLOT, INIT_ENDPOINT_ID, 3)
    LOG_SERVICE_STATE = log_service.service_state(CHILD_PID, LOG_SERVICE_ENDPOINT_HANDLE_SLOT)
    READY_QUEUE = state.user_ready_queue(CHILD_TID)
    return capability.find_endpoint_for_handle(HANDLE_TABLES[2], LOG_SERVICE_ENDPOINT_HANDLE_SLOT) == INIT_ENDPOINT_ID
}

func execute_log_service_request_reply() bool {
    request_payload: [4]u8 = endpoint.zero_payload()
    request_payload[0] = LOG_SERVICE_REQUEST_BYTE

    request_send_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, INIT_PID, syscall.build_send_request(INIT_ENDPOINT_HANDLE_SLOT, 1, request_payload))
    SYSCALL_GATE = request_send_result.gate
    HANDLE_TABLES[1] = request_send_result.handle_table
    ENDPOINTS = request_send_result.endpoints
    if syscall.status_score(request_send_result.status) != 2 {
        return false
    }

    service_receive_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[2], ENDPOINTS, syscall.build_receive_request(LOG_SERVICE_ENDPOINT_HANDLE_SLOT))
    SYSCALL_GATE = service_receive_result.gate
    HANDLE_TABLES[2] = service_receive_result.handle_table
    ENDPOINTS = service_receive_result.endpoints
    LOG_SERVICE_RECEIVE_OBSERVATION = service_receive_result.observation
    if syscall.status_score(LOG_SERVICE_RECEIVE_OBSERVATION.status) != 2 {
        return false
    }
    LOG_SERVICE_STATE = log_service.record_open_request(LOG_SERVICE_STATE, LOG_SERVICE_RECEIVE_OBSERVATION)

    ack_send_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[2], ENDPOINTS, CHILD_PID, syscall.build_send_request(LOG_SERVICE_ENDPOINT_HANDLE_SLOT, 1, log_service.ack_payload()))
    SYSCALL_GATE = ack_send_result.gate
    HANDLE_TABLES[2] = ack_send_result.handle_table
    ENDPOINTS = ack_send_result.endpoints
    if syscall.status_score(ack_send_result.status) != 2 {
        return false
    }

    init_receive_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, syscall.build_receive_request(INIT_ENDPOINT_HANDLE_SLOT))
    SYSCALL_GATE = init_receive_result.gate
    HANDLE_TABLES[1] = init_receive_result.handle_table
    ENDPOINTS = init_receive_result.endpoints
    LOG_SERVICE_ACK_OBSERVATION = init_receive_result.observation
    if syscall.status_score(LOG_SERVICE_ACK_OBSERVATION.status) != 2 {
        return false
    }

    LOG_SERVICE_STATE = log_service.record_ack(LOG_SERVICE_STATE, LOG_SERVICE_ACK_OBSERVATION.payload[0])
    LOG_SERVICE_HANDSHAKE = log_service.observe_handshake(LOG_SERVICE_STATE)
    return true
}

func simulate_log_service_exit() {
    processes: [3]state.ProcessSlot = PROCESS_SLOTS
    tasks: [3]state.TaskSlot = TASK_SLOTS
    wait_tables: [3]capability.WaitTable = WAIT_TABLES

    processes[2] = state.exit_process_slot(processes[2])
    tasks[2] = state.exit_task_slot(tasks[2])
    wait_tables[1] = capability.mark_wait_handle_exited(wait_tables[1], CHILD_PID, LOG_SERVICE_EXIT_CODE)

    PROCESS_SLOTS = processes
    TASK_SLOTS = tasks
    WAIT_TABLES = wait_tables
    READY_QUEUE = state.empty_queue()
}

func execute_phase105_log_service_handshake() bool {
    if !seed_log_service_program_capability() {
        return false
    }
    if !spawn_log_service() {
        return false
    }
    if !execute_log_service_request_reply() {
        return false
    }
    simulate_log_service_exit()

    wait_result: syscall.WaitResult = syscall.perform_wait(SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], syscall.build_wait_request(LOG_SERVICE_WAIT_HANDLE_SLOT))
    SYSCALL_GATE = wait_result.gate
    PROCESS_SLOTS = wait_result.process_slots
    TASK_SLOTS = wait_result.task_slots
    WAIT_TABLES[1] = wait_result.wait_table
    LOG_SERVICE_WAIT_OBSERVATION = wait_result.observation
    HANDLE_TABLES[2] = capability.empty_handle_table()
    READY_QUEUE = state.empty_queue()
    return syscall.status_score(LOG_SERVICE_WAIT_OBSERVATION.status) == 2
}

func validate_phase105_log_service_handshake() bool {
    if capability.kind_score(LOG_SERVICE_PROGRAM_CAPABILITY.kind) != 1 {
        return false
    }
    if syscall.id_score(SYSCALL_GATE.last_id) != 16 {
        return false
    }
    if syscall.status_score(SYSCALL_GATE.last_status) != 2 {
        return false
    }
    if SYSCALL_GATE.send_count != 5 {
        return false
    }
    if SYSCALL_GATE.receive_count != 5 {
        return false
    }
    if LOG_SERVICE_SPAWN_OBSERVATION.child_pid != CHILD_PID {
        return false
    }
    if LOG_SERVICE_SPAWN_OBSERVATION.child_tid != CHILD_TID {
        return false
    }
    if LOG_SERVICE_SPAWN_OBSERVATION.child_asid != CHILD_ASID {
        return false
    }
    if LOG_SERVICE_SPAWN_OBSERVATION.wait_handle_slot != LOG_SERVICE_WAIT_HANDLE_SLOT {
        return false
    }
    if syscall.status_score(LOG_SERVICE_SPAWN_OBSERVATION.status) != 2 {
        return false
    }
    if LOG_SERVICE_RECEIVE_OBSERVATION.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if LOG_SERVICE_RECEIVE_OBSERVATION.source_pid != INIT_PID {
        return false
    }
    if LOG_SERVICE_RECEIVE_OBSERVATION.payload_len != 1 {
        return false
    }
    if LOG_SERVICE_RECEIVE_OBSERVATION.payload[0] != LOG_SERVICE_REQUEST_BYTE {
        return false
    }
    if LOG_SERVICE_ACK_OBSERVATION.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if LOG_SERVICE_ACK_OBSERVATION.source_pid != CHILD_PID {
        return false
    }
    if LOG_SERVICE_ACK_OBSERVATION.payload_len != 1 {
        return false
    }
    if LOG_SERVICE_ACK_OBSERVATION.payload[0] != 33 {
        return false
    }
    if LOG_SERVICE_STATE.owner_pid != CHILD_PID {
        return false
    }
    if LOG_SERVICE_STATE.endpoint_handle_slot != LOG_SERVICE_ENDPOINT_HANDLE_SLOT {
        return false
    }
    if LOG_SERVICE_STATE.handled_request_count != 1 {
        return false
    }
    if LOG_SERVICE_STATE.ack_count != 1 {
        return false
    }
    if LOG_SERVICE_STATE.last_client_pid != INIT_PID {
        return false
    }
    if LOG_SERVICE_STATE.last_endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if LOG_SERVICE_STATE.last_request_len != 1 {
        return false
    }
    if LOG_SERVICE_STATE.last_request_byte != LOG_SERVICE_REQUEST_BYTE {
        return false
    }
    if LOG_SERVICE_STATE.last_ack_byte != 33 {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.service_pid != CHILD_PID {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.client_pid != INIT_PID {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if log_service.tag_score(LOG_SERVICE_HANDSHAKE.tag) != 4 {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.request_len != 1 {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.request_byte != LOG_SERVICE_REQUEST_BYTE {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.ack_byte != 33 {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.request_count != 1 {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.ack_count != 1 {
        return false
    }
    if syscall.status_score(LOG_SERVICE_WAIT_OBSERVATION.status) != 2 {
        return false
    }
    if LOG_SERVICE_WAIT_OBSERVATION.child_pid != CHILD_PID {
        return false
    }
    if LOG_SERVICE_WAIT_OBSERVATION.exit_code != LOG_SERVICE_EXIT_CODE {
        return false
    }
    if LOG_SERVICE_WAIT_OBSERVATION.wait_handle_slot != LOG_SERVICE_WAIT_HANDLE_SLOT {
        return false
    }
    if WAIT_TABLES[1].count != 0 {
        return false
    }
    if capability.find_child_for_wait_handle(WAIT_TABLES[1], LOG_SERVICE_WAIT_HANDLE_SLOT) != 0 {
        return false
    }
    if HANDLE_TABLES[2].count != 0 {
        return false
    }
    if READY_QUEUE.count != 0 {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[2].state) != 1 {
        return false
    }
    if state.task_state_score(TASK_SLOTS[2].state) != 1 {
        return false
    }
    if address_space.state_score(LOG_SERVICE_ADDRESS_SPACE.state) != 2 {
        return false
    }
    if LOG_SERVICE_ADDRESS_SPACE.owner_pid != CHILD_PID {
        return false
    }
    if LOG_SERVICE_USER_FRAME.task_id != CHILD_TID {
        return false
    }
    return true
}

func seed_echo_service_program_capability() bool {
    ECHO_SERVICE_PROGRAM_CAPABILITY = capability.CapabilitySlot{ slot_id: ECHO_SERVICE_PROGRAM_SLOT, owner_pid: INIT_PID, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: ECHO_SERVICE_PROGRAM_OBJECT_ID }
    return capability.is_program_capability(ECHO_SERVICE_PROGRAM_CAPABILITY)
}

func spawn_echo_service() bool {
    WAIT_TABLES[1] = capability.wait_table_for_owner(INIT_PID)
    HANDLE_TABLES[2] = capability.handle_table_for_owner(CHILD_PID)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(ECHO_SERVICE_WAIT_HANDLE_SLOT)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(SYSCALL_GATE, ECHO_SERVICE_PROGRAM_CAPABILITY, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], INIT_IMAGE, spawn_request, CHILD_PID, CHILD_TID, CHILD_ASID, CHILD_ROOT_PAGE_TABLE)
    SYSCALL_GATE = spawn_result.gate
    ECHO_SERVICE_PROGRAM_CAPABILITY = spawn_result.program_capability
    PROCESS_SLOTS = spawn_result.process_slots
    TASK_SLOTS = spawn_result.task_slots
    WAIT_TABLES[1] = spawn_result.wait_table
    ECHO_SERVICE_ADDRESS_SPACE = spawn_result.child_address_space
    ECHO_SERVICE_USER_FRAME = spawn_result.child_frame
    ECHO_SERVICE_SPAWN_OBSERVATION = spawn_result.observation
    if syscall.status_score(ECHO_SERVICE_SPAWN_OBSERVATION.status) != 2 {
        return false
    }

    HANDLE_TABLES[2] = capability.install_endpoint_handle(HANDLE_TABLES[2], ECHO_SERVICE_ENDPOINT_HANDLE_SLOT, INIT_ENDPOINT_ID, 3)
    ECHO_SERVICE_STATE = echo_service.service_state(CHILD_PID, ECHO_SERVICE_ENDPOINT_HANDLE_SLOT)
    READY_QUEUE = state.user_ready_queue(CHILD_TID)
    return capability.find_endpoint_for_handle(HANDLE_TABLES[2], ECHO_SERVICE_ENDPOINT_HANDLE_SLOT) == INIT_ENDPOINT_ID
}

func execute_echo_service_request_reply() bool {
    request_payload: [4]u8 = endpoint.zero_payload()
    request_payload[0] = ECHO_SERVICE_REQUEST_BYTE0
    request_payload[1] = ECHO_SERVICE_REQUEST_BYTE1

    request_send_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, INIT_PID, syscall.build_send_request(INIT_ENDPOINT_HANDLE_SLOT, 2, request_payload))
    SYSCALL_GATE = request_send_result.gate
    HANDLE_TABLES[1] = request_send_result.handle_table
    ENDPOINTS = request_send_result.endpoints
    if syscall.status_score(request_send_result.status) != 2 {
        return false
    }

    service_receive_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[2], ENDPOINTS, syscall.build_receive_request(ECHO_SERVICE_ENDPOINT_HANDLE_SLOT))
    SYSCALL_GATE = service_receive_result.gate
    HANDLE_TABLES[2] = service_receive_result.handle_table
    ENDPOINTS = service_receive_result.endpoints
    ECHO_SERVICE_RECEIVE_OBSERVATION = service_receive_result.observation
    if syscall.status_score(ECHO_SERVICE_RECEIVE_OBSERVATION.status) != 2 {
        return false
    }
    ECHO_SERVICE_STATE = echo_service.record_request(ECHO_SERVICE_STATE, ECHO_SERVICE_RECEIVE_OBSERVATION)

    reply_send_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[2], ENDPOINTS, CHILD_PID, syscall.build_send_request(ECHO_SERVICE_ENDPOINT_HANDLE_SLOT, 2, echo_service.reply_payload(ECHO_SERVICE_STATE)))
    SYSCALL_GATE = reply_send_result.gate
    HANDLE_TABLES[2] = reply_send_result.handle_table
    ENDPOINTS = reply_send_result.endpoints
    if syscall.status_score(reply_send_result.status) != 2 {
        return false
    }

    init_receive_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, syscall.build_receive_request(INIT_ENDPOINT_HANDLE_SLOT))
    SYSCALL_GATE = init_receive_result.gate
    HANDLE_TABLES[1] = init_receive_result.handle_table
    ENDPOINTS = init_receive_result.endpoints
    ECHO_SERVICE_REPLY_OBSERVATION = init_receive_result.observation
    if syscall.status_score(ECHO_SERVICE_REPLY_OBSERVATION.status) != 2 {
        return false
    }

    ECHO_SERVICE_STATE = echo_service.record_reply(ECHO_SERVICE_STATE, ECHO_SERVICE_REPLY_OBSERVATION)
    ECHO_SERVICE_EXCHANGE = echo_service.observe_exchange(ECHO_SERVICE_STATE)
    return true
}

func simulate_echo_service_exit() {
    processes: [3]state.ProcessSlot = PROCESS_SLOTS
    tasks: [3]state.TaskSlot = TASK_SLOTS
    wait_tables: [3]capability.WaitTable = WAIT_TABLES

    processes[2] = state.exit_process_slot(processes[2])
    tasks[2] = state.exit_task_slot(tasks[2])
    wait_tables[1] = capability.mark_wait_handle_exited(wait_tables[1], CHILD_PID, ECHO_SERVICE_EXIT_CODE)

    PROCESS_SLOTS = processes
    TASK_SLOTS = tasks
    WAIT_TABLES = wait_tables
    READY_QUEUE = state.empty_queue()
}

func execute_phase106_echo_service_request_reply() bool {
    if !seed_echo_service_program_capability() {
        return false
    }
    if !spawn_echo_service() {
        return false
    }
    if !execute_echo_service_request_reply() {
        return false
    }
    simulate_echo_service_exit()

    wait_result: syscall.WaitResult = syscall.perform_wait(SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], syscall.build_wait_request(ECHO_SERVICE_WAIT_HANDLE_SLOT))
    SYSCALL_GATE = wait_result.gate
    PROCESS_SLOTS = wait_result.process_slots
    TASK_SLOTS = wait_result.task_slots
    WAIT_TABLES[1] = wait_result.wait_table
    ECHO_SERVICE_WAIT_OBSERVATION = wait_result.observation
    HANDLE_TABLES[2] = capability.empty_handle_table()
    READY_QUEUE = state.empty_queue()
    return syscall.status_score(ECHO_SERVICE_WAIT_OBSERVATION.status) == 2
}

func validate_phase106_echo_service_request_reply() bool {
    if capability.kind_score(ECHO_SERVICE_PROGRAM_CAPABILITY.kind) != 1 {
        return false
    }
    if syscall.id_score(SYSCALL_GATE.last_id) != 16 {
        return false
    }
    if syscall.status_score(SYSCALL_GATE.last_status) != 2 {
        return false
    }
    if SYSCALL_GATE.send_count != 7 {
        return false
    }
    if SYSCALL_GATE.receive_count != 7 {
        return false
    }
    if ECHO_SERVICE_SPAWN_OBSERVATION.child_pid != CHILD_PID {
        return false
    }
    if ECHO_SERVICE_SPAWN_OBSERVATION.child_tid != CHILD_TID {
        return false
    }
    if ECHO_SERVICE_SPAWN_OBSERVATION.child_asid != CHILD_ASID {
        return false
    }
    if ECHO_SERVICE_SPAWN_OBSERVATION.wait_handle_slot != ECHO_SERVICE_WAIT_HANDLE_SLOT {
        return false
    }
    if syscall.status_score(ECHO_SERVICE_SPAWN_OBSERVATION.status) != 2 {
        return false
    }
    if ECHO_SERVICE_RECEIVE_OBSERVATION.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if ECHO_SERVICE_RECEIVE_OBSERVATION.source_pid != INIT_PID {
        return false
    }
    if ECHO_SERVICE_RECEIVE_OBSERVATION.payload_len != 2 {
        return false
    }
    if ECHO_SERVICE_RECEIVE_OBSERVATION.payload[0] != ECHO_SERVICE_REQUEST_BYTE0 {
        return false
    }
    if ECHO_SERVICE_RECEIVE_OBSERVATION.payload[1] != ECHO_SERVICE_REQUEST_BYTE1 {
        return false
    }
    if ECHO_SERVICE_REPLY_OBSERVATION.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if ECHO_SERVICE_REPLY_OBSERVATION.source_pid != CHILD_PID {
        return false
    }
    if ECHO_SERVICE_REPLY_OBSERVATION.payload_len != 2 {
        return false
    }
    if ECHO_SERVICE_REPLY_OBSERVATION.payload[0] != ECHO_SERVICE_REQUEST_BYTE0 {
        return false
    }
    if ECHO_SERVICE_REPLY_OBSERVATION.payload[1] != ECHO_SERVICE_REQUEST_BYTE1 {
        return false
    }
    if ECHO_SERVICE_STATE.owner_pid != CHILD_PID {
        return false
    }
    if ECHO_SERVICE_STATE.endpoint_handle_slot != ECHO_SERVICE_ENDPOINT_HANDLE_SLOT {
        return false
    }
    if ECHO_SERVICE_STATE.request_count != 1 {
        return false
    }
    if ECHO_SERVICE_STATE.reply_count != 1 {
        return false
    }
    if ECHO_SERVICE_STATE.last_client_pid != INIT_PID {
        return false
    }
    if ECHO_SERVICE_STATE.last_endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if ECHO_SERVICE_STATE.last_request_len != 2 {
        return false
    }
    if ECHO_SERVICE_STATE.last_request_byte0 != ECHO_SERVICE_REQUEST_BYTE0 {
        return false
    }
    if ECHO_SERVICE_STATE.last_request_byte1 != ECHO_SERVICE_REQUEST_BYTE1 {
        return false
    }
    if ECHO_SERVICE_STATE.last_reply_len != 2 {
        return false
    }
    if ECHO_SERVICE_STATE.last_reply_byte0 != ECHO_SERVICE_REQUEST_BYTE0 {
        return false
    }
    if ECHO_SERVICE_STATE.last_reply_byte1 != ECHO_SERVICE_REQUEST_BYTE1 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.service_pid != CHILD_PID {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.client_pid != INIT_PID {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if echo_service.tag_score(ECHO_SERVICE_EXCHANGE.tag) != 4 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.request_len != 2 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.request_byte0 != ECHO_SERVICE_REQUEST_BYTE0 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.request_byte1 != ECHO_SERVICE_REQUEST_BYTE1 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.reply_len != 2 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.reply_byte0 != ECHO_SERVICE_REQUEST_BYTE0 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.reply_byte1 != ECHO_SERVICE_REQUEST_BYTE1 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.request_count != 1 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.reply_count != 1 {
        return false
    }
    if syscall.status_score(ECHO_SERVICE_WAIT_OBSERVATION.status) != 2 {
        return false
    }
    if ECHO_SERVICE_WAIT_OBSERVATION.child_pid != CHILD_PID {
        return false
    }
    if ECHO_SERVICE_WAIT_OBSERVATION.exit_code != ECHO_SERVICE_EXIT_CODE {
        return false
    }
    if ECHO_SERVICE_WAIT_OBSERVATION.wait_handle_slot != ECHO_SERVICE_WAIT_HANDLE_SLOT {
        return false
    }
    if WAIT_TABLES[1].count != 0 {
        return false
    }
    if capability.find_child_for_wait_handle(WAIT_TABLES[1], ECHO_SERVICE_WAIT_HANDLE_SLOT) != 0 {
        return false
    }
    if HANDLE_TABLES[2].count != 0 {
        return false
    }
    if READY_QUEUE.count != 0 {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[2].state) != 1 {
        return false
    }
    if state.task_state_score(TASK_SLOTS[2].state) != 1 {
        return false
    }
    if address_space.state_score(ECHO_SERVICE_ADDRESS_SPACE.state) != 2 {
        return false
    }
    if ECHO_SERVICE_ADDRESS_SPACE.owner_pid != CHILD_PID {
        return false
    }
    if ECHO_SERVICE_USER_FRAME.task_id != CHILD_TID {
        return false
    }
    return true
}

func seed_transfer_service_program_capability() bool {
    TRANSFER_SERVICE_PROGRAM_CAPABILITY = capability.CapabilitySlot{ slot_id: TRANSFER_SERVICE_PROGRAM_SLOT, owner_pid: INIT_PID, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: TRANSFER_SERVICE_PROGRAM_OBJECT_ID }
    return capability.is_program_capability(TRANSFER_SERVICE_PROGRAM_CAPABILITY)
}

func seed_transfer_service_sender_handle() bool {
    HANDLE_TABLES[1] = capability.install_endpoint_handle(HANDLE_TABLES[1], TRANSFER_SOURCE_HANDLE_SLOT, TRANSFER_ENDPOINT_ID, 5)
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_RECEIVED_HANDLE_SLOT) != TRANSFER_ENDPOINT_ID {
        return false
    }
    if capability.find_rights_for_handle(HANDLE_TABLES[1], TRANSFER_RECEIVED_HANDLE_SLOT) != 5 {
        return false
    }
    return capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_SOURCE_HANDLE_SLOT) == TRANSFER_ENDPOINT_ID
}

func spawn_transfer_service() bool {
    WAIT_TABLES[1] = capability.wait_table_for_owner(INIT_PID)
    HANDLE_TABLES[2] = capability.handle_table_for_owner(CHILD_PID)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(TRANSFER_SERVICE_WAIT_HANDLE_SLOT)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(SYSCALL_GATE, TRANSFER_SERVICE_PROGRAM_CAPABILITY, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], INIT_IMAGE, spawn_request, CHILD_PID, CHILD_TID, CHILD_ASID, CHILD_ROOT_PAGE_TABLE)
    SYSCALL_GATE = spawn_result.gate
    TRANSFER_SERVICE_PROGRAM_CAPABILITY = spawn_result.program_capability
    PROCESS_SLOTS = spawn_result.process_slots
    TASK_SLOTS = spawn_result.task_slots
    WAIT_TABLES[1] = spawn_result.wait_table
    TRANSFER_SERVICE_ADDRESS_SPACE = spawn_result.child_address_space
    TRANSFER_SERVICE_USER_FRAME = spawn_result.child_frame
    TRANSFER_SERVICE_SPAWN_OBSERVATION = spawn_result.observation
    if syscall.status_score(TRANSFER_SERVICE_SPAWN_OBSERVATION.status) != 2 {
        return false
    }

    HANDLE_TABLES[2] = capability.install_endpoint_handle(HANDLE_TABLES[2], TRANSFER_SERVICE_CONTROL_HANDLE_SLOT, INIT_ENDPOINT_ID, 3)
    TRANSFER_SERVICE_STATE = transfer_service.service_state(CHILD_PID, TRANSFER_SERVICE_CONTROL_HANDLE_SLOT, TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT)
    READY_QUEUE = state.user_ready_queue(CHILD_TID)
    return capability.find_endpoint_for_handle(HANDLE_TABLES[2], TRANSFER_SERVICE_CONTROL_HANDLE_SLOT) == INIT_ENDPOINT_ID
}

func execute_user_to_user_capability_transfer() bool {
    grant_payload: [4]u8 = endpoint.zero_payload()
    grant_payload[0] = TRANSFER_SERVICE_GRANT_BYTE0
    grant_payload[1] = TRANSFER_SERVICE_GRANT_BYTE1
    grant_payload[2] = TRANSFER_SERVICE_GRANT_BYTE2
    grant_payload[3] = TRANSFER_SERVICE_GRANT_BYTE3

    transfer_send_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, INIT_PID, syscall.build_transfer_send_request(INIT_ENDPOINT_HANDLE_SLOT, 4, grant_payload, TRANSFER_SOURCE_HANDLE_SLOT))
    SYSCALL_GATE = transfer_send_result.gate
    HANDLE_TABLES[1] = transfer_send_result.handle_table
    ENDPOINTS = transfer_send_result.endpoints
    if syscall.status_score(transfer_send_result.status) != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_SOURCE_HANDLE_SLOT) != 0 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_RECEIVED_HANDLE_SLOT) != TRANSFER_ENDPOINT_ID {
        return false
    }

    service_receive_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[2], ENDPOINTS, syscall.build_transfer_receive_request(TRANSFER_SERVICE_CONTROL_HANDLE_SLOT, TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT))
    SYSCALL_GATE = service_receive_result.gate
    HANDLE_TABLES[2] = service_receive_result.handle_table
    ENDPOINTS = service_receive_result.endpoints
    TRANSFER_SERVICE_GRANT_OBSERVATION = service_receive_result.observation
    if syscall.status_score(TRANSFER_SERVICE_GRANT_OBSERVATION.status) != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[2], TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT) != TRANSFER_ENDPOINT_ID {
        return false
    }

    transferred_endpoint_id: u32 = capability.find_endpoint_for_handle(HANDLE_TABLES[2], TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT)
    transferred_rights: u32 = capability.find_rights_for_handle(HANDLE_TABLES[2], TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT)
    TRANSFER_SERVICE_STATE = transfer_service.record_grant(TRANSFER_SERVICE_STATE, TRANSFER_SERVICE_GRANT_OBSERVATION, transferred_endpoint_id, transferred_rights)

    emit_payload: [4]u8 = transfer_service.emit_payload(TRANSFER_SERVICE_STATE)
    emit_send_result: syscall.SendResult = syscall.perform_send(SYSCALL_GATE, HANDLE_TABLES[2], ENDPOINTS, CHILD_PID, syscall.build_send_request(TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT, 4, emit_payload))
    SYSCALL_GATE = emit_send_result.gate
    HANDLE_TABLES[2] = emit_send_result.handle_table
    ENDPOINTS = emit_send_result.endpoints
    if syscall.status_score(emit_send_result.status) != 2 {
        return false
    }

    init_receive_result: syscall.ReceiveResult = syscall.perform_receive(SYSCALL_GATE, HANDLE_TABLES[1], ENDPOINTS, syscall.build_receive_request(TRANSFER_RECEIVED_HANDLE_SLOT))
    SYSCALL_GATE = init_receive_result.gate
    HANDLE_TABLES[1] = init_receive_result.handle_table
    ENDPOINTS = init_receive_result.endpoints
    TRANSFER_SERVICE_EMIT_OBSERVATION = init_receive_result.observation
    if syscall.status_score(TRANSFER_SERVICE_EMIT_OBSERVATION.status) != 2 {
        return false
    }

    TRANSFER_SERVICE_STATE = transfer_service.record_emit(TRANSFER_SERVICE_STATE, TRANSFER_SERVICE_EMIT_OBSERVATION)
    TRANSFER_SERVICE_TRANSFER = transfer_service.observe_transfer(TRANSFER_SERVICE_STATE)
    return true
}

func simulate_transfer_service_exit() {
    processes: [3]state.ProcessSlot = PROCESS_SLOTS
    tasks: [3]state.TaskSlot = TASK_SLOTS
    wait_tables: [3]capability.WaitTable = WAIT_TABLES

    processes[2] = state.exit_process_slot(processes[2])
    tasks[2] = state.exit_task_slot(tasks[2])
    wait_tables[1] = capability.mark_wait_handle_exited(wait_tables[1], CHILD_PID, TRANSFER_SERVICE_EXIT_CODE)

    PROCESS_SLOTS = processes
    TASK_SLOTS = tasks
    WAIT_TABLES = wait_tables
    READY_QUEUE = state.empty_queue()
}

func execute_phase107_user_to_user_capability_transfer() bool {
    if !seed_transfer_service_program_capability() {
        return false
    }
    if !seed_transfer_service_sender_handle() {
        return false
    }
    if !spawn_transfer_service() {
        return false
    }
    if !execute_user_to_user_capability_transfer() {
        return false
    }
    simulate_transfer_service_exit()

    wait_result: syscall.WaitResult = syscall.perform_wait(SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], syscall.build_wait_request(TRANSFER_SERVICE_WAIT_HANDLE_SLOT))
    SYSCALL_GATE = wait_result.gate
    PROCESS_SLOTS = wait_result.process_slots
    TASK_SLOTS = wait_result.task_slots
    WAIT_TABLES[1] = wait_result.wait_table
    TRANSFER_SERVICE_WAIT_OBSERVATION = wait_result.observation
    HANDLE_TABLES[2] = capability.empty_handle_table()
    READY_QUEUE = state.empty_queue()
    return syscall.status_score(TRANSFER_SERVICE_WAIT_OBSERVATION.status) == 2
}

func validate_phase107_user_to_user_capability_transfer() bool {
    if capability.kind_score(TRANSFER_SERVICE_PROGRAM_CAPABILITY.kind) != 1 {
        return false
    }
    if syscall.id_score(SYSCALL_GATE.last_id) != 16 {
        return false
    }
    if syscall.status_score(SYSCALL_GATE.last_status) != 2 {
        return false
    }
    if SYSCALL_GATE.send_count != 9 {
        return false
    }
    if SYSCALL_GATE.receive_count != 9 {
        return false
    }
    if TRANSFER_SERVICE_SPAWN_OBSERVATION.child_pid != CHILD_PID {
        return false
    }
    if TRANSFER_SERVICE_SPAWN_OBSERVATION.child_tid != CHILD_TID {
        return false
    }
    if TRANSFER_SERVICE_SPAWN_OBSERVATION.child_asid != CHILD_ASID {
        return false
    }
    if TRANSFER_SERVICE_SPAWN_OBSERVATION.wait_handle_slot != TRANSFER_SERVICE_WAIT_HANDLE_SLOT {
        return false
    }
    if syscall.status_score(TRANSFER_SERVICE_SPAWN_OBSERVATION.status) != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_SOURCE_HANDLE_SLOT) != 0 {
        return false
    }
    if capability.find_endpoint_for_handle(HANDLE_TABLES[1], TRANSFER_RECEIVED_HANDLE_SLOT) != TRANSFER_ENDPOINT_ID {
        return false
    }
    if capability.find_rights_for_handle(HANDLE_TABLES[1], TRANSFER_RECEIVED_HANDLE_SLOT) != 5 {
        return false
    }
    if TRANSFER_SERVICE_GRANT_OBSERVATION.endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if TRANSFER_SERVICE_GRANT_OBSERVATION.source_pid != INIT_PID {
        return false
    }
    if TRANSFER_SERVICE_GRANT_OBSERVATION.payload_len != 4 {
        return false
    }
    if TRANSFER_SERVICE_GRANT_OBSERVATION.received_handle_slot != TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT {
        return false
    }
    if TRANSFER_SERVICE_GRANT_OBSERVATION.received_handle_count != 1 {
        return false
    }
    if TRANSFER_SERVICE_GRANT_OBSERVATION.payload[0] != TRANSFER_SERVICE_GRANT_BYTE0 {
        return false
    }
    if TRANSFER_SERVICE_GRANT_OBSERVATION.payload[1] != TRANSFER_SERVICE_GRANT_BYTE1 {
        return false
    }
    if TRANSFER_SERVICE_GRANT_OBSERVATION.payload[2] != TRANSFER_SERVICE_GRANT_BYTE2 {
        return false
    }
    if TRANSFER_SERVICE_GRANT_OBSERVATION.payload[3] != TRANSFER_SERVICE_GRANT_BYTE3 {
        return false
    }
    if TRANSFER_SERVICE_EMIT_OBSERVATION.endpoint_id != TRANSFER_ENDPOINT_ID {
        return false
    }
    if TRANSFER_SERVICE_EMIT_OBSERVATION.source_pid != CHILD_PID {
        return false
    }
    if TRANSFER_SERVICE_EMIT_OBSERVATION.payload_len != 4 {
        return false
    }
    if TRANSFER_SERVICE_EMIT_OBSERVATION.received_handle_slot != 0 {
        return false
    }
    if TRANSFER_SERVICE_EMIT_OBSERVATION.received_handle_count != 0 {
        return false
    }
    if TRANSFER_SERVICE_EMIT_OBSERVATION.payload[0] != TRANSFER_SERVICE_GRANT_BYTE0 {
        return false
    }
    if TRANSFER_SERVICE_EMIT_OBSERVATION.payload[1] != TRANSFER_SERVICE_GRANT_BYTE1 {
        return false
    }
    if TRANSFER_SERVICE_EMIT_OBSERVATION.payload[2] != TRANSFER_SERVICE_GRANT_BYTE2 {
        return false
    }
    if TRANSFER_SERVICE_EMIT_OBSERVATION.payload[3] != TRANSFER_SERVICE_GRANT_BYTE3 {
        return false
    }
    if TRANSFER_SERVICE_STATE.owner_pid != CHILD_PID {
        return false
    }
    if TRANSFER_SERVICE_STATE.control_handle_slot != TRANSFER_SERVICE_CONTROL_HANDLE_SLOT {
        return false
    }
    if TRANSFER_SERVICE_STATE.transferred_handle_slot != TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT {
        return false
    }
    if TRANSFER_SERVICE_STATE.grant_count != 1 {
        return false
    }
    if TRANSFER_SERVICE_STATE.emit_count != 1 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_client_pid != INIT_PID {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_control_endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_transferred_endpoint_id != TRANSFER_ENDPOINT_ID {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_transferred_rights != 5 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_grant_len != 4 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_grant_byte0 != TRANSFER_SERVICE_GRANT_BYTE0 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_grant_byte1 != TRANSFER_SERVICE_GRANT_BYTE1 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_grant_byte2 != TRANSFER_SERVICE_GRANT_BYTE2 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_grant_byte3 != TRANSFER_SERVICE_GRANT_BYTE3 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_emit_len != 4 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_emit_byte0 != TRANSFER_SERVICE_GRANT_BYTE0 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_emit_byte1 != TRANSFER_SERVICE_GRANT_BYTE1 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_emit_byte2 != TRANSFER_SERVICE_GRANT_BYTE2 {
        return false
    }
    if TRANSFER_SERVICE_STATE.last_emit_byte3 != TRANSFER_SERVICE_GRANT_BYTE3 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.service_pid != CHILD_PID {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.client_pid != INIT_PID {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.control_endpoint_id != INIT_ENDPOINT_ID {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.transferred_endpoint_id != TRANSFER_ENDPOINT_ID {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.transferred_rights != 5 {
        return false
    }
    if transfer_service.tag_score(TRANSFER_SERVICE_TRANSFER.tag) != 4 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.grant_len != 4 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.grant_byte0 != TRANSFER_SERVICE_GRANT_BYTE0 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.grant_byte1 != TRANSFER_SERVICE_GRANT_BYTE1 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.grant_byte2 != TRANSFER_SERVICE_GRANT_BYTE2 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.grant_byte3 != TRANSFER_SERVICE_GRANT_BYTE3 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.emit_len != 4 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.emit_byte0 != TRANSFER_SERVICE_GRANT_BYTE0 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.emit_byte1 != TRANSFER_SERVICE_GRANT_BYTE1 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.emit_byte2 != TRANSFER_SERVICE_GRANT_BYTE2 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.emit_byte3 != TRANSFER_SERVICE_GRANT_BYTE3 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.grant_count != 1 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.emit_count != 1 {
        return false
    }
    if syscall.status_score(TRANSFER_SERVICE_WAIT_OBSERVATION.status) != 2 {
        return false
    }
    if TRANSFER_SERVICE_WAIT_OBSERVATION.child_pid != CHILD_PID {
        return false
    }
    if TRANSFER_SERVICE_WAIT_OBSERVATION.exit_code != TRANSFER_SERVICE_EXIT_CODE {
        return false
    }
    if TRANSFER_SERVICE_WAIT_OBSERVATION.wait_handle_slot != TRANSFER_SERVICE_WAIT_HANDLE_SLOT {
        return false
    }
    if WAIT_TABLES[1].count != 0 {
        return false
    }
    if capability.find_child_for_wait_handle(WAIT_TABLES[1], TRANSFER_SERVICE_WAIT_HANDLE_SLOT) != 0 {
        return false
    }
    if HANDLE_TABLES[2].count != 0 {
        return false
    }
    if READY_QUEUE.count != 0 {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[2].state) != 1 {
        return false
    }
    if state.task_state_score(TASK_SLOTS[2].state) != 1 {
        return false
    }
    if address_space.state_score(TRANSFER_SERVICE_ADDRESS_SPACE.state) != 2 {
        return false
    }
    if TRANSFER_SERVICE_ADDRESS_SPACE.owner_pid != CHILD_PID {
        return false
    }
    if TRANSFER_SERVICE_USER_FRAME.task_id != CHILD_TID {
        return false
    }
    return true
}

func validate_phase108_kernel_image_and_program_cap_contracts() bool {
    if !capability.is_program_capability(INIT_BOOTSTRAP_CAPS.program_capability) {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.owner_pid != INIT_PID {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.slot_id != 2 {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.rights != 7 {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.object_id != 1 {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.object_id == LOG_SERVICE_PROGRAM_OBJECT_ID {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.object_id == ECHO_SERVICE_PROGRAM_OBJECT_ID {
        return false
    }
    if INIT_BOOTSTRAP_CAPS.program_capability.object_id == TRANSFER_SERVICE_PROGRAM_OBJECT_ID {
        return false
    }
    if capability.kind_score(LOG_SERVICE_PROGRAM_CAPABILITY.kind) != 1 {
        return false
    }
    if capability.kind_score(ECHO_SERVICE_PROGRAM_CAPABILITY.kind) != 1 {
        return false
    }
    if capability.kind_score(TRANSFER_SERVICE_PROGRAM_CAPABILITY.kind) != 1 {
        return false
    }
    if LOG_SERVICE_SPAWN_OBSERVATION.wait_handle_slot != LOG_SERVICE_WAIT_HANDLE_SLOT {
        return false
    }
    if ECHO_SERVICE_SPAWN_OBSERVATION.wait_handle_slot != ECHO_SERVICE_WAIT_HANDLE_SLOT {
        return false
    }
    if TRANSFER_SERVICE_SPAWN_OBSERVATION.wait_handle_slot != TRANSFER_SERVICE_WAIT_HANDLE_SLOT {
        return false
    }
    if LOG_SERVICE_WAIT_OBSERVATION.exit_code != LOG_SERVICE_EXIT_CODE {
        return false
    }
    if ECHO_SERVICE_WAIT_OBSERVATION.exit_code != ECHO_SERVICE_EXIT_CODE {
        return false
    }
    if TRANSFER_SERVICE_WAIT_OBSERVATION.exit_code != TRANSFER_SERVICE_EXIT_CODE {
        return false
    }
    return true
}

func validate_phase109_first_running_kernel_slice() bool {
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
    if INIT_BOOTSTRAP_HANDOFF.authority_count != 2 {
        return false
    }
    if INIT_BOOTSTRAP_HANDOFF.ambient_root_visible != 0 {
        return false
    }
    if syscall.status_score(RECEIVE_OBSERVATION.status) != 2 {
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
    if syscall.status_score(ATTACHED_RECEIVE_OBSERVATION.status) != 2 {
        return false
    }
    if ATTACHED_RECEIVE_OBSERVATION.received_handle_count != 1 {
        return false
    }
    if syscall.status_score(TRANSFERRED_HANDLE_USE_OBSERVATION.status) != 2 {
        return false
    }
    if TRANSFERRED_HANDLE_USE_OBSERVATION.payload_len != 4 {
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
    if syscall.status_score(PRE_EXIT_WAIT_OBSERVATION.status) != 4 {
        return false
    }
    if syscall.status_score(EXIT_WAIT_OBSERVATION.status) != 2 {
        return false
    }
    if EXIT_WAIT_OBSERVATION.exit_code != CHILD_EXIT_CODE {
        return false
    }
    if syscall.status_score(SLEEP_OBSERVATION.status) != 4 {
        return false
    }
    if syscall.block_reason_score(SLEEP_OBSERVATION.block_reason) != 16 {
        return false
    }
    if TIMER_WAKE_OBSERVATION.task_id != CHILD_TID {
        return false
    }
    if TIMER_WAKE_OBSERVATION.wake_tick != 1 {
        return false
    }
    if !validate_phase104_contract_hardening() {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.request_count != 1 {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.ack_count != 1 {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.request_byte != LOG_SERVICE_REQUEST_BYTE {
        return false
    }
    if LOG_SERVICE_HANDSHAKE.ack_byte != 33 {
        return false
    }
    if LOG_SERVICE_WAIT_OBSERVATION.exit_code != LOG_SERVICE_EXIT_CODE {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.reply_count != 1 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.reply_byte0 != ECHO_SERVICE_REQUEST_BYTE0 {
        return false
    }
    if ECHO_SERVICE_EXCHANGE.reply_byte1 != ECHO_SERVICE_REQUEST_BYTE1 {
        return false
    }
    if ECHO_SERVICE_WAIT_OBSERVATION.exit_code != ECHO_SERVICE_EXIT_CODE {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.transferred_endpoint_id != TRANSFER_ENDPOINT_ID {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.transferred_rights != 5 {
        return false
    }
    if TRANSFER_SERVICE_TRANSFER.emit_count != 1 {
        return false
    }
    if TRANSFER_SERVICE_WAIT_OBSERVATION.exit_code != TRANSFER_SERVICE_EXIT_CODE {
        return false
    }
    if !validate_phase108_kernel_image_and_program_cap_contracts() {
        return false
    }
    if PROCESS_SLOTS[1].pid != INIT_PID {
        return false
    }
    if TASK_SLOTS[1].tid != INIT_TID {
        return false
    }
    if USER_FRAME.task_id != INIT_TID {
        return false
    }
    if BOOT_LOG_APPEND_FAILED != 0 {
        return false
    }
    return true
}

func execute_spawn_child_process() bool {
    WAIT_TABLES[1] = capability.wait_table_for_owner(INIT_PID)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(CHILD_WAIT_HANDLE_SLOT)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(SYSCALL_GATE, INIT_PROGRAM_CAPABILITY, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], INIT_IMAGE, spawn_request, CHILD_PID, CHILD_TID, CHILD_ASID, CHILD_ROOT_PAGE_TABLE)
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
    TIMER_STATE = timer.advance_tick(TIMER_STATE, 1)
    wake_result: timer.TimerWakeResult = timer.wake_fired_sleepers(TIMER_STATE)
    TIMER_STATE = wake_result.timer_state
    TIMER_WAKE_OBSERVATION = wake_result.observation
    TIMER_STATE = timer.consume_wake(TIMER_STATE, TIMER_WAKE_OBSERVATION.task_id)
    TASK_SLOTS[2] = state.user_task_slot(TASK_SLOTS[2].tid, TASK_SLOTS[2].owner_pid, TASK_SLOTS[2].address_space_id, TASK_SLOTS[2].entry_pc, TASK_SLOTS[2].stack_top)
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
    processes[2] = state.exit_process_slot(processes[2])
    tasks[2] = state.exit_task_slot(tasks[2])
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

func validate_program_cap_spawn_and_wait() bool {
    if syscall.id_score(SYSCALL_GATE.last_id) != 16 {
        return false
    }
    if syscall.status_score(SYSCALL_GATE.last_status) != 2 {
        return false
    }
    if capability.kind_score(INIT_PROGRAM_CAPABILITY.kind) != 1 {
        return false
    }
    if READY_QUEUE.count != 0 {
        return false
    }
    if SPAWN_OBSERVATION.child_pid != CHILD_PID {
        return false
    }
    if SPAWN_OBSERVATION.child_tid != CHILD_TID {
        return false
    }
    if SPAWN_OBSERVATION.child_asid != CHILD_ASID {
        return false
    }
    if SPAWN_OBSERVATION.wait_handle_slot != CHILD_WAIT_HANDLE_SLOT {
        return false
    }
    if syscall.status_score(SPAWN_OBSERVATION.status) != 2 {
        return false
    }
    if syscall.status_score(PRE_EXIT_WAIT_OBSERVATION.status) != 4 {
        return false
    }
    if syscall.block_reason_score(PRE_EXIT_WAIT_OBSERVATION.block_reason) != 8 {
        return false
    }
    if PRE_EXIT_WAIT_OBSERVATION.child_pid != CHILD_PID {
        return false
    }
    if PRE_EXIT_WAIT_OBSERVATION.exit_code != 0 {
        return false
    }
    if PRE_EXIT_WAIT_OBSERVATION.wait_handle_slot != CHILD_WAIT_HANDLE_SLOT {
        return false
    }
    if syscall.status_score(EXIT_WAIT_OBSERVATION.status) != 2 {
        return false
    }
    if EXIT_WAIT_OBSERVATION.child_pid != CHILD_PID {
        return false
    }
    if EXIT_WAIT_OBSERVATION.exit_code != CHILD_EXIT_CODE {
        return false
    }
    if EXIT_WAIT_OBSERVATION.wait_handle_slot != CHILD_WAIT_HANDLE_SLOT {
        return false
    }
    if syscall.block_reason_score(EXIT_WAIT_OBSERVATION.block_reason) != 1 {
        return false
    }
    if syscall.status_score(SLEEP_OBSERVATION.status) != 4 {
        return false
    }
    if SLEEP_OBSERVATION.task_id != CHILD_TID {
        return false
    }
    if SLEEP_OBSERVATION.deadline_tick != 1 {
        return false
    }
    if syscall.block_reason_score(SLEEP_OBSERVATION.block_reason) != 16 {
        return false
    }
    if SLEEP_OBSERVATION.wake_tick != 0 {
        return false
    }
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
    if TIMER_STATE.monotonic_tick != 1 {
        return false
    }
    if TIMER_STATE.wake_count != 1 {
        return false
    }
    if TIMER_STATE.count != 0 {
        return false
    }
    if WAKE_READY_QUEUE.count != 1 {
        return false
    }
    if state.ready_slot_at(WAKE_READY_QUEUE, 0) != CHILD_TID {
        return false
    }
    if WAIT_TABLES[1].owner_pid != INIT_PID {
        return false
    }
    if WAIT_TABLES[1].count != 0 {
        return false
    }
    if capability.find_child_for_wait_handle(WAIT_TABLES[1], CHILD_WAIT_HANDLE_SLOT) != 0 {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[2].state) != 1 {
        return false
    }
    if state.task_state_score(TASK_SLOTS[2].state) != 1 {
        return false
    }
    if address_space.state_score(CHILD_ADDRESS_SPACE.state) != 2 {
        return false
    }
    if CHILD_ADDRESS_SPACE.asid != CHILD_ASID {
        return false
    }
    if CHILD_ADDRESS_SPACE.owner_pid != CHILD_PID {
        return false
    }
    if CHILD_ADDRESS_SPACE.root_page_table != CHILD_ROOT_PAGE_TABLE {
        return false
    }
    if CHILD_ADDRESS_SPACE.entry_pc != INIT_IMAGE.entry_pc {
        return false
    }
    if CHILD_ADDRESS_SPACE.stack_top != INIT_IMAGE.stack_top {
        return false
    }
    if CHILD_ADDRESS_SPACE.mapping_count != 2 {
        return false
    }
    if CHILD_USER_FRAME.task_id != CHILD_TID {
        return false
    }
    if CHILD_USER_FRAME.address_space_id != CHILD_ASID {
        return false
    }
    if READY_QUEUE.count != 0 {
        return false
    }
    return true
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
    if !validate_program_cap_spawn_and_wait() {
        return 27
    }
    if !validate_phase104_contract_hardening() {
        return 28
    }
    if !execute_phase105_log_service_handshake() {
        return 29
    }
    if !validate_phase105_log_service_handshake() {
        return 30
    }
    if !execute_phase106_echo_service_request_reply() {
        return 31
    }
    if !validate_phase106_echo_service_request_reply() {
        return 32
    }
    if !execute_phase107_user_to_user_capability_transfer() {
        return 33
    }
    if !validate_phase107_user_to_user_capability_transfer() {
        return 34
    }
    if !validate_phase108_kernel_image_and_program_cap_contracts() {
        return 35
    }
    if !validate_phase109_first_running_kernel_slice() {
        return 36
    }
    BOOT_MARKER_EMITTED = 1
    record_boot_stage(state.BootStage.MarkerEmitted, 109)
    if BOOT_MARKER_EMITTED != 1 {
        return 37
    }
    if BOOT_LOG_APPEND_FAILED != 0 {
        return 38
    }
    if BOOT_LOG.count != 5 {
        return 39
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 3)) != 8 {
        return 40
    }
    if state.log_actor_at(BOOT_LOG, 3) != ARCH_ACTOR {
        return 41
    }
    if state.log_detail_at(BOOT_LOG, 3) != INIT_TID {
        return 42
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 4)) != 16 {
        return 43
    }
    if state.log_actor_at(BOOT_LOG, 4) != ARCH_ACTOR {
        return 44
    }
    if state.log_detail_at(BOOT_LOG, 4) != 109 {
        return 45
    }
    if PROCESS_SLOTS[1].pid != INIT_PID {
        return 46
    }
    if TASK_SLOTS[1].tid != INIT_TID {
        return 47
    }
    if USER_FRAME.task_id != INIT_TID {
        return 48
    }
    return PHASE109_MARKER
}