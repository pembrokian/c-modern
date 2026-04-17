import boot
import completion_mailbox_service
import kernel_dispatch
import scenario_transport
import service_effect
import syscall
import workflow_service

const FIRST_SUBMISSION_ID: u8 = 1

const FAIL_COMPLETION_SCHEDULE_0: i32 = 2324
const FAIL_COMPLETION_RUNNING_0: i32 = 2325
const FAIL_COMPLETION_DONE_0: i32 = 2326
const FAIL_COMPLETION_COUNT_0_STATUS: i32 = 2327
const FAIL_COMPLETION_COUNT_0_LEN: i32 = 2328
const FAIL_COMPLETION_FETCH_STATUS: i32 = 2329
const FAIL_COMPLETION_FETCH_ID: i32 = 2330
const FAIL_COMPLETION_FETCH_STATE: i32 = 2331
const FAIL_COMPLETION_ACK_STATUS: i32 = 2332
const FAIL_COMPLETION_COUNT_1_STATUS: i32 = 2333
const FAIL_COMPLETION_COUNT_1_LEN: i32 = 2334
const FAIL_COMPLETION_FILL_SCHEDULE_BASE: i32 = 2335
const FAIL_COMPLETION_FILL_RUNNING_BASE: i32 = 2340
const FAIL_COMPLETION_FILL_DONE_BASE: i32 = 2345
const FAIL_COMPLETION_FILL_COUNT_STATUS: i32 = 2350
const FAIL_COMPLETION_FILL_COUNT_LEN: i32 = 2351
const FAIL_COMPLETION_STALL_SCHEDULE: i32 = 2352
const FAIL_COMPLETION_STALL_RUNNING: i32 = 2353
const FAIL_COMPLETION_STALL_FIRST_STATUS: i32 = 2354
const FAIL_COMPLETION_STALL_FIRST_STATE: i32 = 2355
const FAIL_COMPLETION_STALL_FIRST_OUTCOME: i32 = 2356
const FAIL_COMPLETION_STALL_SECOND_STATUS: i32 = 2357
const FAIL_COMPLETION_STALL_SECOND_STATE: i32 = 2358
const FAIL_COMPLETION_RELEASE_ACK: i32 = 2359
const FAIL_COMPLETION_RECOVER_STATUS: i32 = 2360
const FAIL_COMPLETION_RECOVER_STATE: i32 = 2361
const FAIL_COMPLETION_FINAL_COUNT_STATUS: i32 = 2362
const FAIL_COMPLETION_FINAL_COUNT_LEN: i32 = 2363

func expect_reply_shape(effect: service_effect.Effect, status: syscall.SyscallStatus, payload_len: usize, code: i32) i32 {
    if service_effect.effect_reply_status(effect) != status {
        return code
    }
    if service_effect.effect_reply_payload_len(effect) != payload_len {
        return code
    }
    return 0
}

func expect_workflow(effect: service_effect.Effect, status: syscall.SyscallStatus, state: u8, restart: u8, task: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == state && payload[1] == restart && payload[2] == task
}

func expect_delivery_payload(effect: service_effect.Effect, state: u8, outcome: u8, state_code: i32, outcome_code: i32) i32 {
    payload := service_effect.effect_reply_payload(effect)
    if payload[0] != state {
        return state_code
    }
    if payload[2] != outcome {
        return outcome_code
    }
    return 0
}

func expected_task_id(submission_count: u8) u8 {
    return FIRST_SUBMISSION_ID + submission_count - 1
}

