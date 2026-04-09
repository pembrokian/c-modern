import address_space
import capability
import endpoint
import init
import interrupt
import state
import syscall

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
const PHASE101_MARKER: i32 = 101

var KERNEL: state.KernelDescriptor
var PROCESS_SLOTS: [3]state.ProcessSlot
var TASK_SLOTS: [3]state.TaskSlot
var READY_QUEUE: state.ReadyQueue
var BOOT_LOG: state.BootLog
var BOOT_MARKER_EMITTED: u32
var ROOT_CAPABILITY: capability.CapabilitySlot
var INIT_PROGRAM_CAPABILITY: capability.CapabilitySlot
var HANDLE_TABLES: [3]capability.HandleTable
var WAIT_TABLES: [3]capability.WaitTable
var ENDPOINTS: endpoint.EndpointTable
var INTERRUPTS: interrupt.InterruptController
var SYSCALL_GATE: syscall.SyscallGate
var INIT_IMAGE: init.InitImage
var ADDRESS_SPACE: address_space.AddressSpace
var USER_FRAME: address_space.UserEntryFrame
var CHILD_ADDRESS_SPACE: address_space.AddressSpace
var CHILD_USER_FRAME: address_space.UserEntryFrame
var DELIVERED_MESSAGE: endpoint.KernelMessage
var RECEIVE_OBSERVATION: syscall.ReceiveObservation
var ATTACHED_RECEIVE_OBSERVATION: syscall.ReceiveObservation
var TRANSFERRED_HANDLE_USE_OBSERVATION: syscall.ReceiveObservation
var SPAWN_OBSERVATION: syscall.SpawnObservation
var PRE_EXIT_WAIT_OBSERVATION: syscall.WaitObservation
var EXIT_WAIT_OBSERVATION: syscall.WaitObservation

func reset_kernel_state() {
    KERNEL = state.empty_descriptor()
    PROCESS_SLOTS = state.zero_process_slots()
    TASK_SLOTS = state.zero_task_slots()
    READY_QUEUE = state.empty_queue()
    BOOT_LOG = state.empty_log()
    BOOT_MARKER_EMITTED = 0
    ROOT_CAPABILITY = capability.empty_slot()
    INIT_PROGRAM_CAPABILITY = capability.empty_slot()
    HANDLE_TABLES = capability.zero_handle_tables()
    WAIT_TABLES = capability.zero_wait_tables()
    ENDPOINTS = endpoint.empty_table()
    INTERRUPTS = interrupt.reset_controller()
    SYSCALL_GATE = syscall.gate_closed()
    INIT_IMAGE = init.bootstrap_image()
    ADDRESS_SPACE = address_space.empty_space()
    USER_FRAME = address_space.empty_frame()
    CHILD_ADDRESS_SPACE = address_space.empty_space()
    CHILD_USER_FRAME = address_space.empty_frame()
    DELIVERED_MESSAGE = endpoint.empty_message()
    RECEIVE_OBSERVATION = syscall.empty_receive_observation()
    ATTACHED_RECEIVE_OBSERVATION = syscall.empty_receive_observation()
    TRANSFERRED_HANDLE_USE_OBSERVATION = syscall.empty_receive_observation()
    SPAWN_OBSERVATION = syscall.empty_spawn_observation()
    PRE_EXIT_WAIT_OBSERVATION = syscall.empty_wait_observation()
    EXIT_WAIT_OBSERVATION = syscall.empty_wait_observation()
}

