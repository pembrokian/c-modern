// Regression: local variable declared as [CAP]u8 where CAP is a module-scoped
// const must resolve to the same array type as [4]u8.  Before Phase 169, MIR
// lowering used TypeFromAst (unevaluated) for local type annotations, producing
// a [CAP]u8 != [4]u8 type mismatch at the store_local validation step.
const CAP: usize = 4

func fill(buf: [CAP]u8, val: u8) [CAP]u8 {
    next: [CAP]u8 = buf
    next[0] = val
    return next
}
