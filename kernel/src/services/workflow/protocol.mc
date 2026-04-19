import completion_mailbox_service
import connection_service
import journal_service
import lease_service
import object_store_service
import service_effect
import syscall
import task_service
import ticket_service
import timer_service
import update_store_service
import workflow/advance
import workflow/core
import workflow/restart

func handle(s: workflow_core.WorkflowServiceState, m: service_effect.Message) workflow_core.WorkflowResult {
    if m.payload_len == 0 {
        return workflow_core.WorkflowResult{ state: s, effect: workflow_core.workflow_reply_invalid() }
    }

    switch m.payload[0] {
    case workflow_core.WORKFLOW_OP_SCHEDULE:
        if m.payload_len != 3 {
            return workflow_core.WorkflowResult{ state: s, effect: workflow_core.workflow_reply_invalid() }
        }
        return workflow_core.workflow_schedule(s, m.payload[1], m.payload[2])
    case workflow_core.WORKFLOW_OP_UPDATE:
        if m.payload_len == 3 {
            return workflow_core.workflow_schedule_object_update(s, m.payload[1], m.payload[2])
        }
        if m.payload_len != 4 {
            return workflow_core.WorkflowResult{ state: s, effect: workflow_core.workflow_reply_invalid() }
        }
        return workflow_core.workflow_schedule_versioned_object_update(s, m.payload[1], m.payload[2], m.payload[3])
    case workflow_core.WORKFLOW_OP_APPLY_UPDATE:
        if m.payload_len != 1 {
            return workflow_core.WorkflowResult{ state: s, effect: workflow_core.workflow_reply_invalid() }
        }
        return workflow_core.workflow_schedule_update_apply(s)
    case workflow_core.WORKFLOW_OP_APPLY_UPDATE_LEASE:
        if m.payload_len != 2 {
            return workflow_core.WorkflowResult{ state: s, effect: workflow_core.workflow_reply_invalid() }
        }
        return workflow_core.workflow_schedule_update_apply_with_lease(s, m.payload[1])
    case workflow_core.WORKFLOW_OP_QUERY:
        if m.payload_len != 2 {
            return workflow_core.WorkflowResult{ state: s, effect: workflow_core.workflow_reply_invalid() }
        }
        return workflow_core.workflow_query(s, m.payload[1])
    case workflow_core.WORKFLOW_OP_CANCEL:
        if m.payload_len != 2 {
            return workflow_core.WorkflowResult{ state: s, effect: workflow_core.workflow_reply_invalid() }
        }
        return workflow_core.workflow_cancel(s, m.payload[1])
    default:
        return workflow_core.WorkflowResult{ state: s, effect: workflow_core.workflow_reply_invalid() }
    }
}

func workflow_step_schedule(workflow: workflow_core.WorkflowServiceState, step: workflow_core.WorkflowStepContext, msg: service_effect.Message) workflow_core.WorkflowStepResult {
    prepared := workflow_core.workflow_prepare_schedule(workflow, step.runtime.workset_generation)
    scheduled := handle(prepared, msg)
    if service_effect.effect_reply_status(scheduled.effect) != syscall.SyscallStatus.Ok {
        return workflow_restart.workflow_step_with_synced_journal(workflow, step, scheduled.effect)
    }

    create := timer_service.timer_create(step.runtime.timer, workflow_core.WORKFLOW_TIMER_ID, workflow_core.workflow_schedule_due_for_message(msg))
    next_step := step with { runtime: step.runtime with { timer: create.state } }
    if service_effect.effect_reply_status(create.effect) != syscall.SyscallStatus.Ok {
        return workflow_restart.workflow_step_with_synced_journal(workflow, next_step, create.effect)
    }

    return workflow_restart.workflow_step_with_synced_journal(scheduled.state, next_step, scheduled.effect)
}

