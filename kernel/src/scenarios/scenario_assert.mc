// Shared assertion helpers for kernel scenarios.

import service_effect
import service_identity
import boot
import service_state
import syscall

func expect_restart_identity(before_endpoint: u32, before_pid: u32, before_generation: u32, after_endpoint: u32, after_pid: u32, after_generation: u32, base: i32) i32 {
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