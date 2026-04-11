#ifndef C_MODERN_COMPILER_SEMA_LAYOUT_H_
#define C_MODERN_COMPILER_SEMA_LAYOUT_H_

#include <functional>
#include <string>

#include "compiler/sema/check.h"

namespace mc::sema {

using LayoutReportFn = std::function<void(const support::SourceSpan&, const std::string&)>;

LayoutInfo ComputeTypeLayout(Module& module,
                             const Type& type,
                             const support::SourceSpan& span,
                             const LayoutReportFn& report);

void ComputeTypeLayouts(Module& module,
                        const support::SourceSpan& span,
                        const LayoutReportFn& report);

}  // namespace mc::sema

#endif  // C_MODERN_COMPILER_SEMA_LAYOUT_H_