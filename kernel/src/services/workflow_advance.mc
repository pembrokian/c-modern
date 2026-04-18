import completion_mailbox_service
import connection_service
import lease_service
import object_store_service
import service_effect
import syscall
import task_service
import ticket_service
import timer_service
import update_store_service
import workflow_core

func workflow_execute_connection_ticket(s: workflow_core.WorkflowServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, ticket_generation: u8) workflow_core.WorkflowDelegatedConnectionResult {
    consume := lease_service.lease_consume_external_ticket(lease, workflow_core.workflow_connection_ticket_lease_id(s), ticket_generation)
    next_lease := consume.state
    next_ticket := ticket
    outcome := workflow_core.workflow_connection_ticket_outcome(consume.effect)

    if consume.op == lease_service.LEASE_OP_USE_EXTERNAL_TICKET {
        used := ticket_service.ticket_use(ticket, consume.first, consume.second)
        next_ticket = used.state
        outcome = workflow_core.workflow_connection_ticket_outcome(used.effect)
    }

    delivered := workflow_core.workflow_deliver_completion(s, completion, outcome)
    return workflow_core.workflow_delegated_connection_result(delivered.workflow, next_lease, next_ticket, delivered.completion)
}

func workflow_execute_update_apply_with_lease(s: workflow_core.WorkflowServiceState, lease: lease_service.LeaseServiceState, update_store: update_store_service.UpdateStoreServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, workset_generation: u8) workflow_core.WorkflowDelegatedUpdateApplyResult {
    consume := lease_service.lease_consume_installer_apply(lease, workflow_core.workflow_update_apply_lease_id(s), workset_generation)
    next_lease := consume.state
    next_update_store := update_store
    outcome := workflow_core.workflow_update_apply_lease_outcome(consume.effect)

    if consume.op == lease_service.LEASE_OP_USE_INSTALLER_APPLY {
        apply := update_store_service.update_apply(update_store)
        next_update_store = apply.state
        outcome = workflow_core.WORKFLOW_STATE_UPDATE_REJECTED
        if service_effect.effect_reply_status(apply.effect) == syscall.SyscallStatus.Ok {
            outcome = workflow_core.WORKFLOW_STATE_UPDATE_APPLIED
        }
    }

    delivered := workflow_core.workflow_deliver_completion(s, completion, outcome)
    return workflow_core.workflow_delegated_update_apply_result(delivered.workflow, next_lease, next_update_store, delivered.completion)
}

