import interrupt
import ipc

const UART_MESSAGE_ID_BASE: u32 = 134
const UART_COMPLETION_MESSAGE_ID_BASE: u32 = 1370

enum UartFailureKind {
    None,
    QueueFull,
    EndpointClosed,
    EndpointUnavailable,
}

struct UartDevice {
    receive_vector: u32
    completion_vector: u32
    service_endpoint_id: u32
    staged_payload_len: usize
    staged_payload: [4]u8
    staging_ready: u32
    completion_payload_len: usize
    completion_payload: [4]u8
    completion_ready: u32
    ack_count: u32
    ingress_count: u32
    retire_count: u32
    completion_ingress_count: u32
    completion_retire_count: u32
    dropped_count: u32
    queue_full_drop_count: u32
    endpoint_closed_drop_count: u32
    endpoint_invalid_drop_count: u32
    last_failure: UartFailureKind
    last_vector: u32
    last_source_actor: u32
    last_published_payload_len: usize
    last_published_payload: [4]u8
    last_completion_payload_len: usize
    last_completion_payload: [4]u8
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
    dropped_count: u32
    queue_full_drop_count: u32
    endpoint_closed_drop_count: u32
    endpoint_invalid_drop_count: u32
    failure_kind: UartFailureKind
    publish: ipc.RuntimePublishObservation
}

struct UartInterruptResult {
    device: UartDevice
    endpoints: ipc.EndpointTable
    observation: UartIngressObservation
    handled: u32
}

struct UartCompletionObservation {
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
    completion_ingress_count: u32
    completion_retire_count: u32
    publish: ipc.RuntimePublishObservation
}

struct UartCompletionInterruptResult {
    device: UartDevice
    endpoints: ipc.EndpointTable
    observation: UartCompletionObservation
    handled: u32
}

func empty_device() UartDevice {
    payload: [4]u8 = ipc.zero_payload()
    return UartDevice{ receive_vector: 0, completion_vector: 0, service_endpoint_id: 0, staged_payload_len: 0, staged_payload: payload, staging_ready: 0, completion_payload_len: 0, completion_payload: payload, completion_ready: 0, ack_count: 0, ingress_count: 0, retire_count: 0, completion_ingress_count: 0, completion_retire_count: 0, dropped_count: 0, queue_full_drop_count: 0, endpoint_closed_drop_count: 0, endpoint_invalid_drop_count: 0, last_failure: UartFailureKind.None, last_vector: 0, last_source_actor: 0, last_published_payload_len: 0, last_published_payload: payload, last_completion_payload_len: 0, last_completion_payload: payload }
}

func configure_receive(device: UartDevice, receive_vector: u32, service_endpoint_id: u32) UartDevice {
    return UartDevice{ receive_vector: receive_vector, completion_vector: device.completion_vector, service_endpoint_id: service_endpoint_id, staged_payload_len: device.staged_payload_len, staged_payload: device.staged_payload, staging_ready: device.staging_ready, completion_payload_len: device.completion_payload_len, completion_payload: device.completion_payload, completion_ready: device.completion_ready, ack_count: device.ack_count, ingress_count: device.ingress_count, retire_count: device.retire_count, completion_ingress_count: device.completion_ingress_count, completion_retire_count: device.completion_retire_count, dropped_count: device.dropped_count, queue_full_drop_count: device.queue_full_drop_count, endpoint_closed_drop_count: device.endpoint_closed_drop_count, endpoint_invalid_drop_count: device.endpoint_invalid_drop_count, last_failure: device.last_failure, last_vector: device.last_vector, last_source_actor: device.last_source_actor, last_published_payload_len: device.last_published_payload_len, last_published_payload: device.last_published_payload, last_completion_payload_len: device.last_completion_payload_len, last_completion_payload: device.last_completion_payload }
}

