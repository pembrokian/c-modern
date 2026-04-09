struct InitImage {
    image_id: u32
    entry_pc: usize
    stack_top: usize
}

func bootstrap_image() InitImage {
    return InitImage{ image_id: 1, entry_pc: 12288, stack_top: 16384 }
}