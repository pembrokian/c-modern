#ifndef MC_COMPILER_MIR_VALIDATE_INTERNAL_H
#define MC_COMPILER_MIR_VALIDATE_INTERNAL_H

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "compiler/mir/mir_internal.h"

namespace mc::mir {

using ValidationReporter = std::function<void(const std::string&)>;

bool ValidateFunctionStructure(const Function& function,
                               const ValidationReporter& report,
                               std::unordered_map<std::string, const BasicBlock*>& blocks_by_label,
                               std::unordered_set<std::string>& reachable_blocks);

void ValidateFunctionDominance(const Function& function,
                               const std::unordered_map<std::string, const BasicBlock*>& blocks_by_label,
                               const std::unordered_set<std::string>& reachable_blocks,
                               const ValidationReporter& report);

}  // namespace mc::mir

#endif