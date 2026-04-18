import boot
import errors
import io
import kernel_dispatch
import live_receive
import serial_protocol
import service_effect
import syscall
import sync

struct WriterContext {
    file: io.File
    status: *i32
}

func close_ignored(file: io.File) {
    ignored: errors.Error = io.close(file)
    if !errors.is_ok(ignored) {
        return
    }
}

func writer(ctx: *WriterContext) {
    current := *ctx
    payload := serial_protocol.encode_echo(65, 66)
    _, err := io.write_file(current.file, (Slice<u8>)(payload))
    if !errors.is_ok(err) {
        *current.status = 10
        return
    }
    err = io.close(current.file)
    if !errors.is_ok(err) {
        *current.status = 11
        return
    }
    *current.status = 0
}

func main() i32 {
    pipe: io.Pipe
    err: errors.Error
    pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return 1
    }
    defer close_ignored(pipe.read_end)

    source: live_receive.LiveReceiveState
    source, err = live_receive.live_open(pipe.read_end)
    if !errors.is_ok(err) {
        close_ignored(pipe.write_end)
        return 2
    }
    defer live_receive.live_close(source)

    step := live_receive.live_step(&source, 0)
    if step.kind != live_receive.LiveReceiveKind.NoInput {
        close_ignored(pipe.write_end)
        return 3
    }

    writer_status: i32 = 0
    writer_ctx := WriterContext{ file: pipe.write_end, status: &writer_status }
    thread: sync.Thread
    thread, err = sync.thread_spawn<WriterContext>(writer, &writer_ctx)
    if !errors.is_ok(err) {
        close_ignored(pipe.write_end)
        return 4
    }

    step = live_receive.live_step(&source, 2000)
    if step.kind != live_receive.LiveReceiveKind.Received {
        ignored: errors.Error = sync.thread_join(thread)
        if !errors.is_ok(ignored) {
            return 5
        }
        return 6
    }
    if step.observation.payload_len != 4 {
        ignored: errors.Error = sync.thread_join(thread)
        if !errors.is_ok(ignored) {
            return 7
        }
        return 8
    }

    ignored: errors.Error = sync.thread_join(thread)
    if !errors.is_ok(ignored) {
        return 9
    }
    if writer_status != 0 {
        return 10
    }

    state := boot.kernel_init()
    effect := kernel_dispatch.kernel_dispatch_step(&state, step.observation)
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 11
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return 12
    }
    payload := service_effect.effect_reply_payload(effect)
    if payload[0] != 65 {
        return 13
    }
    if payload[1] != 66 {
        return 14
    }

    step = live_receive.live_step(&source, 2000)
    if step.kind != live_receive.LiveReceiveKind.Closed {
        return 15
    }

    return 0
}