enum SleepState {
    Empty,
    Armed,
    Fired,
    Delivered,
}

struct SleepWait {
    task_id: u32
    deadline_tick: u64
    wake_tick: u64
    state: SleepState
}

struct TimerState {
    monotonic_tick: u64
    wake_count: u32
    count: usize
    sleepers: [2]SleepWait
}

struct TimerWakeObservation {
    task_id: u32
    deadline_tick: u64
    wake_tick: u64
    wake_count: u32
}

struct TimerWakeResult {
    timer_state: TimerState
    observation: TimerWakeObservation
}

func empty_sleep_wait() SleepWait {
    return SleepWait{ task_id: 0, deadline_tick: 0, wake_tick: 0, state: SleepState.Empty }
}

func zero_sleepers() [2]SleepWait {
    sleepers: [2]SleepWait
    sleepers[0] = empty_sleep_wait()
    sleepers[1] = empty_sleep_wait()
    return sleepers
}

func empty_timer_state() TimerState {
    return TimerState{ monotonic_tick: 0, wake_count: 0, count: 0, sleepers: zero_sleepers() }
}

func sleep_state_score(state_value: SleepState) i32 {
    switch state_value {
    case SleepState.Empty:
        return 1
    case SleepState.Armed:
        return 2
    case SleepState.Fired:
        return 4
    case SleepState.Delivered:
        return 8
    default:
        return 0
    }
    return 0
}

func find_sleep_index(timer_state: TimerState, task_id: u32) usize {
    if sleep_state_score(timer_state.sleepers[0].state) != 1 && timer_state.sleepers[0].task_id == task_id {
        return 0
    }
    if sleep_state_score(timer_state.sleepers[1].state) != 1 && timer_state.sleepers[1].task_id == task_id {
        return 1
    }
    return 2
}

func arm_sleep(timer_state: TimerState, task_id: u32, deadline_tick: u64) TimerState {
    if timer_state.count >= 2 {
        return timer_state
    }
    if find_sleep_index(timer_state, task_id) < 2 {
        return timer_state
    }
    sleepers: [2]SleepWait = timer_state.sleepers
    if sleep_state_score(sleepers[0].state) == 1 {
        sleepers[0] = SleepWait{ task_id: task_id, deadline_tick: deadline_tick, wake_tick: 0, state: SleepState.Armed }
        return TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count + 1, sleepers: sleepers }
    }
    if sleep_state_score(sleepers[1].state) == 1 {
        sleepers[1] = SleepWait{ task_id: task_id, deadline_tick: deadline_tick, wake_tick: 0, state: SleepState.Armed }
        return TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count + 1, sleepers: sleepers }
    }
    return timer_state
}

func advance_tick(timer_state: TimerState, now_tick: u64) TimerState {
    sleepers: [2]SleepWait = timer_state.sleepers
    if sleep_state_score(sleepers[0].state) == 2 && now_tick >= sleepers[0].deadline_tick {
        sleepers[0] = SleepWait{ task_id: sleepers[0].task_id, deadline_tick: sleepers[0].deadline_tick, wake_tick: now_tick, state: SleepState.Fired }
    }
    if sleep_state_score(sleepers[1].state) == 2 && now_tick >= sleepers[1].deadline_tick {
        sleepers[1] = SleepWait{ task_id: sleepers[1].task_id, deadline_tick: sleepers[1].deadline_tick, wake_tick: now_tick, state: SleepState.Fired }
    }
    return TimerState{ monotonic_tick: now_tick, wake_count: timer_state.wake_count, count: timer_state.count, sleepers: sleepers }
}

func wake_fired_sleepers(timer_state: TimerState) TimerWakeResult {
    if sleep_state_score(timer_state.sleepers[0].state) == 4 {
        return TimerWakeResult{ timer_state: TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count + 1, count: timer_state.count, sleepers: timer_state.sleepers }, observation: TimerWakeObservation{ task_id: timer_state.sleepers[0].task_id, deadline_tick: timer_state.sleepers[0].deadline_tick, wake_tick: timer_state.sleepers[0].wake_tick, wake_count: timer_state.wake_count + 1 } }
    }
    if sleep_state_score(timer_state.sleepers[1].state) == 4 {
        return TimerWakeResult{ timer_state: TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count + 1, count: timer_state.count, sleepers: timer_state.sleepers }, observation: TimerWakeObservation{ task_id: timer_state.sleepers[1].task_id, deadline_tick: timer_state.sleepers[1].deadline_tick, wake_tick: timer_state.sleepers[1].wake_tick, wake_count: timer_state.wake_count + 1 } }
    }
    return TimerWakeResult{ timer_state: TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count, sleepers: timer_state.sleepers }, observation: TimerWakeObservation{ task_id: 0, deadline_tick: 0, wake_tick: 0, wake_count: timer_state.wake_count } }
}

func consume_wake(timer_state: TimerState, task_id: u32) TimerState {
    sleepers: [2]SleepWait = timer_state.sleepers
    if sleep_state_score(sleepers[0].state) != 1 && sleepers[0].task_id == task_id {
        sleepers[0] = empty_sleep_wait()
        return TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count - 1, sleepers: sleepers }
    }
    if sleep_state_score(sleepers[1].state) != 1 && sleepers[1].task_id == task_id {
        sleepers[1] = empty_sleep_wait()
        return TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count - 1, sleepers: sleepers }
    }
    return timer_state
}