func configure_receive_completion(device: UartDevice, completion_vector: u32) UartDevice {
    return UartDevice{ receive_vector: device.receive_vector, completion_vector: completion_vector, service_endpoint_id: device.service_endpoint_id, staged_payload_len: device.staged_payload_len, staged_payload: device.staged_payload, staging_ready: device.staging_ready, completion_payload_len: device.completion_payload_len, completion_payload: device.completion_payload, completion_ready: device.completion_ready, ack_count: device.ack_count, ingress_count: device.ingress_count, retire_count: device.retire_count, completion_ingress_count: device.completion_ingress_count, completion_retire_count: device.completion_retire_count, dropped_count: device.dropped_count, queue_full_drop_count: device.queue_full_drop_count, endpoint_closed_drop_count: device.endpoint_closed_drop_count, endpoint_invalid_drop_count: device.endpoint_invalid_drop_count, last_failure: device.last_failure, last_vector: device.last_vector, last_source_actor: device.last_source_actor, last_published_payload_len: device.last_published_payload_len, last_published_payload: device.last_published_payload, last_completion_payload_len: device.last_completion_payload_len, last_completion_payload: device.last_completion_payload }
}

func stage_receive_frame(device: UartDevice, payload_len: usize, payload: [4]u8) UartDevice {
    return UartDevice{ receive_vector: device.receive_vector, completion_vector: device.completion_vector, service_endpoint_id: device.service_endpoint_id, staged_payload_len: payload_len, staged_payload: payload, staging_ready: 1, completion_payload_len: device.completion_payload_len, completion_payload: device.completion_payload, completion_ready: device.completion_ready, ack_count: device.ack_count, ingress_count: device.ingress_count, retire_count: device.retire_count, completion_ingress_count: device.completion_ingress_count, completion_retire_count: device.completion_retire_count, dropped_count: device.dropped_count, queue_full_drop_count: device.queue_full_drop_count, endpoint_closed_drop_count: device.endpoint_closed_drop_count, endpoint_invalid_drop_count: device.endpoint_invalid_drop_count, last_failure: device.last_failure, last_vector: device.last_vector, last_source_actor: device.last_source_actor, last_published_payload_len: device.last_published_payload_len, last_published_payload: device.last_published_payload, last_completion_payload_len: device.last_completion_payload_len, last_completion_payload: device.last_completion_payload }
}

func stage_receive_completion_frame(device: UartDevice, payload_len: usize, payload: [4]u8) UartDevice {
    return UartDevice{ receive_vector: device.receive_vector, completion_vector: device.completion_vector, service_endpoint_id: device.service_endpoint_id, staged_payload_len: device.staged_payload_len, staged_payload: device.staged_payload, staging_ready: device.staging_ready, completion_payload_len: payload_len, completion_payload: payload, completion_ready: 1, ack_count: device.ack_count, ingress_count: device.ingress_count, retire_count: device.retire_count, completion_ingress_count: device.completion_ingress_count, completion_retire_count: device.completion_retire_count, dropped_count: device.dropped_count, queue_full_drop_count: device.queue_full_drop_count, endpoint_closed_drop_count: device.endpoint_closed_drop_count, endpoint_invalid_drop_count: device.endpoint_invalid_drop_count, last_failure: device.last_failure, last_vector: device.last_vector, last_source_actor: device.last_source_actor, last_published_payload_len: device.last_published_payload_len, last_published_payload: device.last_published_payload, last_completion_payload_len: device.last_completion_payload_len, last_completion_payload: device.last_completion_payload }
}

func stage_receive_byte(device: UartDevice, received_byte: u8) UartDevice {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = received_byte
    return stage_receive_frame(device, 1, payload)
}

func empty_ingress_observation() UartIngressObservation {
    return UartIngressObservation{ interrupt_vector: 0, source_actor: 0, service_endpoint_id: 0, staged_payload_len: 0, staged_payload0: 0, staged_payload1: 0, staged_payload2: 0, staged_payload3: 0, published_payload_len: 0, published_payload0: 0, published_payload1: 0, published_payload2: 0, published_payload3: 0, retired_payload_len: 0, retired_payload0: 0, retired_payload1: 0, retired_payload2: 0, retired_payload3: 0, ack_count: 0, ingress_count: 0, retire_count: 0, dropped_count: 0, queue_full_drop_count: 0, endpoint_closed_drop_count: 0, endpoint_invalid_drop_count: 0, failure_kind: UartFailureKind.None, publish: ipc.empty_runtime_publish_observation() }
}

