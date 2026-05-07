#include "lorsc/Codegen.h"

#include <iostream>
#include <memory>
#include <string>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

namespace lorsc {

namespace {

template <typename T, typename U>
T* dynCast(U* ptr) {
  return dynamic_cast<T*>(ptr);
}

void initializeLLVMTargets() {
  static bool initialized = false;
  if (initialized) {
    return;
  }

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();
  initialized = true;
}

}  // namespace

CodeGenerator::CodeGenerator(std::string targetTriple)
    : targetTriple_(std::move(targetTriple)),
      context_(std::make_unique<llvm::LLVMContext>()),
      module_(std::make_unique<llvm::Module>("lors_module", *context_)),
      builder_(std::make_unique<llvm::IRBuilder<>>(*context_)) {
  module_->setTargetTriple(targetTriple_);
}

CodeGenerator::~CodeGenerator() = default;

void CodeGenerator::pushScope() { scopes_.emplace_back(); }

void CodeGenerator::popScope() { scopes_.pop_back(); }

void CodeGenerator::bindVariable(const std::string& name, llvm::AllocaInst* slot) {
  scopes_.back()[name] = slot;
}

llvm::AllocaInst* CodeGenerator::lookupVariable(const std::string& name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto pos = it->find(name);
    if (pos != it->end()) {
      return pos->second;
    }
  }
  return nullptr;
}

llvm::Type* CodeGenerator::llvmType(TypeKind type) const {
  switch (type) {
    case TypeKind::Int:
      return llvm::Type::getInt64Ty(*context_);
    case TypeKind::Float:
      return llvm::Type::getDoubleTy(*context_);
    case TypeKind::Bool:
      return llvm::Type::getInt1Ty(*context_);
    case TypeKind::Void:
      return llvm::Type::getVoidTy(*context_);
    default:
      return nullptr;
  }
}

llvm::Value* CodeGenerator::defaultValue(TypeKind type) const {
  switch (type) {
    case TypeKind::Int:
      return llvm::ConstantInt::getSigned(llvmType(TypeKind::Int), 0);
    case TypeKind::Float:
      return llvm::ConstantFP::get(llvmType(TypeKind::Float), 0.0);
    case TypeKind::Bool:
      return llvm::ConstantInt::getFalse(*context_);
    case TypeKind::Void:
      return nullptr;
    default:
      return nullptr;
  }
}

bool CodeGenerator::generate(Program& program) {
  if (!declareFunctions(program)) {
    return false;
  }
  for (const auto& fn : program.functions) {
    if (!defineFunction(*fn)) {
      return false;
    }
  }
  return verify();
}

bool CodeGenerator::declareFunctions(Program& program) {
  for (const auto& fn : program.functions) {
    std::vector<llvm::Type*> argTypes;
    argTypes.reserve(fn->params.size());
    for (const ParamDecl& p : fn->params) {
      llvm::Type* ty = llvmType(p.type);
      if (ty == nullptr) {
        std::cerr << "internal error: invalid argument type in function " << fn->name << "\n";
        return false;
      }
      argTypes.push_back(ty);
    }

    llvm::Type* retTy = llvmType(fn->returnType);
    if (retTy == nullptr) {
      std::cerr << "internal error: invalid return type in function " << fn->name << "\n";
      return false;
    }

    llvm::FunctionType* fnTy = llvm::FunctionType::get(retTy, argTypes, false);
    llvm::Function* llvmFn =
        llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fn->name, module_.get());
    functions_[fn->name] = llvmFn;
  }
  return true;
}

llvm::AllocaInst* CodeGenerator::createEntryAlloca(llvm::Function* function, const std::string& name,
                                                   TypeKind type) {
  llvm::IRBuilder<> entryBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());
  return entryBuilder.CreateAlloca(llvmType(type), nullptr, name);
}