func workflow_advance_waiting(s: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, connection: connection_service.ConnectionServiceState, workset_generation: u8, ticket_generation: u8) workflow_core.WorkflowAdvanceResult {
    next_task: task_service.TaskServiceState = task
    next_object_store: object_store_service.ObjectStoreServiceState = object_store
    next_update_store: update_store_service.UpdateStoreServiceState = update_store
    next_lease: lease_service.LeaseServiceState = lease
    next_ticket: ticket_service.TicketServiceState = ticket
    next_completion: completion_mailbox_service.CompletionMailboxServiceState = completion

    if workflow_core.workflow_is_connection(s) && !workflow_core.workflow_connection_active(s, connection) {
        connection_cancelled_delivery := workflow_core.workflow_deliver_completion(s with { restart: workflow_core.WORKFLOW_RESTART_NONE }, next_completion, workflow_core.WORKFLOW_STATE_CONNECTION_CANCELLED)
        return workflow_core.workflow_advance_result(connection_cancelled_delivery.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, connection_cancelled_delivery.completion)
    }

    idx := timer_service.timer_find(timer, workflow_core.WORKFLOW_TIMER_ID)
    if idx >= timer_service.TIMER_CAPACITY {
        missing_delivery := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.workflow_cancelled_outcome(s))
        return workflow_core.workflow_advance_result(missing_delivery.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, missing_delivery.completion)
    }

    due := timer.due[idx]
    if timer.state[idx] == timer_service.TIMER_STATE_ACTIVE {
        if workflow_core.workflow_is_connection(s) {
            return workflow_core.workflow_advance_result(workflow_core.workflow_waiting_connection(s, workflow_core.workflow_connection_slot(s), s.restart, s.generation), timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, next_completion)
        }
        if workflow_core.workflow_is_object_update(s) {
            return workflow_core.workflow_advance_result(workflow_core.workflow_waiting_object_update(s, s.restart, s.generation), timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, next_completion)
        }
        if workflow_core.workflow_is_update_apply(s) {
            return workflow_core.workflow_advance_result(workflow_core.workflow_waiting_update_apply(s, s.restart, s.generation), timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, next_completion)
        }
        return workflow_core.workflow_advance_result(workflow_core.workflow_waiting_task(s, due, s.restart, s.generation), timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, next_completion)
    }
    if timer.state[idx] == timer_service.TIMER_STATE_CANCELLED {
        timer_cancelled_delivery := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.workflow_cancelled_outcome(s))
        return workflow_core.workflow_advance_result(timer_cancelled_delivery.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, timer_cancelled_delivery.completion)
    }
    if timer.state[idx] != timer_service.TIMER_STATE_EXPIRED {
        if workflow_core.workflow_is_object_update(s) {
            invalid_object := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_OBJECT_CANCELLED)
            return workflow_core.workflow_advance_result(invalid_object.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, invalid_object.completion)
        }
        if workflow_core.workflow_is_update_apply(s) {
            invalid_update := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_UPDATE_CANCELLED)
            return workflow_core.workflow_advance_result(invalid_update.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, invalid_update.completion)
        }
        invalid_timer := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_FAILED)
        return workflow_core.workflow_advance_result(invalid_timer.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, invalid_timer.completion)
    }

    if workflow_core.workflow_is_object_update(s) {
        update := object_store_service.object_update(next_object_store, workflow_core.workflow_object_name(s), workflow_core.workflow_object_value(s))
        if workflow_core.workflow_object_expected_version(s) != 0 {
            update = object_store_service.object_update_if_version(next_object_store, workflow_core.workflow_object_name(s), workflow_core.workflow_object_value(s), workflow_core.workflow_object_expected_version(s))
        }
        next_object_store = update.state
        update_delivery := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.workflow_object_update_outcome(update.effect))
        return workflow_core.workflow_advance_result(update_delivery.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, update_delivery.completion)
    }

    if workflow_core.workflow_is_update_apply(s) {
        if workflow_core.workflow_update_apply_lease_id(s) != 0 {
            delegated_apply := workflow_execute_update_apply_with_lease(s, next_lease, next_update_store, next_completion, workset_generation)
            return workflow_core.workflow_advance_result(delegated_apply.workflow, timer, next_task, next_object_store, delegated_apply.update_store, delegated_apply.lease, next_ticket, delegated_apply.completion)
        }
        apply := update_store_service.update_apply(next_update_store)
        next_update_store = apply.state
        outcome := workflow_core.WORKFLOW_STATE_UPDATE_REJECTED
        if service_effect.effect_reply_status(apply.effect) == syscall.SyscallStatus.Ok {
            outcome = workflow_core.WORKFLOW_STATE_UPDATE_APPLIED
        }
        apply_delivery := workflow_core.workflow_deliver_completion(s, next_completion, outcome)
        return workflow_core.workflow_advance_result(apply_delivery.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, apply_delivery.completion)
    }

    if workflow_core.workflow_is_connection(s) {
        if workflow_core.workflow_connection_uses_ticket_lease(s) {
            delegated := workflow_execute_connection_ticket(s, next_lease, next_ticket, next_completion, ticket_generation)
            return workflow_core.workflow_advance_result(delegated.workflow, timer, next_task, next_object_store, next_update_store, delegated.lease, delegated.ticket, delegated.completion)
        }

        connection_submit := task_service.task_submit(task, s.opcode)
        next_task = connection_submit.state
        if service_effect.effect_reply_status(connection_submit.effect) != syscall.SyscallStatus.Ok {
            connection_submit_failed := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_CONNECTION_CANCELLED)
            return workflow_core.workflow_advance_result(connection_submit_failed.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, connection_submit_failed.completion)
        }

        connection_payload := service_effect.effect_reply_payload(connection_submit.effect)
        if connection_payload[1] == task_service.TASK_STATE_FAILED {
            immediate_cancel := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_CONNECTION_CANCELLED)
            return workflow_core.workflow_advance_result(immediate_cancel.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, immediate_cancel.completion)
        }

        return workflow_core.workflow_advance_result(workflow_core.workflow_running_connection(s, workflow_core.workflow_connection_slot(s), connection_payload[0]), timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, next_completion)
    }

    submit := task_service.task_submit(task, s.opcode)
    next_task = submit.state
    if service_effect.effect_reply_status(submit.effect) != syscall.SyscallStatus.Ok {
        submit_failed := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_FAILED)
        return workflow_core.workflow_advance_result(submit_failed.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, submit_failed.completion)
    }

    payload := service_effect.effect_reply_payload(submit.effect)
    if payload[1] == task_service.TASK_STATE_FAILED {
        immediate_failed := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_FAILED)
        return workflow_core.workflow_advance_result(immediate_failed.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, immediate_failed.completion)
    }

    return workflow_core.workflow_advance_result(workflow_core.workflow_running_task(s, payload[0]), timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, next_completion)
}

