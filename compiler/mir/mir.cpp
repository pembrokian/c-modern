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

const TypeDecl* FindMirTypeDecl(const Module& module, std::string_view name);

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
        case Instruction::Kind::kBoundsCheck:
            return "bounds_check";
        case Instruction::Kind::kDivCheck:
            return "div_check";
        case Instruction::Kind::kShiftCheck:
            return "shift_check";
        case Instruction::Kind::kUnary:
            return "unary";
        case Instruction::Kind::kBinary:
            return "binary";
        case Instruction::Kind::kConvert:
            return "convert";
        case Instruction::Kind::kConvertNumeric:
            return "convert_numeric";
        case Instruction::Kind::kConvertDistinct:
            return "convert_distinct";
        case Instruction::Kind::kPointerToInt:
            return "pointer_to_int";
        case Instruction::Kind::kIntToPointer:
            return "int_to_pointer";
        case Instruction::Kind::kArrayToSlice:
            return "array_to_slice";
        case Instruction::Kind::kBufferToSlice:
            return "buffer_to_slice";
        case Instruction::Kind::kCall:
            return "call";
        case Instruction::Kind::kVolatileLoad:
            return "volatile_load";
        case Instruction::Kind::kVolatileStore:
            return "volatile_store";
        case Instruction::Kind::kAtomicLoad:
            return "atomic_load";
        case Instruction::Kind::kAtomicStore:
            return "atomic_store";
        case Instruction::Kind::kAtomicExchange:
            return "atomic_exchange";
        case Instruction::Kind::kAtomicCompareExchange:
            return "atomic_compare_exchange";
        case Instruction::Kind::kAtomicFetchAdd:
            return "atomic_fetch_add";
        case Instruction::Kind::kField:
            return "field";
        case Instruction::Kind::kIndex:
            return "index";
        case Instruction::Kind::kSlice:
            return "slice";
        case Instruction::Kind::kAggregateInit:
            return "aggregate_init";
        case Instruction::Kind::kVariantMatch:
            return "variant_match";
        case Instruction::Kind::kVariantExtract:
            return "variant_extract";
    }

    return "instr";
}

std::string_view ToString(Instruction::TargetKind kind) {
    switch (kind) {
        case Instruction::TargetKind::kNone:
            return "none";
        case Instruction::TargetKind::kFunction:
            return "function";
        case Instruction::TargetKind::kGlobal:
            return "global";
        case Instruction::TargetKind::kField:
            return "field";
        case Instruction::TargetKind::kDerefField:
            return "deref_field";
        case Instruction::TargetKind::kIndex:
            return "index";
        case Instruction::TargetKind::kOther:
            return "other";
    }

    return "none";
}

std::string_view ToString(Instruction::ArithmeticSemantics semantics) {
    switch (semantics) {
        case Instruction::ArithmeticSemantics::kNone:
            return "none";
        case Instruction::ArithmeticSemantics::kWrap:
            return "wrap";
    }

    return "none";
}

std::string VariantTypeName(std::string_view variant_name) {
    const std::size_t separator = variant_name.find('.');
    if (separator == std::string_view::npos) {
        return std::string(variant_name);
    }
    return std::string(variant_name.substr(0, separator));
}

std::string VariantLeafName(std::string_view variant_name) {
    const std::size_t separator = variant_name.find('.');
    if (separator == std::string_view::npos) {
        return std::string(variant_name);
    }
    return std::string(variant_name.substr(separator + 1));
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
            const auto render_type = [&](const ast::TypeExpr& type, const auto& self) -> std::string {
                switch (type.kind) {
                    case ast::TypeExpr::Kind::kNamed: {
                        if (type.type_args.empty()) {
                            return type.name;
                        }
                        std::ostringstream type_stream;
                        type_stream << type.name << '<';
                        for (std::size_t index = 0; index < type.type_args.size(); ++index) {
                            if (index > 0) {
                                type_stream << ", ";
                            }
                            type_stream << self(*type.type_args[index], self);
                        }
                        type_stream << '>';
                        return type_stream.str();
                    }
                    case ast::TypeExpr::Kind::kPointer:
                        return "*" + (type.inner != nullptr ? self(*type.inner, self) : std::string("<?>"));
                    case ast::TypeExpr::Kind::kConst:
                        return "const " + (type.inner != nullptr ? self(*type.inner, self) : std::string("<?>"));
                    case ast::TypeExpr::Kind::kArray:
                        return "[" + (type.length_expr != nullptr ? RenderExprInline(*type.length_expr) : std::string("?")) + "]" +
                               (type.inner != nullptr ? self(*type.inner, self) : std::string("<?>"));
                    case ast::TypeExpr::Kind::kParen:
                        return "(" + (type.inner != nullptr ? self(*type.inner, self) : std::string("<?>")) + ")";
                }
                return "<?>";
            };
            stream << (expr.type_target != nullptr ? render_type(*expr.type_target, render_type)
                                                   : (expr.left != nullptr ? RenderExprInline(*expr.left) : std::string("<?>")))
                   << '(';
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

void WriteIndent(std::ostringstream& stream, int indent) {
    for (int count = 0; count < indent; ++count) {
        stream << "  ";
    }
}

void WriteLine(std::ostringstream& stream, int indent, const std::string& text) {
    WriteIndent(stream, indent);
    stream << text << '\n';
}

bool IsIntegerType(const sema::Type& type) {
    return type.kind == sema::Type::Kind::kNamed &&
           (type.name == "i8" || type.name == "i16" || type.name == "i32" || type.name == "i64" || type.name == "isize" ||
            type.name == "u8" || type.name == "u16" || type.name == "u32" || type.name == "u64" || type.name == "usize" ||
            type.name == "uintptr");
}

bool IsFloatType(const sema::Type& type) {
    return type.kind == sema::Type::Kind::kNamed && (type.name == "f32" || type.name == "f64");
}

bool IsNumericType(const sema::Type& type) {
    return type.kind == sema::Type::Kind::kIntLiteral || type.kind == sema::Type::Kind::kFloatLiteral || IsIntegerType(type) || IsFloatType(type);
}

bool IsWraparoundBinaryOp(std::string_view op) {
    return op == "+" || op == "-" || op == "*";
}

bool IsIntegerLikeType(const sema::Type& type) {
    return type.kind == sema::Type::Kind::kIntLiteral || IsIntegerType(type);
}

bool IsBoolType(const sema::Type& type) {
    return type == sema::BoolType() || (type.kind == sema::Type::Kind::kNamed && type.name == "bool");
}

bool IsMemoryOrderType(const sema::Type& type) {
    return type.kind == sema::Type::Kind::kNamed && type.name == "MemoryOrder";
}

bool IsPointerLikeType(const sema::Type& type) {
    return type.kind == sema::Type::Kind::kPointer;
}

bool IsUintPtrType(const sema::Type& type) {
    return type.kind == sema::Type::Kind::kNamed && type.name == "uintptr";
}

bool IsBuiltinNamedNonAggregate(const sema::Type& type) {
    if (type.kind == sema::Type::Kind::kBool || type.kind == sema::Type::Kind::kString) {
        return true;
    }
    if (type.kind != sema::Type::Kind::kNamed) {
        return false;
    }
    return IsIntegerType(type) || IsFloatType(type) || type.name == "bool" || type.name == "string" || type.name == "str" || type.name == "cstr";
}

bool IsUsizeCompatibleType(const sema::Type& type) {
    return type == sema::NamedType("usize") || type.kind == sema::Type::Kind::kIntLiteral;
}

std::optional<std::size_t> ParseArrayLength(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::size_t value = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
        value = (value * 10) + static_cast<std::size_t>(ch - '0');
    }
    return value;
}

bool IsAssignableType(const sema::Type& expected, const sema::Type& actual) {
    if (sema::IsUnknown(expected) || sema::IsUnknown(actual)) {
        return true;
    }
    if (expected == actual) {
        return true;
    }
    if (IsIntegerType(expected) && actual.kind == sema::Type::Kind::kIntLiteral) {
        return true;
    }
    if (IsFloatType(expected) && actual.kind == sema::Type::Kind::kFloatLiteral) {
        return true;
    }
    if (IsPointerLikeType(expected) && actual.kind == sema::Type::Kind::kNil) {
        return true;
    }
    return false;
}

bool HasCompatibleNumericTypes(const sema::Type& left, const sema::Type& right) {
    if (sema::IsUnknown(left) || sema::IsUnknown(right)) {
        return true;
    }
    if (left == right) {
        return IsNumericType(left);
    }
    if (IsIntegerType(left) && right.kind == sema::Type::Kind::kIntLiteral) {
        return true;
    }
    if (IsIntegerType(right) && left.kind == sema::Type::Kind::kIntLiteral) {
        return true;
    }
    if (IsFloatType(left) && right.kind == sema::Type::Kind::kFloatLiteral) {
        return true;
    }
    if (IsFloatType(right) && left.kind == sema::Type::Kind::kFloatLiteral) {
        return true;
    }
    if ((left.kind == sema::Type::Kind::kFloatLiteral && right.kind == sema::Type::Kind::kIntLiteral) ||
        (right.kind == sema::Type::Kind::kFloatLiteral && left.kind == sema::Type::Kind::kIntLiteral)) {
        return true;
    }
    return false;
}

