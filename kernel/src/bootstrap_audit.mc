import address_space
import capability
import debug
import echo_service
import endpoint
import init
import log_service
import mmu
import state
import syscall
import timer
import transfer_service

struct LogServicePhaseAudit {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    ack_observation: syscall.ReceiveObservation
    service_state: log_service.LogServiceState
    handshake: log_service.LogHandshakeObservation
    wait_observation: syscall.WaitObservation
    wait_table: capability.WaitTable
    child_handle_table: capability.HandleTable
    ready_queue: state.ReadyQueue
    child_process: state.ProcessSlot
    child_task: state.TaskSlot
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    init_endpoint_id: u32
    wait_handle_slot: u32
    endpoint_handle_slot: u32
    request_byte: u8
    exit_code: i32
}

struct EchoServicePhaseAudit {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    reply_observation: syscall.ReceiveObservation
    service_state: echo_service.EchoServiceState
    exchange: echo_service.EchoExchangeObservation
    wait_observation: syscall.WaitObservation
    wait_table: capability.WaitTable
    child_handle_table: capability.HandleTable
    ready_queue: state.ReadyQueue
    child_process: state.ProcessSlot
    child_task: state.TaskSlot
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    init_endpoint_id: u32
    wait_handle_slot: u32
    endpoint_handle_slot: u32
    request_byte0: u8
    request_byte1: u8
    exit_code: i32
}

struct TransferServicePhaseAudit {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    spawn_observation: syscall.SpawnObservation
    grant_observation: syscall.ReceiveObservation
    emit_observation: syscall.ReceiveObservation
    service_state: transfer_service.TransferServiceState
    transfer: transfer_service.TransferObservation
    wait_observation: syscall.WaitObservation
    init_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    child_handle_table: capability.HandleTable
    ready_queue: state.ReadyQueue
    child_process: state.ProcessSlot
    child_task: state.TaskSlot
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    init_endpoint_id: u32
    transfer_endpoint_id: u32
    wait_handle_slot: u32
    source_handle_slot: u32
    control_handle_slot: u32
    init_received_handle_slot: u32
    service_received_handle_slot: u32
    grant_byte0: u8
    grant_byte1: u8
    grant_byte2: u8
    grant_byte3: u8
    exit_code: i32
}

struct Phase108ProgramCapContractInputs {
    init_pid: u32
    log_service_program_object_id: u32
    echo_service_program_object_id: u32
    transfer_service_program_object_id: u32
    log_service_wait_handle_slot: u32
    echo_service_wait_handle_slot: u32
    transfer_service_wait_handle_slot: u32
    log_service_exit_code: i32
    echo_service_exit_code: i32
    transfer_service_exit_code: i32
    bootstrap_program_capability: capability.CapabilitySlot
    log_service_program_capability: capability.CapabilitySlot
    echo_service_program_capability: capability.CapabilitySlot
    transfer_service_program_capability: capability.CapabilitySlot
    log_service_spawn: syscall.SpawnObservation
    echo_service_spawn: syscall.SpawnObservation
    transfer_service_spawn: syscall.SpawnObservation
    log_service_wait: syscall.WaitObservation
    echo_service_wait: syscall.WaitObservation
    transfer_service_wait: syscall.WaitObservation
}

struct RunningKernelSliceAuditInputs {
    kernel: state.KernelDescriptor
    init_pid: u32
    init_tid: u32
    init_asid: u32
    child_tid: u32
    child_exit_code: i32
    transfer_endpoint_id: u32
    log_service_request_byte: u8
    echo_service_request_byte0: u8
    echo_service_request_byte1: u8
    log_service_exit_code: i32
    echo_service_exit_code: i32
    transfer_service_exit_code: i32
    init_bootstrap_handoff: init.BootstrapHandoffObservation
    receive_observation: syscall.ReceiveObservation
    attached_receive_observation: syscall.ReceiveObservation
    transferred_handle_use_observation: syscall.ReceiveObservation
    pre_exit_wait_observation: syscall.WaitObservation
    exit_wait_observation: syscall.WaitObservation
    sleep_observation: syscall.SleepObservation
    timer_wake_observation: timer.TimerWakeObservation
    log_service_handshake: log_service.LogHandshakeObservation
    log_service_wait_observation: syscall.WaitObservation
    echo_service_exchange: echo_service.EchoExchangeObservation
    echo_service_wait_observation: syscall.WaitObservation
    transfer_service_transfer: transfer_service.TransferObservation
    transfer_service_wait_observation: syscall.WaitObservation
    phase104_contract_hardened: u32
    phase108_contract_hardened: u32
    init_process: state.ProcessSlot
    init_task: state.TaskSlot
    init_user_frame: address_space.UserEntryFrame
    boot_log_append_failed: u32
}

struct Phase117MultiServiceBringUpAuditInputs {
    running_slice: debug.RunningKernelSliceAudit
    init_endpoint_id: u32
    transfer_endpoint_id: u32
    log_service_program_capability: capability.CapabilitySlot
    echo_service_program_capability: capability.CapabilitySlot
    transfer_service_program_capability: capability.CapabilitySlot
    log_service_spawn: syscall.SpawnObservation
    echo_service_spawn: syscall.SpawnObservation
    transfer_service_spawn: syscall.SpawnObservation
    log_service_wait: syscall.WaitObservation
    echo_service_wait: syscall.WaitObservation
    transfer_service_wait: syscall.WaitObservation
    log_service_handshake: log_service.LogHandshakeObservation
    echo_service_exchange: echo_service.EchoExchangeObservation
    transfer_service_transfer: transfer_service.TransferObservation
}

struct Phase118DelegatedRequestReplyAuditInputs {
    phase117: debug.Phase117MultiServiceBringUpAudit
    transfer_service_transfer: transfer_service.TransferObservation
    invalidated_source_send_status: syscall.SyscallStatus
    invalidated_source_handle_slot: u32
    retained_receive_handle_slot: u32
    retained_receive_endpoint_id: u32
}

