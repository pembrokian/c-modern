import address_space
import capability
import init
import interrupt
import ipc
import lifecycle
import mmu
import sched
import state
import syscall
import timer

struct ChildExecutionConfig {
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_task_slot: u32
    child_asid: u32
    child_wait_handle_slot: u32
    child_root_page_table: usize
    child_exit_code: i32
    timer_interrupt_vector: u32
    interrupt_actor: u32
}

struct InitRuntimeSnapshot {
    bootstrap_caps: init.BootstrapCapabilitySet
    bootstrap_handoff: init.BootstrapHandoffObservation
    address_space: address_space.AddressSpace
    user_frame: address_space.UserEntryFrame
}

struct InitAddressSpaceResult {
    program_capability: capability.CapabilitySlot
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    ready_queue: state.ReadyQueue
    snapshot: InitRuntimeSnapshot
    succeeded: u32
}

struct BootstrapCapabilityHandoffResult {
    snapshot: InitRuntimeSnapshot
    succeeded: u32
}

struct FirstUserEntryTransferResult {
    kernel: state.KernelDescriptor
    snapshot: InitRuntimeSnapshot
    succeeded: u32
}

struct EndpointBootstrapResult {
    handle_tables: [3]capability.HandleTable
    endpoints: ipc.EndpointTable
    delivered_message: ipc.KernelMessage
}

struct SyscallByteIpcResult {
    gate: syscall.SyscallGate
    handle_tables: [3]capability.HandleTable
    endpoints: ipc.EndpointTable
    observation: syscall.ReceiveObservation
    succeeded: u32
}

struct TransferEndpointSeedResult {
    handle_tables: [3]capability.HandleTable
    endpoints: ipc.EndpointTable
    succeeded: u32
}

struct CapabilityCarryingIpcTransferResult {
    gate: syscall.SyscallGate
    handle_tables: [3]capability.HandleTable
    endpoints: ipc.EndpointTable
    attached_receive_observation: syscall.ReceiveObservation
    transferred_handle_use_observation: syscall.ReceiveObservation
    succeeded: u32
}

struct ChildExecutionSnapshot {
    translation_root: mmu.TranslationRoot
    address_space: address_space.AddressSpace
    user_frame: address_space.UserEntryFrame
    spawn_observation: syscall.SpawnObservation
    sleep_observation: syscall.SleepObservation
    timer_wake_observation: timer.TimerWakeObservation
    wake_ready_queue: state.ReadyQueue
    pre_exit_wait_observation: syscall.WaitObservation
    exit_wait_observation: syscall.WaitObservation
}

struct ChildExecutionResult {
    init_program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    wait_tables: [3]capability.WaitTable
    ready_queue: state.ReadyQueue
    timer_state: timer.TimerState
    interrupts: interrupt.InterruptController
    last_interrupt_kind: interrupt.InterruptDispatchKind
    snapshot: ChildExecutionSnapshot
    succeeded: u32
}

func child_execution_snapshot(translation_root: mmu.TranslationRoot, address_space_value: address_space.AddressSpace, user_frame: address_space.UserEntryFrame, spawn_observation: syscall.SpawnObservation, sleep_observation: syscall.SleepObservation, timer_wake_observation: timer.TimerWakeObservation, wake_ready_queue: state.ReadyQueue, pre_exit_wait_observation: syscall.WaitObservation, exit_wait_observation: syscall.WaitObservation) ChildExecutionSnapshot {
    return ChildExecutionSnapshot{ translation_root: translation_root, address_space: address_space_value, user_frame: user_frame, spawn_observation: spawn_observation, sleep_observation: sleep_observation, timer_wake_observation: timer_wake_observation, wake_ready_queue: wake_ready_queue, pre_exit_wait_observation: pre_exit_wait_observation, exit_wait_observation: exit_wait_observation }
}

