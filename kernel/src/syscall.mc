import address_space
import capability
import endpoint
import init
import state
import timer

enum SyscallId {
    None,
    Send,
    Receive,
    Spawn,
    Wait,
    Sleep,
}

enum SyscallStatus {
    None,
    Ok,
    WouldBlock,
    InvalidHandle,
    InvalidEndpoint,
    Closed,
    InvalidCapability,
    Exhausted,
}

struct SyscallGate {
    open: u32
    last_id: SyscallId
    last_status: SyscallStatus
    send_count: u32
    receive_count: u32
}

struct SendRequest {
    handle_slot: u32
    payload_len: usize
    payload: [4]u8
    attached_handle_slot: u32
    attached_handle_count: usize
}

struct ReceiveRequest {
    handle_slot: u32
    receive_handle_slot: u32
}

struct ReceiveObservation {
    status: SyscallStatus
    endpoint_id: u32
    source_pid: u32
    payload_len: usize
    received_handle_slot: u32
    received_handle_count: usize
    payload: [4]u8
}

struct SpawnRequest {
    wait_handle_slot: u32
}

struct WaitRequest {
    wait_handle_slot: u32
}

struct SleepRequest {
    task_slot: u32
    duration_ticks: u64
}

struct SpawnObservation {
    status: SyscallStatus
    child_pid: u32
    child_tid: u32
    child_asid: u32
    wait_handle_slot: u32
}

struct WaitObservation {
    status: SyscallStatus
    child_pid: u32
    exit_code: i32
    wait_handle_slot: u32
}

struct SleepObservation {
    status: SyscallStatus
    task_id: u32
    deadline_tick: u64
    wake_tick: u64
}

struct SendResult {
    gate: SyscallGate
    handle_table: capability.HandleTable
    endpoints: endpoint.EndpointTable
    status: SyscallStatus
}

struct ReceiveResult {
    gate: SyscallGate
    handle_table: capability.HandleTable
    endpoints: endpoint.EndpointTable
    observation: ReceiveObservation
}

struct SpawnResult {
    gate: SyscallGate
    program_capability: capability.CapabilitySlot
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    wait_table: capability.WaitTable
    child_address_space: address_space.AddressSpace
    child_frame: address_space.UserEntryFrame
    observation: SpawnObservation
}

struct WaitResult {
    gate: SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    wait_table: capability.WaitTable
    observation: WaitObservation
}

struct SleepResult {
    gate: SyscallGate
    task_slots: [3]state.TaskSlot
    timer_state: timer.TimerState
    observation: SleepObservation
}

func gate_closed() SyscallGate {
    return SyscallGate{ open: 0, last_id: SyscallId.None, last_status: SyscallStatus.None, send_count: 0, receive_count: 0 }
}

func open_gate(gate: SyscallGate) SyscallGate {
    return SyscallGate{ open: 1, last_id: gate.last_id, last_status: gate.last_status, send_count: gate.send_count, receive_count: gate.receive_count }
}

func build_send_request(handle_slot: u32, payload_len: usize, payload: [4]u8) SendRequest {
    return SendRequest{ handle_slot: handle_slot, payload_len: payload_len, payload: payload, attached_handle_slot: 0, attached_handle_count: 0 }
}

func build_transfer_send_request(handle_slot: u32, payload_len: usize, payload: [4]u8, attached_handle_slot: u32) SendRequest {
    return SendRequest{ handle_slot: handle_slot, payload_len: payload_len, payload: payload, attached_handle_slot: attached_handle_slot, attached_handle_count: 1 }
}

func build_receive_request(handle_slot: u32) ReceiveRequest {
    return ReceiveRequest{ handle_slot: handle_slot, receive_handle_slot: 0 }
}

func build_transfer_receive_request(handle_slot: u32, receive_handle_slot: u32) ReceiveRequest {
    return ReceiveRequest{ handle_slot: handle_slot, receive_handle_slot: receive_handle_slot }
}

func build_spawn_request(wait_handle_slot: u32) SpawnRequest {
    return SpawnRequest{ wait_handle_slot: wait_handle_slot }
}

func build_wait_request(wait_handle_slot: u32) WaitRequest {
    return WaitRequest{ wait_handle_slot: wait_handle_slot }
}

func build_sleep_request(task_slot: u32, duration_ticks: u64) SleepRequest {
    return SleepRequest{ task_slot: task_slot, duration_ticks: duration_ticks }
}

