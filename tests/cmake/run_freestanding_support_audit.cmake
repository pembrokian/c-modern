if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT must be defined")
endif()

set(README_PATH "${SOURCE_ROOT}/README.md")
set(HOSTED_NOTE_PATH "${SOURCE_ROOT}/docs/plan/release_hardening_hosted_slice.txt")
set(FREESTANDING_NOTE_PATH "${SOURCE_ROOT}/docs/plan/freestanding_support_statement.txt")

file(READ "${README_PATH}" README_CONTENT)
file(READ "${HOSTED_NOTE_PATH}" HOSTED_NOTE_CONTENT)
file(READ "${FREESTANDING_NOTE_PATH}" FREESTANDING_NOTE_CONTENT)

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
  "Supported freestanding v0.3 slice:")
require_contains(
  "README"
  "${README_CONTENT}"
  "make freestanding-support-audit")
require_contains(
  "README"
  "${README_CONTENT}"
  "first admitted freestanding `v0.3` slice")
require_contains(
  "README"
  "${README_CONTENT}"
  "admitted manifest boundary: freestanding `exe` targets with explicit `target`, explicit `[targets.<name>.runtime] startup`, and optional explicit `[targets.<name>.link] inputs = [...]`")
require_contains(
  "README"
  "${README_CONTENT}"
  "Phase 93 timer wake proof")
require_contains(
  "README"
  "${README_CONTENT}"
  "Phase 94 first-system demo integration proof")
require_not_contains(
  "README"
  "${README_CONTENT}"
  "unsupported today: non-hosted targets, cross-compilation")

require_contains(
  "release_hardening_hosted_slice.txt"
  "${HOSTED_NOTE_CONTENT}"
  "This note is intentionally separate from the first admitted freestanding")
require_contains(
  "release_hardening_hosted_slice.txt"
  "${HOSTED_NOTE_CONTENT}"
  "For the separate first admitted freestanding `v0.3` slice statement, see")

require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "first admitted freestanding `v0.3` slice statement")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "admitted public stdlib boundary: narrow `hal` `mmio_ptr<T>`,")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "admitted support-statement posture: first admitted freestanding `v0.3`")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "admitted proof set: Phase 81 boot-marker executable proof, Phase 82")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "Phase 85 kernel endpoint queue smoke proof")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "Phase 86 task lifecycle kernel proof")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "Phase 87 kernel static-data and")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "Phase 89 init-to-log-service handshake proof")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "Phase 93 timer wake proof")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "Phase 94 first-system demo integration proof")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "the repository still does not claim full Veya Lite completion")
require_contains(
  "freestanding_support_statement.txt"
  "${FREESTANDING_NOTE_CONTENT}"
  "unsupported today: `mc run` or `mc test` freestanding workflow admission,")