bool CodeGenerator::defineFunction(FunctionDecl& function) {
  llvm::Function* llvmFn = functions_.at(function.name);
  currentFunction_ = llvmFn;

  llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", llvmFn);
  builder_->SetInsertPoint(entry);

  pushScope();

  std::size_t argIndex = 0;
  for (llvm::Argument& arg : llvmFn->args()) {
    const ParamDecl& param = function.params[argIndex++];
    arg.setName(param.name);
    llvm::AllocaInst* slot = createEntryAlloca(llvmFn, param.name, param.type);
    builder_->CreateStore(&arg, slot);
    bindVariable(param.name, slot);
  }

  if (!codegenBlock(*function.body)) {
    popScope();
    return false;
  }

  if (builder_->GetInsertBlock()->getTerminator() == nullptr) {
    if (function.returnType == TypeKind::Void) {
      builder_->CreateRetVoid();
    } else {
      builder_->CreateRet(defaultValue(function.returnType));
    }
  }

  popScope();
  if (llvm::verifyFunction(*llvmFn, &llvm::errs())) {
    std::cerr << "error: invalid LLVM IR in function " << function.name << "\n";
    return false;
  }
  return true;
}

bool CodeGenerator::codegenStmt(Stmt& stmt) {
  if (auto* block = dynCast<BlockStmt>(&stmt)) {
    return codegenBlock(*block);
  }
  if (auto* var = dynCast<VarDeclStmt>(&stmt)) {
    return codegenVarDecl(*var);
  }
  if (auto* assign = dynCast<AssignStmt>(&stmt)) {
    return codegenAssign(*assign);
  }
  if (auto* exprStmt = dynCast<ExprStmt>(&stmt)) {
    return codegenExpr(*exprStmt->expr) != nullptr;
  }
  if (auto* ifStmt = dynCast<IfStmt>(&stmt)) {
    return codegenIf(*ifStmt);
  }
  if (auto* forStmt = dynCast<ForStmt>(&stmt)) {
    return codegenFor(*forStmt);
  }
  if (auto* ret = dynCast<ReturnStmt>(&stmt)) {
    return codegenReturn(*ret);
  }
  std::cerr << "internal error: unknown statement node in codegen\n";
  return false;
}

bool CodeGenerator::codegenBlock(BlockStmt& block) {
  pushScope();
  for (const auto& stmt : block.statements) {
    if (!codegenStmt(*stmt)) {
      popScope();
      return false;
    }
    if (builder_->GetInsertBlock()->getTerminator() != nullptr) {
      break;
    }
  }
  popScope();
  return true;
}

bool CodeGenerator::codegenIf(IfStmt& stmt) {
  llvm::Function* fn = builder_->GetInsertBlock()->getParent();

  llvm::Value* condValue = codegenExpr(*stmt.condition);
  if (condValue == nullptr) {
    return false;
  }
  condValue = toBool(condValue, stmt.condition->inferredType);
  if (condValue == nullptr) {
    return false;
  }

  llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context_, "if.then", fn);
  llvm::BasicBlock* elseBB = stmt.elseBlock ? llvm::BasicBlock::Create(*context_, "if.else", fn) : nullptr;
  llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context_, "if.merge", fn);

  if (elseBB != nullptr) {
    builder_->CreateCondBr(condValue, thenBB, elseBB);
  } else {
    builder_->CreateCondBr(condValue, thenBB, mergeBB);
  }

  builder_->SetInsertPoint(thenBB);
  if (!codegenBlock(*stmt.thenBlock)) {
    return false;
  }
  if (builder_->GetInsertBlock()->getTerminator() == nullptr) {
    builder_->CreateBr(mergeBB);
  }

  if (elseBB != nullptr) {
    builder_->SetInsertPoint(elseBB);
    if (!codegenBlock(*stmt.elseBlock)) {
      return false;
    }
    if (builder_->GetInsertBlock()->getTerminator() == nullptr) {
      builder_->CreateBr(mergeBB);
    }
  }

  builder_->SetInsertPoint(mergeBB);
  return true;
}

