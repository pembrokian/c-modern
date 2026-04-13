# Smoke Tests

Purpose

`tests/smoke/` is for tiny pass-fail checks that answer one question quickly:
does this basically work?

Rules

- Keep tests tiny.
- Prefer direct behavior over internal representations.
- Do not add MIR, sema, audit, or projection goldens here.
- Prefer one small flow per test: build, hello, IPC round trip, serial basic,
  shell boot.
- If a check starts turning into orchestration or replay logic, it belongs in
  `tests/system/` or it is still carrying legacy proof shape.

Phase 152c note

- New kernel cleanup work should prefer landing here first, even if the first
  executable runner temporarily lives under `tests/tool/` while the new
  behavior-first test tree is being established.