func workflow_advance_running(s: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, connection: connection_service.ConnectionServiceState) workflow_core.WorkflowAdvanceResult {
    next_task: task_service.TaskServiceState = task
    next_object_store: object_store_service.ObjectStoreServiceState = object_store
    next_update_store: update_store_service.UpdateStoreServiceState = update_store
    next_lease: lease_service.LeaseServiceState = lease
    next_ticket: ticket_service.TicketServiceState = ticket
    next_completion: completion_mailbox_service.CompletionMailboxServiceState = completion
    running := workflow_core.workflow_running_state(workflow_core.workflow_running_task_id(s))

    if workflow_core.workflow_is_connection(s) && !workflow_core.workflow_connection_active(s, connection) {
        cancelled_execution := workflow_core.workflow_connection_cancel_result(s, timer, task)
        next_task = cancelled_execution.task
        connection_cancelled_delivery := workflow_core.workflow_deliver_completion(s with { restart: workflow_core.WORKFLOW_RESTART_NONE }, next_completion, workflow_core.WORKFLOW_STATE_CONNECTION_CANCELLED)
        return workflow_core.workflow_advance_result(connection_cancelled_delivery.workflow, cancelled_execution.timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, connection_cancelled_delivery.completion)
    }

    complete := task_service.task_complete(task, running.task)
    next_task = complete.state
    if service_effect.effect_reply_status(complete.effect) == syscall.SyscallStatus.Ok {
        if workflow_core.workflow_is_connection(s) {
            connection_done_delivery := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED)
            return workflow_core.workflow_advance_result(connection_done_delivery.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, connection_done_delivery.completion)
        }
        task_done_delivery := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_DONE)
        return workflow_core.workflow_advance_result(task_done_delivery.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, task_done_delivery.completion)
    }

    query := task_service.task_query(next_task, running.task)
    next_task = query.state
    if service_effect.effect_reply_status(query.effect) != syscall.SyscallStatus.Ok {
        query_failed := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_FAILED)
        return workflow_core.workflow_advance_result(query_failed.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, query_failed.completion)
    }

    query_payload := service_effect.effect_reply_payload(query.effect)
    if query_payload[0] == task_service.TASK_STATE_CANCELLED {
        if workflow_core.workflow_is_connection(s) {
            connection_task_cancelled := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_CONNECTION_CANCELLED)
            return workflow_core.workflow_advance_result(connection_task_cancelled.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, connection_task_cancelled.completion)
        }
        task_cancelled_delivery := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_CANCELLED)
        return workflow_core.workflow_advance_result(task_cancelled_delivery.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, task_cancelled_delivery.completion)
    }
    if query_payload[0] == task_service.TASK_STATE_DONE {
        if workflow_core.workflow_is_connection(s) {
            connection_query_done := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_CONNECTION_EXECUTED)
            return workflow_core.workflow_advance_result(connection_query_done.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, connection_query_done.completion)
        }
        task_query_done := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_DONE)
        return workflow_core.workflow_advance_result(task_query_done.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, task_query_done.completion)
    }
    if query_payload[0] == task_service.TASK_STATE_FAILED {
        if workflow_core.workflow_is_connection(s) {
            connection_task_failed := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_CONNECTION_CANCELLED)
            return workflow_core.workflow_advance_result(connection_task_failed.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, connection_task_failed.completion)
        }
        task_failed_delivery := workflow_core.workflow_deliver_completion(s, next_completion, workflow_core.WORKFLOW_STATE_FAILED)
        return workflow_core.workflow_advance_result(task_failed_delivery.workflow, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, task_failed_delivery.completion)
    }

    return workflow_core.workflow_advance_result(s, timer, next_task, next_object_store, next_update_store, next_lease, next_ticket, next_completion)
}

func workflow_advance_delivering(s: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState) workflow_core.WorkflowAdvanceResult {
    delivery := workflow_core.workflow_delivery_state(workflow_core.workflow_delivery_outcome(s), workflow_core.workflow_delivery_status(s))
    retry_delivery := workflow_core.workflow_deliver_completion(s, completion, delivery.outcome)
    return workflow_core.workflow_advance_result(retry_delivery.workflow, timer, task, object_store, update_store, lease, ticket, retry_delivery.completion)
}

func workflow_advance(s: workflow_core.WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, connection: connection_service.ConnectionServiceState, workset_generation: u8, ticket_generation: u8) workflow_core.WorkflowAdvanceResult {
    if workflow_core.workflow_is_waiting(s) {
        return workflow_advance_waiting(s, timer, task, object_store, update_store, lease, ticket, completion, connection, workset_generation, ticket_generation)
    }

    if workflow_core.workflow_is_running(s) {
        return workflow_advance_running(s, timer, task, object_store, update_store, lease, ticket, completion, connection)
    }

    if workflow_core.workflow_is_delivering(s) {
        return workflow_advance_delivering(s, timer, task, object_store, update_store, lease, ticket, completion)
    }

    return workflow_core.workflow_advance_result(s, timer, task, object_store, update_store, lease, ticket, completion)
}