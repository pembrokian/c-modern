// Coordinated retained restart probes for the kv plus log audit lane.

import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_identity
import syscall

const FAIL_AUDIT_QUERY_STATUS: i32 = 182
const FAIL_AUDIT_QUERY_MODE: i32 = 183
const FAIL_AUDIT_KV_SET_0: i32 = 184
const FAIL_AUDIT_KV_SET_1: i32 = 185
const FAIL_AUDIT_LOG_BEFORE_STATUS: i32 = 186
const FAIL_AUDIT_LOG_BEFORE_LEN: i32 = 187
const FAIL_AUDIT_LOG_BEFORE_MARKER_0: i32 = 188
const FAIL_AUDIT_LOG_BEFORE_MARKER_1: i32 = 189
const FAIL_AUDIT_RESTART_STATUS: i32 = 190
const FAIL_AUDIT_KV_IDENTITY_BASE: i32 = 191
const FAIL_AUDIT_LOG_IDENTITY_BASE: i32 = 195
const FAIL_AUDIT_KV_GET_0_STATUS: i32 = 199
const FAIL_AUDIT_KV_GET_0_VALUE: i32 = 200
const FAIL_AUDIT_KV_GET_1_STATUS: i32 = 201
const FAIL_AUDIT_KV_GET_1_VALUE: i32 = 202
const FAIL_AUDIT_LOG_AFTER_STATUS: i32 = 203
const FAIL_AUDIT_LOG_AFTER_LEN: i32 = 204
const FAIL_AUDIT_LOG_AFTER_MARKER_0: i32 = 205
const FAIL_AUDIT_LOG_AFTER_MARKER_1: i32 = 206

func run_retained_audit_coordination_probe(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_query(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_AUDIT_QUERY_STATUS
    }
    if service_effect.effect_reply_payload(effect)[1] != serial_protocol.LIFECYCLE_RELOAD {
        return FAIL_AUDIT_QUERY_MODE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_set(40, 131))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_AUDIT_KV_SET_0
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_set(41, 142))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_AUDIT_KV_SET_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_log_tail())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_AUDIT_LOG_BEFORE_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_AUDIT_LOG_BEFORE_LEN
    }
    if service_effect.effect_reply_payload(effect)[0] != kernel_dispatch.KV_WRITE_LOG_MARKER {
        return FAIL_AUDIT_LOG_BEFORE_MARKER_0
    }
    if service_effect.effect_reply_payload(effect)[1] != kernel_dispatch.KV_WRITE_LOG_MARKER {
        return FAIL_AUDIT_LOG_BEFORE_MARKER_1
    }

    kv_before: service_identity.ServiceMark = boot.boot_kv_mark(*state)
    log_before: service_identity.ServiceMark = boot.boot_log_mark(*state)
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_AUDIT))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_AUDIT, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_AUDIT_RESTART_STATUS
    }

    kv_after: service_identity.ServiceMark = boot.boot_kv_mark(*state)
    log_after: service_identity.ServiceMark = boot.boot_log_mark(*state)
    kv_id: i32 = scenario_assert.expect_restart_identity(kv_before, kv_after, FAIL_AUDIT_KV_IDENTITY_BASE)
    if kv_id != 0 {
        return kv_id
    }
    log_id: i32 = scenario_assert.expect_restart_identity(log_before, log_after, FAIL_AUDIT_LOG_IDENTITY_BASE)
    if log_id != 0 {
        return log_id
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_get(40))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_AUDIT_KV_GET_0_STATUS
    }
    if service_effect.effect_reply_payload(effect)[1] != 131 {
        return FAIL_AUDIT_KV_GET_0_VALUE
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_get(41))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_AUDIT_KV_GET_1_STATUS
    }
    if service_effect.effect_reply_payload(effect)[1] != 142 {
        return FAIL_AUDIT_KV_GET_1_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_log_tail())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_AUDIT_LOG_AFTER_STATUS
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return FAIL_AUDIT_LOG_AFTER_LEN
    }
    if service_effect.effect_reply_payload(effect)[0] != kernel_dispatch.KV_WRITE_LOG_MARKER {
        return FAIL_AUDIT_LOG_AFTER_MARKER_0
    }
    if service_effect.effect_reply_payload(effect)[1] != kernel_dispatch.KV_WRITE_LOG_MARKER {
        return FAIL_AUDIT_LOG_AFTER_MARKER_1
    }

    return 0
}