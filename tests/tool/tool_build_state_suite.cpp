#include <filesystem>
#include <string>

#include "compiler/support/dump_paths.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RequireWriteTime;
using mc::test_support::RunCommandCapture;
using mc::test_support::SleepForTimestampTick;
using mc::test_support::WriteFile;
using namespace mc::tool_tests;
using mc::tool_tests::WriteBasicProject;

void TestProjectBuildAndMciEmission(const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "generated_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "project_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     build_dir / "build_output.txt",
                                                     "project build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase7 project build should succeed:\n" + output);
    }

    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    const auto main_mci = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir).mci;
    const auto helper_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/helper.mc", build_dir).object;
    const auto main_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).object;
    const auto artifacts = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    if (!std::filesystem::exists(helper_mci) || !std::filesystem::exists(main_mci) || !std::filesystem::exists(helper_object) ||
        !std::filesystem::exists(main_object) || !std::filesystem::exists(artifacts.executable)) {
        Fail("phase7 project build should emit module objects, executable, and .mci artifacts");
    }

    const std::string helper_mci_text = ReadFile(helper_mci);
    ExpectOutputContains(helper_mci_text, "package\tapp", "helper .mci should record the package identity");
    ExpectOutputContains(helper_mci_text, "module\thelper", "helper .mci should record the module identity");
    ExpectOutputContains(helper_mci_text, "module_kind\tordinary", "helper .mci should record the module kind");
    ExpectOutputContains(helper_mci_text, "function\tanswer", "helper .mci should record exported functions");
    ExpectOutputContains(helper_mci_text, "interface_hash\t", "helper .mci should record an interface hash");

    const auto [run_outcome, run_output] = RunCommandCapture({artifacts.executable.generic_string()},
                                                             build_dir / "run_output.txt",
                                                             "phase7 project executable run");
    if (!run_outcome.exited || run_outcome.exit_code != 7) {
        Fail("phase7 project executable should return 7, got output:\n" + run_output);
    }
}

void TestProjectTestTargetBuildsAndRuns(const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "test_kind_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-test-kind\"\n"
              "default = \"unit\"\n"
              "\n"
              "[targets.unit]\n"
              "kind = \"test\"\n"
              "package = \"phase13-test-kind\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.unit.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.unit.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc",
              "func main() i32 {\n"
              "    return 6\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "test_kind_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  (project_root / "build.toml").generic_string(),
                                                                  "--build-dir",
                                                                  build_dir.generic_string()},
                                                                 build_dir / "build_output.txt",
                                                                 "test-kind build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase13 test target build should succeed:\n" + build_output);
    }

    const auto executable = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).executable;
    if (!std::filesystem::exists(executable)) {
        Fail("phase13 test target build should emit an executable artifact");
    }

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              (project_root / "build.toml").generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "run_output.txt",
                                                             "test-kind run");
    if (!run_outcome.exited || run_outcome.exit_code != 6) {
        Fail("phase13 test target run should return 6, got:\n" + run_output);
    }
}

void TestProjectImportedGlobalMirDeclarations(const std::filesystem::path& binary_root,
                                              const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "imported_globals_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "const LIMIT: i32 = 9\n"
        "var counter: i32 = 4\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    helper.counter = helper.counter + helper.LIMIT\n"
        "    return helper.counter\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "imported_globals_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string(),
                                                      "--dump-mir"},
                                                     build_dir / "build_output.txt",
                                                     "imported global dump-mir build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase7 imported global dump-mir build should succeed:\n" + output);
    }

    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    const auto main_mir = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir).mir;
    const std::string helper_mci_text = ReadFile(helper_mci);
    const std::string main_mir_text = ReadFile(main_mir);
    ExpectOutputContains(helper_mci_text,
                         "global\t0\tcounter\ti32\t",
                         "helper .mci should record exported mutable globals");
    ExpectOutputContains(main_mir_text,
                         "ConstGlobal names=[helper.LIMIT] type=i32",
                         "merged project MIR should retain imported const globals");
    ExpectOutputContains(main_mir_text,
                         "init 9",
                         "merged project MIR should retain imported const global initializers");
    ExpectOutputContains(main_mir_text,
                         "VarGlobal names=[helper.counter] type=i32",
                         "merged project MIR should retain imported mutable globals");
    ExpectOutputContains(main_mir_text,
                         "init 4",
                         "merged project MIR should retain imported mutable global initializers");
    ExpectOutputContains(main_mir_text,
                         "store_target target=helper.counter target_kind=global target_name=helper.counter",
                         "merged project MIR should lower imported mutable global stores as global targets");
}

