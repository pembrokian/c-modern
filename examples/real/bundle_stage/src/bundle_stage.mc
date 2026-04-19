import errors
import fmt
import fs
import io
import mem

const BUNDLE_ERR_INVALID_SIZE: usize = 1
const BUNDLE_ERR_ALLOC: usize = 2

struct StagedBundle {
    run: mem.Run
    used: usize
}

func empty_stage() StagedBundle {
    return StagedBundle{ run: mem.Run{ raw: 0 }, used: 0 }
}

func release(stage: StagedBundle) {
    mem.run_deinit(stage.run)
}

func stage_payload(stage: StagedBundle) Slice<u8> {
    full: Slice<u8> = mem.run_slice(stage.run)
    return full[0:stage.used]
}

func stage_granules(stage: StagedBundle) usize {
    granule: usize = mem.run_granule_bytes()
    if granule == 0 {
        return 0
    }
    return mem.run_capacity(stage.run) / granule
}

func byte_sum(bytes: Slice<u8>) u64 {
    total: u64 = 0
    index: usize = 0
    while index < bytes.len {
        total = total + (u64)(bytes[index])
        index = index + 1
    }
    return total
}

func stage_byte_sum(stage: StagedBundle) u64 {
    return byte_sum(stage_payload(stage))
}

func stage_path(path: str) (StagedBundle, errors.Error) {
    stage: StagedBundle = empty_stage()
    size: isize
    err: errors.Error
    size, err = fs.file_size(path)
    if errors.is_err(err) {
        return stage, err
    }
    if size < 0 {
        return stage, errors.fail_io(BUNDLE_ERR_INVALID_SIZE)
    }

    used: usize = (usize)(size)
    run: mem.Run = mem.run_init(mem.default_allocator(), used)
    if mem.run_capacity(run) == 0 {
        return stage, errors.fail_mem(BUNDLE_ERR_ALLOC)
    }

    full: Slice<u8> = mem.run_slice(run)
    err = fs.read_exact_err(path, full[0:used])
    if errors.is_err(err) {
        mem.run_deinit(run)
        return stage, err
    }

    return StagedBundle{ run: run, used: used }, errors.ok()
}

func write_u64_line(label: str, value: u64) i32 {
    digits: *Buffer<u8>
    err: errors.Error
    digits, err = fmt.sprint_u64(mem.default_allocator(), value)
    if errors.is_err(err) || digits == nil {
        return 1
    }
    defer mem.buffer_free<u8>(digits)

    text_bytes: Slice<u8> = mem.slice_from_buffer<u8>(digits)
    text: str = str{ ptr: text_bytes.ptr, len: text_bytes.len }
    if io.write(label) != 0 {
        return 1
    }
    if io.write("=") != 0 {
        return 1
    }
    if io.write(text) != 0 {
        return 1
    }
    if io.write_line("") != 0 {
        return 1
    }
    return 0
}

func write_usize_line(label: str, value: usize) i32 {
    return write_u64_line(label, (u64)(value))
}

func print_stage(stage: StagedBundle) i32 {
    status: i32 = write_usize_line("bundle-bytes", stage.used)
    if status != 0 {
        return status
    }
    status = write_usize_line("run-bytes", mem.run_capacity(stage.run))
    if status != 0 {
        return status
    }
    status = write_usize_line("granules", stage_granules(stage))
    if status != 0 {
        return status
    }
    return write_u64_line("sum", stage_byte_sum(stage))
}

func run(path: str) i32 {
    stage: StagedBundle
    err: errors.Error
    stage, err = stage_path(path)
    if errors.is_err(err) {
        if errors.kind(err) == errors.kind_mem() {
            return 90
        }
        return 91
    }
    defer release(stage)
    return print_stage(stage)
}