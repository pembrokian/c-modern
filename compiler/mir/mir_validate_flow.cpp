#include "compiler/mir/mir_validate_internal.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace mc::mir {

bool ValidateFunctionStructure(const Function& function,
                               const ValidationReporter& report,
                               std::unordered_map<std::string, const BasicBlock*>& blocks_by_label,
                               std::unordered_set<std::string>& reachable_blocks) {
    bool ok = true;
    auto fail = [&](const std::string& message) {
        report(message);
        ok = false;
    };

    if (function.blocks.empty()) {
        fail("function has no MIR blocks: " + function.name);
        return false;
    }

    std::unordered_set<std::string> block_labels;
    for (const auto& block : function.blocks) {
        if (!block_labels.insert(block.label).second) {
            fail("duplicate MIR block label in function " + function.name + ": " + block.label);
        }
        blocks_by_label[block.label] = &block;
    }

    for (const auto& block : function.blocks) {
        switch (block.terminator.kind) {
            case Terminator::Kind::kNone:
                fail("unterminated MIR block in function " + function.name + ": " + block.label);
                break;
            case Terminator::Kind::kReturn:
                if (!block.terminator.true_target.empty() || !block.terminator.false_target.empty()) {
                    fail("return terminator must not name branch targets in function " + function.name + ": " + block.label);
                }
                break;
            case Terminator::Kind::kBranch:
                if (!block.terminator.values.empty()) {
                    fail("branch terminator must not carry values in function " + function.name + ": " + block.label);
                }
                if (block.terminator.true_target.empty()) {
                    fail("branch terminator must name a target in function " + function.name + ": " + block.label);
                } else if (!block_labels.contains(block.terminator.true_target)) {
                    fail("branch targets missing MIR block in function " + function.name + ": " + block.terminator.true_target);
                }
                if (!block.terminator.false_target.empty()) {
                    fail("branch terminator must not name a false target in function " + function.name + ": " + block.label);
                }
                break;
            case Terminator::Kind::kCondBranch:
                if (block.terminator.values.size() != 1) {
                    fail("conditional branch must use exactly one condition value in function " + function.name + ": " + block.label);
                }
                if (block.terminator.true_target.empty()) {
                    fail("conditional branch must name a true target in function " + function.name + ": " + block.label);
                } else if (!block_labels.contains(block.terminator.true_target)) {
                    fail("conditional branch true target missing MIR block in function " + function.name + ": " +
                         block.terminator.true_target);
                }
                if (block.terminator.false_target.empty()) {
                    fail("conditional branch must name a false target in function " + function.name + ": " + block.label);
                } else if (!block_labels.contains(block.terminator.false_target)) {
                    fail("conditional branch false target missing MIR block in function " + function.name + ": " +
                         block.terminator.false_target);
                }
                break;
        }
    }

    std::vector<std::string> worklist;
    worklist.push_back(function.blocks.front().label);
    while (!worklist.empty()) {
        const std::string label = worklist.back();
        worklist.pop_back();
        if (!reachable_blocks.insert(label).second) {
            continue;
        }

        const auto found_block = blocks_by_label.find(label);
        if (found_block == blocks_by_label.end()) {
            continue;
        }

        const auto& terminator = found_block->second->terminator;
        if (terminator.kind == Terminator::Kind::kBranch) {
            worklist.push_back(terminator.true_target);
        }
        if (terminator.kind == Terminator::Kind::kCondBranch) {
            worklist.push_back(terminator.true_target);
            worklist.push_back(terminator.false_target);
        }
    }

    return ok;
}

