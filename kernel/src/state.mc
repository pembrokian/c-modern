enum ProcessState {
    Empty,
    BootOwned,
    Ready,
    Exited,
}

enum TaskState {
    Empty,
    Boot,
    Ready,
    Exited,
}

enum BootStage {
    Reset,
    TablesSeeded,
    AddressSpaceReady,
    UserEntryReady,
    MarkerEmitted,
    Halted,
}

struct KernelDescriptor {
    magic: u32
    current_pid: u32
    current_tid: u32
    active_asid: u32
    booted: u32
    user_entry_started: u32
}

struct ProcessSlot {
    pid: u32
    task_slot: u32
    address_space_id: u32
    state: ProcessState
}

struct TaskSlot {
    tid: u32
    owner_pid: u32
    address_space_id: u32
    state: TaskState
    entry_pc: usize
    stack_top: usize
}

struct ReadyQueue {
    count: usize
    slots: [3]u32
}

struct BootRecord {
    stage: BootStage
    actor: u32
    detail: u32
}

struct BootLog {
    count: usize
    entries: [6]BootRecord
}

func empty_descriptor() KernelDescriptor {
    return KernelDescriptor{ magic: 0, current_pid: 0, current_tid: 0, active_asid: 0, booted: 0, user_entry_started: 0 }
}

func boot_descriptor(magic: u32, pid: u32, tid: u32) KernelDescriptor {
    return KernelDescriptor{ magic: magic, current_pid: pid, current_tid: tid, active_asid: 0, booted: 0, user_entry_started: 0 }
}

func mark_booted(desc: KernelDescriptor) KernelDescriptor {
    return KernelDescriptor{ magic: desc.magic, current_pid: desc.current_pid, current_tid: desc.current_tid, active_asid: desc.active_asid, booted: 1, user_entry_started: desc.user_entry_started }
}

func start_user_entry(desc: KernelDescriptor, pid: u32, tid: u32, asid: u32) KernelDescriptor {
    return KernelDescriptor{ magic: desc.magic, current_pid: pid, current_tid: tid, active_asid: asid, booted: desc.booted, user_entry_started: 1 }
}

func empty_process_slot() ProcessSlot {
    return ProcessSlot{ pid: 0, task_slot: 0, address_space_id: 0, state: ProcessState.Empty }
}

func boot_process_slot(pid: u32, task_slot: u32) ProcessSlot {
    return ProcessSlot{ pid: pid, task_slot: task_slot, address_space_id: 0, state: ProcessState.BootOwned }
}

func init_process_slot(pid: u32, task_slot: u32, address_space_id: u32) ProcessSlot {
    return ProcessSlot{ pid: pid, task_slot: task_slot, address_space_id: address_space_id, state: ProcessState.Ready }
}

func empty_task_slot() TaskSlot {
    return TaskSlot{ tid: 0, owner_pid: 0, address_space_id: 0, state: TaskState.Empty, entry_pc: 0, stack_top: 0 }
}

func boot_task_slot(tid: u32, owner_pid: u32, entry_pc: usize, stack_top: usize) TaskSlot {
    return TaskSlot{ tid: tid, owner_pid: owner_pid, address_space_id: 0, state: TaskState.Boot, entry_pc: entry_pc, stack_top: stack_top }
}

func user_task_slot(tid: u32, owner_pid: u32, address_space_id: u32, entry_pc: usize, stack_top: usize) TaskSlot {
    return TaskSlot{ tid: tid, owner_pid: owner_pid, address_space_id: address_space_id, state: TaskState.Ready, entry_pc: entry_pc, stack_top: stack_top }
}

func zero_ready_slots() [3]u32 {
    slots: [3]u32
    slots[0] = 0
    slots[1] = 0
    slots[2] = 0
    return slots
}

