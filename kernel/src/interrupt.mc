enum InterruptState {
    Masked,
    Quiescent,
}

struct InterruptController {
    timer_vector: u32
    external_vector: u32
    state: InterruptState
}

func reset_controller() InterruptController {
    return InterruptController{ timer_vector: 0, external_vector: 0, state: InterruptState.Masked }
}

func unmask_timer(controller: InterruptController, vector: u32) InterruptController {
    return InterruptController{ timer_vector: vector, external_vector: controller.external_vector, state: InterruptState.Quiescent }
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