// Compatibility facade for launcher scenario imports used by repo-owned projects.

import scenario_launcher_demo_probe
import scenario_launcher_manifest_probe
import scenario_launcher_service_probe

func run_launcher_service_probe() i32 {
    return scenario_launcher_service_probe.run_launcher_service_probe()
}

func run_launcher_manifest_probe() i32 {
    return scenario_launcher_manifest_probe.run_launcher_manifest_probe()
}

func run_launcher_installed_workflow_demo_probe() i32 {
    return scenario_launcher_demo_probe.run_launcher_installed_workflow_demo_probe()
}
