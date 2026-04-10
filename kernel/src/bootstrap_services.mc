import address_space
import capability
import echo_service
import endpoint
import init
import lifecycle
import mmu
import log_service
import state
import syscall
import transfer_service

const LOG_SERVICE_PROGRAM_SLOT: u32 = 3
const LOG_SERVICE_PROGRAM_OBJECT_ID: u32 = 2
const LOG_SERVICE_WAIT_HANDLE_SLOT: u32 = 2
const LOG_SERVICE_ENDPOINT_HANDLE_SLOT: u32 = 1
const LOG_SERVICE_REQUEST_BYTE: u8 = 76
const LOG_SERVICE_EXIT_CODE: i32 = 52
const ECHO_SERVICE_PROGRAM_SLOT: u32 = 4
const ECHO_SERVICE_PROGRAM_OBJECT_ID: u32 = 3
const ECHO_SERVICE_WAIT_HANDLE_SLOT: u32 = 3
const ECHO_SERVICE_ENDPOINT_HANDLE_SLOT: u32 = 1
const ECHO_SERVICE_REQUEST_BYTE0: u8 = 69
const ECHO_SERVICE_REQUEST_BYTE1: u8 = 67
const ECHO_SERVICE_EXIT_CODE: i32 = 53
const TRANSFER_SERVICE_PROGRAM_SLOT: u32 = 5
const TRANSFER_SERVICE_PROGRAM_OBJECT_ID: u32 = 4
const TRANSFER_SERVICE_WAIT_HANDLE_SLOT: u32 = 4
const TRANSFER_SERVICE_CONTROL_HANDLE_SLOT: u32 = 1
const TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT: u32 = 2
const TRANSFER_SERVICE_GRANT_BYTE0: u8 = 71
const TRANSFER_SERVICE_GRANT_BYTE1: u8 = 73
const TRANSFER_SERVICE_GRANT_BYTE2: u8 = 86
const TRANSFER_SERVICE_GRANT_BYTE3: u8 = 69
const TRANSFER_SERVICE_EXIT_CODE: i32 = 54

struct LogServiceConfig {
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    init_endpoint_id: u32
    init_endpoint_handle_slot: u32
    child_translation_root: mmu.TranslationRoot
    wait_handle_slot: u32
    endpoint_handle_slot: u32
    request_byte: u8
    exit_code: i32
    program_slot: u32
    program_object_id: u32
}

struct LogServiceExecutionState {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    init_handle_table: capability.HandleTable
    child_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    endpoints: endpoint.EndpointTable
    init_image: init.InitImage
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    service_state: log_service.LogServiceState
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    ack_observation: syscall.ReceiveObservation
    handshake: log_service.LogHandshakeObservation
    wait_observation: syscall.WaitObservation
    ready_queue: state.ReadyQueue
}

struct LogServiceExecutionResult {
    state: LogServiceExecutionState
    succeeded: u32
}

struct EchoServiceConfig {
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    init_endpoint_id: u32
    init_endpoint_handle_slot: u32
    child_translation_root: mmu.TranslationRoot
    wait_handle_slot: u32
    endpoint_handle_slot: u32
    request_byte0: u8
    request_byte1: u8
    exit_code: i32
    program_slot: u32
    program_object_id: u32
}

struct EchoServiceExecutionState {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    init_handle_table: capability.HandleTable
    child_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    endpoints: endpoint.EndpointTable
    init_image: init.InitImage
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    service_state: echo_service.EchoServiceState
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    reply_observation: syscall.ReceiveObservation
    exchange: echo_service.EchoExchangeObservation
    wait_observation: syscall.WaitObservation
    ready_queue: state.ReadyQueue
}

struct EchoServiceExecutionResult {
    state: EchoServiceExecutionState
    succeeded: u32
}

struct TransferServiceConfig {
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    init_endpoint_id: u32
    init_endpoint_handle_slot: u32
    child_translation_root: mmu.TranslationRoot
    wait_handle_slot: u32
    control_handle_slot: u32
    source_handle_slot: u32
    init_received_handle_slot: u32
    service_received_handle_slot: u32
    grant_byte0: u8
    grant_byte1: u8
    grant_byte2: u8
    grant_byte3: u8
    exit_code: i32
    program_slot: u32
    program_object_id: u32
}

