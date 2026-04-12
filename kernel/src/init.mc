import address_space
import capability

struct InitImage {
    image_id: u32
    image_base: usize
    image_size: usize
    entry_pc: usize
    stack_base: usize
    stack_top: usize
    stack_size: usize
}

func bootstrap_image() InitImage {
    // These addresses are proof-owned bootstrap scaffolding until a real loader lands.
    return InitImage{ image_id: 1, image_base: 65536, image_size: 8192, entry_pc: 66048, stack_base: 131072, stack_top: 139264, stack_size: 8192 }
}

func bootstrap_image_valid(image: InitImage) bool {
    if image.image_id == 0 {
        return false
    }
    return address_space.bootstrap_layout_valid(image.image_base, image.image_size, image.entry_pc, image.stack_base, image.stack_size, image.stack_top)
}

struct BootstrapCapabilitySet {
    owner_pid: u32
    endpoint_handle_slot: u32
    endpoint_handle_count: usize
    program_capability_count: usize
    program_capability: capability.CapabilitySlot
    wait_handle_count: usize
    ambient_root_visible: u32
}

struct BootstrapHandoffObservation {
    owner_pid: u32
    authority_count: usize
    endpoint_handle_slot: u32
    program_capability_slot: u32
    program_object_id: u32
    ambient_root_visible: u32
}

struct ServiceRestartObservation {
    owner_pid: u32
    service_key: u32
    previous_service_pid: u32
    replacement_service_pid: u32
    shared_control_endpoint_id: u32
    restart_count: usize
    retained_boot_wiring_visible: u32
}

func empty_bootstrap_capability_set() BootstrapCapabilitySet {
    return BootstrapCapabilitySet{ owner_pid: 0, endpoint_handle_slot: 0, endpoint_handle_count: 0, program_capability_count: 0, program_capability: capability.empty_slot(), wait_handle_count: 0, ambient_root_visible: 0 }
}

func install_bootstrap_capability_set(owner_pid: u32, endpoint_handle_slot: u32, program_capability: capability.CapabilitySlot) BootstrapCapabilitySet {
    endpoint_handle_count: usize = 0
    installed_program: capability.CapabilitySlot = capability.empty_slot()
    program_capability_count: usize = 0

    if endpoint_handle_slot != 0 {
        endpoint_handle_count = 1
    }
    if capability.is_program_capability(program_capability) && program_capability.owner_pid == owner_pid {
        installed_program = program_capability
        program_capability_count = 1
    }

    return BootstrapCapabilitySet{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, endpoint_handle_count: endpoint_handle_count, program_capability_count: program_capability_count, program_capability: installed_program, wait_handle_count: 0, ambient_root_visible: 0 }
}

func observe_bootstrap_handoff(set: BootstrapCapabilitySet) BootstrapHandoffObservation {
    authority_count: usize = set.endpoint_handle_count + set.program_capability_count + set.wait_handle_count
    return BootstrapHandoffObservation{ owner_pid: set.owner_pid, authority_count: authority_count, endpoint_handle_slot: set.endpoint_handle_slot, program_capability_slot: set.program_capability.slot_id, program_object_id: set.program_capability.object_id, ambient_root_visible: set.ambient_root_visible }
}

func observe_service_restart(owner_pid: u32, service_key: u32, previous_service_pid: u32, replacement_service_pid: u32, shared_control_endpoint_id: u32, restart_count: usize, retained_boot_wiring_visible: u32) ServiceRestartObservation {
    return ServiceRestartObservation{ owner_pid: owner_pid, service_key: service_key, previous_service_pid: previous_service_pid, replacement_service_pid: replacement_service_pid, shared_control_endpoint_id: shared_control_endpoint_id, restart_count: restart_count, retained_boot_wiring_visible: retained_boot_wiring_visible }
}