func child_execution_result(init_program_capability: capability.CapabilitySlot, gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_tables: [3]capability.WaitTable, ready_queue: state.ReadyQueue, timer_state: timer.TimerState, interrupts: interrupt.InterruptController, last_interrupt_kind: interrupt.InterruptDispatchKind, snapshot: ChildExecutionSnapshot, succeeded: u32) ChildExecutionResult {
    return ChildExecutionResult{ init_program_capability: init_program_capability, gate: gate, process_slots: process_slots, task_slots: task_slots, wait_tables: wait_tables, ready_queue: ready_queue, timer_state: timer_state, interrupts: interrupts, last_interrupt_kind: last_interrupt_kind, snapshot: snapshot, succeeded: succeeded }
}

func empty_child_execution_snapshot() ChildExecutionSnapshot {
    return child_execution_snapshot(mmu.empty_translation_root(), address_space.empty_space(), address_space.empty_frame(), syscall.empty_spawn_observation(), syscall.empty_sleep_observation(), timer.TimerWakeObservation{ task_id: 0, deadline_tick: 0, wake_tick: 0, wake_count: 0 }, state.empty_queue(), syscall.empty_wait_observation(), syscall.empty_wait_observation())
}

func execute_program_cap_spawn_and_wait(config: ChildExecutionConfig, init_program_capability: capability.CapabilitySlot, gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_tables: [3]capability.WaitTable, ready_queue: state.ReadyQueue, timer_state: timer.TimerState, interrupts: interrupt.InterruptController, last_interrupt_kind: interrupt.InterruptDispatchKind, init_image: init.InitImage, snapshot: ChildExecutionSnapshot) ChildExecutionResult {
    next_init_program_capability: capability.CapabilitySlot = init_program_capability
    next_gate: syscall.SyscallGate = gate
    next_process_slots: [3]state.ProcessSlot = process_slots
    next_task_slots: [3]state.TaskSlot = task_slots
    next_wait_tables: [3]capability.WaitTable = wait_tables
    next_ready_queue: state.ReadyQueue = ready_queue
    next_timer_state: timer.TimerState = timer_state
    next_interrupts: interrupt.InterruptController = interrupts
    next_last_interrupt_kind: interrupt.InterruptDispatchKind = last_interrupt_kind
    next_snapshot: ChildExecutionSnapshot = snapshot

    next_wait_tables[1] = capability.wait_table_for_owner(config.init_pid)
    new_translation_root: mmu.TranslationRoot = mmu.bootstrap_translation_root(config.child_asid, config.child_root_page_table)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(config.child_wait_handle_slot)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(next_gate, next_init_program_capability, next_process_slots, next_task_slots, next_wait_tables[1], init_image, spawn_request, config.child_pid, config.child_tid, config.child_asid, new_translation_root)
    next_gate = spawn_result.gate
    next_init_program_capability = spawn_result.program_capability
    next_process_slots = spawn_result.process_slots
    next_task_slots = spawn_result.task_slots
    next_wait_tables[1] = spawn_result.wait_table
    next_snapshot = child_execution_snapshot(new_translation_root, spawn_result.child_address_space, spawn_result.child_frame, spawn_result.observation, next_snapshot.sleep_observation, next_snapshot.timer_wake_observation, next_snapshot.wake_ready_queue, next_snapshot.pre_exit_wait_observation, next_snapshot.exit_wait_observation)
    next_ready_queue = state.user_ready_queue(config.child_tid)
    if syscall.status_score(next_snapshot.spawn_observation.status) != 2 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }

    sleep_result: syscall.SleepResult = syscall.perform_sleep(next_gate, next_task_slots, next_timer_state, syscall.build_sleep_request(config.child_task_slot, 1))
    next_gate = sleep_result.gate
    next_task_slots = sleep_result.task_slots
    next_timer_state = sleep_result.timer_state
    next_snapshot = child_execution_snapshot(next_snapshot.translation_root, next_snapshot.address_space, next_snapshot.user_frame, next_snapshot.spawn_observation, sleep_result.observation, next_snapshot.timer_wake_observation, next_snapshot.wake_ready_queue, next_snapshot.pre_exit_wait_observation, next_snapshot.exit_wait_observation)
    next_ready_queue = state.empty_queue()
    if syscall.status_score(next_snapshot.sleep_observation.status) != 4 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if syscall.id_score(next_gate.last_id) != 32 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if syscall.status_score(next_gate.last_status) != 4 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if state.task_state_score(next_task_slots[2].state) != 8 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if next_snapshot.sleep_observation.task_id != config.child_tid {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if next_snapshot.sleep_observation.deadline_tick != 1 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }

    interrupt_entry: interrupt.InterruptEntry = interrupt.arch_enter_interrupt(next_interrupts, config.timer_interrupt_vector, config.interrupt_actor)
    next_interrupts = interrupt_entry.controller
    dispatch_result: interrupt.InterruptDispatchResult = interrupt.dispatch_interrupt(interrupt_entry)
    next_interrupts = dispatch_result.controller
    next_last_interrupt_kind = dispatch_result.kind
    delivery: timer.TimerInterruptDelivery = timer.deliver_interrupt_tick(next_timer_state, 1)
    next_timer_state = delivery.timer_state
    next_snapshot = child_execution_snapshot(next_snapshot.translation_root, next_snapshot.address_space, next_snapshot.user_frame, next_snapshot.spawn_observation, next_snapshot.sleep_observation, delivery.observation, next_snapshot.wake_ready_queue, next_snapshot.pre_exit_wait_observation, next_snapshot.exit_wait_observation)
    wake_transition: lifecycle.TaskTransition = lifecycle.ready_task(next_task_slots, 2)
    next_task_slots = wake_transition.task_slots
    next_ready_queue = state.user_ready_queue(config.child_tid)
    next_snapshot = child_execution_snapshot(next_snapshot.translation_root, next_snapshot.address_space, next_snapshot.user_frame, next_snapshot.spawn_observation, next_snapshot.sleep_observation, next_snapshot.timer_wake_observation, next_ready_queue, next_snapshot.pre_exit_wait_observation, next_snapshot.exit_wait_observation)
    if next_snapshot.timer_wake_observation.task_id != config.child_tid {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if next_snapshot.timer_wake_observation.deadline_tick != 1 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if next_snapshot.timer_wake_observation.wake_tick != 1 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if next_snapshot.timer_wake_observation.wake_count != 1 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if interrupt.dispatch_kind_score(next_last_interrupt_kind) != 2 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if next_timer_state.monotonic_tick != 1 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if next_timer_state.wake_count != 1 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if next_timer_state.count != 0 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }
    if state.task_state_score(next_task_slots[2].state) != 4 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }

    pre_wait_result: syscall.WaitResult = syscall.perform_wait(next_gate, next_process_slots, next_task_slots, next_wait_tables[1], syscall.build_wait_request(config.child_wait_handle_slot))
    next_gate = pre_wait_result.gate
    next_process_slots = pre_wait_result.process_slots
    next_task_slots = pre_wait_result.task_slots
    next_wait_tables[1] = pre_wait_result.wait_table
    next_snapshot = child_execution_snapshot(next_snapshot.translation_root, next_snapshot.address_space, next_snapshot.user_frame, next_snapshot.spawn_observation, next_snapshot.sleep_observation, next_snapshot.timer_wake_observation, next_snapshot.wake_ready_queue, pre_wait_result.observation, next_snapshot.exit_wait_observation)
    if syscall.status_score(next_snapshot.pre_exit_wait_observation.status) != 4 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }

    next_process_slots = lifecycle.exit_process(next_process_slots, 2)
    next_task_slots = lifecycle.exit_task(next_task_slots, 2)
    next_wait_tables[1] = capability.mark_wait_handle_exited(next_wait_tables[1], config.child_pid, config.child_exit_code)
    next_ready_queue = state.empty_queue()

    wait_result: syscall.WaitResult = syscall.perform_wait(next_gate, next_process_slots, next_task_slots, next_wait_tables[1], syscall.build_wait_request(config.child_wait_handle_slot))
    next_gate = wait_result.gate
    next_process_slots = wait_result.process_slots
    next_task_slots = wait_result.task_slots
    next_wait_tables[1] = wait_result.wait_table
    next_snapshot = child_execution_snapshot(next_snapshot.translation_root, next_snapshot.address_space, next_snapshot.user_frame, next_snapshot.spawn_observation, next_snapshot.sleep_observation, next_snapshot.timer_wake_observation, next_snapshot.wake_ready_queue, next_snapshot.pre_exit_wait_observation, wait_result.observation)
    next_ready_queue = state.empty_queue()
    if syscall.status_score(next_snapshot.exit_wait_observation.status) != 2 {
        return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 0)
    }

    return child_execution_result(next_init_program_capability, next_gate, next_process_slots, next_task_slots, next_wait_tables, next_ready_queue, next_timer_state, next_interrupts, next_last_interrupt_kind, next_snapshot, 1)
}

