import internal

func count_open_items(text: str) usize {
    return internal.count_open_items(text)
}

func count_closed_items(text: str) usize {
    return internal.count_closed_items(text)
}

func count_urgent_open_items(text: str) usize {
    return internal.count_urgent_open_items(text)
}