bool HasCompatibleComparisonTypes(const sema::Type& left, const sema::Type& right) {
    return HasCompatibleNumericTypes(left, right) || IsAssignableType(left, right) || IsAssignableType(right, left);
}

sema::Type RangeElementType(const sema::Type& range_type) {
    if (range_type.kind == sema::Type::Kind::kRange && !range_type.subtypes.empty()) {
        return range_type.subtypes.front();
    }
    return sema::UnknownType();
}

sema::Type IterableElementType(const sema::Type& iterable_type) {
    if (iterable_type.kind == sema::Type::Kind::kArray && !iterable_type.subtypes.empty()) {
        return iterable_type.subtypes.front();
    }
    if (iterable_type.kind == sema::Type::Kind::kNamed && (iterable_type.name == "Slice" || iterable_type.name == "Buffer") &&
        !iterable_type.subtypes.empty()) {
        return iterable_type.subtypes.front();
    }
    return sema::UnknownType();
}

sema::Type StripMirAliasOrDistinct(const Module& module, sema::Type type) {
    if (type.kind != sema::Type::Kind::kNamed) {
        return type;
    }
    const TypeDecl* type_decl = FindMirTypeDecl(module, type.name);
    if (type_decl == nullptr) {
        return type;
    }
    if (type_decl->kind != TypeDecl::Kind::kDistinct && type_decl->kind != TypeDecl::Kind::kAlias) {
        return type;
    }
    return type_decl->aliased_type;
}

std::optional<sema::Type> PointerPointeeType(const sema::Type& type) {
    if (type.kind != sema::Type::Kind::kPointer || type.subtypes.empty()) {
        return std::nullopt;
    }
    return type.subtypes.front();
}

std::optional<sema::Type> AtomicElementType(const Module& module, const sema::Type& pointer_type) {
    const auto pointee = PointerPointeeType(StripMirAliasOrDistinct(module, pointer_type));
    if (!pointee.has_value()) {
        return std::nullopt;
    }
    const sema::Type stripped_pointee = StripMirAliasOrDistinct(module, *pointee);
    if (stripped_pointee.kind != sema::Type::Kind::kNamed || stripped_pointee.name != "Atomic" || stripped_pointee.subtypes.empty()) {
        return std::nullopt;
    }
    return stripped_pointee.subtypes.front();
}

enum class SpecialCallKind {
    kNone,
    kVolatileLoad,
    kVolatileStore,
    kAtomicLoad,
    kAtomicStore,
    kAtomicExchange,
    kAtomicCompareExchange,
    kAtomicFetchAdd,
};

SpecialCallKind ClassifySpecialCall(std::string_view callee_name) {
    if (callee_name == "volatile_load" || callee_name == "hal.volatile_load") {
        return SpecialCallKind::kVolatileLoad;
    }
    if (callee_name == "volatile_store" || callee_name == "hal.volatile_store") {
        return SpecialCallKind::kVolatileStore;
    }
    if (callee_name == "atomic_load" || callee_name == "sync.atomic_load") {
        return SpecialCallKind::kAtomicLoad;
    }
    if (callee_name == "atomic_store" || callee_name == "sync.atomic_store") {
        return SpecialCallKind::kAtomicStore;
    }
    if (callee_name == "atomic_exchange" || callee_name == "sync.atomic_exchange") {
        return SpecialCallKind::kAtomicExchange;
    }
    if (callee_name == "atomic_compare_exchange" || callee_name == "sync.atomic_compare_exchange") {
        return SpecialCallKind::kAtomicCompareExchange;
    }
    if (callee_name == "atomic_fetch_add" || callee_name == "sync.atomic_fetch_add") {
        return SpecialCallKind::kAtomicFetchAdd;
    }
    return SpecialCallKind::kNone;
}

bool IsDedicatedCallSurface(std::string_view callee_name) {
    return ClassifySpecialCall(callee_name) != SpecialCallKind::kNone;
}

enum class ExplicitConversionKind {
    kGeneric,
    kNumeric,
    kDistinct,
    kPointerToInt,
    kIntToPointer,
    kArrayToSlice,
    kBufferToSlice,
};

ExplicitConversionKind ClassifyMirConversion(const Module& module, const sema::Type& source_type, const sema::Type& target_type) {
    if (target_type.kind == sema::Type::Kind::kNamed && target_type.name == "Slice") {
        if (source_type.kind == sema::Type::Kind::kArray) {
            return ExplicitConversionKind::kArrayToSlice;
        }
        if (source_type.kind == sema::Type::Kind::kNamed && source_type.name == "Buffer") {
            return ExplicitConversionKind::kBufferToSlice;
        }
    }

    const sema::Type stripped_source = StripMirAliasOrDistinct(module, source_type);
    const sema::Type stripped_target = StripMirAliasOrDistinct(module, target_type);
    if (IsPointerLikeType(stripped_source) && IsUintPtrType(stripped_target)) {
        return ExplicitConversionKind::kPointerToInt;
    }
    if (IsUintPtrType(stripped_source) && IsPointerLikeType(stripped_target)) {
        return ExplicitConversionKind::kIntToPointer;
    }
    const bool source_wrapped = stripped_source != source_type;
    const bool target_wrapped = stripped_target != target_type;
    if ((source_wrapped || target_wrapped) && stripped_source == stripped_target) {
        return ExplicitConversionKind::kDistinct;
    }

    if (IsNumericType(stripped_source) && IsNumericType(stripped_target)) {
        return ExplicitConversionKind::kNumeric;
    }

    return ExplicitConversionKind::kGeneric;
}

Instruction::ArithmeticSemantics ClassifyBinaryArithmeticSemantics(const Module& module,
                                                                   std::string_view op,
                                                                   const sema::Type& result_type) {
    if (!IsWraparoundBinaryOp(op)) {
        return Instruction::ArithmeticSemantics::kNone;
    }

    const sema::Type stripped_result = StripMirAliasOrDistinct(module, result_type);
    if (IsIntegerType(stripped_result)) {
        return Instruction::ArithmeticSemantics::kWrap;
    }
    return Instruction::ArithmeticSemantics::kNone;
}

std::size_t ProcedureParamCount(const sema::Type& type) {
    if (type.kind != sema::Type::Kind::kProcedure || type.metadata.empty()) {
        return 0;
    }
    return static_cast<std::size_t>(std::stoul(type.metadata));
}

std::vector<sema::Type> ProcedureReturnTypes(const sema::Type& type) {
    if (type.kind != sema::Type::Kind::kProcedure) {
        return {};
    }
    const std::size_t param_count = ProcedureParamCount(type);
    if (param_count >= type.subtypes.size()) {
        return {};
    }
    return std::vector<sema::Type>(type.subtypes.begin() + static_cast<std::ptrdiff_t>(param_count), type.subtypes.end());
}

sema::Type ProcedureResultType(const sema::Type& type) {
    const std::vector<sema::Type> returns = ProcedureReturnTypes(type);
    if (returns.empty()) {
        return sema::VoidType();
    }
    if (returns.size() == 1) {
        return returns.front();
    }
    return sema::TupleType(returns);
}

const TypeDecl* FindMirTypeDecl(const Module& module, std::string_view name) {
    for (const auto& type_decl : module.type_decls) {
        if (type_decl.name == name) {
            return &type_decl;
        }
    }
    return nullptr;
}

const GlobalDecl* FindMirGlobalDecl(const Module& module, std::string_view name) {
    for (const auto& global : module.globals) {
        for (const auto& global_name : global.names) {
            if (global_name == name) {
                return &global;
            }
        }
    }
    return nullptr;
}

const Function* FindMirFunction(const Module& module, std::string_view name) {
    for (const auto& function : module.functions) {
        if (function.name == name) {
            return &function;
        }
    }
    return nullptr;
}

sema::Type FunctionProcedureType(const Function& function) {
    std::vector<sema::Type> param_types;
    for (const auto& local : function.locals) {
        if (local.is_parameter) {
            param_types.push_back(local.type);
        }
    }
    return sema::ProcedureType(param_types, function.return_types);
}