func build_scheduler_lifecycle_audit(config: ChildExecutionConfig, init_image: init.InitImage, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, wait_tables: [3]capability.WaitTable, ready_queue: state.ReadyQueue, timer_state: timer.TimerState, snapshot: ChildExecutionSnapshot) sched.LifecycleAudit {
    return sched.LifecycleAudit{ init_pid: config.init_pid, child_pid: config.child_pid, child_tid: config.child_tid, child_asid: config.child_asid, child_translation_root: snapshot.translation_root, child_exit_code: config.child_exit_code, child_wait_handle_slot: config.child_wait_handle_slot, init_entry_pc: init_image.entry_pc, init_stack_top: init_image.stack_top, spawn: snapshot.spawn_observation, pre_exit_wait: snapshot.pre_exit_wait_observation, exit_wait: snapshot.exit_wait_observation, sleep: snapshot.sleep_observation, timer_wake: snapshot.timer_wake_observation, timer_state: timer_state, wake_ready_queue: snapshot.wake_ready_queue, wait_table: wait_tables[1], child_process: process_slots[2], child_task: task_slots[2], child_address_space: snapshot.address_space, child_user_frame: snapshot.user_frame, ready_queue: ready_queue }
}

func empty_init_runtime_snapshot() InitRuntimeSnapshot {
    return InitRuntimeSnapshot{ bootstrap_caps: init.empty_bootstrap_capability_set(), bootstrap_handoff: init.BootstrapHandoffObservation{ owner_pid: 0, authority_count: 0, endpoint_handle_slot: 0, program_capability_slot: 0, program_object_id: 0, ambient_root_visible: 0 }, address_space: address_space.empty_space(), user_frame: address_space.empty_frame() }
}