bool CodeGenerator::codegenFor(ForStmt& stmt) {
  llvm::Function* fn = builder_->GetInsertBlock()->getParent();
  pushScope();

  if (stmt.init && !codegenStmt(*stmt.init)) {
    popScope();
    return false;
  }

  llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*context_, "for.cond", fn);
  llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*context_, "for.body", fn);
  llvm::BasicBlock* postBB = llvm::BasicBlock::Create(*context_, "for.post", fn);
  llvm::BasicBlock* exitBB = llvm::BasicBlock::Create(*context_, "for.exit", fn);

  if (builder_->GetInsertBlock()->getTerminator() == nullptr) {
    builder_->CreateBr(condBB);
  }

  builder_->SetInsertPoint(condBB);
  llvm::Value* condValue = codegenExpr(*stmt.condition);
  if (condValue == nullptr) {
    popScope();
    return false;
  }
  condValue = toBool(condValue, stmt.condition->inferredType);
  if (condValue == nullptr) {
    popScope();
    return false;
  }
  builder_->CreateCondBr(condValue, bodyBB, exitBB);

  builder_->SetInsertPoint(bodyBB);
  if (!codegenBlock(*stmt.body)) {
    popScope();
    return false;
  }
  if (builder_->GetInsertBlock()->getTerminator() == nullptr) {
    builder_->CreateBr(postBB);
  }

  builder_->SetInsertPoint(postBB);
  if (stmt.post && !codegenAssign(*stmt.post)) {
    popScope();
    return false;
  }
  if (builder_->GetInsertBlock()->getTerminator() == nullptr) {
    builder_->CreateBr(condBB);
  }

  builder_->SetInsertPoint(exitBB);
  popScope();
  return true;
}

bool CodeGenerator::codegenReturn(ReturnStmt& stmt) {
  if (stmt.value == nullptr) {
    builder_->CreateRetVoid();
    return true;
  }
  llvm::Value* value = codegenExpr(*stmt.value);
  if (value == nullptr) {
    return false;
  }
  llvm::Type* expected = currentFunction_->getReturnType();
  if (value->getType() != expected) {
    TypeKind sourceType = stmt.value->inferredType;
    TypeKind targetType = TypeKind::Invalid;
    if (expected->isDoubleTy()) {
      targetType = TypeKind::Float;
    } else if (expected->isIntegerTy(64)) {
      targetType = TypeKind::Int;
    } else if (expected->isIntegerTy(1)) {
      targetType = TypeKind::Bool;
    }
    value = castTo(targetType, value, sourceType);
    if (value == nullptr) {
      return false;
    }
  }
  builder_->CreateRet(value);
  return true;
}

bool CodeGenerator::codegenVarDecl(VarDeclStmt& stmt) {
  llvm::AllocaInst* slot = createEntryAlloca(currentFunction_, stmt.name, stmt.type);
  bindVariable(stmt.name, slot);

  llvm::Value* initial = defaultValue(stmt.type);
  if (stmt.initializer) {
    initial = codegenExpr(*stmt.initializer);
    if (initial == nullptr) {
      return false;
    }
    if (stmt.type != stmt.initializer->inferredType) {
      initial = castTo(stmt.type, initial, stmt.initializer->inferredType);
      if (initial == nullptr) {
        return false;
      }
    }
  }
  builder_->CreateStore(initial, slot);
  return true;
}

bool CodeGenerator::codegenAssign(AssignStmt& stmt) {
  llvm::AllocaInst* slot = lookupVariable(stmt.name);
  if (slot == nullptr) {
    std::cerr << "internal error: missing variable slot for '" << stmt.name << "'\n";
    return false;
  }
  llvm::Value* value = codegenExpr(*stmt.value);
  if (value == nullptr) {
    return false;
  }

  llvm::Type* varType = slot->getAllocatedType();
  if (value->getType() != varType) {
    TypeKind target = TypeKind::Invalid;
    if (varType->isDoubleTy()) {
      target = TypeKind::Float;
    } else if (varType->isIntegerTy(64)) {
      target = TypeKind::Int;
    } else if (varType->isIntegerTy(1)) {
      target = TypeKind::Bool;
    }
    value = castTo(target, value, stmt.value->inferredType);
    if (value == nullptr) {
      return false;
    }
  }
  builder_->CreateStore(value, slot);
  return true;
}

