// Service identity and addressing for the reset-lane kernel.
//
// In the current fixed-wiring model a service is identified by one endpoint_id
// that boot wiring assigns at init time and never changes.  A ServiceRef names
// that fixed address.
//
// Stability rules (Phase 154):
//   - ServiceRef.endpoint_id is stable across service restart.
//   - Service state is NOT stable by default.  It resets to the post-init
//     value unless init explicitly reloads retained state.
//   - Clients hold a ServiceRef obtained from boot wiring; no discovery.
//   - Holding a ServiceRef does not imply the service is alive.
//
// Split contract (Phase 182):
//   - Routing continuity is stable: endpoint_id survives restart.
//   - Owner pid continuity is stable: restart reuses the wired service pid.
//   - Service-instance continuity is NOT stable: restart creates a fresh
//     instance generation even when routing and pid stay the same.
//
// What survives restart:
//   - The endpoint_id in ServiceRef remains valid.
//   - Any retained state that init explicitly saves and reloads survives.
//   - The current explicit reload path preserves log and kv retained state.
//   - Callers that retained their ServiceRef before restart may resume
//     sending without reacquiring the ref.
//
// What does not survive restart:
//   - Retained state with no explicit reload path.
//   - Pending effects or in-flight message chains.
//
// Discovery and dynamic binding are explicitly ruled out in Phase 154.
// All refs are wired by boot.kernel_init() and exposed from there.

struct ServiceRef {
    endpoint_id: u32
}

struct ServiceMark {
    endpoint_id: u32
    pid: u32
    generation: u32
    generation_payload: [4]u8
}

func service_ref(endpoint_id: u32) ServiceRef {
    return ServiceRef{ endpoint_id: endpoint_id }
}

func service_mark(endpoint_id: u32, pid: u32, generation: u32, generation_payload: [4]u8) ServiceMark {
    return ServiceMark{ endpoint_id: endpoint_id, pid: pid, generation: generation, generation_payload: generation_payload }
}

func ref_endpoint(ref: ServiceRef) u32 {
    return ref.endpoint_id
}

func mark_endpoint(mark: ServiceMark) u32 {
    return mark.endpoint_id
}

func mark_pid(mark: ServiceMark) u32 {
    return mark.pid
}

func mark_generation(mark: ServiceMark) u32 {
    return mark.generation
}

func mark_generation_payload(mark: ServiceMark) [4]u8 {
    return mark.generation_payload
}

func generation_initial_payload() [4]u8 {
    payload: [4]u8
    payload[0] = 0
    payload[1] = 0
    payload[2] = 0
    payload[3] = 1
    return payload
}

func generation_zero_payload() [4]u8 {
    payload: [4]u8
    payload[0] = 0
    payload[1] = 0
    payload[2] = 0
    payload[3] = 0
    return payload
}

func generation_next_payload(current: [4]u8) [4]u8 {
    next: [4]u8 = current
    if next[3] != 255 {
        next[3] = next[3] + 1
        return next
    }
    next[3] = 0
    if next[2] != 255 {
        next[2] = next[2] + 1
        return next
    }
    next[2] = 0
    if next[1] != 255 {
        next[1] = next[1] + 1
        return next
    }
    next[1] = 0
    next[0] = next[0] + 1
    return next
}

func refs_equal(a: ServiceRef, b: ServiceRef) bool {
    if a.endpoint_id == b.endpoint_id {
        return true
    }
    return false
}

func ref_matches_mark(ref: ServiceRef, mark: ServiceMark) bool {
    if ref.endpoint_id == mark.endpoint_id {
        return true
    }
    return false
}

func marks_same_endpoint(a: ServiceMark, b: ServiceMark) bool {
    if a.endpoint_id == b.endpoint_id {
        return true
    }
    return false
}

func marks_same_pid(a: ServiceMark, b: ServiceMark) bool {
    if a.pid == b.pid {
        return true
    }
    return false
}

func marks_same_instance(a: ServiceMark, b: ServiceMark) bool {
    if !marks_same_endpoint(a, b) {
        return false
    }
    if !marks_same_pid(a, b) {
        return false
    }
    if a.generation == b.generation {
        return true
    }
    return false
}

// ref_is_valid returns true when the endpoint_id matches a non-zero wiring slot.
// An endpoint_id of 0 is treated as the null/unset slot and is never valid.
func ref_is_valid(ref: ServiceRef) bool {
    if ref.endpoint_id == 0 {
        return false
    }
    return true
}
