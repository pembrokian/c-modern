#ifndef C_MODERN_COMPILER_SUPPORT_SOURCE_SPAN_H_
#define C_MODERN_COMPILER_SUPPORT_SOURCE_SPAN_H_

#include <cstddef>

namespace mc::support {

struct SourcePosition {
    std::size_t line = 1;
    std::size_t column = 1;
};

struct SourceSpan {
    SourcePosition begin {};
    SourcePosition end {};
};

inline constexpr SourceSpan kDefaultSourceSpan {};

}  // namespace mc::support

#endif  // C_MODERN_COMPILER_SUPPORT_SOURCE_SPAN_H_
