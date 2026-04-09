struct InitImage {
    image_id: u32
    image_base: usize
    image_size: usize
    entry_pc: usize
    stack_base: usize
    stack_top: usize
    stack_size: usize
}

func bootstrap_image() InitImage {
    return InitImage{ image_id: 1, image_base: 65536, image_size: 8192, entry_pc: 66048, stack_base: 131072, stack_top: 139264, stack_size: 8192 }
}