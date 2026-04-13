import capability
import echo_service
import interrupt
import ipc
import kv_service
import log_service
import serial_service
import shell_service
import state
import syscall
import timer

struct KernelExecState {
    descriptor: state.KernelDescriptor
    gate: syscall.SyscallGate
    process_slots: [3]state.ProcessSlot
    task_slots: [3]state.TaskSlot
    ready_queue: state.ReadyQueue
    timer_state: timer.TimerState
    interrupts: interrupt.InterruptController
}

struct CapabilityState {
    handle_tables: [3]capability.HandleTable
    wait_tables: [3]capability.WaitTable
}

struct IpcState {
    endpoints: ipc.EndpointTable
}

struct ServiceState {
    log_service_state: log_service.LogServiceState
    echo_service_state: echo_service.EchoServiceState
    serial_service_state: serial_service.SerialServiceState
    kv_service_state: kv_service.KvServiceState
    shell_service_state: shell_service.ShellServiceState
}

struct SystemState {
    identity: state.IdentityConfig
    kernel: KernelExecState
    capabilities: CapabilityState
    ipc: IpcState
    services: ServiceState
    contracts: state.ContractState
}

struct UartProbeFrames {
    queue_frame_one: [2]u8
    queue_frame_two: [2]u8
    queue_frame_three: [2]u8
    closed_frame: [2]u8
    completion_frame: [4]u8
}

func kernel_exec_state(descriptor: state.KernelDescriptor, gate: syscall.SyscallGate, process_slots: [3]state.ProcessSlot, task_slots: [3]state.TaskSlot, ready_queue: state.ReadyQueue, timer_state: timer.TimerState, interrupts: interrupt.InterruptController) KernelExecState {
    return KernelExecState{ descriptor: descriptor, gate: gate, process_slots: process_slots, task_slots: task_slots, ready_queue: ready_queue, timer_state: timer_state, interrupts: interrupts }
}

func capability_state(handle_tables: [3]capability.HandleTable, wait_tables: [3]capability.WaitTable) CapabilityState {
    return CapabilityState{ handle_tables: handle_tables, wait_tables: wait_tables }
}

func ipc_state(endpoints: ipc.EndpointTable) IpcState {
    return IpcState{ endpoints: endpoints }
}

func service_state(log_service_state: log_service.LogServiceState, echo_service_state: echo_service.EchoServiceState, serial_service_state: serial_service.SerialServiceState, kv_service_state: kv_service.KvServiceState, shell_service_state: shell_service.ShellServiceState) ServiceState {
    return ServiceState{ log_service_state: log_service_state, echo_service_state: echo_service_state, serial_service_state: serial_service_state, kv_service_state: kv_service_state, shell_service_state: shell_service_state }
}

func system_state(identity: state.IdentityConfig, kernel: KernelExecState, capabilities: CapabilityState, ipc: IpcState, services: ServiceState, contracts: state.ContractState) SystemState {
    return SystemState{ identity: identity, kernel: kernel, capabilities: capabilities, ipc: ipc, services: services, contracts: contracts }
}