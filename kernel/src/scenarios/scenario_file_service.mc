// Phase 218 / Phase 226: bounded file-service probe and payload growth.
//
// Exercises the full dispatch path: serial -> shell -> file_service.
// Verifies create, append, bounded read, count, missing-file, and capacity-full
// failure contracts. Retained-only: no durable substrate claimed.

import boot
import kernel_dispatch
import scenario_transport
import service_effect
import syscall

const FAIL_FILE_CREATE: i32 = 2180
const FAIL_FILE_CREATE_IDEMPOTENT: i32 = 2181
const FAIL_FILE_WRITE: i32 = 2182
const FAIL_FILE_READ_BEFORE_WRITE: i32 = 2183
const FAIL_FILE_READ_ONE_BYTE: i32 = 2184
const FAIL_FILE_COUNT_AFTER_TWO: i32 = 2185
const FAIL_FILE_MISSING_READ: i32 = 2186
const FAIL_FILE_CAPACITY_FULL: i32 = 2187
const FAIL_FILE_APPEND_SECOND: i32 = 2188
const FAIL_FILE_APPEND_THIRD: i32 = 2189
const FAIL_FILE_APPEND_FOURTH: i32 = 2190
const FAIL_FILE_APPEND_FULL: i32 = 2191
const FAIL_FILE_READ_FULL: i32 = 2192
const FAIL_FILE_DURABILITY: i32 = 2193

func expect_ok(effect: service_effect.Effect) bool {
    return service_effect.effect_reply_status(effect) == syscall.SyscallStatus.Ok
}

func expect_status(effect: service_effect.Effect, status: syscall.SyscallStatus) bool {
    return service_effect.effect_reply_status(effect) == status
}

func expect_count(effect: service_effect.Effect, n: usize) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    return service_effect.effect_reply_payload_len(effect) == n
}

func expect_read_prefix(effect: service_effect.Effect, len: usize, b0: u8, b1: u8, b2: u8, b3: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != len {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    return payload[0] == b0 && payload[1] == b1 && payload[2] == b2 && payload[3] == b3
}

func run_file_service_probe() i32 {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Create file 'a' (name token 97).
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_create(97))
    if !expect_ok(effect) {
        return FAIL_FILE_CREATE
    }

    // Create same file again -- idempotent open.
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_create(97))
    if !expect_ok(effect) {
        return FAIL_FILE_CREATE_IDEMPOTENT
    }

    // Write value 42 to file 'a'.
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_write(97, 42))
    if !expect_ok(effect) {
        return FAIL_FILE_WRITE
    }

    // Read file 'a' before a second write -- expect 1 byte with value 42.
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_read(97))
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return FAIL_FILE_READ_BEFORE_WRITE
    }
    if !expect_read_prefix(effect, 1, 42, 0, 0, 0) {
        return FAIL_FILE_READ_ONE_BYTE
    }

    // Create a second file 'b' (name token 98) and check count == 2.
    kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_create(98))
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_count())
    if !expect_count(effect, 2) {
        return FAIL_FILE_COUNT_AFTER_TWO
    }

    // Read a non-existent file 'z' (name token 122) -- expect InvalidArgument.
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_read(122))
    if !expect_status(effect, syscall.SyscallStatus.InvalidArgument) {
        return FAIL_FILE_MISSING_READ
    }

    // Fill to FILE_CAPACITY (4 files: a, b, c, d).
    kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_create(99))
    kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_create(100))

    // A fifth create must return Exhausted.
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_create(101))
    if !expect_status(effect, syscall.SyscallStatus.Exhausted) {
        return FAIL_FILE_CAPACITY_FULL
    }

    // Append three more bytes to file 'a' up to the fixed four-byte limit.
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_write(97, 99))
    if !expect_ok(effect) {
        return FAIL_FILE_APPEND_SECOND
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_write(97, 100))
    if !expect_ok(effect) {
        return FAIL_FILE_APPEND_THIRD
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_write(97, 101))
    if !expect_ok(effect) {
        return FAIL_FILE_APPEND_FOURTH
    }

    // A fifth byte must return Exhausted.
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_write(97, 102))
    if !expect_status(effect, syscall.SyscallStatus.Exhausted) {
        return FAIL_FILE_APPEND_FULL
    }

    // Read file 'a' returns the retained four-byte prefix.
    effect = kernel_dispatch.kernel_dispatch_step(&state, scenario_transport.cmd_file_read(97))
    if !expect_read_prefix(effect, 4, 42, 99, 100, 101) {
        return FAIL_FILE_READ_FULL
    }

    return 0
}