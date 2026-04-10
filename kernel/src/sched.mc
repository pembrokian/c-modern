import address_space
import capability
import state
import syscall
import timer

struct LifecycleAudit {
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    child_root_page_table: usize
    child_exit_code: i32
    child_wait_handle_slot: u32
    init_entry_pc: usize
    init_stack_top: usize
    spawn: syscall.SpawnObservation
    pre_exit_wait: syscall.WaitObservation
    exit_wait: syscall.WaitObservation
    sleep: syscall.SleepObservation
    timer_wake: timer.TimerWakeObservation
    timer_state: timer.TimerState
    wake_ready_queue: state.ReadyQueue
    wait_table: capability.WaitTable
    child_process: state.ProcessSlot
    child_task: state.TaskSlot
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    ready_queue: state.ReadyQueue
}

func validate_program_cap_spawn_and_wait(audit: LifecycleAudit) bool {
    if syscall.status_score(audit.spawn.status) != 2 {
        return false
    }
    if audit.spawn.child_pid != audit.child_pid {
        return false
    }
    if audit.spawn.child_tid != audit.child_tid {
        return false
    }
    if audit.spawn.child_asid != audit.child_asid {
        return false
    }
    if audit.spawn.wait_handle_slot != audit.child_wait_handle_slot {
        return false
    }
    if syscall.status_score(audit.pre_exit_wait.status) != 4 {
        return false
    }
    if syscall.block_reason_score(audit.pre_exit_wait.block_reason) != 8 {
        return false
    }
    if audit.pre_exit_wait.child_pid != audit.child_pid {
        return false
    }
    if audit.pre_exit_wait.exit_code != 0 {
        return false
    }
    if audit.pre_exit_wait.wait_handle_slot != audit.child_wait_handle_slot {
        return false
    }
    if syscall.status_score(audit.exit_wait.status) != 2 {
        return false
    }
    if audit.exit_wait.child_pid != audit.child_pid {
        return false
    }
    if audit.exit_wait.exit_code != audit.child_exit_code {
        return false
    }
    if audit.exit_wait.wait_handle_slot != audit.child_wait_handle_slot {
        return false
    }
    if syscall.block_reason_score(audit.exit_wait.block_reason) != 1 {
        return false
    }
    if syscall.status_score(audit.sleep.status) != 4 {
        return false
    }
    if audit.sleep.task_id != audit.child_tid {
        return false
    }
    if audit.sleep.deadline_tick != 1 {
        return false
    }
    if syscall.block_reason_score(audit.sleep.block_reason) != 16 {
        return false
    }
    if audit.sleep.wake_tick != 0 {
        return false
    }
    if audit.timer_wake.task_id != audit.child_tid {
        return false
    }
    if audit.timer_wake.deadline_tick != 1 {
        return false
    }
    if audit.timer_wake.wake_tick != 1 {
        return false
    }
    if audit.timer_wake.wake_count != 1 {
        return false
    }
    if audit.timer_state.monotonic_tick != 1 {
        return false
    }
    if audit.timer_state.wake_count != 1 {
        return false
    }
    if audit.timer_state.count != 0 {
        return false
    }
    if audit.wake_ready_queue.count != 1 {
        return false
    }
    if state.ready_slot_at(audit.wake_ready_queue, 0) != audit.child_tid {
        return false
    }
    if audit.wait_table.owner_pid != audit.init_pid {
        return false
    }
    if audit.wait_table.count != 0 {
        return false
    }
    if capability.find_child_for_wait_handle(audit.wait_table, audit.child_wait_handle_slot) != 0 {
        return false
    }
    if state.process_state_score(audit.child_process.state) != 1 {
        return false
    }
    if state.task_state_score(audit.child_task.state) != 1 {
        return false
    }
    if address_space.state_score(audit.child_address_space.state) != 2 {
        return false
    }
    if audit.child_address_space.asid != audit.child_asid {
        return false
    }
    if audit.child_address_space.owner_pid != audit.child_pid {
        return false
    }
    if audit.child_address_space.root_page_table != audit.child_root_page_table {
        return false
    }
    if audit.child_address_space.entry_pc != audit.init_entry_pc {
        return false
    }
    if audit.child_address_space.stack_top != audit.init_stack_top {
        return false
    }
    if audit.child_address_space.mapping_count != 2 {
        return false
    }
    if audit.child_user_frame.task_id != audit.child_tid {
        return false
    }
    if audit.child_user_frame.address_space_id != audit.child_asid {
        return false
    }
    if audit.ready_queue.count != 0 {
        return false
    }
    return true
}