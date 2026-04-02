export { Counter, add_one }

type Counter = i32

func add_one(value: Counter) Counter {
    return value + 1
}