#include "compiler/mir/mir_internal.h"

#include <algorithm>
#include <cassert>
#include <unordered_set>

#include "compiler/support/assert.h"

namespace mc::mir {

using mc::ast::Decl;
using mc::ast::Expr;
using mc::ast::Stmt;
using mc::support::DiagnosticSeverity;


// FunctionLowerer lowers a single sema-checked function declaration into a
// mir::Function by walking the AST and emitting MIR instructions into basic
// blocks.
//
// Call sequence:
//   FunctionLowerer lowerer(decl, module, file_path, sema_module, diagnostics);
//   Function result = lowerer.Run();
//
// Invariants:
//   - After construction, function_.name and parameter locals are filled in.
//   - EnsureCurrentBlock() must be called before any Emit() that is not
//     allowed to be in a dead block.  Dead-block instructions are lowered
//     into the current block; the reachability pass in ValidateModule will
//     surface them.
//   - DeferScope stack: scopes are pushed by PushDeferScope and must be
//     popped via PopDeferScope before any non-fallthrough control transfer.
//     A scope pushed in a loop body is popped on break/continue/return.
//
// After Run() the FunctionLowerer is consumed; no further methods may be
// called on it.
class FunctionLowerer {
  public:
    FunctionLowerer(const Decl& decl,
                    const Module& module,
                    const std::filesystem::path& file_path,
                    const sema::Module& sema_module,
                    support::DiagnosticSink& diagnostics)
        : decl_(decl), module_(module), file_path_(file_path), sema_module_(sema_module), diagnostics_(diagnostics) {
        function_.name = decl.name;
        function_.type_params = decl.type_params;
        // Prefer sema-computed return types over re-deriving from AST TypeExprs.
        // TypeFromAst is a fallback for extern functions absent from the module.
        const sema::FunctionSignature* signature = sema::FindFunctionSignature(sema_module_, decl.name);
        if (signature != nullptr && !signature->return_types.empty()) {
            function_.return_types = signature->return_types;
        } else {
            function_.return_types.reserve(decl.return_types.size());
            for (const auto& return_type : decl.return_types) {
                function_.return_types.push_back(sema::TypeFromAst(return_type.get()));
            }
        }
    }

    Function Run() {
        assert(decl_.kind == Decl::Kind::kFunc && "FunctionLowerer only lowers non-extern function declarations");
        assert(decl_.body != nullptr && "FunctionLowerer expects sema-checked function bodies");

        const sema::FunctionSignature* signature = sema::FindFunctionSignature(sema_module_, decl_.name);
        for (std::size_t index = 0; index < decl_.params.size(); ++index) {
            const sema::Type type =
                (signature != nullptr && index < signature->param_types.size())
                    ? signature->param_types[index]
                    : sema::TypeFromAst(decl_.params[index].type.get());
            function_.locals.push_back({
                .name = decl_.params[index].name,
                .type = type,
                .is_parameter = true,
                .is_noalias = signature != nullptr && index < signature->param_is_noalias.size() && signature->param_is_noalias[index],
                .is_mutable = false,
            });
            local_types_[decl_.params[index].name] = type;
        }

        current_block_ = CreateBlock("entry");
        LowerStmt(*decl_.body);

        if (current_block_.has_value() && function_.blocks[*current_block_].terminator.kind == Terminator::Kind::kNone) {
            if (function_.return_types.empty()) {
                function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kReturn;
            } else {
                Report(decl_.span, "missing explicit return before end of function body");
            }
        }

        return std::move(function_);
    }

  private:
    struct LoopTarget {
        std::string break_target;
        std::string continue_target;
        std::size_t unwind_scope_depth = 0;
    };

    struct TargetMetadata {
        std::string target;
        Instruction::TargetKind kind = Instruction::TargetKind::kNone;
        std::string name;
        std::string display;
    };

    struct DeferredCall {
        TargetMetadata target;
        SpecialCallKind special_kind = SpecialCallKind::kNone;
        sema::Type type;
        // Atomic order fields are only populated for deferred atomic intrinsics.
        std::string atomic_order;
        std::string atomic_success_order;
        std::string atomic_failure_order;
        // Only generic deferred calls snapshot a callee local.
        std::string callee_local;
        std::vector<std::string> arg_locals;
    };

    struct DeferScope {
        std::vector<DeferredCall> calls;
    };

    std::size_t CreateBlock(const std::string& prefix) {
        const std::string label = prefix + std::to_string(next_block_id_++);
        function_.blocks.push_back({.label = label});
        return function_.blocks.size() - 1;
    }

    // Lowering keeps statements after an unconditional terminator by switching into a
    // fresh dead block. ValidateModule will later report unreachable blocks.
    void EnsureCurrentBlock() {
        if (!current_block_.has_value()) {
            current_block_ = CreateBlock("dead");
        }
    }

    std::string NewValue() {
        return "%v" + std::to_string(next_value_id_++);
    }

    // Hidden lowering temporaries use a sigil outside the source identifier grammar
    // so they cannot collide with user-visible local names.
    std::string NewHiddenLocal(const std::string& prefix) {
        return "%hidden." + prefix + std::to_string(next_hidden_local_id_++);
    }

    std::vector<sema::Type> ExpandReturnTypes(const sema::Type& type) const {
        if (type.kind == sema::Type::Kind::kVoid) {
            return {};
        }
        if (type.kind == sema::Type::Kind::kTuple) {
            return type.subtypes;
        }
        return {type};
    }

    sema::Type ExprTypeOrUnknown(const Expr& expr) const {
        if (const sema::Type* type = sema::FindExprType(sema_module_, expr); type != nullptr) {
            return *type;
        }
        return sema::UnknownType();
    }

    std::string CalleeName(const Expr& expr) const {
        if (expr.kind == Expr::Kind::kName) {
            return expr.text;
        }
        if (expr.kind == Expr::Kind::kQualifiedName) {
            return CombineQualifiedName(expr);
        }
        return {};
    }

    bool IsImportedModuleName(std::string_view name) const {
        return std::find(sema_module_.imports.begin(), sema_module_.imports.end(), name) != sema_module_.imports.end();
    }

    Instruction::TargetKind InferTargetKindForExpr(const Expr& expr) const {
        const std::string canonical_name = CalleeName(expr);
        if (canonical_name.empty()) {
            return Instruction::TargetKind::kNone;
        }

        const Instruction::TargetKind existing_kind = InferSymbolRefKind(canonical_name);
        if (existing_kind != Instruction::TargetKind::kOther) {
            return existing_kind;
        }

        if (expr.kind == Expr::Kind::kQualifiedName && IsImportedModuleName(expr.text)) {
            const sema::Type type = ExprTypeOrUnknown(expr);
            if (type.kind == sema::Type::Kind::kProcedure) {
                return Instruction::TargetKind::kFunction;
            }
            if (!sema::IsUnknown(type)) {
                return Instruction::TargetKind::kGlobal;
            }
        }

        return Instruction::TargetKind::kOther;
    }

    TargetMetadata TargetMetadataForExpr(const Expr& expr) const {
        const std::string canonical_name = CalleeName(expr);
        if (!canonical_name.empty()) {
            return {
                .target = canonical_name,
                .kind = InferTargetKindForExpr(expr),
                .name = canonical_name,
                .display = canonical_name,
            };
        }
        const std::string display = RenderExprInline(expr);
        return {
            .target = display,
            .kind = Instruction::TargetKind::kNone,
            .name = {},
            .display = display,
        };
    }

    struct StoreBaseMetadata {
        Instruction::StorageBaseKind kind = Instruction::StorageBaseKind::kNone;
        std::string name;
    };

    StoreBaseMetadata StoreBaseMetadataForExpr(const Expr& expr) const {
        if (!expr.text.empty() && local_types_.contains(expr.text)) {
            return {
                .kind = Instruction::StorageBaseKind::kLocal,
                .name = expr.text,
            };
        }

        const std::string canonical_name = CalleeName(expr);
        if (!canonical_name.empty() && InferTargetKindForExpr(expr) == Instruction::TargetKind::kGlobal) {
            return {
                .kind = Instruction::StorageBaseKind::kGlobal,
                .name = canonical_name,
            };
        }

        return {};
    }

