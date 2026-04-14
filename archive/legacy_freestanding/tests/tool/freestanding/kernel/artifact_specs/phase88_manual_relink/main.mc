extern(c) func phase88_kernel_delta() i32

func bootstrap_main() i32 {
    return 80 + phase88_kernel_delta()
}
