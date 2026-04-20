#include "lorsc/Sema.h"

#include <sstream>

namespace lorsc {

namespace {

template <typename T, typename U>
T* dynCast(U* ptr) {
  return dynamic_cast<T*>(ptr);
}

template <typename T, typename U>
const T* dynCast(const U* ptr) {
  return dynamic_cast<const T*>(ptr);
}

}  // namespace

SemanticAnalyzer::SemanticAnalyzer(DiagnosticsEngine& diagnostics, std::string fileName)
    : diagnostics_(diagnostics), fileName_(std::move(fileName)) {}

bool SemanticAnalyzer::analyze(Program& program) {
  bool ok = registerFunctions(program);
  for (const auto& fn : program.functions) {
    ok = analyzeFunction(*fn) && ok;
  }
  return ok && !diagnostics_.hasErrors();
}

void SemanticAnalyzer::pushScope() { scopes_.emplace_back(); }

void SemanticAnalyzer::popScope() { scopes_.pop_back(); }

bool SemanticAnalyzer::declareVariable(const std::string& name, TypeKind type, SourceLocation loc) {
  auto& scope = scopes_.back();
  if (scope.find(name) != scope.end()) {
    diagnostics_.reportError(fileName_, loc, "redefinition of variable '" + name + "' in the same scope");
    return false;
  }
  scope[name] = type;
  return true;
}

TypeKind SemanticAnalyzer::lookupVariable(const std::string& name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto pos = it->find(name);
    if (pos != it->end()) {
      return pos->second;
    }
  }
  return TypeKind::Invalid;
}

bool SemanticAnalyzer::registerFunctions(const Program& program) {
  bool ok = true;
  for (const auto& fn : program.functions) {
    if (functions_.find(fn->name) != functions_.end()) {
      diagnostics_.reportError(fileName_, fn->loc(), "redefinition of function '" + fn->name + "'");
      ok = false;
      continue;
    }

    FunctionSymbol symbol;
    symbol.returnType = fn->returnType;
    symbol.paramTypes.reserve(fn->params.size());
    for (const ParamDecl& param : fn->params) {
      if (param.type == TypeKind::Void) {
        diagnostics_.reportError(fileName_, param.loc, "parameter '" + param.name + "' cannot have type void");
        ok = false;
      }
      symbol.paramTypes.push_back(param.type);
    }
    functions_[fn->name] = std::move(symbol);
  }
  return ok;
}

bool SemanticAnalyzer::analyzeFunction(FunctionDecl& fn) {
  pushScope();
  currentReturnType_ = fn.returnType;

  bool ok = true;
  for (const ParamDecl& param : fn.params) {
    ok = declareVariable(param.name, param.type, param.loc) && ok;
  }

  ok = analyzeBlock(*fn.body) && ok;
  if (fn.returnType != TypeKind::Void && !blockGuaranteesReturn(*fn.body)) {
    diagnostics_.reportError(fileName_, fn.loc(),
                             "function '" + fn.name + "' must return a value on all control-flow paths");
    ok = false;
  }

  popScope();
  return ok;
}

bool SemanticAnalyzer::analyzeBlock(BlockStmt& block) {
  pushScope();
  bool ok = true;
  for (const auto& stmt : block.statements) {
    ok = analyzeStmt(*stmt) && ok;
  }
  popScope();
  return ok;
}