struct TransferServiceExecutionState {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    init_handle_table: capability.HandleTable
    child_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    endpoints: endpoint.EndpointTable
    init_image: init.InitImage
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    service_state: transfer_service.TransferServiceState
    spawn_observation: syscall.SpawnObservation
    grant_observation: syscall.ReceiveObservation
    emit_observation: syscall.ReceiveObservation
    transfer: transfer_service.TransferObservation
    wait_observation: syscall.WaitObservation
    ready_queue: state.ReadyQueue
}

struct TransferServiceExecutionResult {
    state: TransferServiceExecutionState
    succeeded: u32
}

func log_service_config(init_pid: u32, child_pid: u32, child_tid: u32, child_asid: u32, init_endpoint_id: u32, init_endpoint_handle_slot: u32, child_translation_root: mmu.TranslationRoot) LogServiceConfig {
    return LogServiceConfig{ init_pid: init_pid, child_pid: child_pid, child_tid: child_tid, child_asid: child_asid, init_endpoint_id: init_endpoint_id, init_endpoint_handle_slot: init_endpoint_handle_slot, child_translation_root: child_translation_root, wait_handle_slot: LOG_SERVICE_WAIT_HANDLE_SLOT, endpoint_handle_slot: LOG_SERVICE_ENDPOINT_HANDLE_SLOT, request_byte: LOG_SERVICE_REQUEST_BYTE, exit_code: LOG_SERVICE_EXIT_CODE, program_slot: LOG_SERVICE_PROGRAM_SLOT, program_object_id: LOG_SERVICE_PROGRAM_OBJECT_ID }
}

func log_service_result(state: LogServiceExecutionState, succeeded: u32) LogServiceExecutionResult {
    return LogServiceExecutionResult{ state: state, succeeded: succeeded }
}

func echo_service_config(init_pid: u32, child_pid: u32, child_tid: u32, child_asid: u32, init_endpoint_id: u32, init_endpoint_handle_slot: u32, child_translation_root: mmu.TranslationRoot) EchoServiceConfig {
    return EchoServiceConfig{ init_pid: init_pid, child_pid: child_pid, child_tid: child_tid, child_asid: child_asid, init_endpoint_id: init_endpoint_id, init_endpoint_handle_slot: init_endpoint_handle_slot, child_translation_root: child_translation_root, wait_handle_slot: ECHO_SERVICE_WAIT_HANDLE_SLOT, endpoint_handle_slot: ECHO_SERVICE_ENDPOINT_HANDLE_SLOT, request_byte0: ECHO_SERVICE_REQUEST_BYTE0, request_byte1: ECHO_SERVICE_REQUEST_BYTE1, exit_code: ECHO_SERVICE_EXIT_CODE, program_slot: ECHO_SERVICE_PROGRAM_SLOT, program_object_id: ECHO_SERVICE_PROGRAM_OBJECT_ID }
}

func echo_service_result(state: EchoServiceExecutionState, succeeded: u32) EchoServiceExecutionResult {
    return EchoServiceExecutionResult{ state: state, succeeded: succeeded }
}

func transfer_service_config(init_pid: u32, child_pid: u32, child_tid: u32, child_asid: u32, init_endpoint_id: u32, init_endpoint_handle_slot: u32, child_translation_root: mmu.TranslationRoot, source_handle_slot: u32, init_received_handle_slot: u32) TransferServiceConfig {
    return TransferServiceConfig{ init_pid: init_pid, child_pid: child_pid, child_tid: child_tid, child_asid: child_asid, init_endpoint_id: init_endpoint_id, init_endpoint_handle_slot: init_endpoint_handle_slot, child_translation_root: child_translation_root, wait_handle_slot: TRANSFER_SERVICE_WAIT_HANDLE_SLOT, control_handle_slot: TRANSFER_SERVICE_CONTROL_HANDLE_SLOT, source_handle_slot: source_handle_slot, init_received_handle_slot: init_received_handle_slot, service_received_handle_slot: TRANSFER_SERVICE_RECEIVED_HANDLE_SLOT, grant_byte0: TRANSFER_SERVICE_GRANT_BYTE0, grant_byte1: TRANSFER_SERVICE_GRANT_BYTE1, grant_byte2: TRANSFER_SERVICE_GRANT_BYTE2, grant_byte3: TRANSFER_SERVICE_GRANT_BYTE3, exit_code: TRANSFER_SERVICE_EXIT_CODE, program_slot: TRANSFER_SERVICE_PROGRAM_SLOT, program_object_id: TRANSFER_SERVICE_PROGRAM_OBJECT_ID }
}