func record_boot_stage(stage_value: state.BootStage, detail: u32) {
    BOOT_LOG = state.append_record(BOOT_LOG, stage_value, ARCH_ACTOR, detail)
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
    INIT_IMAGE = init.bootstrap_image()
    ADDRESS_SPACE = address_space.empty_space()
    USER_FRAME = address_space.empty_frame()
    CHILD_ADDRESS_SPACE = address_space.empty_space()
    CHILD_USER_FRAME = address_space.empty_frame()
    DELIVERED_MESSAGE = endpoint.empty_message()
    RECEIVE_OBSERVATION = syscall.empty_receive_observation()
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

func architecture_entry() bool {
    reset_kernel_state()
    record_boot_stage(state.BootStage.Reset, KERNEL_MAGIC)
    seed_kernel_descriptor()
    seed_process_table()
    seed_task_table()
    seed_ready_queue()
    seed_kernel_owners()
    record_boot_stage(state.BootStage.TablesSeeded, BOOT_TID)
    if !validate_seeded_tables() {
        return false
    }
    KERNEL = state.mark_booted(KERNEL)
    return true
}

func construct_first_user_address_space() {
    slots: [3]state.ProcessSlot = PROCESS_SLOTS
    tasks: [3]state.TaskSlot = TASK_SLOTS
    ADDRESS_SPACE = address_space.bootstrap_space(INIT_ASID, INIT_PID, INIT_ROOT_PAGE_TABLE, INIT_IMAGE.image_base, INIT_IMAGE.image_size, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_base, INIT_IMAGE.stack_size, INIT_IMAGE.stack_top)
    INIT_PROGRAM_CAPABILITY = capability.bootstrap_init_program_slot(INIT_PID)
    slots[1] = state.init_process_slot(INIT_PID, INIT_TASK_SLOT, INIT_ASID)
    tasks[1] = state.user_task_slot(INIT_TID, INIT_PID, INIT_ASID, INIT_IMAGE.entry_pc, INIT_IMAGE.stack_top)
    PROCESS_SLOTS = slots
    TASK_SLOTS = tasks
    READY_QUEUE = state.user_ready_queue(INIT_TID)
    USER_FRAME = address_space.bootstrap_user_frame(ADDRESS_SPACE, INIT_TID)
    record_boot_stage(state.BootStage.AddressSpaceReady, INIT_ASID)
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
    if capability.kind_score(INIT_PROGRAM_CAPABILITY.kind) != 4 {
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
    if address_space.kind_score(address_space.mapping_kind_at(ADDRESS_SPACE, 0)) != 2 {
        return false
    }
    if address_space.kind_score(address_space.mapping_kind_at(ADDRESS_SPACE, 1)) != 4 {
        return false
    }
    if ADDRESS_SPACE.mappings[0].base != INIT_IMAGE.image_base {
        return false
    }
    if ADDRESS_SPACE.mappings[0].size != INIT_IMAGE.image_size {
        return false
    }
    if ADDRESS_SPACE.mappings[0].writable != 0 {
        return false
    }
    if ADDRESS_SPACE.mappings[0].executable != 1 {
        return false
    }
    if ADDRESS_SPACE.mappings[1].base != INIT_IMAGE.stack_base {
        return false
    }
    if ADDRESS_SPACE.mappings[1].size != INIT_IMAGE.stack_size {
        return false
    }
    if ADDRESS_SPACE.mappings[1].writable != 1 {
        return false
    }
    if ADDRESS_SPACE.mappings[1].executable != 0 {
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
    construct_first_user_address_space()
    if !validate_first_user_entry_ready() {
        return false
    }
    ADDRESS_SPACE = address_space.activate(ADDRESS_SPACE)
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
    endpoints = endpoint.enqueue_message(endpoints, 0, endpoint.bootstrap_init_message(BOOT_PID, INIT_ENDPOINT_ID, INIT_ENDPOINT_HANDLE_SLOT))
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
    if DELIVERED_MESSAGE.handle_slot != INIT_ENDPOINT_HANDLE_SLOT {
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
    return syscall.status_score(would_block_result.observation.status) == 4
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

func execute_program_cap_spawn_and_wait() bool {
    WAIT_TABLES[1] = capability.wait_table_for_owner(INIT_PID)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(CHILD_WAIT_HANDLE_SLOT)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(SYSCALL_GATE, INIT_PROGRAM_CAPABILITY, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], INIT_IMAGE, spawn_request, CHILD_PID, CHILD_TID, CHILD_TASK_SLOT, CHILD_ASID, CHILD_ROOT_PAGE_TABLE)
    SYSCALL_GATE = spawn_result.gate
    INIT_PROGRAM_CAPABILITY = spawn_result.program_capability
    PROCESS_SLOTS = spawn_result.process_slots
    TASK_SLOTS = spawn_result.task_slots
    WAIT_TABLES[1] = spawn_result.wait_table
    CHILD_ADDRESS_SPACE = spawn_result.child_address_space
    CHILD_USER_FRAME = spawn_result.child_frame
    SPAWN_OBSERVATION = spawn_result.observation
    READY_QUEUE = state.user_ready_queue(CHILD_TID)
    if syscall.status_score(SPAWN_OBSERVATION.status) != 2 {
        return false
    }

    pre_wait_result: syscall.WaitResult = syscall.perform_wait(SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], syscall.build_wait_request(CHILD_WAIT_HANDLE_SLOT))
    SYSCALL_GATE = pre_wait_result.gate
    PROCESS_SLOTS = pre_wait_result.process_slots
    TASK_SLOTS = pre_wait_result.task_slots
    WAIT_TABLES[1] = pre_wait_result.wait_table
    PRE_EXIT_WAIT_OBSERVATION = pre_wait_result.observation
    if syscall.status_score(PRE_EXIT_WAIT_OBSERVATION.status) != 4 {
        return false
    }

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

    wait_result: syscall.WaitResult = syscall.perform_wait(SYSCALL_GATE, PROCESS_SLOTS, TASK_SLOTS, WAIT_TABLES[1], syscall.build_wait_request(CHILD_WAIT_HANDLE_SLOT))
    SYSCALL_GATE = wait_result.gate
    PROCESS_SLOTS = wait_result.process_slots
    TASK_SLOTS = wait_result.task_slots
    WAIT_TABLES[1] = wait_result.wait_table
    EXIT_WAIT_OBSERVATION = wait_result.observation
    return syscall.status_score(EXIT_WAIT_OBSERVATION.status) == 2
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
    if !execute_syscall_byte_ipc() {
        return 19
    }
    if !validate_syscall_byte_ipc() {
        return 20
    }
    if !seed_transfer_endpoint_handle() {
        return 21
    }
    if !execute_capability_carrying_ipc_transfer() {
        return 22
    }
    if !validate_capability_carrying_ipc_transfer() {
        return 23
    }
    if !execute_program_cap_spawn_and_wait() {
        return 24
    }
    if !validate_program_cap_spawn_and_wait() {
        return 25
    }
    BOOT_MARKER_EMITTED = 1
    record_boot_stage(state.BootStage.MarkerEmitted, 101)
    if BOOT_MARKER_EMITTED != 1 {
        return 26
    }
    if BOOT_LOG.count != 5 {
        return 27
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 3)) != 8 {
        return 28
    }
    if state.log_actor_at(BOOT_LOG, 3) != ARCH_ACTOR {
        return 29
    }
    if state.log_detail_at(BOOT_LOG, 3) != INIT_TID {
        return 30
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 4)) != 16 {
        return 31
    }
    if state.log_actor_at(BOOT_LOG, 4) != ARCH_ACTOR {
        return 32
    }
    if state.log_detail_at(BOOT_LOG, 4) != 101 {
        return 33
    }
    if PROCESS_SLOTS[1].pid != INIT_PID {
        return 34
    }
    if TASK_SLOTS[1].tid != INIT_TID {
        return 35
    }
    if USER_FRAME.task_id != INIT_TID {
        return 36
    }
    return PHASE101_MARKER
}