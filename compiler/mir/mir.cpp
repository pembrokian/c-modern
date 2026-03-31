#include "compiler/mir/mir.h"

#include <cstddef>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace mc::mir {
namespace {

using mc::ast::Decl;
using mc::ast::Expr;
using mc::ast::Stmt;
using mc::support::DiagnosticSeverity;

struct ValueInfo {
    std::string value;
    sema::Type type;
};

std::string_view ToString(TypeDecl::Kind kind) {
    switch (kind) {
        case TypeDecl::Kind::kStruct:
            return "struct";
        case TypeDecl::Kind::kEnum:
            return "enum";
        case TypeDecl::Kind::kDistinct:
            return "distinct";
        case TypeDecl::Kind::kAlias:
            return "alias";
    }

    return "type";
}

std::string_view ToString(Instruction::Kind kind) {
    switch (kind) {
        case Instruction::Kind::kConst:
            return "const";
        case Instruction::Kind::kLoadLocal:
            return "load_local";
        case Instruction::Kind::kStoreLocal:
            return "store_local";
        case Instruction::Kind::kStoreTarget:
            return "store_target";
        case Instruction::Kind::kSymbolRef:
            return "symbol_ref";
        case Instruction::Kind::kUnary:
            return "unary";
        case Instruction::Kind::kBinary:
            return "binary";
        case Instruction::Kind::kCall:
            return "call";
        case Instruction::Kind::kField:
            return "field";
        case Instruction::Kind::kIndex:
            return "index";
        case Instruction::Kind::kSlice:
            return "slice";
        case Instruction::Kind::kAggregateInit:
            return "aggregate_init";
    }

    return "instr";
}

std::string CombineQualifiedName(const Expr& expr) {
    if (expr.kind == Expr::Kind::kQualifiedName) {
        return expr.text + "." + expr.secondary_text;
    }
    return expr.text;
}

std::string RenderExprInline(const Expr& expr) {
    switch (expr.kind) {
        case Expr::Kind::kName:
            return expr.text;
        case Expr::Kind::kQualifiedName:
            return CombineQualifiedName(expr);
        case Expr::Kind::kLiteral:
            return expr.text;
        case Expr::Kind::kUnary:
            return expr.text + (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>"));
        case Expr::Kind::kBinary:
        case Expr::Kind::kRange:
            return (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + " " + expr.text + " " +
                   (expr.right != nullptr ? RenderExprInline(*expr.right) : std::string("<?>"));
        case Expr::Kind::kCall: {
            std::ostringstream stream;
            stream << (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) << '(';
            for (std::size_t index = 0; index < expr.args.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << RenderExprInline(*expr.args[index]);
            }
            stream << ')';
            return stream.str();
        }
        case Expr::Kind::kField:
            return (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + "." + expr.text;
        case Expr::Kind::kDerefField:
            return (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + ".*";
        case Expr::Kind::kIndex:
            return (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + "[" +
                   (expr.right != nullptr ? RenderExprInline(*expr.right) : std::string("<?>")) + "]";
        case Expr::Kind::kSlice: {
            std::ostringstream stream;
            stream << (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) << '[';
            if (expr.right != nullptr) {
                stream << RenderExprInline(*expr.right);
            }
            stream << ':';
            if (expr.extra != nullptr) {
                stream << RenderExprInline(*expr.extra);
            }
            stream << ']';
            return stream.str();
        }
        case Expr::Kind::kAggregateInit: {
            std::ostringstream stream;
            stream << (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) << '{';
            for (std::size_t index = 0; index < expr.field_inits.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                if (expr.field_inits[index].has_name) {
                    stream << expr.field_inits[index].name << ": ";
                }
                stream << RenderExprInline(*expr.field_inits[index].value);
            }
            stream << '}';
            return stream.str();
        }
        case Expr::Kind::kParen:
            return "(" + (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")) + ")";
    }

    return "<?>";
}

sema::Type InferLiteralType(const Expr& expr) {
    if (expr.secondary_text == "int_lit") {
        return sema::IntLiteralType();
    }
    if (expr.secondary_text == "float_lit") {
        return sema::FloatLiteralType();
    }
    if (expr.secondary_text == "string_lit") {
        return sema::StringType();
    }
    if (expr.text == "true" || expr.text == "false") {
        return sema::BoolType();
    }
    if (expr.text == "nil") {
        return sema::NilType();
    }
    return sema::UnknownType();
}

sema::Type InferUnaryType(const std::string& op, const sema::Type& operand_type) {
    if (op == "!") {
        return sema::BoolType();
    }
    if (op == "&") {
        return sema::PointerType(operand_type);
    }
    if (op == "*" && operand_type.kind == sema::Type::Kind::kPointer && !operand_type.subtypes.empty()) {
        return operand_type.subtypes.front();
    }
    return operand_type;
}

sema::Type InferBinaryType(const std::string& op, const sema::Type& left_type, const sema::Type& right_type) {
    if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=" || op == "&&" || op == "||") {
        return sema::BoolType();
    }
    if (op == "..") {
        if (!sema::IsUnknown(left_type)) {
            return sema::RangeType(left_type);
        }
        if (!sema::IsUnknown(right_type)) {
            return sema::RangeType(right_type);
        }
        return sema::RangeType(sema::UnknownType());
    }
    if (left_type == right_type && !sema::IsUnknown(left_type)) {
        return left_type;
    }
    if (!sema::IsUnknown(left_type)) {
        return left_type;
    }
    return right_type;
}

sema::Type InferCallType(const Expr& callee, const sema::Module& sema_module) {
    const auto callee_name = RenderExprInline(callee);
    const auto* found = sema::FindFunctionSignature(sema_module, callee_name);
    if (found == nullptr) {
        return sema::UnknownType();
    }
    if (found->return_types.empty()) {
        return sema::VoidType();
    }
    if (found->return_types.size() == 1) {
        return found->return_types.front();
    }
    return sema::TupleType(found->return_types);
}

void WriteIndent(std::ostringstream& stream, int indent) {
    for (int count = 0; count < indent; ++count) {
        stream << "  ";
    }
}

void WriteLine(std::ostringstream& stream, int indent, const std::string& text) {
    WriteIndent(stream, indent);
    stream << text << '\n';
}

class FunctionLowerer {
  public:
    FunctionLowerer(const Decl& decl,
                    const std::filesystem::path& file_path,
                    const sema::Module& sema_module,
                    support::DiagnosticSink& diagnostics)
        : decl_(decl), file_path_(file_path), sema_module_(sema_module), diagnostics_(diagnostics) {
        function_.name = decl.name;
        function_.type_params = decl.type_params;
        function_.return_types.reserve(decl.return_types.size());
        for (const auto& return_type : decl.return_types) {
            function_.return_types.push_back(sema::TypeFromAst(return_type.get()));
        }
    }

    Function Run() {
        for (const auto& param : decl_.params) {
            const sema::Type type = sema::TypeFromAst(param.type.get());
            function_.locals.push_back({
                .name = param.name,
                .type = type,
                .is_parameter = true,
                .is_mutable = false,
            });
            local_types_[param.name] = type;
        }

        current_block_ = CreateBlock("entry");

        if (decl_.body != nullptr) {
            LowerStmt(*decl_.body);
        }

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

    struct DeferredCall {
        std::string target;
        sema::Type type;
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

    void EnsureCurrentBlock() {
        if (!current_block_.has_value()) {
            current_block_ = CreateBlock("dead");
        }
    }

    std::string NewValue() {
        return "%v" + std::to_string(next_value_id_++);
    }

    std::string NewHiddenLocal(const std::string& prefix) {
        return "$" + prefix + std::to_string(next_hidden_local_id_++);
    }

    void SnapshotValueToLocal(const std::string& local_name, const ValueInfo& value) {
        StoreLocal(local_name, value.value, value.type, false);
    }

    DeferredCall CaptureDeferredCall(const Expr& expr) {
        const auto callee = LowerExpr(*expr.left);
        DeferredCall call {
            .target = RenderExprInline(*expr.left),
            .type = InferCallType(*expr.left, sema_module_),
            .callee_local = NewHiddenLocal("defer_callee"),
        };
        SnapshotValueToLocal(call.callee_local, callee);
        for (const auto& arg : expr.args) {
            const auto value = LowerExpr(*arg);
            const std::string arg_local = NewHiddenLocal("defer_arg");
            SnapshotValueToLocal(arg_local, value);
            call.arg_locals.push_back(arg_local);
        }
        return call;
    }

    void EmitDeferredCall(const DeferredCall& call) {
        const auto callee = LoadLocalValue(call.callee_local);
        std::vector<std::string> operands = {callee.value};
        for (const auto& arg_local : call.arg_locals) {
            operands.push_back(LoadLocalValue(arg_local).value);
        }
        Emit({
            .kind = Instruction::Kind::kCall,
            .type = call.type,
            .target = call.target,
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

    sema::Type EnsureLocal(const std::string& name, const sema::Type& suggested_type, bool is_mutable) {
        const auto found = local_types_.find(name);
        if (found != local_types_.end()) {
            if (sema::IsUnknown(found->second) && !sema::IsUnknown(suggested_type)) {
                found->second = suggested_type;
                for (auto& local : function_.locals) {
                    if (local.name == name) {
                        local.type = suggested_type;
                        local.is_mutable = is_mutable;
                        break;
                    }
                }
            }
            return found->second;
        }

        const sema::Type type = sema::IsUnknown(suggested_type) ? sema::UnknownType() : suggested_type;
        function_.locals.push_back({
            .name = name,
            .type = type,
            .is_parameter = false,
            .is_mutable = is_mutable,
        });
        local_types_[name] = type;
        return type;
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

    ValueInfo EmitBinaryValue(const std::string& op, const ValueInfo& left, const ValueInfo& right) {
        const std::string value = NewValue();
        const sema::Type type = InferBinaryType(op, left.type, right.type);
        Emit({
            .kind = Instruction::Kind::kBinary,
            .result = value,
            .type = type,
            .op = op,
            .operands = {left.value, right.value},
        });
        return {value, type};
    }

    ValueInfo LoadLocalValue(const std::string& name) {
        const auto found = local_types_.find(name);
        const sema::Type type = found == local_types_.end() ? sema::UnknownType() : found->second;
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
                Emit({
                    .kind = Instruction::Kind::kSymbolRef,
                    .result = value,
                    .type = sema::UnknownType(),
                    .target = expr.text,
                });
                return {value, sema::UnknownType()};
            }
            case Expr::Kind::kQualifiedName: {
                const std::string value = NewValue();
                Emit({
                    .kind = Instruction::Kind::kSymbolRef,
                    .result = value,
                    .type = sema::UnknownType(),
                    .target = CombineQualifiedName(expr),
                });
                return {value, sema::UnknownType()};
            }
            case Expr::Kind::kLiteral:
                return EmitLiteralValue(expr.text, InferLiteralType(expr));
            case Expr::Kind::kUnary: {
                const auto operand = LowerExpr(*expr.left);
                const sema::Type type = InferUnaryType(expr.text, operand.type);
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
            case Expr::Kind::kBinary:
            case Expr::Kind::kRange: {
                const auto left = LowerExpr(*expr.left);
                const auto right = LowerExpr(*expr.right);
                return EmitBinaryValue(expr.text, left, right);
            }
            case Expr::Kind::kCall: {
                const auto callee = LowerExpr(*expr.left);
                std::vector<std::string> operands = {callee.value};
                for (const auto& arg : expr.args) {
                    operands.push_back(LowerExpr(*arg).value);
                }
                const sema::Type type = InferCallType(*expr.left, sema_module_);
                const std::string value = NewValue();
                Emit({
                    .kind = Instruction::Kind::kCall,
                    .result = value,
                    .type = type,
                    .target = RenderExprInline(*expr.left),
                    .operands = std::move(operands),
                });
                return {value, type};
            }
            case Expr::Kind::kField:
            case Expr::Kind::kDerefField: {
                const auto base = LowerExpr(*expr.left);
                const std::string value = NewValue();
                Emit({
                    .kind = Instruction::Kind::kField,
                    .result = value,
                    .type = sema::UnknownType(),
                    .target = expr.text.empty() ? RenderExprInline(expr) : expr.text,
                    .operands = {base.value},
                });
                return {value, sema::UnknownType()};
            }
            case Expr::Kind::kIndex: {
                const auto base = LowerExpr(*expr.left);
                const auto index = LowerExpr(*expr.right);
                const std::string value = NewValue();
                Emit({
                    .kind = Instruction::Kind::kIndex,
                    .result = value,
                    .type = sema::UnknownType(),
                    .operands = {base.value, index.value},
                });
                return {value, sema::UnknownType()};
            }
            case Expr::Kind::kSlice: {
                std::vector<std::string> operands = {LowerExpr(*expr.left).value};
                if (expr.right != nullptr) {
                    operands.push_back(LowerExpr(*expr.right).value);
                }
                if (expr.extra != nullptr) {
                    operands.push_back(LowerExpr(*expr.extra).value);
                }
                const std::string value = NewValue();
                Emit({
                    .kind = Instruction::Kind::kSlice,
                    .result = value,
                    .type = sema::UnknownType(),
                    .operands = std::move(operands),
                });
                return {value, sema::UnknownType()};
            }
            case Expr::Kind::kAggregateInit: {
                std::vector<std::string> operands;
                std::vector<std::string> field_names;
                for (const auto& field_init : expr.field_inits) {
                    operands.push_back(LowerExpr(*field_init.value).value);
                    field_names.push_back(field_init.has_name ? field_init.name : std::string("_"));
                }
                const sema::Type type = expr.left != nullptr ? sema::NamedType(RenderExprInline(*expr.left)) : sema::UnknownType();
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

        return {NewValue(), sema::UnknownType()};
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

    void LowerBindingLike(const Stmt& stmt, bool is_mutable, bool allow_existing) {
        const sema::Type declared_type = sema::TypeFromAst(stmt.type_ann.get());
        if (!stmt.has_initializer) {
            for (const auto& name : stmt.pattern.names) {
                EnsureLocal(name, declared_type, is_mutable);
            }
            return;
        }

        if (stmt.pattern.names.size() != stmt.exprs.size()) {
            Report(stmt.span, "bootstrap MIR requires one initializer per binding name");
            return;
        }

        for (std::size_t index = 0; index < stmt.pattern.names.size(); ++index) {
            const auto value = LowerExpr(*stmt.exprs[index]);
            if (!allow_existing && local_types_.contains(stmt.pattern.names[index])) {
                Report(stmt.span, "bootstrap MIR binding would shadow an existing local");
                return;
            }
            StoreLocal(stmt.pattern.names[index], value.value, sema::IsUnknown(declared_type) ? value.type : declared_type, is_mutable);
        }
    }

    void LowerAssign(const Stmt& stmt) {
        if (stmt.assign_targets.size() != stmt.assign_values.size()) {
            Report(stmt.span, "bootstrap MIR requires one value per assignment target");
            return;
        }

        for (std::size_t index = 0; index < stmt.assign_targets.size(); ++index) {
            const auto value = LowerExpr(*stmt.assign_values[index]);
            const Expr& target = *stmt.assign_targets[index];
            if (target.kind == Expr::Kind::kName) {
                StoreLocal(target.text, value.value, value.type, true);
                continue;
            }

            Emit({
                .kind = Instruction::Kind::kStoreTarget,
                .type = value.type,
                .target = RenderExprInline(target),
                .operands = {value.value},
            });
        }
    }

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
                LowerBindingLike(stmt, true, false);
                return;
            case Stmt::Kind::kConst:
                LowerBindingLike(stmt, false, false);
                return;
            case Stmt::Kind::kBindingOrAssign: {
                bool any_existing = false;
                bool any_missing = false;
                for (const auto& name : stmt.pattern.names) {
                    if (local_types_.contains(name)) {
                        any_existing = true;
                    } else {
                        any_missing = true;
                    }
                }
                if (any_existing && any_missing) {
                    Report(stmt.span, "bootstrap MIR cannot resolve mixed binding-or-assignment patterns without semantic analysis");
                    return;
                }
                LowerBindingLike(stmt, true, any_existing);
                return;
            }
            case Stmt::Kind::kAssign:
                LowerAssign(stmt);
                return;
            case Stmt::Kind::kExpr:
                if (!stmt.exprs.empty()) {
                    static_cast<void>(LowerExpr(*stmt.exprs.front()));
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
        current_block_ = (then_reaches_merge || else_reaches_merge) ? std::optional<std::size_t>(merge_block) : std::nullopt;
    }

    void LowerSwitch(const Stmt& stmt) {
        if (stmt.exprs.empty()) {
            Report(stmt.span, "switch statement is missing selector expression");
            return;
        }

        EnsureCurrentBlock();
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
            if (switch_case.pattern.kind != mc::ast::CasePattern::Kind::kExpr || switch_case.pattern.expr == nullptr) {
                Report(switch_case.span, "bootstrap MIR only lowers expression switch cases");
                return;
            }

            current_block_ = test_blocks[index];
            const auto case_value = LowerExpr(*switch_case.pattern.expr);
            const auto comparison = EmitBinaryValue("==", selector, case_value);
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

        current_block_ = reaches_merge ? std::optional<std::size_t>(merge_block) : std::nullopt;
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
            Report(stmt.span, "bootstrap MIR for-range requires an explicit range expression");
            return;
        }

        const auto start = LowerExpr(*stmt.exprs.front()->left);
        const auto end = LowerExpr(*stmt.exprs.front()->right);
        const sema::Type loop_type = !sema::IsUnknown(start.type) ? start.type : end.type;
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
        const auto condition = EmitBinaryValue("<", current_value, end);
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
        const auto one = EmitLiteralValue("1", sema::IntLiteralType());
        const auto next = EmitBinaryValue("+", current_index, one);
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

        const auto iterable = LowerExpr(*stmt.exprs.front());
        const std::string iterable_name = NewHiddenLocal("iterable");
        const std::string index_name = NewHiddenLocal("index");
        StoreLocal(iterable_name, iterable.value, iterable.type, true);
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
            .operands = {stored_iterable.value},
        });
        const auto condition = EmitBinaryValue("<", current_index, {len_value, sema::NamedType("usize")});
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
        const std::string item_value = NewValue();
        Emit({
            .kind = Instruction::Kind::kIndex,
            .result = item_value,
            .type = sema::UnknownType(),
            .operands = {body_iterable.value, body_index.value},
        });
        if (stmt.kind == Stmt::Kind::kForEachIndex) {
            StoreLocal(stmt.loop_name, body_index.value, sema::NamedType("usize"), true);
            StoreLocal(stmt.loop_second_name, item_value, sema::UnknownType(), true);
        } else {
            StoreLocal(stmt.loop_name, item_value, sema::UnknownType(), true);
        }
        LowerStmt(*stmt.then_branch);
        if (current_block_.has_value() && function_.blocks[*current_block_].terminator.kind == Terminator::Kind::kNone) {
            function_.blocks[*current_block_].terminator.kind = Terminator::Kind::kBranch;
            function_.blocks[*current_block_].terminator.true_target = function_.blocks[step_block].label;
        }

        current_block_ = step_block;
        const auto step_index = LoadLocalValue(index_name);
        const auto one = EmitLiteralValue("1", sema::NamedType("usize"));
        const auto next = EmitBinaryValue("+", step_index, one);
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
            Report(stmt.span, "bootstrap MIR only lowers defer call expressions");
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
        for (const auto& expr : stmt.exprs) {
            return_values.push_back(LowerExpr(*expr).value);
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

    const Decl& decl_;
    std::filesystem::path file_path_;
    const sema::Module& sema_module_;
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

}  // namespace

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
            TypeDecl type_decl;
            switch (decl.kind) {
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

            type_decl.name = decl.name;
            type_decl.type_params = decl.type_params;
            for (const auto& field : decl.fields) {
                type_decl.fields.emplace_back(field.name, sema::TypeFromAst(field.type.get()));
            }
            for (const auto& variant : decl.variants) {
                type_decl.variants.push_back(variant.name);
            }
            type_decl.aliased_type = sema::TypeFromAst(decl.aliased_type.get());
            module->type_decls.push_back(std::move(type_decl));
            continue;
        }

        if (decl.kind == Decl::Kind::kConst || decl.kind == Decl::Kind::kVar) {
            GlobalDecl global;
            global.is_const = decl.kind == Decl::Kind::kConst;
            global.names = decl.pattern.names;
            global.type = sema::TypeFromAst(decl.type_ann.get());
            for (const auto& value : decl.values) {
                global.initializers.push_back(RenderExprInline(*value));
            }
            module->globals.push_back(std::move(global));
        }
    }

    for (const auto& decl : source_file.decls) {
        if (decl.kind == Decl::Kind::kExternFunc) {
            Function function;
            function.name = decl.name;
            function.is_extern = true;
            function.extern_abi = decl.extern_abi;
            function.type_params = decl.type_params;
            for (const auto& param : decl.params) {
                function.locals.push_back({
                    .name = param.name,
                    .type = sema::TypeFromAst(param.type.get()),
                    .is_parameter = true,
                    .is_mutable = false,
                });
            }
            for (const auto& return_type : decl.return_types) {
                function.return_types.push_back(sema::TypeFromAst(return_type.get()));
            }
            module->functions.push_back(std::move(function));
            continue;
        }

        if (decl.kind == Decl::Kind::kFunc) {
            module->functions.push_back(FunctionLowerer(decl, file_path, sema_module, diagnostics).Run());
        }
    }

    return {
        .module = std::move(module),
        .ok = !diagnostics.HasErrors(),
    };
}

bool ValidateModule(const Module& module,
                    const std::filesystem::path& file_path,
                    support::DiagnosticSink& diagnostics) {
    bool ok = true;

    auto report = [&](const std::string& message) {
        diagnostics.Report({
            .file_path = file_path,
            .span = mc::support::kDefaultSourceSpan,
            .severity = DiagnosticSeverity::kError,
            .message = message,
        });
        ok = false;
    };

    std::unordered_set<std::string> function_names;
    for (const auto& function : module.functions) {
        if (!function_names.insert(function.name).second) {
            report("duplicate MIR function: " + function.name);
        }

        if (function.is_extern) {
            continue;
        }

        if (function.blocks.empty()) {
            report("function has no MIR blocks: " + function.name);
            continue;
        }

        std::unordered_set<std::string> block_labels;
        std::unordered_map<std::string, const BasicBlock*> blocks_by_label;
        for (const auto& block : function.blocks) {
            if (!block_labels.insert(block.label).second) {
                report("duplicate MIR block label in function " + function.name + ": " + block.label);
            }
            blocks_by_label[block.label] = &block;
        }

        std::unordered_set<std::string> reachable_blocks;
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

        std::unordered_set<std::string> defined_values;
        for (const auto& block : function.blocks) {
            if (!reachable_blocks.contains(block.label)) {
                continue;
            }

            for (const auto& instruction : block.instructions) {
                for (const auto& operand : instruction.operands) {
                    if (!operand.empty() && !defined_values.contains(operand)) {
                        report("undefined MIR operand in function " + function.name + ": " + operand);
                    }
                }

                if (!instruction.result.empty()) {
                    if (!defined_values.insert(instruction.result).second) {
                        report("duplicate MIR value in function " + function.name + ": " + instruction.result);
                    }
                }
            }

            switch (block.terminator.kind) {
                case Terminator::Kind::kNone:
                    report("unterminated MIR block in function " + function.name + ": " + block.label);
                    break;
                case Terminator::Kind::kReturn:
                    for (const auto& value : block.terminator.values) {
                        if (!defined_values.contains(value)) {
                            report("return uses undefined MIR value in function " + function.name + ": " + value);
                        }
                    }
                    break;
                case Terminator::Kind::kBranch:
                    if (!block_labels.contains(block.terminator.true_target)) {
                        report("branch targets missing MIR block in function " + function.name + ": " + block.terminator.true_target);
                    }
                    break;
                case Terminator::Kind::kCondBranch:
                    if (block.terminator.values.size() != 1 || !defined_values.contains(block.terminator.values.front())) {
                        report("conditional branch must use one defined MIR condition in function " + function.name);
                    }
                    if (!block_labels.contains(block.terminator.true_target)) {
                        report("conditional branch true target missing MIR block in function " + function.name + ": " + block.terminator.true_target);
                    }
                    if (!block_labels.contains(block.terminator.false_target)) {
                        report("conditional branch false target missing MIR block in function " + function.name + ": " + block.terminator.false_target);
                    }
                    break;
            }
        }
    }

    return ok;
}

std::string DumpModule(const Module& module) {
    std::ostringstream stream;
    WriteLine(stream, 0, "Module");

    for (const auto& import_name : module.imports) {
        WriteLine(stream, 1, "Import name=" + import_name);
    }

    for (const auto& type_decl : module.type_decls) {
        WriteLine(stream, 1, std::string("TypeDecl kind=") + std::string(ToString(type_decl.kind)) + " name=" + type_decl.name);
        if (!type_decl.type_params.empty()) {
            std::ostringstream type_params;
            type_params << "typeParams=[";
            for (std::size_t index = 0; index < type_decl.type_params.size(); ++index) {
                if (index > 0) {
                    type_params << ", ";
                }
                type_params << type_decl.type_params[index];
            }
            type_params << ']';
            WriteLine(stream, 2, type_params.str());
        }
        for (const auto& field : type_decl.fields) {
            WriteLine(stream, 2, "Field name=" + field.first + " type=" + sema::FormatType(field.second));
        }
        for (const auto& variant : type_decl.variants) {
            WriteLine(stream, 2, "Variant name=" + variant);
        }
        if (!sema::IsUnknown(type_decl.aliased_type)) {
            WriteLine(stream, 2, "AliasedType=" + sema::FormatType(type_decl.aliased_type));
        }
    }

    for (const auto& global : module.globals) {
        std::ostringstream header;
        header << (global.is_const ? "ConstGlobal names=[" : "VarGlobal names=[");
        for (std::size_t index = 0; index < global.names.size(); ++index) {
            if (index > 0) {
                header << ", ";
            }
            header << global.names[index];
        }
        header << ']';
        if (!sema::IsUnknown(global.type)) {
            header << " type=" << sema::FormatType(global.type);
        }
        WriteLine(stream, 1, header.str());
        for (const auto& initializer : global.initializers) {
            WriteLine(stream, 2, "init " + initializer);
        }
    }

    for (const auto& function : module.functions) {
        std::ostringstream header;
        header << "Function name=" << function.name;
        if (function.is_extern) {
            header << " extern=" << function.extern_abi;
        }
        if (!function.return_types.empty()) {
            header << " returns=[";
            for (std::size_t index = 0; index < function.return_types.size(); ++index) {
                if (index > 0) {
                    header << ", ";
                }
                header << sema::FormatType(function.return_types[index]);
            }
            header << ']';
        }
        WriteLine(stream, 1, header.str());

        for (const auto& local : function.locals) {
            std::ostringstream local_line;
            local_line << "Local name=" << local.name << " type=" << sema::FormatType(local.type);
            if (local.is_parameter) {
                local_line << " param";
            }
            if (!local.is_mutable) {
                local_line << " readonly";
            }
            WriteLine(stream, 2, local_line.str());
        }

        for (const auto& block : function.blocks) {
            WriteLine(stream, 2, "Block label=" + block.label);
            for (const auto& instruction : block.instructions) {
                std::ostringstream line;
                line << ToString(instruction.kind);
                if (!instruction.result.empty()) {
                    line << ' ' << instruction.result << ':' << sema::FormatType(instruction.type);
                }
                if (!instruction.op.empty()) {
                    line << " op=" << instruction.op;
                }
                if (!instruction.target.empty()) {
                    line << " target=" << instruction.target;
                }
                if (!instruction.operands.empty()) {
                    line << " operands=[";
                    for (std::size_t index = 0; index < instruction.operands.size(); ++index) {
                        if (index > 0) {
                            line << ", ";
                        }
                        line << instruction.operands[index];
                    }
                    line << ']';
                }
                if (!instruction.field_names.empty()) {
                    line << " fields=[";
                    for (std::size_t index = 0; index < instruction.field_names.size(); ++index) {
                        if (index > 0) {
                            line << ", ";
                        }
                        line << instruction.field_names[index];
                    }
                    line << ']';
                }
                WriteLine(stream, 3, line.str());
            }

            std::ostringstream terminator;
            switch (block.terminator.kind) {
                case Terminator::Kind::kNone:
                    terminator << "terminator none";
                    break;
                case Terminator::Kind::kReturn:
                    terminator << "terminator return";
                    if (!block.terminator.values.empty()) {
                        terminator << " values=[";
                        for (std::size_t index = 0; index < block.terminator.values.size(); ++index) {
                            if (index > 0) {
                                terminator << ", ";
                            }
                            terminator << block.terminator.values[index];
                        }
                        terminator << ']';
                    }
                    break;
                case Terminator::Kind::kBranch:
                    terminator << "terminator branch target=" << block.terminator.true_target;
                    break;
                case Terminator::Kind::kCondBranch:
                    terminator << "terminator cond value="
                               << (block.terminator.values.empty() ? std::string("<?>") : block.terminator.values.front())
                               << " true=" << block.terminator.true_target << " false=" << block.terminator.false_target;
                    break;
            }
            WriteLine(stream, 3, terminator.str());
        }
    }

    return stream.str();
}

}  // namespace mc::mir