void TestProjectImportedAbiTypeMirDeclarations(const std::filesystem::path& binary_root,
                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "imported_abi_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "@abi(c)\n"
        "struct Header {\n"
        "    tag: u8\n"
        "    value: i32\n"
        "}\n"
        "\n"
        "func make_header() Header {\n"
        "    return Header{ tag: 1, value: 9 }\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    header: helper.Header = helper.make_header()\n"
        "    return (i32)(header.tag) + header.value\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "imported_abi_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string(),
                                                      "--dump-mir"},
                                                     build_dir / "build_output.txt",
                                                     "imported abi dump-mir build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase59 imported abi dump-mir build should succeed:\n" + output);
    }

    const auto main_mir = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir).mir;
    const std::string main_mir_text = ReadFile(main_mir);
    ExpectOutputContains(main_mir_text,
                         "TypeDecl kind=struct name=helper.Header",
                         "merged project MIR should retain imported abi-marked type declarations");
    ExpectOutputContains(main_mir_text,
                         "attributes=[abi(c)]",
                         "merged project MIR should retain imported abi(c) attributes");
}

void TestProjectImportedGenericNominalIdentity(const std::filesystem::path& binary_root,
                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "imported_identity_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "struct Box<T> {\n"
        "    value: T\n"
        "}\n"
        "\n"
        "struct Wrapper {\n"
        "    item: Box<i32>\n"
        "}\n"
        "\n"
        "func make() Wrapper {\n"
        "    return Wrapper{ item: Box<i32>{ value: 7 } }\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    wrapped: helper.Wrapper = helper.make()\n"
        "    return wrapped.item.value\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "imported_identity_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string(),
                                                      "--dump-mir"},
                                                     build_dir / "build_output.txt",
                                                     "imported identity dump-mir build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase61 imported identity dump-mir build should succeed:\n" + output);
    }

    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    const auto main_mir = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir).mir;
    const std::string helper_mci_text = ReadFile(helper_mci);
    const std::string main_mir_text = ReadFile(main_mir);
    ExpectOutputContains(helper_mci_text,
                         "type_field\tWrapper\titem\tBox<i32>",
                         "helper .mci should preserve the module-local generic nominal spelling");
    ExpectOutputContains(main_mir_text,
                         "TypeDecl kind=struct name=helper.Box",
                         "merged project MIR should retain imported generic nominal declarations");
    ExpectOutputContains(main_mir_text,
                         "aggregate_init",
                         "merged project MIR should keep imported helper aggregate initialization after namespacing");
    ExpectOutputContains(main_mir_text,
                         "target=helper.Box<i32>",
                         "merged project MIR should rewrite imported aggregate target metadata to the qualified nominal identity");
}

void TestProjectImportedGenericVariantConstructor(const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "generic_variant_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "enum Option<T> {\n"
        "    Some(value: T)\n"
        "    None\n"
        "}\n"
        "\n"
        "func make() Option<i32> {\n"
        "    return Option<i32>.Some(7)\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    value: helper.Option<i32> = helper.make()\n"
        "    switch value {\n"
        "    case helper.Option.Some(v):\n"
        "        return v\n"
        "    default:\n"
        "        return 0\n"
        "    }\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "generic_variant_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string(),
                                                      "--dump-mir"},
                                                     build_dir / "build_output.txt",
                                                     "generic variant dump-mir build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase63 generic variant dump-mir build should succeed:\n" + output);
    }

    const auto main_mir = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir).mir;
    const std::string main_mir_text = ReadFile(main_mir);
    ExpectOutputContains(main_mir_text,
                         "variant_init",
                         "merged project MIR should retain the helper-side generic enum variant constructor");
    ExpectOutputContains(main_mir_text,
                         "TypeDecl kind=enum name=helper.Option",
                         "merged project MIR should retain the imported generic enum declaration");
    ExpectOutputContains(main_mir_text,
                         "variant_extract",
                         "merged project MIR should extract payloads from imported generic enum variants");
    ExpectOutputContains(main_mir_text,
                         "target=helper.Option.Some",
                         "merged project MIR should preserve the imported generic variant target identity");
}