    std::string StableExprMetadataText(const Expr& expr) const {
        const TargetMetadata metadata = TargetMetadataForExpr(expr);
        return !metadata.name.empty() ? metadata.name : metadata.display;
    }

    std::string RenderOrderName(const Expr& expr, std::size_t order_index) const {
        if (expr.args.size() <= order_index) {
            return {};
        }
        return StableExprMetadataText(*expr.args[order_index]);
    }

    std::string RenderCompareExchangeSuccessOrder(const Expr& expr) const {
        if (expr.args.size() <= 4) {
            return {};
        }
        return StableExprMetadataText(*expr.args[3]);
    }

    std::string RenderCompareExchangeFailureOrder(const Expr& expr) const {
        if (expr.args.size() <= 4) {
            return {};
        }
        return StableExprMetadataText(*expr.args[4]);
    }

    bool SpecialCallProducesValue(SpecialCallKind kind) const {
        return kind == SpecialCallKind::kMmioPtr || kind == SpecialCallKind::kBufferNew || kind == SpecialCallKind::kArenaNew ||
               kind == SpecialCallKind::kSliceFromBuffer || kind == SpecialCallKind::kVolatileLoad ||
               kind == SpecialCallKind::kAtomicLoad || kind == SpecialCallKind::kAtomicExchange ||
               kind == SpecialCallKind::kAtomicCompareExchange || kind == SpecialCallKind::kAtomicFetchAdd;
    }

    std::string SpecialCallAtomicOrder(const Expr& expr, SpecialCallKind kind) const {
        switch (kind) {
            case SpecialCallKind::kAtomicLoad:
                return RenderOrderName(expr, 1);
            case SpecialCallKind::kAtomicStore:
            case SpecialCallKind::kAtomicExchange:
            case SpecialCallKind::kAtomicFetchAdd:
                return RenderOrderName(expr, 2);
            case SpecialCallKind::kBufferNew:
            case SpecialCallKind::kBufferFree:
            case SpecialCallKind::kArenaNew:
            case SpecialCallKind::kSliceFromBuffer:
            case SpecialCallKind::kAtomicCompareExchange:
            case SpecialCallKind::kNone:
            case SpecialCallKind::kMmioPtr:
            case SpecialCallKind::kVolatileLoad:
            case SpecialCallKind::kVolatileStore:
                return {};
        }

        return {};
    }

    std::string SpecialCallAtomicSuccessOrder(const Expr& expr, SpecialCallKind kind) const {
        if (kind != SpecialCallKind::kAtomicCompareExchange) {
            return {};
        }
        return RenderCompareExchangeSuccessOrder(expr);
    }

    std::string SpecialCallAtomicFailureOrder(const Expr& expr, SpecialCallKind kind) const {
        if (kind != SpecialCallKind::kAtomicCompareExchange) {
            return {};
        }
        return RenderCompareExchangeFailureOrder(expr);
    }

    bool EmitSpecialCallInstruction(const TargetMetadata& target_metadata,
                                    SpecialCallKind kind,
                                    const std::vector<ValueInfo>& args,
                                    const sema::Type& result_type,
                                    const std::string& atomic_order,
                                    const std::string& atomic_success_order,
                                    const std::string& atomic_failure_order,
                                    bool wants_result,
                                    ValueInfo* result) {
        if (kind == SpecialCallKind::kNone) {
            return false;
        }

        if (kind == SpecialCallKind::kArenaNew) {
            if (args.size() != 1) {
                Report({}, "arena_new expects exactly one argument during MIR lowering");
                return false;
            }
            const std::string value = NewValue();
            Emit({
                .kind = Instruction::Kind::kArenaNew,
                .result = value,
                .type = result_type,
                .target = sema::FormatType(result_type),
                .target_display = sema::FormatType(result_type),
                .operands = {args.front().value},
            });
            if (wants_result && result != nullptr) {
                *result = {value, result_type};
            }
            return true;
        }

        if (kind == SpecialCallKind::kMmioPtr) {
            const std::string value = NewValue();
            std::vector<std::string> operands;
            if (!args.empty()) {
                operands.push_back(args.front().value);
            }
            Emit({
                .kind = Instruction::Kind::kIntToPointer,
                .result = value,
                .type = result_type,
                .target = sema::FormatType(result_type),
                .target_display = sema::FormatType(result_type),
                .operands = std::move(operands),
            });
            if (wants_result && result != nullptr) {
                *result = {value, result_type};
            }
            return true;
        }

        if (kind == SpecialCallKind::kSliceFromBuffer) {
            if (args.size() != 1) {
                Report({}, "slice_from_buffer expects exactly one argument during MIR lowering");
                return false;
            }
            const sema::Type pointed = PointerPointeeType(args.front().type).value_or(sema::UnknownType());
            const std::string value = NewValue();
            Emit({
                .kind = Instruction::Kind::kUnary,
                .result = value,
                .type = pointed,
                .op = "*",
                .operands = {args.front().value},
            });
            const std::string result_value = NewValue();
            Emit({
                .kind = Instruction::Kind::kBufferToSlice,
                .result = result_value,
                .type = result_type,
                .target = sema::FormatType(result_type),
                .target_display = sema::FormatType(result_type),
                .operands = {value},
            });
            if (wants_result && result != nullptr) {
                *result = {result_value, result_type};
            }
            return true;
        }

        Instruction instruction {
            .type = result_type,
            .atomic_order = atomic_order,
            .atomic_success_order = atomic_success_order,
            .atomic_failure_order = atomic_failure_order,
            .target = target_metadata.target,
            .target_kind = target_metadata.kind,
            .target_name = target_metadata.name,
            .target_display = target_metadata.display,
        };
        if (wants_result || SpecialCallProducesValue(kind)) {
            instruction.result = NewValue();
        }
        for (const auto& arg : args) {
            instruction.operands.push_back(arg.value);
        }

        switch (kind) {
            case SpecialCallKind::kBufferNew:
                instruction.kind = Instruction::Kind::kBufferNew;
                break;
            case SpecialCallKind::kBufferFree:
                instruction.kind = Instruction::Kind::kBufferFree;
                break;
            case SpecialCallKind::kVolatileLoad:
                instruction.kind = Instruction::Kind::kVolatileLoad;
                break;
            case SpecialCallKind::kVolatileStore:
                instruction.kind = Instruction::Kind::kVolatileStore;
                break;
            case SpecialCallKind::kAtomicLoad:
                instruction.kind = Instruction::Kind::kAtomicLoad;
                break;
            case SpecialCallKind::kAtomicStore:
                instruction.kind = Instruction::Kind::kAtomicStore;
                break;
            case SpecialCallKind::kAtomicExchange:
                instruction.kind = Instruction::Kind::kAtomicExchange;
                break;
            case SpecialCallKind::kAtomicCompareExchange:
                instruction.kind = Instruction::Kind::kAtomicCompareExchange;
                break;
            case SpecialCallKind::kAtomicFetchAdd:
                instruction.kind = Instruction::Kind::kAtomicFetchAdd;
                break;
            default:
                // kArenaNew, kMmioPtr, kSliceFromBuffer, and kNone are handled
                // before the switch above and cannot reach this point.
                MC_UNREACHABLE("unhandled SpecialCallKind in EmitSpecialCallInstruction");
        }

        Emit(std::move(instruction));
        if (wants_result && result != nullptr) {
            *result = {function_.blocks[*current_block_].instructions.back().result, result_type};
        }
        return true;
    }

    bool TryEmitSpecialCall(const Expr& expr,
                           const std::vector<ValueInfo>& args,
                           const sema::Type& result_type,
                           bool wants_result,
                           ValueInfo* result) {
        if (expr.left == nullptr) {
            return false;
        }

        const std::string callee_name = CalleeName(*expr.left);
        const SpecialCallKind kind = ClassifySpecialCall(callee_name);
        if (kind == SpecialCallKind::kNone) {
            return false;
        }

        const TargetMetadata target_metadata = TargetMetadataForExpr(*expr.left);
        return EmitSpecialCallInstruction(
            target_metadata,
            kind,
            args,
            result_type,
            SpecialCallAtomicOrder(expr, kind),
            SpecialCallAtomicSuccessOrder(expr, kind),
            SpecialCallAtomicFailureOrder(expr, kind),
            wants_result,
            result);
    }

