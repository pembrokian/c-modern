#!/usr/bin/env python3

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re


@dataclass(frozen=True)
class ToolCaseDescriptor:
    name: str
    family: str
    runner: str
    selector: str
    function: str
    descriptor_path: str
    roots: tuple[str, ...]


def parse_tool_case_descriptor(descriptor_path: Path, source_root: Path) -> ToolCaseDescriptor:
    values: dict[str, str] = {}
    arrays: dict[str, list[str]] = {}
    current_array: str | None = None

    for raw_line in descriptor_path.read_text().splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue

        if current_array is not None:
            if line == "]":
                current_array = None
                continue
            match = re.fullmatch(r'"([^\"]+)",?', line)
            if not match:
                raise ValueError(f"invalid array entry in {descriptor_path}: {raw_line}")
            arrays[current_array].append(match.group(1))
            continue

        string_match = re.fullmatch(r'([a-z_]+)\s*=\s*"([^\"]+)"', line)
        if string_match:
            values[string_match.group(1)] = string_match.group(2)
            continue

        array_start_match = re.fullmatch(r'([a-z_]+)\s*=\s*\[', line)
        if array_start_match:
            current_array = array_start_match.group(1)
            arrays[current_array] = []
            continue

        raise ValueError(f"unsupported descriptor line in {descriptor_path}: {raw_line}")

    if current_array is not None:
        raise ValueError(f"unterminated array '{current_array}' in {descriptor_path}")

    required_fields = ("name", "family", "runner", "selector", "function")
    missing = [field for field in required_fields if field not in values]
    if missing:
        raise ValueError(f"missing descriptor fields {missing} in {descriptor_path}")

    if values["name"] != values["selector"]:
        raise ValueError(f"descriptor {descriptor_path} must keep name and selector identical")

    if descriptor_path.parent.name != values["name"]:
        raise ValueError(
            f"descriptor {descriptor_path} must match its directory name '{descriptor_path.parent.name}'"
        )

    return ToolCaseDescriptor(
        name=values["name"],
        family=values["family"],
        runner=values["runner"],
        selector=values["selector"],
        function=values["function"],
        descriptor_path=descriptor_path.relative_to(source_root).as_posix(),
        roots=tuple(arrays.get("roots", [])),
    )


def load_tool_case_descriptors(
    source_root: Path,
    relative_root: str,
    expected_family: str,
    expected_runner: str,
) -> list[ToolCaseDescriptor]:
    descriptor_paths = sorted((source_root / relative_root).glob("*/case.toml"))
    if not descriptor_paths:
        raise ValueError(f"no tool case descriptors found under {relative_root}")

    descriptors: list[ToolCaseDescriptor] = []
    for descriptor_path in descriptor_paths:
        descriptor = parse_tool_case_descriptor(descriptor_path, source_root)
        if descriptor.family != expected_family:
            raise ValueError(
                f"descriptor {descriptor_path} has family '{descriptor.family}', expected '{expected_family}'"
            )
        if descriptor.runner != expected_runner:
            raise ValueError(
                f"descriptor {descriptor_path} has runner '{descriptor.runner}', expected '{expected_runner}'"
            )
        descriptors.append(descriptor)
    return descriptors


def ctest_name_for_descriptor(descriptor: ToolCaseDescriptor) -> str:
    if descriptor.family == "tool-workflow":
        prefix = "mc_tool_workflow_"
    elif descriptor.family == "tool-real-project":
        prefix = "mc_tool_real_project_"
    else:
        raise ValueError(f"unsupported tool case family: {descriptor.family}")
    return prefix + descriptor.selector.replace("-", "_") + "_unit"


def render_case_include(descriptors: list[ToolCaseDescriptor]) -> str:
    return "".join(f'{{"{descriptor.selector}", &{descriptor.function}}},\n' for descriptor in descriptors)


def render_ctest_registration(descriptors: list[ToolCaseDescriptor]) -> str:
    lines: list[str] = []
    for descriptor in descriptors:
        lines.append(
            "add_test(NAME "
            + ctest_name_for_descriptor(descriptor)
            + " COMMAND "
            + descriptor.runner
            + " ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} "
            + descriptor.selector
            + ")\n"
        )
    return "".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Load and render grouped tool case descriptors.")
    parser.add_argument("--source-root", required=True, help="Repository root.")
    parser.add_argument("--relative-root", required=True, help="Descriptor family root, relative to the repository root.")
    parser.add_argument("--expected-family", required=True, help="Expected family field.")
    parser.add_argument("--expected-runner", required=True, help="Expected runner field.")
    parser.add_argument("--emit-case-include", default=None, help="Write generated C++ case-table entries to this file.")
    parser.add_argument("--emit-ctest-registration", default=None, help="Write generated CTest registration commands to this file.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source_root = Path(args.source_root).resolve()
    descriptors = load_tool_case_descriptors(
        source_root,
        args.relative_root,
        args.expected_family,
        args.expected_runner,
    )

    if args.emit_case_include is not None:
        output_path = Path(args.emit_case_include)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(render_case_include(descriptors))

    if args.emit_ctest_registration is not None:
        output_path = Path(args.emit_ctest_registration)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(render_ctest_registration(descriptors))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())