func zero_process_slots() [3]ProcessSlot {
    slots: [3]ProcessSlot
    slots[0] = empty_process_slot()
    slots[1] = empty_process_slot()
    slots[2] = empty_process_slot()
    return slots
}

func zero_task_slots() [3]TaskSlot {
    slots: [3]TaskSlot
    slots[0] = empty_task_slot()
    slots[1] = empty_task_slot()
    slots[2] = empty_task_slot()
    return slots
}

func empty_queue() ReadyQueue {
    return ReadyQueue{ count: 0, slots: zero_ready_slots() }
}

func single_ready_queue(tid: u32) ReadyQueue {
    slots: [3]u32 = zero_ready_slots()
    slots[0] = tid
    return ReadyQueue{ count: 1, slots: slots }
}

func boot_ready_queue(tid: u32) ReadyQueue {
    return single_ready_queue(tid)
}

func user_ready_queue(tid: u32) ReadyQueue {
    return single_ready_queue(tid)
}

func empty_record() BootRecord {
    return BootRecord{ stage: BootStage.Reset, actor: 0, detail: 0 }
}

func zero_boot_records() [6]BootRecord {
    entries: [6]BootRecord
    entries[0] = empty_record()
    entries[1] = empty_record()
    entries[2] = empty_record()
    entries[3] = empty_record()
    entries[4] = empty_record()
    entries[5] = empty_record()
    return entries
}

func empty_log() BootLog {
    return BootLog{ count: 0, entries: zero_boot_records() }
}

func append_record(log: BootLog, stage: BootStage, actor: u32, detail: u32) BootLog {
    if log.count >= 6 {
        return log
    }
    entries: [6]BootRecord = log.entries
    entries[log.count] = BootRecord{ stage: stage, actor: actor, detail: detail }
    return BootLog{ count: log.count + 1, entries: entries }
}

func ready_slot_at(queue: ReadyQueue, index: usize) u32 {
    if index >= queue.count {
        return 0
    }
    return queue.slots[index]
}

func log_stage_at(log: BootLog, index: usize) BootStage {
    if index >= log.count {
        return BootStage.Reset
    }
    return log.entries[index].stage
}

func exit_process_slot(slot: ProcessSlot) ProcessSlot {
    return ProcessSlot{ pid: slot.pid, task_slot: slot.task_slot, address_space_id: slot.address_space_id, state: ProcessState.Exited }
}

func exit_task_slot(slot: TaskSlot) TaskSlot {
    return TaskSlot{ tid: slot.tid, owner_pid: slot.owner_pid, address_space_id: slot.address_space_id, state: TaskState.Exited, entry_pc: slot.entry_pc, stack_top: slot.stack_top }
}

func log_actor_at(log: BootLog, index: usize) u32 {
    if index >= log.count {
        return 0
    }
    return log.entries[index].actor
}

func log_detail_at(log: BootLog, index: usize) u32 {
    if index >= log.count {
        return 0
    }
    return log.entries[index].detail
}

func process_state_score(state: ProcessState) i32 {
    switch state {
    case ProcessState.Empty:
        return 1
    case ProcessState.BootOwned:
        return 2
    case ProcessState.Ready:
        return 4
    case ProcessState.Exited:
        return 8
    default:
        return 0
    }
    return 0
}

func task_state_score(state: TaskState) i32 {
    switch state {
    case TaskState.Empty:
        return 1
    case TaskState.Boot:
        return 2
    case TaskState.Ready:
        return 4
    case TaskState.Exited:
        return 8
    default:
        return 0
    }
    return 0
}

func boot_stage_score(stage: BootStage) i32 {
    switch stage {
    case BootStage.Reset:
        return 1
    case BootStage.TablesSeeded:
        return 2
    case BootStage.AddressSpaceReady:
        return 4
    case BootStage.UserEntryReady:
        return 8
    case BootStage.MarkerEmitted:
        return 16
    case BootStage.Halted:
        return 32
    default:
        return 0
    }
    return 0
}