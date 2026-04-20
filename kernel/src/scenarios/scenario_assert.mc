// Shared assertion helpers for kernel scenarios.

import service_effect
import service_identity
import boot
import service_state
import syscall

const DISPLAY_STATE_BLANK: [4]u8 = [4]u8{ 0, 0, 0, 0 }
const DISPLAY_STATE_FRESH: [4]u8 = [4]u8{ 70, 82, 83, 72 }
const DISPLAY_STATE_RESUMED: [4]u8 = [4]u8{ 82, 83, 85, 77 }
const DISPLAY_STATE_INVALIDATED: [4]u8 = [4]u8{ 73, 78, 86, 68 }
const DISPLAY_STATE_EMPTY: [4]u8 = [4]u8{ 69, 77, 84, 89 }
const DISPLAY_STATE_STEADY: [4]u8 = [4]u8{ 83, 84, 68, 89 }
const DISPLAY_STATE_BUSY: [4]u8 = [4]u8{ 66, 85, 83, 89 }
const DISPLAY_STATE_ATTN: [4]u8 = [4]u8{ 65, 84, 84, 78 }

func expect_reply(effect: service_effect.Effect, status: syscall.SyscallStatus, len: usize, b0: u8, b1: u8, b2: u8, b3: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != len {
        return false
    }
    payload := service_effect.effect_reply_payload(effect)
    return payload[0] == b0 && payload[1] == b1 && payload[2] == b2 && payload[3] == b3
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

func expect_restart_identity(before: service_identity.ServiceMark, after: service_identity.ServiceMark, base: i32) i32 {
    before_endpoint: u32 = service_identity.mark_endpoint(before)
    before_pid: u32 = service_identity.mark_pid(before)
    before_generation: u32 = service_identity.mark_generation(before)
    after_endpoint: u32 = service_identity.mark_endpoint(after)
    after_pid: u32 = service_identity.mark_pid(after)
    after_generation: u32 = service_identity.mark_generation(after)
    if before_endpoint != after_endpoint {
        return base
    }
    if before_pid != after_pid {
        return base + 1
    }
    if after_generation == before_generation {
        return base + 2
    }
    if after_generation != before_generation + 1 {
        return base + 3
    }
    return 0
}

func expect_lifecycle(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, mode: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = [4]u8{ target, mode, 0, 0 }
    return payload == expected
}

func expect_identity(effect: service_effect.Effect, status: syscall.SyscallStatus, mark: service_identity.ServiceMark) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = service_identity.mark_generation_payload(mark)
    return payload == expected
}

func expect_policy(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, owner0: u8, owner1: u8, policy: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = [4]u8{ target, owner0, owner1, policy }
    return payload == expected
}

func expect_authority(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, class: u8, transfer: u8, scope: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = [4]u8{ target, class, transfer, scope }
    return payload == expected
}

func expect_service_state(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, class: u8, mode: u8, participation: u8, policy: u8, metadata: u8, generation: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = service_state.state_payload(target, class, mode, participation, policy, metadata, generation)
    return payload == expected
}

func expect_durability(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = service_state.state_durability_payload(target)
    return payload == expected
}

func expect_generation_payload(effect: service_effect.Effect, status: syscall.SyscallStatus, generation: u32) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = service_identity.generation_payload(generation)
    return payload == expected
}

func expect_summary(effect: service_effect.Effect, status: syscall.SyscallStatus, participation: boot.RetainedSummaryParticipation, outcome: boot.RestartOutcome, generation: u32) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = boot.summary_payload(participation, outcome, generation)
    return payload == expected
}

func expect_display(effect: service_effect.Effect, status: syscall.SyscallStatus, cells: [4]u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    return payload == cells
}