// Identity taxonomy for restartable services and coordinated retained lanes.
//
// The current running-system slice distinguishes five identity classes:
//   stable public identity    -> boot-wired ServiceRef endpoint_id
//   restart generation        -> per-service ServiceMark.generation
//   coordinated lane identity -> boot-owned workset or audit generation
//   continuity outcome        -> restart outcome classification in boot state
//   derived inspection        -> compact retained-summary projection
//
// This module keeps the shell-visible target classification in one place so
// lifecycle routing does not repeat workset-versus-audit checks locally.

import serial_protocol

func identity_target_is_lane(target: u8) bool {
    return target == serial_protocol.TARGET_WORKSET || target == serial_protocol.TARGET_AUDIT
}

func identity_target_has_generation_query(target: u8) bool {
    if target == serial_protocol.TARGET_AUDIT {
        return false
    }
    return true
}

func identity_target_has_direct_generation(target: u8) bool {
    if target == serial_protocol.TARGET_WORKSET {
        return true
    }
    if identity_target_is_lane(target) {
        return false
    }
    return true
}