bool SemanticAnalyzer::analyzeStmt(Stmt& stmt) {
  if (auto* var = dynCast<VarDeclStmt>(&stmt)) {
    bool ok = declareVariable(var->name, var->type, var->loc());
    if (var->initializer) {
      TypeKind initType = analyzeExpr(*var->initializer);
      if (!canAssign(var->type, initType)) {
        diagnostics_.reportError(
            fileName_, var->initializer->loc(),
            "cannot initialize variable '" + var->name + "' of type " + typeToString(var->type) + " with " +
                typeToString(initType));
        ok = false;
      }
    }
    return ok;
  }

  if (auto* assign = dynCast<AssignStmt>(&stmt)) {
    TypeKind dstType = lookupVariable(assign->name);
    if (dstType == TypeKind::Invalid) {
      diagnostics_.reportError(fileName_, assign->loc(), "use of undeclared variable '" + assign->name + "'");
      return false;
    }
    TypeKind srcType = analyzeExpr(*assign->value);
    if (!canAssign(dstType, srcType)) {
      diagnostics_.reportError(fileName_, assign->value->loc(),
                               "cannot assign " + typeToString(srcType) + " to variable '" + assign->name +
                                   "' of type " + typeToString(dstType));
      return false;
    }
    return true;
  }

  if (auto* exprStmt = dynCast<ExprStmt>(&stmt)) {
    analyzeExpr(*exprStmt->expr);
    return exprStmt->expr->inferredType != TypeKind::Invalid;
  }

  if (auto* block = dynCast<BlockStmt>(&stmt)) {
    return analyzeBlock(*block);
  }

  if (auto* ifStmt = dynCast<IfStmt>(&stmt)) {
    bool ok = true;
    TypeKind cond = analyzeExpr(*ifStmt->condition);
    if (cond != TypeKind::Bool) {
      diagnostics_.reportError(fileName_, ifStmt->condition->loc(), "if condition must have type bool");
      ok = false;
    }
    ok = analyzeBlock(*ifStmt->thenBlock) && ok;
    if (ifStmt->elseBlock) {
      ok = analyzeBlock(*ifStmt->elseBlock) && ok;
    }
    return ok;
  }

  if (auto* forStmt = dynCast<ForStmt>(&stmt)) {
    return analyzeFor(*forStmt);
  }

  if (auto* ret = dynCast<ReturnStmt>(&stmt)) {
    if (currentReturnType_ == TypeKind::Void) {
      if (ret->value != nullptr) {
        diagnostics_.reportError(fileName_, ret->loc(), "void function cannot return a value");
        return false;
      }
      return true;
    }
    if (ret->value == nullptr) {
      diagnostics_.reportError(fileName_, ret->loc(),
                               "non-void function must return a value of type " + typeToString(currentReturnType_));
      return false;
    }
    TypeKind valueType = analyzeExpr(*ret->value);
    if (!canAssign(currentReturnType_, valueType)) {
      diagnostics_.reportError(fileName_, ret->value->loc(),
                               "return type mismatch: expected " + typeToString(currentReturnType_) + ", got " +
                                   typeToString(valueType));
      return false;
    }
    return true;
  }

  diagnostics_.reportError(fileName_, stmt.loc(), "internal error: unknown statement kind");
  return false;
}

bool SemanticAnalyzer::analyzeFor(ForStmt& stmt) {
  pushScope();
  bool ok = true;

  if (stmt.init) {
    ok = analyzeStmt(*stmt.init) && ok;
  }
  TypeKind condType = analyzeExpr(*stmt.condition);
  if (condType != TypeKind::Bool) {
    diagnostics_.reportError(fileName_, stmt.condition->loc(), "for condition must have type bool");
    ok = false;
  }
  if (stmt.post) {
    ok = analyzeStmt(*stmt.post) && ok;
  }
  ok = analyzeBlock(*stmt.body) && ok;

  popScope();
  return ok;
}

bool SemanticAnalyzer::guaranteesReturn(const Stmt& stmt) const {
  if (dynCast<const ReturnStmt>(&stmt)) {
    return true;
  }
  if (const auto* block = dynCast<const BlockStmt>(&stmt)) {
    return blockGuaranteesReturn(*block);
  }
  if (const auto* ifStmt = dynCast<const IfStmt>(&stmt)) {
    if (ifStmt->elseBlock == nullptr) {
      return false;
    }
    return blockGuaranteesReturn(*ifStmt->thenBlock) && blockGuaranteesReturn(*ifStmt->elseBlock);
  }
  return false;
}

bool SemanticAnalyzer::blockGuaranteesReturn(const BlockStmt& block) const {
  for (const auto& stmt : block.statements) {
    if (guaranteesReturn(*stmt)) {
      return true;
    }
  }
  return false;
}