func empty_receive_observation() ReceiveObservation {
    return ReceiveObservation{ status: SyscallStatus.None, endpoint_id: 0, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: endpoint.zero_payload() }
}

func empty_spawn_observation() SpawnObservation {
    return SpawnObservation{ status: SyscallStatus.None, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 }
}

func empty_wait_observation() WaitObservation {
    return WaitObservation{ status: SyscallStatus.None, child_pid: 0, exit_code: 0, wait_handle_slot: 0 }
}

func empty_sleep_observation() SleepObservation {
    return SleepObservation{ status: SyscallStatus.None, task_id: 0, deadline_tick: 0, wake_tick: 0 }
}

func send_result(gate: SyscallGate, handle_table: capability.HandleTable, endpoints: endpoint.EndpointTable, status: SyscallStatus) SendResult {
    return SendResult{ gate: gate, handle_table: handle_table, endpoints: endpoints, status: status }
}

func receive_result(gate: SyscallGate, handle_table: capability.HandleTable, endpoints: endpoint.EndpointTable, observation: ReceiveObservation) ReceiveResult {
    return ReceiveResult{ gate: gate, handle_table: handle_table, endpoints: endpoints, observation: observation }
}

func spawn_result(gate: SyscallGate, program_capability: capability.CapabilitySlot, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_table: capability.WaitTable, child_address_space: address_space.AddressSpace, child_frame: address_space.UserEntryFrame, observation: SpawnObservation) SpawnResult {
    return SpawnResult{ gate: gate, program_capability: program_capability, process_slots: process_slots, task_slots: task_slots, wait_table: wait_table, child_address_space: child_address_space, child_frame: child_frame, observation: observation }
}

func wait_result(gate: SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_table: capability.WaitTable, observation: WaitObservation) WaitResult {
    return WaitResult{ gate: gate, process_slots: process_slots, task_slots: task_slots, wait_table: wait_table, observation: observation }
}

func sleep_result(gate: SyscallGate, task_slots: [3]state.TaskSlot, timer_state: timer.TimerState, observation: SleepObservation) SleepResult {
    return SleepResult{ gate: gate, task_slots: task_slots, timer_state: timer_state, observation: observation }
}

func update_gate(gate: SyscallGate, id: SyscallId, status: SyscallStatus, send_delta: u32, receive_delta: u32) SyscallGate {
    return SyscallGate{ open: gate.open, last_id: id, last_status: status, send_count: gate.send_count + send_delta, receive_count: gate.receive_count + receive_delta }
}

