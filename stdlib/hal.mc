extern(c) func mmio_ptr<T>(addr: uintptr) *T
extern(c) func volatile_load<T>(ptr: *T) T
extern(c) func volatile_store<T>(ptr: *T, value: T)