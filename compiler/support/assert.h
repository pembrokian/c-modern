#ifndef C_MODERN_COMPILER_SUPPORT_ASSERT_H_
#define C_MODERN_COMPILER_SUPPORT_ASSERT_H_

#include <cassert>

// MC_UNREACHABLE(msg)
//
// Marks a code path that must never be reached during correct operation.
// In a debug build the assert fires immediately with the given message so that
// newly added enum cases are caught at the first affected dump or lowering.
// In a release build __builtin_unreachable() lets the optimiser elide the
// dead branch.
//
// Usage: replace a silent fallback return in a switch default with
//   MC_UNREACHABLE("unhandled Kind in ToString");
//
// IMPORTANT: do NOT use this for user-visible error conditions.  Use
// diagnostic reporting for those.  MC_UNREACHABLE is a programmer-error guard.
#define MC_UNREACHABLE(msg)                         \
    do {                                            \
        assert(false && msg);                       \
        __builtin_unreachable();                    \
    } while (0)

#endif  // C_MODERN_COMPILER_SUPPORT_ASSERT_H_