func init_address_space_result(program_capability: capability.CapabilitySlot, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, ready_queue: state.ReadyQueue, snapshot: InitRuntimeSnapshot, succeeded: u32) InitAddressSpaceResult {
    return InitAddressSpaceResult{ program_capability: program_capability, process_slots: process_slots, task_slots: task_slots, ready_queue: ready_queue, snapshot: snapshot, succeeded: succeeded }
}

func construct_first_user_address_space(init_pid: u32, init_tid: u32, init_asid: u32, init_task_slot: u32, init_root_page_table: usize, init_image: init.InitImage, snapshot: InitRuntimeSnapshot, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot) InitAddressSpaceResult {
    if !init.bootstrap_image_valid(init_image) {
        return init_address_space_result(capability.empty_slot(), process_slots, task_slots, state.empty_queue(), snapshot, 0)
    }
    new_address_space: address_space.AddressSpace = address_space.bootstrap_space(init_asid, init_pid, mmu.bootstrap_translation_root(init_asid, init_root_page_table), init_image.image_base, init_image.image_size, init_image.entry_pc, init_image.stack_base, init_image.stack_size, init_image.stack_top)
    next_snapshot: InitRuntimeSnapshot = InitRuntimeSnapshot{ bootstrap_caps: snapshot.bootstrap_caps, bootstrap_handoff: snapshot.bootstrap_handoff, address_space: new_address_space, user_frame: snapshot.user_frame }
    if address_space.state_score(next_snapshot.address_space.state) != 2 {
        return init_address_space_result(capability.empty_slot(), process_slots, task_slots, state.empty_queue(), next_snapshot, 0)
    }
    next_program_capability: capability.CapabilitySlot = capability.bootstrap_init_program_slot(init_pid)
    next_process_slots: [3]state.ProcessSlot = process_slots
    next_task_slots: [3]state.TaskSlot = task_slots
    next_process_slots[1] = state.init_process_slot(init_pid, init_task_slot, init_asid)
    next_task_slots[1] = state.user_task_slot(init_tid, init_pid, init_asid, init_image.entry_pc, init_image.stack_top)
    next_ready_queue: state.ReadyQueue = state.user_ready_queue(init_tid)
    next_user_frame: address_space.UserEntryFrame = address_space.bootstrap_user_frame(new_address_space, init_tid)
    next_snapshot = InitRuntimeSnapshot{ bootstrap_caps: next_snapshot.bootstrap_caps, bootstrap_handoff: next_snapshot.bootstrap_handoff, address_space: next_snapshot.address_space, user_frame: next_user_frame }
    return init_address_space_result(next_program_capability, next_process_slots, next_task_slots, next_ready_queue, next_snapshot, 1)
}

