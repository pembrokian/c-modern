import address_space
import capability
import endpoint
import init
import lifecycle
import mmu
import state
import timer

const FIRST_RUNTIME_MESSAGE_ID: u32 = 2

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

enum BlockReason {
    None,
    EndpointQueueFull,
    EndpointQueueEmpty,
    WaitPending,
    TimerPending,
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
    block_reason: BlockReason
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
    block_reason: BlockReason
    child_pid: u32
    exit_code: i32
    wait_handle_slot: u32
}

struct SleepObservation {
    status: SyscallStatus
    block_reason: BlockReason
    task_id: u32
    deadline_tick: u64
    wake_tick: u64
}

struct SendResult {
    gate: SyscallGate
    handle_table: capability.HandleTable
    endpoints: endpoint.EndpointTable
    status: SyscallStatus
    block_reason: BlockReason
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

struct SleepSlotTransition {
    task_slots: [3]state.TaskSlot
    timer_state: timer.TimerState
    observation: SleepObservation
    status: SyscallStatus
}

struct SpawnAdmission {
    wait_table: capability.WaitTable
    process_slot: u32
    task_slot: u32
    status: SyscallStatus
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
    return ReceiveObservation{ status: SyscallStatus.None, block_reason: BlockReason.None, endpoint_id: 0, source_pid: 0, payload_len: 0, received_handle_slot: 0, received_handle_count: 0, payload: endpoint.zero_payload() }
}

func empty_spawn_observation() SpawnObservation {
    return SpawnObservation{ status: SyscallStatus.None, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 }
}

func empty_wait_observation() WaitObservation {
    return WaitObservation{ status: SyscallStatus.None, block_reason: BlockReason.None, child_pid: 0, exit_code: 0, wait_handle_slot: 0 }
}

func empty_sleep_observation() SleepObservation {
    return SleepObservation{ status: SyscallStatus.None, block_reason: BlockReason.None, task_id: 0, deadline_tick: 0, wake_tick: 0 }
}

func send_result(gate: SyscallGate, handle_table: capability.HandleTable, endpoints: endpoint.EndpointTable, status: SyscallStatus, block_reason: BlockReason) SendResult {
    return SendResult{ gate: gate, handle_table: handle_table, endpoints: endpoints, status: status, block_reason: block_reason }
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

func receive_observation(status: SyscallStatus, block_reason: BlockReason, endpoint_id: u32, source_pid: u32, payload_len: usize, received_handle_slot: u32, received_handle_count: usize, payload: [4]u8) ReceiveObservation {
    return ReceiveObservation{ status: status, block_reason: block_reason, endpoint_id: endpoint_id, source_pid: source_pid, payload_len: payload_len, received_handle_slot: received_handle_slot, received_handle_count: received_handle_count, payload: payload }
}

func wait_observation(status: SyscallStatus, block_reason: BlockReason, child_pid: u32, exit_code: i32, wait_handle_slot: u32) WaitObservation {
    return WaitObservation{ status: status, block_reason: block_reason, child_pid: child_pid, exit_code: exit_code, wait_handle_slot: wait_handle_slot }
}

func sleep_observation(status: SyscallStatus, block_reason: BlockReason, task_id: u32, deadline_tick: u64, wake_tick: u64) SleepObservation {
    return SleepObservation{ status: status, block_reason: block_reason, task_id: task_id, deadline_tick: deadline_tick, wake_tick: wake_tick }
}

func update_gate(gate: SyscallGate, id: SyscallId, status: SyscallStatus, send_delta: u32, receive_delta: u32) SyscallGate {
    return SyscallGate{ open: gate.open, last_id: id, last_status: status, send_count: gate.send_count + send_delta, receive_count: gate.receive_count + receive_delta }
}

func admit_spawn(program_capability: capability.CapabilitySlot, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_table: capability.WaitTable, child_image: init.InitImage, request: SpawnRequest, child_pid: u32) SpawnAdmission {
    if !capability.is_program_capability(program_capability) {
        return SpawnAdmission{ wait_table: wait_table, process_slot: 0, task_slot: 0, status: SyscallStatus.InvalidCapability }
    }
    if !init.bootstrap_image_valid(child_image) {
        return SpawnAdmission{ wait_table: wait_table, process_slot: 0, task_slot: 0, status: SyscallStatus.InvalidCapability }
    }
    process_slot: u32 = state.find_empty_process_slot(process_slots)
    task_slot: u32 = state.find_empty_task_slot(task_slots)
    if process_slot >= 3 || task_slot >= 3 {
        return SpawnAdmission{ wait_table: wait_table, process_slot: 0, task_slot: 0, status: SyscallStatus.Exhausted }
    }
    wait_admission: capability.SpawnWaitHandleAdmission = capability.admit_spawn_wait_handle(wait_table, request.wait_handle_slot, child_pid)
    if wait_admission.valid == 0 {
        if wait_admission.invalid_handle != 0 {
            return SpawnAdmission{ wait_table: wait_table, process_slot: 0, task_slot: 0, status: SyscallStatus.InvalidHandle }
        }
        return SpawnAdmission{ wait_table: wait_table, process_slot: 0, task_slot: 0, status: SyscallStatus.Exhausted }
    }
    return SpawnAdmission{ wait_table: wait_admission.wait_table, process_slot: process_slot, task_slot: task_slot, status: SyscallStatus.Ok }
}

func perform_sleep_slot(task_slots: [3]state.TaskSlot, timer_state: timer.TimerState, task_slot: u32, duration_ticks: u64) SleepSlotTransition {
    selected_task: state.TaskSlot = state.task_slot_at(task_slots, task_slot)
    if !state.can_block_task(selected_task) {
        return SleepSlotTransition{ task_slots: task_slots, timer_state: timer_state, observation: sleep_observation(SyscallStatus.InvalidHandle, BlockReason.None, 0, 0, 0), status: SyscallStatus.InvalidHandle }
    }
    deadline_tick: u64 = timer_state.monotonic_tick + duration_ticks
    updated_timer_state: timer.TimerState = timer.arm_sleep(timer_state, selected_task.tid, deadline_tick)
    if updated_timer_state.count == timer_state.count {
        return SleepSlotTransition{ task_slots: task_slots, timer_state: timer_state, observation: sleep_observation(SyscallStatus.Exhausted, BlockReason.None, 0, 0, 0), status: SyscallStatus.Exhausted }
    }
    blocked: lifecycle.TaskTransition = lifecycle.block_task_on_timer(task_slots, task_slot)
    return SleepSlotTransition{ task_slots: blocked.task_slots, timer_state: updated_timer_state, observation: sleep_observation(SyscallStatus.WouldBlock, BlockReason.TimerPending, selected_task.tid, deadline_tick, 0), status: SyscallStatus.WouldBlock }
}

func perform_sleep(gate: SyscallGate, task_slots: [3]state.TaskSlot, timer_state: timer.TimerState, request: SleepRequest) SleepResult {
    if gate.open == 0 {
        return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.Closed, 0, 0), task_slots, timer_state, sleep_observation(SyscallStatus.Closed, BlockReason.None, 0, 0, 0))
    }
    if request.task_slot >= 3 {
        return sleep_result(update_gate(gate, SyscallId.Sleep, SyscallStatus.InvalidHandle, 0, 0), task_slots, timer_state, sleep_observation(SyscallStatus.InvalidHandle, BlockReason.None, 0, 0, 0))
    }
    transition: SleepSlotTransition = perform_sleep_slot(task_slots, timer_state, request.task_slot, request.duration_ticks)
    return sleep_result(update_gate(gate, SyscallId.Sleep, transition.status, 0, 0), transition.task_slots, transition.timer_state, transition.observation)
}

