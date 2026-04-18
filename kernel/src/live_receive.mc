import boot
import errors
import io
import kernel_dispatch
import primitives
import service_effect
import service_topology
import syscall

const LIVE_FRAME_LEN: usize = 4
const LIVE_POLL_TIMEOUT_MS: i32 = 100
const LIVE_INPUT_FD: io.File = 0
const LIVE_OUTPUT_FD: io.File = 1
const LIVE_SOURCE_PID: u32 = 1

enum LiveReceiveKind {
    NoInput,
    Closed,
    Received,
    Failed,
}

struct LiveReceiveState {
    file: io.File
    poller: *io.Poller
    buf: [LIVE_FRAME_LEN]u8
    len: usize
}

struct LiveReceiveStep {
    kind: LiveReceiveKind
    observation: syscall.ReceiveObservation
}

func livewith(s: LiveReceiveState, buf: [LIVE_FRAME_LEN]u8, len: usize) LiveReceiveState {
    return LiveReceiveState{ file: s.file, poller: s.poller, buf: buf, len: len }
}

func live_empty_step(kind: LiveReceiveKind) LiveReceiveStep {
    return LiveReceiveStep{ kind: kind, observation: syscall.empty_receive_observation() }
}

func live_observation(buf: [LIVE_FRAME_LEN]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: LIVE_SOURCE_PID, payload_len: LIVE_FRAME_LEN, received_handle_slot: 0, received_handle_count: 0, payload: buf }
}

func live_open(file: io.File) (LiveReceiveState, errors.Error) {
    poller: *io.Poller
    err: errors.Error
    poller, err = io.poller_new()
    if !errors.is_ok(err) {
        return LiveReceiveState{ file: file, poller: poller, buf: primitives.zero_payload(), len: 0 }, err
    }

    err = io.poller_add(poller, file, io.EVENT_READABLE)
    if !errors.is_ok(err) {
        io.poller_close(poller)
        return LiveReceiveState{ file: file, poller: poller, buf: primitives.zero_payload(), len: 0 }, err
    }

    return LiveReceiveState{ file: file, poller: poller, buf: primitives.zero_payload(), len: 0 }, errors.ok()
}

func live_close(s: LiveReceiveState) {
    io.poller_close(s.poller)
}

func live_step(s: *LiveReceiveState, timeout_ms: i32) LiveReceiveStep {
    current := *s
    events: [1]io.Event
    ready: usize
    err: errors.Error
    ready, err = io.poller_wait(current.poller, (Slice<io.Event>)(events), timeout_ms)
    if !errors.is_ok(err) {
        return live_empty_step(LiveReceiveKind.Failed)
    }
    if ready == 0 {
        return live_empty_step(LiveReceiveKind.NoInput)
    }

    event: io.Event = events[0]
    if event.file != current.file {
        return live_empty_step(LiveReceiveKind.Failed)
    }
    if event.failed {
        return live_empty_step(LiveReceiveKind.Failed)
    }
    if !event.readable {
        return live_empty_step(LiveReceiveKind.NoInput)
    }

    next_buf: [LIVE_FRAME_LEN]u8 = current.buf
    bytes: Slice<u8> = (Slice<u8>)(next_buf)
    nread: usize
    nread, err = io.read(current.file, bytes[current.len:LIVE_FRAME_LEN])
    if !errors.is_ok(err) {
        return live_empty_step(LiveReceiveKind.Failed)
    }
    if nread == 0 {
        *s = livewith(current, primitives.zero_payload(), 0)
        return live_empty_step(LiveReceiveKind.Closed)
    }

    next_len := current.len + nread
    if next_len < LIVE_FRAME_LEN {
        *s = livewith(current, next_buf, next_len)
        return live_empty_step(LiveReceiveKind.NoInput)
    }

    *s = livewith(current, primitives.zero_payload(), 0)
    return LiveReceiveStep{ kind: LiveReceiveKind.Received, observation: live_observation(next_buf) }
}

func live_emit_reply(effect: service_effect.Effect) errors.Error {
    if service_effect.effect_has_reply(effect) == 0 {
        return errors.ok()
    }
    reply_len := service_effect.effect_reply_payload_len(effect)
    if reply_len == 0 {
        return errors.ok()
    }
    payload := service_effect.effect_reply_payload(effect)
    ignored: usize
    err: errors.Error
    ignored, err = io.write_file(LIVE_OUTPUT_FD, ((Slice<u8>)(payload))[0:reply_len])
    _ = ignored
    return err
}

func run_live(state: *boot.KernelBootState, file: io.File) i32 {
    source: LiveReceiveState
    err: errors.Error
    source, err = live_open(file)
    if !errors.is_ok(err) {
        return 70
    }
    defer live_close(source)

    while true {
        step := live_step(&source, LIVE_POLL_TIMEOUT_MS)
        switch step.kind {
        case LiveReceiveKind.NoInput:
            continue
        case LiveReceiveKind.Closed:
            return 0
        case LiveReceiveKind.Failed:
            return 71
        case LiveReceiveKind.Received:
            effect := kernel_dispatch.kernel_dispatch_step(state, step.observation)
            err = live_emit_reply(effect)
            if !errors.is_ok(err) {
                return 72
            }
        }
    }

    return 73
}

func run_stdin(state: *boot.KernelBootState) i32 {
    return run_live(state, LIVE_INPUT_FD)
}