func empty_completion_observation() UartCompletionObservation {
    return UartCompletionObservation{ interrupt_vector: 0, source_actor: 0, service_endpoint_id: 0, staged_payload_len: 0, staged_payload0: 0, staged_payload1: 0, staged_payload2: 0, staged_payload3: 0, published_payload_len: 0, published_payload0: 0, published_payload1: 0, published_payload2: 0, published_payload3: 0, retired_payload_len: 0, retired_payload0: 0, retired_payload1: 0, retired_payload2: 0, retired_payload3: 0, completion_ingress_count: 0, completion_retire_count: 0, publish: ipc.empty_runtime_publish_observation() }
}

func failure_kind_score(kind: UartFailureKind) i32 {
    switch kind {
    case UartFailureKind.None:
        return 1
    case UartFailureKind.QueueFull:
        return 2
    case UartFailureKind.EndpointClosed:
        return 4
    case UartFailureKind.EndpointUnavailable:
        return 8
    default:
        return 0
    }
    return 0
}

func publish_failure_kind(observation: ipc.RuntimePublishObservation) UartFailureKind {
    if observation.queue_full != 0 {
        return UartFailureKind.QueueFull
    }
    if observation.endpoint_closed != 0 {
        return UartFailureKind.EndpointClosed
    }
    if observation.endpoint_valid == 0 {
        return UartFailureKind.EndpointUnavailable
    }
    return UartFailureKind.None
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
    failure_kind: UartFailureKind = publish_failure_kind(publish.observation)
    dropped_count: u32 = device.dropped_count
    queue_full_drop_count: u32 = device.queue_full_drop_count
    endpoint_closed_drop_count: u32 = device.endpoint_closed_drop_count
    endpoint_invalid_drop_count: u32 = device.endpoint_invalid_drop_count
    if failure_kind_score(failure_kind) != 1 {
        dropped_count = dropped_count + 1
        if failure_kind_score(failure_kind) == 2 {
            queue_full_drop_count = queue_full_drop_count + 1
        }
        if failure_kind_score(failure_kind) == 4 {
            endpoint_closed_drop_count = endpoint_closed_drop_count + 1
        }
        if failure_kind_score(failure_kind) == 8 {
            endpoint_invalid_drop_count = endpoint_invalid_drop_count + 1
        }
    }
    next_device: UartDevice = UartDevice{ receive_vector: device.receive_vector, completion_vector: device.completion_vector, service_endpoint_id: device.service_endpoint_id, staged_payload_len: 0, staged_payload: retired_payload, staging_ready: 0, completion_payload_len: device.completion_payload_len, completion_payload: device.completion_payload, completion_ready: device.completion_ready, ack_count: device.ack_count + 1, ingress_count: device.ingress_count + 1, retire_count: device.retire_count + 1, completion_ingress_count: device.completion_ingress_count, completion_retire_count: device.completion_retire_count, dropped_count: dropped_count, queue_full_drop_count: queue_full_drop_count, endpoint_closed_drop_count: endpoint_closed_drop_count, endpoint_invalid_drop_count: endpoint_invalid_drop_count, last_failure: failure_kind, last_vector: entry.frame.vector, last_source_actor: entry.frame.source_actor, last_published_payload_len: device.staged_payload_len, last_published_payload: device.staged_payload, last_completion_payload_len: device.last_completion_payload_len, last_completion_payload: device.last_completion_payload }
    return UartInterruptResult{ device: next_device, endpoints: publish.endpoints, observation: UartIngressObservation{ interrupt_vector: entry.frame.vector, source_actor: entry.frame.source_actor, service_endpoint_id: device.service_endpoint_id, staged_payload_len: device.staged_payload_len, staged_payload0: device.staged_payload[0], staged_payload1: device.staged_payload[1], staged_payload2: device.staged_payload[2], staged_payload3: device.staged_payload[3], published_payload_len: next_device.last_published_payload_len, published_payload0: next_device.last_published_payload[0], published_payload1: next_device.last_published_payload[1], published_payload2: next_device.last_published_payload[2], published_payload3: next_device.last_published_payload[3], retired_payload_len: next_device.staged_payload_len, retired_payload0: next_device.staged_payload[0], retired_payload1: next_device.staged_payload[1], retired_payload2: next_device.staged_payload[2], retired_payload3: next_device.staged_payload[3], ack_count: next_device.ack_count, ingress_count: next_device.ingress_count, retire_count: next_device.retire_count, dropped_count: next_device.dropped_count, queue_full_drop_count: next_device.queue_full_drop_count, endpoint_closed_drop_count: next_device.endpoint_closed_drop_count, endpoint_invalid_drop_count: next_device.endpoint_invalid_drop_count, failure_kind: next_device.last_failure, publish: publish.observation }, handled: 1 }
}

