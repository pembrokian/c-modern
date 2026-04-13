const SLEEP_SLOT_COUNT: usize = 2
const SLEEP_NOT_FOUND: usize = 2

enum SleepState {
    Empty,
    Armed,
    Fired,
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

struct TimerInterruptDelivery {
    timer_state: TimerState
    observation: TimerWakeObservation
    delivered: u32
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

func empty_wake_observation(wake_count: u32) TimerWakeObservation {
    return TimerWakeObservation{ task_id: 0, deadline_tick: 0, wake_tick: 0, wake_count: wake_count }
}

func sleep_state_is_empty(state_value: SleepState) bool {
    return sleep_state_score(state_value) == 1
}

func sleep_state_is_armed(state_value: SleepState) bool {
    return sleep_state_score(state_value) == 2
}

func sleep_state_is_fired(state_value: SleepState) bool {
    return sleep_state_score(state_value) == 4
}

func sleep_state_score(state_value: SleepState) i32 {
    switch state_value {
    case SleepState.Empty:
        return 1
    case SleepState.Armed:
        return 2
    case SleepState.Fired:
        return 4
    default:
        return 0
    }
    return 0
}

func find_sleep_index(timer_state: TimerState, task_id: u32) usize {
    if !sleep_state_is_empty(timer_state.sleepers[0].state) && timer_state.sleepers[0].task_id == task_id {
        return 0
    }
    if !sleep_state_is_empty(timer_state.sleepers[1].state) && timer_state.sleepers[1].task_id == task_id {
        return 1
    }
    return SLEEP_NOT_FOUND
}

func has_fired_sleeper(timer_state: TimerState) u32 {
    if sleep_state_is_fired(timer_state.sleepers[0].state) {
        return 1
    }
    if sleep_state_is_fired(timer_state.sleepers[1].state) {
        return 1
    }
    return 0
}

func arm_sleep(timer_state: TimerState, task_id: u32, deadline_tick: u64) TimerState {
    if timer_state.count >= SLEEP_SLOT_COUNT {
        return timer_state
    }
    if find_sleep_index(timer_state, task_id) < SLEEP_SLOT_COUNT {
        return timer_state
    }
    sleepers: [2]SleepWait = timer_state.sleepers
    if sleep_state_is_empty(sleepers[0].state) {
        sleepers[0] = SleepWait{ task_id: task_id, deadline_tick: deadline_tick, wake_tick: 0, state: SleepState.Armed }
        return TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count + 1, sleepers: sleepers }
    }
    if sleep_state_is_empty(sleepers[1].state) {
        sleepers[1] = SleepWait{ task_id: task_id, deadline_tick: deadline_tick, wake_tick: 0, state: SleepState.Armed }
        return TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count + 1, sleepers: sleepers }
    }
    return timer_state
}

func advance_tick(timer_state: TimerState, now_tick: u64) TimerState {
    if now_tick < timer_state.monotonic_tick {
        return timer_state
    }
    sleepers: [2]SleepWait = timer_state.sleepers
    if sleep_state_is_armed(sleepers[0].state) && now_tick >= sleepers[0].deadline_tick {
        sleepers[0] = SleepWait{ task_id: sleepers[0].task_id, deadline_tick: sleepers[0].deadline_tick, wake_tick: now_tick, state: SleepState.Fired }
    }
    if sleep_state_is_armed(sleepers[1].state) && now_tick >= sleepers[1].deadline_tick {
        sleepers[1] = SleepWait{ task_id: sleepers[1].task_id, deadline_tick: sleepers[1].deadline_tick, wake_tick: now_tick, state: SleepState.Fired }
    }
    return TimerState{ monotonic_tick: now_tick, wake_count: timer_state.wake_count, count: timer_state.count, sleepers: sleepers }
}

// Observes one fired sleeper per call; callers must keep draining while
// has_fired_sleeper(timer_state) == 1.
func wake_fired_sleepers(timer_state: TimerState) TimerWakeResult {
    if sleep_state_is_fired(timer_state.sleepers[0].state) {
        return TimerWakeResult{ timer_state: TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count + 1, count: timer_state.count, sleepers: timer_state.sleepers }, observation: TimerWakeObservation{ task_id: timer_state.sleepers[0].task_id, deadline_tick: timer_state.sleepers[0].deadline_tick, wake_tick: timer_state.sleepers[0].wake_tick, wake_count: timer_state.wake_count + 1 } }
    }
    if sleep_state_is_fired(timer_state.sleepers[1].state) {
        return TimerWakeResult{ timer_state: TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count + 1, count: timer_state.count, sleepers: timer_state.sleepers }, observation: TimerWakeObservation{ task_id: timer_state.sleepers[1].task_id, deadline_tick: timer_state.sleepers[1].deadline_tick, wake_tick: timer_state.sleepers[1].wake_tick, wake_count: timer_state.wake_count + 1 } }
    }
    return TimerWakeResult{ timer_state: TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count, sleepers: timer_state.sleepers }, observation: TimerWakeObservation{ task_id: 0, deadline_tick: 0, wake_tick: 0, wake_count: timer_state.wake_count } }
}

func consume_wake(timer_state: TimerState, task_id: u32) TimerState {
    sleepers: [2]SleepWait = timer_state.sleepers
    if sleep_state_is_fired(sleepers[0].state) && sleepers[0].task_id == task_id {
        sleepers[0] = empty_sleep_wait()
        return TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count - 1, sleepers: sleepers }
    }
    if sleep_state_is_fired(sleepers[1].state) && sleepers[1].task_id == task_id {
        sleepers[1] = empty_sleep_wait()
        return TimerState{ monotonic_tick: timer_state.monotonic_tick, wake_count: timer_state.wake_count, count: timer_state.count - 1, sleepers: sleepers }
    }
    return timer_state
}

func deliver_interrupt_tick(timer_state: TimerState, now_tick: u64) TimerInterruptDelivery {
    advanced_timer: TimerState = advance_tick(timer_state, now_tick)
    wake_result: TimerWakeResult = wake_fired_sleepers(advanced_timer)
    if wake_result.observation.task_id == 0 {
        return TimerInterruptDelivery{ timer_state: wake_result.timer_state, observation: empty_wake_observation(wake_result.timer_state.wake_count), delivered: 0 }
    }
    return TimerInterruptDelivery{ timer_state: consume_wake(wake_result.timer_state, wake_result.observation.task_id), observation: wake_result.observation, delivered: 1 }
}

func validate_interrupt_delivery_boundary() bool {
    armed_timer: TimerState = arm_sleep(empty_timer_state(), 7, 1)
    first_delivery: TimerInterruptDelivery = deliver_interrupt_tick(armed_timer, 1)
    if first_delivery.delivered != 1 {
        return false
    }
    if first_delivery.observation.task_id != 7 {
        return false
    }
    if first_delivery.observation.deadline_tick != 1 {
        return false
    }
    if first_delivery.observation.wake_tick != 1 {
        return false
    }
    if first_delivery.observation.wake_count != 1 {
        return false
    }
    if first_delivery.timer_state.monotonic_tick != 1 {
        return false
    }
    if first_delivery.timer_state.wake_count != 1 {
        return false
    }
    if first_delivery.timer_state.count != 0 {
        return false
    }
    second_delivery: TimerInterruptDelivery = deliver_interrupt_tick(first_delivery.timer_state, 1)
    if second_delivery.delivered != 0 {
        return false
    }
    if second_delivery.observation.task_id != 0 {
        return false
    }
    if second_delivery.observation.wake_count != 1 {
        return false
    }
    return second_delivery.timer_state.count == 0
}