void TestProjectImportedGenericVariantInspection(const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "variant_inspection_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "enum Option<T> {\n"
        "    Some(value: T)\n"
        "    None\n"
        "}\n"
        "\n"
        "func make() Option<i32> {\n"
        "    return Option<i32>.Some(7)\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    value: helper.Option<i32> = helper.make()\n"
        "    if value is helper.Option.Some {\n"
        "        return value.Some.0\n"
        "    }\n"
        "    return 0\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "variant_inspection_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string(),
                                                      "--dump-mir"},
                                                     build_dir / "build_output.txt",
                                                     "generic variant inspection dump-mir build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase64 generic variant inspection dump-mir build should succeed:\n" + output);
    }

    const auto main_mir = mc::support::ComputeDumpTargets(project_root / "src/main.mc", build_dir).mir;
    const std::string main_mir_text = ReadFile(main_mir);
    ExpectOutputContains(main_mir_text,
                         "variant_match",
                         "merged project MIR should lower expression-form variant tests through variant_match");
    ExpectOutputContains(main_mir_text,
                         "variant_extract",
                         "merged project MIR should lower payload projection through variant_extract");
    ExpectOutputContains(main_mir_text,
                         "target=helper.Option.Some",
                         "merged project MIR should preserve imported variant identity for expression-form inspection");
}

void TestCorruptedInterfaceArtifactFailsBuild(const std::filesystem::path& binary_root,
                                              const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "corrupt_mci_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "corrupt_mci_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [initial_outcome, initial_output] = RunCommandCapture({mc_path.generic_string(),
                                                                      "build",
                                                                      "--project",
                                                                      project_path.generic_string(),
                                                                      "--build-dir",
                                                                      build_dir.generic_string()},
                                                                     build_dir / "initial_build_output.txt",
                                                                     "initial build");
    if (!initial_outcome.exited || initial_outcome.exit_code != 0) {
        Fail("phase13 initial build should succeed:\n" + initial_output);
    }

    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    WriteFile(helper_mci,
              "format\t7\n"
              "target\tarm64-apple-darwin25.4.0\n"
              "package\tapp\n"
              "module\thelper\n"
              "module_kind\tordinary\n"
              "source\t" + (project_root / "src/helper.mc").generic_string() + "\n"
              "interface_hash\tdeadbeef\n");

    const auto [corrupt_outcome, corrupt_output] = RunCommandCapture({mc_path.generic_string(),
                                                                      "build",
                                                                      "--project",
                                                                      project_path.generic_string(),
                                                                      "--build-dir",
                                                                      build_dir.generic_string()},
                                                                     build_dir / "corrupt_build_output.txt",
                                                                     "corrupt mci build");
    if (!corrupt_outcome.exited || corrupt_outcome.exit_code == 0) {
        Fail("build with corrupted existing .mci should fail");
    }
    ExpectOutputContains(corrupt_output,
                         "interface artifact hash mismatch",
                         "corrupted .mci should fail with a trust-boundary diagnostic");
}

