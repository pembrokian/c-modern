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
}

func service_ref(endpoint_id: u32) ServiceRef {
    return ServiceRef{ endpoint_id: endpoint_id }
}

func service_mark(endpoint_id: u32, pid: u32, generation: u32) ServiceMark {
    return ServiceMark{ endpoint_id: endpoint_id, pid: pid, generation: generation }
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
    payload: [4]u8
    payload[0] = u8(mark.generation >> 24)
    payload[1] = u8(mark.generation >> 16)
    payload[2] = u8(mark.generation >> 8)
    payload[3] = u8(mark.generation)
    return payload
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