llvm::Value* CodeGenerator::codegenExpr(Expr& expr) {
  if (auto* lit = dynCast<IntLiteralExpr>(&expr)) {
    return llvm::ConstantInt::getSigned(llvmType(TypeKind::Int), lit->value);
  }
  if (auto* lit = dynCast<FloatLiteralExpr>(&expr)) {
    return llvm::ConstantFP::get(llvmType(TypeKind::Float), lit->value);
  }
  if (auto* lit = dynCast<BoolLiteralExpr>(&expr)) {
    return llvm::ConstantInt::get(llvmType(TypeKind::Bool), lit->value ? 1 : 0);
  }
  if (auto* var = dynCast<VariableExpr>(&expr)) {
    llvm::AllocaInst* slot = lookupVariable(var->name);
    if (slot == nullptr) {
      std::cerr << "internal error: missing variable slot for '" << var->name << "'\n";
      return nullptr;
    }
    return builder_->CreateLoad(slot->getAllocatedType(), slot, var->name + ".load");
  }
  if (auto* call = dynCast<CallExpr>(&expr)) {
    auto fnIt = functions_.find(call->callee);
    if (fnIt == functions_.end()) {
      std::cerr << "internal error: missing LLVM function '" << call->callee << "'\n";
      return nullptr;
    }
    llvm::Function* callee = fnIt->second;
    std::vector<llvm::Value*> args;
    args.reserve(call->args.size());
    for (std::size_t i = 0; i < call->args.size(); ++i) {
      llvm::Value* argVal = codegenExpr(*call->args[i]);
      if (argVal == nullptr) {
        return nullptr;
      }
      llvm::Type* expectedType = callee->getFunctionType()->getParamType(i);
      if (argVal->getType() != expectedType) {
        TypeKind expected = expectedType->isDoubleTy()   ? TypeKind::Float
                            : expectedType->isIntegerTy(64) ? TypeKind::Int
                            : expectedType->isIntegerTy(1)  ? TypeKind::Bool
                                                             : TypeKind::Invalid;
        argVal = castTo(expected, argVal, call->args[i]->inferredType);
        if (argVal == nullptr) {
          return nullptr;
        }
      }
      args.push_back(argVal);
    }
    if (callee->getReturnType()->isVoidTy()) {
      return builder_->CreateCall(callee, args);
    }
    return builder_->CreateCall(callee, args, call->callee + ".call");
  }
  if (auto* unary = dynCast<UnaryExpr>(&expr)) {
    return codegenUnary(*unary);
  }
  if (auto* binary = dynCast<BinaryExpr>(&expr)) {
    return codegenBinary(*binary);
  }
  if (auto* ternary = dynCast<TernaryExpr>(&expr)) {
    return codegenTernary(*ternary);
  }
  std::cerr << "internal error: unknown expression node in codegen\n";
  return nullptr;
}

llvm::Value* CodeGenerator::codegenUnary(UnaryExpr& expr) {
  llvm::Value* operand = codegenExpr(*expr.operand);
  if (operand == nullptr) {
    return nullptr;
  }
  if (expr.op == UnaryOpKind::Negate) {
    if (expr.inferredType == TypeKind::Float) {
      return builder_->CreateFNeg(operand, "fneg");
    }
    return builder_->CreateNeg(operand, "ineg");
  }
  return builder_->CreateNot(operand, "lnot");
}

