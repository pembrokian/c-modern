# System Tests

Purpose

`tests/system/` is for direct service and multi-service behavior.

Examples

- serial ingress
- serial observability via flat event logs
- serial forward
- malformed serial input
- basic IPC round trip
- multi-service message flow
- echo, log, and key-value behavior

Rules

- Test real behavior and composition.
- Avoid phase numbers in test names.
- Avoid MIR projections, phase audits, and internal-shape goldens.
- Keep helpers in `tests/support/` and keep system tests focused on the flow
  being checked.

Phase 152c note

- The legacy proof-driven kernel runtime suites remain read-only while new
  cleanup work starts moving here.
