#ifndef C_MODERN_COMPILER_SUPPORT_TARGET_H_
#define C_MODERN_COMPILER_SUPPORT_TARGET_H_

#include <string_view>

namespace mc {

// Single source of truth for the bootstrap target triple and family.
// Both the backend (backend.cpp) and the driver (driver.cpp) reference these
// constants directly so that changing the OS version requires editing exactly
// one file.
constexpr std::string_view kBootstrapTriple = "arm64-apple-darwin25.4.0";
constexpr std::string_view kBootstrapTargetFamily = "arm64-apple-darwin";

}  // namespace mc

#endif  // C_MODERN_COMPILER_SUPPORT_TARGET_H_
