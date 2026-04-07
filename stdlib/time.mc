distinct Duration = u64

@private
const NANOSECONDS_PER_MILLISECOND: u64 = 1000000
@private
const NANOSECONDS_PER_SECOND: u64 = 1000000000

@private
extern(c) func __mc_time_monotonic_nanos() u64

@private
func max_duration_nanos() u64 {
    return (u64)(-1)
}

@private
func saturating_mul(left: u64, right: u64) u64 {
    if left == 0 {
        return 0
    }

    product: u64 = left * right
    if product / left != right {
        return max_duration_nanos()
    }
    return product
}

@private
func saturating_add(left: u64, right: u64) u64 {
    sum: u64 = left + right
    if sum < left {
        return max_duration_nanos()
    }
    return sum
}

func from_nanos(value: u64) Duration {
    return (Duration)(value)
}

func nanos(value: Duration) u64 {
    return (u64)(value)
}

func millis(value: u64) Duration {
    return (Duration)(saturating_mul(value, NANOSECONDS_PER_MILLISECOND))
}

func seconds(value: u64) Duration {
    return (Duration)(saturating_mul(value, NANOSECONDS_PER_SECOND))
}

func add(left: Duration, right: Duration) Duration {
    return (Duration)(saturating_add((u64)(left), (u64)(right)))
}

func less(left: Duration, right: Duration) bool {
    return (u64)(left) < (u64)(right)
}

func monotonic() Duration {
    return (Duration)(__mc_time_monotonic_nanos())
}