func bootstrap_capability_handoff_result(snapshot: InitRuntimeSnapshot, succeeded: u32) BootstrapCapabilityHandoffResult {
    return BootstrapCapabilityHandoffResult{ snapshot: snapshot, succeeded: succeeded }
}

func handoff_init_bootstrap_capability_set(init_pid: u32, init_endpoint_handle_slot: u32, program_capability: capability.CapabilitySlot, snapshot: InitRuntimeSnapshot) BootstrapCapabilityHandoffResult {
    new_bootstrap_caps: init.BootstrapCapabilitySet = init.install_bootstrap_capability_set(init_pid, init_endpoint_handle_slot, program_capability)
    new_bootstrap_handoff: init.BootstrapHandoffObservation = init.observe_bootstrap_handoff(new_bootstrap_caps)
    next_snapshot: InitRuntimeSnapshot = InitRuntimeSnapshot{ bootstrap_caps: new_bootstrap_caps, bootstrap_handoff: new_bootstrap_handoff, address_space: snapshot.address_space, user_frame: snapshot.user_frame }
    if next_snapshot.bootstrap_handoff.authority_count != 2 {
        return bootstrap_capability_handoff_result(next_snapshot, 0)
    }
    return bootstrap_capability_handoff_result(next_snapshot, 1)
}

func first_user_entry_transfer_result(kernel: state.KernelDescriptor, snapshot: InitRuntimeSnapshot, succeeded: u32) FirstUserEntryTransferResult {
    return FirstUserEntryTransferResult{ kernel: kernel, snapshot: snapshot, succeeded: succeeded }
}

func transfer_to_first_user_entry(kernel: state.KernelDescriptor, init_pid: u32, init_tid: u32, init_asid: u32, snapshot: InitRuntimeSnapshot) FirstUserEntryTransferResult {
    if !address_space.can_activate(snapshot.address_space) {
        return first_user_entry_transfer_result(kernel, snapshot, 0)
    }
    activated_address_space: address_space.AddressSpace = address_space.activate(snapshot.address_space)
    next_snapshot: InitRuntimeSnapshot = InitRuntimeSnapshot{ bootstrap_caps: snapshot.bootstrap_caps, bootstrap_handoff: snapshot.bootstrap_handoff, address_space: activated_address_space, user_frame: snapshot.user_frame }
    if address_space.state_score(next_snapshot.address_space.state) != 4 {
        return first_user_entry_transfer_result(kernel, next_snapshot, 0)
    }
    next_kernel: state.KernelDescriptor = state.start_user_entry(kernel, init_pid, init_tid, init_asid)
    return first_user_entry_transfer_result(next_kernel, next_snapshot, 1)
}

