distinct UserId = i32
distinct GroupId = i32

func parse_user(raw: i32) UserId {
    return (UserId)(raw)
}

func store_group(id: GroupId) i32 {
    return (i32)(id)
}

func main() i32 {
    user: UserId = parse_user(41)
    group: GroupId = (GroupId)(user)
    return store_group(group)
}