llvm::Value* CodeGenerator::codegenBinary(BinaryExpr& expr) {
  if (expr.op == BinaryOpKind::LogicalAnd || expr.op == BinaryOpKind::LogicalOr) {
    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::Value* lhs = codegenExpr(*expr.lhs);
    if (lhs == nullptr) {
      return nullptr;
    }
    lhs = toBool(lhs, expr.lhs->inferredType);
    if (lhs == nullptr) {
      return nullptr;
    }
    llvm::BasicBlock* lhsEnd = builder_->GetInsertBlock();

    llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(*context_, "logic.rhs", fn);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context_, "logic.merge", fn);

    if (expr.op == BinaryOpKind::LogicalAnd) {
      builder_->CreateCondBr(lhs, rhsBB, mergeBB);
    } else {
      builder_->CreateCondBr(lhs, mergeBB, rhsBB);
    }

    builder_->SetInsertPoint(rhsBB);
    llvm::Value* rhs = codegenExpr(*expr.rhs);
    if (rhs == nullptr) {
      return nullptr;
    }
    rhs = toBool(rhs, expr.rhs->inferredType);
    if (rhs == nullptr) {
      return nullptr;
    }
    builder_->CreateBr(mergeBB);
    llvm::BasicBlock* rhsEnd = builder_->GetInsertBlock();

    builder_->SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder_->CreatePHI(llvmType(TypeKind::Bool), 2, "logic.phi");
    if (expr.op == BinaryOpKind::LogicalAnd) {
      phi->addIncoming(llvm::ConstantInt::getFalse(*context_), lhsEnd);
      phi->addIncoming(rhs, rhsEnd);
    } else {
      phi->addIncoming(llvm::ConstantInt::getTrue(*context_), lhsEnd);
      phi->addIncoming(rhs, rhsEnd);
    }
    return phi;
  }

  llvm::Value* lhs = codegenExpr(*expr.lhs);
  llvm::Value* rhs = codegenExpr(*expr.rhs);
  if (lhs == nullptr || rhs == nullptr) {
    return nullptr;
  }

  TypeKind lhsType = expr.lhs->inferredType;
  TypeKind rhsType = expr.rhs->inferredType;

  const bool numericResultIsFloat =
      (expr.inferredType == TypeKind::Float &&
       (expr.op == BinaryOpKind::Add || expr.op == BinaryOpKind::Sub || expr.op == BinaryOpKind::Mul ||
        expr.op == BinaryOpKind::Div));

  if (numericResultIsFloat || (lhsType == TypeKind::Float || rhsType == TypeKind::Float)) {
    if (lhsType == TypeKind::Int) {
      lhs = castTo(TypeKind::Float, lhs, lhsType);
      lhsType = TypeKind::Float;
    }
    if (rhsType == TypeKind::Int) {
      rhs = castTo(TypeKind::Float, rhs, rhsType);
      rhsType = TypeKind::Float;
    }
  }

  switch (expr.op) {
    case BinaryOpKind::Add:
      return expr.inferredType == TypeKind::Float ? builder_->CreateFAdd(lhs, rhs, "fadd")
                                                  : builder_->CreateAdd(lhs, rhs, "iadd");
    case BinaryOpKind::Sub:
      return expr.inferredType == TypeKind::Float ? builder_->CreateFSub(lhs, rhs, "fsub")
                                                  : builder_->CreateSub(lhs, rhs, "isub");
    case BinaryOpKind::Mul:
      return expr.inferredType == TypeKind::Float ? builder_->CreateFMul(lhs, rhs, "fmul")
                                                  : builder_->CreateMul(lhs, rhs, "imul");
    case BinaryOpKind::Div:
      return expr.inferredType == TypeKind::Float ? builder_->CreateFDiv(lhs, rhs, "fdiv")
                                                  : builder_->CreateSDiv(lhs, rhs, "idiv");
    case BinaryOpKind::Mod:
      return builder_->CreateSRem(lhs, rhs, "imod");
    case BinaryOpKind::Less:
      return (lhsType == TypeKind::Float || rhsType == TypeKind::Float) ? builder_->CreateFCmpOLT(lhs, rhs, "flt")
                                                                         : builder_->CreateICmpSLT(lhs, rhs, "ilt");
    case BinaryOpKind::LessEq:
      return (lhsType == TypeKind::Float || rhsType == TypeKind::Float) ? builder_->CreateFCmpOLE(lhs, rhs, "fle")
                                                                         : builder_->CreateICmpSLE(lhs, rhs, "ile");
    case BinaryOpKind::Greater:
      return (lhsType == TypeKind::Float || rhsType == TypeKind::Float) ? builder_->CreateFCmpOGT(lhs, rhs, "fgt")
                                                                         : builder_->CreateICmpSGT(lhs, rhs, "igt");
    case BinaryOpKind::GreaterEq:
      return (lhsType == TypeKind::Float || rhsType == TypeKind::Float) ? builder_->CreateFCmpOGE(lhs, rhs, "fge")
                                                                         : builder_->CreateICmpSGE(lhs, rhs, "ige");
    case BinaryOpKind::Eq:
      return (lhsType == TypeKind::Float || rhsType == TypeKind::Float) ? builder_->CreateFCmpOEQ(lhs, rhs, "feq")
                                                                         : builder_->CreateICmpEQ(lhs, rhs, "ieq");
    case BinaryOpKind::NotEq:
      return (lhsType == TypeKind::Float || rhsType == TypeKind::Float) ? builder_->CreateFCmpONE(lhs, rhs, "fne")
                                                                         : builder_->CreateICmpNE(lhs, rhs, "ine");
    case BinaryOpKind::LogicalAnd:
    case BinaryOpKind::LogicalOr:
      break;
  }
  return nullptr;
}

