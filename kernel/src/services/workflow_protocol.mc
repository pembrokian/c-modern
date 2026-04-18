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
import workflow_advance
import workflow_core
import workflow_restart

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
        if m.payload_len != 3 {
            return workflow_core.WorkflowResult{ state: s, effect: workflow_core.workflow_reply_invalid() }
        }
        return workflow_core.workflow_schedule_object_update(s, m.payload[1], m.payload[2])
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

func workflow_step_schedule(workflow: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, journal: journal_service.JournalServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, msg: service_effect.Message, generation: u8) workflow_core.WorkflowStepResult {
    prepared := workflow_core.workflow_prepare_schedule(workflow, generation)
    scheduled := handle(prepared, msg)
    if service_effect.effect_reply_status(scheduled.effect) != syscall.SyscallStatus.Ok {
        return workflow_restart.workflow_step_with_synced_journal(workflow, timer, task, object_store, update_store, journal, lease, ticket, completion, scheduled.effect)
    }

    create := timer_service.timer_create(timer, workflow_core.WORKFLOW_TIMER_ID, workflow_core.workflow_schedule_due_for_message(msg))
    next_timer := create.state
    if service_effect.effect_reply_status(create.effect) != syscall.SyscallStatus.Ok {
        return workflow_restart.workflow_step_with_synced_journal(workflow, next_timer, task, object_store, update_store, journal, lease, ticket, completion, create.effect)
    }

    return workflow_restart.workflow_step_with_synced_journal(scheduled.state, next_timer, task, object_store, update_store, journal, lease, ticket, completion, scheduled.effect)
}

func workflow_step_schedule_connection(workflow: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, journal: journal_service.JournalServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, id: u8, slot: u8, opcode: u8, generation: u8) workflow_core.WorkflowStepResult {
    prepared := workflow_core.workflow_prepare_schedule(workflow, generation)
    scheduled := workflow_core.workflow_schedule_connection(prepared, id, slot, opcode)
    if service_effect.effect_reply_status(scheduled.effect) != syscall.SyscallStatus.Ok {
        return workflow_restart.workflow_step_with_synced_journal(workflow, timer, task, object_store, update_store, journal, lease, ticket, completion, scheduled.effect)
    }

    create := timer_service.timer_create(timer, workflow_core.WORKFLOW_TIMER_ID, 2)
    next_timer := create.state
    if service_effect.effect_reply_status(create.effect) != syscall.SyscallStatus.Ok {
        return workflow_restart.workflow_step_with_synced_journal(workflow, next_timer, task, object_store, update_store, journal, lease, ticket, completion, create.effect)
    }

    return workflow_restart.workflow_step_with_synced_journal(scheduled.state, next_timer, task, object_store, update_store, journal, lease, ticket, completion, scheduled.effect)
}

func workflow_execute_cancel(workflow: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState) workflow_core.WorkflowCancelExecution {
    if workflow_core.workflow_is_waiting(workflow) {
        cancel_timer := timer_service.timer_cancel(timer, workflow_core.WORKFLOW_TIMER_ID)
        return workflow_core.WorkflowCancelExecution{ timer: cancel_timer.state, task: task, effect: cancel_timer.effect }
    }

    cancel_task := task_service.task_cancel(task, workflow_core.workflow_running_task_id(workflow))
    return workflow_core.WorkflowCancelExecution{ timer: timer, task: cancel_task.state, effect: cancel_task.effect }
}

func workflow_step_cancel(workflow: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, journal: journal_service.JournalServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, msg: service_effect.Message) workflow_core.WorkflowStepResult {
    if msg.payload_len != 2 || !workflow_core.workflow_can_cancel(workflow, msg.payload[1]) {
        return workflow_restart.workflow_step_with_synced_journal(workflow, timer, task, object_store, update_store, journal, lease, ticket, completion, workflow_core.workflow_reply_invalid())
    }

    cancelled_execution := workflow_execute_cancel(workflow, timer, task)
    if service_effect.effect_reply_status(cancelled_execution.effect) != syscall.SyscallStatus.Ok {
        return workflow_restart.workflow_step_with_synced_journal(workflow, cancelled_execution.timer, cancelled_execution.task, object_store, update_store, journal, lease, ticket, completion, cancelled_execution.effect)
    }

    cancelled := handle(workflow, msg)
    return workflow_restart.workflow_step_with_synced_journal(cancelled.state, cancelled_execution.timer, cancelled_execution.task, object_store, update_store, journal, lease, ticket, completion, cancelled.effect)
}

func step(s: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, journal: journal_service.JournalServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, connection: connection_service.ConnectionServiceState, msg: service_effect.Message, generation: u8, ticket_generation: u8) workflow_core.WorkflowStepResult {
    ticked_timer := timer_service.timer_tick(timer)
    advanced := workflow_advance.workflow_advance(s, ticked_timer, task, object_store, update_store, lease, ticket, completion, connection, generation, ticket_generation)
    next_workflow := advanced.workflow
    next_timer := advanced.timer
    next_task := advanced.task
    next_object_store := advanced.object_store
    next_update_store := advanced.update_store
    next_lease := advanced.lease
    next_ticket := advanced.ticket
    next_completion := advanced.completion

    if msg.payload_len == 0 {
        return workflow_restart.workflow_step_with_synced_journal(next_workflow, next_timer, next_task, next_object_store, next_update_store, journal, next_lease, next_ticket, next_completion, workflow_core.workflow_reply_invalid())
    }

    if msg.payload[0] == workflow_core.WORKFLOW_OP_SCHEDULE || msg.payload[0] == workflow_core.WORKFLOW_OP_UPDATE || msg.payload[0] == workflow_core.WORKFLOW_OP_APPLY_UPDATE || msg.payload[0] == workflow_core.WORKFLOW_OP_APPLY_UPDATE_LEASE {
        return workflow_step_schedule(next_workflow, next_timer, next_task, next_object_store, next_update_store, journal, next_lease, next_ticket, next_completion, msg, generation)
    }

    if msg.payload[0] == workflow_core.WORKFLOW_OP_CANCEL {
        return workflow_step_cancel(next_workflow, next_timer, next_task, next_object_store, next_update_store, journal, next_lease, next_ticket, next_completion, msg)
    }

    handled := handle(next_workflow, msg)
    return workflow_restart.workflow_step_with_synced_journal(handled.state, next_timer, next_task, next_object_store, next_update_store, journal, next_lease, next_ticket, next_completion, handled.effect)
}