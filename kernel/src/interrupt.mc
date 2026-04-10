import timer

enum InterruptState {
    Masked,
    Quiescent,
}

enum InterruptDispatchKind {
    None,
    TimerWake,
}

struct InterruptController {
    timer_vector: u32
    state: InterruptState
    last_vector: u32
    last_source_actor: u32
    entry_count: u32
    dispatch_count: u32
}

struct InterruptFrame {
    vector: u32
    source_actor: u32
}

struct InterruptEntry {
    controller: InterruptController
    frame: InterruptFrame
    accepted: u32
}

struct InterruptDispatchResult {
    controller: InterruptController
    timer_state: timer.TimerState
    wake_observation: timer.TimerWakeObservation
    kind: InterruptDispatchKind
    handled: u32
}

func reset_controller() InterruptController {
    return InterruptController{ timer_vector: 0, state: InterruptState.Masked, last_vector: 0, last_source_actor: 0, entry_count: 0, dispatch_count: 0 }
}

func unmask_timer(controller: InterruptController, vector: u32) InterruptController {
    return InterruptController{ timer_vector: vector, state: InterruptState.Quiescent, last_vector: controller.last_vector, last_source_actor: controller.last_source_actor, entry_count: controller.entry_count, dispatch_count: controller.dispatch_count }
}

func dispatch_kind_score(kind: InterruptDispatchKind) i32 {
    switch kind {
    case InterruptDispatchKind.None:
        return 1
    case InterruptDispatchKind.TimerWake:
        return 2
    default:
        return 0
    }
    return 0
}

func arch_enter_interrupt(controller: InterruptController, vector: u32, source_actor: u32) InterruptEntry {
    accepted: u32 = 0
    if state_score(controller.state) == 2 && vector == controller.timer_vector {
        accepted = 1
    }
    next_controller: InterruptController = InterruptController{ timer_vector: controller.timer_vector, state: controller.state, last_vector: vector, last_source_actor: source_actor, entry_count: controller.entry_count + 1, dispatch_count: controller.dispatch_count }
    return InterruptEntry{ controller: next_controller, frame: InterruptFrame{ vector: vector, source_actor: source_actor }, accepted: accepted }
}

func dispatch_interrupt(entry: InterruptEntry, timer_state: timer.TimerState, now_tick: u64) InterruptDispatchResult {
    if entry.accepted == 0 {
        return InterruptDispatchResult{ controller: entry.controller, timer_state: timer_state, wake_observation: timer.TimerWakeObservation{ task_id: 0, deadline_tick: 0, wake_tick: 0, wake_count: timer_state.wake_count }, kind: InterruptDispatchKind.None, handled: 0 }
    }
    advanced_timer: timer.TimerState = timer.advance_tick(timer_state, now_tick)
    wake_result: timer.TimerWakeResult = timer.wake_fired_sleepers(advanced_timer)
    kind: InterruptDispatchKind = InterruptDispatchKind.None
    handled: u32 = 0
    if wake_result.observation.task_id != 0 {
        kind = InterruptDispatchKind.TimerWake
        handled = 1
    }
    next_controller: InterruptController = InterruptController{ timer_vector: entry.controller.timer_vector, state: entry.controller.state, last_vector: entry.frame.vector, last_source_actor: entry.frame.source_actor, entry_count: entry.controller.entry_count, dispatch_count: entry.controller.dispatch_count + 1 }
    return InterruptDispatchResult{ controller: next_controller, timer_state: wake_result.timer_state, wake_observation: wake_result.observation, kind: kind, handled: handled }
}

func validate_interrupt_entry_and_dispatch_boundary() bool {
    controller: InterruptController = unmask_timer(reset_controller(), 32)
    armed_timer: timer.TimerState = timer.arm_sleep(timer.empty_timer_state(), 7, 1)
    entry: InterruptEntry = arch_enter_interrupt(controller, 32, 99)
    if entry.accepted != 1 {
        return false
    }
    if entry.controller.last_vector != 32 {
        return false
    }
    if entry.controller.last_source_actor != 99 {
        return false
    }
    if entry.controller.entry_count != 1 {
        return false
    }
    dispatch: InterruptDispatchResult = dispatch_interrupt(entry, armed_timer, 1)
    if dispatch_kind_score(dispatch.kind) != 2 {
        return false
    }
    if dispatch.handled != 1 {
        return false
    }
    if dispatch.controller.dispatch_count != 1 {
        return false
    }
    if dispatch.wake_observation.task_id != 7 {
        return false
    }
    if dispatch.wake_observation.wake_tick != 1 {
        return false
    }
    return dispatch.timer_state.monotonic_tick == 1
}

func state_score(state: InterruptState) i32 {
    switch state {
    case InterruptState.Masked:
        return 1
    case InterruptState.Quiescent:
        return 2
    default:
        return 0
    }
    return 0
}