func perform_send(gate: SyscallGate, handle_table: capability.HandleTable, endpoints: endpoint.EndpointTable, sender_pid: u32, request: SendRequest) SendResult {
    if gate.open == 0 {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.Closed, 0, 0), handle_table, endpoints, SyscallStatus.Closed, BlockReason.None)
    }
    resolved_handle: capability.EndpointHandleResolution = capability.resolve_send_endpoint_handle(handle_table, request.handle_slot)
    if resolved_handle.valid == 0 {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, SyscallStatus.InvalidHandle, BlockReason.None)
    }
    attached: capability.AttachedTransferResolution = capability.resolve_attached_transfer_handle(handle_table, request.attached_handle_slot, request.attached_handle_count)
    if attached.valid == 0 {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, SyscallStatus.InvalidHandle, BlockReason.None)
    }
    updated_handle_table: capability.HandleTable = handle_table
    if request.attached_handle_count == 1 {
        updated_handle_table = capability.remove_handle(handle_table, request.attached_handle_slot)
        if !capability.handle_remove_succeeded(handle_table, updated_handle_table, request.attached_handle_slot) {
            return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, SyscallStatus.InvalidHandle, BlockReason.None)
        }
    }
    message_id: u32 = gate.send_count + FIRST_RUNTIME_MESSAGE_ID
    message: endpoint.KernelMessage = endpoint.build_runtime_message(message_id, sender_pid, resolved_handle.endpoint_id, request.payload_len, request.payload, attached.attached_count, attached.attached_endpoint_id, attached.attached_rights, attached.attached_source_handle_slot)
    sent: endpoint.RuntimeSendResult = endpoint.enqueue_runtime_message(endpoints, resolved_handle.endpoint_id, message)
    if sent.endpoint_valid == 0 {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.InvalidEndpoint, 0, 0), handle_table, endpoints, SyscallStatus.InvalidEndpoint, BlockReason.None)
    }
    if sent.queue_full != 0 {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.WouldBlock, 0, 0), handle_table, endpoints, SyscallStatus.WouldBlock, BlockReason.EndpointQueueFull)
    }
    return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.Ok, 1, 0), updated_handle_table, sent.endpoints, SyscallStatus.Ok, BlockReason.None)
}

