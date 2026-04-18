import boot
import io
import live_receive
import scenarios
import strings

const LIVE_MODE_ARG: str = "live"

func run(state: *boot.KernelBootState, args: Slice<cstr>) i32 {
    if args.len == 1 {
        return scenarios.run(state)
    }
    if args.len == 2 && strings.eq(args[1], LIVE_MODE_ARG) {
        return live_receive.run_stdin(state)
    }
    if io.write_line("usage: kernel [live]") != 0 {
        return 64
    }
    return 64
}