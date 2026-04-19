struct RollupManifest {
    version: u8
    busy_open_floor: usize
    blocked_attention_floor: usize
    priority_bonus: usize
}

const ROLLUP_MANIFEST_VERSION: u8 = 1
const ROLLUP_MANIFEST_LEN: usize = 3

const DEFAULT_MANIFEST: RollupManifest = RollupManifest{
    version: ROLLUP_MANIFEST_VERSION,
    busy_open_floor: 2,
    blocked_attention_floor: 2,
    priority_bonus: 0
}

func default_manifest() RollupManifest {
    return DEFAULT_MANIFEST
}

func manifest_decode(version: u8, len: usize, busy_open_floor: u8, blocked_attention_floor: u8, priority_bonus: u8) RollupManifest {
    if version != ROLLUP_MANIFEST_VERSION {
        return default_manifest()
    }
    if len != ROLLUP_MANIFEST_LEN {
        return default_manifest()
    }
    return RollupManifest{
        version: version,
        busy_open_floor: usize(busy_open_floor),
        blocked_attention_floor: usize(blocked_attention_floor),
        priority_bonus: usize(priority_bonus)
    }
}