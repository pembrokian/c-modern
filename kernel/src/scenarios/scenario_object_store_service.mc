import boot
import kernel_dispatch
import object_store_service
import scenario_assert
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall

const FAIL_OBJECT_STORE_SETUP: i32 = 2340
const FAIL_OBJECT_STORE_CREATE: i32 = 2341
const FAIL_OBJECT_STORE_READ_0: i32 = 2342
const FAIL_OBJECT_STORE_REPLACE: i32 = 2343
const FAIL_OBJECT_STORE_READ_1: i32 = 2344
const FAIL_OBJECT_STORE_RESTART: i32 = 2345
const FAIL_OBJECT_STORE_STATE_COUNT: i32 = 2346
const FAIL_OBJECT_STORE_STATE_VALUE: i32 = 2347
const FAIL_OBJECT_STORE_READ_2: i32 = 2348

func expect_value(effect: service_effect.Effect, value: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return false
    }
    return service_effect.effect_reply_payload(effect)[0] == value
}

func run_object_store_service_probe() i32 {
    if !object_store_service.object_store_persist(object_store_service.object_store_init(service_topology.OBJECT_STORE_SLOT.pid, 1)) {
        return FAIL_OBJECT_STORE_SETUP
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_create(7, 41))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_OBJECT_STORE_CREATE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 41) {
        return FAIL_OBJECT_STORE_READ_0
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_replace(7, 99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return FAIL_OBJECT_STORE_REPLACE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 99) {
        return FAIL_OBJECT_STORE_READ_1
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_lifecycle_restart(serial_protocol.TARGET_OBJECT_STORE))
    if !scenario_assert.expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_OBJECT_STORE, serial_protocol.LIFECYCLE_RELOAD) {
        return FAIL_OBJECT_STORE_RESTART
    }

    if object_store_service.object_count(state.object_store.state) != 1 {
        return FAIL_OBJECT_STORE_STATE_COUNT
    }
    idx := object_store_service.object_find(state.object_store.state, 7)
    if idx >= object_store_service.OBJECT_STORE_CAPACITY {
        return FAIL_OBJECT_STORE_STATE_VALUE
    }
    if object_store_service.object_slot_at(state.object_store.state, idx).value != 99 {
        return FAIL_OBJECT_STORE_STATE_VALUE
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_object_read(7))
    if !expect_value(effect, 99) {
        return FAIL_OBJECT_STORE_READ_2
    }

    return 0
}