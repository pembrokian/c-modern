enum TaskState {
    Ready,
    Running,
}

enum Option<T> {
    Some(value: T)
    None
}

const RUNNING_STATE: TaskState = TaskState.Running
const SOME_VALUE: Option<i32> = Option<i32>.Some(7)