llvm::Value* CodeGenerator::codegenTernary(TernaryExpr& expr) {
  llvm::Function* fn = builder_->GetInsertBlock()->getParent();
  llvm::Value* cond = codegenExpr(*expr.condition);
  if (cond == nullptr) {
    return nullptr;
  }
  cond = toBool(cond, expr.condition->inferredType);
  if (cond == nullptr) {
    return nullptr;
  }

  llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context_, "ternary.then", fn);
  llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*context_, "ternary.else", fn);
  llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context_, "ternary.merge", fn);
  builder_->CreateCondBr(cond, thenBB, elseBB);

  builder_->SetInsertPoint(thenBB);
  llvm::Value* thenValue = codegenExpr(*expr.thenExpr);
  if (thenValue == nullptr) {
    return nullptr;
  }
  thenValue = castTo(expr.inferredType, thenValue, expr.thenExpr->inferredType);
  if (thenValue == nullptr) {
    return nullptr;
  }
  builder_->CreateBr(mergeBB);
  llvm::BasicBlock* thenEnd = builder_->GetInsertBlock();

  builder_->SetInsertPoint(elseBB);
  llvm::Value* elseValue = codegenExpr(*expr.elseExpr);
  if (elseValue == nullptr) {
    return nullptr;
  }
  elseValue = castTo(expr.inferredType, elseValue, expr.elseExpr->inferredType);
  if (elseValue == nullptr) {
    return nullptr;
  }
  builder_->CreateBr(mergeBB);
  llvm::BasicBlock* elseEnd = builder_->GetInsertBlock();

  builder_->SetInsertPoint(mergeBB);
  llvm::PHINode* phi = builder_->CreatePHI(llvmType(expr.inferredType), 2, "ternary.phi");
  phi->addIncoming(thenValue, thenEnd);
  phi->addIncoming(elseValue, elseEnd);
  return phi;
}