void TestStaleInterfaceArtifactFormatFailsBuild(const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "stale_mci_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "stale_mci_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [initial_outcome, initial_output] = RunCommandCapture({mc_path.generic_string(),
                                                                      "build",
                                                                      "--project",
                                                                      project_path.generic_string(),
                                                                      "--build-dir",
                                                                      build_dir.generic_string()},
                                                                     build_dir / "initial_build_output.txt",
                                                                     "initial build");
    if (!initial_outcome.exited || initial_outcome.exit_code != 0) {
        Fail("phase54 initial build should succeed:\n" + initial_output);
    }

    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    WriteFile(helper_mci,
              "format\t5\n"
              "target\tarm64-apple-darwin25.4.0\n"
              "module\thelper\n"
              "source\t" + (project_root / "src/helper.mc").generic_string() + "\n"
              "interface_hash\tdeadbeef\n");

    const auto [stale_outcome, stale_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--build-dir",
                                                                  build_dir.generic_string()},
                                                                 build_dir / "stale_build_output.txt",
                                                                 "stale mci build");
    if (!stale_outcome.exited || stale_outcome.exit_code == 0) {
        Fail("build with stale pre-phase54 .mci should fail");
    }
    ExpectOutputContains(stale_output,
                         "unsupported interface artifact format version",
                         "stale pre-phase54 .mci should fail with a format-cutover diagnostic");
}

void TestModuleBuildStateIsVersionedAndDeterministic(const std::filesystem::path& binary_root,
                                                     const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "state_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-state\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"phase13-state\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/alpha.mc",
              "func answer_alpha() i32 {\n"
              "    return 2\n"
              "}\n");
    WriteFile(project_root / "src/zeta.mc",
              "func answer_zeta() i32 {\n"
              "    return 5\n"
              "}\n");
    WriteFile(project_root / "src/main.mc",
              "import zeta\n"
              "import alpha\n"
              "\n"
              "func main() i32 {\n"
              "    return zeta.answer_zeta() + alpha.answer_alpha()\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "state_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     build_dir / "state_build_output.txt",
                                                     "deterministic state build");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("phase13 deterministic state build should succeed:\n" + output);
    }

    const auto state_path = build_dir / "state" /
                            (mc::support::SanitizeArtifactStem(project_root / "src/main.mc") + ".state.txt");
    const std::string state_text = ReadFile(state_path);
    ExpectOutputContains(state_text, "format\t2\n", "module build state should record its format version");
    ExpectOutputContains(state_text, "package\tphase13-state\n", "module build state should record package identity");

    const std::size_t alpha_hash = state_text.find("import_hash\talpha=");
    const std::size_t zeta_hash = state_text.find("import_hash\tzeta=");
    if (alpha_hash == std::string::npos || zeta_hash == std::string::npos) {
        Fail("module build state should record imported interface hashes for all imports");
    }
    if (!(alpha_hash < zeta_hash)) {
        Fail("module build state should serialize import hashes in deterministic sorted order");
    }
}

