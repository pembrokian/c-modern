import service_cell_helpers

struct Payload {
    value: i32
}

func main() i32 {
    base := service_cell_helpers.ServiceCell<Payload>{ state: Payload{ value: 3 }, generation: 7 }
    updated := service_cell_helpers.service_cell_with_state<Payload>(base, Payload{ value: 11 })
    restarted := service_cell_helpers.service_cell_restart<Payload>(updated, Payload{ value: 13 })
    if restarted.generation != 8 {
        return 1
    }
    return restarted.state.value
}