#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Iterable
import re


CORE_COMPILER_TESTS = [
    "mc_parser_fixture_unit",
    "mc_sema_fixture_unit",
    "mc_mir_fixture_unit",
    "mc_mir_unit",
    "mc_codegen_fixture_unit",
    "mc_codegen_llvm_unit",
]

ALL_EXECUTABLE_CODEGEN_TESTS = [
    "mc_codegen_executable_core_unit",
    "mc_codegen_executable_stdlib_unit",
    "mc_codegen_executable_project_unit",
]

SMOKE_AND_AUDIT_TESTS = [
    "mc_help_smoke",
    "mc_check_smoke",
    "mc_check_stdlib_import_smoke",
    "mc_build_smoke",
    "mc_check_import_root_smoke",
    "mc_check_dump_mir_smoke",
    "mc_dump_paths_smoke",
    "mc_first_use_smoke",
    "mc_supported_slice_doc_audit",
    "mc_freestanding_support_audit",
    "mc_public_cut_smoke",
    "mc_release_readiness_audit",
    "mc_v0_2_gate",
    "mc_release_snapshot_prep",
]

DEFAULT_BASE_CANDIDATES = ["origin/main", "main", "origin/master", "master"]
CACHE_VERSION = 1
KERNEL_DOCS_TEST = "mc_tool_freestanding_kernel_docs_unit"
KERNEL_SYNTHETIC_TEST = "mc_tool_freestanding_kernel_synthetic_unit"


ALL_KERNEL_SURFACE_TESTS = [
    KERNEL_SYNTHETIC_TEST,
    KERNEL_DOCS_TEST,
]

ALL_KERNEL_TESTS = list(ALL_KERNEL_SURFACE_TESTS)

ALL_FREESTANDING_TESTS = [
    "mc_tool_freestanding_bootstrap_unit",
    *ALL_KERNEL_TESTS,
    "mc_tool_freestanding_system_unit",
]

ALL_TOOL_TESTS = [
    "mc_tool_workflow_unit",
    "mc_tool_build_state_unit",
    "mc_tool_real_project_unit",
    *ALL_FREESTANDING_TESTS,
]

FULL_LOCAL_UNIT_SET = [
    "mc_bootstrap_unit",
    *CORE_COMPILER_TESTS,
    *ALL_EXECUTABLE_CODEGEN_TESTS,
    *ALL_TOOL_TESTS,
    *SMOKE_AND_AUDIT_TESTS,
]


@dataclass(frozen=True)
class Rule:
    prefixes: tuple[str, ...]
    tests: tuple[str, ...]
    reason: str