func transfer_service_result(state: TransferServiceExecutionState, succeeded: u32) TransferServiceExecutionResult {
    return TransferServiceExecutionResult{ state: state, succeeded: succeeded }
}

func seed_log_service_program_capability(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionState {
    return LogServiceExecutionState{ program_capability: capability.CapabilitySlot{ slot_id: config.program_slot, owner_pid: config.init_pid, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: config.program_object_id }, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ack_observation: execution.ack_observation, handshake: execution.handshake, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func spawn_log_service(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionResult {
    child_handles: capability.HandleTable = capability.handle_table_for_owner(config.child_pid)
    wait_table: capability.WaitTable = capability.wait_table_for_owner(config.init_pid)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(config.wait_handle_slot)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(execution.gate, execution.program_capability, execution.process_slots, execution.task_slots, wait_table, execution.init_image, spawn_request, config.child_pid, config.child_tid, config.child_asid, config.child_translation_root)
    next_state: LogServiceExecutionState = LogServiceExecutionState{ program_capability: spawn_result.program_capability, gate: spawn_result.gate, process_slots: spawn_result.process_slots, task_slots: spawn_result.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: child_handles, wait_table: spawn_result.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: spawn_result.child_address_space, child_user_frame: spawn_result.child_frame, service_state: execution.service_state, spawn_observation: spawn_result.observation, receive_observation: execution.receive_observation, ack_observation: execution.ack_observation, handshake: execution.handshake, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(next_state.spawn_observation.status) != 2 {
        return log_service_result(next_state, 0)
    }

    child_handles = capability.install_endpoint_handle(next_state.child_handle_table, config.endpoint_handle_slot, config.init_endpoint_id, 3)
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: child_handles, wait_table: next_state.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: log_service.service_state(config.child_pid, config.endpoint_handle_slot), spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ack_observation: next_state.ack_observation, handshake: next_state.handshake, wait_observation: next_state.wait_observation, ready_queue: state.user_ready_queue(config.child_tid) }
    if capability.find_endpoint_for_handle(next_state.child_handle_table, config.endpoint_handle_slot) != config.init_endpoint_id {
        return log_service_result(next_state, 0)
    }
    return log_service_result(next_state, 1)
}

func execute_log_service_request_reply(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionResult {
    request_payload: [4]u8 = endpoint.zero_payload()
    request_payload[0] = config.request_byte

    request_send_result: syscall.SendResult = syscall.perform_send(execution.gate, execution.init_handle_table, execution.endpoints, config.init_pid, syscall.build_send_request(config.init_endpoint_handle_slot, 1, request_payload))
    next_state: LogServiceExecutionState = LogServiceExecutionState{ program_capability: execution.program_capability, gate: request_send_result.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: request_send_result.handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: request_send_result.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ack_observation: execution.ack_observation, handshake: execution.handshake, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(request_send_result.status) != 2 {
        return log_service_result(next_state, 0)
    }

    service_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.child_handle_table, next_state.endpoints, syscall.build_receive_request(config.endpoint_handle_slot))
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: service_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: service_receive_result.handle_table, wait_table: next_state.wait_table, endpoints: service_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: service_receive_result.observation, ack_observation: next_state.ack_observation, handshake: next_state.handshake, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.receive_observation.status) != 2 {
        return log_service_result(next_state, 0)
    }
    updated_service_state: log_service.LogServiceState = log_service.record_open_request(next_state.service_state, next_state.receive_observation)

    ack_send_result: syscall.SendResult = syscall.perform_send(next_state.gate, next_state.child_handle_table, next_state.endpoints, config.child_pid, syscall.build_send_request(config.endpoint_handle_slot, 1, log_service.ack_payload()))
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: ack_send_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: ack_send_result.handle_table, wait_table: next_state.wait_table, endpoints: ack_send_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ack_observation: next_state.ack_observation, handshake: next_state.handshake, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(ack_send_result.status) != 2 {
        return log_service_result(next_state, 0)
    }

    init_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.init_handle_table, next_state.endpoints, syscall.build_receive_request(config.init_endpoint_handle_slot))
    updated_service_state = log_service.record_ack(next_state.service_state, init_receive_result.observation.payload[0])
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: init_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: init_receive_result.handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: init_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ack_observation: init_receive_result.observation, handshake: log_service.observe_handshake(updated_service_state), wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.ack_observation.status) != 2 {
        return log_service_result(next_state, 0)
    }
    return log_service_result(next_state, 1)
}

func simulate_log_service_exit(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionState {
    processes: [3]state.ProcessSlot = lifecycle.exit_process(execution.process_slots, 2)
    tasks: [3]state.TaskSlot = lifecycle.exit_task(execution.task_slots, 2)
    wait_table: capability.WaitTable = capability.mark_wait_handle_exited(execution.wait_table, config.child_pid, config.exit_code)
    return LogServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: processes, task_slots: tasks, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ack_observation: execution.ack_observation, handshake: execution.handshake, wait_observation: execution.wait_observation, ready_queue: state.empty_queue() }
}

func execute_phase105_log_service_handshake(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionResult {
    next_state: LogServiceExecutionState = seed_log_service_program_capability(config, execution)
    if !capability.is_program_capability(next_state.program_capability) {
        return log_service_result(next_state, 0)
    }

    spawned: LogServiceExecutionResult = spawn_log_service(config, next_state)
    if spawned.succeeded == 0 {
        return spawned
    }

    exchanged: LogServiceExecutionResult = execute_log_service_request_reply(config, spawned.state)
    if exchanged.succeeded == 0 {
        return exchanged
    }

    next_state = simulate_log_service_exit(config, exchanged.state)
    wait_result: syscall.WaitResult = syscall.perform_wait(next_state.gate, next_state.process_slots, next_state.task_slots, next_state.wait_table, syscall.build_wait_request(config.wait_handle_slot))
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: wait_result.gate, process_slots: wait_result.process_slots, task_slots: wait_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: capability.empty_handle_table(), wait_table: wait_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ack_observation: next_state.ack_observation, handshake: next_state.handshake, wait_observation: wait_result.observation, ready_queue: state.empty_queue() }
    if syscall.status_score(next_state.wait_observation.status) != 2 {
        return log_service_result(next_state, 0)
    }
    return log_service_result(next_state, 1)
}

func seed_echo_service_program_capability(config: EchoServiceConfig, execution: EchoServiceExecutionState) EchoServiceExecutionState {
    return EchoServiceExecutionState{ program_capability: capability.CapabilitySlot{ slot_id: config.program_slot, owner_pid: config.init_pid, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: config.program_object_id }, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, reply_observation: execution.reply_observation, exchange: execution.exchange, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func spawn_echo_service(config: EchoServiceConfig, execution: EchoServiceExecutionState) EchoServiceExecutionResult {
    child_handles: capability.HandleTable = capability.handle_table_for_owner(config.child_pid)
    wait_table: capability.WaitTable = capability.wait_table_for_owner(config.init_pid)
    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(config.wait_handle_slot)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(execution.gate, execution.program_capability, execution.process_slots, execution.task_slots, wait_table, execution.init_image, spawn_request, config.child_pid, config.child_tid, config.child_asid, config.child_translation_root)
    next_state: EchoServiceExecutionState = EchoServiceExecutionState{ program_capability: spawn_result.program_capability, gate: spawn_result.gate, process_slots: spawn_result.process_slots, task_slots: spawn_result.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: child_handles, wait_table: spawn_result.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: spawn_result.child_address_space, child_user_frame: spawn_result.child_frame, service_state: execution.service_state, spawn_observation: spawn_result.observation, receive_observation: execution.receive_observation, reply_observation: execution.reply_observation, exchange: execution.exchange, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(next_state.spawn_observation.status) != 2 {
        return echo_service_result(next_state, 0)
    }
    child_handles = capability.install_endpoint_handle(next_state.child_handle_table, config.endpoint_handle_slot, config.init_endpoint_id, 3)
    next_state = EchoServiceExecutionState{ program_capability: next_state.program_capability, gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: child_handles, wait_table: next_state.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: echo_service.service_state(config.child_pid, config.endpoint_handle_slot), spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, reply_observation: next_state.reply_observation, exchange: next_state.exchange, wait_observation: next_state.wait_observation, ready_queue: state.user_ready_queue(config.child_tid) }
    if capability.find_endpoint_for_handle(next_state.child_handle_table, config.endpoint_handle_slot) != config.init_endpoint_id {
        return echo_service_result(next_state, 0)
    }
    return echo_service_result(next_state, 1)
}

func execute_echo_service_request_reply(config: EchoServiceConfig, execution: EchoServiceExecutionState) EchoServiceExecutionResult {
    request_payload: [4]u8 = endpoint.zero_payload()
    request_payload[0] = config.request_byte0
    request_payload[1] = config.request_byte1
    request_send_result: syscall.SendResult = syscall.perform_send(execution.gate, execution.init_handle_table, execution.endpoints, config.init_pid, syscall.build_send_request(config.init_endpoint_handle_slot, 2, request_payload))
    next_state: EchoServiceExecutionState = EchoServiceExecutionState{ program_capability: execution.program_capability, gate: request_send_result.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: request_send_result.handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: request_send_result.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, reply_observation: execution.reply_observation, exchange: execution.exchange, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(request_send_result.status) != 2 {
        return echo_service_result(next_state, 0)
    }
    service_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.child_handle_table, next_state.endpoints, syscall.build_receive_request(config.endpoint_handle_slot))
    next_state = EchoServiceExecutionState{ program_capability: next_state.program_capability, gate: service_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: service_receive_result.handle_table, wait_table: next_state.wait_table, endpoints: service_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: service_receive_result.observation, reply_observation: next_state.reply_observation, exchange: next_state.exchange, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.receive_observation.status) != 2 {
        return echo_service_result(next_state, 0)
    }
    updated_service_state: echo_service.EchoServiceState = echo_service.record_request(next_state.service_state, next_state.receive_observation)
    reply_send_result: syscall.SendResult = syscall.perform_send(next_state.gate, next_state.child_handle_table, next_state.endpoints, config.child_pid, syscall.build_send_request(config.endpoint_handle_slot, 2, echo_service.reply_payload(updated_service_state)))
    next_state = EchoServiceExecutionState{ program_capability: next_state.program_capability, gate: reply_send_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: reply_send_result.handle_table, wait_table: next_state.wait_table, endpoints: reply_send_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, reply_observation: next_state.reply_observation, exchange: next_state.exchange, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(reply_send_result.status) != 2 {
        return echo_service_result(next_state, 0)
    }
    init_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.init_handle_table, next_state.endpoints, syscall.build_receive_request(config.init_endpoint_handle_slot))
    updated_service_state = echo_service.record_reply(next_state.service_state, init_receive_result.observation)
    next_state = EchoServiceExecutionState{ program_capability: next_state.program_capability, gate: init_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: init_receive_result.handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: init_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, reply_observation: init_receive_result.observation, exchange: echo_service.observe_exchange(updated_service_state), wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.reply_observation.status) != 2 {
        return echo_service_result(next_state, 0)
    }
    return echo_service_result(next_state, 1)
}

func simulate_echo_service_exit(config: EchoServiceConfig, execution: EchoServiceExecutionState) EchoServiceExecutionState {
    processes: [3]state.ProcessSlot = lifecycle.exit_process(execution.process_slots, 2)
    tasks: [3]state.TaskSlot = lifecycle.exit_task(execution.task_slots, 2)
    wait_table: capability.WaitTable = capability.mark_wait_handle_exited(execution.wait_table, config.child_pid, config.exit_code)
    return EchoServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: processes, task_slots: tasks, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, reply_observation: execution.reply_observation, exchange: execution.exchange, wait_observation: execution.wait_observation, ready_queue: state.empty_queue() }
}

func execute_phase106_echo_service_request_reply(config: EchoServiceConfig, execution: EchoServiceExecutionState) EchoServiceExecutionResult {
    next_state: EchoServiceExecutionState = seed_echo_service_program_capability(config, execution)
    if !capability.is_program_capability(next_state.program_capability) {
        return echo_service_result(next_state, 0)
    }
    spawned: EchoServiceExecutionResult = spawn_echo_service(config, next_state)
    if spawned.succeeded == 0 {
        return spawned
    }
    exchanged: EchoServiceExecutionResult = execute_echo_service_request_reply(config, spawned.state)
    if exchanged.succeeded == 0 {
        return exchanged
    }
    next_state = simulate_echo_service_exit(config, exchanged.state)
    wait_result: syscall.WaitResult = syscall.perform_wait(next_state.gate, next_state.process_slots, next_state.task_slots, next_state.wait_table, syscall.build_wait_request(config.wait_handle_slot))
    next_state = EchoServiceExecutionState{ program_capability: next_state.program_capability, gate: wait_result.gate, process_slots: wait_result.process_slots, task_slots: wait_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: capability.empty_handle_table(), wait_table: wait_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, reply_observation: next_state.reply_observation, exchange: next_state.exchange, wait_observation: wait_result.observation, ready_queue: state.empty_queue() }
    if syscall.status_score(next_state.wait_observation.status) != 2 {
        return echo_service_result(next_state, 0)
    }
    return echo_service_result(next_state, 1)
}

func seed_transfer_service_program_capability(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionState {
    return TransferServiceExecutionState{ program_capability: capability.CapabilitySlot{ slot_id: config.program_slot, owner_pid: config.init_pid, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: config.program_object_id }, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func seed_transfer_service_sender_handle(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionResult {
    init_handle_table: capability.HandleTable = capability.install_endpoint_handle(execution.init_handle_table, config.source_handle_slot, 2, 5)
    next_state: TransferServiceExecutionState = TransferServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if capability.find_endpoint_for_handle(next_state.init_handle_table, config.init_received_handle_slot) != 2 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_rights_for_handle(next_state.init_handle_table, config.init_received_handle_slot) != 5 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.init_handle_table, config.source_handle_slot) != 2 {
        return transfer_service_result(next_state, 0)
    }
    return transfer_service_result(next_state, 1)
}

func spawn_transfer_service(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionResult {
    child_handles: capability.HandleTable = capability.handle_table_for_owner(config.child_pid)
    wait_table: capability.WaitTable = capability.wait_table_for_owner(config.init_pid)
    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(config.wait_handle_slot)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(execution.gate, execution.program_capability, execution.process_slots, execution.task_slots, wait_table, execution.init_image, spawn_request, config.child_pid, config.child_tid, config.child_asid, config.child_translation_root)
    next_state: TransferServiceExecutionState = TransferServiceExecutionState{ program_capability: spawn_result.program_capability, gate: spawn_result.gate, process_slots: spawn_result.process_slots, task_slots: spawn_result.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: child_handles, wait_table: spawn_result.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: spawn_result.child_address_space, child_user_frame: spawn_result.child_frame, service_state: execution.service_state, spawn_observation: spawn_result.observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(next_state.spawn_observation.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    child_handles = capability.install_endpoint_handle(next_state.child_handle_table, config.control_handle_slot, config.init_endpoint_id, 3)
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: child_handles, wait_table: next_state.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: transfer_service.service_state(config.child_pid, config.control_handle_slot, config.service_received_handle_slot), spawn_observation: next_state.spawn_observation, grant_observation: next_state.grant_observation, emit_observation: next_state.emit_observation, transfer: next_state.transfer, wait_observation: next_state.wait_observation, ready_queue: state.user_ready_queue(config.child_tid) }
    if capability.find_endpoint_for_handle(next_state.child_handle_table, config.control_handle_slot) != config.init_endpoint_id {
        return transfer_service_result(next_state, 0)
    }
    return transfer_service_result(next_state, 1)
}

func execute_user_to_user_capability_transfer(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionResult {
    grant_payload: [4]u8 = endpoint.zero_payload()
    grant_payload[0] = config.grant_byte0
    grant_payload[1] = config.grant_byte1
    grant_payload[2] = config.grant_byte2
    grant_payload[3] = config.grant_byte3
    transfer_send_result: syscall.SendResult = syscall.perform_send(execution.gate, execution.init_handle_table, execution.endpoints, config.init_pid, syscall.build_transfer_send_request(config.init_endpoint_handle_slot, 4, grant_payload, config.source_handle_slot))
    next_state: TransferServiceExecutionState = TransferServiceExecutionState{ program_capability: execution.program_capability, gate: transfer_send_result.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: transfer_send_result.handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: transfer_send_result.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(transfer_send_result.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.init_handle_table, config.source_handle_slot) != 0 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.init_handle_table, config.init_received_handle_slot) != 2 {
        return transfer_service_result(next_state, 0)
    }
    service_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.child_handle_table, next_state.endpoints, syscall.build_transfer_receive_request(config.control_handle_slot, config.service_received_handle_slot))
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: service_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: service_receive_result.handle_table, wait_table: next_state.wait_table, endpoints: service_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, grant_observation: service_receive_result.observation, emit_observation: next_state.emit_observation, transfer: next_state.transfer, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.grant_observation.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.child_handle_table, config.service_received_handle_slot) != 2 {
        return transfer_service_result(next_state, 0)
    }
    transferred_endpoint_id: u32 = capability.find_endpoint_for_handle(next_state.child_handle_table, config.service_received_handle_slot)
    transferred_rights: u32 = capability.find_rights_for_handle(next_state.child_handle_table, config.service_received_handle_slot)
    updated_service_state: transfer_service.TransferServiceState = transfer_service.record_grant(next_state.service_state, next_state.grant_observation, transferred_endpoint_id, transferred_rights)
    emit_payload: [4]u8 = transfer_service.emit_payload(updated_service_state)
    emit_send_result: syscall.SendResult = syscall.perform_send(next_state.gate, next_state.child_handle_table, next_state.endpoints, config.child_pid, syscall.build_send_request(config.service_received_handle_slot, 4, emit_payload))
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: emit_send_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: emit_send_result.handle_table, wait_table: next_state.wait_table, endpoints: emit_send_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, grant_observation: next_state.grant_observation, emit_observation: next_state.emit_observation, transfer: next_state.transfer, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(emit_send_result.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    init_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.init_handle_table, next_state.endpoints, syscall.build_receive_request(config.init_received_handle_slot))
    updated_service_state = transfer_service.record_emit(next_state.service_state, init_receive_result.observation)
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: init_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: init_receive_result.handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: init_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, grant_observation: next_state.grant_observation, emit_observation: init_receive_result.observation, transfer: transfer_service.observe_transfer(updated_service_state), wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.emit_observation.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    return transfer_service_result(next_state, 1)
}

func simulate_transfer_service_exit(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionState {
    processes: [3]state.ProcessSlot = lifecycle.exit_process(execution.process_slots, 2)
    tasks: [3]state.TaskSlot = lifecycle.exit_task(execution.task_slots, 2)
    wait_table: capability.WaitTable = capability.mark_wait_handle_exited(execution.wait_table, config.child_pid, config.exit_code)
    return TransferServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: processes, task_slots: tasks, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: state.empty_queue() }
}

func execute_phase107_user_to_user_capability_transfer(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionResult {
    next_state: TransferServiceExecutionState = seed_transfer_service_program_capability(config, execution)
    if !capability.is_program_capability(next_state.program_capability) {
        return transfer_service_result(next_state, 0)
    }
    seeded: TransferServiceExecutionResult = seed_transfer_service_sender_handle(config, next_state)
    if seeded.succeeded == 0 {
        return seeded
    }
    spawned: TransferServiceExecutionResult = spawn_transfer_service(config, seeded.state)
    if spawned.succeeded == 0 {
        return spawned
    }
    transferred: TransferServiceExecutionResult = execute_user_to_user_capability_transfer(config, spawned.state)
    if transferred.succeeded == 0 {
        return transferred
    }
    next_state = simulate_transfer_service_exit(config, transferred.state)
    wait_result: syscall.WaitResult = syscall.perform_wait(next_state.gate, next_state.process_slots, next_state.task_slots, next_state.wait_table, syscall.build_wait_request(config.wait_handle_slot))
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: wait_result.gate, process_slots: wait_result.process_slots, task_slots: wait_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: capability.empty_handle_table(), wait_table: wait_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, grant_observation: next_state.grant_observation, emit_observation: next_state.emit_observation, transfer: next_state.transfer, wait_observation: wait_result.observation, ready_queue: state.empty_queue() }
    if syscall.status_score(next_state.wait_observation.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    return transfer_service_result(next_state, 1)
}