void TestForeignInterfaceArtifactFailsBuild(const std::filesystem::path& binary_root,
                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path source_project_root = binary_root / "foreign_artifact_source_project";
    const std::filesystem::path source_project_path = WriteBasicProject(
        source_project_root,
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path source_build_dir = binary_root / "foreign_artifact_source_build";
    std::filesystem::remove_all(source_build_dir);
    std::filesystem::create_directories(source_build_dir);

    const auto [source_outcome, source_output] = RunCommandCapture({mc_path.generic_string(),
                                                                    "build",
                                                                    "--project",
                                                                    source_project_path.generic_string(),
                                                                    "--build-dir",
                                                                    source_build_dir.generic_string()},
                                                                   source_build_dir / "build_output.txt",
                                                                   "foreign artifact source build");
    if (!source_outcome.exited || source_outcome.exit_code != 0) {
        Fail("phase13 foreign artifact source build should succeed:\n" + source_output);
    }

    const std::filesystem::path contaminated_project_root = binary_root / "foreign_artifact_target_project";
    const std::filesystem::path contaminated_project_path = WriteBasicProject(
        contaminated_project_root,
        "func answer() i32 {\n"
        "    return 11\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path contaminated_build_dir = binary_root / "foreign_artifact_target_build";
    std::filesystem::remove_all(contaminated_build_dir);
    std::filesystem::create_directories(contaminated_build_dir / "mci");

    const auto source_helper_mci = mc::support::ComputeDumpTargets(source_project_root / "src/helper.mc", source_build_dir).mci;
    const auto target_helper_mci = mc::support::ComputeDumpTargets(contaminated_project_root / "src/helper.mc", contaminated_build_dir).mci;
    WriteFile(target_helper_mci, ReadFile(source_helper_mci));

    const auto [contaminated_outcome, contaminated_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                "build",
                                                                                "--project",
                                                                                contaminated_project_path.generic_string(),
                                                                                "--build-dir",
                                                                                contaminated_build_dir.generic_string()},
                                                                               contaminated_build_dir / "contaminated_output.txt",
                                                                               "foreign artifact contaminated build");
    if (!contaminated_outcome.exited || contaminated_outcome.exit_code == 0) {
        Fail("build with foreign .mci contamination should fail");
    }
    ExpectOutputContains(contaminated_output,
                         "interface artifact source path mismatch",
                         "foreign .mci contamination should fail with metadata mismatch");
}

void TestInvalidStateFileForcesRebuild(const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "invalid_state_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "invalid_state_build";
    std::filesystem::remove_all(build_dir);

    const auto run_build = [&](const std::string& context) {
        const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                          "build",
                                                          "--project",
                                                          project_path.generic_string(),
                                                          "--build-dir",
                                                          build_dir.generic_string()},
                                                         build_dir / (context + ".txt"),
                                                         context);
        if (!outcome.exited || outcome.exit_code != 0) {
            Fail(context + " should succeed:\n" + output);
        }
    };

    std::filesystem::remove_all(build_dir);
    run_build("invalid_state_initial_build");

    const auto helper_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/helper.mc", build_dir).object;
    const auto helper_state = build_dir / "state" /
                              (mc::support::SanitizeArtifactStem(project_root / "src/helper.mc") + ".state.txt");
    const auto helper_object_time_1 = RequireWriteTime(helper_object);

    SleepForTimestampTick();
    WriteFile(helper_state,
              "format\t999\n"
              "target\tarm64-apple-darwin25.4.0\n");
    run_build("invalid_state_rebuild");

    if (!(RequireWriteTime(helper_object) > helper_object_time_1)) {
        Fail("invalid stale state file should force rebuilding the affected module object");
    }
}