func perform_sleep(gate: SyscallGate, task_slots: [3]state.TaskSlot, timer_state: timer.TimerState, request: SleepRequest) SleepResult {
    if gate.open == 0 {
        return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.Closed, 0, 0), task_slots, timer_state, SleepObservation{ status: SyscallStatus.Closed, task_id: 0, deadline_tick: 0, wake_tick: 0 })
    }
    if request.task_slot >= 3 {
        return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.InvalidHandle, 0, 0), task_slots, timer_state, SleepObservation{ status: SyscallStatus.InvalidHandle, task_id: 0, deadline_tick: 0, wake_tick: 0 })
    }
    if request.task_slot == 0 {
        if state.task_state_score(task_slots[0].state) != 4 {
            return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.InvalidHandle, 0, 0), task_slots, timer_state, SleepObservation{ status: SyscallStatus.InvalidHandle, task_id: 0, deadline_tick: 0, wake_tick: 0 })
        }
        deadline_tick0: u64 = timer_state.monotonic_tick + request.duration_ticks
        updated_timer_state0: timer.TimerState = timer.arm_sleep(timer_state, task_slots[0].tid, deadline_tick0)
        if updated_timer_state0.count == timer_state.count {
            return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.Exhausted, 0, 0), task_slots, timer_state, SleepObservation{ status: SyscallStatus.Exhausted, task_id: 0, deadline_tick: 0, wake_tick: 0 })
        }
        updated_task_slots0: [3]state.TaskSlot = task_slots
        updated_task_slots0[0] = state.blocked_task_slot(task_slots[0])
        return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.WouldBlock, 0, 0), updated_task_slots0, updated_timer_state0, SleepObservation{ status: SyscallStatus.WouldBlock, task_id: task_slots[0].tid, deadline_tick: deadline_tick0, wake_tick: 0 })
    }
    if request.task_slot == 1 {
        if state.task_state_score(task_slots[1].state) != 4 {
            return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.InvalidHandle, 0, 0), task_slots, timer_state, SleepObservation{ status: SyscallStatus.InvalidHandle, task_id: 0, deadline_tick: 0, wake_tick: 0 })
        }
        deadline_tick1: u64 = timer_state.monotonic_tick + request.duration_ticks
        updated_timer_state1: timer.TimerState = timer.arm_sleep(timer_state, task_slots[1].tid, deadline_tick1)
        if updated_timer_state1.count == timer_state.count {
            return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.Exhausted, 0, 0), task_slots, timer_state, SleepObservation{ status: SyscallStatus.Exhausted, task_id: 0, deadline_tick: 0, wake_tick: 0 })
        }
        updated_task_slots1: [3]state.TaskSlot = task_slots
        updated_task_slots1[1] = state.blocked_task_slot(task_slots[1])
        return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.WouldBlock, 0, 0), updated_task_slots1, updated_timer_state1, SleepObservation{ status: SyscallStatus.WouldBlock, task_id: task_slots[1].tid, deadline_tick: deadline_tick1, wake_tick: 0 })
    }
    if state.task_state_score(task_slots[2].state) != 4 {
        return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.InvalidHandle, 0, 0), task_slots, timer_state, SleepObservation{ status: SyscallStatus.InvalidHandle, task_id: 0, deadline_tick: 0, wake_tick: 0 })
    }
    deadline_tick2: u64 = timer_state.monotonic_tick + request.duration_ticks
    updated_timer_state2: timer.TimerState = timer.arm_sleep(timer_state, task_slots[2].tid, deadline_tick2)
    if updated_timer_state2.count == timer_state.count {
        return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.Exhausted, 0, 0), task_slots, timer_state, SleepObservation{ status: SyscallStatus.Exhausted, task_id: 0, deadline_tick: 0, wake_tick: 0 })
    }
    updated_task_slots2: [3]state.TaskSlot = task_slots
    updated_task_slots2[2] = state.blocked_task_slot(task_slots[2])
    return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.WouldBlock, 0, 0), updated_task_slots2, updated_timer_state2, SleepObservation{ status: SyscallStatus.WouldBlock, task_id: task_slots[2].tid, deadline_tick: deadline_tick2, wake_tick: 0 })
}

func perform_send(gate: SyscallGate, handle_table: capability.HandleTable, endpoints: endpoint.EndpointTable, sender_pid: u32, request: SendRequest) SendResult {
    if gate.open == 0 {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.Closed, 0, 0), handle_table, endpoints, SyscallStatus.Closed)
    }
    endpoint_id: u32 = capability.find_endpoint_for_handle(handle_table, request.handle_slot)
    if endpoint_id == 0 {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, SyscallStatus.InvalidHandle)
    }
    endpoint_index: usize = endpoint.find_endpoint_index(endpoints, endpoint_id)
    if endpoint_index >= endpoints.count {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.InvalidEndpoint, 0, 0), handle_table, endpoints, SyscallStatus.InvalidEndpoint)
    }
    attached_endpoint_id: u32 = 0
    attached_rights: u32 = 0
    if request.attached_handle_count == 1 {
        attached_endpoint_id = capability.find_endpoint_for_handle(handle_table, request.attached_handle_slot)
        attached_rights = capability.find_rights_for_handle(handle_table, request.attached_handle_slot)
        if attached_endpoint_id == 0 || attached_rights == 0 {
            return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, SyscallStatus.InvalidHandle)
        }
    }
    message_id: u32 = gate.send_count + 2
    message: endpoint.KernelMessage = endpoint.byte_message(message_id, sender_pid, endpoint_id, request.payload_len, request.payload)
    if request.attached_handle_count == 1 {
        message = endpoint.attached_message(message_id, sender_pid, endpoint_id, request.payload_len, request.payload, attached_endpoint_id, attached_rights)
    }
    queued_before: usize = endpoints.slots[endpoint_index].queued_messages
    updated_endpoints: endpoint.EndpointTable = endpoint.enqueue_message(endpoints, endpoint_index, message)
    if updated_endpoints.slots[endpoint_index].queued_messages == queued_before {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.WouldBlock, 0, 0), handle_table, endpoints, SyscallStatus.WouldBlock)
    }
    updated_handle_table: capability.HandleTable = handle_table
    if request.attached_handle_count == 1 {
        updated_handle_table = capability.remove_handle(handle_table, request.attached_handle_slot)
    }
    return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.Ok, 1, 0), updated_handle_table, updated_endpoints, SyscallStatus.Ok)
}