TypeKind SemanticAnalyzer::analyzeExpr(Expr& expr) {
  if (auto* lit = dynCast<IntLiteralExpr>(&expr)) {
    lit->inferredType = TypeKind::Int;
    return lit->inferredType;
  }
  if (auto* lit = dynCast<FloatLiteralExpr>(&expr)) {
    lit->inferredType = TypeKind::Float;
    return lit->inferredType;
  }
  if (auto* lit = dynCast<BoolLiteralExpr>(&expr)) {
    lit->inferredType = TypeKind::Bool;
    return lit->inferredType;
  }
  if (auto* var = dynCast<VariableExpr>(&expr)) {
    TypeKind type = lookupVariable(var->name);
    if (type == TypeKind::Invalid) {
      diagnostics_.reportError(fileName_, var->loc(), "use of undeclared variable '" + var->name + "'");
      var->inferredType = TypeKind::Invalid;
      return TypeKind::Invalid;
    }
    var->inferredType = type;
    return type;
  }
  if (auto* call = dynCast<CallExpr>(&expr)) {
    auto fnIt = functions_.find(call->callee);
    if (fnIt == functions_.end()) {
      diagnostics_.reportError(fileName_, call->loc(), "call to undeclared function '" + call->callee + "'");
      call->inferredType = TypeKind::Invalid;
      return TypeKind::Invalid;
    }
    const FunctionSymbol& fn = fnIt->second;
    if (fn.paramTypes.size() != call->args.size()) {
      std::ostringstream ss;
      ss << "function '" << call->callee << "' expects " << fn.paramTypes.size() << " arguments, got "
         << call->args.size();
      diagnostics_.reportError(fileName_, call->loc(), ss.str());
      call->inferredType = TypeKind::Invalid;
      return TypeKind::Invalid;
    }
    bool ok = true;
    for (std::size_t i = 0; i < call->args.size(); ++i) {
      TypeKind argType = analyzeExpr(*call->args[i]);
      if (!canAssign(fn.paramTypes[i], argType)) {
        diagnostics_.reportError(fileName_, call->args[i]->loc(),
                                 "argument " + std::to_string(i + 1) + " of '" + call->callee + "' expects " +
                                     typeToString(fn.paramTypes[i]) + ", got " + typeToString(argType));
        ok = false;
      }
    }
    call->inferredType = ok ? fn.returnType : TypeKind::Invalid;
    return call->inferredType;
  }
  if (auto* unary = dynCast<UnaryExpr>(&expr)) {
    TypeKind operand = analyzeExpr(*unary->operand);
    if (unary->op == UnaryOpKind::Negate) {
      if (operand != TypeKind::Int && operand != TypeKind::Float) {
        diagnostics_.reportError(fileName_, unary->loc(), "unary '-' expects int or float operand");
        unary->inferredType = TypeKind::Invalid;
      } else {
        unary->inferredType = operand;
      }
      return unary->inferredType;
    }
    if (operand != TypeKind::Bool) {
      diagnostics_.reportError(fileName_, unary->loc(), "logical '!' expects bool operand");
      unary->inferredType = TypeKind::Invalid;
    } else {
      unary->inferredType = TypeKind::Bool;
    }
    return unary->inferredType;
  }
  if (auto* binary = dynCast<BinaryExpr>(&expr)) {
    TypeKind lhs = analyzeExpr(*binary->lhs);
    TypeKind rhs = analyzeExpr(*binary->rhs);
    if (lhs == TypeKind::Invalid || rhs == TypeKind::Invalid) {
      binary->inferredType = TypeKind::Invalid;
      return binary->inferredType;
    }

    switch (binary->op) {
      case BinaryOpKind::Add:
      case BinaryOpKind::Sub:
      case BinaryOpKind::Mul:
      case BinaryOpKind::Div: {
        TypeKind common = commonNumericType(lhs, rhs);
        if (common == TypeKind::Invalid) {
          diagnostics_.reportError(fileName_, binary->loc(),
                                   "arithmetic operators require numeric operands, got " + typeToString(lhs) +
                                       " and " + typeToString(rhs));
          binary->inferredType = TypeKind::Invalid;
        } else {
          binary->inferredType = common;
        }
        return binary->inferredType;
      }
      case BinaryOpKind::Mod: {
        if (lhs != TypeKind::Int || rhs != TypeKind::Int) {
          diagnostics_.reportError(fileName_, binary->loc(), "operator '%' requires int operands");
          binary->inferredType = TypeKind::Invalid;
        } else {
          binary->inferredType = TypeKind::Int;
        }
        return binary->inferredType;
      }
      case BinaryOpKind::Less:
      case BinaryOpKind::LessEq:
      case BinaryOpKind::Greater:
      case BinaryOpKind::GreaterEq: {
        if (commonNumericType(lhs, rhs) == TypeKind::Invalid) {
          diagnostics_.reportError(fileName_, binary->loc(),
                                   "comparison requires numeric operands, got " + typeToString(lhs) + " and " +
                                       typeToString(rhs));
          binary->inferredType = TypeKind::Invalid;
        } else {
          binary->inferredType = TypeKind::Bool;
        }
        return binary->inferredType;
      }
      case BinaryOpKind::Eq:
      case BinaryOpKind::NotEq: {
        if (lhs == TypeKind::Bool && rhs == TypeKind::Bool) {
          binary->inferredType = TypeKind::Bool;
          return binary->inferredType;
        }
        if (commonNumericType(lhs, rhs) != TypeKind::Invalid) {
          binary->inferredType = TypeKind::Bool;
          return binary->inferredType;
        }
        diagnostics_.reportError(fileName_, binary->loc(),
                                 "equality comparison requires compatible operand types, got " + typeToString(lhs) +
                                     " and " + typeToString(rhs));
        binary->inferredType = TypeKind::Invalid;
        return binary->inferredType;
      }
      case BinaryOpKind::LogicalAnd:
      case BinaryOpKind::LogicalOr: {
        if (lhs != TypeKind::Bool || rhs != TypeKind::Bool) {
          diagnostics_.reportError(fileName_, binary->loc(),
                                   "logical operators require bool operands, got " + typeToString(lhs) + " and " +
                                       typeToString(rhs));
          binary->inferredType = TypeKind::Invalid;
        } else {
          binary->inferredType = TypeKind::Bool;
        }
        return binary->inferredType;
      }
    }
  }
  if (auto* ternary = dynCast<TernaryExpr>(&expr)) {
    TypeKind cond = analyzeExpr(*ternary->condition);
    TypeKind thenType = analyzeExpr(*ternary->thenExpr);
    TypeKind elseType = analyzeExpr(*ternary->elseExpr);
    if (cond != TypeKind::Bool) {
      diagnostics_.reportError(fileName_, ternary->condition->loc(), "ternary condition must have type bool");
      ternary->inferredType = TypeKind::Invalid;
      return ternary->inferredType;
    }
    if (thenType == elseType) {
      ternary->inferredType = thenType;
      return ternary->inferredType;
    }
    if ((thenType == TypeKind::Int && elseType == TypeKind::Float) ||
        (thenType == TypeKind::Float && elseType == TypeKind::Int)) {
      ternary->inferredType = TypeKind::Float;
      return ternary->inferredType;
    }
    diagnostics_.reportError(fileName_, ternary->loc(),
                             "ternary branches must have compatible types, got " + typeToString(thenType) + " and " +
                                 typeToString(elseType));
    ternary->inferredType = TypeKind::Invalid;
    return ternary->inferredType;
  }

  diagnostics_.reportError(fileName_, expr.loc(), "internal error: unknown expression kind");
  expr.inferredType = TypeKind::Invalid;
  return TypeKind::Invalid;
}

bool SemanticAnalyzer::canAssign(TypeKind dst, TypeKind src) const {
  if (dst == src) {
    return true;
  }
  return dst == TypeKind::Float && src == TypeKind::Int;
}

TypeKind SemanticAnalyzer::commonNumericType(TypeKind a, TypeKind b) const {
  if (a == TypeKind::Int && b == TypeKind::Int) {
    return TypeKind::Int;
  }
  if ((a == TypeKind::Int || a == TypeKind::Float) && (b == TypeKind::Int || b == TypeKind::Float)) {
    return TypeKind::Float;
  }
  return TypeKind::Invalid;
}

}  // namespace lorsc

