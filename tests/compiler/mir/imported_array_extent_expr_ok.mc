import helper_extents

const LOCAL_OFFSET: usize = 1

func widen(input: [6]u8) [6]u8 {
    mirror: [helper_extents.WIDTH + LOCAL_OFFSET]u8 = input
    return mirror
}
