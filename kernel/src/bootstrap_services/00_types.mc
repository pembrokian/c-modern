import address_space
import capability
import echo_service
import ipc
import init
import lifecycle
import mmu
import log_service
import kv_service
import shell_service
import serial_service
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
const COMPOSITION_SERVICE_PROGRAM_SLOT: u32 = 6
const COMPOSITION_SERVICE_PROGRAM_OBJECT_ID: u32 = 5
const COMPOSITION_SERVICE_WAIT_HANDLE_SLOT: u32 = 2
const COMPOSITION_SERVICE_CONTROL_HANDLE_SLOT: u32 = 1
const COMPOSITION_SERVICE_ECHO_HANDLE_SLOT: u32 = 2
const COMPOSITION_SERVICE_LOG_HANDLE_SLOT: u32 = 3
const COMPOSITION_SERVICE_INIT_ECHO_HANDLE_SLOT: u32 = 4
const COMPOSITION_SERVICE_INIT_LOG_HANDLE_SLOT: u32 = 5
const COMPOSITION_SERVICE_REQUEST_BYTE0: u8 = 69
const COMPOSITION_SERVICE_REQUEST_BYTE1: u8 = 67
const COMPOSITION_SERVICE_EXIT_CODE: i32 = 55
const COMPOSITION_SERVICE_EDGE_COUNT: u8 = 2
const SERIAL_SERVICE_PROGRAM_SLOT: u32 = 7
const SERIAL_SERVICE_PROGRAM_OBJECT_ID: u32 = 6
const SERIAL_SERVICE_WAIT_HANDLE_SLOT: u32 = 2
const SERIAL_SERVICE_ENDPOINT_HANDLE_SLOT: u32 = 1
const SERIAL_SERVICE_COMPOSITION_HANDLE_SLOT: u32 = 2
const SERIAL_SERVICE_EXIT_CODE: i32 = 56

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
    endpoints: ipc.EndpointTable
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
    endpoints: ipc.EndpointTable
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
    endpoints: ipc.EndpointTable
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

struct CompositionServiceConfig {
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    control_endpoint_id: u32
    echo_endpoint_id: u32
    log_endpoint_id: u32
    init_control_handle_slot: u32
    init_echo_handle_slot: u32
    init_log_handle_slot: u32
    child_control_handle_slot: u32
    child_echo_handle_slot: u32
    child_log_handle_slot: u32
    child_translation_root: mmu.TranslationRoot
    wait_handle_slot: u32
    request_byte0: u8
    request_byte1: u8
    edge_count: u8
    exit_code: i32
    program_slot: u32
    program_object_id: u32
}

struct CompositionFlowObservation {
    service_pid: u32
    client_pid: u32
    control_endpoint_id: u32
    echo_endpoint_id: u32
    log_endpoint_id: u32
    request_len: usize
    request_byte0: u8
    request_byte1: u8
    echo_reply_byte0: u8
    echo_reply_byte1: u8
    log_ack_byte: u8
    aggregate_reply_byte0: u8
    aggregate_reply_byte1: u8
    aggregate_reply_byte2: u8
    aggregate_reply_byte3: u8
    outbound_edge_count: usize
    aggregate_reply_count: usize
}

struct CompositionServiceExecutionState {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    init_handle_table: capability.HandleTable
    child_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    endpoints: ipc.EndpointTable
    init_image: init.InitImage
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    echo_peer_state: echo_service.EchoServiceState
    log_peer_state: log_service.LogServiceState
    spawn_observation: syscall.SpawnObservation
    request_receive_observation: syscall.ReceiveObservation
    echo_fanout_observation: syscall.SendObservation
    echo_peer_receive_observation: syscall.ReceiveObservation
    echo_reply_send_observation: syscall.SendObservation
    echo_reply_observation: syscall.ReceiveObservation
    log_fanout_observation: syscall.SendObservation
    log_peer_receive_observation: syscall.ReceiveObservation
    log_ack_send_observation: syscall.SendObservation
    log_ack_observation: syscall.ReceiveObservation
    aggregate_reply_send_observation: syscall.SendObservation
    aggregate_reply_observation: syscall.ReceiveObservation
    wait_observation: syscall.WaitObservation
    observation: CompositionFlowObservation
    ready_queue: state.ReadyQueue
}

struct CompositionServiceExecutionResult {
    state: CompositionServiceExecutionState
    succeeded: u32
}