func bootstrap_endpoint_result(handle_tables: [3]capability.HandleTable, endpoints: ipc.EndpointTable, delivered_message: ipc.KernelMessage) EndpointBootstrapResult {
    return EndpointBootstrapResult{ handle_tables: handle_tables, endpoints: endpoints, delivered_message: delivered_message }
}

func bootstrap_endpoint_handle_core(boot_pid: u32, init_pid: u32, init_endpoint_id: u32, init_endpoint_handle_slot: u32, handle_tables: [3]capability.HandleTable, endpoints: ipc.EndpointTable) EndpointBootstrapResult {
    next_handle_tables: [3]capability.HandleTable = handle_tables
    next_endpoints: ipc.EndpointTable = endpoints

    next_endpoints = ipc.install_endpoint(next_endpoints, init_pid, init_endpoint_id)
    next_handle_tables[1] = capability.handle_table_for_owner(init_pid)
    next_handle_tables[1] = capability.install_endpoint_handle(next_handle_tables[1], init_endpoint_handle_slot, init_endpoint_id, 7)
    next_endpoints = ipc.enqueue_message(next_endpoints, 0, ipc.bootstrap_init_message(boot_pid, init_endpoint_id))
    delivered_message: ipc.KernelMessage = ipc.mark_delivered(ipc.peek_head_message(next_endpoints, 0))
    next_endpoints = ipc.consume_head_message(next_endpoints, 0)

    return bootstrap_endpoint_result(next_handle_tables, next_endpoints, delivered_message)
}

func syscall_byte_ipc_result(gate: syscall.SyscallGate, handle_tables: [3]capability.HandleTable, endpoints: ipc.EndpointTable, observation: syscall.ReceiveObservation, succeeded: u32) SyscallByteIpcResult {
    return SyscallByteIpcResult{ gate: gate, handle_tables: handle_tables, endpoints: endpoints, observation: observation, succeeded: succeeded }
}

func execute_syscall_byte_ipc(init_pid: u32, init_endpoint_handle_slot: u32, gate: syscall.SyscallGate, handle_tables: [3]capability.HandleTable, endpoints: ipc.EndpointTable) SyscallByteIpcResult {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = 83
    payload[1] = 89
    payload[2] = 83
    payload[3] = 67

    next_gate: syscall.SyscallGate = syscall.open_gate(gate)
    next_handle_tables: [3]capability.HandleTable = handle_tables
    next_endpoints: ipc.EndpointTable = endpoints

    send_result: syscall.SendResult = syscall.perform_send(next_gate, next_handle_tables[1], next_endpoints, init_pid, syscall.build_send_request(init_endpoint_handle_slot, 4, payload))
    next_gate = send_result.gate
    next_handle_tables[1] = send_result.handle_table
    next_endpoints = send_result.endpoints
    if syscall.status_score(send_result.status) != 2 {
        return syscall_byte_ipc_result(next_gate, next_handle_tables, next_endpoints, syscall.empty_receive_observation(), 0)
    }

    receive_result: syscall.ReceiveResult = syscall.perform_receive(next_gate, next_handle_tables[1], next_endpoints, syscall.build_receive_request(init_endpoint_handle_slot))
    next_gate = receive_result.gate
    next_handle_tables[1] = receive_result.handle_table
    next_endpoints = receive_result.endpoints
    observation: syscall.ReceiveObservation = receive_result.observation
    if syscall.status_score(observation.status) != 2 {
        return syscall_byte_ipc_result(next_gate, next_handle_tables, next_endpoints, observation, 0)
    }

    would_block_result: syscall.ReceiveResult = syscall.perform_receive(next_gate, next_handle_tables[1], next_endpoints, syscall.build_receive_request(init_endpoint_handle_slot))
    next_gate = would_block_result.gate
    next_handle_tables[1] = would_block_result.handle_table
    next_endpoints = would_block_result.endpoints
    if syscall.status_score(would_block_result.observation.status) != 4 {
        return syscall_byte_ipc_result(next_gate, next_handle_tables, next_endpoints, observation, 0)
    }
    if syscall.block_reason_score(would_block_result.observation.block_reason) != 4 {
        return syscall_byte_ipc_result(next_gate, next_handle_tables, next_endpoints, observation, 0)
    }
    return syscall_byte_ipc_result(next_gate, next_handle_tables, next_endpoints, observation, 1)
}

