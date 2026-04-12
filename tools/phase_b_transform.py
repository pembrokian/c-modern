#!/usr/bin/env python3
"""
Phase B transformation: replace ExpectExitCodeAtLeast with exit-0 check + ExpectOutputContains.

For each kernel phase test file (105-147), replaces:
    ExpectExitCodeAtLeast(outcome_var,
                          N,
                          output_var,
                          "context string");

with:
    if (!outcome_var.exited || outcome_var.exit_code != 0) {
        Fail("context string\n" + output_var);
    }
    ExpectOutputContains(output_var,
                         "PASS phase_stem_name",
                         "context string");

Also adds `using mc::test_support::ExpectOutputContains;` where missing.
"""

import os
import re
import sys

KERNEL_TEST_DIR = "/Users/ro/dev/c_modern/tests/tool/freestanding/kernel"

# These are the 40 phase test files to transform (filename stem → PASS label)
PHASE_FILES = [
    "phase105_real_log_service_handshake",
    "phase106_real_echo_service_request_reply",
    "phase107_real_user_to_user_capability_transfer",
    "phase108_kernel_image_program_cap_audit",
    "phase109_first_running_kernel_slice_audit",
    "phase110_kernel_ownership_split_audit",
    "phase111_scheduler_lifecycle_ownership_clarification",
    "phase112_syscall_boundary_thinness_audit",
    "phase113_interrupt_entry_and_generic_dispatch_boundary",
    "phase114_address_space_and_mmu_ownership_split",
    "phase115_timer_ownership_hardening",
    "phase116_mmu_activation_barrier_follow_through",
    "phase117_init_orchestrated_multi_service_bring_up",
    "phase118_request_reply_and_delegation_follow_through",
    "phase119_namespace_pressure_audit",
    "phase120_running_system_support_statement",
    "phase121_kernel_image_contract_hardening",
    "phase122_target_surface_audit",
    "phase123_next_plateau_audit",
    "phase124_delegation_chain_stress",
    "phase125_invalidation_and_rejection_audit",
    "phase126_authority_lifetime_classification",
    "phase128_service_death_observation",
    "phase129_partial_failure_propagation",
    "phase130_explicit_restart_or_replacement",
    "phase131_fan_in_or_fan_out_composition",
    "phase132_backpressure_and_blocking",
    "phase133_message_lifetime_and_reuse",
    "phase134_minimal_device_service_handoff",
    "phase135_buffer_ownership_boundary_audit",
    "phase136_device_failure_containment_probe",
    "phase137_optional_dma_or_equivalent_follow_through",
    "phase140_serial_ingress_composed_service_graph",
    "phase141_interactive_service_system_scope_freeze",
    "phase142_serial_shell_command_routing",
    "phase143_long_lived_log_service_follow_through",
    "phase144_stateful_key_value_service_follow_through",
    "phase145_service_restart_failure_and_usage_pressure_audit",
    "phase146_service_shape_consolidation",
    "phase147_ipc_shape_audit_under_real_usage",
]


def transform_expecexitcodeatloast(content, pass_label):
    """Replace all ExpectExitCodeAtLeast calls with exit-0 check + ExpectOutputContains."""
    # Match the full multi-line call:
    #     ExpectExitCodeAtLeast(OUTCOME_VAR,
    #                           N,
    #                           OUTPUT_VAR,
    #                           "CONTEXT STRING");
    # We need to capture:
    # - leading whitespace (indentation)
    # - outcome variable name
    # - output variable name
    # - context string (may span multiple lines? No, it's on one line)
    pattern = re.compile(
        r'( {4,})ExpectExitCodeAtLeast\((\w+),\s*\n'
        r'\s+\d+,\s*\n'
        r'\s+(\w+),\s*\n'
        r'\s+"([^"]+)"\);',
        re.MULTILINE
    )

    def replacement(m):
        indent = m.group(1)
        outcome_var = m.group(2)
        output_var = m.group(3)
        context = m.group(4)
        inner_indent = indent + "    "
        return (
            f'{indent}if (!{outcome_var}.exited || {outcome_var}.exit_code != 0) {{\n'
            f'{inner_indent}Fail("{context}\\n" + {output_var});\n'
            f'{indent}}}\n'
            f'{indent}ExpectOutputContains({output_var},\n'
            f'{indent}                     "PASS {pass_label}",\n'
            f'{indent}                     "{context}");'
        )

    new_content, count = pattern.subn(replacement, content)
    return new_content, count


def ensure_expect_output_contains_using(content):
    """Add `using mc::test_support::ExpectOutputContains;` if not present."""
    if "using mc::test_support::ExpectOutputContains;" in content:
        return content, False

    # Insert after the last `using mc::test_support::` line in the namespace block
    # Find the block of using declarations
    # Look for `using mc::test_support::Fail;` and insert before it (alphabetically
    # ExpectOutputContains comes before Fail)
    insert_pattern = re.compile(
        r'(using mc::test_support::(?:Fail|ReadFile|RunCommandCapture);)'
    )
    # Find the first using declaration from mc::test_support that comes after ExpectOutputContains
    # alphabetically: ExpectOutputContains < Fail < ReadFile < RunCommandCapture
    # Insert before the first one that sorts after it

    match = insert_pattern.search(content)
    if match:
        insert_pos = match.start()
        new_content = (
            content[:insert_pos]
            + "using mc::test_support::ExpectOutputContains;\n"
            + content[insert_pos:]
        )
        return new_content, True

    return content, False


def transform_file(stem):
    filepath = os.path.join(KERNEL_TEST_DIR, stem + ".cpp")
    if not os.path.exists(filepath):
        print(f"  SKIP (not found): {filepath}")
        return

    with open(filepath, "r") as f:
        content = f.read()

    original = content

    # Transform ExpectExitCodeAtLeast calls
    content, count = transform_expecexitcodeatloast(content, stem)
    if count == 0:
        print(f"  WARNING: no ExpectExitCodeAtLeast found in {stem}.cpp")
    else:
        print(f"  Transformed {count} ExpectExitCodeAtLeast call(s) in {stem}.cpp")

    # Ensure using declaration
    content, added = ensure_expect_output_contains_using(content)
    if added:
        print(f"  Added using ExpectOutputContains to {stem}.cpp")

    if content != original:
        with open(filepath, "w") as f:
            f.write(content)
        print(f"  Written: {stem}.cpp")
    else:
        print(f"  No changes: {stem}.cpp")


def main():
    print("Phase B transformation: kernel test files")
    print(f"Processing {len(PHASE_FILES)} files in {KERNEL_TEST_DIR}")
    print()

    for stem in PHASE_FILES:
        transform_file(stem)

    print()
    print("Done.")


if __name__ == "__main__":
    main()
