struct ServiceCell<T> {
    state: T
    generation: u32
}

func service_cell_with_state<T>(cell: ServiceCell<T>, state: T) ServiceCell<T> {
    return ServiceCell<T>{ state:, generation: cell.generation }
}

func service_cell_restart<T>(cell: ServiceCell<T>, state: T) ServiceCell<T> {
    return ServiceCell<T>{ state:, generation: cell.generation + 1 }
}