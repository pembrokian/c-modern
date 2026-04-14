# Legacy Freestanding Archive

This directory preserves the retired freestanding proof harness and related
historical material that no longer participates in the live build, CTest
registration, or changed-path selection contract.

Archived contents

- `kernel_old/`: frozen legacy kernel tree that backed the old freestanding
  proof ladder.
- `tests/tool/freestanding/`: retired freestanding proof suites, descriptors,
  and goldens.
- `tests/tool/tool_freestanding_tests.cpp`: retired freestanding suite driver.
- `tests/cmake/run_freestanding_support_audit.cmake`: retired documentation
  audit for the old freestanding support statement split.
- `tools/phase_b_transform.py` and `tools/phase_c_consolidate.py`: retired
  maintenance scripts that only operated on the old freestanding suite.

Active policy

- Do not add new active tests or new implementation work here.
- Do not wire archived paths back into CMake, `tools/select_tests.py`, or the
  main READMEs.
- New kernel validation belongs on the active hosted reset-lane project under
  `kernel/build.toml` and the grouped workflow suites.