func transfer_endpoint_seed_result(handle_tables: [3]capability.HandleTable, endpoints: ipc.EndpointTable, succeeded: u32) TransferEndpointSeedResult {
    return TransferEndpointSeedResult{ handle_tables: handle_tables, endpoints: endpoints, succeeded: succeeded }
}

func seed_transfer_endpoint_handle(init_pid: u32, transfer_endpoint_id: u32, transfer_source_handle_slot: u32, handle_tables: [3]capability.HandleTable, endpoints: ipc.EndpointTable) TransferEndpointSeedResult {
    next_handle_tables: [3]capability.HandleTable = handle_tables
    next_endpoints: ipc.EndpointTable = endpoints

    next_endpoints = ipc.install_endpoint(next_endpoints, init_pid, transfer_endpoint_id)
    next_handle_tables[1] = capability.install_endpoint_handle(next_handle_tables[1], transfer_source_handle_slot, transfer_endpoint_id, 7)

    if next_endpoints.count != 2 {
        return transfer_endpoint_seed_result(next_handle_tables, next_endpoints, 0)
    }
    if next_endpoints.slots[1].endpoint_id != transfer_endpoint_id {
        return transfer_endpoint_seed_result(next_handle_tables, next_endpoints, 0)
    }
    if next_endpoints.slots[1].owner_pid != init_pid {
        return transfer_endpoint_seed_result(next_handle_tables, next_endpoints, 0)
    }
    if next_endpoints.slots[1].active != 1 {
        return transfer_endpoint_seed_result(next_handle_tables, next_endpoints, 0)
    }
    if next_handle_tables[1].count != 2 {
        return transfer_endpoint_seed_result(next_handle_tables, next_endpoints, 0)
    }
    if capability.find_endpoint_for_handle(next_handle_tables[1], transfer_source_handle_slot) != transfer_endpoint_id {
        return transfer_endpoint_seed_result(next_handle_tables, next_endpoints, 0)
    }
    if capability.find_rights_for_handle(next_handle_tables[1], transfer_source_handle_slot) != 7 {
        return transfer_endpoint_seed_result(next_handle_tables, next_endpoints, 0)
    }

    return transfer_endpoint_seed_result(next_handle_tables, next_endpoints, 1)
}

func capability_carrying_ipc_transfer_result(gate: syscall.SyscallGate, handle_tables: [3]capability.HandleTable, endpoints: ipc.EndpointTable, attached_receive_observation: syscall.ReceiveObservation, transferred_handle_use_observation: syscall.ReceiveObservation, succeeded: u32) CapabilityCarryingIpcTransferResult {
    return CapabilityCarryingIpcTransferResult{ gate: gate, handle_tables: handle_tables, endpoints: endpoints, attached_receive_observation: attached_receive_observation, transferred_handle_use_observation: transferred_handle_use_observation, succeeded: succeeded }
}

