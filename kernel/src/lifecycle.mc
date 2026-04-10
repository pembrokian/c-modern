import state

struct SpawnInstallResult {
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
}

struct TaskTransition {
    task_slots: [3]state.TaskSlot
    task_id: u32
    applied: u32
}

struct ReleaseTransition {
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
}

func install_spawned_child(process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, child_pid: u32, child_tid: u32, child_asid: u32, process_slot: u32, task_slot: u32, entry_pc: usize, stack_top: usize) SpawnInstallResult {
    updated_process_slots: [3]state.ProcessSlot = state.with_updated_process_slot(process_slots, process_slot, state.init_process_slot(child_pid, task_slot, child_asid))
    updated_task_slots: [3]state.TaskSlot = state.with_updated_task_slot(task_slots, task_slot, state.user_task_slot(child_tid, child_pid, child_asid, entry_pc, stack_top))
    return SpawnInstallResult{ process_slots: updated_process_slots, task_slots: updated_task_slots }
}

func block_task_on_timer(task_slots: [3]state.TaskSlot, task_slot: u32) TaskTransition {
    selected_task: state.TaskSlot = state.task_slot_at(task_slots, task_slot)
    if !state.can_block_task(selected_task) {
        return TaskTransition{ task_slots: task_slots, task_id: 0, applied: 0 }
    }
    updated_task_slots: [3]state.TaskSlot = state.with_updated_task_slot(task_slots, task_slot, state.blocked_task_slot(selected_task))
    return TaskTransition{ task_slots: updated_task_slots, task_id: selected_task.tid, applied: 1 }
}

func ready_task(task_slots: [3]state.TaskSlot, task_slot: u32) TaskTransition {
    selected_task: state.TaskSlot = state.task_slot_at(task_slots, task_slot)
    if state.task_state_score(selected_task.state) == 1 {
        return TaskTransition{ task_slots: task_slots, task_id: 0, applied: 0 }
    }
    updated_task: state.TaskSlot = state.user_task_slot(selected_task.tid, selected_task.owner_pid, selected_task.address_space_id, selected_task.entry_pc, selected_task.stack_top)
    updated_task_slots: [3]state.TaskSlot = state.with_updated_task_slot(task_slots, task_slot, updated_task)
    return TaskTransition{ task_slots: updated_task_slots, task_id: selected_task.tid, applied: 1 }
}

func exit_process(process_slots: [3]state.ProcessSlot, process_slot: u32) [3]state.ProcessSlot {
    selected_process: state.ProcessSlot = state.process_slot_at(process_slots, process_slot)
    return state.with_updated_process_slot(process_slots, process_slot, state.exit_process_slot(selected_process))
}

func exit_task(task_slots: [3]state.TaskSlot, task_slot: u32) [3]state.TaskSlot {
    selected_task: state.TaskSlot = state.task_slot_at(task_slots, task_slot)
    return state.with_updated_task_slot(task_slots, task_slot, state.exit_task_slot(selected_task))
}

func release_waited_child_slots(process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, child_pid: u32) ReleaseTransition {
    updated_process_slots: [3]state.ProcessSlot = process_slots
    updated_task_slots: [3]state.TaskSlot = task_slots
    process_slot: u32 = state.find_process_slot_by_pid(process_slots, child_pid)
    task_slot: u32 = state.find_task_slot_by_owner_pid(task_slots, child_pid)
    if process_slot < 3 {
        updated_process_slots = state.with_updated_process_slot(updated_process_slots, process_slot, state.empty_process_slot())
    }
    if task_slot < 3 {
        updated_task_slots = state.with_updated_task_slot(updated_task_slots, task_slot, state.empty_task_slot())
    }
    return ReleaseTransition{ process_slots: updated_process_slots, task_slots: updated_task_slots }
}

func validate_task_transition_contracts() bool {
    processes: [3]state.ProcessSlot = state.zero_process_slots()
    tasks: [3]state.TaskSlot = state.zero_task_slots()
    boot_task: state.TaskSlot = state.boot_task_slot(1, 1, 4096, 8192)
    ready_seed: state.TaskSlot = state.user_task_slot(3, 2, 5, 12288, 16384)
    processes[1] = state.init_process_slot(2, 1, 5)
    tasks[0] = boot_task
    tasks[1] = ready_seed

    blocked_boot: TaskTransition = block_task_on_timer(tasks, 0)
    if blocked_boot.applied != 0 {
        return false
    }

    blocked_ready: TaskTransition = block_task_on_timer(tasks, 1)
    if blocked_ready.applied == 0 {
        return false
    }
    if blocked_ready.task_id != 3 {
        return false
    }
    if state.task_state_score(state.task_slot_at(blocked_ready.task_slots, 1).state) != 8 {
        return false
    }

    resumed_ready: TaskTransition = ready_task(blocked_ready.task_slots, 1)
    if resumed_ready.applied == 0 {
        return false
    }
    if resumed_ready.task_id != 3 {
        return false
    }
    resumed_task: state.TaskSlot = state.task_slot_at(resumed_ready.task_slots, 1)
    if state.task_state_score(resumed_task.state) != 4 {
        return false
    }
    if resumed_task.entry_pc != ready_seed.entry_pc {
        return false
    }
    if resumed_task.stack_top != ready_seed.stack_top {
        return false
    }

    exited_processes: [3]state.ProcessSlot = exit_process(processes, 1)
    if state.process_state_score(state.process_slot_at(exited_processes, 1).state) != 8 {
        return false
    }

    exited_tasks: [3]state.TaskSlot = exit_task(resumed_ready.task_slots, 1)
    if state.task_state_score(state.task_slot_at(exited_tasks, 1).state) != 16 {
        return false
    }

    released: ReleaseTransition = release_waited_child_slots(exited_processes, exited_tasks, 2)
    if state.process_state_score(state.process_slot_at(released.process_slots, 1).state) != 1 {
        return false
    }
    if state.task_state_score(state.task_slot_at(released.task_slots, 1).state) != 1 {
        return false
    }

    spawned_tasks: [3]state.TaskSlot = state.zero_task_slots()
    spawned_processes: [3]state.ProcessSlot = state.zero_process_slots()
    install: SpawnInstallResult = install_spawned_child(spawned_processes, spawned_tasks, 7, 8, 9, 2, 2, 20480, 24576)
    if state.process_slot_at(install.process_slots, 2).pid != 7 {
        return false
    }
    installed_task: state.TaskSlot = state.task_slot_at(install.task_slots, 2)
    if installed_task.tid != 8 {
        return false
    }
    if state.task_state_score(installed_task.state) != 4 {
        return false
    }
    if installed_task.address_space_id != 9 {
        return false
    }
    return true
}