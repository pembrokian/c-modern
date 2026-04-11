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