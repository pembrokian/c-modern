export { main }

import review_status

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 90
    }
    return review_status.run_audit(args[1])
}