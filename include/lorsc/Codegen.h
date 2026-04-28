#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "lorsc/AST.h"
#include "llvm/IR/IRBuilder.h"

namespace llvm {
class LLVMContext;
class Module;
class Value;
class Function;
class AllocaInst;
class Type;
class TargetMachine;
}  // namespace llvm

namespace lorsc {

class CodeGenerator {
 public:
  explicit CodeGenerator(std::string targetTriple);
  ~CodeGenerator();

  bool generate(Program& program);
  bool emitIR(const std::string& path) const;
  bool emitAssembly(const std::string& path);
  bool emitObject(const std::string& path);

 private:
  std::string targetTriple_;
  std::unique_ptr<llvm::LLVMContext> context_;
  std::unique_ptr<llvm::Module> module_;
  std::unique_ptr<llvm::IRBuilder<>> builder_;

  std::unordered_map<std::string, llvm::Function*> functions_;
  std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> scopes_;
  llvm::Function* currentFunction_ = nullptr;

  llvm::Type* llvmType(TypeKind type) const;
  llvm::Value* defaultValue(TypeKind type) const;

  void pushScope();
  void popScope();
  void bindVariable(const std::string& name, llvm::AllocaInst* slot);
  llvm::AllocaInst* lookupVariable(const std::string& name) const;

  bool declareFunctions(Program& program);
  bool defineFunction(FunctionDecl& function);
  llvm::AllocaInst* createEntryAlloca(llvm::Function* function, const std::string& name, TypeKind type);

  bool codegenStmt(Stmt& stmt);
  bool codegenBlock(BlockStmt& block);
  bool codegenIf(IfStmt& stmt);
  bool codegenFor(ForStmt& stmt);
  bool codegenReturn(ReturnStmt& stmt);
  bool codegenVarDecl(VarDeclStmt& stmt);
  bool codegenAssign(AssignStmt& stmt);

  llvm::Value* codegenExpr(Expr& expr);
  llvm::Value* codegenUnary(UnaryExpr& expr);
  llvm::Value* codegenBinary(BinaryExpr& expr);
  llvm::Value* codegenTernary(TernaryExpr& expr);
  llvm::Value* castTo(TypeKind target, llvm::Value* value, TypeKind source) const;
  llvm::Value* toBool(llvm::Value* value, TypeKind source) const;

  std::unique_ptr<llvm::TargetMachine> createTargetMachine();
  bool verify() const;
};

}  // namespace lorsc
