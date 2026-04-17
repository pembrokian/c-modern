import boot
import completion_mailbox_service
import kernel_dispatch
import object_store_service
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall
import workflow_service

const FIRST_SUBMISSION_ID: u8 = 1

const FAIL_PRESSURE_SETUP: i32 = 23701
const FAIL_PRESSURE_CREATE: i32 = 23702
const FAIL_PRESSURE_FILL_SCHEDULE_BASE: i32 = 23703
const FAIL_PRESSURE_FILL_RUNNING_BASE: i32 = 23708
const FAIL_PRESSURE_FILL_DONE_BASE: i32 = 23713
const FAIL_PRESSURE_FULL_COUNT: i32 = 23718
const FAIL_PRESSURE_ISSUE: i32 = 23719
const FAIL_PRESSURE_CONSUME: i32 = 23720
const FAIL_PRESSURE_WAITING: i32 = 23721
const FAIL_PRESSURE_EXHAUSTED: i32 = 23722
const FAIL_PRESSURE_EXHAUSTED_OUTCOME: i32 = 23723
const FAIL_PRESSURE_OBJECT_VALUE_PENDING: i32 = 23724
const FAIL_PRESSURE_RESTART: i32 = 23725
const FAIL_PRESSURE_RESUMED: i32 = 23726
const FAIL_PRESSURE_RESUMED_OUTCOME: i32 = 23727
const FAIL_PRESSURE_OBJECT_VALUE_RESUMED: i32 = 23728
const FAIL_PRESSURE_RELEASE: i32 = 23729
const FAIL_PRESSURE_RECOVERED: i32 = 23730
const FAIL_PRESSURE_COUNT_AFTER_RECOVER: i32 = 23731
const FAIL_PRESSURE_DRAIN_0: i32 = 23732
const FAIL_PRESSURE_DRAIN_1: i32 = 23733
const FAIL_PRESSURE_DRAIN_2: i32 = 23734
const FAIL_PRESSURE_FETCH: i32 = 23735
const FAIL_PRESSURE_ACK_FINAL: i32 = 23736

func expect_value(effect: service_effect.Effect, value: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return false
    }
    return service_effect.effect_reply_payload(effect)[0] == value
}

func expect_workflow(effect: service_effect.Effect, status: syscall.SyscallStatus, state: u8, restart: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == state && payload[1] == restart
}

func expect_delivery_outcome(effect: service_effect.Effect, outcome: u8) bool {
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    return service_effect.effect_reply_payload(effect)[2] == outcome
}

func expect_completion(effect: service_effect.Effect, id: u8, state: u8, restart: u8, generation: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == id && payload[1] == state && payload[2] == restart && payload[3] == generation
}

func expected_task_id(submission_count: u8) u8 {
    return FIRST_SUBMISSION_ID + submission_count - 1
}

func expect_count(effect: service_effect.Effect, count: usize) bool {
    return service_effect.effect_reply_status(effect) == syscall.SyscallStatus.Ok && service_effect.effect_reply_payload_len(effect) == count
}

func run_named_object_delivery_pressure_probe() i32 {
    if !object_store_service.object_store_persist(object_store_service.object_store_init(service_topology.OBJECT_STORE_SLOT.pid, 1)) {
        return FAIL_PRESSURE_SETUP
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_create(7, 41))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESSURE_CREATE
    }

    workflow_submission_count: u8 = 0
    for id in 11..15 {
        effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(u8(id), 1))
        if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
            return FAIL_PRESSURE_FILL_SCHEDULE_BASE + (id - 11)
        }
        workflow_submission_count = workflow_submission_count + 1

        effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(u8(id)))
        if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_RUNNING, workflow_service.WORKFLOW_RESTART_NONE) {
            return FAIL_PRESSURE_FILL_RUNNING_BASE + (id - 11)
        }
        if service_effect.effect_reply_payload(effect)[2] != expected_task_id(workflow_submission_count) {
            return FAIL_PRESSURE_FILL_RUNNING_BASE + (id - 11)
        }

        effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(u8(id)))
        if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_DONE, workflow_service.WORKFLOW_RESTART_NONE) {
            return FAIL_PRESSURE_FILL_DONE_BASE + (id - 11)
        }
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_count())
    if !expect_count(effect, completion_mailbox_service.MAILBOX_CAPACITY) {
        return FAIL_PRESSURE_FULL_COUNT
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_issue_object_update(7, 99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_PRESSURE_ISSUE
    }
    lease_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lease_consume_object_update(lease_id))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok || service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_PRESSURE_CONSUME
    }
    update_id := service_effect.effect_reply_payload(effect)[0]

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_WAITING, workflow_service.WORKFLOW_RESTART_NONE) {
        return FAIL_PRESSURE_WAITING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Exhausted, workflow_service.WORKFLOW_STATE_DELIVERING, workflow_service.WORKFLOW_RESTART_NONE) {
        return FAIL_PRESSURE_EXHAUSTED
    }
    if !expect_delivery_outcome(effect, workflow_service.WORKFLOW_STATE_OBJECT_UPDATED) {
        return FAIL_PRESSURE_EXHAUSTED_OUTCOME
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 99) {
        return FAIL_PRESSURE_OBJECT_VALUE_PENDING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_WORKFLOW))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_WORKFLOW, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_PRESSURE_RESTART
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.WouldBlock, workflow_service.WORKFLOW_STATE_DELIVERING, workflow_service.WORKFLOW_RESTART_RESUMED) {
        return FAIL_PRESSURE_RESUMED
    }
    if !expect_delivery_outcome(effect, workflow_service.WORKFLOW_STATE_OBJECT_UPDATED) {
        return FAIL_PRESSURE_RESUMED_OUTCOME
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 99) {
        return FAIL_PRESSURE_OBJECT_VALUE_RESUMED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESSURE_RELEASE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(update_id))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_OBJECT_UPDATED, workflow_service.WORKFLOW_RESTART_RESUMED) {
        return FAIL_PRESSURE_RECOVERED
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_count())
    if !expect_count(effect, completion_mailbox_service.MAILBOX_CAPACITY) {
        return FAIL_PRESSURE_COUNT_AFTER_RECOVER
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESSURE_DRAIN_0
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESSURE_DRAIN_1
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESSURE_DRAIN_2
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    if !expect_completion(effect, update_id, workflow_service.WORKFLOW_STATE_OBJECT_UPDATED, workflow_service.WORKFLOW_RESTART_RESUMED, 1) {
        return FAIL_PRESSURE_FETCH
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_PRESSURE_ACK_FINAL
    }

    return 0
}