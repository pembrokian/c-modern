// Transport adapter and protocol-script helpers for kernel scenarios.

import serial_protocol
import service_topology
import syscall

struct SerialRoute {
    endpoint: u32
    pid: u32
}

const DEFAULT_SERIAL_ROUTE: SerialRoute = SerialRoute{ endpoint: service_topology.SERIAL_ENDPOINT_ID, pid: 1 }

// serial_obs wraps a protocol payload into a ReceiveObservation.
// endpoint and pid are explicit: swap the endpoint to route to a different
// service; swap pid to simulate a different client.
func serial_obs(endpoint: u32, pid: u32, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint, source_pid: pid, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// cmd_* are single-client convenience wrappers: serial endpoint, pid=1.
func cmd_log_append(value: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_log_append(value))
}

func cmd_log_tail() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_log_tail())
}

func cmd_echo(left: u8, right: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_echo(left, right))
}

func cmd_kv_set(key: u8, value: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_kv_set(key, value))
}

func cmd_kv_get(key: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_kv_get(key))
}

func cmd_lifecycle_query(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_query(target))
}

func cmd_lifecycle_authority(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_authority(target))
}

func cmd_lifecycle_state(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_state(target))
}

func cmd_lifecycle_durability(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_durability(target))
}

func cmd_lifecycle_identity(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_identity(target))
}

func cmd_lifecycle_policy(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_policy(target))
}

func cmd_lifecycle_summary(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_summary(target))
}

func cmd_lifecycle_compare(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_compare(target))
}

func cmd_lifecycle_restart(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_restart(target))
}

func cmd_queue_enqueue(value: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_queue_enqueue(value))
}

func cmd_queue_dequeue() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_queue_dequeue())
}

func cmd_queue_count() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_queue_count())
}

func cmd_queue_peek() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_queue_peek())
}

func cmd_queue_stall_count() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_queue_stall_count())
}

func cmd_ticket_issue() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_ticket_issue())
}

func cmd_ticket_use(epoch: u8, id: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_ticket_use(epoch, id))
}

func cmd_file_create(name: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_file_create(name))
}

func cmd_file_write(name: u8, val: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_file_write(name, val))
}

func cmd_file_read(name: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_file_read(name))
}

func cmd_file_count() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_file_count())
}

func cmd_timer_create(id: u8, due: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_timer_create(id, due))
}

func cmd_timer_cancel(id: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_timer_cancel(id))
}

func cmd_timer_query(id: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_timer_query(id))
}

func cmd_timer_expired(window: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_timer_expired(window))
}

func cmd_task_submit(opcode: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_task_submit(opcode))
}

func cmd_task_query(id: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_task_query(id))
}

func cmd_task_cancel(id: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_task_cancel(id))
}

func cmd_task_complete(id: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_task_complete(id))
}

func cmd_task_list(window: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_task_list(window))
}

func cmd_journal_append(name: u8, value: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_journal_append(name, value))
}

func cmd_journal_replay(name: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_journal_replay(name))
}

func cmd_journal_clear(name: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_journal_clear(name))
}

func cmd_workflow_schedule(id: u8, due: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_workflow_schedule(id, due))
}

func cmd_workflow_query(id: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_workflow_query(id))
}

func cmd_workflow_cancel(id: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_workflow_cancel(id))
}

func cmd_completion_fetch() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_completion_fetch())
}

func cmd_completion_ack() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_completion_ack())
}

func cmd_completion_count() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_completion_count())
}

func kv_count_obs(pid: u32) syscall.ReceiveObservation {
    return serial_obs(service_topology.SERIAL_ENDPOINT_ID, pid, serial_protocol.encode_kv_count())
}