func perform_receive(gate: SyscallGate, handle_table: capability.HandleTable, endpoints: endpoint.EndpointTable, request: ReceiveRequest) ReceiveResult {
    if gate.open == 0 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.Closed, 0, 0), handle_table, endpoints, receive_observation(SyscallStatus.Closed, BlockReason.None, 0, 0, 0, 0, 0, endpoint.zero_payload()))
    }
    resolved_handle: capability.EndpointHandleResolution = capability.resolve_receive_endpoint_handle(handle_table, request.handle_slot)
    if resolved_handle.valid == 0 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, receive_observation(SyscallStatus.InvalidHandle, BlockReason.None, 0, 0, 0, 0, 0, endpoint.zero_payload()))
    }
    received: endpoint.RuntimeReceiveResult = endpoint.receive_runtime_message(endpoints, resolved_handle.endpoint_id)
    if received.endpoint_valid == 0 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.InvalidEndpoint, 0, 0), handle_table, endpoints, receive_observation(SyscallStatus.InvalidEndpoint, BlockReason.None, resolved_handle.endpoint_id, 0, 0, 0, 0, endpoint.zero_payload()))
    }
    if received.queue_empty != 0 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.WouldBlock, 0, 0), handle_table, endpoints, receive_observation(SyscallStatus.WouldBlock, BlockReason.EndpointQueueEmpty, resolved_handle.endpoint_id, 0, 0, 0, 0, endpoint.zero_payload()))
    }
    install: capability.ReceivedHandleInstall = capability.install_received_endpoint_handle(handle_table, request.receive_handle_slot, received.message.attached_count, received.message.attached_endpoint_id, received.message.attached_rights)
    if install.valid == 0 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.InvalidHandle, 0, 0), handle_table, endpoints, receive_observation(SyscallStatus.InvalidHandle, BlockReason.None, resolved_handle.endpoint_id, 0, 0, 0, 0, endpoint.zero_payload()))
    }
    observation: ReceiveObservation = receive_observation(SyscallStatus.Ok, BlockReason.None, received.message.endpoint_id, received.message.source_pid, received.message.len, install.received_handle_slot, install.received_handle_count, received.message.payload)
    return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.Ok, 0, 1), install.handle_table, received.endpoints, observation)
}

