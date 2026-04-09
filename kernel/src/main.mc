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
const KERNEL_MAGIC: u32 = 9601
const BOOT_ENTRY_PC: usize = 4096
const BOOT_STACK_TOP: usize = 8192
const PHASE96_MARKER: i32 = 96

var KERNEL: state.KernelDescriptor
var PROCESS_SLOTS: [2]state.ProcessSlot
var TASK_SLOTS: [2]state.TaskSlot
var READY_QUEUE: state.ReadyQueue
var BOOT_LOG: state.BootLog
var BOOT_MARKER_EMITTED: u32
var ROOT_CAPABILITY: capability.CapabilitySlot
var ENDPOINTS: endpoint.EndpointTable
var INTERRUPTS: interrupt.InterruptController
var SYSCALL_GATE: syscall.SyscallGate
var INIT_IMAGE: init.InitImage

func reset_kernel_state() {
    KERNEL = state.empty_descriptor()
    PROCESS_SLOTS = state.zero_process_slots()
    TASK_SLOTS = state.zero_task_slots()
    READY_QUEUE = state.empty_queue()
    BOOT_LOG = state.empty_log()
    BOOT_MARKER_EMITTED = 0
    ROOT_CAPABILITY = capability.empty_slot()
    ENDPOINTS = endpoint.empty_table()
    INTERRUPTS = interrupt.reset_controller()
    SYSCALL_GATE = syscall.gate_closed()
    INIT_IMAGE = init.bootstrap_image()
}

func record_boot_stage(stage_value: state.BootStage, detail: u32) {
    BOOT_LOG = state.append_record(BOOT_LOG, stage_value, ARCH_ACTOR, detail)
}

func seed_kernel_descriptor() {
    KERNEL = state.boot_descriptor(KERNEL_MAGIC, BOOT_PID, BOOT_TID)
}

func seed_process_table() {
    slots: [2]state.ProcessSlot = state.zero_process_slots()
    slots[0] = state.boot_process_slot(BOOT_PID, BOOT_TASK_SLOT)
    PROCESS_SLOTS = slots
}

func seed_task_table() {
    slots: [2]state.TaskSlot = state.zero_task_slots()
    slots[0] = state.boot_task_slot(BOOT_TID, BOOT_PID, BOOT_ENTRY_PC, BOOT_STACK_TOP)
    TASK_SLOTS = slots
}

func seed_ready_queue() {
    READY_QUEUE = state.boot_ready_queue(BOOT_TID)
}

func seed_kernel_owners() {
    ROOT_CAPABILITY = capability.bootstrap_root_slot(BOOT_PID)
    ENDPOINTS = endpoint.empty_table()
    INTERRUPTS = interrupt.unmask_timer(INTERRUPTS, 32)
    SYSCALL_GATE = syscall.gate_closed()
    INIT_IMAGE = init.bootstrap_image()
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
    if state.process_state_score(PROCESS_SLOTS[0].state) != 2 {
        return false
    }
    if state.process_state_score(PROCESS_SLOTS[1].state) != 1 {
        return false
    }
    if TASK_SLOTS[0].tid != BOOT_TID {
        return false
    }
    if TASK_SLOTS[0].owner_pid != BOOT_PID {
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
    if SYSCALL_GATE.open != 0 {
        return false
    }
    if INIT_IMAGE.image_id != 1 {
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
    BOOT_MARKER_EMITTED = 1
    record_boot_stage(state.BootStage.MarkerEmitted, 96)
    return true
}

func halt_without_user_entry() bool {
    if KERNEL.user_entry_started != 0 {
        return false
    }
    if BOOT_MARKER_EMITTED != 1 {
        return false
    }
    record_boot_stage(state.BootStage.Halted, BOOT_PID)
    return true
}

func bootstrap_main() i32 {
    if !architecture_entry() {
        return 10
    }
    if KERNEL.booted != 1 {
        return 11
    }
    if BOOT_LOG.count != 3 {
        return 12
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 2)) != 4 {
        return 13
    }
    if state.log_actor_at(BOOT_LOG, 2) != ARCH_ACTOR {
        return 14
    }
    if state.log_detail_at(BOOT_LOG, 2) != 96 {
        return 15
    }
    if !halt_without_user_entry() {
        return 16
    }
    if BOOT_LOG.count != 4 {
        return 17
    }
    if state.boot_stage_score(state.log_stage_at(BOOT_LOG, 3)) != 8 {
        return 18
    }
    if state.log_actor_at(BOOT_LOG, 3) != ARCH_ACTOR {
        return 19
    }
    if state.log_detail_at(BOOT_LOG, 3) != BOOT_PID {
        return 20
    }
    if KERNEL.user_entry_started != 0 {
        return 21
    }
    if READY_QUEUE.count != 1 {
        return 22
    }
    if state.ready_slot_at(READY_QUEUE, 0) != BOOT_TID {
        return 23
    }
    if PROCESS_SLOTS[0].pid != BOOT_PID {
        return 24
    }
    if TASK_SLOTS[0].tid != BOOT_TID {
        return 25
    }
    return PHASE96_MARKER
}