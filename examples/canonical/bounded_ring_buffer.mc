export { main }

const CAPACITY: usize = 4

func total(values: [4]i32, head: usize, len: usize) i32 {
    slots: Slice<i32> = (Slice<i32>)(values)
    sum: i32 = 0
    index: usize = 0
    while index < len {
        slot: usize = (head + index) % CAPACITY
        sum = sum + slots[slot]
        index = index + 1
    }
    return sum
}

func main() i32 {
    values: [4]i32
    values[0] = 0
    values[1] = 0
    values[2] = 0
    values[3] = 0
    head: usize = 0
    len: usize = 0
    tail: usize = 0
    value: i32 = 0

    if len == CAPACITY {
        return 90
    }
    tail = (head + len) % CAPACITY
    values[tail] = 11
    len = len + 1

    if len == CAPACITY {
        return 91
    }
    tail = (head + len) % CAPACITY
    values[tail] = 22
    len = len + 1

    if len == CAPACITY {
        return 92
    }
    tail = (head + len) % CAPACITY
    values[tail] = 33
    len = len + 1

    if len == CAPACITY {
        return 93
    }
    tail = (head + len) % CAPACITY
    values[tail] = 44
    len = len + 1

    if len != CAPACITY {
        return 94
    }
    tail = (head + len) % CAPACITY
    if tail != 0 {
        return 95
    }
    if total(values, head, len) != 110 {
        return 96
    }

    if len == 0 {
        return 97
    }
    value = values[head]
    head = (head + 1) % CAPACITY
    len = len - 1
    if value != 11 {
        return 98
    }
    if len == 0 {
        return 99
    }
    value = values[head]
    head = (head + 1) % CAPACITY
    len = len - 1
    if value != 22 {
        return 100
    }

    if len == CAPACITY {
        return 101
    }
    tail = (head + len) % CAPACITY
    values[tail] = 55
    len = len + 1

    if len == CAPACITY {
        return 102
    }
    tail = (head + len) % CAPACITY
    values[tail] = 66
    len = len + 1

    if total(values, head, len) != 198 {
        return 103
    }

    if len == 0 {
        return 104
    }
    value = values[head]
    head = (head + 1) % CAPACITY
    len = len - 1
    if value != 33 {
        return 105
    }
    if len == 0 {
        return 106
    }
    value = values[head]
    head = (head + 1) % CAPACITY
    len = len - 1
    if value != 44 {
        return 107
    }
    if len == 0 {
        return 108
    }
    value = values[head]
    head = (head + 1) % CAPACITY
    len = len - 1
    if value != 55 {
        return 109
    }
    if len == 0 {
        return 110
    }
    value = values[head]
    head = (head + 1) % CAPACITY
    len = len - 1
    if value != 66 {
        return 111
    }
    if len != 0 {
        return 112
    }
    if head != 2 {
        return 113
    }

    return 0
}