    void LowerCallStmt(const Expr& expr) {
        if (expr.left == nullptr) {
            Report(expr.span, "call expression is missing callee");
            return;
        }
        if (IsExplicitConversionExpr(expr)) {
            static_cast<void>(LowerExpr(expr));
            return;
        }

        std::vector<ValueInfo> args;
        args.reserve(expr.args.size());
        for (const auto& arg : expr.args) {
            args.push_back(LowerExpr(*arg));
        }

        const sema::Type call_type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "statement call result type");
        if (TryEmitVariantInit(expr, args, call_type, false, nullptr)) {
            return;
        }
        if (TryEmitSpecialCall(expr, args, call_type, false, nullptr)) {
            return;
        }
        const auto callee = LowerCalleeExpr(*expr.left);

        std::vector<std::string> operands = {callee.value};
        for (const auto& arg : args) {
            operands.push_back(arg.value);
        }
        const TargetMetadata target_metadata = TargetMetadataForExpr(*expr.left);

        Emit({
            .kind = Instruction::Kind::kCall,
            .type = call_type,
            .target = target_metadata.target,
            .target_kind = target_metadata.kind,
            .target_name = target_metadata.name,
            .target_display = target_metadata.display,
            .operands = std::move(operands),
        });
    }

    ValueInfo LowerCalleeExpr(const Expr& expr) {
        if (expr.kind == Expr::Kind::kName) {
            const std::string value = NewValue();
            const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "callee type for " + expr.text);
            Emit({
                .kind = Instruction::Kind::kSymbolRef,
                .result = value,
                .type = type,
                .target = expr.text,
                .target_kind = InferSymbolRefKind(expr.text),
                .target_name = expr.text,
                .target_display = expr.text,
            });
            return {value, type};
        }
        if (expr.kind == Expr::Kind::kQualifiedName) {
            const std::string qualified_name = CombineQualifiedName(expr);
            const std::string value = NewValue();
            const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "callee type for " + qualified_name);
            Emit({
                .kind = Instruction::Kind::kSymbolRef,
                .result = value,
                .type = type,
                .target = qualified_name,
                .target_kind = InferTargetKindForExpr(expr),
                .target_name = qualified_name,
                .target_display = qualified_name,
            });
            return {value, type};
        }
        return LowerExpr(expr);
    }

    void SnapshotValueToLocal(const std::string& local_name, const ValueInfo& value) {
        StoreLocal(local_name, value.value, value.type, false);
    }

    DeferredCall CaptureDeferredCall(const Expr& expr) {
        std::vector<ValueInfo> args;
        args.reserve(expr.args.size());
        for (const auto& arg : expr.args) {
            args.push_back(LowerExpr(*arg));
        }
        const SpecialCallKind special_kind = ClassifySpecialCall(CalleeName(*expr.left));
        const auto callee = special_kind == SpecialCallKind::kNone ? LowerCalleeExpr(*expr.left) : ValueInfo {};
        const TargetMetadata target_metadata = TargetMetadataForExpr(*expr.left);
        DeferredCall call {
            .target = target_metadata,
            .special_kind = special_kind,
            .type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "deferred call result type"),
            .atomic_order = SpecialCallAtomicOrder(expr, special_kind),
            .atomic_success_order = SpecialCallAtomicSuccessOrder(expr, special_kind),
            .atomic_failure_order = SpecialCallAtomicFailureOrder(expr, special_kind),
        };
        if (special_kind == SpecialCallKind::kNone) {
            call.callee_local = NewHiddenLocal("defer_callee");
            SnapshotValueToLocal(call.callee_local, callee);
        }
        for (const auto& value : args) {
            const std::string arg_local = NewHiddenLocal("defer_arg");
            SnapshotValueToLocal(arg_local, value);
            call.arg_locals.push_back(arg_local);
        }
        return call;
    }

    void EmitDeferredCall(const DeferredCall& call) {
        if (call.special_kind != SpecialCallKind::kNone) {
            std::vector<ValueInfo> args;
            args.reserve(call.arg_locals.size());
            for (const auto& arg_local : call.arg_locals) {
                args.push_back(LoadLocalValue(arg_local));
            }
            static_cast<void>(EmitSpecialCallInstruction(call.target,
                                                         call.special_kind,
                                                         args,
                                                         call.type,
                                                         call.atomic_order,
                                                         call.atomic_success_order,
                                                         call.atomic_failure_order,
                                                         false,
                                                         nullptr));
            return;
        }

        const auto callee = LoadLocalValue(call.callee_local);
        std::vector<std::string> operands = {callee.value};
        for (const auto& arg_local : call.arg_locals) {
            operands.push_back(LoadLocalValue(arg_local).value);
        }
        Emit({
            .kind = Instruction::Kind::kCall,
            .type = call.type,
            .target = call.target.target,
            .target_kind = call.target.kind,
            .target_name = call.target.name,
            .target_display = call.target.display,
            .operands = std::move(operands),
        });
    }

    void Emit(Instruction instruction) {
        EnsureCurrentBlock();
        function_.blocks[*current_block_].instructions.push_back(std::move(instruction));
    }

    void Report(const mc::support::SourceSpan& span, const std::string& message) {
        diagnostics_.Report({
            .file_path = file_path_,
            .span = span,
            .severity = DiagnosticSeverity::kError,
            .message = message,
        });
    }

    sema::Type KnownTypeOrError(const sema::Type& type, const mc::support::SourceSpan& span, const std::string& context) {
        if (!sema::IsUnknown(type)) {
            return type;
        }
        Report(span, "MIR lowering requires semantic type information for " + context);
        return sema::VoidType();
    }

    sema::Type EnsureLocal(const std::string& name, const sema::Type& suggested_type, bool is_mutable) {
        const sema::Type local_type = KnownTypeOrError(suggested_type, decl_.span, "local type for " + name);
        const auto found = local_types_.find(name);
        if (found != local_types_.end()) {
            if (sema::IsUnknown(found->second)) {
                found->second = local_type;
                for (auto& local : function_.locals) {
                    if (local.name == name) {
                        local.type = local_type;
                        local.is_mutable = is_mutable;
                        break;
                    }
                }
            } else {
                assert((sema::IsUnknown(local_type) || found->second == local_type) &&
                       "EnsureLocal: type conflict for previously-bound local");
            }
            return found->second;
        }

        function_.locals.push_back({
            .name = name,
            .type = local_type,
            .is_parameter = false,
            .is_mutable = is_mutable,
        });
        local_types_[name] = local_type;
        return local_type;
    }

    ValueInfo EmitLiteralValue(const std::string& text, const sema::Type& type) {
        const std::string value = NewValue();
        Emit({
            .kind = Instruction::Kind::kConst,
            .result = value,
            .type = type,
            .op = text,
        });
        return {value, type};
    }

    void EmitDivideCheck(const std::string& op, const ValueInfo& left, const ValueInfo& right) {
        Emit({
            .kind = Instruction::Kind::kDivCheck,
            .op = op,
            .operands = {left.value, right.value},
        });
    }

    void EmitShiftCheck(const std::string& op, const ValueInfo& left, const ValueInfo& right) {
        Emit({
            .kind = Instruction::Kind::kShiftCheck,
            .op = op,
            .operands = {left.value, right.value},
        });
    }

    ValueInfo EmitBinaryValue(const std::string& op, const ValueInfo& left, const ValueInfo& right, const sema::Type& type) {
        if (op == "/" || op == "%") {
            EmitDivideCheck(op, left, right);
        }
        if (op == "<<" || op == ">>") {
            EmitShiftCheck(op, left, right);
        }
        const std::string value = NewValue();
        Emit({
            .kind = Instruction::Kind::kBinary,
            .result = value,
            .type = type,
            .op = op,
            .operands = {left.value, right.value},
            .arithmetic_semantics = ClassifyBinaryArithmeticSemantics(module_, op, type),
        });
        return {value, type};
    }

    ValueInfo LowerShortCircuitLogicalExpr(const Expr& expr) {
        EnsureCurrentBlock();

        const sema::Type result_type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "logical expression type");
        const std::string result_local = NewHiddenLocal(expr.text == "&&" ? "logical_and" : "logical_or");
        EnsureLocal(result_local, result_type, true);

        const ValueInfo left = LowerExpr(*expr.left);
        const std::size_t rhs_block = CreateBlock(expr.text == "&&" ? "logical_and_rhs" : "logical_or_rhs");
        const std::size_t short_block = CreateBlock(expr.text == "&&" ? "logical_and_short" : "logical_or_short");
        const std::size_t merge_block = CreateBlock(expr.text == "&&" ? "logical_and_merge" : "logical_or_merge");

        auto& entry = function_.blocks[*current_block_];
        entry.terminator.kind = Terminator::Kind::kCondBranch;
        entry.terminator.values = {left.value};
        if (expr.text == "&&") {
            entry.terminator.true_target = function_.blocks[rhs_block].label;
            entry.terminator.false_target = function_.blocks[short_block].label;
        } else {
            entry.terminator.true_target = function_.blocks[short_block].label;
            entry.terminator.false_target = function_.blocks[rhs_block].label;
        }

        current_block_ = short_block;
        const ValueInfo short_value = EmitLiteralValue(expr.text == "&&" ? "false" : "true", result_type);
        StoreLocal(result_local, short_value.value, result_type, true);
        function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
        function_.blocks[*current_block_].terminator.true_target = function_.blocks[merge_block].label;

        current_block_ = rhs_block;
        const ValueInfo right = LowerExpr(*expr.right);
        StoreLocal(result_local, right.value, result_type, true);
        function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
        function_.blocks[*current_block_].terminator.true_target = function_.blocks[merge_block].label;

        current_block_ = merge_block;
        return LoadLocalValue(result_local);
    }

    std::optional<ValueInfo> EmitBoundsLengthValue(const ValueInfo& base) {
        const sema::Type stripped_base = StripMirAliasOrDistinct(module_, base.type);
        if (stripped_base.kind == sema::Type::Kind::kArray) {
            const auto length = mc::support::ParseArrayLength(stripped_base.metadata);
            if (!length.has_value()) {
                return std::nullopt;
            }
            return EmitLiteralValue(std::to_string(*length), sema::NamedType("usize"));
        }
           if ((stripped_base.kind == sema::Type::Kind::kNamed &&
               (IsNamedTypeFamily(stripped_base, "Slice") || IsNamedTypeFamily(stripped_base, "Buffer") ||
                stripped_base.name == "str" || stripped_base.name == "string")) ||
            stripped_base.kind == sema::Type::Kind::kString) {
            const std::string value = NewValue();
            Emit({
                .kind = Instruction::Kind::kField,
                .result = value,
                .type = sema::NamedType("usize"),
                .target = "len",
                .target_kind = Instruction::TargetKind::kField,
                .target_name = "len",
                .target_display = "len",
                .target_base_type = base.type,
                .operands = {base.value},
            });
            return ValueInfo {value, sema::NamedType("usize")};
        }
        return std::nullopt;
    }

    void EmitIndexBoundsCheck(const ValueInfo& base, const ValueInfo& index) {
        const auto length = EmitBoundsLengthValue(base);
        if (!length.has_value()) {
            return;
        }
        Emit({
            .kind = Instruction::Kind::kBoundsCheck,
            .op = "index",
            .operands = {index.value, length->value},
        });
    }

    void EmitSliceBoundsCheck(const ValueInfo& base, const std::optional<ValueInfo>& lower, const std::optional<ValueInfo>& upper) {
        if (!lower.has_value() && !upper.has_value()) {
            return;
        }
        const auto length = EmitBoundsLengthValue(base);
        if (!length.has_value()) {
            return;
        }
        std::vector<std::string> operands;
        if (lower.has_value() && upper.has_value()) {
            operands = {lower->value, upper->value, length->value};
        } else {
            operands = {lower.has_value() ? lower->value : upper->value, length->value};
        }
        Emit({
            .kind = Instruction::Kind::kBoundsCheck,
            .op = "slice",
            .operands = std::move(operands),
        });
    }

    ValueInfo EmitConvertValue(const ValueInfo& operand, const sema::Type& target_type) {
        const std::string value = NewValue();
        Instruction::Kind kind = Instruction::Kind::kConvert;
        switch (ClassifyMirConversion(module_, operand.type, target_type)) {
            case ExplicitConversionKind::kNumeric:
                kind = Instruction::Kind::kConvertNumeric;
                break;
            case ExplicitConversionKind::kDistinct:
                kind = Instruction::Kind::kConvertDistinct;
                break;
            case ExplicitConversionKind::kPointerToInt:
                kind = Instruction::Kind::kPointerToInt;
                break;
            case ExplicitConversionKind::kIntToPointer:
                kind = Instruction::Kind::kIntToPointer;
                break;
            case ExplicitConversionKind::kArrayToSlice:
                kind = Instruction::Kind::kArrayToSlice;
                break;
            case ExplicitConversionKind::kBufferToSlice:
                kind = Instruction::Kind::kBufferToSlice;
                break;
            case ExplicitConversionKind::kGeneric:
                kind = Instruction::Kind::kConvert;
                break;
        }
        Emit({
            .kind = kind,
            .result = value,
            .type = target_type,
            .target = sema::FormatType(target_type),
            .target_display = sema::FormatType(target_type),
            .operands = {operand.value},
        });
        return {value, target_type};
    }

    ValueInfo CoerceBinaryOperand(const ValueInfo& operand,
                                  const sema::Type& target_type) {
        if (sema::IsUnknown(target_type) || operand.type == target_type) {
            return operand;
        }
        return EmitConvertValue(operand, target_type);
    }

    ValueInfo EmitVariantMatchValue(const ValueInfo& selector, const std::string& variant_name) {
        const std::string value = NewValue();
        const std::string canonical_variant_name = CanonicalVariantDisplayName(selector.type, variant_name);
        Emit({
            .kind = Instruction::Kind::kVariantMatch,
            .result = value,
            .type = sema::BoolType(),
            .target = canonical_variant_name,
            .target_name = VariantLeafName(variant_name),
            .target_display = canonical_variant_name,
            .target_base_type = selector.type,
            .operands = {selector.value},
        });
        return {value, sema::BoolType()};
    }

    ValueInfo EmitVariantExtractValue(const ValueInfo& selector,
                                     const sema::Type& selector_type,
                                     const mc::support::SourceSpan& span,
                                     const std::string& variant_name,
                                     std::size_t payload_index) {
        const std::string value = NewValue();
        const sema::Type payload_type = KnownTypeOrError(InferVariantPayloadType(selector_type, variant_name, payload_index),
                                                         span,
                                                         "variant payload type for " + variant_name);
        const std::string canonical_variant_name = CanonicalVariantDisplayName(selector_type, variant_name);
        Emit({
            .kind = Instruction::Kind::kVariantExtract,
            .result = value,
            .type = payload_type,
            .op = std::to_string(payload_index),
            .target = canonical_variant_name,
            .target_name = VariantLeafName(variant_name),
            .target_display = canonical_variant_name,
            .target_base_type = selector_type,
            .target_index = static_cast<std::int64_t>(payload_index),
            .operands = {selector.value},
        });
        return {value, payload_type};
    }

    bool TryEmitVariantInit(const Expr& expr,
                           const std::vector<ValueInfo>& args,
                           const sema::Type& result_type,
                           bool wants_result,
                           ValueInfo* result) {
        if (expr.left == nullptr || expr.left->kind != Expr::Kind::kQualifiedName || result_type.kind != sema::Type::Kind::kNamed) {
            return false;
        }

        const auto* type_decl = sema::FindTypeDecl(sema_module_, result_type.name);
        if (type_decl == nullptr || type_decl->kind != Decl::Kind::kEnum || expr.left->text != result_type.name) {
            return false;
        }

        std::size_t variant_index = 0;
        for (; variant_index < type_decl->variants.size(); ++variant_index) {
            if (type_decl->variants[variant_index].name == expr.left->secondary_text) {
                break;
            }
        }
        if (variant_index >= type_decl->variants.size()) {
            return false;
        }

        const std::string value = wants_result ? NewValue() : std::string();
        const std::string canonical_variant_name = CanonicalVariantDisplayName(result_type, expr.left->secondary_text);
        Instruction instruction {
            .kind = Instruction::Kind::kVariantInit,
            .result = value,
            .type = result_type,
            .target = canonical_variant_name,
            .target_name = VariantLeafName(expr.left->secondary_text),
            .target_display = canonical_variant_name,
            .target_base_type = result_type,
            .target_index = static_cast<std::int64_t>(variant_index),
        };
        for (const auto& arg : args) {
            instruction.operands.push_back(arg.value);
        }
        Emit(std::move(instruction));
        if (wants_result && result != nullptr) {
            *result = {value, result_type};
        }
        return true;
    }

    Instruction::TargetKind InferSymbolRefKind(std::string_view name) const {
        if (sema::FindGlobalSummary(sema_module_, name) != nullptr) {
            return Instruction::TargetKind::kGlobal;
        }
        if (sema::FindFunctionSignature(sema_module_, name) != nullptr) {
            return Instruction::TargetKind::kFunction;
        }
        return Instruction::TargetKind::kOther;
    }

    bool IsExplicitConversionExpr(const Expr& expr) const {
        return expr.kind == Expr::Kind::kCall && expr.type_target != nullptr && !sema::IsUnknown(ExprTypeOrUnknown(expr));
    }

    sema::Type InferVariantPayloadType(const sema::Type& selector_type, std::string_view variant_name, std::size_t payload_index) const {
        const sema::Type stripped_selector = StripMirAliasOrDistinct(module_, selector_type);
        if (stripped_selector.kind != sema::Type::Kind::kNamed) {
            return sema::UnknownType();
        }

        const auto* type_decl = FindMirTypeDecl(module_, stripped_selector.name);
        if (type_decl == nullptr) {
            return sema::UnknownType();
        }

        const auto variant = InstantiateMirVariantDecl(*type_decl, stripped_selector, variant_name);
        if (!variant.has_value() || payload_index >= variant->payload_fields.size()) {
            return sema::UnknownType();
        }
        return variant->payload_fields[payload_index].second;
    }

    ValueInfo LoadLocalValue(const std::string& name) {
        const auto found = local_types_.find(name);
        if (found == local_types_.end()) {
            Report(decl_.span, "bootstrap MIR load references undeclared local: " + name);
        }
        const sema::Type type = KnownTypeOrError(found == local_types_.end() ? sema::UnknownType() : found->second,
                                                 decl_.span,
                                                 "local load type for " + name);
        const std::string value = NewValue();
        Emit({
            .kind = Instruction::Kind::kLoadLocal,
            .result = value,
            .type = type,
            .target = name,
        });
        return {value, type};
    }

    ValueInfo LowerExpr(const Expr& expr) {
        switch (expr.kind) {
            case Expr::Kind::kName: {
                if (local_types_.contains(expr.text)) {
                    return LoadLocalValue(expr.text);
                }
                const std::string value = NewValue();
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "symbol reference type for " + expr.text);
                Emit({
                    .kind = Instruction::Kind::kSymbolRef,
                    .result = value,
                    .type = type,
                    .target = expr.text,
                    .target_kind = InferSymbolRefKind(expr.text),
                    .target_name = expr.text,
                    .target_display = expr.text,
                });
                return {value, type};
            }
            case Expr::Kind::kQualifiedName: {
                if (local_types_.contains(expr.text)) {
                    const auto base = LoadLocalValue(expr.text);
                    const std::string value = NewValue();
                    const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "qualified field expression type");
                    const std::string target_display = CombineQualifiedName(expr);
                    Emit({
                        .kind = Instruction::Kind::kField,
                        .result = value,
                        .type = type,
                        .target = expr.secondary_text,
                        .target_kind = Instruction::TargetKind::kField,
                        .target_name = expr.secondary_text,
                        .target_display = target_display,
                        .target_base_type = base.type,
                        .operands = {base.value},
                    });
                    return {value, type};
                }
                if (const auto* global = sema::FindGlobalSummary(sema_module_, expr.text); global != nullptr) {
                    const std::string base_value = NewValue();
                    Emit({
                        .kind = Instruction::Kind::kSymbolRef,
                        .result = base_value,
                        .type = KnownTypeOrError(global->type, expr.span, "qualified global base type"),
                        .target = expr.text,
                        .target_kind = Instruction::TargetKind::kGlobal,
                        .target_name = expr.text,
                        .target_display = expr.text,
                    });

                    const std::string value = NewValue();
                    const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "qualified global field expression type");
                    const std::string target_display = CombineQualifiedName(expr);
                    Emit({
                        .kind = Instruction::Kind::kField,
                        .result = value,
                        .type = type,
                        .target = expr.secondary_text,
                        .target_kind = Instruction::TargetKind::kField,
                        .target_name = expr.secondary_text,
                        .target_display = target_display,
                        .target_base_type = KnownTypeOrError(global->type, expr.span, "qualified global field base type"),
                        .operands = {base_value},
                    });
                    return {value, type};
                }
                const std::string value = NewValue();
                const std::string qualified_name = CombineQualifiedName(expr);
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "symbol reference type for " + qualified_name);
                Emit({
                    .kind = Instruction::Kind::kSymbolRef,
                    .result = value,
                    .type = type,
                    .target = qualified_name,
                    .target_kind = InferTargetKindForExpr(expr),
                    .target_name = qualified_name,
                    .target_display = qualified_name,
                });
                return {value, type};
            }
            case Expr::Kind::kLiteral:
                return EmitLiteralValue(expr.text, KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "literal expression type"));
            case Expr::Kind::kUnary: {
                if (expr.text == "&" && expr.left != nullptr) {
                    const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "address-of expression type");
                    if (expr.left->kind == Expr::Kind::kName && local_types_.contains(expr.left->text)) {
                        const std::string value = NewValue();
                        Emit({
                            .kind = Instruction::Kind::kLocalAddr,
                            .result = value,
                            .type = type,
                            .target = expr.left->text,
                        });
                        return {value, type};
                    }

                    const TargetMetadata target_metadata = TargetMetadataForExpr(*expr.left);
                    if (target_metadata.kind == Instruction::TargetKind::kGlobal) {
                        const std::string value = NewValue();
                        Emit({
                            .kind = Instruction::Kind::kLocalAddr,
                            .result = value,
                            .type = type,
                            .target = target_metadata.target,
                            .target_kind = target_metadata.kind,
                            .target_name = target_metadata.name,
                            .target_display = target_metadata.display,
                        });
                        return {value, type};
                    }

                    if (expr.left->kind == Expr::Kind::kQualifiedName) {
                        StoreBaseMetadata base_storage;
                        sema::Type base_type = sema::UnknownType();
                        if (local_types_.contains(expr.left->text)) {
                            base_storage = {
                                .kind = Instruction::StorageBaseKind::kLocal,
                                .name = expr.left->text,
                            };
                            base_type = local_types_.at(expr.left->text);
                        } else if (const auto* global = sema::FindGlobalSummary(sema_module_, expr.left->text); global != nullptr) {
                            base_storage = {
                                .kind = Instruction::StorageBaseKind::kGlobal,
                                .name = expr.left->text,
                            };
                            base_type = global->type;
                        } else if (InferTargetKindForExpr(*expr.left) == Instruction::TargetKind::kGlobal) {
                            base_storage = {
                                .kind = Instruction::StorageBaseKind::kGlobal,
                                .name = CombineQualifiedName(*expr.left),
                            };
                            base_type = ExprTypeOrUnknown(*expr.left);
                        }
                        if (base_storage.kind != Instruction::StorageBaseKind::kNone) {
                            const std::string value = NewValue();
                            Emit({
                                .kind = Instruction::Kind::kLocalAddr,
                                .result = value,
                                .type = type,
                                .target = CombineQualifiedName(*expr.left),
                                .target_kind = Instruction::TargetKind::kField,
                                .target_name = expr.left->secondary_text,
                                .target_display = CombineQualifiedName(*expr.left),
                                .target_base_storage = base_storage.kind,
                                .target_base_name = base_storage.name,
                                .target_base_type = KnownTypeOrError(base_type,
                                                                     expr.left->span,
                                                                     "address-of qualified field base type"),
                            });
                            return {value, type};
                        }
                    }

                    if (expr.left->kind == Expr::Kind::kField && expr.left->left != nullptr) {
                        const StoreBaseMetadata base_storage = StoreBaseMetadataForExpr(*expr.left->left);
                        if (base_storage.kind != Instruction::StorageBaseKind::kNone) {
                            const std::string value = NewValue();
                            Emit({
                                .kind = Instruction::Kind::kLocalAddr,
                                .result = value,
                                .type = type,
                                .target = RenderExprInline(*expr.left),
                                .target_kind = Instruction::TargetKind::kField,
                                .target_name = expr.left->text,
                                .target_display = RenderExprInline(*expr.left),
                                .target_base_storage = base_storage.kind,
                                .target_base_name = base_storage.name,
                                .target_base_type = KnownTypeOrError(ExprTypeOrUnknown(*expr.left->left),
                                                                     expr.left->left->span,
                                                                     "address-of field base type"),
                            });
                            return {value, type};
                        }
                    }

                    if (expr.left->kind == Expr::Kind::kParen && expr.left->left != nullptr) {
                        Report(expr.span,
                               "not yet supported in MIR: address-of " + std::string(ToString(expr.left->left->kind)) + " expression");
                        return EmitLiteralValue("0", sema::UnknownType());
                    }
                    if (IsAddressOfLvalueKind(expr.left->kind)) {
                        Report(expr.span,
                               "not yet supported in MIR: address-of " + std::string(ToString(expr.left->kind)) + " expression");
                        return EmitLiteralValue("0", sema::UnknownType());
                    }
                }
                const auto operand = LowerExpr(*expr.left);
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "unary expression type");
                const std::string value = NewValue();
                Emit({
                    .kind = Instruction::Kind::kUnary,
                    .result = value,
                    .type = type,
                    .op = expr.text,
                    .operands = {operand.value},
                });
                return {value, type};
            }
            case Expr::Kind::kBinary: {
                if (expr.text == "&&" || expr.text == "||") {
                    return LowerShortCircuitLogicalExpr(expr);
                }
                ValueInfo left = LowerExpr(*expr.left);
                ValueInfo right = LowerExpr(*expr.right);
                const sema::Type result_type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "binary expression type");

                if (expr.text == "+" || expr.text == "-" || expr.text == "*" || expr.text == "/" || expr.text == "%" ||
                    expr.text == "&" || expr.text == "|" || expr.text == "^") {
                    if (!sema::IsUnknown(result_type) && result_type.kind != sema::Type::Kind::kIntLiteral &&
                        result_type.kind != sema::Type::Kind::kFloatLiteral) {
                        left = CoerceBinaryOperand(left, result_type);
                        right = CoerceBinaryOperand(right, result_type);
                    }
                } else if (expr.text == "<<" || expr.text == ">>") {
                    if (!sema::IsUnknown(result_type) && result_type.kind != sema::Type::Kind::kIntLiteral) {
                        left = CoerceBinaryOperand(left, result_type);
                        right = CoerceBinaryOperand(right, result_type);
                    }
                } else if ((expr.text == "==" || expr.text == "!=" || expr.text == "<" || expr.text == ">" ||
                            expr.text == "<=" || expr.text == ">=") &&
                           left.type != right.type) {
                    if (left.type.kind == sema::Type::Kind::kIntLiteral || left.type.kind == sema::Type::Kind::kFloatLiteral) {
                        left = CoerceBinaryOperand(left, right.type);
                    } else if (right.type.kind == sema::Type::Kind::kIntLiteral || right.type.kind == sema::Type::Kind::kFloatLiteral) {
                        right = CoerceBinaryOperand(right, left.type);
                    }
                }

                return EmitBinaryValue(expr.text, left, right, result_type);
            }
            case Expr::Kind::kRange: {
                const auto left = LowerExpr(*expr.left);
                const auto right = LowerExpr(*expr.right);
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "binary expression type");
                const std::string value = NewValue();
                Emit({
                    .kind = Instruction::Kind::kBinary,
                    .result = value,
                    .type = type,
                    .op = expr.text,
                    .operands = {left.value, right.value},
                    .arithmetic_semantics = ClassifyBinaryArithmeticSemantics(module_, expr.text, type),
                });
                return {value, type};
            }
            case Expr::Kind::kCall: {
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "call result type");
                if (IsExplicitConversionExpr(expr) && !expr.args.empty()) {
                    return EmitConvertValue(LowerExpr(*expr.args.front()), type);
                }
                std::vector<ValueInfo> args;
                args.reserve(expr.args.size());
                for (const auto& arg : expr.args) {
                    args.push_back(LowerExpr(*arg));
                }
                ValueInfo variant_init_result;
                if (TryEmitVariantInit(expr, args, type, true, &variant_init_result)) {
                    return variant_init_result;
                }
                ValueInfo special_result;
                if (TryEmitSpecialCall(expr, args, type, true, &special_result)) {
                    return special_result;
                }
                const auto callee = LowerCalleeExpr(*expr.left);
                std::vector<std::string> operands = {callee.value};
                for (const auto& arg : args) {
                    operands.push_back(arg.value);
                }
                const std::string value = NewValue();
                const TargetMetadata target_metadata = TargetMetadataForExpr(*expr.left);
                Emit({
                    .kind = Instruction::Kind::kCall,
                    .result = value,
                    .type = type,
                    .target = target_metadata.target,
                    .target_kind = target_metadata.kind,
                    .target_name = target_metadata.name,
                    .target_display = target_metadata.display,
                    .operands = std::move(operands),
                });
                return {value, type};
            }
            case Expr::Kind::kField:
            case Expr::Kind::kDerefField: {
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "field expression type");
                if (expr.kind == Expr::Kind::kField && expr.left != nullptr) {
                    const sema::Type selector_type = StripMirAliasOrDistinct(module_, ExprTypeOrUnknown(*expr.left));
                    if (selector_type.kind == sema::Type::Kind::kNamed) {
                        const TypeDecl* type_decl = FindMirTypeDecl(module_, selector_type.name);
                        if (type_decl != nullptr && !type_decl->variants.empty()) {
                            for (const auto& variant : type_decl->variants) {
                                if (variant.name == expr.text) {
                                    const std::string value = NewValue();
                                    Emit({
                                        .kind = Instruction::Kind::kSymbolRef,
                                        .result = value,
                                        .type = type,
                                        .target = expr.text,
                                        .target_kind = Instruction::TargetKind::kOther,
                                        .target_name = expr.text,
                                        .target_display = RenderExprInline(expr),
                                    });
                                    return {value, type};
                                }
                            }
                        }
                    }
                }
                const auto base = LowerExpr(*expr.left);
                const std::string value = NewValue();
                const std::string target_display = RenderExprInline(expr);
                Emit({
                    .kind = Instruction::Kind::kField,
                    .result = value,
                    .type = type,
                    .target = expr.text.empty() ? target_display : expr.text,
                    .target_kind = expr.kind == Expr::Kind::kField ? Instruction::TargetKind::kField
                                                                    : Instruction::TargetKind::kDerefField,
                    .target_name = expr.text,
                    .target_display = target_display,
                    .target_base_type = base.type,
                    .operands = {base.value},
                });
                return {value, type};
            }
            case Expr::Kind::kIndex: {
                const auto base = LowerExpr(*expr.left);
                const auto index = LowerExpr(*expr.right);
                EmitIndexBoundsCheck(base, index);
                const std::string value = NewValue();
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "index expression type");
                const std::string target_display = RenderExprInline(expr);
                Emit({
                    .kind = Instruction::Kind::kIndex,
                    .result = value,
                    .type = type,
                    .target_kind = Instruction::TargetKind::kIndex,
                    .target_display = target_display,
                    .target_base_type = base.type,
                    .target_aux_types = {index.type},
                    .operands = {base.value, index.value},
                });
                return {value, type};
            }
            case Expr::Kind::kSlice: {
                const auto base = LowerExpr(*expr.left);
                std::optional<ValueInfo> lower;
                std::optional<ValueInfo> upper;
                std::vector<std::string> operands = {base.value};
                std::vector<sema::Type> target_aux_types;
                if (expr.right != nullptr) {
                    lower = LowerExpr(*expr.right);
                    operands.push_back(lower->value);
                    target_aux_types.push_back(lower->type);
                }
                if (expr.extra != nullptr) {
                    upper = LowerExpr(*expr.extra);
                    operands.push_back(upper->value);
                    target_aux_types.push_back(upper->type);
                }
                EmitSliceBoundsCheck(base, lower, upper);
                const std::string value = NewValue();
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "slice expression type");
                Emit({
                    .kind = Instruction::Kind::kSlice,
                    .result = value,
                    .type = type,
                    .target_display = RenderExprInline(expr),
                    .target_base_type = base.type,
                    .target_aux_types = std::move(target_aux_types),
                    .operands = std::move(operands),
                });
                return {value, type};
            }
            case Expr::Kind::kAggregateInit: {
                std::vector<std::string> operands;
                std::vector<std::string> field_names;
                for (const auto& field_init : expr.field_inits) {
                    operands.push_back(LowerExpr(*field_init.value).value);
                    field_names.push_back(field_init.has_name ? field_init.name : std::string("_"));
                }
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "aggregate initializer type");
                const std::string value = NewValue();
                Emit({
                    .kind = Instruction::Kind::kAggregateInit,
                    .result = value,
                    .type = type,
                    .target = sema::FormatType(type),
                    .operands = std::move(operands),
                    .field_names = std::move(field_names),
                });
                return {value, type};
            }
            case Expr::Kind::kParen:
                return LowerExpr(*expr.left);
        }

        Report(expr.span, "not yet supported in MIR: " + std::string(ToString(expr.kind)) + " expression");
        return EmitLiteralValue("0", sema::UnknownType());
    }

    void BindLocal(const std::string& name,
                   const std::string& value,
                   const sema::Type& type,
                   bool is_mutable,
                   const mc::support::SourceSpan& span) {
        if (local_types_.contains(name)) {
            Report(span, "MIR lowering cannot rebind existing local: " + name);
            return;
        }
        const sema::Type local_type = EnsureLocal(name, type, is_mutable);
        Emit({
            .kind = Instruction::Kind::kStoreLocal,
            .type = local_type,
            .target = name,
            .operands = {value},
        });
    }

    void AssignNamedTarget(const std::string& name, const ValueInfo& value, const mc::support::SourceSpan& span) {
        const auto found_local = local_types_.find(name);
        if (found_local != local_types_.end()) {
            Emit({
                .kind = Instruction::Kind::kStoreLocal,
                .type = found_local->second,
                .target = name,
                .operands = {value.value},
            });
            return;
        }

        if (const auto* global = sema::FindGlobalSummary(sema_module_, name); global != nullptr) {
            Emit({
                .kind = Instruction::Kind::kStoreTarget,
                .type = global->type,
                .target = name,
                .target_kind = Instruction::TargetKind::kGlobal,
                .target_name = name,
                .operands = {value.value},
            });
            return;
        }

        Report(span, "MIR lowering expected semantically resolved assignment target: " + name);
    }

    void EmitStoreTarget(const Expr& target, const ValueInfo& value) {
        std::vector<std::string> target_operands = {value.value};
        const StoreBaseMetadata base_storage = target.left != nullptr ? StoreBaseMetadataForExpr(*target.left) : StoreBaseMetadata{};
        if (target.kind == Expr::Kind::kIndex && target.left != nullptr && target.right != nullptr) {
            const auto base = LowerExpr(*target.left);
            const auto index = LowerExpr(*target.right);
            EmitIndexBoundsCheck(base, index);
            target_operands.push_back(base.value);
            target_operands.push_back(index.value);
        } else if ((target.kind == Expr::Kind::kField || target.kind == Expr::Kind::kDerefField) && target.left != nullptr) {
            target_operands.push_back(LowerExpr(*target.left).value);
        } else if (target.kind == Expr::Kind::kUnary && target.text == "*" && target.left != nullptr) {
            target_operands.push_back(LowerExpr(*target.left).value);
        }
        Emit({
            .kind = Instruction::Kind::kStoreTarget,
            .type = KnownTypeOrError(ExprTypeOrUnknown(target), target.span, "assignment target type"),
            .target = RenderExprInline(target),
            .target_kind = target.kind == Expr::Kind::kField ? Instruction::TargetKind::kField
                          : target.kind == Expr::Kind::kDerefField ? Instruction::TargetKind::kDerefField
                          : target.kind == Expr::Kind::kIndex ? Instruction::TargetKind::kIndex
                                                              : Instruction::TargetKind::kOther,
            .target_name = (target.kind == Expr::Kind::kField || target.kind == Expr::Kind::kDerefField) ? target.text : std::string(),
            .target_display = RenderExprInline(target),
            .target_base_storage = base_storage.kind,
            .target_base_name = base_storage.name,
            .target_base_type = target.left != nullptr
                                    ? KnownTypeOrError(ExprTypeOrUnknown(*target.left), target.left->span, "assignment target base type")
                                    : sema::UnknownType(),
            .target_aux_types = target.kind == Expr::Kind::kIndex && target.right != nullptr
                                    ? std::vector<sema::Type> {KnownTypeOrError(ExprTypeOrUnknown(*target.right),
                                                                                target.right->span,
                                                                                "assignment index type")}
                                    : std::vector<sema::Type> {},
            .operands = std::move(target_operands),
        });
    }

    void StoreLocal(const std::string& name, const std::string& value, const sema::Type& type, bool is_mutable) {
        const sema::Type local_type = EnsureLocal(name, type, is_mutable);
        Emit({
            .kind = Instruction::Kind::kStoreLocal,
            .type = local_type,
            .target = name,
            .operands = {value},
        });
    }

    ValueInfo EmitTupleExtractValue(const ValueInfo& tuple_value,
                                    std::size_t index,
                                    const mc::support::SourceSpan& span) {
        if (tuple_value.type.kind != sema::Type::Kind::kTuple || index >= tuple_value.type.subtypes.size()) {
            Report(span, "MIR lowering expected tuple value for multi-result destructuring");
            return EmitLiteralValue("0", sema::UnknownType());
        }

        const std::string value = NewValue();
        const std::string field_name = std::to_string(index);
        Emit({
            .kind = Instruction::Kind::kField,
            .result = value,
            .type = tuple_value.type.subtypes[index],
            .target = field_name,
            .target_kind = Instruction::TargetKind::kField,
            .target_name = field_name,
            .target_display = tuple_value.value + "." + field_name,
            .target_base_type = tuple_value.type,
            .operands = {tuple_value.value},
        });
        return {value, tuple_value.type.subtypes[index]};
    }

    std::optional<std::vector<ValuePiece>> LowerExprValuesForArity(const std::vector<std::unique_ptr<Expr>>& exprs,
                                                                   std::size_t expected_count,
                                                                   const mc::support::SourceSpan& report_span,
                                                                   std::string_view count_error) {
        auto lower_direct = [&]() {
            std::vector<ValuePiece> values;
            values.reserve(exprs.size());
            for (const auto& expr : exprs) {
                values.push_back({
                    .value = LowerExpr(*expr),
                    .span = expr->span,
                });
            }
            return values;
        };

        if (exprs.size() == expected_count) {
            return lower_direct();
        }

        std::vector<ValuePiece> flattened;
        flattened.reserve(expected_count);
        for (const auto& expr : exprs) {
            const ValueInfo value = LowerExpr(*expr);
            if (value.type.kind == sema::Type::Kind::kTuple) {
                for (std::size_t index = 0; index < value.type.subtypes.size(); ++index) {
                    flattened.push_back({
                        .value = EmitTupleExtractValue(value , index, expr->span),
                        .span = expr->span,
                    });
                }
            } else {
                flattened.push_back({
                    .value = value,
                    .span = expr->span,
                });
            }
        }

        if (flattened.size() != expected_count) {
            Report(report_span, std::string(count_error));
            return std::nullopt;
        }

        return flattened;
    }

    void LowerBindingLike(const Stmt& stmt, bool is_mutable) {
        const sema::Type declared_type = sema::TypeFromAst(stmt.type_ann.get());
        if (!stmt.has_initializer) {
            for (const auto& name : stmt.pattern.names) {
                if (local_types_.contains(name)) {
                    Report(stmt.span, "MIR lowering cannot rebind existing local: " + name);
                    return;
                }
                EnsureLocal(name, declared_type, is_mutable);
            }
            return;
        }

        const auto values = LowerExprValuesForArity(stmt.exprs,
                                                    stmt.pattern.names.size(),
                                                    stmt.span,
                                                    "bootstrap MIR requires one initializer per binding name");
        if (!values.has_value()) {
            return;
        }

        for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
            BindLocal(stmt.pattern.names[index],
                      (*values)[index].value.value,
                      sema::IsUnknown(declared_type) ? (*values)[index].value.type : declared_type,
                      is_mutable,
                      stmt.span);
        }
    }

    void LowerAssign(const Stmt& stmt) {
        const auto values = LowerExprValuesForArity(stmt.assign_values,
                                                    stmt.assign_targets.size(),
                                                    stmt.span,
                                                    "bootstrap MIR requires one value per assignment target");
        if (!values.has_value()) {
            return;
        }

        for (std::size_t index = 0; index < stmt.assign_targets.size(); ++index) {
            const Expr& target = *stmt.assign_targets[index];
            if (target.kind == Expr::Kind::kName) {
                AssignNamedTarget(target.text, (*values)[index].value, target.span);
                continue;
            }

            if (target.kind == Expr::Kind::kQualifiedName && InferTargetKindForExpr(target) == Instruction::TargetKind::kGlobal) {
                const std::string qualified_name = CombineQualifiedName(target);
                Emit({
                    .kind = Instruction::Kind::kStoreTarget,
                    .type = KnownTypeOrError(ExprTypeOrUnknown(target), target.span, "assignment target type"),
                    .target = qualified_name,
                    .target_kind = Instruction::TargetKind::kGlobal,
                    .target_name = qualified_name,
                    .target_display = qualified_name,
                    .operands = {(*values)[index].value.value},
                });
                continue;
            }

            EmitStoreTarget(target, (*values)[index].value);
        }
    }

    #include "compiler/mir/mir_lower_stmt.inc"

    const Decl& decl_;
    std::filesystem::path file_path_;
    const sema::Module& sema_module_;
    const Module& module_;
    support::DiagnosticSink& diagnostics_;
    Function function_;
    std::unordered_map<std::string, sema::Type> local_types_;
    std::optional<std::size_t> current_block_;
    std::vector<LoopTarget> loop_stack_;
    std::vector<DeferScope> defer_scopes_;
    std::size_t next_value_id_ = 0;
    std::size_t next_block_id_ = 0;
    std::size_t next_hidden_local_id_ = 0;
};

