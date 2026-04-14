#!/usr/bin/env python3
"""
Phase C transformation: consolidate 44 per-phase kernel test .cpp files
into 9 shard .cpp files, one per shard.

Shard assignments (from suite.cpp kKernelTestCases):
  shard 1: phase85, phase86, phase87, phase88, phase105, phase106
  shard 2: phase107, phase108, phase109, phase110, phase111
  shard 3: phase112, phase113, phase114, phase115, phase116
  shard 4: phase117, phase118, phase119, phase120, phase121
  shard 5: phase122, phase123, phase124, phase125, phase126
  shard 6: phase128, phase129, phase130, phase131, phase132
  shard 7: phase133, phase134, phase135, phase136, phase137
  shard 8: phase140, phase141, phase142
  shard 9: phase143, phase144, phase145, phase146, phase147
"""

import os
import re
import sys

KERNEL_TEST_DIR = "/Users/ro/dev/c_modern/tests/tool/freestanding/kernel"

SHARDS = {
    1: ["phase85_endpoint_queue",
        "phase86_task_lifecycle",
        "phase87_static_data",
        "phase88_build_integration",
        "phase105_real_log_service_handshake",
        "phase106_real_echo_service_request_reply"],
    2: ["phase107_real_user_to_user_capability_transfer",
        "phase108_kernel_image_program_cap_audit",
        "phase109_first_running_kernel_slice_audit",
        "phase110_kernel_ownership_split_audit",
        "phase111_scheduler_lifecycle_ownership_clarification"],
    3: ["phase112_syscall_boundary_thinness_audit",
        "phase113_interrupt_entry_and_generic_dispatch_boundary",
        "phase114_address_space_and_mmu_ownership_split",
        "phase115_timer_ownership_hardening",
        "phase116_mmu_activation_barrier_follow_through"],
    4: ["phase117_init_orchestrated_multi_service_bring_up",
        "phase118_request_reply_and_delegation_follow_through",
        "phase119_namespace_pressure_audit",
        "phase120_running_system_support_statement",
        "phase121_kernel_image_contract_hardening"],
    5: ["phase122_target_surface_audit",
        "phase123_next_plateau_audit",
        "phase124_delegation_chain_stress",
        "phase125_invalidation_and_rejection_audit",
        "phase126_authority_lifetime_classification"],
    6: ["phase128_service_death_observation",
        "phase129_partial_failure_propagation",
        "phase130_explicit_restart_or_replacement",
        "phase131_fan_in_or_fan_out_composition",
        "phase132_backpressure_and_blocking"],
    7: ["phase133_message_lifetime_and_reuse",
        "phase134_minimal_device_service_handoff",
        "phase135_buffer_ownership_boundary_audit",
        "phase136_device_failure_containment_probe",
        "phase137_optional_dma_or_equivalent_follow_through"],
    8: ["phase140_serial_ingress_composed_service_graph",
        "phase141_interactive_service_system_scope_freeze",
        "phase142_serial_shell_command_routing"],
    9: ["phase143_long_lived_log_service_follow_through",
        "phase144_stateful_key_value_service_follow_through",
        "phase145_service_restart_failure_and_usage_pressure_audit",
        "phase146_service_shape_consolidation",
        "phase147_ipc_shape_audit_under_real_usage"],
}