struct SerialServiceConfig {
    init_pid: u32
    child_pid: u32
    child_tid: u32
    child_asid: u32
    serial_endpoint_id: u32
    composition_endpoint_id: u32
    child_translation_root: mmu.TranslationRoot
    wait_handle_slot: u32
    endpoint_handle_slot: u32
    composition_handle_slot: u32
    exit_code: i32
    program_slot: u32
    program_object_id: u32
}

struct SerialServiceFailureObservation {
    endpoint_id: u32
    closed: u32
    aborted_messages: usize
    wake_count: usize
}

struct SerialServiceExecutionState {
    program_capability: capability.CapabilitySlot
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    init_handle_table: capability.HandleTable
    child_handle_table: capability.HandleTable
    wait_table: capability.WaitTable
    endpoints: ipc.EndpointTable
    init_image: init.InitImage
    child_address_space: address_space.AddressSpace
    child_user_frame: address_space.UserEntryFrame
    service_state: serial_service.SerialServiceState
    spawn_observation: syscall.SpawnObservation
    receive_observation: syscall.ReceiveObservation
    ingress: serial_service.SerialIngressObservation
    composition: serial_service.SerialCompositionObservation
    failure_observation: SerialServiceFailureObservation
    wait_observation: syscall.WaitObservation
    ready_queue: state.ReadyQueue
}

struct SerialServiceExecutionResult {
    state: SerialServiceExecutionState
    succeeded: u32
}

struct Phase140SerialIngressCompositionResult {
    serial_state: SerialServiceExecutionState
    composition_state: CompositionServiceExecutionState
    succeeded: u32
}

struct Phase142ShellCommandRouteResult {
    serial_state: serial_service.SerialServiceState
    shell_state: shell_service.ShellServiceState
    log_state: log_service.LogServiceState
    echo_state: echo_service.EchoServiceState
    kv_state: kv_service.KvServiceState
    routing: shell_service.ShellRoutingObservation
    succeeded: u32
}

struct Phase147IpcShapeWorkflowResult {
    serial_state: serial_service.SerialServiceState
    shell_state: shell_service.ShellServiceState
    log_state: log_service.LogServiceState
    echo_state: echo_service.EchoServiceState
    kv_state: kv_service.KvServiceState
    log_append_route: shell_service.ShellRoutingObservation
    log_tail_route: shell_service.ShellRoutingObservation
    kv_set_route: shell_service.ShellRoutingObservation
    kv_get_route: shell_service.ShellRoutingObservation
    repeated_log_tail_route: shell_service.ShellRoutingObservation
    repeated_kv_get_route: shell_service.ShellRoutingObservation
    succeeded: u32
}

struct Phase150RebuiltSystemState {
    serial_state: serial_service.SerialServiceState
    shell_state: shell_service.ShellServiceState
    log_state: log_service.LogServiceState
    echo_state: echo_service.EchoServiceState
    kv_state: kv_service.KvServiceState
}

struct Phase150RebuiltSystemCommandResult {
    system: Phase150RebuiltSystemState
    routing: shell_service.ShellRoutingObservation
    succeeded: u32
}

struct Phase150RebuiltSystemRestartResult {
    system: Phase150RebuiltSystemState
    restart: init.ServiceRestartObservation
}

