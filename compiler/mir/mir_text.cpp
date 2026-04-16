#include "compiler/mir/mir_internal.h"

#include <cstddef>
#include <sstream>

namespace mc::mir {

namespace {

std::string RenderFieldInitName(const ast::FieldInit& init) {
    if (!init.field_path.empty()) {
        std::ostringstream stream;
        for (std::size_t index = 0; index < init.field_path.size(); ++index) {
            if (index > 0) {
                stream << '.';
            }
            stream << init.field_path[index];
        }
        return stream.str();
    }
    return init.name;
}

}  // namespace

using mc::ast::Expr;

constexpr std::string_view kTypeDeclKindNames[] = {"struct", "enum", "distinct", "alias"};
constexpr std::size_t kTypeDeclKindCount = static_cast<std::size_t>(TypeDecl::Kind::kAlias) + 1;
static_assert(sizeof(kTypeDeclKindNames) / sizeof(kTypeDeclKindNames[0]) == kTypeDeclKindCount);

constexpr std::string_view kInstructionKindNames[] = {"const",
                                                      "local_addr",
                                                      "arena_new",
                                                      "load_local",
                                                      "store_local",
                                                      "store_target",
                                                      "symbol_ref",
                                                      "bounds_check",
                                                      "div_check",
                                                      "shift_check",
                                                      "unary",
                                                      "binary",
                                                      "convert",
                                                      "convert_numeric",
                                                      "convert_distinct",
                                                      "pointer_to_int",
                                                      "int_to_pointer",
                                                      "array_to_slice",
                                                      "buffer_to_slice",
                                                      "buffer_new",
                                                      "buffer_free",
                                                      "slice_from_buffer",
                                                      "call",
                                                      "memory_barrier",
                                                      "volatile_load",
                                                      "volatile_store",
                                                      "atomic_load",
                                                      "atomic_store",
                                                      "atomic_exchange",
                                                      "atomic_compare_exchange",
                                                      "atomic_fetch_add",
                                                      "field",
                                                      "index",
                                                      "slice",
                                                      "aggregate_init",
                                                      "variant_init",
                                                      "variant_match",
                                                      "variant_extract"};
constexpr std::size_t kInstructionKindCount = static_cast<std::size_t>(Instruction::Kind::kVariantExtract) + 1;
static_assert(sizeof(kInstructionKindNames) / sizeof(kInstructionKindNames[0]) == kInstructionKindCount);

constexpr std::string_view kInstructionTargetKindNames[] = {"none", "function", "global", "field", "deref_field", "index", "other"};
constexpr std::size_t kInstructionTargetKindCount = static_cast<std::size_t>(Instruction::TargetKind::kOther) + 1;
static_assert(sizeof(kInstructionTargetKindNames) / sizeof(kInstructionTargetKindNames[0]) == kInstructionTargetKindCount);

constexpr std::string_view kInstructionArithmeticSemanticsNames[] = {"none", "wrap"};
constexpr std::size_t kInstructionArithmeticSemanticsCount =
    static_cast<std::size_t>(Instruction::ArithmeticSemantics::kWrap) + 1;
static_assert(sizeof(kInstructionArithmeticSemanticsNames) / sizeof(kInstructionArithmeticSemanticsNames[0]) ==
              kInstructionArithmeticSemanticsCount);

constexpr std::string_view kExprKindNames[] = {"name",
                                               "qualified-name",
                                               "literal",
                                               "unary",
                                               "binary",
                                               "range",
                                               "call",
                                               "field",
                                               "deref-field",
                                               "index",
                                               "slice",
                                               "aggregate-init",
                                               "record-update",
                                               "paren"};
constexpr std::size_t kExprKindCount = static_cast<std::size_t>(Expr::Kind::kParen) + 1;
static_assert(sizeof(kExprKindNames) / sizeof(kExprKindNames[0]) == kExprKindCount);

constexpr std::size_t TypeDeclKindIndex(TypeDecl::Kind kind) {
    return static_cast<std::size_t>(kind);
}

constexpr std::size_t InstructionKindIndex(Instruction::Kind kind) {
    return static_cast<std::size_t>(kind);
}

constexpr std::size_t InstructionTargetKindIndex(Instruction::TargetKind kind) {
    return static_cast<std::size_t>(kind);
}

constexpr std::size_t InstructionArithmeticSemanticsIndex(Instruction::ArithmeticSemantics semantics) {
    return static_cast<std::size_t>(semantics);
}

constexpr std::size_t ExprKindIndex(Expr::Kind kind) {
    return static_cast<std::size_t>(kind);
}

std::string RenderExprOrPlaceholder(const Expr* expr) {
    return expr != nullptr ? RenderExprInline(*expr) : std::string("<?>");
}

void AppendRenderedTypeArgs(std::ostringstream& stream, const std::vector<std::unique_ptr<ast::TypeExpr>>& type_args) {
    stream << '<';
    for (std::size_t index = 0; index < type_args.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << RenderTypeExprInline(*type_args[index]);
    }
    stream << '>';
}

std::string_view ToString(TypeDecl::Kind kind) {
    const std::size_t index = TypeDeclKindIndex(kind);
    return index < kTypeDeclKindCount ? kTypeDeclKindNames[index] : "type";
}

std::string_view ToString(Instruction::Kind kind) {
    const std::size_t index = InstructionKindIndex(kind);
    return index < kInstructionKindCount ? kInstructionKindNames[index] : "instr";
}

std::string_view ToString(Instruction::TargetKind kind) {
    const std::size_t index = InstructionTargetKindIndex(kind);
    return index < kInstructionTargetKindCount ? kInstructionTargetKindNames[index] : "none";
}

std::string_view ToString(Instruction::ArithmeticSemantics semantics) {
    const std::size_t index = InstructionArithmeticSemanticsIndex(semantics);
    return index < kInstructionArithmeticSemanticsCount ? kInstructionArithmeticSemanticsNames[index] : "none";
}

std::string_view ToString(Expr::Kind kind) {
    const std::size_t index = ExprKindIndex(kind);
    return index < kExprKindCount ? kExprKindNames[index] : "expr";
}

std::string VariantTypeName(std::string_view variant_name) {
    const std::size_t separator = variant_name.rfind('.');
    if (separator == std::string_view::npos) {
        return std::string(variant_name);
    }
    return std::string(variant_name.substr(0, separator));
}

std::string VariantLeafName(std::string_view variant_name) {
    const std::size_t separator = variant_name.rfind('.');
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

std::string RenderTypeExprInline(const ast::TypeExpr& type) {
    switch (type.kind) {
        case ast::TypeExpr::Kind::kNamed: {
            if (type.type_args.empty()) {
                return type.name;
            }
            std::ostringstream stream;
            stream << type.name << '<';
            for (std::size_t index = 0; index < type.type_args.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << RenderTypeExprInline(*type.type_args[index]);
            }
            stream << '>';
            return stream.str();
        }
        case ast::TypeExpr::Kind::kPointer:
            return "*" + (type.inner != nullptr ? RenderTypeExprInline(*type.inner) : std::string("<?>"));
        case ast::TypeExpr::Kind::kConst:
            return "const " + (type.inner != nullptr ? RenderTypeExprInline(*type.inner) : std::string("<?>"));
        case ast::TypeExpr::Kind::kArray:
            return "[" + (type.length_expr != nullptr ? RenderExprInline(*type.length_expr) : std::string("?")) + "]" +
                   (type.inner != nullptr ? RenderTypeExprInline(*type.inner) : std::string("<?>"));
        case ast::TypeExpr::Kind::kParen:
            return "(" + (type.inner != nullptr ? RenderTypeExprInline(*type.inner) : std::string("<?>")) + ")";
        case ast::TypeExpr::Kind::kProcedure: {
            std::ostringstream stream;
            stream << "func(";
            for (std::size_t index = 0; index < type.params.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                stream << RenderTypeExprInline(*type.params[index]);
            }
            stream << ')';
            if (type.returns.size() == 1) {
                stream << ' ' << RenderTypeExprInline(*type.returns.front());
            } else if (!type.returns.empty()) {
                stream << " (";
                for (std::size_t index = 0; index < type.returns.size(); ++index) {
                    if (index > 0) {
                        stream << ", ";
                    }
                    stream << RenderTypeExprInline(*type.returns[index]);
                }
                stream << ')';
            }
            return stream.str();
        }
    }
    return "<?>";
}

std::string RenderExprInline(const Expr& expr) {
    switch (expr.kind) {
        case Expr::Kind::kName:
        case Expr::Kind::kQualifiedName: {
            const std::string base = expr.kind == Expr::Kind::kQualifiedName ? CombineQualifiedName(expr) : expr.text;
            if (expr.type_args.empty()) {
                return base;
            }
            std::ostringstream stream;
            stream << base;
            AppendRenderedTypeArgs(stream, expr.type_args);
            return stream.str();
        }
        case Expr::Kind::kLiteral:
            return expr.text;
        case Expr::Kind::kUnary:
            return expr.text + RenderExprOrPlaceholder(expr.left.get());
        case Expr::Kind::kBinary:
        case Expr::Kind::kRange:
            return RenderExprOrPlaceholder(expr.left.get()) + " " + expr.text + " " + RenderExprOrPlaceholder(expr.right.get());
        case Expr::Kind::kCall: {
            std::ostringstream stream;
            stream << (expr.type_target != nullptr ? RenderTypeExprInline(*expr.type_target) : RenderExprOrPlaceholder(expr.left.get())) << '(';
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
            return RenderExprOrPlaceholder(expr.left.get()) + "." + expr.text;
        case Expr::Kind::kDerefField:
            return RenderExprOrPlaceholder(expr.left.get()) + ".*";
        case Expr::Kind::kIndex:
            return RenderExprOrPlaceholder(expr.left.get()) + "[" + RenderExprOrPlaceholder(expr.right.get()) + "]";
        case Expr::Kind::kSlice: {
            std::ostringstream stream;
            stream << RenderExprOrPlaceholder(expr.left.get()) << '[';
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
            stream << RenderExprOrPlaceholder(expr.left.get()) << '{';
            for (std::size_t index = 0; index < expr.field_inits.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                if (expr.field_inits[index].has_name) {
                    stream << RenderFieldInitName(expr.field_inits[index]) << ": ";
                }
                stream << RenderExprInline(*expr.field_inits[index].value);
            }
            stream << '}';
            return stream.str();
        }
        case Expr::Kind::kRecordUpdate: {
            std::ostringstream stream;
            stream << RenderExprOrPlaceholder(expr.left.get()) << " with {";
            for (std::size_t index = 0; index < expr.field_inits.size(); ++index) {
                if (index > 0) {
                    stream << ", ";
                }
                if (expr.field_inits[index].has_name) {
                    stream << RenderFieldInitName(expr.field_inits[index]) << ": ";
                }
                stream << RenderExprInline(*expr.field_inits[index].value);
            }
            stream << '}';
            return stream.str();
        }
        case Expr::Kind::kParen:
            return "(" + RenderExprOrPlaceholder(expr.left.get()) + ")";
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

}  // namespace mc::mir