const VariantDecl* FindMirVariantDecl(const TypeDecl& type_decl, std::string_view variant_name) {
    const std::size_t separator = variant_name.find('.');
    const std::string qualified_type = separator == std::string_view::npos ? type_decl.name : std::string(variant_name.substr(0, separator));
    const std::string leaf_name = separator == std::string_view::npos ? std::string(variant_name) : std::string(variant_name.substr(separator + 1));
    if (!qualified_type.empty() && qualified_type != type_decl.name) {
        return nullptr;
    }
    for (const auto& variant : type_decl.variants) {
        if (variant.name == leaf_name) {
            return &variant;
        }
    }
    return nullptr;
}

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

    std::vector<sema::Type> ExpandReturnTypes(const sema::Type& type) const {
        if (type.kind == sema::Type::Kind::kVoid) {
            return {};
        }
        if (type.kind == sema::Type::Kind::kTuple) {
            return type.subtypes;
        }
        return {type};
    }

    sema::Type InferCalleeTypeFromCallSite(const Expr& call_expr, const sema::Type& result_type) const {
        std::vector<sema::Type> param_types;
        param_types.reserve(call_expr.args.size());
        for (const auto& arg : call_expr.args) {
            param_types.push_back(ExprTypeOrUnknown(*arg));
        }
        return sema::ProcedureType(std::move(param_types), ExpandReturnTypes(result_type));
    }

    sema::Type ExprTypeOrUnknown(const Expr& expr) const {
        if (const sema::Type* type = sema::FindExprType(sema_module_, expr); type != nullptr) {
            return *type;
        }
        if (expr.kind == Expr::Kind::kName) {
            return InferSymbolRefType(expr.text);
        }
        if (expr.kind == Expr::Kind::kQualifiedName) {
            return InferSymbolRefType(CombineQualifiedName(expr));
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

    std::string RenderOrderMetadata(const Expr& expr, std::size_t order_index) const {
        if (expr.args.size() <= order_index) {
            return {};
        }
        return "order=" + RenderExprInline(*expr.args[order_index]);
    }

    std::string RenderCompareExchangeOrderMetadata(const Expr& expr) const {
        if (expr.args.size() <= 4) {
            return {};
        }
        return "success=" + RenderExprInline(*expr.args[3]) + ",failure=" + RenderExprInline(*expr.args[4]);
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

        const bool produces_value = kind == SpecialCallKind::kVolatileLoad || kind == SpecialCallKind::kAtomicLoad ||
                                    kind == SpecialCallKind::kAtomicExchange || kind == SpecialCallKind::kAtomicCompareExchange ||
                                    kind == SpecialCallKind::kAtomicFetchAdd;

        Instruction instruction {
            .type = result_type,
            .target = callee_name,
        };
        if (wants_result || produces_value) {
            instruction.result = NewValue();
        }
        for (const auto& arg : args) {
            instruction.operands.push_back(arg.value);
        }

        switch (kind) {
            case SpecialCallKind::kVolatileLoad:
                instruction.kind = Instruction::Kind::kVolatileLoad;
                break;
            case SpecialCallKind::kVolatileStore:
                instruction.kind = Instruction::Kind::kVolatileStore;
                break;
            case SpecialCallKind::kAtomicLoad:
                instruction.kind = Instruction::Kind::kAtomicLoad;
                instruction.op = RenderOrderMetadata(expr, 1);
                break;
            case SpecialCallKind::kAtomicStore:
                instruction.kind = Instruction::Kind::kAtomicStore;
                instruction.op = RenderOrderMetadata(expr, 2);
                break;
            case SpecialCallKind::kAtomicExchange:
                instruction.kind = Instruction::Kind::kAtomicExchange;
                instruction.op = RenderOrderMetadata(expr, 2);
                break;
            case SpecialCallKind::kAtomicCompareExchange:
                instruction.kind = Instruction::Kind::kAtomicCompareExchange;
                instruction.op = RenderCompareExchangeOrderMetadata(expr);
                break;
            case SpecialCallKind::kAtomicFetchAdd:
                instruction.kind = Instruction::Kind::kAtomicFetchAdd;
                instruction.op = RenderOrderMetadata(expr, 2);
                break;
            case SpecialCallKind::kNone:
                return false;
        }

        Emit(instruction);
        if (wants_result && result != nullptr) {
            *result = {instruction.result, result_type};
        }
        return true;
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

        const sema::Type call_type = ExprTypeOrUnknown(expr);
        const sema::Type fallback_result_type = sema::IsUnknown(call_type) ? sema::VoidType() : call_type;
        if (TryEmitSpecialCall(expr, args, fallback_result_type, false, nullptr)) {
            return;
        }
        const auto callee = LowerCalleeExpr(*expr.left, InferCalleeTypeFromCallSite(expr, fallback_result_type));

        std::vector<std::string> operands = {callee.value};
        for (const auto& arg : args) {
            operands.push_back(arg.value);
        }

        Emit({
            .kind = Instruction::Kind::kCall,
            .type = fallback_result_type,
            .target = RenderExprInline(*expr.left),
            .operands = std::move(operands),
        });
    }

    ValueInfo LowerCalleeExpr(const Expr& expr, const sema::Type& fallback_type) {
        if (expr.kind == Expr::Kind::kName) {
            const std::string value = NewValue();
            sema::Type type = ExprTypeOrUnknown(expr);
            if (sema::IsUnknown(type)) {
                type = fallback_type;
            }
            Emit({
                .kind = Instruction::Kind::kSymbolRef,
                .result = value,
                .type = type,
                .target = expr.text,
                .target_kind = InferSymbolRefKind(expr.text),
                .target_name = expr.text,
            });
            return {value, type};
        }
        if (expr.kind == Expr::Kind::kQualifiedName) {
            const std::string qualified_name = CombineQualifiedName(expr);
            const std::string value = NewValue();
            sema::Type type = ExprTypeOrUnknown(expr);
            if (sema::IsUnknown(type)) {
                type = fallback_type;
            }
            Emit({
                .kind = Instruction::Kind::kSymbolRef,
                .result = value,
                .type = type,
                .target = qualified_name,
                .target_kind = InferSymbolRefKind(qualified_name),
                .target_name = qualified_name,
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
        const auto callee = LowerCalleeExpr(*expr.left, InferCalleeTypeFromCallSite(expr, sema::VoidType()));
        DeferredCall call {
            .target = RenderExprInline(*expr.left),
            .type = sema::VoidType(),
            .callee_local = NewHiddenLocal("defer_callee"),
        };
        SnapshotValueToLocal(call.callee_local, callee);
        for (const auto& value : args) {
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

    sema::Type KnownTypeOrError(const sema::Type& type, const mc::support::SourceSpan& span, const std::string& context) {
        if (!sema::IsUnknown(type)) {
            return type;
        }
        Report(span, "bootstrap MIR requires sema-known " + context);
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

    std::optional<ValueInfo> EmitBoundsLengthValue(const ValueInfo& base) {
        const sema::Type stripped_base = StripMirAliasOrDistinct(module_, base.type);
        if (stripped_base.kind == sema::Type::Kind::kArray) {
            const auto length = ParseArrayLength(stripped_base.metadata);
            if (!length.has_value()) {
                return std::nullopt;
            }
            return EmitLiteralValue(std::to_string(*length), sema::NamedType("usize"));
        }
        if ((stripped_base.kind == sema::Type::Kind::kNamed &&
             (stripped_base.name == "Slice" || stripped_base.name == "Buffer" || stripped_base.name == "str" || stripped_base.name == "string")) ||
            stripped_base.kind == sema::Type::Kind::kString) {
            const std::string value = NewValue();
            Emit({
                .kind = Instruction::Kind::kField,
                .result = value,
                .type = sema::NamedType("usize"),
                .target = "len",
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
            .operands = {operand.value},
        });
        return {value, target_type};
    }

    ValueInfo EmitVariantMatchValue(const ValueInfo& selector, const std::string& variant_name) {
        const std::string value = NewValue();
        Emit({
            .kind = Instruction::Kind::kVariantMatch,
            .result = value,
            .type = sema::BoolType(),
            .target = variant_name,
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
        Emit({
            .kind = Instruction::Kind::kVariantExtract,
            .result = value,
            .type = payload_type,
            .op = std::to_string(payload_index),
            .target = variant_name,
            .operands = {selector.value},
        });
        return {value, payload_type};
    }

    sema::Type InferSymbolRefType(std::string_view name) const {
        if (const auto* global = sema::FindGlobalSummary(sema_module_, name); global != nullptr) {
            return global->type;
        }
        if (const auto* function = sema::FindFunctionSignature(sema_module_, name); function != nullptr) {
            return sema::ProcedureType(function->param_types, function->return_types);
        }
        const std::string qualified_type = VariantTypeName(name);
        const std::string leaf_name = VariantLeafName(name);
        if (!qualified_type.empty() && qualified_type != std::string(name)) {
            if (const auto* type_decl = sema::FindTypeDecl(sema_module_, qualified_type);
                type_decl != nullptr && type_decl->kind == Decl::Kind::kEnum) {
                for (const auto& variant : type_decl->variants) {
                    if (variant.name == leaf_name) {
                        return sema::NamedType(qualified_type);
                    }
                }
            }
        }
        return sema::UnknownType();
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
        if (selector_type.kind != sema::Type::Kind::kNamed) {
            return sema::UnknownType();
        }

        const auto* type_decl = sema::FindTypeDecl(sema_module_, selector_type.name);
        if (type_decl == nullptr) {
            return sema::UnknownType();
        }

        const std::string qualified_name = VariantTypeName(variant_name);
        const std::string leaf_name = VariantLeafName(variant_name);
        if (!qualified_name.empty() && qualified_name != selector_type.name) {
            return sema::UnknownType();
        }

        for (const auto& variant : type_decl->variants) {
            if (variant.name != leaf_name) {
                continue;
            }
            if (payload_index >= variant.payload_fields.size()) {
                return sema::UnknownType();
            }
            return variant.payload_fields[payload_index].second;
        }

        return sema::UnknownType();
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
                const sema::Type type = KnownTypeOrError(InferSymbolRefType(expr.text), expr.span, "symbol reference type for " + expr.text);
                Emit({
                    .kind = Instruction::Kind::kSymbolRef,
                    .result = value,
                    .type = type,
                    .target = expr.text,
                    .target_kind = InferSymbolRefKind(expr.text),
                    .target_name = expr.text,
                });
                return {value, type};
            }
            case Expr::Kind::kQualifiedName: {
                if (local_types_.contains(expr.text)) {
                    const auto base = LoadLocalValue(expr.text);
                    const std::string value = NewValue();
                    const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "qualified field expression type");
                    Emit({
                        .kind = Instruction::Kind::kField,
                        .result = value,
                        .type = type,
                        .target = expr.secondary_text,
                        .operands = {base.value},
                    });
                    return {value, type};
                }
                const std::string value = NewValue();
                const std::string qualified_name = CombineQualifiedName(expr);
                const sema::Type type = KnownTypeOrError(InferSymbolRefType(qualified_name), expr.span, "symbol reference type for " + qualified_name);
                Emit({
                    .kind = Instruction::Kind::kSymbolRef,
                    .result = value,
                    .type = type,
                    .target = qualified_name,
                    .target_kind = InferSymbolRefKind(qualified_name),
                    .target_name = qualified_name,
                });
                return {value, type};
            }
            case Expr::Kind::kLiteral:
                return EmitLiteralValue(expr.text, KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "literal expression type"));
            case Expr::Kind::kUnary: {
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
            case Expr::Kind::kBinary:
                return EmitBinaryValue(expr.text,
                                       LowerExpr(*expr.left),
                                       LowerExpr(*expr.right),
                                       KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "binary expression type"));
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
                ValueInfo special_result;
                if (TryEmitSpecialCall(expr, args, type, true, &special_result)) {
                    return special_result;
                }
                const auto callee = LowerCalleeExpr(*expr.left, InferCalleeTypeFromCallSite(expr, type));
                std::vector<std::string> operands = {callee.value};
                for (const auto& arg : args) {
                    operands.push_back(arg.value);
                }
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
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "field expression type");
                Emit({
                    .kind = Instruction::Kind::kField,
                    .result = value,
                    .type = type,
                    .target = expr.text.empty() ? RenderExprInline(expr) : expr.text,
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
                Emit({
                    .kind = Instruction::Kind::kIndex,
                    .result = value,
                    .type = type,
                    .operands = {base.value, index.value},
                });
                return {value, type};
            }
            case Expr::Kind::kSlice: {
                const auto base = LowerExpr(*expr.left);
                std::optional<ValueInfo> lower;
                std::optional<ValueInfo> upper;
                std::vector<std::string> operands = {base.value};
                if (expr.right != nullptr) {
                    lower = LowerExpr(*expr.right);
                    operands.push_back(lower->value);
                }
                if (expr.extra != nullptr) {
                    upper = LowerExpr(*expr.extra);
                    operands.push_back(upper->value);
                }
                EmitSliceBoundsCheck(base, lower, upper);
                const std::string value = NewValue();
                const sema::Type type = KnownTypeOrError(ExprTypeOrUnknown(expr), expr.span, "slice expression type");
                Emit({
                    .kind = Instruction::Kind::kSlice,
                    .result = value,
                    .type = type,
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

        Report(expr.span, "bootstrap MIR encountered unsupported expression form");
        return EmitLiteralValue("0", sema::VoidType());
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
                .target_kind = target.kind == Expr::Kind::kField ? Instruction::TargetKind::kField
                              : target.kind == Expr::Kind::kDerefField ? Instruction::TargetKind::kDerefField
                              : target.kind == Expr::Kind::kIndex ? Instruction::TargetKind::kIndex
                                                                  : Instruction::TargetKind::kOther,
                .target_name = (target.kind == Expr::Kind::kField || target.kind == Expr::Kind::kDerefField) ? target.text : std::string(),
                .target_base_type = target.left != nullptr ? ExprTypeOrUnknown(*target.left) : sema::UnknownType(),
                .target_aux_types = target.kind == Expr::Kind::kIndex && target.right != nullptr ? std::vector<sema::Type> {ExprTypeOrUnknown(*target.right)}
                                                                                                  : std::vector<sema::Type> {},
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

        const sema::Type loop_type = RangeElementType(ExprTypeOrUnknown(*stmt.exprs.front()));
        if (sema::IsUnknown(loop_type)) {
            Report(stmt.span, "bootstrap MIR for-range requires a sema-known range element type");
            return;
        }

        const auto start = LowerExpr(*stmt.exprs.front()->left);
        const auto end = LowerExpr(*stmt.exprs.front()->right);
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
        const auto one = EmitLiteralValue("1", sema::IntLiteralType());
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
            Report(stmt.span, "bootstrap MIR foreach requires a sema-known iterable element type");
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
        const std::string item_value = NewValue();
        Emit({
            .kind = Instruction::Kind::kIndex,
            .result = item_value,
            .type = element_type,
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
                VariantDecl mir_variant;
                mir_variant.name = variant.name;
                for (const auto& field : variant.payload_fields) {
                    mir_variant.payload_fields.emplace_back(field.name, sema::TypeFromAst(field.type.get()));
                }
                type_decl.variants.push_back(std::move(mir_variant));
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
            module->functions.push_back(FunctionLowerer(decl, *module, file_path, sema_module, diagnostics).Run());
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
        std::unordered_map<std::string, sema::Type> value_types;
        std::unordered_map<std::string, sema::Type> local_types;
        for (const auto& local : function.locals) {
            local_types[local.name] = local.type;
        }
        for (const auto& block : function.blocks) {
            if (!reachable_blocks.contains(block.label)) {
                continue;
            }

            for (std::size_t instruction_index = 0; instruction_index < block.instructions.size(); ++instruction_index) {
                const auto& instruction = block.instructions[instruction_index];
                const Instruction* previous_instruction = instruction_index == 0 ? nullptr : &block.instructions[instruction_index - 1];
                std::vector<sema::Type> operand_types;
                operand_types.reserve(instruction.operands.size());
                for (const auto& operand : instruction.operands) {
                    if (!operand.empty() && !defined_values.contains(operand)) {
                        report("undefined MIR operand in function " + function.name + ": " + operand);
                        operand_types.push_back(sema::UnknownType());
                        continue;
                    }
                    const auto found_type = value_types.find(operand);
                    operand_types.push_back(found_type == value_types.end() ? sema::UnknownType() : found_type->second);
                }

                switch (instruction.kind) {
                    case Instruction::Kind::kLoadLocal: {
                        if (!instruction.operands.empty()) {
                            report("load_local must not take operands in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("load_local must name a local target in function " + function.name);
                            break;
                        }
                        const auto found_local = local_types.find(instruction.target);
                        if (found_local == local_types.end()) {
                            report("load_local references unknown local in function " + function.name + ": " + instruction.target);
                            break;
                        }
                        if (instruction.type != found_local->second) {
                            report("load_local type mismatch in function " + function.name + " for " + instruction.target + ": expected " +
                                   sema::FormatType(found_local->second) + ", got " + sema::FormatType(instruction.type));
                        }
                        break;
                    }
                    case Instruction::Kind::kStoreLocal: {
                        if (instruction.operands.size() != 1) {
                            report("store_local must use exactly one operand in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("store_local must name a local target in function " + function.name);
                            break;
                        }
                        const auto found_local = local_types.find(instruction.target);
                        if (found_local == local_types.end()) {
                            report("store_local references unknown local in function " + function.name + ": " + instruction.target);
                            break;
                        }
                        if (instruction.type != found_local->second) {
                            report("store_local declared type mismatch in function " + function.name + " for " + instruction.target + ": expected " +
                                   sema::FormatType(found_local->second) + ", got " + sema::FormatType(instruction.type));
                        }
                        if (operand_types.size() == 1 && !IsAssignableType(found_local->second, operand_types.front())) {
                            report("store_local operand type mismatch in function " + function.name + " for " + instruction.target + ": expected " +
                                   sema::FormatType(found_local->second) + ", got " + sema::FormatType(operand_types.front()));
                        }
                        break;
                    }
                    case Instruction::Kind::kStoreTarget:
                        if (instruction.operands.size() != 1) {
                            report("store_target must use exactly one operand in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("store_target must name a target in function " + function.name);
                        }
                        if (operand_types.size() == 1 && !sema::IsUnknown(instruction.type) && !IsAssignableType(instruction.type, operand_types.front())) {
                            report("store_target declared type mismatch in function " + function.name + ": expected " +
                                   sema::FormatType(instruction.type) + ", got " + sema::FormatType(operand_types.front()));
                        }
                        switch (instruction.target_kind) {
                            case Instruction::TargetKind::kField:
                            case Instruction::TargetKind::kDerefField: {
                                if (instruction.target_name.empty()) {
                                    report("store_target field access must name a field in function " + function.name);
                                    break;
                                }
                                if (instruction.target_base_type.kind != sema::Type::Kind::kNamed) {
                                    if (!sema::IsUnknown(instruction.target_base_type)) {
                                        report("store_target field access requires named aggregate base in function " + function.name);
                                    }
                                    break;
                                }
                                const TypeDecl* type_decl = FindMirTypeDecl(module, instruction.target_base_type.name);
                                if (type_decl == nullptr) {
                                    report("store_target field base type is unknown in function " + function.name + ": " +
                                           instruction.target_base_type.name);
                                    break;
                                }
                                bool found_field = false;
                                for (const auto& field : type_decl->fields) {
                                    if (field.first == instruction.target_name) {
                                        found_field = true;
                                        if (operand_types.size() == 1 && !IsAssignableType(field.second, operand_types.front())) {
                                            report("store_target field type mismatch in function " + function.name + " for " +
                                                   instruction.target_name + ": expected " + sema::FormatType(field.second) + ", got " +
                                                   sema::FormatType(operand_types.front()));
                                        }
                                        break;
                                    }
                                }
                                if (!found_field) {
                                    report("store_target field is unknown in function " + function.name + ": " + instruction.target_name);
                                }
                                break;
                            }
                            case Instruction::TargetKind::kIndex: {
                                sema::Type element_type = sema::UnknownType();
                                if (instruction.target_base_type.kind == sema::Type::Kind::kArray && !instruction.target_base_type.subtypes.empty()) {
                                    element_type = instruction.target_base_type.subtypes.front();
                                }
                                if (instruction.target_base_type.kind == sema::Type::Kind::kNamed &&
                                    (instruction.target_base_type.name == "Slice" || instruction.target_base_type.name == "Buffer") &&
                                    !instruction.target_base_type.subtypes.empty()) {
                                    element_type = instruction.target_base_type.subtypes.front();
                                }
                                if (sema::IsUnknown(element_type) && !sema::IsUnknown(instruction.target_base_type)) {
                                    report("store_target index requires array, slice, or buffer base in function " + function.name);
                                }
                                if (!instruction.target_aux_types.empty() && instruction.target_aux_types.front() != sema::NamedType("usize") &&
                                    instruction.target_aux_types.front().kind != sema::Type::Kind::kIntLiteral) {
                                    report("store_target index operand must be usize-compatible in function " + function.name);
                                }
                                if (operand_types.size() == 1 && !sema::IsUnknown(element_type) && !IsAssignableType(element_type, operand_types.front())) {
                                    report("store_target indexed element type mismatch in function " + function.name + ": expected " +
                                           sema::FormatType(element_type) + ", got " + sema::FormatType(operand_types.front()));
                                }
                                break;
                            }
                            case Instruction::TargetKind::kNone:
                            case Instruction::TargetKind::kFunction:
                            case Instruction::TargetKind::kGlobal:
                            case Instruction::TargetKind::kOther:
                                break;
                        }
                        break;
                    case Instruction::Kind::kUnary: {
                        if (instruction.operands.size() != 1) {
                            report("unary must use exactly one operand in function " + function.name);
                        }
                        if (instruction.op == "!" && !IsBoolType(instruction.type)) {
                            report("logical unary must produce bool in function " + function.name);
                        }
                        if (instruction.op == "!" && operand_types.size() == 1 && !IsBoolType(operand_types.front()) && !sema::IsUnknown(operand_types.front())) {
                            report("logical unary requires bool operand in function " + function.name);
                        }
                        if (instruction.op == "&" && instruction.type.kind == sema::Type::Kind::kPointer && !instruction.type.subtypes.empty() &&
                            operand_types.size() == 1 && !IsAssignableType(instruction.type.subtypes.front(), operand_types.front())) {
                            report("address-of unary result type mismatch in function " + function.name);
                        }
                        if (instruction.op == "&" && !sema::IsUnknown(instruction.type) && instruction.type.kind != sema::Type::Kind::kPointer) {
                            report("address-of unary must produce pointer type in function " + function.name);
                        }
                        const sema::Type deref_operand = operand_types.size() == 1 ? StripMirAliasOrDistinct(module, operand_types.front()) : sema::UnknownType();
                        if (instruction.op == "*" && operand_types.size() == 1 && !sema::IsUnknown(deref_operand) &&
                            deref_operand.kind != sema::Type::Kind::kPointer) {
                            report("pointer dereference requires pointer operand in function " + function.name);
                        }
                        if (instruction.op == "*" && operand_types.size() == 1 && deref_operand.kind == sema::Type::Kind::kPointer &&
                            !deref_operand.subtypes.empty() && instruction.type != deref_operand.subtypes.front()) {
                            report("pointer dereference result type mismatch in function " + function.name);
                        }
                        break;
                    }
                    case Instruction::Kind::kBoundsCheck:
                        if (!instruction.result.empty()) {
                            report("bounds_check must not produce a result in function " + function.name);
                        }
                        if (instruction.op == "index") {
                            if (instruction.operands.size() != 2) {
                                report("bounds_check index must use index and length operands in function " + function.name);
                                break;
                            }
                        } else if (instruction.op == "slice") {
                            if (instruction.operands.size() != 2 && instruction.operands.size() != 3) {
                                report("bounds_check slice must use bound/length or start/end/length operands in function " + function.name);
                                break;
                            }
                        } else {
                            report("bounds_check must record index or slice mode in function " + function.name);
                            break;
                        }
                        for (const auto& operand_type : operand_types) {
                            if (!sema::IsUnknown(operand_type) && !IsUsizeCompatibleType(StripMirAliasOrDistinct(module, operand_type))) {
                                report("bounds_check operands must be usize-compatible in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kDivCheck:
                        if (!instruction.result.empty()) {
                            report("div_check must not produce a result in function " + function.name);
                        }
                        if (instruction.op != "/" && instruction.op != "%") {
                            report("div_check must record '/' or '%' mode in function " + function.name);
                        }
                        if (instruction.operands.size() != 2) {
                            report("div_check must use dividend and divisor operands in function " + function.name);
                            break;
                        }
                        for (const auto& operand_type : operand_types) {
                            if (!sema::IsUnknown(operand_type) && !IsNumericType(operand_type)) {
                                report("div_check operands must be numeric in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kShiftCheck:
                        if (!instruction.result.empty()) {
                            report("shift_check must not produce a result in function " + function.name);
                        }
                        if (instruction.op != "<<" && instruction.op != ">>") {
                            report("shift_check must record '<<' or '>>' mode in function " + function.name);
                        }
                        if (instruction.operands.size() != 2) {
                            report("shift_check must use value and count operands in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 2) {
                            if (!sema::IsUnknown(operand_types.front()) && !IsIntegerLikeType(operand_types.front())) {
                                report("shift_check value operand must be integer-typed in function " + function.name);
                            }
                            if (!sema::IsUnknown(operand_types.back()) && !IsIntegerLikeType(operand_types.back())) {
                                report("shift_check count operand must be integer-typed in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kBinary: {
                        if (instruction.operands.size() != 2) {
                            report("binary must use exactly two operands in function " + function.name);
                        }
                        const sema::Type binary_result_type = StripMirAliasOrDistinct(module, instruction.type);
                        const bool requires_wrap_semantics = IsWraparoundBinaryOp(instruction.op) && !sema::IsUnknown(binary_result_type) &&
                                                            IsIntegerType(binary_result_type);
                        if (requires_wrap_semantics &&
                            instruction.arithmetic_semantics != Instruction::ArithmeticSemantics::kWrap) {
                            report("integer arithmetic binary must record wrap semantics in function " + function.name);
                        }
                        if (instruction.arithmetic_semantics == Instruction::ArithmeticSemantics::kWrap &&
                            (!IsWraparoundBinaryOp(instruction.op) || !sema::IsUnknown(binary_result_type) && !IsIntegerType(binary_result_type))) {
                            report("wrap semantics are only valid on integer +, -, or * binaries in function " + function.name);
                        }
                        if ((instruction.op == "==" || instruction.op == "!=" || instruction.op == "<" || instruction.op == "<=" || instruction.op == ">" ||
                             instruction.op == ">=" || instruction.op == "&&" || instruction.op == "||") &&
                            !IsBoolType(instruction.type)) {
                            report("comparison/logical binary must produce bool in function " + function.name);
                        }
                        if ((instruction.op == "==" || instruction.op == "!=" || instruction.op == "<" || instruction.op == "<=" || instruction.op == ">" ||
                             instruction.op == ">=") && operand_types.size() == 2 &&
                            !HasCompatibleComparisonTypes(operand_types.front(), operand_types.back())) {
                            report("comparison requires compatible operand types in function " + function.name);
                        }
                        if (instruction.op == "<<" || instruction.op == ">>") {
                            if (operand_types.size() == 2) {
                                if (!sema::IsUnknown(operand_types.front()) && !IsIntegerLikeType(operand_types.front())) {
                                    report("shift binary requires integer left operand in function " + function.name);
                                }
                                if (!sema::IsUnknown(operand_types.back()) && !IsIntegerLikeType(operand_types.back())) {
                                    report("shift binary requires integer right operand in function " + function.name);
                                }
                                if (!sema::IsUnknown(operand_types.front()) && instruction.type != operand_types.front()) {
                                    report("shift binary must preserve the left operand type in function " + function.name);
                                }
                            }
                            if (previous_instruction == nullptr || previous_instruction->kind != Instruction::Kind::kShiftCheck ||
                                previous_instruction->op != instruction.op || previous_instruction->operands != instruction.operands) {
                                report("shift binary must be preceded by matching shift_check in function " + function.name);
                            }
                        }
                        if (instruction.op == "&&" || instruction.op == "||") {
                            for (const auto& operand_type : operand_types) {
                                if (!sema::IsUnknown(operand_type) && !IsBoolType(operand_type)) {
                                    report("logical binary requires bool operands in function " + function.name);
                                }
                            }
                        }
                        if (instruction.op == "+" || instruction.op == "-" || instruction.op == "*" || instruction.op == "/" || instruction.op == "%") {
                            for (const auto& operand_type : operand_types) {
                                if (!sema::IsUnknown(operand_type) && !IsNumericType(operand_type)) {
                                    report("arithmetic binary requires numeric operands in function " + function.name);
                                }
                            }
                            if (!sema::IsUnknown(instruction.type) && !IsNumericType(instruction.type)) {
                                report("arithmetic binary must produce numeric type in function " + function.name);
                            }
                            if ((instruction.op == "/" || instruction.op == "%") &&
                                (previous_instruction == nullptr || previous_instruction->kind != Instruction::Kind::kDivCheck ||
                                 previous_instruction->op != instruction.op || previous_instruction->operands != instruction.operands)) {
                                report("division and remainder must be preceded by matching div_check in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kConvert:
                        if (instruction.operands.size() != 1) {
                            report("convert must use exactly one operand in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("convert must name a target type in function " + function.name);
                        }
                        if (!sema::IsUnknown(instruction.type) && instruction.target != sema::FormatType(instruction.type)) {
                            report("convert target metadata must match result type in function " + function.name);
                        }
                        if (operand_types.size() == 1 &&
                            ClassifyMirConversion(module, operand_types.front(), instruction.type) != ExplicitConversionKind::kGeneric) {
                            report("convert must use a dedicated conversion opcode when one exists in function " + function.name);
                        }
                        break;
                    case Instruction::Kind::kConvertNumeric:
                        if (instruction.operands.size() != 1) {
                            report("convert_numeric must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 1 && (!IsNumericType(operand_types.front()) || !IsNumericType(instruction.type))) {
                            report("convert_numeric requires numeric source and target types in function " + function.name);
                        }
                        break;
                    case Instruction::Kind::kConvertDistinct: {
                        if (instruction.operands.size() != 1) {
                            report("convert_distinct must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (operand_types.empty()) {
                            break;
                        }
                        const sema::Type stripped_source = StripMirAliasOrDistinct(module, operand_types.front());
                        const sema::Type stripped_target = StripMirAliasOrDistinct(module, instruction.type);
                        if (stripped_source != stripped_target || (stripped_source == operand_types.front() && stripped_target == instruction.type)) {
                            report("convert_distinct requires representation-preserving distinct or alias conversion in function " + function.name);
                        }
                        break;
                    }
                    case Instruction::Kind::kPointerToInt:
                        if (instruction.operands.size() != 1) {
                            report("pointer_to_int must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 1 && (!IsPointerLikeType(StripMirAliasOrDistinct(module, operand_types.front())) ||
                                                         !IsUintPtrType(StripMirAliasOrDistinct(module, instruction.type)))) {
                            report("pointer_to_int requires pointer source and uintptr target in function " + function.name);
                        }
                        break;
                    case Instruction::Kind::kIntToPointer:
                        if (instruction.operands.size() != 1) {
                            report("int_to_pointer must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 1 && (!IsUintPtrType(StripMirAliasOrDistinct(module, operand_types.front())) ||
                                                         !IsPointerLikeType(StripMirAliasOrDistinct(module, instruction.type)))) {
                            report("int_to_pointer requires uintptr source and pointer target in function " + function.name);
                        }
                        break;
                    case Instruction::Kind::kArrayToSlice:
                        if (instruction.operands.size() != 1) {
                            report("array_to_slice must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 1) {
                            if (operand_types.front().kind != sema::Type::Kind::kArray || instruction.type.kind != sema::Type::Kind::kNamed ||
                                instruction.type.name != "Slice") {
                                report("array_to_slice requires array source and Slice target in function " + function.name);
                            } else if (!operand_types.front().subtypes.empty() && !instruction.type.subtypes.empty() &&
                                       !IsAssignableType(instruction.type.subtypes.front(), operand_types.front().subtypes.front())) {
                                report("array_to_slice element type mismatch in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kBufferToSlice:
                        if (instruction.operands.size() != 1) {
                            report("buffer_to_slice must use exactly one operand in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 1) {
                            if (operand_types.front().kind != sema::Type::Kind::kNamed || operand_types.front().name != "Buffer" ||
                                instruction.type.kind != sema::Type::Kind::kNamed || instruction.type.name != "Slice") {
                                report("buffer_to_slice requires Buffer source and Slice target in function " + function.name);
                            } else if (!operand_types.front().subtypes.empty() && !instruction.type.subtypes.empty() &&
                                       !IsAssignableType(instruction.type.subtypes.front(), operand_types.front().subtypes.front())) {
                                report("buffer_to_slice element type mismatch in function " + function.name);
                            }
                        }
                        break;
                    case Instruction::Kind::kCall: {
                        if (instruction.operands.empty()) {
                            report("call must include a callee operand in function " + function.name);
                            break;
                        }
                        if (IsDedicatedCallSurface(instruction.target)) {
                            report("call must use a dedicated volatile/atomic opcode when one exists in function " + function.name);
                        }
                        const sema::Type& callee_type = operand_types.front();
                        if (!sema::IsUnknown(callee_type) && callee_type.kind != sema::Type::Kind::kProcedure) {
                            report("call callee must have procedure type in function " + function.name);
                            break;
                        }
                        if (callee_type.kind == sema::Type::Kind::kProcedure) {
                            const std::size_t param_count = ProcedureParamCount(callee_type);
                            if (instruction.operands.size() - 1 != param_count) {
                                report("call argument count mismatch in function " + function.name + ": expected " + std::to_string(param_count) +
                                       ", got " + std::to_string(instruction.operands.size() - 1));
                            }
                            for (std::size_t index = 0; index < param_count && index + 1 < operand_types.size(); ++index) {
                                const sema::Type& expected = callee_type.subtypes[index];
                                const sema::Type& actual = operand_types[index + 1];
                                if (!IsAssignableType(expected, actual)) {
                                    report("call argument type mismatch in function " + function.name + ": expected " +
                                           sema::FormatType(expected) + ", got " + sema::FormatType(actual));
                                }
                            }
                            if (instruction.type != ProcedureResultType(callee_type)) {
                                report("call result type mismatch in function " + function.name + ": expected " +
                                       sema::FormatType(ProcedureResultType(callee_type)) + ", got " + sema::FormatType(instruction.type));
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kVolatileLoad: {
                        if (instruction.result.empty()) {
                            report("volatile_load must produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 1) {
                            report("volatile_load must use exactly one pointer operand in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 1) {
                            const auto pointee = PointerPointeeType(StripMirAliasOrDistinct(module, operand_types.front()));
                            if (!pointee.has_value()) {
                                if (!sema::IsUnknown(operand_types.front())) {
                                    report("volatile_load requires pointer operand in function " + function.name);
                                }
                            } else if (!IsAssignableType(instruction.type, *pointee)) {
                                report("volatile_load result type mismatch in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kVolatileStore: {
                        if (!instruction.result.empty()) {
                            report("volatile_store must not produce a result in function " + function.name);
                        }
                        if (instruction.operands.size() != 2) {
                            report("volatile_store must use pointer and value operands in function " + function.name);
                            break;
                        }
                        if (operand_types.size() == 2) {
                            const auto pointee = PointerPointeeType(StripMirAliasOrDistinct(module, operand_types.front()));
                            if (!pointee.has_value()) {
                                if (!sema::IsUnknown(operand_types.front())) {
                                    report("volatile_store requires pointer operand in function " + function.name);
                                }
                            } else if (!IsAssignableType(*pointee, operand_types[1])) {
                                report("volatile_store value type mismatch in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kAtomicLoad:
                    case Instruction::Kind::kAtomicStore:
                    case Instruction::Kind::kAtomicExchange:
                    case Instruction::Kind::kAtomicCompareExchange:
                    case Instruction::Kind::kAtomicFetchAdd: {
                        const bool is_load = instruction.kind == Instruction::Kind::kAtomicLoad;
                        const bool is_store = instruction.kind == Instruction::Kind::kAtomicStore;
                        const bool is_exchange = instruction.kind == Instruction::Kind::kAtomicExchange;
                        const bool is_compare_exchange = instruction.kind == Instruction::Kind::kAtomicCompareExchange;
                        const bool is_fetch_add = instruction.kind == Instruction::Kind::kAtomicFetchAdd;
                        const std::size_t expected_operands = is_load ? 2 : (is_store || is_exchange || is_fetch_add ? 3 : 5);
                        if (instruction.operands.size() != expected_operands) {
                            report(std::string(ToString(instruction.kind)) + " operand count mismatch in function " + function.name);
                            break;
                        }
                        if ((is_load || is_exchange || is_fetch_add) && instruction.result.empty()) {
                            report(std::string(ToString(instruction.kind)) + " must produce a result in function " + function.name);
                        }
                        if (is_store && !instruction.result.empty()) {
                            report("atomic_store must not produce a result in function " + function.name);
                        }
                        if (is_compare_exchange && !IsBoolType(instruction.type)) {
                            report("atomic_compare_exchange must produce bool in function " + function.name);
                        }
                        if (operand_types.empty()) {
                            break;
                        }
                        const auto atomic_element = AtomicElementType(module, operand_types.front());
                        if (!atomic_element.has_value()) {
                            if (!sema::IsUnknown(operand_types.front())) {
                                report(std::string(ToString(instruction.kind)) + " requires *Atomic<T> operand in function " + function.name);
                            }
                            break;
                        }
                        const auto require_order = [&](std::size_t index, const std::string& label) {
                            if (index < operand_types.size() && !sema::IsUnknown(operand_types[index]) && !IsMemoryOrderType(operand_types[index])) {
                                report(label + " must use MemoryOrder operand in function " + function.name);
                            }
                        };
                        if (is_load) {
                            if (!IsAssignableType(instruction.type, *atomic_element)) {
                                report("atomic_load result type mismatch in function " + function.name);
                            }
                            require_order(1, "atomic_load");
                        }
                        if (is_store) {
                            if (operand_types.size() > 1 && !IsAssignableType(*atomic_element, operand_types[1])) {
                                report("atomic_store value type mismatch in function " + function.name);
                            }
                            require_order(2, "atomic_store");
                        }
                        if (is_exchange) {
                            if (!IsAssignableType(instruction.type, *atomic_element)) {
                                report("atomic_exchange result type mismatch in function " + function.name);
                            }
                            if (operand_types.size() > 1 && !IsAssignableType(*atomic_element, operand_types[1])) {
                                report("atomic_exchange value type mismatch in function " + function.name);
                            }
                            require_order(2, "atomic_exchange");
                        }
                        if (is_compare_exchange) {
                            if (operand_types.size() > 1) {
                                const auto expected_pointee = PointerPointeeType(StripMirAliasOrDistinct(module, operand_types[1]));
                                if (!expected_pointee.has_value() || !IsAssignableType(*expected_pointee, *atomic_element)) {
                                    report("atomic_compare_exchange expected pointer type mismatch in function " + function.name);
                                }
                            }
                            if (operand_types.size() > 2 && !IsAssignableType(*atomic_element, operand_types[2])) {
                                report("atomic_compare_exchange desired value type mismatch in function " + function.name);
                            }
                            require_order(3, "atomic_compare_exchange success order");
                            require_order(4, "atomic_compare_exchange failure order");
                        }
                        if (is_fetch_add) {
                            if (!IsAssignableType(instruction.type, *atomic_element)) {
                                report("atomic_fetch_add result type mismatch in function " + function.name);
                            }
                            if (operand_types.size() > 1 && !IsAssignableType(*atomic_element, operand_types[1])) {
                                report("atomic_fetch_add value type mismatch in function " + function.name);
                            }
                            require_order(2, "atomic_fetch_add");
                        }
                        break;
                    }
                    case Instruction::Kind::kField: {
                        if (instruction.operands.size() != 1) {
                            report("field must use exactly one base operand in function " + function.name);
                            break;
                        }
                        const sema::Type& base_type = operand_types.front();
                        if (instruction.target == "len") {
                            const bool len_ok = (base_type.kind == sema::Type::Kind::kNamed &&
                                                 (base_type.name == "Slice" || base_type.name == "Buffer" || base_type.name == "str" ||
                                                  base_type.name == "string")) ||
                                                base_type.kind == sema::Type::Kind::kString;
                            if (!sema::IsUnknown(base_type) && !len_ok) {
                                report("field len requires slice, buffer, or string base in function " + function.name);
                            }
                            if (instruction.type != sema::NamedType("usize")) {
                                report("field len must produce usize in function " + function.name);
                            }
                        } else if (base_type.kind == sema::Type::Kind::kNamed) {
                            const TypeDecl* type_decl = FindMirTypeDecl(module, base_type.name);
                            if (type_decl != nullptr) {
                                bool found_field = false;
                                for (const auto& field : type_decl->fields) {
                                    if (field.first == instruction.target) {
                                        found_field = true;
                                        if (instruction.type != field.second) {
                                            report("field result type mismatch in function " + function.name + " for " + instruction.target);
                                        }
                                        break;
                                    }
                                }
                                if (!found_field) {
                                    report("field references unknown member in function " + function.name + ": " + instruction.target);
                                }
                            } else if (IsBuiltinNamedNonAggregate(base_type)) {
                                report("field requires named aggregate base in function " + function.name);
                            }
                        } else if (!sema::IsUnknown(base_type)) {
                            report("field requires named aggregate base in function " + function.name);
                        }
                        break;
                    }
                    case Instruction::Kind::kIndex: {
                        if (instruction.operands.size() != 2) {
                            report("index must use base and index operands in function " + function.name);
                            break;
                        }
                        const sema::Type& base_type = operand_types.front();
                        const sema::Type& index_type = operand_types[1];
                        sema::Type expected_type = sema::UnknownType();
                        if (base_type.kind == sema::Type::Kind::kArray && !base_type.subtypes.empty()) {
                            expected_type = base_type.subtypes.front();
                        }
                        if (base_type.kind == sema::Type::Kind::kNamed && (base_type.name == "Slice" || base_type.name == "Buffer") &&
                            !base_type.subtypes.empty()) {
                            expected_type = base_type.subtypes.front();
                        }
                        if (sema::IsUnknown(expected_type) && !sema::IsUnknown(base_type)) {
                            report("index requires array, slice, or buffer base in function " + function.name);
                        }
                        if (!sema::IsUnknown(expected_type) && instruction.type != expected_type) {
                            report("index result type mismatch in function " + function.name + ": expected " + sema::FormatType(expected_type) +
                                   ", got " + sema::FormatType(instruction.type));
                        }
                        if (!sema::IsUnknown(index_type) && index_type != sema::NamedType("usize") && index_type.kind != sema::Type::Kind::kIntLiteral) {
                            report("index operand must be usize-compatible in function " + function.name);
                        }
                        break;
                    }
                    case Instruction::Kind::kSlice: {
                        if (instruction.operands.empty() || instruction.operands.size() > 3) {
                            report("slice must use one to three operands in function " + function.name);
                            break;
                        }
                        const sema::Type& base_type = operand_types.front();
                        sema::Type expected_type = sema::UnknownType();
                        if (base_type.kind == sema::Type::Kind::kArray && !base_type.subtypes.empty()) {
                            expected_type = sema::NamedType("Slice");
                            expected_type.subtypes.push_back(base_type.subtypes.front());
                        }
                        if (base_type.kind == sema::Type::Kind::kNamed && base_type.name == "Slice") {
                            expected_type = base_type;
                        }
                        if (base_type.kind == sema::Type::Kind::kNamed && base_type.name == "Buffer") {
                            expected_type = sema::NamedType("Slice");
                            expected_type.subtypes = base_type.subtypes;
                        }
                        if (base_type.kind == sema::Type::Kind::kString) {
                            expected_type = sema::StringType();
                        }
                        if (sema::IsUnknown(expected_type) && !sema::IsUnknown(base_type)) {
                            report("slice requires array, slice, buffer, or string base in function " + function.name);
                        }
                        if (!sema::IsUnknown(expected_type) && instruction.type != expected_type) {
                            report("slice result type mismatch in function " + function.name + ": expected " + sema::FormatType(expected_type) +
                                   ", got " + sema::FormatType(instruction.type));
                        }
                        for (std::size_t index = 1; index < operand_types.size(); ++index) {
                            const sema::Type& bound_type = operand_types[index];
                            if (!sema::IsUnknown(bound_type) && bound_type != sema::NamedType("usize") &&
                                bound_type.kind != sema::Type::Kind::kIntLiteral) {
                                report("slice bounds must be usize-compatible in function " + function.name);
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kAggregateInit:
                        if (instruction.field_names.size() != instruction.operands.size()) {
                            report("aggregate_init field metadata mismatch in function " + function.name);
                        }
                        if (instruction.type.kind != sema::Type::Kind::kNamed) {
                            if (!sema::IsUnknown(instruction.type)) {
                                report("aggregate_init must produce a named aggregate type in function " + function.name);
                            }
                            break;
                        }
                        if (const TypeDecl* type_decl = FindMirTypeDecl(module, instruction.type.name); type_decl == nullptr) {
                            report("aggregate_init references unknown type in function " + function.name + ": " + instruction.type.name);
                        } else {
                            if (!instruction.target.empty() && instruction.target != sema::FormatType(instruction.type)) {
                                report("aggregate_init target metadata must match result type in function " + function.name);
                            }
                            std::unordered_set<std::string> seen_named_fields;
                            for (std::size_t index = 0; index < instruction.field_names.size() && index < operand_types.size(); ++index) {
                                const std::string& field_name = instruction.field_names[index];
                                sema::Type expected_type = sema::UnknownType();
                                if (field_name == "_") {
                                    if (index >= type_decl->fields.size()) {
                                        report("aggregate_init has too many positional fields in function " + function.name);
                                        continue;
                                    }
                                    expected_type = type_decl->fields[index].second;
                                } else {
                                    if (!seen_named_fields.insert(field_name).second) {
                                        report("aggregate_init has duplicate named field in function " + function.name + ": " + field_name);
                                        continue;
                                    }
                                    for (const auto& field : type_decl->fields) {
                                        if (field.first == field_name) {
                                            expected_type = field.second;
                                            break;
                                        }
                                    }
                                    if (sema::IsUnknown(expected_type)) {
                                        report("aggregate_init field is unknown in function " + function.name + ": " + field_name);
                                        continue;
                                    }
                                }
                                if (!IsAssignableType(expected_type, operand_types[index])) {
                                    report("aggregate_init field type mismatch in function " + function.name + " for " + field_name + ": expected " +
                                           sema::FormatType(expected_type) + ", got " + sema::FormatType(operand_types[index]));
                                }
                            }
                        }
                        break;
                    case Instruction::Kind::kVariantMatch:
                        if (instruction.operands.size() != 1) {
                            report("variant_match must use exactly one selector operand in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("variant_match must name a target variant in function " + function.name);
                        }
                        if (instruction.type != sema::BoolType()) {
                            report("variant_match must produce bool in function " + function.name);
                        }
                        if (operand_types.size() == 1 && operand_types.front().kind == sema::Type::Kind::kNamed) {
                            const TypeDecl* type_decl = FindMirTypeDecl(module, operand_types.front().name);
                            if (type_decl == nullptr) {
                                report("variant_match selector type is unknown in function " + function.name + ": " + operand_types.front().name);
                            } else if (FindMirVariantDecl(*type_decl, instruction.target) == nullptr) {
                                report("variant_match references unknown variant in function " + function.name + ": " + instruction.target);
                            }
                        }
                        break;
                    case Instruction::Kind::kVariantExtract: {
                        if (instruction.operands.size() != 1) {
                            report("variant_extract must use exactly one selector operand in function " + function.name);
                        }
                        if (instruction.target.empty()) {
                            report("variant_extract must name a target variant in function " + function.name);
                        }
                        if (instruction.op.empty()) {
                            report("variant_extract must record a payload index in function " + function.name);
                        }
                        bool payload_index_ok = false;
                        std::size_t payload_index = 0;
                        if (!instruction.op.empty()) {
                            try {
                                payload_index = static_cast<std::size_t>(std::stoul(instruction.op));
                                payload_index_ok = true;
                            } catch (const std::exception&) {
                                report("variant_extract payload index must be numeric in function " + function.name);
                            }
                        }
                        if (operand_types.size() == 1 && operand_types.front().kind == sema::Type::Kind::kNamed) {
                            const TypeDecl* type_decl = FindMirTypeDecl(module, operand_types.front().name);
                            if (type_decl == nullptr) {
                                report("variant_extract selector type is unknown in function " + function.name + ": " + operand_types.front().name);
                                break;
                            }
                            const VariantDecl* variant_decl = FindMirVariantDecl(*type_decl, instruction.target);
                            if (variant_decl == nullptr) {
                                report("variant_extract references unknown variant in function " + function.name + ": " + instruction.target);
                                break;
                            }
                            if (!payload_index_ok) {
                                break;
                            }
                            if (payload_index >= variant_decl->payload_fields.size()) {
                                report("variant_extract payload index out of range in function " + function.name + " for " + instruction.target);
                                break;
                            }
                            if (instruction.type != variant_decl->payload_fields[payload_index].second) {
                                report("variant_extract result type mismatch in function " + function.name + " for " + instruction.target + ": expected " +
                                       sema::FormatType(variant_decl->payload_fields[payload_index].second) + ", got " +
                                       sema::FormatType(instruction.type));
                            }
                        }
                        break;
                    }
                    case Instruction::Kind::kConst:
                        break;
                    case Instruction::Kind::kSymbolRef:
                        if (instruction.target_kind == Instruction::TargetKind::kFunction) {
                            const Function* callee = FindMirFunction(module, instruction.target_name.empty() ? instruction.target : instruction.target_name);
                            if (callee != nullptr && instruction.type != FunctionProcedureType(*callee)) {
                                report("symbol_ref function type mismatch in function " + function.name + " for " + callee->name + ": expected " +
                                       sema::FormatType(FunctionProcedureType(*callee)) + ", got " + sema::FormatType(instruction.type));
                            }
                        }
                        if (instruction.target_kind == Instruction::TargetKind::kGlobal) {
                            const GlobalDecl* global = FindMirGlobalDecl(module, instruction.target_name.empty() ? instruction.target : instruction.target_name);
                            if (global != nullptr && instruction.type != global->type) {
                                report("symbol_ref global type mismatch in function " + function.name + " for " +
                                       (instruction.target_name.empty() ? instruction.target : instruction.target_name) + ": expected " +
                                       sema::FormatType(global->type) + ", got " + sema::FormatType(instruction.type));
                            }
                        }
                        break;
                }

                if (!instruction.result.empty()) {
                    if (!defined_values.insert(instruction.result).second) {
                        report("duplicate MIR value in function " + function.name + ": " + instruction.result);
                    }
                    value_types[instruction.result] = instruction.type;
                }
            }

            switch (block.terminator.kind) {
                case Terminator::Kind::kNone:
                    report("unterminated MIR block in function " + function.name + ": " + block.label);
                    break;
                case Terminator::Kind::kReturn:
                    if (block.terminator.values.size() != function.return_types.size()) {
                        report("return value count mismatch in function " + function.name + ": expected " +
                               std::to_string(function.return_types.size()) + ", got " + std::to_string(block.terminator.values.size()));
                    }
                    for (std::size_t index = 0; index < block.terminator.values.size(); ++index) {
                        const auto& value = block.terminator.values[index];
                        if (!defined_values.contains(value)) {
                            report("return uses undefined MIR value in function " + function.name + ": " + value);
                            continue;
                        }
                        if (index < function.return_types.size()) {
                            const auto found_type = value_types.find(value);
                            const sema::Type actual = found_type == value_types.end() ? sema::UnknownType() : found_type->second;
                            if (!IsAssignableType(function.return_types[index], actual)) {
                                report("return type mismatch in function " + function.name + ": expected " +
                                       sema::FormatType(function.return_types[index]) + ", got " + sema::FormatType(actual));
                            }
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
                    } else {
                        const auto found_type = value_types.find(block.terminator.values.front());
                        const sema::Type condition_type = found_type == value_types.end() ? sema::UnknownType() : found_type->second;
                        if (!sema::IsUnknown(condition_type) && !IsBoolType(condition_type)) {
                            report("conditional branch condition must have bool type in function " + function.name);
                        }
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
            WriteLine(stream, 2, "Variant name=" + variant.name);
            for (const auto& payload_field : variant.payload_fields) {
                WriteLine(stream, 3, "PayloadField name=" + payload_field.first + " type=" + sema::FormatType(payload_field.second));
            }
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
                if (instruction.arithmetic_semantics != Instruction::ArithmeticSemantics::kNone) {
                    line << " arithmetic=" << ToString(instruction.arithmetic_semantics);
                }
                if (!instruction.target.empty()) {
                    line << " target=" << instruction.target;
                }
                if (instruction.target_kind != Instruction::TargetKind::kNone) {
                    line << " target_kind=" << ToString(instruction.target_kind);
                }
                if (!instruction.target_name.empty()) {
                    line << " target_name=" << instruction.target_name;
                }
                if (!sema::IsUnknown(instruction.target_base_type)) {
                    line << " target_base=" << sema::FormatType(instruction.target_base_type);
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
                if (!instruction.target_aux_types.empty()) {
                    line << " target_types=[";
                    for (std::size_t index = 0; index < instruction.target_aux_types.size(); ++index) {
                        if (index > 0) {
                            line << ", ";
                        }
                        line << sema::FormatType(instruction.target_aux_types[index]);
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