void ValidateFunctionDominance(const Function& function,
                               const std::unordered_map<std::string, const BasicBlock*>& blocks_by_label,
                               const std::unordered_set<std::string>& reachable_blocks,
                               const ValidationReporter& report) {
    std::unordered_map<std::string, std::vector<std::string>> preds;
    for (const auto& label : reachable_blocks) {
        preds.emplace(label, std::vector<std::string>{});
    }
    for (const auto& blk : function.blocks) {
        if (!reachable_blocks.contains(blk.label)) {
            continue;
        }
        auto add_edge = [&](const std::string& target) {
            if (reachable_blocks.contains(target)) {
                preds[target].push_back(blk.label);
            }
        };
        if (blk.terminator.kind == Terminator::Kind::kBranch) {
            add_edge(blk.terminator.true_target);
        } else if (blk.terminator.kind == Terminator::Kind::kCondBranch) {
            add_edge(blk.terminator.true_target);
            add_edge(blk.terminator.false_target);
        }
    }

    const std::string entry_label = function.blocks.front().label;
    std::vector<std::string> rpo;
    {
        std::unordered_set<std::string> visited;
        std::vector<std::pair<std::string, bool>> stack;
        stack.push_back({entry_label, false});
        while (!stack.empty()) {
            auto [label, processed] = stack.back();
            stack.pop_back();
            if (processed) {
                rpo.push_back(label);
                continue;
            }
            if (!visited.insert(label).second) {
                continue;
            }
            stack.push_back({label, true});
            const auto found = blocks_by_label.find(label);
            if (found == blocks_by_label.end()) {
                continue;
            }
            const auto& term = found->second->terminator;
            if (term.kind == Terminator::Kind::kCondBranch) {
                stack.push_back({term.false_target, false});
                stack.push_back({term.true_target, false});
            } else if (term.kind == Terminator::Kind::kBranch) {
                stack.push_back({term.true_target, false});
            }
        }
    }
    std::reverse(rpo.begin(), rpo.end());

    std::unordered_map<std::string, int> rpo_index;
    for (int index = 0; index < static_cast<int>(rpo.size()); ++index) {
        rpo_index[rpo[index]] = index;
    }

    std::unordered_map<std::string, std::string> idom;
    idom[entry_label] = entry_label;
    auto intersect = [&](std::string first, std::string second) {
        while (first != second) {
            while (rpo_index[first] < rpo_index[second]) {
                first = idom[first];
            }
            while (rpo_index[second] < rpo_index[first]) {
                second = idom[second];
            }
        }
        return first;
    };

    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& label : rpo) {
            if (label == entry_label) {
                continue;
            }
            std::string new_idom;
            for (const auto& pred : preds[label]) {
                if (idom.contains(pred)) {
                    new_idom = pred;
                    break;
                }
            }
            if (new_idom.empty()) {
                continue;
            }
            for (const auto& pred : preds[label]) {
                if (pred == new_idom || !idom.contains(pred)) {
                    continue;
                }
                new_idom = intersect(pred, new_idom);
            }
            if (!idom.contains(label) || idom[label] != new_idom) {
                idom[label] = new_idom;
                changed = true;
            }
        }
    }

    std::unordered_map<std::string, std::string> value_defs;
    for (const auto& blk : function.blocks) {
        if (!reachable_blocks.contains(blk.label)) {
            continue;
        }
        for (const auto& instr : blk.instructions) {
            if (!instr.result.empty()) {
                value_defs[instr.result] = blk.label;
            }
        }
    }

    auto dominates = [&](const std::string& def_block, const std::string& use_block) {
        if (def_block == use_block) {
            return true;
        }
        if (!idom.contains(use_block)) {
            return false;
        }
        std::string cursor = use_block;
        while (cursor != entry_label) {
            cursor = idom[cursor];
            if (cursor == def_block) {
                return true;
            }
        }
        return false;
    };

    auto check_operand_dominance = [&](const std::string& operand, const std::string& use_block) {
        const auto found_def = value_defs.find(operand);
        if (found_def == value_defs.end()) {
            return;
        }
        if (!dominates(found_def->second, use_block)) {
            report("MIR operand does not dominate use in function " + function.name + ": " + operand);
        }
    };

    for (const auto& blk : function.blocks) {
        if (!reachable_blocks.contains(blk.label)) {
            continue;
        }
        for (const auto& instr : blk.instructions) {
            for (const auto& operand : instr.operands) {
                if (!operand.empty()) {
                    check_operand_dominance(operand, blk.label);
                }
            }
        }
        for (const auto& value : blk.terminator.values) {
            if (!value.empty()) {
                check_operand_dominance(value, blk.label);
            }
        }
    }
}

}  // namespace mc::mir