struct Phase150RebuiltSystemWorkflowResult {
    rebuilt_system: Phase150RebuiltSystemState
    log_append_route: shell_service.ShellRoutingObservation
    log_tail_route: shell_service.ShellRoutingObservation
    pre_failure_set_route: shell_service.ShellRoutingObservation
    pre_failure_get_route: shell_service.ShellRoutingObservation
    failed_get_route: shell_service.ShellRoutingObservation
    restarted_get_route: shell_service.ShellRoutingObservation
    pre_failure_retention: kv_service.KvRetentionObservation
    post_restart_retention: kv_service.KvRetentionObservation
    restart: init.ServiceRestartObservation
    event_log_retention: log_service.LogRetentionObservation
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

func composition_service_config(init_pid: u32, child_pid: u32, child_tid: u32, child_asid: u32, control_endpoint_id: u32, echo_endpoint_id: u32, log_endpoint_id: u32, init_control_handle_slot: u32, child_translation_root: mmu.TranslationRoot) CompositionServiceConfig {
    return CompositionServiceConfig{ init_pid: init_pid, child_pid: child_pid, child_tid: child_tid, child_asid: child_asid, control_endpoint_id: control_endpoint_id, echo_endpoint_id: echo_endpoint_id, log_endpoint_id: log_endpoint_id, init_control_handle_slot: init_control_handle_slot, init_echo_handle_slot: COMPOSITION_SERVICE_INIT_ECHO_HANDLE_SLOT, init_log_handle_slot: COMPOSITION_SERVICE_INIT_LOG_HANDLE_SLOT, child_control_handle_slot: COMPOSITION_SERVICE_CONTROL_HANDLE_SLOT, child_echo_handle_slot: COMPOSITION_SERVICE_ECHO_HANDLE_SLOT, child_log_handle_slot: COMPOSITION_SERVICE_LOG_HANDLE_SLOT, child_translation_root: child_translation_root, wait_handle_slot: COMPOSITION_SERVICE_WAIT_HANDLE_SLOT, request_byte0: COMPOSITION_SERVICE_REQUEST_BYTE0, request_byte1: COMPOSITION_SERVICE_REQUEST_BYTE1, edge_count: COMPOSITION_SERVICE_EDGE_COUNT, exit_code: COMPOSITION_SERVICE_EXIT_CODE, program_slot: COMPOSITION_SERVICE_PROGRAM_SLOT, program_object_id: COMPOSITION_SERVICE_PROGRAM_OBJECT_ID }
}

func empty_composition_flow_observation() CompositionFlowObservation {
    return CompositionFlowObservation{ service_pid: 0, client_pid: 0, control_endpoint_id: 0, echo_endpoint_id: 0, log_endpoint_id: 0, request_len: 0, request_byte0: 0, request_byte1: 0, echo_reply_byte0: 0, echo_reply_byte1: 0, log_ack_byte: 0, aggregate_reply_byte0: 0, aggregate_reply_byte1: 0, aggregate_reply_byte2: 0, aggregate_reply_byte3: 0, outbound_edge_count: 0, aggregate_reply_count: 0 }
}

func empty_composition_service_execution_state() CompositionServiceExecutionState {
    return CompositionServiceExecutionState{ program_capability: capability.empty_slot(), gate: syscall.gate_closed(), process_slots: state.zero_process_slots(), task_slots: state.zero_task_slots(), init_handle_table: capability.empty_handle_table(), child_handle_table: capability.empty_handle_table(), wait_table: capability.empty_wait_table(), endpoints: ipc.empty_table(), init_image: init.bootstrap_image(), child_address_space: address_space.empty_space(), child_user_frame: address_space.empty_frame(), echo_peer_state: echo_service.service_state(0, 0), log_peer_state: log_service.service_state(0, 0), spawn_observation: syscall.empty_spawn_observation(), request_receive_observation: syscall.empty_receive_observation(), echo_fanout_observation: syscall.empty_send_observation(), echo_peer_receive_observation: syscall.empty_receive_observation(), echo_reply_send_observation: syscall.empty_send_observation(), echo_reply_observation: syscall.empty_receive_observation(), log_fanout_observation: syscall.empty_send_observation(), log_peer_receive_observation: syscall.empty_receive_observation(), log_ack_send_observation: syscall.empty_send_observation(), log_ack_observation: syscall.empty_receive_observation(), aggregate_reply_send_observation: syscall.empty_send_observation(), aggregate_reply_observation: syscall.empty_receive_observation(), wait_observation: syscall.empty_wait_observation(), observation: empty_composition_flow_observation(), ready_queue: state.empty_queue() }
}

func composition_service_result(state: CompositionServiceExecutionState, succeeded: u32) CompositionServiceExecutionResult {
    return CompositionServiceExecutionResult{ state: state, succeeded: succeeded }
}

func serial_service_config(init_pid: u32, child_pid: u32, child_tid: u32, child_asid: u32, serial_endpoint_id: u32, child_translation_root: mmu.TranslationRoot) SerialServiceConfig {
    return SerialServiceConfig{ init_pid: init_pid, child_pid: child_pid, child_tid: child_tid, child_asid: child_asid, serial_endpoint_id: serial_endpoint_id, composition_endpoint_id: 0, child_translation_root: child_translation_root, wait_handle_slot: SERIAL_SERVICE_WAIT_HANDLE_SLOT, endpoint_handle_slot: SERIAL_SERVICE_ENDPOINT_HANDLE_SLOT, composition_handle_slot: 0, exit_code: SERIAL_SERVICE_EXIT_CODE, program_slot: SERIAL_SERVICE_PROGRAM_SLOT, program_object_id: SERIAL_SERVICE_PROGRAM_OBJECT_ID }
}

func serial_service_composition_config(init_pid: u32, child_pid: u32, child_tid: u32, child_asid: u32, serial_endpoint_id: u32, composition_endpoint_id: u32, child_translation_root: mmu.TranslationRoot) SerialServiceConfig {
    return SerialServiceConfig{ init_pid: init_pid, child_pid: child_pid, child_tid: child_tid, child_asid: child_asid, serial_endpoint_id: serial_endpoint_id, composition_endpoint_id: composition_endpoint_id, child_translation_root: child_translation_root, wait_handle_slot: SERIAL_SERVICE_WAIT_HANDLE_SLOT, endpoint_handle_slot: SERIAL_SERVICE_ENDPOINT_HANDLE_SLOT, composition_handle_slot: SERIAL_SERVICE_COMPOSITION_HANDLE_SLOT, exit_code: SERIAL_SERVICE_EXIT_CODE, program_slot: SERIAL_SERVICE_PROGRAM_SLOT, program_object_id: SERIAL_SERVICE_PROGRAM_OBJECT_ID }
}

func empty_serial_service_failure_observation() SerialServiceFailureObservation {
    return SerialServiceFailureObservation{ endpoint_id: 0, closed: 0, aborted_messages: 0, wake_count: 0 }
}

func empty_serial_service_execution_state() SerialServiceExecutionState {
    return SerialServiceExecutionState{ program_capability: capability.empty_slot(), gate: syscall.gate_closed(), process_slots: state.zero_process_slots(), task_slots: state.zero_task_slots(), init_handle_table: capability.empty_handle_table(), child_handle_table: capability.empty_handle_table(), wait_table: capability.empty_wait_table(), endpoints: ipc.empty_table(), init_image: init.bootstrap_image(), child_address_space: address_space.empty_space(), child_user_frame: address_space.empty_frame(), service_state: serial_service.service_state(0, 0), spawn_observation: syscall.empty_spawn_observation(), receive_observation: syscall.empty_receive_observation(), ingress: serial_service.empty_ingress_observation(), composition: serial_service.empty_composition_observation(), failure_observation: empty_serial_service_failure_observation(), wait_observation: syscall.empty_wait_observation(), ready_queue: state.empty_queue() }
}

func serial_service_result(state: SerialServiceExecutionState, succeeded: u32) SerialServiceExecutionResult {
    return SerialServiceExecutionResult{ state: state, succeeded: succeeded }
}

func phase140_serial_ingress_composition_result(serial_state: SerialServiceExecutionState, composition_state: CompositionServiceExecutionState, succeeded: u32) Phase140SerialIngressCompositionResult {
    return Phase140SerialIngressCompositionResult{ serial_state: serial_state, composition_state: composition_state, succeeded: succeeded }
}

func phase150_rebuilt_system_state(serial_state: serial_service.SerialServiceState, shell_state: shell_service.ShellServiceState, log_state: log_service.LogServiceState, echo_state: echo_service.EchoServiceState, kv_state: kv_service.KvServiceState) Phase150RebuiltSystemState {
    return Phase150RebuiltSystemState{ serial_state: serial_state, shell_state: shell_state, log_state: log_state, echo_state: echo_state, kv_state: kv_state }
}

func phase150_rebuilt_system_command_result(system: Phase150RebuiltSystemState, routing: shell_service.ShellRoutingObservation, succeeded: u32) Phase150RebuiltSystemCommandResult {
    return Phase150RebuiltSystemCommandResult{ system: system, routing: routing, succeeded: succeeded }
}

func phase150_rebuilt_system_restart_result(system: Phase150RebuiltSystemState, restart: init.ServiceRestartObservation) Phase150RebuiltSystemRestartResult {
    return Phase150RebuiltSystemRestartResult{ system: system, restart: restart }
}

func phase150_rebuilt_system_workflow_result(rebuilt_system: Phase150RebuiltSystemState, log_append_route: shell_service.ShellRoutingObservation, log_tail_route: shell_service.ShellRoutingObservation, pre_failure_set_route: shell_service.ShellRoutingObservation, pre_failure_get_route: shell_service.ShellRoutingObservation, failed_get_route: shell_service.ShellRoutingObservation, restarted_get_route: shell_service.ShellRoutingObservation, pre_failure_retention: kv_service.KvRetentionObservation, post_restart_retention: kv_service.KvRetentionObservation, restart: init.ServiceRestartObservation, event_log_retention: log_service.LogRetentionObservation, succeeded: u32) Phase150RebuiltSystemWorkflowResult {
    return Phase150RebuiltSystemWorkflowResult{ rebuilt_system: rebuilt_system, log_append_route: log_append_route, log_tail_route: log_tail_route, pre_failure_set_route: pre_failure_set_route, pre_failure_get_route: pre_failure_get_route, failed_get_route: failed_get_route, restarted_get_route: restarted_get_route, pre_failure_retention: pre_failure_retention, post_restart_retention: post_restart_retention, restart: restart, event_log_retention: event_log_retention, succeeded: succeeded }
}