void TestIncrementalRebuildBehavior(const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "incremental_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "func answer() i32 {\n"
        "    return 7\n"
        "}\n",
        "import helper\n"
        "\n"
        "func main() i32 {\n"
        "    return helper.answer()\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "incremental_build";
    std::filesystem::remove_all(build_dir);

    const auto run_build = [&](const std::string& context) {
        const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                          "build",
                                                          "--project",
                                                          project_path.generic_string(),
                                                          "--build-dir",
                                                          build_dir.generic_string()},
                                                         build_dir / (context + ".txt"),
                                                         context);
        if (!outcome.exited || outcome.exit_code != 0) {
            Fail(context + " should succeed:\n" + output);
        }
    };

    run_build("initial_build");

    const auto helper_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/helper.mc", build_dir).object;
    const auto main_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).object;
    const auto helper_mci = mc::support::ComputeDumpTargets(project_root / "src/helper.mc", build_dir).mci;
    const auto executable = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).executable;

    const auto helper_object_time_1 = RequireWriteTime(helper_object);
    const auto main_object_time_1 = RequireWriteTime(main_object);
    const auto helper_mci_time_1 = RequireWriteTime(helper_mci);
    const std::string helper_mci_text_1 = ReadFile(helper_mci);
    const auto executable_time_1 = RequireWriteTime(executable);

    SleepForTimestampTick();
    run_build("noop_build");

    if (RequireWriteTime(helper_object) != helper_object_time_1 || RequireWriteTime(main_object) != main_object_time_1 ||
        RequireWriteTime(executable) != executable_time_1) {
        Fail("no-op rebuild should reuse existing helper/main objects and executable outputs");
    }
    if (ReadFile(helper_mci) != helper_mci_text_1) {
        Fail("no-op rebuild should preserve helper interface artifact contents");
    }

    SleepForTimestampTick();
    WriteFile(project_root / "src/helper.mc",
              "func answer() i32 {\n"
              "    return 9\n"
              "}\n");
    run_build("impl_only_build");

    const auto helper_object_time_2 = RequireWriteTime(helper_object);
    const auto main_object_time_2 = RequireWriteTime(main_object);
    const auto helper_mci_time_2 = RequireWriteTime(helper_mci);
    const std::string helper_mci_text_2 = ReadFile(helper_mci);
    const auto executable_time_2 = RequireWriteTime(executable);
    if (!(helper_object_time_2 > helper_object_time_1)) {
        Fail("implementation-only helper edit should rebuild the helper object");
    }
    if (main_object_time_2 != main_object_time_1) {
        Fail("implementation-only helper edit should not rebuild the dependent main object");
    }
    if (helper_mci_text_2 != helper_mci_text_1) {
        Fail("implementation-only helper edit should preserve the helper interface artifact contents");
    }
    if (!(executable_time_2 > executable_time_1)) {
        Fail("implementation-only helper edit should relink the final executable");
    }

    const auto [impl_run_outcome, impl_run_output] = RunCommandCapture({executable.generic_string()},
                                                                       build_dir / "impl_only_run.txt",
                                                                       "impl-only executable run");
    if (!impl_run_outcome.exited || impl_run_outcome.exit_code != 9) {
        Fail("implementation-only helper edit should change runtime behavior to 9, got:\n" + impl_run_output);
    }

    SleepForTimestampTick();
    WriteFile(project_root / "src/helper.mc",
              "func answer() i32 {\n"
              "    return 9\n"
              "}\n"
              "\n"
              "func extra() i32 {\n"
              "    return 1\n"
              "}\n");
    run_build("interface_change_build");

    if (!(RequireWriteTime(helper_object) > helper_object_time_2)) {
        Fail("interface-changing helper edit should rebuild the helper object");
    }
    if (!(RequireWriteTime(main_object) > main_object_time_2)) {
        Fail("interface-changing helper edit should rebuild the dependent main object");
    }
    if (!(RequireWriteTime(helper_mci) > helper_mci_time_2)) {
        Fail("interface-changing helper edit should rewrite the helper interface artifact");
    }
}

