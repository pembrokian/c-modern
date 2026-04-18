import boot
import kernel_entry

func main(args: Slice<cstr>) i32 {
    state := boot.kernel_init()
    return kernel_entry.run(&state, args)
}