func perform_spawn(gate: SyscallGate, program_capability: capability.CapabilitySlot, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_table: capability.WaitTable, child_image: init.InitImage, request: SpawnRequest, child_pid: u32, child_tid: u32, child_asid: u32, child_translation_root: mmu.TranslationRoot) SpawnResult {
    if gate.open == 0 {
        return spawn_result(update_gate(gate, SyscallId.Spawn, SyscallStatus.Closed, 0, 0), program_capability, process_slots, task_slots, wait_table, address_space.empty_space(), address_space.empty_frame(), SpawnObservation{ status: SyscallStatus.Closed, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 })
    }
    admission: SpawnAdmission = admit_spawn(program_capability, process_slots, task_slots, wait_table, child_image, request, child_pid)
    if status_score(admission.status) != 2 {
        return spawn_result(update_gate(gate, SyscallId.Spawn, admission.status, 0, 0), program_capability, process_slots, task_slots, wait_table, address_space.empty_space(), address_space.empty_frame(), SpawnObservation{ status: admission.status, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 })
    }
    child_bootstrap: address_space.SpawnBootstrap = address_space.build_child_bootstrap_context(child_pid, child_tid, child_asid, child_translation_root, child_image.image_base, child_image.image_size, child_image.entry_pc, child_image.stack_base, child_image.stack_size, child_image.stack_top)
    if child_bootstrap.valid == 0 {
        return spawn_result(update_gate(gate, SyscallId.Spawn, SyscallStatus.InvalidCapability, 0, 0), program_capability, process_slots, task_slots, wait_table, address_space.empty_space(), address_space.empty_frame(), SpawnObservation{ status: SyscallStatus.InvalidCapability, child_pid: 0, child_tid: 0, child_asid: 0, wait_handle_slot: 0 })
    }
    install: lifecycle.SpawnInstallResult = lifecycle.install_spawned_child(process_slots, task_slots, child_pid, child_tid, child_asid, admission.process_slot, admission.task_slot, child_image.entry_pc, child_image.stack_top)
    return spawn_result(update_gate(gate, SyscallId.Spawn, SyscallStatus.Ok, 0, 0), capability.empty_slot(), install.process_slots, install.task_slots, admission.wait_table, child_bootstrap.child_address_space, child_bootstrap.child_frame, SpawnObservation{ status: SyscallStatus.Ok, child_pid: child_pid, child_tid: child_tid, child_asid: child_asid, wait_handle_slot: request.wait_handle_slot })
}

func perform_wait(gate: SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_table: capability.WaitTable, request: WaitRequest) WaitResult {
    if gate.open == 0 {
        return wait_result(update_gate(gate, SyscallId.Wait, SyscallStatus.Closed, 0, 0), process_slots, task_slots, wait_table, wait_observation(SyscallStatus.Closed, BlockReason.None, 0, 0, 0))
    }
    child_pid: u32 = capability.find_child_for_wait_handle(wait_table, request.wait_handle_slot)
    if child_pid == 0 {
        return wait_result(update_gate(gate, SyscallId.Wait, SyscallStatus.InvalidHandle, 0, 0), process_slots, task_slots, wait_table, wait_observation(SyscallStatus.InvalidHandle, BlockReason.None, 0, 0, 0))
    }
    if capability.wait_handle_signaled(wait_table, request.wait_handle_slot) == 0 {
        return wait_result(update_gate(gate, SyscallId.Wait, SyscallStatus.WouldBlock, 0, 0), process_slots, task_slots, wait_table, wait_observation(SyscallStatus.WouldBlock, BlockReason.WaitPending, child_pid, 0, request.wait_handle_slot))
    }
    updated_wait_table: capability.WaitTable = capability.consume_wait_handle(wait_table, request.wait_handle_slot)
    released: lifecycle.ReleaseTransition = lifecycle.release_waited_child_slots(process_slots, task_slots, child_pid)
    exit_code: i32 = capability.find_exit_code_for_wait_handle(wait_table, request.wait_handle_slot)
    return wait_result(update_gate(gate, SyscallId.Wait, SyscallStatus.Ok, 0, 0), released.process_slots, released.task_slots, updated_wait_table, wait_observation(SyscallStatus.Ok, BlockReason.None, child_pid, exit_code, request.wait_handle_slot))
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

func block_reason_score(reason: BlockReason) i32 {
    switch reason {
    case BlockReason.None:
        return 1
    case BlockReason.EndpointQueueFull:
        return 2
    case BlockReason.EndpointQueueEmpty:
        return 4
    case BlockReason.WaitPending:
        return 8
    case BlockReason.TimerPending:
        return 16
    default:
        return 0
    }
    return 0
}