RULES = [
    Rule(("tests/compiler/parser/",), ("mc_parser_fixture_unit",), "parser fixture ownership"),
    Rule(("tests/compiler/sema/",), ("mc_sema_fixture_unit",), "semantic fixture ownership"),
    Rule(("tests/compiler/mir/",), ("mc_mir_fixture_unit", "mc_mir_unit"), "MIR fixture ownership"),
    Rule(("tests/compiler/codegen/",), ("mc_codegen_fixture_unit", "mc_codegen_llvm_unit", *ALL_EXECUTABLE_CODEGEN_TESTS), "codegen fixture ownership"),
    Rule(("tests/tool/tool_workflow_tests.cpp", "tests/tool/tool_workflow_suite.cpp"), ("mc_tool_workflow_unit",), "workflow suite ownership"),
    Rule(("tests/tool/tool_build_state_tests.cpp", "tests/tool/tool_build_state_suite.cpp"), ("mc_tool_build_state_unit",), "build-state suite ownership"),
    Rule(("tests/tool/tool_real_project_tests.cpp", "tests/tool/tool_real_project_suite.cpp"), ("mc_tool_real_project_unit",), "real-project suite ownership"),
    Rule(("tests/tool/freestanding/bootstrap/",), ("mc_tool_freestanding_bootstrap_unit",), "freestanding bootstrap ownership"),
    Rule(("tests/tool/freestanding/system/",), ("mc_tool_freestanding_system_unit",), "freestanding system ownership"),
    Rule(("tests/tool/freestanding/kernel/artifacts.cpp",), ("mc_tool_freestanding_kernel_artifacts_unit",), "kernel artifact suite ownership"),
    Rule(("tests/tool/freestanding/kernel/suite.cpp",), tuple(ALL_KERNEL_SURFACE_TESTS), "kernel surface ownership"),
    Rule(("tests/tool/tool_freestanding_tests.cpp",), tuple(ALL_FREESTANDING_TESTS), "freestanding suite driver ownership"),
    Rule(("tests/tool/tool_suite_common.cpp", "tests/tool/tool_suite_common.h"), tuple(ALL_TOOL_TESTS), "shared tool helper ownership"),
    Rule(("compiler/lex/", "compiler/parse/", "compiler/ast/"), tuple(CORE_COMPILER_TESTS), "frontend syntax-layer ownership"),
    Rule(("compiler/resolve/", "compiler/sema/"), ("mc_sema_fixture_unit", "mc_mir_fixture_unit", "mc_mir_unit", "mc_codegen_fixture_unit", "mc_codegen_llvm_unit", *ALL_EXECUTABLE_CODEGEN_TESTS), "semantic-layer ownership"),
    Rule(("compiler/mir/",), ("mc_mir_fixture_unit", "mc_mir_unit", "mc_codegen_fixture_unit", "mc_codegen_llvm_unit", *ALL_EXECUTABLE_CODEGEN_TESTS), "MIR-layer ownership"),
    Rule(("compiler/codegen_llvm/",), ("mc_codegen_llvm_unit", *ALL_EXECUTABLE_CODEGEN_TESTS), "LLVM codegen ownership"),
    Rule(("compiler/driver/", "compiler/support/", "compiler/mci/", "mci/"), tuple(ALL_TOOL_TESTS), "driver and tool-surface ownership"),
    Rule(("runtime/freestanding/",), tuple(ALL_FREESTANDING_TESTS), "freestanding runtime ownership"),
    Rule(("tests/support/",), tuple(FULL_LOCAL_UNIT_SET), "shared test harness ownership"),
    Rule(("CMakeLists.txt",), tuple(FULL_LOCAL_UNIT_SET), "build graph ownership"),
]