struct Phase119NamespacePressureAuditInputs {
    phase118: debug.Phase118DelegatedRequestReplyAudit
    directory_owner_pid: u32
    directory_entry_count: usize
    log_service_key: u32
    echo_service_key: u32
    transfer_service_key: u32
    shared_directory_endpoint_id: u32
    log_service_program_slot: u32
    echo_service_program_slot: u32
    transfer_service_program_slot: u32
    log_service_program_object_id: u32
    echo_service_program_object_id: u32
    transfer_service_program_object_id: u32
    log_service_wait_handle_slot: u32
    echo_service_wait_handle_slot: u32
    transfer_service_wait_handle_slot: u32
    dynamic_namespace_visible: u32
}

struct Phase120RunningSystemSupportAuditInputs {
    phase119: debug.Phase119NamespacePressureAudit
    service_policy_owner_pid: u32
    running_service_count: usize
    fixed_directory_count: usize
    shared_control_endpoint_id: u32
    retained_reply_endpoint_id: u32
    program_capability_count: usize
    wait_handle_count: usize
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct Phase121KernelImageContractAuditInputs {
    phase120: debug.Phase120RunningSystemSupportAudit
    kernel_manifest_visible: u32
    kernel_target_visible: u32
    kernel_runtime_startup_visible: u32
    bootstrap_target_family_visible: u32
    emitted_image_input_visible: u32
    linked_kernel_executable_visible: u32
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct Phase122TargetSurfaceAuditInputs {
    phase121: debug.Phase121KernelImageContractAudit
    kernel_target_visible: u32
    kernel_runtime_startup_visible: u32
    bootstrap_target_family_visible: u32
    bootstrap_target_family_only_visible: u32
    broader_target_family_visible: u32
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct BootstrapLayoutAudit {
    init_image: init.InitImage
    init_root_page_table: usize
}

struct EndpointCapabilityAudit {
    init_pid: u32
    init_endpoint_id: u32
    transfer_endpoint_id: u32
    child_wait_handle_slot: u32
}

struct StateHardeningAudit {
    boot_tid: u32
    boot_pid: u32
    boot_entry_pc: usize
    boot_stack_top: usize
    child_tid: u32
    child_pid: u32
    child_asid: u32
    init_image: init.InitImage
    arch_actor: u32
}

struct SyscallHardeningAudit {
    init_pid: u32
    init_endpoint_handle_slot: u32
    init_endpoint_id: u32
    child_wait_handle_slot: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    child_root_page_table: usize
    boot_pid: u32
    boot_tid: u32
    boot_task_slot: u32
    boot_entry_pc: usize
    boot_stack_top: usize
    init_image: init.InitImage
    transfer_source_handle_slot: u32
}

struct Phase104HardeningAudit {
    boot_log_append_failed: u32
    timer_hardened: u32
    bootstrap_layout: BootstrapLayoutAudit
    endpoint_capability: EndpointCapabilityAudit
    state_hardening: StateHardeningAudit
    syscall_hardening: SyscallHardeningAudit
}

func validate_timer_hardening_contracts() bool {
    premature_timer: timer.TimerState = timer.empty_timer_state()
    premature_timer = timer.arm_sleep(premature_timer, 41, 5)
    premature_consumed: timer.TimerState = timer.consume_wake(premature_timer, 41)
    if premature_consumed.count != 1 {
        return false
    }
    if timer.find_sleep_index(premature_consumed, 41) != 0 {
        return false
    }
    if timer.sleep_state_score(premature_consumed.sleepers[0].state) != 2 {
        return false
    }

    dual_timer: timer.TimerState = timer.empty_timer_state()
    dual_timer = timer.arm_sleep(dual_timer, 51, 1)
    dual_timer = timer.arm_sleep(dual_timer, 52, 1)
    dual_timer = timer.advance_tick(dual_timer, 1)
    if timer.has_fired_sleeper(dual_timer) == 0 {
        return false
    }
    first_wake: timer.TimerWakeResult = timer.wake_fired_sleepers(dual_timer)
    if first_wake.observation.task_id != 51 {
        return false
    }
    dual_timer = timer.consume_wake(first_wake.timer_state, first_wake.observation.task_id)
    if dual_timer.count != 1 {
        return false
    }
    if timer.has_fired_sleeper(dual_timer) == 0 {
        return false
    }
    second_wake: timer.TimerWakeResult = timer.wake_fired_sleepers(dual_timer)
    if second_wake.observation.task_id != 52 {
        return false
    }
    if second_wake.observation.wake_count != 2 {
        return false
    }
    dual_timer = timer.consume_wake(second_wake.timer_state, second_wake.observation.task_id)
    if dual_timer.count != 0 {
        return false
    }
    return timer.has_fired_sleeper(dual_timer) == 0
}

func validate_bootstrap_layout_contracts(audit: BootstrapLayoutAudit) bool {
    if !init.bootstrap_image_valid(audit.init_image) {
        return false
    }
    invalid_image: init.InitImage = init.InitImage{ image_id: 9, image_base: 65536, image_size: 4096, entry_pc: 70000, stack_base: 67584, stack_top: 71680, stack_size: 4096 }
    if init.bootstrap_image_valid(invalid_image) {
        return false
    }
    invalid_space: address_space.AddressSpace = address_space.bootstrap_space(3, 7, mmu.bootstrap_translation_root(3, audit.init_root_page_table), invalid_image.image_base, invalid_image.image_size, invalid_image.entry_pc, invalid_image.stack_base, invalid_image.stack_size, invalid_image.stack_top)
    if address_space.state_score(invalid_space.state) != 1 {
        return false
    }
    if address_space.can_activate(address_space.empty_space()) {
        return false
    }
    still_empty: address_space.AddressSpace = address_space.activate(address_space.empty_space())
    return address_space.state_score(still_empty.state) == 1
}

func validate_endpoint_and_capability_contracts(audit: EndpointCapabilityAudit) bool {
    payload: [4]u8 = endpoint.zero_payload()
    local_endpoints: endpoint.EndpointTable = endpoint.empty_table()
    local_endpoints = endpoint.install_endpoint(local_endpoints, audit.init_pid, audit.init_endpoint_id)
    enqueue_one: endpoint.EndpointTable = endpoint.enqueue_message(local_endpoints, 0, endpoint.byte_message(2, audit.init_pid, audit.init_endpoint_id, 0, payload))
    if !endpoint.enqueue_succeeded(local_endpoints, enqueue_one, 0) {
        return false
    }
    enqueue_two: endpoint.EndpointTable = endpoint.enqueue_message(enqueue_one, 0, endpoint.byte_message(3, audit.init_pid, audit.init_endpoint_id, 0, payload))
    if !endpoint.enqueue_succeeded(enqueue_one, enqueue_two, 0) {
        return false
    }
    enqueue_three: endpoint.EndpointTable = endpoint.enqueue_message(enqueue_two, 0, endpoint.byte_message(4, audit.init_pid, audit.init_endpoint_id, 0, payload))
    if endpoint.enqueue_succeeded(enqueue_two, enqueue_three, 0) {
        return false
    }

    local_handles: capability.HandleTable = capability.handle_table_for_owner(audit.init_pid)
    installed_handle: capability.HandleTable = capability.install_endpoint_handle(local_handles, 1, audit.init_endpoint_id, 7)
    if !capability.handle_install_succeeded(local_handles, installed_handle, 1) {
        return false
    }
    duplicate_handle: capability.HandleTable = capability.install_endpoint_handle(installed_handle, 1, audit.transfer_endpoint_id, 7)
    if capability.handle_install_succeeded(installed_handle, duplicate_handle, 1) {
        return false
    }
    invalid_rights_handle: capability.HandleTable = capability.install_endpoint_handle(local_handles, 2, audit.transfer_endpoint_id, 8)
    if capability.handle_install_succeeded(local_handles, invalid_rights_handle, 2) {
        return false
    }
    if capability.attenuate_endpoint_rights(8) != 0 {
        return false
    }
    if capability.find_transfer_rights_for_handle(installed_handle, 1) != 7 {
        return false
    }

    local_waits: capability.WaitTable = capability.wait_table_for_owner(audit.init_pid)
    installed_wait: capability.WaitTable = capability.install_wait_handle(local_waits, audit.child_wait_handle_slot, 3)
    if !capability.wait_install_succeeded(local_waits, installed_wait, audit.child_wait_handle_slot) {
        return false
    }
    duplicate_wait: capability.WaitTable = capability.install_wait_handle(installed_wait, audit.child_wait_handle_slot, 3)
    return !capability.wait_install_succeeded(installed_wait, duplicate_wait, audit.child_wait_handle_slot)
}

func validate_state_hardening_contracts(audit: StateHardeningAudit) bool {
    boot_task: state.TaskSlot = state.boot_task_slot(audit.boot_tid, audit.boot_pid, audit.boot_entry_pc, audit.boot_stack_top)
    if state.can_block_task(boot_task) {
        return false
    }
    if state.task_state_score(state.blocked_task_slot(boot_task).state) != 2 {
        return false
    }
    ready_task: state.TaskSlot = state.user_task_slot(audit.child_tid, audit.child_pid, audit.child_asid, audit.init_image.entry_pc, audit.init_image.stack_top)
    if !state.can_block_task(ready_task) {
        return false
    }
    if state.task_state_score(state.blocked_task_slot(ready_task).state) != 8 {
        return false
    }

    local_log: state.BootLog = state.empty_log()
    append0: state.BootLogAppendResult = state.append_record(local_log, state.BootStage.Reset, audit.arch_actor, 1)
    append1: state.BootLogAppendResult = state.append_record(append0.log, state.BootStage.TablesSeeded, audit.arch_actor, 2)
    append2: state.BootLogAppendResult = state.append_record(append1.log, state.BootStage.AddressSpaceReady, audit.arch_actor, 3)
    append3: state.BootLogAppendResult = state.append_record(append2.log, state.BootStage.UserEntryReady, audit.arch_actor, 4)
    append4: state.BootLogAppendResult = state.append_record(append3.log, state.BootStage.MarkerEmitted, audit.arch_actor, 5)
    append5: state.BootLogAppendResult = state.append_record(append4.log, state.BootStage.Halted, audit.arch_actor, 6)
    overflow: state.BootLogAppendResult = state.append_record(append5.log, state.BootStage.Halted, audit.arch_actor, 7)
    if overflow.appended != 0 {
        return false
    }
    return overflow.log.count == 6
}

func validate_syscall_contract_hardening(audit: SyscallHardeningAudit) bool {
    local_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    payload: [4]u8 = endpoint.zero_payload()
    payload[0] = 81
    payload[1] = 85
    payload[2] = 69
    payload[3] = 85

    local_handles: capability.HandleTable = capability.handle_table_for_owner(audit.init_pid)
    local_handles = capability.install_endpoint_handle(local_handles, audit.init_endpoint_handle_slot, audit.init_endpoint_id, 5)
    local_endpoints: endpoint.EndpointTable = endpoint.empty_table()
    local_endpoints = endpoint.install_endpoint(local_endpoints, audit.init_pid, audit.init_endpoint_id)

    send_one: syscall.SendResult = syscall.perform_send(local_gate, local_handles, local_endpoints, audit.init_pid, syscall.build_send_request(audit.init_endpoint_handle_slot, 4, payload))
    send_two: syscall.SendResult = syscall.perform_send(send_one.gate, send_one.handle_table, send_one.endpoints, audit.init_pid, syscall.build_send_request(audit.init_endpoint_handle_slot, 4, payload))
    send_block: syscall.SendResult = syscall.perform_send(send_two.gate, send_two.handle_table, send_two.endpoints, audit.init_pid, syscall.build_send_request(audit.init_endpoint_handle_slot, 4, payload))
    if syscall.status_score(send_block.status) != 4 {
        return false
    }
    if syscall.block_reason_score(send_block.block_reason) != 2 {
        return false
    }
    if send_block.endpoints.slots[0].queued_messages != 2 {
        return false
    }

    receive_one: syscall.ReceiveResult = syscall.perform_receive(send_block.gate, send_block.handle_table, send_block.endpoints, syscall.build_receive_request(audit.init_endpoint_handle_slot))
    receive_two: syscall.ReceiveResult = syscall.perform_receive(receive_one.gate, receive_one.handle_table, receive_one.endpoints, syscall.build_receive_request(audit.init_endpoint_handle_slot))
    receive_block: syscall.ReceiveResult = syscall.perform_receive(receive_two.gate, receive_two.handle_table, receive_two.endpoints, syscall.build_receive_request(audit.init_endpoint_handle_slot))
    if syscall.status_score(receive_block.observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(receive_block.observation.block_reason) != 4 {
        return false
    }

    local_waits: capability.WaitTable = capability.wait_table_for_owner(audit.init_pid)
    local_waits = capability.install_wait_handle(local_waits, audit.child_wait_handle_slot, audit.child_pid)
    wait_block: syscall.WaitResult = syscall.perform_wait(local_gate, state.zero_process_slots(), state.zero_task_slots(), local_waits, syscall.build_wait_request(audit.child_wait_handle_slot))
    if syscall.status_score(wait_block.observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(wait_block.observation.block_reason) != 8 {
        return false
    }

    sleep_tasks: [3]state.TaskSlot = state.zero_task_slots()
    sleep_tasks[1] = state.user_task_slot(audit.child_tid, audit.child_pid, audit.child_asid, audit.init_image.entry_pc, audit.init_image.stack_top)
    sleep_block: syscall.SleepResult = syscall.perform_sleep(local_gate, sleep_tasks, timer.empty_timer_state(), syscall.build_sleep_request(1, 2))
    if syscall.status_score(sleep_block.observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(sleep_block.observation.block_reason) != 16 {
        return false
    }

    spawn_process_slots: [3]state.ProcessSlot = state.zero_process_slots()
    spawn_process_slots[0] = state.boot_process_slot(audit.boot_pid, audit.boot_task_slot)
    spawn_process_slots[2] = state.init_process_slot(9, 2, 9)
    spawn_task_slots: [3]state.TaskSlot = state.zero_task_slots()
    spawn_task_slots[0] = state.boot_task_slot(audit.boot_tid, audit.boot_pid, audit.boot_entry_pc, audit.boot_stack_top)
    spawn_task_slots[2] = state.user_task_slot(9, 9, 9, audit.init_image.entry_pc, audit.init_image.stack_top)
    spawn_waits: capability.WaitTable = capability.wait_table_for_owner(audit.init_pid)
    local_program_capability: capability.CapabilitySlot = capability.bootstrap_init_program_slot(audit.init_pid)
    local_spawn: syscall.SpawnResult = syscall.perform_spawn(local_gate, local_program_capability, spawn_process_slots, spawn_task_slots, spawn_waits, audit.init_image, syscall.build_spawn_request(audit.child_wait_handle_slot), audit.child_pid, audit.child_tid, audit.child_asid, mmu.bootstrap_translation_root(audit.child_asid, audit.child_root_page_table))
    if syscall.status_score(local_spawn.observation.status) != 2 {
        return false
    }
    if local_spawn.process_slots[1].pid != audit.child_pid {
        return false
    }
    if local_spawn.process_slots[2].pid != 9 {
        return false
    }
    if local_spawn.task_slots[1].tid != audit.child_tid {
        return false
    }
    if local_spawn.task_slots[2].tid != 9 {
        return false
    }
    if !capability.wait_handle_installed(local_spawn.wait_table, audit.child_wait_handle_slot) {
        return false
    }

    transfer_handles: capability.HandleTable = capability.handle_table_for_owner(audit.init_pid)
    transfer_handles = capability.install_endpoint_handle(transfer_handles, audit.init_endpoint_handle_slot, audit.init_endpoint_id, 5)
    transfer_handles = capability.install_endpoint_handle(transfer_handles, audit.transfer_source_handle_slot, 2, 5)
    transfer_endpoints: endpoint.EndpointTable = endpoint.empty_table()
    transfer_endpoints = endpoint.install_endpoint(transfer_endpoints, audit.init_pid, audit.init_endpoint_id)
    transfer_send: syscall.SendResult = syscall.perform_send(local_gate, transfer_handles, transfer_endpoints, audit.init_pid, syscall.build_transfer_send_request(audit.init_endpoint_handle_slot, 4, payload, audit.transfer_source_handle_slot))
    if capability.find_endpoint_for_handle(transfer_send.handle_table, audit.transfer_source_handle_slot) != 0 {
        return false
    }
    blocked_receive: syscall.ReceiveResult = syscall.perform_receive(transfer_send.gate, transfer_send.handle_table, transfer_send.endpoints, syscall.build_transfer_receive_request(audit.init_endpoint_handle_slot, audit.init_endpoint_handle_slot))
    if syscall.status_score(blocked_receive.observation.status) != 8 {
        return false
    }
    if blocked_receive.endpoints.slots[0].queued_messages != 1 {
        return false
    }
    return capability.find_endpoint_for_handle(blocked_receive.handle_table, audit.transfer_source_handle_slot) == 0
}

func validate_phase104_contract_hardening(audit: Phase104HardeningAudit) bool {
    if audit.boot_log_append_failed != 0 {
        return false
    }
    if audit.timer_hardened == 0 {
        return false
    }
    if !validate_bootstrap_layout_contracts(audit.bootstrap_layout) {
        return false
    }
    if !validate_endpoint_and_capability_contracts(audit.endpoint_capability) {
        return false
    }
    if !validate_state_hardening_contracts(audit.state_hardening) {
        return false
    }
    return validate_syscall_contract_hardening(audit.syscall_hardening)
}

func validate_phase105_log_service_handshake(audit: LogServicePhaseAudit) bool {
    if capability.kind_score(audit.program_capability.kind) != 1 {
        return false
    }
    if syscall.id_score(audit.gate.last_id) != 16 {
        return false
    }
    if syscall.status_score(audit.gate.last_status) != 2 {
        return false
    }
    if audit.gate.send_count != 5 {
        return false
    }
    if audit.gate.receive_count != 5 {
        return false
    }
    if audit.spawn_observation.child_pid != audit.child_pid {
        return false
    }
    if audit.spawn_observation.child_tid != audit.child_tid {
        return false
    }
    if audit.spawn_observation.child_asid != audit.child_asid {
        return false
    }
    if audit.spawn_observation.wait_handle_slot != audit.wait_handle_slot {
        return false
    }
    if syscall.status_score(audit.spawn_observation.status) != 2 {
        return false
    }
    if audit.receive_observation.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.receive_observation.source_pid != audit.init_pid {
        return false
    }
    if audit.receive_observation.payload_len != 1 {
        return false
    }
    if audit.receive_observation.payload[0] != audit.request_byte {
        return false
    }
    if audit.ack_observation.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.ack_observation.source_pid != audit.child_pid {
        return false
    }
    if audit.ack_observation.payload_len != 1 {
        return false
    }
    if audit.ack_observation.payload[0] != 33 {
        return false
    }
    if audit.service_state.owner_pid != audit.child_pid {
        return false
    }
    if audit.service_state.endpoint_handle_slot != audit.endpoint_handle_slot {
        return false
    }
    if audit.service_state.handled_request_count != 1 {
        return false
    }
    if audit.service_state.ack_count != 1 {
        return false
    }
    if audit.service_state.last_client_pid != audit.init_pid {
        return false
    }
    if audit.service_state.last_endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.service_state.last_request_len != 1 {
        return false
    }
    if audit.service_state.last_request_byte != audit.request_byte {
        return false
    }
    if audit.service_state.last_ack_byte != 33 {
        return false
    }
    if audit.handshake.service_pid != audit.child_pid {
        return false
    }
    if audit.handshake.client_pid != audit.init_pid {
        return false
    }
    if audit.handshake.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if log_service.tag_score(audit.handshake.tag) != 4 {
        return false
    }
    if audit.handshake.request_len != 1 {
        return false
    }
    if audit.handshake.request_byte != audit.request_byte {
        return false
    }
    if audit.handshake.ack_byte != 33 {
        return false
    }
    if audit.handshake.request_count != 1 {
        return false
    }
    if audit.handshake.ack_count != 1 {
        return false
    }
    if syscall.status_score(audit.wait_observation.status) != 2 {
        return false
    }
    if audit.wait_observation.child_pid != audit.child_pid {
        return false
    }
    if audit.wait_observation.exit_code != audit.exit_code {
        return false
    }
    if audit.wait_observation.wait_handle_slot != audit.wait_handle_slot {
        return false
    }
    if audit.wait_table.count != 0 {
        return false
    }
    if capability.find_child_for_wait_handle(audit.wait_table, audit.wait_handle_slot) != 0 {
        return false
    }
    if audit.child_handle_table.count != 0 {
        return false
    }
    if audit.ready_queue.count != 0 {
        return false
    }
    if state.process_state_score(audit.child_process.state) != 1 {
        return false
    }
    if state.task_state_score(audit.child_task.state) != 1 {
        return false
    }
    if address_space.state_score(audit.child_address_space.state) != 2 {
        return false
    }
    if audit.child_address_space.owner_pid != audit.child_pid {
        return false
    }
    return audit.child_user_frame.task_id == audit.child_tid
}

func validate_phase106_echo_service_request_reply(audit: EchoServicePhaseAudit) bool {
    if capability.kind_score(audit.program_capability.kind) != 1 {
        return false
    }
    if syscall.id_score(audit.gate.last_id) != 16 {
        return false
    }
    if syscall.status_score(audit.gate.last_status) != 2 {
        return false
    }
    if audit.gate.send_count != 7 {
        return false
    }
    if audit.gate.receive_count != 7 {
        return false
    }
    if audit.spawn_observation.child_pid != audit.child_pid {
        return false
    }
    if audit.spawn_observation.child_tid != audit.child_tid {
        return false
    }
    if audit.spawn_observation.child_asid != audit.child_asid {
        return false
    }
    if audit.spawn_observation.wait_handle_slot != audit.wait_handle_slot {
        return false
    }
    if syscall.status_score(audit.spawn_observation.status) != 2 {
        return false
    }
    if audit.receive_observation.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.receive_observation.source_pid != audit.init_pid {
        return false
    }
    if audit.receive_observation.payload_len != 2 {
        return false
    }
    if audit.receive_observation.payload[0] != audit.request_byte0 {
        return false
    }
    if audit.receive_observation.payload[1] != audit.request_byte1 {
        return false
    }
    if audit.reply_observation.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.reply_observation.source_pid != audit.child_pid {
        return false
    }
    if audit.reply_observation.payload_len != 2 {
        return false
    }
    if audit.reply_observation.payload[0] != audit.request_byte0 {
        return false
    }
    if audit.reply_observation.payload[1] != audit.request_byte1 {
        return false
    }
    if audit.service_state.owner_pid != audit.child_pid {
        return false
    }
    if audit.service_state.endpoint_handle_slot != audit.endpoint_handle_slot {
        return false
    }
    if audit.service_state.request_count != 1 {
        return false
    }
    if audit.service_state.reply_count != 1 {
        return false
    }
    if audit.service_state.last_client_pid != audit.init_pid {
        return false
    }
    if audit.service_state.last_endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.service_state.last_request_len != 2 {
        return false
    }
    if audit.service_state.last_request_byte0 != audit.request_byte0 {
        return false
    }
    if audit.service_state.last_request_byte1 != audit.request_byte1 {
        return false
    }
    if audit.service_state.last_reply_len != 2 {
        return false
    }
    if audit.service_state.last_reply_byte0 != audit.request_byte0 {
        return false
    }
    if audit.service_state.last_reply_byte1 != audit.request_byte1 {
        return false
    }
    if audit.exchange.service_pid != audit.child_pid {
        return false
    }
    if audit.exchange.client_pid != audit.init_pid {
        return false
    }
    if audit.exchange.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if echo_service.tag_score(audit.exchange.tag) != 4 {
        return false
    }
    if audit.exchange.request_len != 2 {
        return false
    }
    if audit.exchange.request_byte0 != audit.request_byte0 {
        return false
    }
    if audit.exchange.request_byte1 != audit.request_byte1 {
        return false
    }
    if audit.exchange.reply_len != 2 {
        return false
    }
    if audit.exchange.reply_byte0 != audit.request_byte0 {
        return false
    }
    if audit.exchange.reply_byte1 != audit.request_byte1 {
        return false
    }
    if audit.exchange.request_count != 1 {
        return false
    }
    if audit.exchange.reply_count != 1 {
        return false
    }
    if syscall.status_score(audit.wait_observation.status) != 2 {
        return false
    }
    if audit.wait_observation.child_pid != audit.child_pid {
        return false
    }
    if audit.wait_observation.exit_code != audit.exit_code {
        return false
    }
    if audit.wait_observation.wait_handle_slot != audit.wait_handle_slot {
        return false
    }
    if audit.wait_table.count != 0 {
        return false
    }
    if capability.find_child_for_wait_handle(audit.wait_table, audit.wait_handle_slot) != 0 {
        return false
    }
    if audit.child_handle_table.count != 0 {
        return false
    }
    if audit.ready_queue.count != 0 {
        return false
    }
    if state.process_state_score(audit.child_process.state) != 1 {
        return false
    }
    if state.task_state_score(audit.child_task.state) != 1 {
        return false
    }
    if address_space.state_score(audit.child_address_space.state) != 2 {
        return false
    }
    if audit.child_address_space.owner_pid != audit.child_pid {
        return false
    }
    return audit.child_user_frame.task_id == audit.child_tid
}

func validate_phase107_user_to_user_capability_transfer(audit: TransferServicePhaseAudit) bool {
    if capability.kind_score(audit.program_capability.kind) != 1 {
        return false
    }
    if syscall.id_score(audit.gate.last_id) != 16 {
        return false
    }
    if syscall.status_score(audit.gate.last_status) != 2 {
        return false
    }
    if audit.gate.send_count != 9 {
        return false
    }
    if audit.gate.receive_count != 9 {
        return false
    }
    if audit.spawn_observation.child_pid != audit.child_pid {
        return false
    }
    if audit.spawn_observation.child_tid != audit.child_tid {
        return false
    }
    if audit.spawn_observation.child_asid != audit.child_asid {
        return false
    }
    if audit.spawn_observation.wait_handle_slot != audit.wait_handle_slot {
        return false
    }
    if syscall.status_score(audit.spawn_observation.status) != 2 {
        return false
    }
    if capability.find_endpoint_for_handle(audit.init_handle_table, audit.source_handle_slot) != 0 {
        return false
    }
    if capability.find_endpoint_for_handle(audit.init_handle_table, audit.init_received_handle_slot) != audit.transfer_endpoint_id {
        return false
    }
    if capability.find_rights_for_handle(audit.init_handle_table, audit.init_received_handle_slot) != 5 {
        return false
    }
    if audit.grant_observation.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.grant_observation.source_pid != audit.init_pid {
        return false
    }
    if audit.grant_observation.payload_len != 4 {
        return false
    }
    if audit.grant_observation.received_handle_slot != audit.service_received_handle_slot {
        return false
    }
    if audit.grant_observation.received_handle_count != 1 {
        return false
    }
    if audit.grant_observation.payload[0] != audit.grant_byte0 {
        return false
    }
    if audit.grant_observation.payload[1] != audit.grant_byte1 {
        return false
    }
    if audit.grant_observation.payload[2] != audit.grant_byte2 {
        return false
    }
    if audit.grant_observation.payload[3] != audit.grant_byte3 {
        return false
    }
    if audit.emit_observation.endpoint_id != audit.transfer_endpoint_id {
        return false
    }
    if audit.emit_observation.source_pid != audit.child_pid {
        return false
    }
    if audit.emit_observation.payload_len != 4 {
        return false
    }
    if audit.emit_observation.received_handle_slot != 0 {
        return false
    }
    if audit.emit_observation.received_handle_count != 0 {
        return false
    }
    if audit.emit_observation.payload[0] != audit.grant_byte0 {
        return false
    }
    if audit.emit_observation.payload[1] != audit.grant_byte1 {
        return false
    }
    if audit.emit_observation.payload[2] != audit.grant_byte2 {
        return false
    }
    if audit.emit_observation.payload[3] != audit.grant_byte3 {
        return false
    }
    if audit.service_state.owner_pid != audit.child_pid {
        return false
    }
    if audit.service_state.control_handle_slot != audit.control_handle_slot {
        return false
    }
    if audit.service_state.transferred_handle_slot != audit.service_received_handle_slot {
        return false
    }
    if audit.service_state.grant_count != 1 {
        return false
    }
    if audit.service_state.emit_count != 1 {
        return false
    }
    if audit.service_state.last_client_pid != audit.init_pid {
        return false
    }
    if audit.service_state.last_control_endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.service_state.last_transferred_endpoint_id != audit.transfer_endpoint_id {
        return false
    }
    if audit.service_state.last_transferred_rights != 5 {
        return false
    }
    if audit.service_state.last_grant_len != 4 {
        return false
    }
    if audit.service_state.last_grant_byte0 != audit.grant_byte0 {
        return false
    }
    if audit.service_state.last_grant_byte1 != audit.grant_byte1 {
        return false
    }
    if audit.service_state.last_grant_byte2 != audit.grant_byte2 {
        return false
    }
    if audit.service_state.last_grant_byte3 != audit.grant_byte3 {
        return false
    }
    if audit.service_state.last_emit_len != 4 {
        return false
    }
    if audit.service_state.last_emit_byte0 != audit.grant_byte0 {
        return false
    }
    if audit.service_state.last_emit_byte1 != audit.grant_byte1 {
        return false
    }
    if audit.service_state.last_emit_byte2 != audit.grant_byte2 {
        return false
    }
    if audit.service_state.last_emit_byte3 != audit.grant_byte3 {
        return false
    }
    if audit.transfer.service_pid != audit.child_pid {
        return false
    }
    if audit.transfer.client_pid != audit.init_pid {
        return false
    }
    if audit.transfer.control_endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.transfer.transferred_endpoint_id != audit.transfer_endpoint_id {
        return false
    }
    if audit.transfer.transferred_rights != 5 {
        return false
    }
    if transfer_service.tag_score(audit.transfer.tag) != 4 {
        return false
    }
    if audit.transfer.grant_len != 4 {
        return false
    }
    if audit.transfer.grant_byte0 != audit.grant_byte0 {
        return false
    }
    if audit.transfer.grant_byte1 != audit.grant_byte1 {
        return false
    }
    if audit.transfer.grant_byte2 != audit.grant_byte2 {
        return false
    }
    if audit.transfer.grant_byte3 != audit.grant_byte3 {
        return false
    }
    if audit.transfer.emit_len != 4 {
        return false
    }
    if audit.transfer.emit_byte0 != audit.grant_byte0 {
        return false
    }
    if audit.transfer.emit_byte1 != audit.grant_byte1 {
        return false
    }
    if audit.transfer.emit_byte2 != audit.grant_byte2 {
        return false
    }
    if audit.transfer.emit_byte3 != audit.grant_byte3 {
        return false
    }
    if audit.transfer.grant_count != 1 {
        return false
    }
    if audit.transfer.emit_count != 1 {
        return false
    }
    if syscall.status_score(audit.wait_observation.status) != 2 {
        return false
    }
    if audit.wait_observation.child_pid != audit.child_pid {
        return false
    }
    if audit.wait_observation.exit_code != audit.exit_code {
        return false
    }
    if audit.wait_observation.wait_handle_slot != audit.wait_handle_slot {
        return false
    }
    if audit.wait_table.count != 0 {
        return false
    }
    if capability.find_child_for_wait_handle(audit.wait_table, audit.wait_handle_slot) != 0 {
        return false
    }
    if audit.child_handle_table.count != 0 {
        return false
    }
    if audit.ready_queue.count != 0 {
        return false
    }
    if state.process_state_score(audit.child_process.state) != 1 {
        return false
    }
    if state.task_state_score(audit.child_task.state) != 1 {
        return false
    }
    if address_space.state_score(audit.child_address_space.state) != 2 {
        return false
    }
    if audit.child_address_space.owner_pid != audit.child_pid {
        return false
    }
    return audit.child_user_frame.task_id == audit.child_tid
}

func build_phase108_program_cap_contract(inputs: Phase108ProgramCapContractInputs) debug.Phase108ProgramCapContract {
    return debug.Phase108ProgramCapContract{ init_pid: inputs.init_pid, log_service_program_object_id: inputs.log_service_program_object_id, echo_service_program_object_id: inputs.echo_service_program_object_id, transfer_service_program_object_id: inputs.transfer_service_program_object_id, log_service_wait_handle_slot: inputs.log_service_wait_handle_slot, echo_service_wait_handle_slot: inputs.echo_service_wait_handle_slot, transfer_service_wait_handle_slot: inputs.transfer_service_wait_handle_slot, log_service_exit_code: inputs.log_service_exit_code, echo_service_exit_code: inputs.echo_service_exit_code, transfer_service_exit_code: inputs.transfer_service_exit_code, bootstrap_program_capability: inputs.bootstrap_program_capability, log_service_program_capability: inputs.log_service_program_capability, echo_service_program_capability: inputs.echo_service_program_capability, transfer_service_program_capability: inputs.transfer_service_program_capability, log_service_spawn: inputs.log_service_spawn, echo_service_spawn: inputs.echo_service_spawn, transfer_service_spawn: inputs.transfer_service_spawn, log_service_wait: inputs.log_service_wait, echo_service_wait: inputs.echo_service_wait, transfer_service_wait: inputs.transfer_service_wait }
}

func build_phase109_running_kernel_slice_audit(inputs: RunningKernelSliceAuditInputs) debug.RunningKernelSliceAudit {
    return debug.RunningKernelSliceAudit{ kernel: inputs.kernel, init_pid: inputs.init_pid, init_tid: inputs.init_tid, init_asid: inputs.init_asid, child_tid: inputs.child_tid, child_exit_code: inputs.child_exit_code, transfer_endpoint_id: inputs.transfer_endpoint_id, log_service_request_byte: inputs.log_service_request_byte, echo_service_request_byte0: inputs.echo_service_request_byte0, echo_service_request_byte1: inputs.echo_service_request_byte1, log_service_exit_code: inputs.log_service_exit_code, echo_service_exit_code: inputs.echo_service_exit_code, transfer_service_exit_code: inputs.transfer_service_exit_code, init_bootstrap_handoff: inputs.init_bootstrap_handoff, receive_observation: inputs.receive_observation, attached_receive_observation: inputs.attached_receive_observation, transferred_handle_use_observation: inputs.transferred_handle_use_observation, pre_exit_wait_observation: inputs.pre_exit_wait_observation, exit_wait_observation: inputs.exit_wait_observation, sleep_observation: inputs.sleep_observation, timer_wake_observation: inputs.timer_wake_observation, log_service_handshake: inputs.log_service_handshake, log_service_wait_observation: inputs.log_service_wait_observation, echo_service_exchange: inputs.echo_service_exchange, echo_service_wait_observation: inputs.echo_service_wait_observation, transfer_service_transfer: inputs.transfer_service_transfer, transfer_service_wait_observation: inputs.transfer_service_wait_observation, phase104_contract_hardened: inputs.phase104_contract_hardened, phase108_contract_hardened: inputs.phase108_contract_hardened, init_process: inputs.init_process, init_task: inputs.init_task, init_user_frame: inputs.init_user_frame, boot_log_append_failed: inputs.boot_log_append_failed }
}

func build_phase117_multi_service_bring_up_audit(inputs: Phase117MultiServiceBringUpAuditInputs) debug.Phase117MultiServiceBringUpAudit {
    return debug.Phase117MultiServiceBringUpAudit{ running_slice: inputs.running_slice, init_endpoint_id: inputs.init_endpoint_id, transfer_endpoint_id: inputs.transfer_endpoint_id, log_service_program_capability: inputs.log_service_program_capability, echo_service_program_capability: inputs.echo_service_program_capability, transfer_service_program_capability: inputs.transfer_service_program_capability, log_service_spawn: inputs.log_service_spawn, echo_service_spawn: inputs.echo_service_spawn, transfer_service_spawn: inputs.transfer_service_spawn, log_service_wait: inputs.log_service_wait, echo_service_wait: inputs.echo_service_wait, transfer_service_wait: inputs.transfer_service_wait, log_service_handshake: inputs.log_service_handshake, echo_service_exchange: inputs.echo_service_exchange, transfer_service_transfer: inputs.transfer_service_transfer }
}

func build_phase118_delegated_request_reply_audit(inputs: Phase118DelegatedRequestReplyAuditInputs) debug.Phase118DelegatedRequestReplyAudit {
    return debug.Phase118DelegatedRequestReplyAudit{ phase117: inputs.phase117, transfer_service_transfer: inputs.transfer_service_transfer, invalidated_source_send_status: inputs.invalidated_source_send_status, invalidated_source_handle_slot: inputs.invalidated_source_handle_slot, retained_receive_handle_slot: inputs.retained_receive_handle_slot, retained_receive_endpoint_id: inputs.retained_receive_endpoint_id }
}

func build_phase119_namespace_pressure_audit(inputs: Phase119NamespacePressureAuditInputs) debug.Phase119NamespacePressureAudit {
    return debug.Phase119NamespacePressureAudit{ phase118: inputs.phase118, directory_owner_pid: inputs.directory_owner_pid, directory_entry_count: inputs.directory_entry_count, log_service_key: inputs.log_service_key, echo_service_key: inputs.echo_service_key, transfer_service_key: inputs.transfer_service_key, shared_directory_endpoint_id: inputs.shared_directory_endpoint_id, log_service_program_slot: inputs.log_service_program_slot, echo_service_program_slot: inputs.echo_service_program_slot, transfer_service_program_slot: inputs.transfer_service_program_slot, log_service_program_object_id: inputs.log_service_program_object_id, echo_service_program_object_id: inputs.echo_service_program_object_id, transfer_service_program_object_id: inputs.transfer_service_program_object_id, log_service_wait_handle_slot: inputs.log_service_wait_handle_slot, echo_service_wait_handle_slot: inputs.echo_service_wait_handle_slot, transfer_service_wait_handle_slot: inputs.transfer_service_wait_handle_slot, dynamic_namespace_visible: inputs.dynamic_namespace_visible }
}

func build_phase120_running_system_support_audit(inputs: Phase120RunningSystemSupportAuditInputs) debug.Phase120RunningSystemSupportAudit {
    return debug.Phase120RunningSystemSupportAudit{ phase119: inputs.phase119, service_policy_owner_pid: inputs.service_policy_owner_pid, running_service_count: inputs.running_service_count, fixed_directory_count: inputs.fixed_directory_count, shared_control_endpoint_id: inputs.shared_control_endpoint_id, retained_reply_endpoint_id: inputs.retained_reply_endpoint_id, program_capability_count: inputs.program_capability_count, wait_handle_count: inputs.wait_handle_count, dynamic_loading_visible: inputs.dynamic_loading_visible, service_manager_visible: inputs.service_manager_visible, dynamic_namespace_visible: inputs.dynamic_namespace_visible }
}

func build_phase121_kernel_image_contract_audit(inputs: Phase121KernelImageContractAuditInputs) debug.Phase121KernelImageContractAudit {
    return debug.Phase121KernelImageContractAudit{ phase120: inputs.phase120, kernel_manifest_visible: inputs.kernel_manifest_visible, kernel_target_visible: inputs.kernel_target_visible, kernel_runtime_startup_visible: inputs.kernel_runtime_startup_visible, bootstrap_target_family_visible: inputs.bootstrap_target_family_visible, emitted_image_input_visible: inputs.emitted_image_input_visible, linked_kernel_executable_visible: inputs.linked_kernel_executable_visible, dynamic_loading_visible: inputs.dynamic_loading_visible, service_manager_visible: inputs.service_manager_visible, dynamic_namespace_visible: inputs.dynamic_namespace_visible }
}

func build_phase122_target_surface_audit(inputs: Phase122TargetSurfaceAuditInputs) debug.Phase122TargetSurfaceAudit {
    return debug.Phase122TargetSurfaceAudit{ phase121: inputs.phase121, kernel_target_visible: inputs.kernel_target_visible, kernel_runtime_startup_visible: inputs.kernel_runtime_startup_visible, bootstrap_target_family_visible: inputs.bootstrap_target_family_visible, bootstrap_target_family_only_visible: inputs.bootstrap_target_family_only_visible, broader_target_family_visible: inputs.broader_target_family_visible, dynamic_loading_visible: inputs.dynamic_loading_visible, service_manager_visible: inputs.service_manager_visible, dynamic_namespace_visible: inputs.dynamic_namespace_visible }
}