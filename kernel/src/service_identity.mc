// Service identity and addressing for the reset-lane kernel.
//
// In the current fixed-wiring model a service is identified by one endpoint_id
// that boot wiring assigns at init time and never changes.  A ServiceRef names
// that fixed address.
//
// Stability rules (Phase 154):
//   - ServiceRef.endpoint_id is stable across service restart.
//   - Service state is NOT stable.  It resets to the post-init value.
//   - Clients hold a ServiceRef obtained from boot wiring; no discovery.
//   - Holding a ServiceRef does not imply the service is alive.
//
// What survives restart:
//   - The endpoint_id in ServiceRef remains valid.
//   - Any in-flight state (retained log entries, kv pairs) is lost.
//   - Callers that retained their ServiceRef before restart may resume
//     sending without reacquiring the ref.
//
// What does not survive restart:
//   - Retained state (LogServiceState.retained, KvServiceState.keys/values).
//   - Pending effects or in-flight message chains.
//
// Discovery and dynamic binding are explicitly ruled out in Phase 154.
// All refs are wired by boot.kernel_init() and exposed from there.

struct ServiceRef {
    endpoint_id: u32
}

func service_ref(endpoint_id: u32) ServiceRef {
    return ServiceRef{ endpoint_id: endpoint_id }
}

func ref_endpoint(ref: ServiceRef) u32 {
    return ref.endpoint_id
}

func refs_equal(a: ServiceRef, b: ServiceRef) bool {
    if a.endpoint_id == b.endpoint_id {
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
