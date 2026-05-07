#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "lorsc/AST.h"
#include "lorsc/Diagnostics.h"

namespace lorsc {

class SemanticAnalyzer {
 public:
  SemanticAnalyzer(DiagnosticsEngine& diagnostics, std::string fileName);
  bool analyze(Program& program);

 private:
  struct FunctionSymbol {
    TypeKind returnType = TypeKind::Invalid;
    std::vector<TypeKind> paramTypes;
  };

  DiagnosticsEngine& diagnostics_;
  std::string fileName_;

  std::unordered_map<std::string, FunctionSymbol> functions_;
  std::vector<std::unordered_map<std::string, TypeKind>> scopes_;
  TypeKind currentReturnType_ = TypeKind::Invalid;

  void pushScope();
  void popScope();
  bool declareVariable(const std::string& name, TypeKind type, SourceLocation loc);
  TypeKind lookupVariable(const std::string& name) const;

  bool registerFunctions(const Program& program);
  bool analyzeFunction(FunctionDecl& fn);
  bool analyzeBlock(BlockStmt& block);
  bool analyzeStmt(Stmt& stmt);
  bool analyzeFor(ForStmt& stmt);

  bool guaranteesReturn(const Stmt& stmt) const;
  bool blockGuaranteesReturn(const BlockStmt& block) const;

  TypeKind analyzeExpr(Expr& expr);
  bool canAssign(TypeKind dst, TypeKind src) const;
  TypeKind commonNumericType(TypeKind a, TypeKind b) const;
};

}  // namespace lorsc


