distinct UserId = i32
distinct GroupId = i32

func main(raw: i32) GroupId {
    user: UserId = (UserId)(raw)
    return user
}