func perform_receive(gate: SyscallGate, handle_table: capability.HandleTable, endpoints: endpoint.EndpointTable, request: ReceiveRequest) ReceiveResult {
    if gate.open == 0 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.Closed, 0, 0), handle_table, endpoints, ReceiveObservation{ status: SyscallStatus.Closed, endpoint_id: 0, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: endpoint.zero_payload() })
    }
    endpoint_id: u32 = capability.find_endpoint_for_handle(handle_table, request.handle_slot)
    if endpoint_id == 0 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, ReceiveObservation{ status: SyscallStatus.InvalidHandle, endpoint_id: 0, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: endpoint.zero_payload() })
    }
    endpoint_index: usize = endpoint.find_endpoint_index(endpoints, endpoint_id)
    if endpoint_index >= endpoints.count {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.InvalidEndpoint, 0, 0), handle_table, endpoints, ReceiveObservation{ status: SyscallStatus.InvalidEndpoint, endpoint_id: endpoint_id, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: endpoint.zero_payload() })
    }
    message: endpoint.KernelMessage = endpoint.peek_head_message(endpoints, endpoint_index)
    if endpoint.message_state_score(message.state) == 1 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.WouldBlock, 0, 0), handle_table, endpoints, ReceiveObservation{ status: SyscallStatus.WouldBlock, endpoint_id: endpoint_id, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: endpoint.zero_payload() })
    }
    updated_handle_table: capability.HandleTable = handle_table
    if message.attached_count == 1 {
        if request.receive_handle_slot == 0 {
            return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, ReceiveObservation{ status: SyscallStatus.InvalidHandle, endpoint_id: endpoint_id, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: endpoint.zero_payload() })
        }
        updated_handle_table = capability.install_endpoint_handle(handle_table, request.receive_handle_slot, message.attached_endpoint_id, message.attached_rights)
        if updated_handle_table.count != handle_table.count + 1 {
            return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, ReceiveObservation{ status: SyscallStatus.InvalidHandle, endpoint_id: endpoint_id, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: endpoint.zero_payload() })
        }
    }
    observation: ReceiveObservation = ReceiveObservation{ status: SyscallStatus.Ok, endpoint_id: message.endpoint_id, source_pid: message.source_pid, payload_len: message.len, received_handle_slot: request.receive_handle_slot, received_handle_count: message.attached_count, payload: message.payload }
    updated_endpoints: endpoint.EndpointTable = endpoint.consume_head_message(endpoints, endpoint_index)
    return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.Ok, 0, 1), updated_handle_table, updated_endpoints, observation)
}

func perform_spawn(gate: SyscallGate, program_capability: capability.CapabilitySlot, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_table: capability.WaitTable, child_image: init.InitImage, request: SpawnRequest, child_pid: u32, child_tid: u32, child_task_slot: u32, child_asid: u32, child_root_page_table: usize) SpawnResult {
    if gate.open == 0 {
        return spawn_result(update_gate(gate, SyscallId.Spawn, SyscallStatus.Closed, 0, 0), program_capability, process_slots, task_slots, wait_table, address_space.empty_space(), address_space.empty_frame(), SpawnObservation{ status: SyscallStatus.Closed, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 })
    }
    if capability.kind_score(program_capability.kind) != 4 {
        return spawn_result(update_gate(gate, SyscallId.Spawn, SyscallStatus.InvalidCapability, 0, 0), program_capability, process_slots, task_slots, wait_table, address_space.empty_space(), address_space.empty_frame(), SpawnObservation{ status: SyscallStatus.InvalidCapability, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 })
    }
    if request.wait_handle_slot == 0 {
        return spawn_result(update_gate(gate, SyscallId.Spawn, SyscallStatus.InvalidHandle, 0, 0), program_capability, process_slots, task_slots, wait_table, address_space.empty_space(), address_space.empty_frame(), SpawnObservation{ status: SyscallStatus.InvalidHandle, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 })
    }
    if state.process_state_score(process_slots[2].state) != 1 || state.task_state_score(task_slots[2].state) != 1 {
        return spawn_result(update_gate(gate, SyscallId.Spawn, SyscallStatus.Exhausted, 0, 0), program_capability, process_slots, task_slots, wait_table, address_space.empty_space(), address_space.empty_frame(), SpawnObservation{ status: SyscallStatus.Exhausted, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 })
    }
    updated_wait_table: capability.WaitTable = capability.install_wait_handle(wait_table, request.wait_handle_slot, child_pid)
    if updated_wait_table.count != wait_table.count + 1 {
        return spawn_result(update_gate(gate, SyscallId.Spawn, SyscallStatus.Exhausted, 0, 0), program_capability, process_slots, task_slots, wait_table, address_space.empty_space(), address_space.empty_frame(), SpawnObservation{ status: SyscallStatus.Exhausted, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 })
    }
    updated_process_slots: [3]state.ProcessSlot = process_slots
    updated_task_slots: [3]state.TaskSlot = task_slots
    child_space: address_space.AddressSpace = address_space.bootstrap_space(child_asid, child_pid, child_root_page_table, child_image.image_base, child_image.image_size, child_image.entry_pc, child_image.stack_base, child_image.stack_size, child_image.stack_top)
    child_frame: address_space.UserEntryFrame = address_space.bootstrap_user_frame(child_space, child_tid)
    updated_process_slots[2] = state.init_process_slot(child_pid, child_task_slot, child_asid)
    updated_task_slots[2] = state.user_task_slot(child_tid, child_pid, child_asid, child_image.entry_pc, child_image.stack_top)
    return spawn_result(update_gate(gate, SyscallId.Spawn, SyscallStatus.Ok, 0, 0), capability.empty_slot(), updated_process_slots, updated_task_slots, updated_wait_table, child_space, child_frame, SpawnObservation{ status: SyscallStatus.Ok, child_pid: child_pid, child_tid: child_tid, child_asid: child_asid, wait_handle_slot: request.wait_handle_slot })
}