func handle_receive_completion_interrupt(device: UartDevice, entry: interrupt.InterruptEntry, dispatch: interrupt.InterruptDispatchResult, endpoints: ipc.EndpointTable, kernel_pid: u32) UartCompletionInterruptResult {
    if interrupt.dispatch_kind_score(dispatch.kind) != 8 {
        return UartCompletionInterruptResult{ device: device, endpoints: endpoints, observation: empty_completion_observation(), handled: 0 }
    }
    if device.completion_ready == 0 {
        return UartCompletionInterruptResult{ device: device, endpoints: endpoints, observation: empty_completion_observation(), handled: 0 }
    }
    publish: ipc.RuntimePublishResult = ipc.publish_runtime_frame(endpoints, device.service_endpoint_id, kernel_pid, UART_COMPLETION_MESSAGE_ID_BASE + device.completion_ingress_count, device.completion_payload_len, device.completion_payload)
    retired_payload: [4]u8 = ipc.zero_payload()
    next_device: UartDevice = UartDevice{ receive_vector: device.receive_vector, completion_vector: device.completion_vector, service_endpoint_id: device.service_endpoint_id, staged_payload_len: device.staged_payload_len, staged_payload: device.staged_payload, staging_ready: device.staging_ready, completion_payload_len: 0, completion_payload: retired_payload, completion_ready: 0, ack_count: device.ack_count, ingress_count: device.ingress_count, retire_count: device.retire_count, completion_ingress_count: device.completion_ingress_count + 1, completion_retire_count: device.completion_retire_count + 1, dropped_count: device.dropped_count, queue_full_drop_count: device.queue_full_drop_count, endpoint_closed_drop_count: device.endpoint_closed_drop_count, endpoint_invalid_drop_count: device.endpoint_invalid_drop_count, last_failure: device.last_failure, last_vector: entry.frame.vector, last_source_actor: entry.frame.source_actor, last_published_payload_len: device.last_published_payload_len, last_published_payload: device.last_published_payload, last_completion_payload_len: device.completion_payload_len, last_completion_payload: device.completion_payload }
    return UartCompletionInterruptResult{ device: next_device, endpoints: publish.endpoints, observation: UartCompletionObservation{ interrupt_vector: entry.frame.vector, source_actor: entry.frame.source_actor, service_endpoint_id: device.service_endpoint_id, staged_payload_len: device.completion_payload_len, staged_payload0: device.completion_payload[0], staged_payload1: device.completion_payload[1], staged_payload2: device.completion_payload[2], staged_payload3: device.completion_payload[3], published_payload_len: next_device.last_completion_payload_len, published_payload0: next_device.last_completion_payload[0], published_payload1: next_device.last_completion_payload[1], published_payload2: next_device.last_completion_payload[2], published_payload3: next_device.last_completion_payload[3], retired_payload_len: next_device.completion_payload_len, retired_payload0: next_device.completion_payload[0], retired_payload1: next_device.completion_payload[1], retired_payload2: next_device.completion_payload[2], retired_payload3: next_device.completion_payload[3], completion_ingress_count: next_device.completion_ingress_count, completion_retire_count: next_device.completion_retire_count, publish: publish.observation }, handled: 1 }
}