llvm::Value* CodeGenerator::castTo(TypeKind target, llvm::Value* value, TypeKind source) const {
  if (target == source || target == TypeKind::Invalid || source == TypeKind::Invalid) {
    return value;
  }
  if (target == TypeKind::Float && source == TypeKind::Int) {
    return builder_->CreateSIToFP(value, llvmType(TypeKind::Float), "sitofp");
  }
  if (target == TypeKind::Bool && source == TypeKind::Int) {
    return builder_->CreateICmpNE(value, llvm::ConstantInt::getSigned(llvmType(TypeKind::Int), 0), "i2b");
  }
  if (target == TypeKind::Bool && source == TypeKind::Float) {
    return builder_->CreateFCmpONE(value, llvm::ConstantFP::get(llvmType(TypeKind::Float), 0.0), "f2b");
  }
  std::cerr << "internal error: unsupported cast from " << typeToString(source) << " to "
            << typeToString(target) << "\n";
  return nullptr;
}

llvm::Value* CodeGenerator::toBool(llvm::Value* value, TypeKind source) const { return castTo(TypeKind::Bool, value, source); }

bool CodeGenerator::verify() const {
  if (llvm::verifyModule(*module_, &llvm::errs())) {
    std::cerr << "error: LLVM module verification failed\n";
    return false;
  }
  return true;
}

bool CodeGenerator::emitIR(const std::string& path) const {
  std::error_code ec;
  llvm::raw_fd_ostream os(path, ec, llvm::sys::fs::OF_Text);
  if (ec) {
    std::cerr << "error: cannot open IR file '" << path << "': " << ec.message() << "\n";
    return false;
  }
  module_->print(os, nullptr);
  return true;
}

std::unique_ptr<llvm::TargetMachine> CodeGenerator::createTargetMachine() {
  initializeLLVMTargets();
  std::string err;
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(targetTriple_, err);
  if (target == nullptr) {
    std::cerr << "error: target lookup failed for '" << targetTriple_ << "': " << err << "\n";
    return nullptr;
  }

  llvm::TargetOptions options;
  std::string cpu = "generic";
  std::string features;
  if (targetTriple_.rfind("riscv64", 0) == 0) {
    cpu = "generic-rv64";
    features = "+m,+a,+f,+d,+c";
    options.MCOptions.ABIName = "lp64d";
  }
  std::unique_ptr<llvm::TargetMachine> targetMachine(
      target->createTargetMachine(targetTriple_, cpu, features, options, llvm::Reloc::Static));
  if (!targetMachine) {
    std::cerr << "error: failed to create TargetMachine for '" << targetTriple_ << "'\n";
    return nullptr;
  }

  module_->setDataLayout(targetMachine->createDataLayout());
  module_->setTargetTriple(targetTriple_);
  return targetMachine;
}

bool CodeGenerator::emitAssembly(const std::string& path) {
  std::unique_ptr<llvm::TargetMachine> targetMachine = createTargetMachine();
  if (!targetMachine) {
    return false;
  }

  std::error_code ec;
  llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_Text);
  if (ec) {
    std::cerr << "error: cannot open assembly file '" << path << "': " << ec.message() << "\n";
    return false;
  }

  llvm::legacy::PassManager passManager;
  if (targetMachine->addPassesToEmitFile(passManager, dest, nullptr,
                                         llvm::CodeGenFileType::AssemblyFile)) {
    std::cerr << "error: target machine cannot emit assembly file\n";
    return false;
  }
  passManager.run(*module_);
  dest.flush();
  return true;
}

bool CodeGenerator::emitObject(const std::string& path) {
  std::unique_ptr<llvm::TargetMachine> targetMachine = createTargetMachine();
  if (!targetMachine) {
    return false;
  }

  std::error_code ec;
  llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
  if (ec) {
    std::cerr << "error: cannot open object file '" << path << "': " << ec.message() << "\n";
    return false;
  }

  llvm::legacy::PassManager passManager;
  if (targetMachine->addPassesToEmitFile(passManager, dest, nullptr,
                                         llvm::CodeGenFileType::ObjectFile)) {
    std::cerr << "error: target machine cannot emit object file\n";
    return false;
  }
  passManager.run(*module_);
  dest.flush();
  return true;
}

}  // namespace lorsc

