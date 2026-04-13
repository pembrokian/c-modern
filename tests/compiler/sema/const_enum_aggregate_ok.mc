enum TaskState {
    Empty,
    Ready,
    Running,
}

struct Task {
    pid: u32
    state: TaskState
}

const READY_STATE: TaskState = TaskState.Ready
const BOOT_TASK: Task = Task{ pid: 1, state: TaskState.Running }

func main() i32 {
    return 0
}