func perform_wait(gate: SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_table: capability.WaitTable, request: WaitRequest) WaitResult {
    if gate.open == 0 {
        return wait_result(update_gate(gate, SyscallId.Wait, SyscallStatus.Closed, 0, 0), process_slots, task_slots, wait_table, WaitObservation{ status: SyscallStatus.Closed, child_pid: 0, exit_code: 0, wait_handle_slot: 0 })
    }
    child_pid: u32 = capability.find_child_for_wait_handle(wait_table, request.wait_handle_slot)
    if child_pid == 0 {
        return wait_result(update_gate(gate, SyscallId.Wait, SyscallStatus.InvalidHandle, 0, 0), process_slots, task_slots, wait_table, WaitObservation{ status: SyscallStatus.InvalidHandle, child_pid: 0, exit_code: 0, wait_handle_slot: 0 })
    }
    if capability.wait_handle_signaled(wait_table, request.wait_handle_slot) == 0 {
        return wait_result(update_gate(gate, SyscallId.Wait, SyscallStatus.WouldBlock, 0, 0), process_slots, task_slots, wait_table, WaitObservation{ status: SyscallStatus.WouldBlock, child_pid: child_pid, exit_code: 0, wait_handle_slot: request.wait_handle_slot })
    }
    updated_wait_table: capability.WaitTable = capability.consume_wait_handle(wait_table, request.wait_handle_slot)
    updated_process_slots: [3]state.ProcessSlot = process_slots
    updated_task_slots: [3]state.TaskSlot = task_slots
    if updated_process_slots[2].pid == child_pid {
        updated_process_slots[2] = state.empty_process_slot()
    }
    if updated_task_slots[2].owner_pid == child_pid {
        updated_task_slots[2] = state.empty_task_slot()
    }
    return wait_result(update_gate(gate, SyscallId.Wait, SyscallStatus.Ok, 0, 0), updated_process_slots, updated_task_slots, updated_wait_table, WaitObservation{ status: SyscallStatus.Ok, child_pid: child_pid, exit_code: capability.find_exit_code_for_wait_handle(wait_table, request.wait_handle_slot), wait_handle_slot: request.wait_handle_slot })
}

func id_score(id: SyscallId) i32 {
    switch id {
    case SyscallId.None:
        return 1
    case SyscallId.Send:
        return 2
    case SyscallId.Receive:
        return 4
    case SyscallId.Spawn:
        return 8
    case SyscallId.Wait:
        return 16
    case SyscallId.Sleep:
        return 32
    default:
        return 0
    }
    return 0
}

func status_score(status: SyscallStatus) i32 {
    switch status {
    case SyscallStatus.None:
        return 1
    case SyscallStatus.Ok:
        return 2
    case SyscallStatus.WouldBlock:
        return 4
    case SyscallStatus.InvalidHandle:
        return 8
    case SyscallStatus.InvalidEndpoint:
        return 16
    case SyscallStatus.Closed:
        return 32
    case SyscallStatus.InvalidCapability:
        return 64
    case SyscallStatus.Exhausted:
        return 128
    default:
        return 0
    }
    return 0
}