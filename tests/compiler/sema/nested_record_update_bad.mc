struct Leaf {
    value: i32
}

struct Box {
    leaf: Leaf
    count: i32
}

func bad_unknown(box: Box) Box {
    return box with { leaf.missing: 1 }
}

func bad_non_record(box: Box) Box {
    return box with { count.value: 1 }
}

func bad_duplicate(box: Box) Box {
    return box with { leaf.value: 1, leaf.value: 2 }
}

func bad_conflict(box: Box) Box {
    return box with { leaf: Leaf{ value: 1 }, leaf.value: 2 }
}

func bad_type(box: Box) Box {
    return box with { leaf.value: true }
}