void TestPrivateAndInternalIncrementalBehavior(const std::filesystem::path& binary_root,
                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "private_internal_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase54-private-internal\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"pkg_app\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\", \"pkg_common\"]\n"
              "\n"
              "[targets.app.packages.pkg_common]\n"
              "roots = [\"pkg_common\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "pkg_common/internal.mc",
              "@private\n"
              "func hidden_seed() i32 {\n"
              "    return 3\n"
              "}\n"
              "\n"
              "func visible_value() i32 {\n"
              "    return hidden_seed() + 4\n"
              "}\n");
    WriteFile(project_root / "pkg_common/helper.mc",
              "import internal\n"
              "\n"
              "func answer() i32 {\n"
              "    return internal.visible_value()\n"
              "}\n");
    WriteFile(project_root / "src/main.mc",
              "import helper\n"
              "\n"
              "func main() i32 {\n"
              "    return helper.answer()\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "private_internal_build";
    std::filesystem::remove_all(build_dir);

    const auto run_build = [&](const std::string& context) {
        const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                          "build",
                                                          "--project",
                                                          (project_root / "build.toml").generic_string(),
                                                          "--build-dir",
                                                          build_dir.generic_string()},
                                                         build_dir / (context + ".txt"),
                                                         context);
        if (!outcome.exited || outcome.exit_code != 0) {
            Fail(context + " should succeed:\n" + output);
        }
    };

    run_build("private_internal_initial");

    const auto internal_mci = mc::support::ComputeDumpTargets(project_root / "pkg_common/internal.mc", build_dir).mci;
    const auto helper_object = mc::support::ComputeBuildArtifactTargets(project_root / "pkg_common/helper.mc", build_dir).object;
    const auto main_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).object;
    const auto executable = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir).executable;

    const std::string internal_mci_text = ReadFile(internal_mci);
    ExpectOutputContains(internal_mci_text,
                         "package\tpkg_common",
                         "internal module .mci should record the owning package identity");
    ExpectOutputContains(internal_mci_text,
                         "module_kind\tinternal",
                         "internal module .mci should record internal module kind");

    const auto helper_object_time_1 = RequireWriteTime(helper_object);
    const auto main_object_time_1 = RequireWriteTime(main_object);
    const auto internal_mci_time_1 = RequireWriteTime(internal_mci);
    const std::string internal_mci_text_1 = ReadFile(internal_mci);
    const auto executable_time_1 = RequireWriteTime(executable);

    SleepForTimestampTick();
    WriteFile(project_root / "pkg_common/internal.mc",
              "@private\n"
              "func hidden_seed() i32 {\n"
              "    return 9\n"
              "}\n"
              "\n"
              "func visible_value() i32 {\n"
              "    return hidden_seed() + 4\n"
              "}\n");
    run_build("private_internal_private_change");

    if (RequireWriteTime(helper_object) != helper_object_time_1) {
        Fail("@private internal edit should not rebuild the dependent helper object");
    }
    if (RequireWriteTime(main_object) != main_object_time_1) {
        Fail("@private internal edit should not rebuild the downstream main object");
    }
    if (ReadFile(internal_mci) != internal_mci_text_1) {
        Fail("@private internal edit should preserve the internal module interface artifact contents");
    }
    if (!(RequireWriteTime(executable) > executable_time_1)) {
        Fail("@private internal edit should still relink the final executable");
    }

    const auto [private_run_outcome, private_run_output] = RunCommandCapture({executable.generic_string()},
                                                                             build_dir / "private_run.txt",
                                                                             "private-change executable run");
    if (!private_run_outcome.exited || private_run_outcome.exit_code != 13) {
        Fail("@private internal edit should change runtime behavior to 13, got:\n" + private_run_output);
    }

    SleepForTimestampTick();
    WriteFile(project_root / "pkg_common/internal.mc",
              "@private\n"
              "func hidden_seed() i32 {\n"
              "    return 9\n"
              "}\n"
              "\n"
              "func visible_value() i32 {\n"
              "    return hidden_seed() + 4\n"
              "}\n"
              "\n"
              "func extra_visible() i32 {\n"
              "    return 10\n"
              "}\n");
    run_build("private_internal_interface_change");

    if (!(RequireWriteTime(helper_object) > helper_object_time_1)) {
        Fail("public internal edit should rebuild the direct dependent helper object");
    }
    if (RequireWriteTime(main_object) != main_object_time_1) {
        Fail("public internal edit should not rebuild the downstream main object when helper's interface stays stable");
    }
    if (!(RequireWriteTime(internal_mci) > internal_mci_time_1)) {
        Fail("public internal edit should rewrite the internal module interface artifact");
    }
}

}  // namespace

namespace mc::tool_tests {

void RunBuildStateToolSuite(const std::filesystem::path& binary_root,
                            const std::filesystem::path& mc_path) {
    const std::filesystem::path suite_root = binary_root / "tool" / "build_state";

    TestProjectBuildAndMciEmission(suite_root, mc_path);
    TestProjectTestTargetBuildsAndRuns(suite_root, mc_path);
    TestProjectImportedGlobalMirDeclarations(suite_root, mc_path);
    TestProjectImportedAbiTypeMirDeclarations(suite_root, mc_path);
    TestProjectImportedGenericNominalIdentity(suite_root, mc_path);
    TestProjectImportedGenericVariantConstructor(suite_root, mc_path);
    TestProjectImportedGenericVariantInspection(suite_root, mc_path);
    TestCorruptedInterfaceArtifactFailsBuild(suite_root, mc_path);
    TestStaleInterfaceArtifactFormatFailsBuild(suite_root, mc_path);
    TestModuleBuildStateIsVersionedAndDeterministic(suite_root, mc_path);
    TestForeignInterfaceArtifactFailsBuild(suite_root, mc_path);
    TestInvalidStateFileForcesRebuild(suite_root, mc_path);
    TestIncrementalRebuildBehavior(suite_root, mc_path);
    TestPrivateAndInternalIncrementalBehavior(suite_root, mc_path);
}

}  // namespace mc::tool_tests
