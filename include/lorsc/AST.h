#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lorsc/SourceLocation.h"

namespace lorsc {

enum class TypeKind { Int, Float, Bool, Void, Invalid };

std::string typeToString(TypeKind type);

enum class UnaryOpKind { Negate, Not };

enum class BinaryOpKind {
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Less,
  LessEq,
  Greater,
  GreaterEq,
  Eq,
  NotEq,
  LogicalAnd,
  LogicalOr
};

struct ParamDecl {
  std::string name;
  TypeKind type = TypeKind::Invalid;
  SourceLocation loc;
};

class Node {
 public:
  explicit Node(SourceLocation loc) : loc_(loc) {}
  virtual ~Node() = default;

  SourceLocation loc() const { return loc_; }

 private:
  SourceLocation loc_;
};

class Expr : public Node {
 public:
  explicit Expr(SourceLocation loc) : Node(loc) {}
  virtual ~Expr() = default;

  TypeKind inferredType = TypeKind::Invalid;
};

class IntLiteralExpr : public Expr {
 public:
  IntLiteralExpr(SourceLocation loc, long long value) : Expr(loc), value(value) {}
  long long value;
};

class FloatLiteralExpr : public Expr {
 public:
  FloatLiteralExpr(SourceLocation loc, double value) : Expr(loc), value(value) {}
  double value;
};

class BoolLiteralExpr : public Expr {
 public:
  BoolLiteralExpr(SourceLocation loc, bool value) : Expr(loc), value(value) {}
  bool value;
};

class VariableExpr : public Expr {
 public:
  VariableExpr(SourceLocation loc, std::string name) : Expr(loc), name(std::move(name)) {}
  std::string name;
};

class CallExpr : public Expr {
 public:
  CallExpr(SourceLocation loc, std::string callee, std::vector<Expr*> args);
  std::string callee;
  std::vector<std::unique_ptr<Expr>> args;
};

class UnaryExpr : public Expr {
 public:
  UnaryExpr(SourceLocation loc, UnaryOpKind op, Expr* operand);
  UnaryOpKind op;
  std::unique_ptr<Expr> operand;
};

class BinaryExpr : public Expr {
 public:
  BinaryExpr(SourceLocation loc, BinaryOpKind op, Expr* lhs, Expr* rhs);
  BinaryOpKind op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
};

class TernaryExpr : public Expr {
 public:
  TernaryExpr(SourceLocation loc, Expr* condition, Expr* thenExpr, Expr* elseExpr);
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Expr> thenExpr;
  std::unique_ptr<Expr> elseExpr;
};

class Stmt : public Node {
 public:
  explicit Stmt(SourceLocation loc) : Node(loc) {}
  virtual ~Stmt() = default;
};

class BlockStmt : public Stmt {
 public:
  BlockStmt(SourceLocation loc, std::vector<Stmt*> statements);
  std::vector<std::unique_ptr<Stmt>> statements;
};

class VarDeclStmt : public Stmt {
 public:
  VarDeclStmt(SourceLocation loc, std::string name, TypeKind type, Expr* initializer);
  std::string name;
  TypeKind type = TypeKind::Invalid;
  std::unique_ptr<Expr> initializer;
};

class AssignStmt : public Stmt {
 public:
  AssignStmt(SourceLocation loc, std::string name, Expr* value);
  std::string name;
  std::unique_ptr<Expr> value;
};

class ExprStmt : public Stmt {
 public:
  ExprStmt(SourceLocation loc, Expr* expr);
  std::unique_ptr<Expr> expr;
};

class IfStmt : public Stmt {
 public:
  IfStmt(SourceLocation loc, Expr* condition, BlockStmt* thenBlock, BlockStmt* elseBlock);
  std::unique_ptr<Expr> condition;
  std::unique_ptr<BlockStmt> thenBlock;
  std::unique_ptr<BlockStmt> elseBlock;
};

class ForStmt : public Stmt {
 public:
  ForStmt(SourceLocation loc, Stmt* init, Expr* condition, AssignStmt* post, BlockStmt* body);
  std::unique_ptr<Stmt> init;
  std::unique_ptr<Expr> condition;
  std::unique_ptr<AssignStmt> post;
  std::unique_ptr<BlockStmt> body;
};

class ReturnStmt : public Stmt {
 public:
  ReturnStmt(SourceLocation loc, Expr* value);
  std::unique_ptr<Expr> value;
};

class FunctionDecl : public Node {
 public:
  FunctionDecl(SourceLocation loc, std::string name, std::vector<ParamDecl>* params, TypeKind returnType,
               BlockStmt* body);
  std::string name;
  std::vector<ParamDecl> params;
  TypeKind returnType = TypeKind::Invalid;
  std::unique_ptr<BlockStmt> body;
};

class Program : public Node {
 public:
  Program(SourceLocation loc, std::vector<FunctionDecl*> functions);
  std::vector<std::unique_ptr<FunctionDecl>> functions;
};

}  // namespace lorsc