def parse_phase_file(path):
    """
    Parse a phase .cpp file and return a dict with:
      - sys_includes: ordered list of #include <...> lines
      - local_includes: ordered list of #include "..." lines
      - using_decls: ordered list of using declarations
      - anon_namespace_body: string content inside the anonymous namespace (if any)
      - public_functions: string of the public function definitions
    """
    with open(path) as f:
        content = f.read()

    # Extract system includes
    sys_includes = re.findall(r'^#include <[^>]+>', content, re.MULTILINE)
    # Extract local includes
    local_includes = re.findall(r'^#include "[^"]+"', content, re.MULTILINE)

    # Find the namespace mc::tool_tests block
    # It starts after includes and ends at the final }
    ns_match = re.search(
        r'^namespace mc::tool_tests \{(.+)\}  // namespace mc::tool_tests\s*$',
        content,
        re.MULTILINE | re.DOTALL
    )
    if not ns_match:
        # Try without trailing comment
        ns_match = re.search(
            r'^namespace mc::tool_tests \{(.+)\}  // namespace mc::tool_tests',
            content,
            re.MULTILINE | re.DOTALL
        )
    if not ns_match:
        raise ValueError(f"Could not find namespace mc::tool_tests in {path}")

    ns_body = ns_match.group(1)

    # Extract using declarations (at the beginning of the namespace body)
    using_decls = re.findall(r'using mc::test_support::\w+;', ns_body)

    # Check for anonymous namespace
    anon_ns_match = re.search(
        r'^namespace \{\s*\n(.*?)\n\}  // namespace\s*$',
        ns_body,
        re.MULTILINE | re.DOTALL
    )
    anon_namespace_body = ""
    if anon_ns_match:
        anon_namespace_body = anon_ns_match.group(1)

    # Extract public function definitions (everything after the anonymous namespace or using decls)
    # Find where the public function starts
    # Public functions start with "void RunFreestandingKernel..."
    public_fn_match = re.search(
        r'^void RunFreestandingKernel.+',
        ns_body,
        re.MULTILINE | re.DOTALL
    )
    public_functions = ""
    if public_fn_match:
        public_functions = public_fn_match.group(0).rstrip()
        # Strip any trailing closing brace that belongs to the namespace
        # (the last "}" should be the function body close)
    else:
        raise ValueError(f"Could not find public function in {path}")

    return {
        "sys_includes": sys_includes,
        "local_includes": local_includes,
        "using_decls": using_decls,
        "anon_namespace_body": anon_namespace_body,
        "public_functions": public_functions,
    }


def merge_shard(shard_num, stems):
    """Build a merged shard file content from a list of phase stems."""
    parsed = []
    for stem in stems:
        path = os.path.join(KERNEL_TEST_DIR, stem + ".cpp")
        if not os.path.exists(path):
            print(f"  WARNING: {path} not found, skipping")
            continue
        parsed.append(parse_phase_file(path))
        print(f"  Parsed {stem}.cpp")

    if not parsed:
        return None

    # Deduplicate includes preserving order
    all_sys = []
    seen_sys = set()
    for p in parsed:
        for inc in p["sys_includes"]:
            if inc not in seen_sys:
                all_sys.append(inc)
                seen_sys.add(inc)

    all_local = []
    seen_local = set()
    for p in parsed:
        for inc in p["local_includes"]:
            if inc not in seen_local:
                all_local.append(inc)
                seen_local.add(inc)

    # Deduplicate using declarations preserving order
    all_using = []
    seen_using = set()
    for p in parsed:
        for decl in p["using_decls"]:
            if decl not in seen_using:
                all_using.append(decl)
                seen_using.add(decl)

    # Collect anonymous namespace bodies
    anon_bodies = [p["anon_namespace_body"] for p in parsed if p["anon_namespace_body"].strip()]

    # Collect public functions
    public_fns = [p["public_functions"] for p in parsed]

    # Build the output
    lines = []

    # Includes
    for inc in all_sys:
        lines.append(inc)
    lines.append("")
    for inc in all_local:
        lines.append(inc)
    lines.append("")
    lines.append("namespace mc::tool_tests {")
    lines.append("")

    # using declarations
    for decl in all_using:
        lines.append(decl)
    lines.append("")

    # Anonymous namespace (if any)
    if anon_bodies:
        lines.append("namespace {")
        lines.append("")
        for body in anon_bodies:
            lines.append(body)
            lines.append("")
        lines.append("}  // namespace")
        lines.append("")

    # Public functions
    for fn in public_fns:
        lines.append(fn)
        lines.append("")

    lines.append("}  // namespace mc::tool_tests")

    return "\n".join(lines) + "\n"


def main():
    print("Phase C consolidation: kernel test shards")
    print()

    for shard_num, stems in sorted(SHARDS.items()):
        print(f"Shard {shard_num}: {', '.join(stems)}")
        content = merge_shard(shard_num, stems)
        if content is None:
            print(f"  SKIP: no files found")
            continue
        out_path = os.path.join(KERNEL_TEST_DIR, f"shard{shard_num}.cpp")
        with open(out_path, "w") as f:
            f.write(content)
        print(f"  Written: shard{shard_num}.cpp")
        print()

    print("Done.")


if __name__ == "__main__":
    main()