KERNEL_DOC_PATHS = {
    "AGENTS.md",
    "docs/agent/prompts/repo_map.md",
    "tests/tool/README.md",
    "tests/tool/freestanding/README.md",
    "kernel/README.md",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Select owning CTest targets for changed Modern C paths.")
    parser.add_argument("--source-root", default=None, help="Repository root. Defaults to the parent of tools/.")
    parser.add_argument("--build-dir", default=None, help="Build directory for ctest/cmake. Defaults to <source-root>/build/debug.")
    parser.add_argument("--base-ref", default="auto", help="Git base ref used when discovering changed paths. Use 'auto' to diff against the current branch merge-base with main/master. Default: auto.")
    parser.add_argument("--changed", action="append", default=[], help="Relative or absolute changed path. Repeatable.")
    parser.add_argument("--build", action="store_true", help="Run cmake --build on the selected build directory before tests.")
    parser.add_argument("--run", action="store_true", help="Run the selected CTest targets.")
    parser.add_argument("--cache", action="store_true", help="Skip selected tests whose owned inputs fingerprint matches the last successful local run.")
    parser.add_argument("--cache-file", default=None, help="Path to the selector cache file. Defaults to <build-dir>/tool/select_tests_cache.json.")
    parser.add_argument("--jobs", type=int, default=4, help="Parallelism for cmake --build. Default: 4.")
    parser.add_argument("--strict", action="store_true", help="Exit non-zero if any non-doc path has no explicit owner mapping.")
    return parser.parse_args()


def run_capture(command: list[str], cwd: Path) -> list[str]:
    completed = subprocess.run(command, cwd=cwd, check=True, capture_output=True, text=True)
    return [line.strip() for line in completed.stdout.splitlines() if line.strip()]


def normalize_changed_path(path_text: str, source_root: Path) -> str:
    path = Path(path_text)
    if path.is_absolute():
        path = path.relative_to(source_root)
    return PurePosixPath(path).as_posix()


def discover_changed_paths(source_root: Path, base_ref: str) -> list[str]:
    tracked = run_capture(["git", "diff", "--name-only", "--relative", base_ref], source_root)
    untracked = run_capture(["git", "ls-files", "--others", "--exclude-standard"], source_root)
    return sorted(set(tracked) | set(untracked))


def run_capture_one(command: list[str], cwd: Path) -> str | None:
    completed = subprocess.run(command, cwd=cwd, check=False, capture_output=True, text=True)
    if completed.returncode != 0:
        return None
    return completed.stdout.strip() or None


def resolve_base_ref(source_root: Path, requested_base_ref: str) -> str:
    if requested_base_ref != "auto":
        return requested_base_ref

    current_branch = run_capture_one(["git", "rev-parse", "--abbrev-ref", "HEAD"], source_root)
    if current_branch in {"main", "master"}:
        return "HEAD"

    for candidate in DEFAULT_BASE_CANDIDATES:
        merge_base = run_capture_one(["git", "merge-base", "HEAD", candidate], source_root)
        if merge_base is not None:
            return merge_base
    return "HEAD"

def classify_kernel_path(path: str) -> tuple[list[str], str] | None:
    if path in KERNEL_DOC_PATHS:
        return ([KERNEL_DOCS_TEST], "kernel documentation ownership")

    if path.startswith("tests/tool/freestanding/kernel/synthetic/"):
        return ([KERNEL_SYNTHETIC_TEST], "kernel synthetic surface ownership")

    if path.startswith("tests/tool/freestanding/kernel/docs/"):
        return ([KERNEL_DOCS_TEST], "kernel docs descriptor ownership")

    if path.startswith("kernel/"):
        return (["mc_tool_freestanding_system_unit", "mc_tool_workflow_unit"], "kernel ownership")

    if path.startswith("stdlib/"):
        return ([*ALL_KERNEL_SURFACE_TESTS,
                 "mc_tool_freestanding_system_unit",
                 "mc_codegen_executable_stdlib_unit",
                 "mc_check_stdlib_import_smoke"],
                "stdlib ownership")

    return None


def classify_path(path: str) -> tuple[list[str], str] | None:
    kernel_classification = classify_kernel_path(path)
    if kernel_classification is not None:
        return kernel_classification

    if path.startswith("tests/cases/"):
        return (["mc_check_smoke", "mc_build_smoke", "mc_check_dump_mir_smoke", "mc_dump_paths_smoke"],
                "shared case-fixture ownership")

    if path.startswith("tests/stdlib/") or path.startswith("examples/"):
        return (["mc_codegen_executable_stdlib_unit", "mc_codegen_executable_project_unit", "mc_tool_real_project_unit"],
                "stdlib example and real-project ownership")

    if path.startswith("tests/cmake/"):
        return (SMOKE_AND_AUDIT_TESTS, "CTest audit helper ownership")

    for rule in RULES:
        if any(path == prefix or path.startswith(prefix) for prefix in rule.prefixes):
            return (list(rule.tests), rule.reason)

    if path.startswith("docs/") or path.endswith(".md") or path.endswith(".txt"):
        return ([], "doc-only path with no executable owner mapping")

    return None


def build_selected(source_root: Path, build_dir: Path, jobs: int) -> None:
    subprocess.run(["cmake", "--build", str(build_dir), f"-j{jobs}"], cwd=source_root, check=True)


def cache_path_for(build_dir: Path, explicit_cache_file: str | None) -> Path:
    if explicit_cache_file:
        return Path(explicit_cache_file).resolve()
    return build_dir / "tool" / "select_tests_cache.json"


def load_cache(cache_path: Path) -> dict[str, object]:
    if not cache_path.exists():
        return {"version": CACHE_VERSION, "tests": {}}
    try:
        payload = json.loads(cache_path.read_text())
    except (OSError, json.JSONDecodeError):
        return {"version": CACHE_VERSION, "tests": {}}
    if payload.get("version") != CACHE_VERSION or not isinstance(payload.get("tests"), dict):
        return {"version": CACHE_VERSION, "tests": {}}
    return payload


def save_cache(cache_path: Path, payload: dict[str, object]) -> None:
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    cache_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")


def iter_owned_files(source_root: Path, relative_paths: Iterable[str]) -> list[Path]:
    owned_files: set[Path] = set()
    for relative_path in relative_paths:
        absolute_path = source_root / relative_path
        if absolute_path.is_file():
            owned_files.add(absolute_path)
            continue
        if not absolute_path.exists():
            continue
        for child in absolute_path.rglob("*"):
            if child.is_file() and ".git" not in child.parts and "build" not in child.parts and ".venv" not in child.parts:
                owned_files.add(child)
    return sorted(owned_files)


def owned_inputs_for_test(test_name: str, source_root: Path) -> list[Path]:
    common_tool_inputs = [
        "tests/tool/tool_freestanding_tests.cpp",
        "tests/tool/tool_suite_common.cpp",
        "tests/tool/tool_suite_common.h",
        "compiler/driver/",
        "compiler/support/",
        "compiler/mci/",
        "mci/",
        "tests/support/",
    ]

    explicit_inputs = {
        "mc_parser_fixture_unit": ["tests/compiler/parser/", "compiler/lex/", "compiler/parse/", "compiler/ast/", "tests/support/"],
        "mc_sema_fixture_unit": ["tests/compiler/sema/", "compiler/lex/", "compiler/parse/", "compiler/ast/", "compiler/resolve/", "compiler/sema/", "tests/support/"],
        "mc_mir_fixture_unit": ["tests/compiler/mir/", "compiler/lex/", "compiler/parse/", "compiler/ast/", "compiler/resolve/", "compiler/sema/", "compiler/mir/", "tests/support/"],
        "mc_mir_unit": ["compiler/mir/", "compiler/resolve/", "compiler/sema/", "compiler/parse/", "compiler/ast/"],
        "mc_codegen_fixture_unit": ["tests/compiler/codegen/", "compiler/codegen_llvm/", "compiler/mir/", "compiler/sema/", "tests/support/"],
        "mc_codegen_llvm_unit": ["compiler/codegen_llvm/", "compiler/mir/", "compiler/sema/", "tests/tool/codegen_llvm_tests.cpp"],
        "mc_codegen_executable_core_unit": ["tests/compiler/codegen/", "compiler/codegen_llvm/", "compiler/mir/", "compiler/sema/", "tests/compiler/codegen/codegen_executable_tests.cpp"],
        "mc_codegen_executable_stdlib_unit": ["tests/compiler/codegen/", "compiler/codegen_llvm/", "compiler/mir/", "compiler/sema/", "stdlib/", "tests/compiler/codegen/codegen_executable_tests.cpp"],
        "mc_codegen_executable_project_unit": ["tests/compiler/codegen/", "compiler/codegen_llvm/", "compiler/mir/", "compiler/sema/", "examples/", "tests/compiler/codegen/codegen_executable_tests.cpp"],
        "mc_tool_workflow_unit": ["tests/tool/tool_workflow_tests.cpp", "tests/tool/tool_workflow_suite.cpp", "tests/tool/tool_suite_common.cpp", "tests/tool/tool_suite_common.h", "compiler/driver/", "compiler/support/", "compiler/mci/", "tests/support/"],
        "mc_tool_build_state_unit": ["tests/tool/tool_build_state_tests.cpp", "tests/tool/tool_build_state_suite.cpp", "tests/tool/tool_suite_common.cpp", "tests/tool/tool_suite_common.h", "compiler/driver/", "compiler/support/", "compiler/mci/", "tests/support/"],
        "mc_tool_real_project_unit": ["tests/tool/tool_real_project_tests.cpp", "tests/tool/tool_real_project_suite.cpp", "tests/tool/tool_suite_common.cpp", "tests/tool/tool_suite_common.h", "compiler/driver/", "compiler/support/", "compiler/mci/", "examples/", "tests/support/"],
        "mc_tool_freestanding_bootstrap_unit": ["tests/tool/freestanding/bootstrap/", *common_tool_inputs, "runtime/freestanding/", "stdlib/"],
        KERNEL_SYNTHETIC_TEST: ["tests/tool/freestanding/kernel/synthetic/", "tests/tool/freestanding/kernel/suite.cpp", *common_tool_inputs, "runtime/freestanding/", "stdlib/"],
        KERNEL_DOCS_TEST: ["AGENTS.md", "docs/agent/prompts/repo_map.md", "tests/tool/README.md", "tests/tool/freestanding/README.md", "kernel/README.md", "tests/tool/freestanding/kernel/suite.cpp", "tests/tool/freestanding/kernel/docs/"],
        "mc_tool_freestanding_system_unit": ["tests/tool/freestanding/system/", *common_tool_inputs, "runtime/freestanding/", "kernel/", "stdlib/"],
        "mc_help_smoke": ["compiler/driver/", "compiler/support/", "README.md"],
        "mc_check_smoke": ["tests/cases/", "compiler/driver/", "compiler/support/", "compiler/parse/", "compiler/sema/", "compiler/mir/"],
        "mc_check_stdlib_import_smoke": ["stdlib/", "compiler/driver/", "compiler/support/", "compiler/sema/", "compiler/mir/"],
        "mc_build_smoke": ["tests/cases/", "compiler/driver/", "compiler/support/", "compiler/mir/", "compiler/codegen_llvm/"],
        "mc_check_import_root_smoke": ["tests/cases/", "compiler/driver/", "compiler/support/", "compiler/sema/"],
        "mc_check_dump_mir_smoke": ["tests/cases/", "compiler/driver/", "compiler/support/", "compiler/mir/"],
        "mc_dump_paths_smoke": ["tests/cases/", "compiler/support/", "compiler/driver/"],
        "mc_first_use_smoke": ["README.md", "examples/", "compiler/driver/", "compiler/support/"],
        "mc_supported_slice_doc_audit": ["tests/cmake/run_supported_slice_doc_audit.cmake", "README.md", "docs/"],
        "mc_freestanding_support_audit": ["tests/cmake/run_freestanding_support_audit.cmake", "README.md", "kernel/README.md", "tests/tool/freestanding/README.md"],
        "mc_public_cut_smoke": ["tests/cmake/run_public_cut_smoke.cmake", "README.md", "tests/cases/"],
        "mc_release_readiness_audit": ["tests/cmake/run_release_readiness_audit.cmake", "docs/", "README.md"],
        "mc_v0_2_gate": ["tests/cmake/run_v0_2_gate.cmake", "docs/", "README.md", "tests/cases/"],
        "mc_release_snapshot_prep": ["tests/cmake/run_release_snapshot_prep.cmake", "docs/", "README.md"],
    }

    return iter_owned_files(source_root, explicit_inputs.get(test_name, []))


def fingerprint_for_test(test_name: str, source_root: Path) -> str:
    hasher = hashlib.sha256()
    for file_path in owned_inputs_for_test(test_name, source_root):
        relative = PurePosixPath(file_path.relative_to(source_root)).as_posix()
        hasher.update(relative.encode("utf-8"))
        hasher.update(b"\0")
        hasher.update(file_path.read_bytes())
        hasher.update(b"\0")
    return hasher.hexdigest()


def partition_cached_tests(selected_tests: list[str], source_root: Path, cache_payload: dict[str, object]) -> tuple[list[str], list[str], dict[str, str]]:
    cache_tests = cache_payload.setdefault("tests", {})
    assert isinstance(cache_tests, dict)

    fingerprint_by_test: dict[str, str] = {}
    runnable: list[str] = []
    skipped: list[str] = []
    for test_name in selected_tests:
        fingerprint = fingerprint_for_test(test_name, source_root)
        fingerprint_by_test[test_name] = fingerprint
        cached_entry = cache_tests.get(test_name)
        if isinstance(cached_entry, dict) and cached_entry.get("fingerprint") == fingerprint and cached_entry.get("status") == "passed":
            skipped.append(test_name)
        else:
            runnable.append(test_name)
    return runnable, skipped, fingerprint_by_test


def update_cache_after_run(cache_payload: dict[str, object], run_tests: list[str], skipped_tests: list[str], fingerprints: dict[str, str], success: bool) -> None:
    cache_tests = cache_payload.setdefault("tests", {})
    assert isinstance(cache_tests, dict)
    if success:
        for test_name in [*run_tests, *skipped_tests]:
            cache_tests[test_name] = {"fingerprint": fingerprints[test_name], "status": "passed"}
        return
    for test_name in run_tests:
        cache_tests[test_name] = {"fingerprint": fingerprints[test_name], "status": "failed"}


def run_selected_tests(source_root: Path, build_dir: Path, tests: list[str]) -> None:
    if not tests:
        print("No CTest targets selected; nothing to run.")
        return
    regex = "^(" + "|".join(re.escape(test) for test in tests) + ")$"
    subprocess.run(["ctest", "--test-dir", str(build_dir), "-R", regex, "--output-on-failure"], cwd=source_root, check=True)


def main() -> int:
    args = parse_args()
    source_root = Path(args.source_root).resolve() if args.source_root else Path(__file__).resolve().parents[1]
    build_dir = Path(args.build_dir).resolve() if args.build_dir else source_root / "build" / "debug"
    resolved_base_ref = resolve_base_ref(source_root, args.base_ref)

    changed_paths = [normalize_changed_path(path_text, source_root) for path_text in args.changed]
    if not changed_paths:
        try:
            changed_paths = discover_changed_paths(source_root, resolved_base_ref)
        except subprocess.CalledProcessError as exc:
            print(exc.stderr or exc.stdout or str(exc), file=sys.stderr)
            return 2

    selected_tests: set[str] = set()
    reasons: dict[str, list[str]] = defaultdict(list)
    unmapped: list[str] = []

    for path in changed_paths:
        classification = classify_path(path)
        if classification is None:
            unmapped.append(path)
            continue
        tests, reason = classification
        if not tests:
            reasons[f"(no tests) {reason}"].append(path)
            continue
        for test in tests:
            selected_tests.add(test)
            reasons[test].append(f"{path} [{reason}]")

    ordered_tests = sorted(selected_tests)
    if not args.changed:
        print(f"Base ref: {resolved_base_ref}")

    print("Changed paths:")
    for path in changed_paths:
        print(f"- {path}")

    print("\nSelected tests:")
    if ordered_tests:
        for test in ordered_tests:
            print(f"- {test}")
            for reason in sorted(reasons[test]):
                print(f"  from {reason}")
    else:
        print("- none")

    if unmapped:
        print("\nUnmapped paths:")
        for path in unmapped:
            print(f"- {path}")

    no_test_reasons = sorted(key for key in reasons if key.startswith("(no tests) "))
    if no_test_reasons:
        print("\nDoc-only or non-executable paths:")
        for key in no_test_reasons:
            for path in sorted(reasons[key]):
                print(f"- {path}")

    if args.strict and unmapped:
        print("\nStrict mode refused unmapped executable paths.", file=sys.stderr)
        return 1

    tests_to_run = ordered_tests
    skipped_tests: list[str] = []
    fingerprints: dict[str, str] = {}
    cache_payload: dict[str, object] | None = None
    cache_file: Path | None = None
    if args.cache:
        cache_file = cache_path_for(build_dir, args.cache_file)
        cache_payload = load_cache(cache_file)
        tests_to_run, skipped_tests, fingerprints = partition_cached_tests(ordered_tests, source_root, cache_payload)
        if skipped_tests:
            print("\nCached tests:")
            for test_name in skipped_tests:
                print(f"- {test_name}")
        if tests_to_run != ordered_tests:
            print("\nTests to run after cache filtering:")
            if tests_to_run:
                for test_name in tests_to_run:
                    print(f"- {test_name}")
            else:
                print("- none")

    if args.build:
        build_selected(source_root, build_dir, args.jobs)
    if args.run:
        try:
            run_selected_tests(source_root, build_dir, tests_to_run)
        except subprocess.CalledProcessError:
            if args.cache and cache_payload is not None and cache_file is not None:
                update_cache_after_run(cache_payload, tests_to_run, skipped_tests, fingerprints, success=False)
                save_cache(cache_file, cache_payload)
            raise
        if args.cache and cache_payload is not None and cache_file is not None:
            update_cache_after_run(cache_payload, tests_to_run, skipped_tests, fingerprints, success=True)
            save_cache(cache_file, cache_payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())