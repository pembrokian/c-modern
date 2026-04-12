import interrupt
import ipc

const UART_MESSAGE_ID_BASE: u32 = 134

struct UartDevice {
    receive_vector: u32
    service_endpoint_id: u32
    data_register: u8
    receive_ready: u32
    ack_count: u32
    ingress_count: u32
    last_vector: u32
    last_source_actor: u32
    last_received_byte: u8
}

struct UartIngressObservation {
    interrupt_vector: u32
    source_actor: u32
    service_endpoint_id: u32
    received_byte: u8
    ack_count: u32
    ingress_count: u32
    publish: ipc.RuntimePublishObservation
}

struct UartInterruptResult {
    device: UartDevice
    endpoints: ipc.EndpointTable
    observation: UartIngressObservation
    handled: u32
}

func empty_device() UartDevice {
    return UartDevice{ receive_vector: 0, service_endpoint_id: 0, data_register: 0, receive_ready: 0, ack_count: 0, ingress_count: 0, last_vector: 0, last_source_actor: 0, last_received_byte: 0 }
}

func configure_receive(device: UartDevice, receive_vector: u32, service_endpoint_id: u32) UartDevice {
    return UartDevice{ receive_vector: receive_vector, service_endpoint_id: service_endpoint_id, data_register: device.data_register, receive_ready: device.receive_ready, ack_count: device.ack_count, ingress_count: device.ingress_count, last_vector: device.last_vector, last_source_actor: device.last_source_actor, last_received_byte: device.last_received_byte }
}

func stage_receive_byte(device: UartDevice, received_byte: u8) UartDevice {
    return UartDevice{ receive_vector: device.receive_vector, service_endpoint_id: device.service_endpoint_id, data_register: received_byte, receive_ready: 1, ack_count: device.ack_count, ingress_count: device.ingress_count, last_vector: device.last_vector, last_source_actor: device.last_source_actor, last_received_byte: device.last_received_byte }
}

func empty_ingress_observation() UartIngressObservation {
    return UartIngressObservation{ interrupt_vector: 0, source_actor: 0, service_endpoint_id: 0, received_byte: 0, ack_count: 0, ingress_count: 0, publish: ipc.empty_runtime_publish_observation() }
}

func handle_receive_interrupt(device: UartDevice, entry: interrupt.InterruptEntry, dispatch: interrupt.InterruptDispatchResult, endpoints: ipc.EndpointTable, kernel_pid: u32) UartInterruptResult {
    if interrupt.dispatch_kind_score(dispatch.kind) != 4 {
        return UartInterruptResult{ device: device, endpoints: endpoints, observation: empty_ingress_observation(), handled: 0 }
    }
    if device.receive_ready == 0 {
        return UartInterruptResult{ device: device, endpoints: endpoints, observation: empty_ingress_observation(), handled: 0 }
    }
    publish: ipc.RuntimePublishResult = ipc.publish_runtime_byte(endpoints, device.service_endpoint_id, kernel_pid, UART_MESSAGE_ID_BASE + device.ingress_count, device.data_register)
    next_device: UartDevice = UartDevice{ receive_vector: device.receive_vector, service_endpoint_id: device.service_endpoint_id, data_register: device.data_register, receive_ready: 0, ack_count: device.ack_count + 1, ingress_count: device.ingress_count + 1, last_vector: entry.frame.vector, last_source_actor: entry.frame.source_actor, last_received_byte: device.data_register }
    return UartInterruptResult{ device: next_device, endpoints: publish.endpoints, observation: UartIngressObservation{ interrupt_vector: entry.frame.vector, source_actor: entry.frame.source_actor, service_endpoint_id: device.service_endpoint_id, received_byte: device.data_register, ack_count: next_device.ack_count, ingress_count: next_device.ingress_count, publish: publish.observation }, handled: 1 }
}