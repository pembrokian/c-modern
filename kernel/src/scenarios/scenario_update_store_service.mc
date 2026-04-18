import boot
import kernel_dispatch
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall
import update_store_service

const FAIL_UPDATE_STORE_SETUP: i32 = 2420
const FAIL_UPDATE_STORE_STAGE0: i32 = 2421
const FAIL_UPDATE_STORE_STAGE1: i32 = 2422
const FAIL_UPDATE_STORE_STAGE2: i32 = 2423
const FAIL_UPDATE_STORE_MANIFEST: i32 = 2424
const FAIL_UPDATE_STORE_QUERY0: i32 = 2425
const FAIL_UPDATE_STORE_RESTART: i32 = 2426
const FAIL_UPDATE_STORE_STATE_LEN: i32 = 2427
const FAIL_UPDATE_STORE_STATE_DATA: i32 = 2428
const FAIL_UPDATE_STORE_STATE_CLASS: i32 = 2429
const FAIL_UPDATE_STORE_QUERY1: i32 = 2430

func expect_update_query(effect: service_effect.Effect, class: u8, version: u8, len: u8, expected_len: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    if payload[0] != class {
        return false
    }
    if payload[1] != version {
        return false
    }
    if payload[2] != len {
        return false
    }
    return payload[3] == expected_len
}

func run_update_store_service_probe() i32 {
    if !update_store_service.update_store_persist(update_store_service.update_store_init(service_topology.UPDATE_STORE_SLOT.pid, 1)) {
        return FAIL_UPDATE_STORE_SETUP
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_STORE_STAGE0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_STORE_STAGE1
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_stage(33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_STORE_STAGE2
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_manifest(7, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_UPDATE_STORE_MANIFEST
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_query())
    if !expect_update_query(effect, update_store_service.UPDATE_CLASS_READY, 7, 3, 3) {
        return FAIL_UPDATE_STORE_QUERY0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_UPDATE_STORE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_UPDATE_STORE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_UPDATE_STORE_RESTART
    }

    if update_store_service.update_artifact_len(state.update_store.state) != 3 {
        return FAIL_UPDATE_STORE_STATE_LEN
    }
    if state.update_store.state.data[0] != 11 {
        return FAIL_UPDATE_STORE_STATE_DATA
    }
    if state.update_store.state.data[1] != 22 {
        return FAIL_UPDATE_STORE_STATE_DATA
    }
    if state.update_store.state.data[2] != 33 {
        return FAIL_UPDATE_STORE_STATE_DATA
    }
    if update_store_service.update_manifest_classification(state.update_store.state) != update_store_service.UPDATE_CLASS_READY {
        return FAIL_UPDATE_STORE_STATE_CLASS
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_update_query())
    if !expect_update_query(effect, update_store_service.UPDATE_CLASS_READY, 7, 3, 3) {
        return FAIL_UPDATE_STORE_QUERY1
    }

    return 0
}