import hal

enum TranslationRootState {
    None,
    Prepared,
    Active,
}

enum ActivationBarrierKind {
    None,
    TranslationRootPublish,
}

struct TranslationRoot {
    owner_asid: u32
    page_table_base: usize
    state: TranslationRootState
}

struct ActivationBarrierObservation {
    translation_root: TranslationRoot
    barrier_kind: ActivationBarrierKind
    applied: u32
}

func empty_translation_root() TranslationRoot {
    return TranslationRoot{ owner_asid: 0, page_table_base: 0, state: TranslationRootState.None }
}

func bootstrap_translation_root(owner_asid: u32, page_table_base: usize) TranslationRoot {
    if page_table_base == 0 {
        return empty_translation_root()
    }
    return TranslationRoot{ owner_asid: owner_asid, page_table_base: page_table_base, state: TranslationRootState.Prepared }
}

func can_activate(root: TranslationRoot) bool {
    if state_score(root.state) != 2 {
        return false
    }
    if root.page_table_base == 0 {
        return false
    }
    return true
}

func activate(root: TranslationRoot) TranslationRoot {
    observation: ActivationBarrierObservation = activate_with_barrier(root)
    return observation.translation_root
}

func activate_with_barrier(root: TranslationRoot) ActivationBarrierObservation {
    if !can_activate(root) {
        return ActivationBarrierObservation{ translation_root: root, barrier_kind: ActivationBarrierKind.None, applied: 0 }
    }
    hal.memory_barrier()
    return ActivationBarrierObservation{ translation_root: TranslationRoot{ owner_asid: root.owner_asid, page_table_base: root.page_table_base, state: TranslationRootState.Active }, barrier_kind: ActivationBarrierKind.TranslationRootPublish, applied: 1 }
}

func validate_address_space_mmu_boundary() bool {
    root: TranslationRoot = bootstrap_translation_root(5, 32768)
    if state_score(root.state) != 2 {
        return false
    }
    if root.owner_asid != 5 {
        return false
    }
    if root.page_table_base != 32768 {
        return false
    }
    return state_score(activate(root).state) == 4
}

func validate_activation_barrier_boundary() bool {
    prepared_root: TranslationRoot = bootstrap_translation_root(7, 49152)
    barrier_observation: ActivationBarrierObservation = activate_with_barrier(prepared_root)
    if barrier_observation.applied != 1 {
        return false
    }
    if barrier_kind_score(barrier_observation.barrier_kind) != 2 {
        return false
    }
    if state_score(barrier_observation.translation_root.state) != 4 {
        return false
    }
    if barrier_observation.translation_root.owner_asid != 7 {
        return false
    }
    if barrier_observation.translation_root.page_table_base != 49152 {
        return false
    }
    blocked_observation: ActivationBarrierObservation = activate_with_barrier(empty_translation_root())
    if blocked_observation.applied != 0 {
        return false
    }
    return barrier_kind_score(blocked_observation.barrier_kind) == 1
}

func barrier_kind_score(kind: ActivationBarrierKind) i32 {
    switch kind {
    case ActivationBarrierKind.None:
        return 1
    case ActivationBarrierKind.TranslationRootPublish:
        return 2
    default:
        return 0
    }
    return 0
}

func state_score(state: TranslationRootState) i32 {
    switch state {
    case TranslationRootState.None:
        return 1
    case TranslationRootState.Prepared:
        return 2
    case TranslationRootState.Active:
        return 4
    default:
        return 0
    }
    return 0
}