func execute_capability_carrying_ipc_transfer(init_pid: u32, init_endpoint_handle_slot: u32, transfer_source_handle_slot: u32, transfer_received_handle_slot: u32, transfer_endpoint_id: u32, gate: syscall.SyscallGate, handle_tables: [3]capability.HandleTable, endpoints: ipc.EndpointTable) CapabilityCarryingIpcTransferResult {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = 67
    payload[1] = 65
    payload[2] = 80
    payload[3] = 83

    next_gate: syscall.SyscallGate = gate
    next_handle_tables: [3]capability.HandleTable = handle_tables
    next_endpoints: ipc.EndpointTable = endpoints
    attached_receive_observation: syscall.ReceiveObservation = syscall.empty_receive_observation()
    transferred_handle_use_observation: syscall.ReceiveObservation = syscall.empty_receive_observation()

    transfer_send_result: syscall.SendResult = syscall.perform_send(next_gate, next_handle_tables[1], next_endpoints, init_pid, syscall.build_transfer_send_request(init_endpoint_handle_slot, 4, payload, transfer_source_handle_slot))
    next_gate = transfer_send_result.gate
    next_handle_tables[1] = transfer_send_result.handle_table
    next_endpoints = transfer_send_result.endpoints
    if syscall.status_score(transfer_send_result.status) != 2 {
        return capability_carrying_ipc_transfer_result(next_gate, next_handle_tables, next_endpoints, attached_receive_observation, transferred_handle_use_observation, 0)
    }
    if capability.find_endpoint_for_handle(next_handle_tables[1], transfer_source_handle_slot) != 0 {
        return capability_carrying_ipc_transfer_result(next_gate, next_handle_tables, next_endpoints, attached_receive_observation, transferred_handle_use_observation, 0)
    }

    transfer_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_gate, next_handle_tables[1], next_endpoints, syscall.build_transfer_receive_request(init_endpoint_handle_slot, transfer_received_handle_slot))
    next_gate = transfer_receive_result.gate
    next_handle_tables[1] = transfer_receive_result.handle_table
    next_endpoints = transfer_receive_result.endpoints
    attached_receive_observation = transfer_receive_result.observation
    if syscall.status_score(attached_receive_observation.status) != 2 {
        return capability_carrying_ipc_transfer_result(next_gate, next_handle_tables, next_endpoints, attached_receive_observation, transferred_handle_use_observation, 0)
    }
    if capability.find_endpoint_for_handle(next_handle_tables[1], transfer_received_handle_slot) != transfer_endpoint_id {
        return capability_carrying_ipc_transfer_result(next_gate, next_handle_tables, next_endpoints, attached_receive_observation, transferred_handle_use_observation, 0)
    }

    follow_payload: [4]u8 = ipc.zero_payload()
    follow_payload[0] = 77
    follow_payload[1] = 79
    follow_payload[2] = 86
    follow_payload[3] = 69

    follow_send_result: syscall.SendResult = syscall.perform_send(next_gate, next_handle_tables[1], next_endpoints, init_pid, syscall.build_send_request(transfer_received_handle_slot, 4, follow_payload))
    next_gate = follow_send_result.gate
    next_handle_tables[1] = follow_send_result.handle_table
    next_endpoints = follow_send_result.endpoints
    if syscall.status_score(follow_send_result.status) != 2 {
        return capability_carrying_ipc_transfer_result(next_gate, next_handle_tables, next_endpoints, attached_receive_observation, transferred_handle_use_observation, 0)
    }

    follow_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_gate, next_handle_tables[1], next_endpoints, syscall.build_receive_request(transfer_received_handle_slot))
    next_gate = follow_receive_result.gate
    next_handle_tables[1] = follow_receive_result.handle_table
    next_endpoints = follow_receive_result.endpoints
    transferred_handle_use_observation = follow_receive_result.observation
    if syscall.status_score(transferred_handle_use_observation.status) != 2 {
        return capability_carrying_ipc_transfer_result(next_gate, next_handle_tables, next_endpoints, attached_receive_observation, transferred_handle_use_observation, 0)
    }

    return capability_carrying_ipc_transfer_result(next_gate, next_handle_tables, next_endpoints, attached_receive_observation, transferred_handle_use_observation, 1)
}