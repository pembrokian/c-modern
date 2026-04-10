#include <filesystem>
#include <string>
#include <vector>

#include "compiler/support/dump_paths.h"
#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;

void RunFreestandingKernelPhase108KernelImageProgramCapAudit(const std::filesystem::path& source_root,
                                                             const std::filesystem::path& binary_root,
                                                             const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path build_dir = binary_root / "kernel_phase108_image_program_cap_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_phase108_image_program_cap_build_output.txt",
                                                                 "freestanding kernel phase108 image-and-program-cap audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase108 freestanding kernel image-and-program-cap audit build should succeed:\n" + build_output);
    }

    const std::string manifest = ReadFile(project_path);
    ExpectOutputContains(manifest,
                         "kind = \"exe\"",
                         "phase108 kernel manifest should keep the kernel target executable-owned and explicit");
    ExpectOutputContains(manifest,
                         "package = \"kernel\"",
                         "phase108 kernel manifest should keep explicit package identity");
    ExpectOutputContains(manifest,
                         "root = \"src/main.mc\"",
                         "phase108 kernel manifest should keep the kernel root module explicit");
    ExpectOutputContains(manifest,
                         "env = \"freestanding\"",
                         "phase108 kernel manifest should keep the freestanding environment explicit");
    ExpectOutputContains(manifest,
                         "target = \"arm64-apple-darwin25.4.0\"",
                         "phase108 kernel manifest should keep explicit target identity visible");
    ExpectOutputContains(manifest,
                         "startup = \"bootstrap_main\"",
                         "phase108 kernel manifest should keep startup ownership explicit");

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const std::filesystem::path runtime_object =
        build_targets.object.parent_path() /
        (build_targets.object.stem().generic_string() + ".freestanding.bootstrap_main.runtime.o");
    if (!std::filesystem::exists(build_targets.object)) {
        Fail("phase108 kernel image audit should emit an entry object artifact for target-owned relink");
    }
    if (!std::filesystem::exists(runtime_object)) {
        Fail("phase108 kernel image audit should emit a freestanding runtime object artifact for target-owned relink");
    }
    if (!std::filesystem::exists(build_targets.executable)) {
        Fail("phase108 kernel image audit should emit the linked kernel executable artifact");
    }
    if (!std::filesystem::exists(build_targets.object.parent_path() / "_Users_ro_dev_c_modern_kernel_src_address_space.mc.o")) {
        Fail("phase108 kernel image audit should emit separate module objects for imported kernel modules");
    }

    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase108_image_program_cap_run_output.txt",
                                                             "freestanding kernel phase108 image-and-program-cap audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 109) {
        Fail("phase108 freestanding kernel image-and-program-cap audit run should exit with the current kernel proof marker:\n" +
             run_output);
    }

    const std::filesystem::path manual_executable = build_dir / "bin" / "phase108_manual_kernel_image";
    std::vector<std::string> manual_link_args{"xcrun", "clang", "-target", std::string(mc::kBootstrapTriple)};
    for (const auto& entry : std::filesystem::directory_iterator(build_targets.object.parent_path())) {
        if (entry.path().extension() == ".o") {
            manual_link_args.push_back(entry.path().generic_string());
        }
    }
    manual_link_args.push_back("-o");
    manual_link_args.push_back(manual_executable.generic_string());
    const auto [manual_link_outcome, manual_link_output] = RunCommandCapture(manual_link_args,
                                                                             build_dir / "phase108_manual_link_output.txt",
                                                                             "phase108 manual kernel image relink");
    if (!manual_link_outcome.exited || manual_link_outcome.exit_code != 0) {
        Fail("phase108 manual kernel image relink should succeed from the emitted image-input artifacts:\n" + manual_link_output);
    }

    const auto [manual_run_outcome, manual_run_output] = RunCommandCapture({manual_executable.generic_string()},
                                                                           build_dir / "phase108_manual_link_run_output.txt",
                                                                           "phase108 manual kernel image run");
    if (!manual_run_outcome.exited || manual_run_outcome.exit_code != 109) {
        Fail("phase108 manual kernel image run should preserve the current kernel proof marker after target-owned relink:\n" +
             manual_run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_phase107_user_to_user_capability_transfer returns=[bool]",
                         "phase108 merged MIR should preserve the landed phase107 transfer proof");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[INIT_PROGRAM_CAPABILITY] type=capability.CapabilitySlot",
                         "phase108 merged MIR should retain the explicit mutable init program-cap global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[LOG_SERVICE_PROGRAM_CAPABILITY] type=capability.CapabilitySlot",
                         "phase108 merged MIR should retain explicit log-service program-cap ownership");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[ECHO_SERVICE_PROGRAM_CAPABILITY] type=capability.CapabilitySlot",
                         "phase108 merged MIR should retain explicit echo-service program-cap ownership");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[TRANSFER_SERVICE_PROGRAM_CAPABILITY] type=capability.CapabilitySlot",
                         "phase108 merged MIR should retain explicit transfer-service program-cap ownership");
    ExpectOutputContains(kernel_mir,
                         "Function name=seed_log_service_program_capability returns=[bool]",
                         "phase108 merged MIR should keep log-service program-cap seeding explicit in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=seed_echo_service_program_capability returns=[bool]",
                         "phase108 merged MIR should keep echo-service program-cap seeding explicit in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=seed_transfer_service_program_capability returns=[bool]",
                         "phase108 merged MIR should keep transfer-service program-cap seeding explicit in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_phase108_kernel_image_and_program_cap_contracts returns=[bool]",
                         "phase108 merged MIR should retain the bounded kernel-image and program-cap audit path");
}

}  // namespace mc::tool_tests