func run_completion_mailbox_probe() i32 {
    state := boot.kernel_init()
    expected_completion_count: usize = completion_mailbox_service.MAILBOX_CAPACITY
    workflow_submission_count: u8 = 0

    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(7, 1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COMPLETION_SCHEDULE_0
    }
    workflow_submission_count = workflow_submission_count + 1

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(7))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_RUNNING, workflow_service.WORKFLOW_RESTART_NONE, expected_task_id(workflow_submission_count)) {
        return FAIL_COMPLETION_RUNNING_0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(7))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_DONE, workflow_service.WORKFLOW_RESTART_NONE, 0) {
        return FAIL_COMPLETION_DONE_0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_count())
    count_check := expect_reply_shape(effect, syscall.SyscallStatus.Ok, 1, FAIL_COMPLETION_COUNT_0_STATUS)
    if count_check != 0 {
        if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
            return FAIL_COMPLETION_COUNT_0_STATUS
        }
        return FAIL_COMPLETION_COUNT_0_LEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_fetch())
    fetch_check := expect_reply_shape(effect, syscall.SyscallStatus.Ok, 4, FAIL_COMPLETION_FETCH_STATUS)
    if fetch_check != 0 {
        return FAIL_COMPLETION_FETCH_STATUS
    }
    payload := service_effect.effect_reply_payload(effect)
    if payload[0] != 7 {
        return FAIL_COMPLETION_FETCH_ID
    }
    if payload[1] != workflow_service.WORKFLOW_STATE_DONE {
        return FAIL_COMPLETION_FETCH_STATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COMPLETION_ACK_STATUS
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_count())
    empty_count_check := expect_reply_shape(effect, syscall.SyscallStatus.Ok, 0, FAIL_COMPLETION_COUNT_1_STATUS)
    if empty_count_check != 0 {
        if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
            return FAIL_COMPLETION_COUNT_1_STATUS
        }
        return FAIL_COMPLETION_COUNT_1_LEN
    }

    for id in 11..15 {
        effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(u8(id), 1))
        if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
            return FAIL_COMPLETION_FILL_SCHEDULE_BASE + (id - 11)
        }
        workflow_submission_count = workflow_submission_count + 1
        effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(u8(id)))
        if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_RUNNING, workflow_service.WORKFLOW_RESTART_NONE, expected_task_id(workflow_submission_count)) {
            return FAIL_COMPLETION_FILL_RUNNING_BASE + (id - 11)
        }
        effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(u8(id)))
        if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_DONE, workflow_service.WORKFLOW_RESTART_NONE, 0) {
            return FAIL_COMPLETION_FILL_DONE_BASE + (id - 11)
        }
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_count())
    full_count_check := expect_reply_shape(effect, syscall.SyscallStatus.Ok, expected_completion_count, FAIL_COMPLETION_FILL_COUNT_STATUS)
    if full_count_check != 0 {
        if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
            return FAIL_COMPLETION_FILL_COUNT_STATUS
        }
        return FAIL_COMPLETION_FILL_COUNT_LEN
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_schedule(15, 1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COMPLETION_STALL_SCHEDULE
    }
    workflow_submission_count = workflow_submission_count + 1

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(15))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_RUNNING, workflow_service.WORKFLOW_RESTART_NONE, expected_task_id(workflow_submission_count)) {
        return FAIL_COMPLETION_STALL_RUNNING
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(15))
    if !expect_workflow(effect, syscall.SyscallStatus.Exhausted, workflow_service.WORKFLOW_STATE_DELIVERING, workflow_service.WORKFLOW_RESTART_NONE, workflow_service.WORKFLOW_STATE_DONE) {
        return FAIL_COMPLETION_STALL_FIRST_STATUS
    }

    stall_first_check := expect_delivery_payload(effect, workflow_service.WORKFLOW_STATE_DELIVERING, workflow_service.WORKFLOW_STATE_DONE, FAIL_COMPLETION_STALL_FIRST_STATE, FAIL_COMPLETION_STALL_FIRST_OUTCOME)
    if stall_first_check != 0 {
        return stall_first_check
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(15))
    if !expect_workflow(effect, syscall.SyscallStatus.WouldBlock, workflow_service.WORKFLOW_STATE_DELIVERING, workflow_service.WORKFLOW_RESTART_NONE, workflow_service.WORKFLOW_STATE_DONE) {
        return FAIL_COMPLETION_STALL_SECOND_STATUS
    }

    stall_second_check := expect_delivery_payload(effect, workflow_service.WORKFLOW_STATE_DELIVERING, workflow_service.WORKFLOW_STATE_DONE, FAIL_COMPLETION_STALL_SECOND_STATE, FAIL_COMPLETION_STALL_SECOND_STATE)
    if stall_second_check != 0 {
        return FAIL_COMPLETION_STALL_SECOND_STATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_ack())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_COMPLETION_RELEASE_ACK
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_workflow_query(15))
    if !expect_workflow(effect, syscall.SyscallStatus.Ok, workflow_service.WORKFLOW_STATE_DONE, workflow_service.WORKFLOW_RESTART_NONE, 0) {
        return FAIL_COMPLETION_RECOVER_STATUS
    }

    payload = service_effect.effect_reply_payload(effect)
    if payload[0] != workflow_service.WORKFLOW_STATE_DONE {
        return FAIL_COMPLETION_RECOVER_STATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_completion_count())
    final_count_check := expect_reply_shape(effect, syscall.SyscallStatus.Ok, expected_completion_count, FAIL_COMPLETION_FINAL_COUNT_STATUS)
    if final_count_check != 0 {
        if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
            return FAIL_COMPLETION_FINAL_COUNT_STATUS
        }
        return FAIL_COMPLETION_FINAL_COUNT_LEN
    }

    return 0
}