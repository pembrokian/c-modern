import interrupt
import ipc

const UART_MESSAGE_ID_BASE: u32 = 134

struct UartDevice {
    receive_vector: u32
    service_endpoint_id: u32
    staged_payload_len: usize
    staged_payload: [4]u8
    staging_ready: u32
    ack_count: u32
    ingress_count: u32
    retire_count: u32
    last_vector: u32
    last_source_actor: u32
    last_published_payload_len: usize
    last_published_payload: [4]u8
}

struct UartIngressObservation {
    interrupt_vector: u32
    source_actor: u32
    service_endpoint_id: u32
    staged_payload_len: usize
    staged_payload0: u8
    staged_payload1: u8
    staged_payload2: u8
    staged_payload3: u8
    published_payload_len: usize
    published_payload0: u8
    published_payload1: u8
    published_payload2: u8
    published_payload3: u8
    retired_payload_len: usize
    retired_payload0: u8
    retired_payload1: u8
    retired_payload2: u8
    retired_payload3: u8
    ack_count: u32
    ingress_count: u32
    retire_count: u32
    publish: ipc.RuntimePublishObservation
}

struct UartInterruptResult {
    device: UartDevice
    endpoints: ipc.EndpointTable
    observation: UartIngressObservation
    handled: u32
}

func empty_device() UartDevice {
    payload: [4]u8 = ipc.zero_payload()
    return UartDevice{ receive_vector: 0, service_endpoint_id: 0, staged_payload_len: 0, staged_payload: payload, staging_ready: 0, ack_count: 0, ingress_count: 0, retire_count: 0, last_vector: 0, last_source_actor: 0, last_published_payload_len: 0, last_published_payload: payload }
}

func configure_receive(device: UartDevice, receive_vector: u32, service_endpoint_id: u32) UartDevice {
    return UartDevice{ receive_vector: receive_vector, service_endpoint_id: service_endpoint_id, staged_payload_len: device.staged_payload_len, staged_payload: device.staged_payload, staging_ready: device.staging_ready, ack_count: device.ack_count, ingress_count: device.ingress_count, retire_count: device.retire_count, last_vector: device.last_vector, last_source_actor: device.last_source_actor, last_published_payload_len: device.last_published_payload_len, last_published_payload: device.last_published_payload }
}

func stage_receive_frame(device: UartDevice, payload_len: usize, payload: [4]u8) UartDevice {
    return UartDevice{ receive_vector: device.receive_vector, service_endpoint_id: device.service_endpoint_id, staged_payload_len: payload_len, staged_payload: payload, staging_ready: 1, ack_count: device.ack_count, ingress_count: device.ingress_count, retire_count: device.retire_count, last_vector: device.last_vector, last_source_actor: device.last_source_actor, last_published_payload_len: device.last_published_payload_len, last_published_payload: device.last_published_payload }
}

func stage_receive_byte(device: UartDevice, received_byte: u8) UartDevice {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = received_byte
    return stage_receive_frame(device, 1, payload)
}

func empty_ingress_observation() UartIngressObservation {
    return UartIngressObservation{ interrupt_vector: 0, source_actor: 0, service_endpoint_id: 0, staged_payload_len: 0, staged_payload0: 0, staged_payload1: 0, staged_payload2: 0, staged_payload3: 0, published_payload_len: 0, published_payload0: 0, published_payload1: 0, published_payload2: 0, published_payload3: 0, retired_payload_len: 0, retired_payload0: 0, retired_payload1: 0, retired_payload2: 0, retired_payload3: 0, ack_count: 0, ingress_count: 0, retire_count: 0, publish: ipc.empty_runtime_publish_observation() }
}

func handle_receive_interrupt(device: UartDevice, entry: interrupt.InterruptEntry, dispatch: interrupt.InterruptDispatchResult, endpoints: ipc.EndpointTable, kernel_pid: u32) UartInterruptResult {
    if interrupt.dispatch_kind_score(dispatch.kind) != 4 {
        return UartInterruptResult{ device: device, endpoints: endpoints, observation: empty_ingress_observation(), handled: 0 }
    }
    if device.staging_ready == 0 {
        return UartInterruptResult{ device: device, endpoints: endpoints, observation: empty_ingress_observation(), handled: 0 }
    }
    publish: ipc.RuntimePublishResult = ipc.publish_runtime_frame(endpoints, device.service_endpoint_id, kernel_pid, UART_MESSAGE_ID_BASE + device.ingress_count, device.staged_payload_len, device.staged_payload)
    retired_payload: [4]u8 = ipc.zero_payload()
    next_device: UartDevice = UartDevice{ receive_vector: device.receive_vector, service_endpoint_id: device.service_endpoint_id, staged_payload_len: 0, staged_payload: retired_payload, staging_ready: 0, ack_count: device.ack_count + 1, ingress_count: device.ingress_count + 1, retire_count: device.retire_count + 1, last_vector: entry.frame.vector, last_source_actor: entry.frame.source_actor, last_published_payload_len: device.staged_payload_len, last_published_payload: device.staged_payload }
    return UartInterruptResult{ device: next_device, endpoints: publish.endpoints, observation: UartIngressObservation{ interrupt_vector: entry.frame.vector, source_actor: entry.frame.source_actor, service_endpoint_id: device.service_endpoint_id, staged_payload_len: device.staged_payload_len, staged_payload0: device.staged_payload[0], staged_payload1: device.staged_payload[1], staged_payload2: device.staged_payload[2], staged_payload3: device.staged_payload[3], published_payload_len: next_device.last_published_payload_len, published_payload0: next_device.last_published_payload[0], published_payload1: next_device.last_published_payload[1], published_payload2: next_device.last_published_payload[2], published_payload3: next_device.last_published_payload[3], retired_payload_len: next_device.staged_payload_len, retired_payload0: next_device.staged_payload[0], retired_payload1: next_device.staged_payload[1], retired_payload2: next_device.staged_payload[2], retired_payload3: next_device.staged_payload[3], ack_count: next_device.ack_count, ingress_count: next_device.ingress_count, retire_count: next_device.retire_count, publish: publish.observation }, handled: 1 }
}