func workflow_step_schedule_connection(workflow: workflow_core.WorkflowServiceState, step: workflow_core.WorkflowStepContext, id: u8, slot: u8, opcode: u8) workflow_core.WorkflowStepResult {
    prepared := workflow_core.workflow_prepare_schedule(workflow, step.runtime.workset_generation)
    scheduled := workflow_core.workflow_schedule_connection(prepared, id, slot, opcode)
    if service_effect.effect_reply_status(scheduled.effect) != syscall.SyscallStatus.Ok {
        return workflow_restart.workflow_step_with_synced_journal(workflow, step, scheduled.effect)
    }

    create := timer_service.timer_create(step.runtime.timer, workflow_core.WORKFLOW_TIMER_ID, 2)
    next_step := step with { runtime: step.runtime with { timer: create.state } }
    if service_effect.effect_reply_status(create.effect) != syscall.SyscallStatus.Ok {
        return workflow_restart.workflow_step_with_synced_journal(workflow, next_step, create.effect)
    }

    return workflow_restart.workflow_step_with_synced_journal(scheduled.state, next_step, scheduled.effect)
}

func workflow_execute_cancel(workflow: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState) workflow_core.WorkflowCancelExecution {
    if workflow_core.workflow_is_waiting(workflow) {
        cancel_timer := timer_service.timer_cancel(timer, workflow_core.WORKFLOW_TIMER_ID)
        return workflow_core.WorkflowCancelExecution{ timer: cancel_timer.state, task: task, effect: cancel_timer.effect }
    }

    cancel_task := task_service.task_cancel(task, workflow_core.workflow_running_task_id(workflow))
    return workflow_core.WorkflowCancelExecution{ timer: timer, task: cancel_task.state, effect: cancel_task.effect }
}

func workflow_step_cancel(workflow: workflow_core.WorkflowServiceState, step: workflow_core.WorkflowStepContext, msg: service_effect.Message) workflow_core.WorkflowStepResult {
    if msg.payload_len != 2 || !workflow_core.workflow_can_cancel(workflow, msg.payload[1]) {
        return workflow_restart.workflow_step_with_synced_journal(workflow, step, workflow_core.workflow_reply_invalid())
    }

    cancelled_execution := workflow_execute_cancel(workflow, step.runtime.timer, step.runtime.task)
    if service_effect.effect_reply_status(cancelled_execution.effect) != syscall.SyscallStatus.Ok {
        next_step := step with { runtime: step.runtime with { timer: cancelled_execution.timer, task: cancelled_execution.task } }
        return workflow_restart.workflow_step_with_synced_journal(workflow, next_step, cancelled_execution.effect)
    }

    cancelled := handle(workflow, msg)
    next_step := step with { runtime: step.runtime with { timer: cancelled_execution.timer, task: cancelled_execution.task } }
    return workflow_restart.workflow_step_with_synced_journal(cancelled.state, next_step, cancelled.effect)
}

func step(s: workflow_core.WorkflowServiceState, step: workflow_core.WorkflowStepContext, msg: service_effect.Message) workflow_core.WorkflowStepResult {
    ticked_step := step with { runtime: step.runtime with { timer: timer_service.timer_tick(step.runtime.timer) } }
    advanced := workflow_advance.workflow_advance(s, ticked_step.runtime)
    next_workflow := advanced.workflow
    next_step := ticked_step with {
        runtime: ticked_step.runtime with {
            timer: advanced.timer,
            task: advanced.task,
            object_store: advanced.object_store,
            update_store: advanced.update_store,
            lease: advanced.lease,
            ticket: advanced.ticket,
            completion: advanced.completion
        }
    }

    if msg.payload_len == 0 {
        return workflow_restart.workflow_step_with_synced_journal(next_workflow, next_step, workflow_core.workflow_reply_invalid())
    }

    if msg.payload[0] == workflow_core.WORKFLOW_OP_SCHEDULE || msg.payload[0] == workflow_core.WORKFLOW_OP_UPDATE || msg.payload[0] == workflow_core.WORKFLOW_OP_APPLY_UPDATE || msg.payload[0] == workflow_core.WORKFLOW_OP_APPLY_UPDATE_LEASE {
        return workflow_step_schedule(next_workflow, next_step, msg)
    }

    if msg.payload[0] == workflow_core.WORKFLOW_OP_CANCEL {
        return workflow_step_cancel(next_workflow, next_step, msg)
    }

    handled := handle(next_workflow, msg)
    return workflow_restart.workflow_step_with_synced_journal(handled.state, next_step, handled.effect)
}