TypeDecl LowerTypeDeclSummary(const sema::TypeDeclSummary& summary) {
    TypeDecl type_decl;
    switch (summary.kind) {
        case Decl::Kind::kStruct:
            type_decl.kind = TypeDecl::Kind::kStruct;
            break;
        case Decl::Kind::kEnum:
            type_decl.kind = TypeDecl::Kind::kEnum;
            break;
        case Decl::Kind::kDistinct:
            type_decl.kind = TypeDecl::Kind::kDistinct;
            break;
        case Decl::Kind::kTypeAlias:
            type_decl.kind = TypeDecl::Kind::kAlias;
            break;
        default:
            break;
    }

    type_decl.name = summary.name;
    type_decl.type_params = summary.type_params;
    type_decl.is_packed = summary.is_packed;
    type_decl.fields = summary.fields;
    type_decl.aliased_type = summary.aliased_type;
    for (const auto& variant : summary.variants) {
        VariantDecl mir_variant;
        mir_variant.name = variant.name;
        mir_variant.payload_fields = variant.payload_fields;
        type_decl.variants.push_back(std::move(mir_variant));
    }
    return type_decl;
}

LowerResult LowerSourceFile(const ast::SourceFile& source_file,
                            const sema::Module& sema_module,
                            const std::filesystem::path& file_path,
                            support::DiagnosticSink& diagnostics) {
    auto module = std::make_unique<Module>();

    module->imports.reserve(source_file.imports.size());
    for (const auto& import_decl : source_file.imports) {
        module->imports.push_back(import_decl.module_name);
    }

    for (const auto& decl : source_file.decls) {
        if (decl.kind == Decl::Kind::kStruct || decl.kind == Decl::Kind::kEnum || decl.kind == Decl::Kind::kDistinct ||
            decl.kind == Decl::Kind::kTypeAlias) {
            const sema::TypeDeclSummary* summary = sema::FindTypeDecl(sema_module, decl.name);
            if (summary != nullptr) {
                module->type_decls.push_back(LowerTypeDeclSummary(*summary));
            }
            continue;
        }

        if (decl.kind == Decl::Kind::kConst || decl.kind == Decl::Kind::kVar) {
            GlobalDecl global;
            global.is_const = decl.kind == Decl::Kind::kConst;
            global.is_extern = false;
            global.names = decl.pattern.names;
            global.type = sema::TypeFromAst(decl.type_ann.get());
            for (const auto& value : decl.values) {
                global.initializers.push_back(RenderExprInline(*value));
            }
            if (!decl.pattern.names.empty()) {
                if (const sema::GlobalSummary* summary = sema::FindGlobalSummary(sema_module, decl.pattern.names.front()); summary != nullptr) {
                    global.constant_values = summary->constant_values;
                }
            }
            module->globals.push_back(std::move(global));
        }
    }

    std::unordered_set<std::string> seen_type_names;
    for (const auto& type_decl : module->type_decls) {
        seen_type_names.insert(type_decl.name);
    }
    for (const auto& type_decl : sema_module.type_decls) {
        if (!seen_type_names.insert(type_decl.name).second) {
            continue;
        }
        module->type_decls.push_back(LowerTypeDeclSummary(type_decl));
    }

    for (const auto& decl : source_file.decls) {
        if (decl.kind == Decl::Kind::kExternFunc) {
            const sema::FunctionSignature* signature = sema::FindFunctionSignature(sema_module, decl.name);
            Function function;
            function.name = decl.name;
            function.is_extern = true;
            function.extern_abi = decl.extern_abi;
            function.type_params = decl.type_params;
            for (std::size_t index = 0; index < decl.params.size(); ++index) {
                const sema::Type type =
                    (signature != nullptr && index < signature->param_types.size())
                        ? signature->param_types[index]
                        : sema::TypeFromAst(decl.params[index].type.get());
                function.locals.push_back({
                    .name = decl.params[index].name,
                    .type = type,
                    .is_parameter = true,
                    .is_noalias = signature != nullptr && index < signature->param_is_noalias.size() && signature->param_is_noalias[index],
                    .is_mutable = false,
                });
            }
            if (signature != nullptr && !signature->return_types.empty()) {
                function.return_types = signature->return_types;
            } else {
                for (const auto& return_type : decl.return_types) {
                    function.return_types.push_back(sema::TypeFromAst(return_type.get()));
                }
            }
            module->functions.push_back(std::move(function));
            continue;
        }

        if (decl.kind == Decl::Kind::kFunc) {
            module->functions.push_back(FunctionLowerer(decl, *module, file_path, sema_module, diagnostics).Run());
        }
    }

    return {
        .module = std::move(module),
        .ok = !diagnostics.HasErrors(),
    };
}

Function LowerFunctionDecl(const ast::Decl& decl,
                            const Module& module,
                            const std::filesystem::path& file_path,
                            const sema::Module& sema_module,
                            support::DiagnosticSink& diagnostics) {
    return FunctionLowerer(decl, module, file_path, sema_module, diagnostics).Run();
}

}  // namespace mc::mir