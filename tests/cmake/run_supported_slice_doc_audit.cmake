if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT must be defined")
endif()

set(README_PATH "${SOURCE_ROOT}/README.md")
set(RELEASE_NOTE_PATH "${SOURCE_ROOT}/docs/plan/release_hardening_hosted_slice.txt")

file(READ "${README_PATH}" README_CONTENT)
file(READ "${RELEASE_NOTE_PATH}" RELEASE_NOTE_CONTENT)

function(require_contains label content needle)
  string(FIND "${content}" "${needle}" found_index)
  if(found_index EQUAL -1)
    message(FATAL_ERROR "${label} is missing required text: ${needle}")
  endif()
endfunction()

function(require_not_contains label content needle)
  string(FIND "${content}" "${needle}" found_index)
  if(NOT found_index EQUAL -1)
    message(FATAL_ERROR "${label} still contains stale text: ${needle}")
  endif()
endfunction()

require_contains(
  "README"
  "${README_CONTENT}"
  "the admitted repository-bounded narrow `time` and `path` helper slice")
require_contains(
  "README"
  "${README_CONTENT}"
  "make first-use-smoke")
require_contains(
  "README"
  "${README_CONTENT}"
  "supported workflow guarantee on the admitted richer proof: deterministic same-build-dir selected-target reuse without non-selected-target churn, now exposed directly on the shipped `issue_rollup` project")
require_not_contains(
  "README"
  "${README_CONTENT}"
  "Phase 29 static-library slice with `examples/real/issue_rollup/`, proving one hosted `staticlib` target consumed by two thin executable targets")

require_contains(
  "release_hardening_hosted_slice.txt"
  "${RELEASE_NOTE_CONTENT}"
  "admitted adjacent helper boundary: narrow hosted `time` `Duration` plus")
require_contains(
  "release_hardening_hosted_slice.txt"
  "${RELEASE_NOTE_CONTENT}"
  "wait, signal, and broadcast, plus a narrow `MemoryOrder` and `Atomic<T>`")
require_contains(
  "release_hardening_hosted_slice.txt"
  "${RELEASE_NOTE_CONTENT}"
  "the repository also admits one narrow hosted `time` plus `path` helper slice")
require_not_contains(
  "release_hardening_hosted_slice.txt"
  "${RELEASE_NOTE_CONTENT}"
  "condvar broadcast, and broader portability claims remain")
require_not_contains(
  "release_hardening_hosted_slice.txt"
  "${RELEASE_NOTE_CONTENT}"
  "does not yet claim `hal` or `time` as admitted module")