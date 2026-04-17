// Shared assertion helpers for kernel scenarios.

import service_effect
import service_identity
import boot
import syscall

func expect_restart_identity(before: service_identity.ServiceMark, after: service_identity.ServiceMark, base: i32) i32 {
    if !service_identity.marks_same_endpoint(before, after) {
        return base
    }
    if !service_identity.marks_same_pid(before, after) {
        return base + 1
    }
    if service_identity.marks_same_instance(before, after) {
        return base + 2
    }
    if service_identity.mark_generation(after) != service_identity.mark_generation(before) + 1 {
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
    if payload[0] != target {
        return false
    }
    if payload[1] != mode {
        return false
    }
    return true
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
    if payload[0] != expected[0] {
        return false
    }
    if payload[1] != expected[1] {
        return false
    }
    if payload[2] != expected[2] {
        return false
    }
    if payload[3] != expected[3] {
        return false
    }
    return true
}

func expect_policy(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, owner0: u8, owner1: u8, policy: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != target {
        return false
    }
    if payload[1] != owner0 {
        return false
    }
    if payload[2] != owner1 {
        return false
    }
    if payload[3] != policy {
        return false
    }
    return true
}

func expect_authority(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, class: u8, transfer: u8, scope: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != target {
        return false
    }
    if payload[1] != class {
        return false
    }
    if payload[2] != transfer {
        return false
    }
    if payload[3] != scope {
        return false
    }
    return true
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
    if payload[0] != expected[0] {
        return false
    }
    if payload[1] != expected[1] {
        return false
    }
    if payload[2] != expected[2] {
        return false
    }
    if payload[3] != expected[3] {
        return false
    }
    return true
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
    if payload[0] != expected[0] {
        return false
    }
    if payload[1] != expected[1] {
        return false
    }
    if payload[2] != expected[2] {
        return false
    }
    if payload[3] != expected[3] {
        return false
    }
    return true
}