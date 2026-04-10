enum TranslationRootState {
    None,
    Prepared,
    Active,
}

struct TranslationRoot {
    owner_asid: u32
    page_table_base: usize
    state: TranslationRootState
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
    if !can_activate(root) {
        return root
    }
    return TranslationRoot{ owner_asid: root.owner_asid, page_table_base: root.page_table_base, state: TranslationRootState.Active }
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