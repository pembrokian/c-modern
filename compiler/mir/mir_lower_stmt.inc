    void LowerStmtList(const std::vector<std::unique_ptr<Stmt>>& statements) {
        for (const auto& child : statements) {
            LowerStmt(*child);
        }
    }

    void PushDeferScope() {
        defer_scopes_.push_back({});
    }

    void EmitDeferredCalls(const std::vector<DeferredCall>& calls) {
        for (auto it = calls.rbegin(); it != calls.rend(); ++it) {
            EmitDeferredCall(*it);
        }
    }

    void EmitDeferredScopesToDepth(std::size_t depth) {
        for (std::size_t index = defer_scopes_.size(); index > depth; --index) {
            EmitDeferredCalls(defer_scopes_[index - 1].calls);
        }
    }

    void PopDeferScope() {
        if (defer_scopes_.empty()) {
            return;
        }
        if (current_block_.has_value()) {
            EmitDeferredCalls(defer_scopes_.back().calls);
        }
        defer_scopes_.pop_back();
    }

    void LowerBlock(const Stmt& stmt) {
        PushDeferScope();
        LowerStmtList(stmt.statements);
        PopDeferScope();
    }

    void LowerScopedStmtList(const std::vector<std::unique_ptr<Stmt>>& statements) {
        PushDeferScope();
        LowerStmtList(statements);
        PopDeferScope();
    }

    void LowerStmt(const Stmt& stmt) {
        switch (stmt.kind) {
            case Stmt::Kind::kBlock:
                LowerBlock(stmt);
                return;
            case Stmt::Kind::kBinding:
            case Stmt::Kind::kVar:
                LowerBindingLike(stmt, true);
                return;
            case Stmt::Kind::kConst:
                LowerBindingLike(stmt, false);
                return;
            case Stmt::Kind::kBindingOrAssign: {
                const auto* fact = sema::FindBindingOrAssignFact(sema_module_, stmt);
                if (fact == nullptr) {
                    Report(stmt.span, "MIR lowering requires semantic classification for binding-or-assignment statement");
                    return;
                }
                if (fact->resolutions.size() != stmt.pattern.names.size()) {
                    Report(stmt.span, "MIR lowering requires one semantic resolution and one initializer per binding-or-assignment name");
                    return;
                }

                const auto values = LowerExprValuesForArity(stmt.exprs,
                                                            stmt.pattern.names.size(),
                                                            stmt.span,
                                                            "MIR lowering requires one semantic resolution and one initializer per binding-or-assignment name");
                if (!values.has_value()) {
                    return;
                }

                for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
                    if (fact->resolutions[index] == sema::BindingOrAssignResolution::kAssign) {
                        AssignNamedTarget(stmt.pattern.names[index], (*values)[index].value, stmt.span);
                        continue;
                    }
                    BindLocal(stmt.pattern.names[index],
                              (*values)[index].value.value,
                              (*values)[index].value.type,
                              true,
                              stmt.span);
                }
                return;
            }
            case Stmt::Kind::kAssign:
                LowerAssign(stmt);
                return;
            case Stmt::Kind::kExpr:
                if (!stmt.exprs.empty()) {
                    if (stmt.exprs.front()->kind == Expr::Kind::kCall) {
                        LowerCallStmt(*stmt.exprs.front());
                    } else {
                        static_cast<void>(LowerExpr(*stmt.exprs.front()));
                    }
                }
                return;
            case Stmt::Kind::kIf:
                LowerIf(stmt);
                return;
            case Stmt::Kind::kSwitch:
                LowerSwitch(stmt);
                return;
            case Stmt::Kind::kWhile:
            case Stmt::Kind::kForCondition:
                LowerWhileLike(stmt);
                return;
            case Stmt::Kind::kForIn: {
                const auto* fact = sema::FindForInFact(sema_module_, stmt);
                if (fact == nullptr) {
                    Report(stmt.span, "MIR lowering requires semantic classification for for-in statement");
                    return;
                }
                if (fact->resolution == sema::ForInResolution::kForRange) {
                    LowerForRange(stmt);
                } else {
                    LowerForEach(stmt);
                }
                return;
            }
            case Stmt::Kind::kForEach:
            case Stmt::Kind::kForEachIndex:
                LowerForEach(stmt);
                return;
            case Stmt::Kind::kForRange:
                LowerForRange(stmt);
                return;
            case Stmt::Kind::kReturn:
                LowerReturn(stmt);
                return;
            case Stmt::Kind::kBreak:
                LowerBreak(stmt);
                return;
            case Stmt::Kind::kContinue:
                LowerContinue(stmt);
                return;
            case Stmt::Kind::kDefer:
                LowerDefer(stmt);
                return;
        }
    }

    void LowerIf(const Stmt& stmt) {
        EnsureCurrentBlock();
        const auto condition = LowerExpr(*stmt.exprs.front());
        const std::size_t then_block = CreateBlock("if_then");
        const std::optional<std::size_t> else_block = stmt.else_branch != nullptr ? std::optional<std::size_t>(CreateBlock("if_else")) : std::nullopt;
        const std::size_t merge_block = CreateBlock("if_merge");

        auto& current = function_.blocks[*current_block_];
        current.terminator.kind = Terminator::Kind::kCondBranch;
        current.terminator.values = {condition.value};
        current.terminator.true_target = function_.blocks[then_block].label;
        current.terminator.false_target = else_block.has_value() ? function_.blocks[*else_block].label : function_.blocks[merge_block].label;

        current_block_ = then_block;
        LowerStmt(*stmt.then_branch);
        if (current_block_.has_value() && function_.blocks[*current_block_].terminator.kind == Terminator::Kind::kNone) {
            function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
            function_.blocks[*current_block_].terminator.true_target = function_.blocks[merge_block].label;
        }

        bool else_reaches_merge = false;
        if (else_block.has_value()) {
            current_block_ = *else_block;
            LowerStmt(*stmt.else_branch);
            if (current_block_.has_value() && function_.blocks[*current_block_].terminator.kind == Terminator::Kind::kNone) {
                function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
                function_.blocks[*current_block_].terminator.true_target = function_.blocks[merge_block].label;
                else_reaches_merge = true;
            }
        } else {
            else_reaches_merge = true;
        }

        const bool then_reaches_merge = function_.blocks[then_block].terminator.kind == Terminator::Kind::kBranch &&
                                        function_.blocks[then_block].terminator.true_target == function_.blocks[merge_block].label;
        if (then_reaches_merge || else_reaches_merge) {
            current_block_ = merge_block;
        } else {
            function_.blocks.erase(function_.blocks.begin() + static_cast<std::ptrdiff_t>(merge_block));
            current_block_ = std::nullopt;
        }
    }

    void LowerSwitch(const Stmt& stmt) {
        if (stmt.exprs.empty()) {
            Report(stmt.span, "switch statement is missing selector expression");
            return;
        }

        EnsureCurrentBlock();
        const sema::Type selector_type = ExprTypeOrUnknown(*stmt.exprs.front());
        const auto selector = LowerExpr(*stmt.exprs.front());
        const std::size_t merge_block = CreateBlock("switch_merge");
        const std::optional<std::size_t> default_block = stmt.has_default_case ? std::optional<std::size_t>(CreateBlock("switch_default")) : std::nullopt;
        bool reaches_merge = !stmt.has_default_case;

        std::vector<std::size_t> test_blocks;
        test_blocks.push_back(*current_block_);
        for (std::size_t index = 1; index < stmt.switch_cases.size(); ++index) {
            test_blocks.push_back(CreateBlock("switch_test"));
        }

        for (std::size_t index = 0; index < stmt.switch_cases.size(); ++index) {
            const auto& switch_case = stmt.switch_cases[index];

            current_block_ = test_blocks[index];
            ValueInfo comparison;
            if (switch_case.pattern.kind == mc::ast::CasePattern::Kind::kExpr) {
                if (switch_case.pattern.expr == nullptr) {
                    Report(switch_case.span, "switch expression case is missing a pattern expression");
                    return;
                }
                const auto case_value = LowerExpr(*switch_case.pattern.expr);
                comparison = EmitBinaryValue("==", selector, case_value, sema::BoolType());
            } else {
                comparison = EmitVariantMatchValue(selector, switch_case.pattern.variant_name);
            }

            const std::size_t body_block = CreateBlock("switch_case");
            const std::string false_target =
                index + 1 < test_blocks.size() ? function_.blocks[test_blocks[index + 1]].label
                                               : (default_block.has_value() ? function_.blocks[*default_block].label : function_.blocks[merge_block].label);

            auto& test = function_.blocks[*current_block_];
            test.terminator.kind = Terminator::Kind::kCondBranch;
            test.terminator.values = {comparison.value};
            test.terminator.true_target = function_.blocks[body_block].label;
            test.terminator.false_target = false_target;

            current_block_ = body_block;
            if (switch_case.pattern.kind == mc::ast::CasePattern::Kind::kVariant) {
                for (std::size_t binding_index = 0; binding_index < switch_case.pattern.bindings.size(); ++binding_index) {
                    const std::string& binding_name = switch_case.pattern.bindings[binding_index];
                    if (local_types_.contains(binding_name)) {
                        Report(switch_case.pattern.span, "bootstrap MIR variant binding would shadow an existing local");
                        return;
                    }
                    SnapshotValueToLocal(binding_name,
                                         EmitVariantExtractValue(selector,
                                                                 selector_type,
                                                                 switch_case.pattern.span,
                                                                 switch_case.pattern.variant_name,
                                                                 binding_index));
                }
            }
            LowerScopedStmtList(switch_case.statements);
            if (current_block_.has_value() && function_.blocks[*current_block_].terminator.kind == Terminator::Kind::kNone) {
                function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
                function_.blocks[*current_block_].terminator.true_target = function_.blocks[merge_block].label;
                reaches_merge = true;
            }
        }

        if (stmt.switch_cases.empty()) {
            auto& entry = function_.blocks[*current_block_];
            entry.terminator.kind = Terminator::Kind::kBranch;
            entry.terminator.true_target = default_block.has_value() ? function_.blocks[*default_block].label : function_.blocks[merge_block].label;
        }

        if (default_block.has_value()) {
            current_block_ = *default_block;
            LowerScopedStmtList(stmt.default_case.statements);
            if (current_block_.has_value() && function_.blocks[*current_block_].terminator.kind == Terminator::Kind::kNone) {
                function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
                function_.blocks[*current_block_].terminator.true_target = function_.blocks[merge_block].label;
                reaches_merge = true;
            }
        }

        if (reaches_merge) {
            current_block_ = merge_block;
        } else {
            function_.blocks.erase(function_.blocks.begin() + static_cast<std::ptrdiff_t>(merge_block));
            current_block_ = std::nullopt;
        }
    }

    void LowerWhileLike(const Stmt& stmt) {
        EnsureCurrentBlock();
        const std::size_t cond_block = CreateBlock("loop_cond");
        const std::size_t body_block = CreateBlock("loop_body");
        const std::size_t exit_block = CreateBlock("loop_exit");

        auto& incoming = function_.blocks[*current_block_];
        incoming.terminator.kind = Terminator::Kind::kBranch;
        incoming.terminator.true_target = function_.blocks[cond_block].label;

        current_block_ = cond_block;
        const auto condition = LowerExpr(*stmt.exprs.front());
        auto& cond = function_.blocks[*current_block_];
        cond.terminator.kind = Terminator::Kind::kCondBranch;
        cond.terminator.values = {condition.value};
        cond.terminator.true_target = function_.blocks[body_block].label;
        cond.terminator.false_target = function_.blocks[exit_block].label;

        loop_stack_.push_back({
            .break_target = function_.blocks[exit_block].label,
            .continue_target = function_.blocks[cond_block].label,
            .unwind_scope_depth = defer_scopes_.size(),
        });

        current_block_ = body_block;
        LowerStmt(*stmt.then_branch);
        if (current_block_.has_value() && function_.blocks[*current_block_].terminator.kind == Terminator::Kind::kNone) {
            function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
            function_.blocks[*current_block_].terminator.true_target = function_.blocks[cond_block].label;
        }

        loop_stack_.pop_back();
        current_block_ = exit_block;
    }

    void LowerForRange(const Stmt& stmt) {
        if (stmt.exprs.empty() || stmt.exprs.front()->kind != Expr::Kind::kRange || stmt.exprs.front()->left == nullptr || stmt.exprs.front()->right == nullptr) {
            Report(stmt.span, "not yet supported in MIR: for-range without explicit lower and upper bounds");
            return;
        }

        const sema::Type loop_type = RangeElementType(ExprTypeOrUnknown(*stmt.exprs.front()));
        if (sema::IsUnknown(loop_type)) {
            Report(stmt.span, "MIR lowering requires semantic range element type for for-range loop");
            return;
        }

        ValueInfo start = LowerExpr(*stmt.exprs.front()->left);
        ValueInfo end = LowerExpr(*stmt.exprs.front()->right);
        if (start.type != loop_type) {
            start = EmitConvertValue(start, loop_type);
        }
        if (end.type != loop_type) {
            end = EmitConvertValue(end, loop_type);
        }
        StoreLocal(stmt.loop_name, start.value, loop_type, true);

        EnsureCurrentBlock();
        const std::size_t cond_block = CreateBlock("range_cond");
        const std::size_t body_block = CreateBlock("range_body");
        const std::size_t step_block = CreateBlock("range_step");
        const std::size_t exit_block = CreateBlock("range_exit");

        auto& incoming = function_.blocks[*current_block_];
        incoming.terminator.kind = Terminator::Kind::kBranch;
        incoming.terminator.true_target = function_.blocks[cond_block].label;

        current_block_ = cond_block;
        const auto current_value = LoadLocalValue(stmt.loop_name);
        const auto condition = EmitBinaryValue("<", current_value, end, sema::BoolType());
        auto& cond = function_.blocks[*current_block_];
        cond.terminator.kind = Terminator::Kind::kCondBranch;
        cond.terminator.values = {condition.value};
        cond.terminator.true_target = function_.blocks[body_block].label;
        cond.terminator.false_target = function_.blocks[exit_block].label;

        loop_stack_.push_back({
            .break_target = function_.blocks[exit_block].label,
            .continue_target = function_.blocks[step_block].label,
            .unwind_scope_depth = defer_scopes_.size(),
        });

        current_block_ = body_block;
        LowerStmt(*stmt.then_branch);
        if (current_block_.has_value() && function_.blocks[*current_block_].terminator.kind == Terminator::Kind::kNone) {
            function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
            function_.blocks[*current_block_].terminator.true_target = function_.blocks[step_block].label;
        }

        current_block_ = step_block;
        const auto current_index = LoadLocalValue(stmt.loop_name);
        const auto one = EmitLiteralValue("1", loop_type);
        const auto next = EmitBinaryValue("+", current_index, one, loop_type);
        StoreLocal(stmt.loop_name, next.value, loop_type, true);
        function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
        function_.blocks[*current_block_].terminator.true_target = function_.blocks[cond_block].label;

        loop_stack_.pop_back();
        current_block_ = exit_block;
    }

    void LowerForEach(const Stmt& stmt) {
        if (stmt.exprs.empty()) {
            Report(stmt.span, "foreach statement is missing iterable expression");
            return;
        }

        const sema::Type iterable_type = ExprTypeOrUnknown(*stmt.exprs.front());
        const auto iterable = LowerExpr(*stmt.exprs.front());
        const sema::Type element_type = IterableElementType(iterable_type);
        if (sema::IsUnknown(element_type)) {
            Report(stmt.span, "MIR lowering requires semantic iterable element type for foreach loop");
            return;
        }
        const std::string iterable_name = NewHiddenLocal("iterable");
        const std::string index_name = NewHiddenLocal("index");
        StoreLocal(iterable_name, iterable.value, iterable_type, true);
        const auto zero = EmitLiteralValue("0", sema::NamedType("usize"));
        StoreLocal(index_name, zero.value, sema::NamedType("usize"), true);

        EnsureCurrentBlock();
        const std::size_t cond_block = CreateBlock("foreach_cond");
        const std::size_t body_block = CreateBlock("foreach_body");
        const std::size_t step_block = CreateBlock("foreach_step");
        const std::size_t exit_block = CreateBlock("foreach_exit");

        auto& incoming = function_.blocks[*current_block_];
        incoming.terminator.kind = Terminator::Kind::kBranch;
        incoming.terminator.true_target = function_.blocks[cond_block].label;

        current_block_ = cond_block;
        const auto current_index = LoadLocalValue(index_name);
        const auto stored_iterable = LoadLocalValue(iterable_name);
        const std::string len_value = NewValue();
        Emit({
            .kind = Instruction::Kind::kField,
            .result = len_value,
            .type = sema::NamedType("usize"),
            .target = "len",
            .target_kind = Instruction::TargetKind::kField,
            .target_name = "len",
            .target_display = stmt.exprs.front() != nullptr ? RenderExprInline(*stmt.exprs.front()) + ".len" : std::string("len"),
            .target_base_type = iterable_type,
            .operands = {stored_iterable.value},
        });
        const auto condition = EmitBinaryValue("<", current_index, {len_value, sema::NamedType("usize")}, sema::BoolType());
        auto& cond = function_.blocks[*current_block_];
        cond.terminator.kind = Terminator::Kind::kCondBranch;
        cond.terminator.values = {condition.value};
        cond.terminator.true_target = function_.blocks[body_block].label;
        cond.terminator.false_target = function_.blocks[exit_block].label;

        loop_stack_.push_back({
            .break_target = function_.blocks[exit_block].label,
            .continue_target = function_.blocks[step_block].label,
            .unwind_scope_depth = defer_scopes_.size(),
        });

        current_block_ = body_block;
        const auto body_iterable = LoadLocalValue(iterable_name);
        const auto body_index = LoadLocalValue(index_name);
        EmitIndexBoundsCheck(body_iterable, body_index);
        const std::string item_value = NewValue();
        Emit({
            .kind = Instruction::Kind::kIndex,
            .result = item_value,
            .type = element_type,
            .target_kind = Instruction::TargetKind::kIndex,
            .target_display = stmt.exprs.front() != nullptr && !stmt.loop_name.empty()
                                  ? RenderExprInline(*stmt.exprs.front()) + "[" + stmt.loop_name + "]"
                                  : std::string(),
            .target_base_type = iterable_type,
            .target_aux_types = {sema::NamedType("usize")},
            .operands = {body_iterable.value, body_index.value},
        });
        if (stmt.kind == Stmt::Kind::kForEachIndex) {
            StoreLocal(stmt.loop_name, body_index.value, sema::NamedType("usize"), true);
            StoreLocal(stmt.loop_second_name, item_value, element_type, true);
        } else {
            StoreLocal(stmt.loop_name, item_value, element_type, true);
        }
        LowerStmt(*stmt.then_branch);
        if (current_block_.has_value() && function_.blocks[*current_block_].terminator.kind == Terminator::Kind::kNone) {
            function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
            function_.blocks[*current_block_].terminator.true_target = function_.blocks[step_block].label;
        }

        current_block_ = step_block;
        const auto step_index = LoadLocalValue(index_name);
        const auto one = EmitLiteralValue("1", sema::NamedType("usize"));
        const auto next = EmitBinaryValue("+", step_index, one, sema::NamedType("usize"));
        StoreLocal(index_name, next.value, sema::NamedType("usize"), true);
        function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
        function_.blocks[*current_block_].terminator.true_target = function_.blocks[cond_block].label;

        loop_stack_.pop_back();
        current_block_ = exit_block;
    }

    void LowerDefer(const Stmt& stmt) {
        if (stmt.exprs.empty()) {
            Report(stmt.span, "defer statement is missing expression");
            return;
        }
        const Expr& expr = *stmt.exprs.front();
        if (expr.kind != Expr::Kind::kCall || expr.left == nullptr) {
            Report(stmt.span, "not yet supported in MIR: defer of " + std::string(ToString(expr.kind)) + " expression");
            return;
        }
        if (defer_scopes_.empty()) {
            PushDeferScope();
        }
        defer_scopes_.back().calls.push_back(CaptureDeferredCall(expr));
    }

    void LowerReturn(const Stmt& stmt) {
        EnsureCurrentBlock();
        std::vector<std::string> return_values;
        const auto values = LowerExprValuesForArity(stmt.exprs,
                                                    function_.return_types.size(),
                                                    stmt.span,
                                                    "bootstrap MIR return value count does not match function signature");
        if (!values.has_value()) {
            return;
        }
        return_values.reserve(values->size());
        for (const auto& value : *values) {
            return_values.push_back(value.value.value);
        }
        EmitDeferredScopesToDepth(0);
        auto& block = function_.blocks[*current_block_];
        block.terminator.kind = Terminator::Kind::kReturn;
        block.terminator.values = std::move(return_values);
        current_block_ = std::nullopt;
    }

    void LowerBreak(const Stmt& stmt) {
        EnsureCurrentBlock();
        if (loop_stack_.empty()) {
            Report(stmt.span, "break used outside of a loop");
            return;
        }
        EmitDeferredScopesToDepth(loop_stack_.back().unwind_scope_depth);
        auto& block = function_.blocks[*current_block_];
        block.terminator.kind = Terminator::Kind::kBranch;
        block.terminator.true_target = loop_stack_.back().break_target;
        current_block_ = std::nullopt;
    }

    void LowerContinue(const Stmt& stmt) {
        EnsureCurrentBlock();
        if (loop_stack_.empty()) {
            Report(stmt.span, "continue used outside of a loop");
            return;
        }
        EmitDeferredScopesToDepth(loop_stack_.back().unwind_scope_depth);
        auto& block = function_.blocks[*current_block_];
        block.terminator.kind = Terminator::Kind::kBranch;
        block.terminator.true_target = loop_stack_.back().continue_target;
        current_block_ = std::nullopt;
    }