#include "lorsc/AST.h"

#include <utility>

namespace lorsc {

std::string typeToString(TypeKind type) {
  switch (type) {
    case TypeKind::Int:
      return "int";
    case TypeKind::Float:
      return "float";
    case TypeKind::Bool:
      return "bool";
    case TypeKind::Void:
      return "void";
    default:
      return "<invalid>";
  }
}

CallExpr::CallExpr(SourceLocation loc, std::string callee, std::vector<Expr*> args)
    : Expr(loc), callee(std::move(callee)) {
  this->args.reserve(args.size());
  for (Expr* arg : args) {
    this->args.emplace_back(arg);
  }
}

UnaryExpr::UnaryExpr(SourceLocation loc, UnaryOpKind op, Expr* operand)
    : Expr(loc), op(op), operand(operand) {}

BinaryExpr::BinaryExpr(SourceLocation loc, BinaryOpKind op, Expr* lhs, Expr* rhs)
    : Expr(loc), op(op), lhs(lhs), rhs(rhs) {}

TernaryExpr::TernaryExpr(SourceLocation loc, Expr* condition, Expr* thenExpr, Expr* elseExpr)
    : Expr(loc), condition(condition), thenExpr(thenExpr), elseExpr(elseExpr) {}

BlockStmt::BlockStmt(SourceLocation loc, std::vector<Stmt*> statements) : Stmt(loc) {
  this->statements.reserve(statements.size());
  for (Stmt* stmt : statements) {
    this->statements.emplace_back(stmt);
  }
}

VarDeclStmt::VarDeclStmt(SourceLocation loc, std::string name, TypeKind type, Expr* initializer)
    : Stmt(loc), name(std::move(name)), type(type), initializer(initializer) {}

AssignStmt::AssignStmt(SourceLocation loc, std::string name, Expr* value)
    : Stmt(loc), name(std::move(name)), value(value) {}

ExprStmt::ExprStmt(SourceLocation loc, Expr* expr) : Stmt(loc), expr(expr) {}

IfStmt::IfStmt(SourceLocation loc, Expr* condition, BlockStmt* thenBlock, BlockStmt* elseBlock)
    : Stmt(loc), condition(condition), thenBlock(thenBlock), elseBlock(elseBlock) {}

ForStmt::ForStmt(SourceLocation loc, Stmt* init, Expr* condition, AssignStmt* post, BlockStmt* body)
    : Stmt(loc), init(init), condition(condition), post(post), body(body) {}

ReturnStmt::ReturnStmt(SourceLocation loc, Expr* value) : Stmt(loc), value(value) {}

FunctionDecl::FunctionDecl(SourceLocation loc, std::string name, std::vector<ParamDecl>* params,
                           TypeKind returnType, BlockStmt* body)
    : Node(loc), name(std::move(name)), returnType(returnType), body(body) {
  if (params != nullptr) {
    this->params = std::move(*params);
    delete params;
  }
}

Program::Program(SourceLocation loc, std::vector<FunctionDecl*> functions) : Node(loc) {
  this->functions.reserve(functions.size());
  for (FunctionDecl* fn : functions) {
    this->functions.emplace_back(fn);
  }
}

}  // namespace lorsc

