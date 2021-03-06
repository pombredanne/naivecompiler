#include "CodeGen.h"
#include "ASTNode.h"
#include "Statement.h"
#include "AST.h"
#include "FuncDefNode.h"
#include "FuncDeclNode.h"
#include "FuncCallNode.h"
#include "Declaration.h"
#include "DeclarationList.h"
#include "StmtList.h"
#include "AssignmentNode.h"
#include "ReturnNode.h"
#include "ValueNode.h"
#include "BinaryOpNode.h"
#include "SymbolNode.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"


namespace naivecompiler{

static  llvm::LLVMContext TheContext;
static  llvm::IRBuilder<> Builder(TheContext);
static  std::unique_ptr<llvm::Module> owner = llvm::make_unique<llvm::Module>("naivescript", TheContext);
static  llvm::Module *TheModule = owner.get();

llvm::Value *LogErrorV(const char *Str) {
  std::cerr << Str << std::endl;
  return nullptr;
}

void CodeGenVisitor::dump( AST *node) {
    auto funcs = node->accept(this);
    //TheModule->dump();
    std::string out;
    llvm::raw_ostream* stream;
    stream = new llvm::raw_string_ostream(out);
    TheModule->print(*stream, nullptr);
    std::cout << out << std::endl;
}

void CodeGenVisitor::GenObj(const std::string &Filename) {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
    auto TargetTriple = llvm::sys::getDefaultTargetTriple();
    TheModule->setTargetTriple(TargetTriple);

    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

    if (!Target) {
        std::cout << Error << std::endl;
        return;
    }

  auto CPU = "generic";
  auto Features = "";

  llvm::TargetOptions opt;
  //auto RM = llvm::Optional<llvm::Reloc::Model>();
  auto RM = llvm::Reloc::Model();
  auto TheTargetMachine =
      Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

  TheModule->setDataLayout(TheTargetMachine->createDataLayout());

  std::error_code EC;
  llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::F_None);

  if (EC) {
    std::cout << "Could not open file: " << EC.message() << std::endl;;
    return;
  }

  llvm::legacy::PassManager pass;
  auto FileType = llvm::TargetMachine::CGFT_ObjectFile;

  if (TheTargetMachine->addPassesToEmitFile(pass, dest, FileType)) {
    std::cout << "TheTargetMachine can't emit a file of this type" << std::endl;
    return;
  }

  pass.run(*TheModule);
  dest.flush();

  std::cout << "Wrote " << Filename << "\n";
}

void CodeGenVisitor::run(AST *node)
{
    auto funcs = node->accept(this);
    //TheModule->dump();
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    //std::unique_ptr<llvm::Module> mod = llvm::make_unique<llvm::Module>(TheModule);
    llvm::ExecutionEngine *engine = llvm::EngineBuilder(std::move(owner)).create();
    llvm::Function * func;
    engine->finalizeObject(); // memory for generated code marked executable:
    for (const auto &f : funcs ) {
        if (f.first == "main") {
            if (f.second){
                func = static_cast<llvm::Function*>(f.second);
                engine->runFunction(func, std::vector<llvm::GenericValue>());
            }
        }
    }
}

std::map<std::string, llvm::Value*> CodeGenVisitor::visit(AST *node)
{
    std::map<std::string, llvm::Value*> funcs;
    for (ASTNode * f : node->GetChildren() ) {
        auto * func = f->accept(this);
        if (func) {
            FuncDeclNode * fnode = static_cast<FuncDeclNode *>(f);
            funcs[fnode->GetFuncName()] = func;
        }
    }
    return funcs;
}

llvm::Value* CodeGenVisitor::visit(FuncDeclNode *func)
{
    auto decls = func->GetParams()->GetChildren();
    std::vector<std::string> params_names;

    for (ASTNode* decl : decls) {
        params_names.push_back(static_cast<Declaration*>(decl)->GetID());
    }

    std::vector<llvm::Type *> Args(decls.size(),
        llvm::Type::getInt32Ty(TheContext));

    llvm::FunctionType *FT =
      llvm::FunctionType::get(llvm::Type::getInt32Ty(TheContext), Args, false);

    llvm::Function *TheFunction =
      llvm::Function::Create(FT, llvm::Function::ExternalLinkage, func->GetFuncName(),
        TheModule);
        
    return TheFunction;
}

llvm::Value* CodeGenVisitor::visit(FuncDefNode *func)
{
    auto decls = func->GetParams()->GetChildren();
    std::vector<std::string> params_names;

    for (ASTNode* decl : decls) {
        params_names.push_back(static_cast<Declaration*>(decl)->GetID());
    }

    std::vector<llvm::Type *> Args(decls.size(),
        llvm::Type::getInt32Ty(TheContext));

    llvm::FunctionType *FT;
    uint32_t return_type = func->GetReturnType();

    switch(return_type) {
    case serialize::ValueType::CONSTINT:
    	FT=  llvm::FunctionType::get(llvm::Type::getInt32Ty(TheContext), Args, false);
    	break;
    case serialize::ValueType::VOID:
    	FT = llvm::FunctionType::get(nullptr, Args, false);
    	break;
    default:
    	FT = llvm::FunctionType::get(nullptr, Args, false);
    }

    llvm::Function *TheFunction =
      llvm::Function::Create(FT, llvm::Function::ExternalLinkage, func->GetFuncName(),
        TheModule);

    llvm::BasicBlock *Entry = llvm::BasicBlock::Create(TheContext, "entry", TheFunction);
    Builder.SetInsertPoint(Entry);

    uint32_t Idx = 0;
    for (auto &Arg : TheFunction->args()) {
        Arg.setName(params_names[Idx++]);
        llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
            TheFunction->getEntryBlock().begin());
        llvm::AllocaInst *Alloca =  TmpB.CreateAlloca(llvm::Type::getInt32Ty(TheContext), nullptr, Arg.getName());
        Builder.CreateStore(&Arg, Alloca);
        // Add arguments to variable symbol table.
        NamedValues[Arg.getName()] = Alloca;
    }
    func->GetBody()->accept(this);
    //llvm::Value *retval = func->GetBody()->accept(this);
    // llvm::BasicBlock *End = llvm::BasicBlock::Create(TheContext, "End", TheFunction);
    // Builder.SetInsertPoint(End);
    //Builder.CreateRet(retval);
    return TheFunction;
}

llvm::Value* CodeGenVisitor::visit(FuncCallNode *node)
{
    llvm::Function *CalleeF = TheModule->getFunction(node->GetFuncName());
    if (!CalleeF)
        return LogErrorV("Unknown function referenced");

    auto Args = node->GetArgs()->GetChildren();
    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<llvm::Value *> ArgsV;
    for (ASTNode * n : Args) {
        ArgsV.push_back(n->accept(this));
    }

    return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Value* CodeGenVisitor::visit(Declaration *node)
{
    llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                   TheFunction->getEntryBlock().begin());
    llvm::AllocaInst *Alloca =  TmpB.CreateAlloca(llvm::Type::getInt32Ty(TheContext), nullptr, node->GetID());
    std::cout << "Alloca " << node->GetID() << std::endl;
    NamedValues[node->GetID()] = Alloca;
    return nullptr;
}

llvm::Value* CodeGenVisitor::visit(DeclarationList *node)
{
    if ( ! node->GetChildren().size() ) {
        return LogErrorV("Empty StmtList");
    }
    for (auto decl : node->GetChildren()) {
        decl->accept(this);
    }
    return nullptr;
}

llvm::Value* CodeGenVisitor::visit(StmtList * stmtlist) {
    llvm::Value* retval = llvm::Constant::getNullValue(llvm::Type::getInt32Ty(TheContext));
    if ( ! stmtlist->GetChildren().size() ) {
        return LogErrorV("Empty StmtList");
    }
    for (ASTNode * node : stmtlist->GetChildren() ) {
        Statement* stmt = static_cast<Statement *>(node);
        serialize::NodeType type = stmt->GetNodeType();
        switch(type) {
            case serialize::TypeReturnStmt:
                //return stmt->accept(this);
            	stmt->accept(this);
                break;
            case serialize::TypeABSJMP:
                stmt->accept(this);
                break;
            case serialize::TypeCMPJMP:
                stmt->accept(this);
                break;
            case serialize::TypeLabel:
                stmt->accept(this);
                break;
            case serialize::TypeFuncCall:
                stmt->accept(this);
                break;
            case serialize::TypeDeclaration:
                stmt->accept(this);
                break;
            default:
                stmt->accept(this);
                break;
        }
    }
    return retval;
}

llvm::Value* CodeGenVisitor::visit(CMPJMPNode *node)
{
    //auto ParentBlock = Builder.GetInsertBlock()->getParent();
    uint32_t id1 = node->GetID1();
    uint32_t id2 = node->GetID2();

    llvm::BasicBlock *L1,*L2;
    if (BlockMap.count(id1)) {
        L1 = BlockMap.at(id1);
    } else {
        std::string name1 = "L" + std::to_string(id1);
        L1 = llvm::BasicBlock::Create(TheContext, name1);
        //ParentBlock->getBasicBlockList().push_back(L1);
        BlockMap[id1] = L1;
    }

    if (BlockMap.count(id2)) {
        L2 = BlockMap.at(id2);
    } else {
        std::string name2 = "L" + std::to_string(id2);
        L2 = llvm::BasicBlock::Create(TheContext, name2);
        //ParentBlock->getBasicBlockList().push_back(L2);
        BlockMap[id2] = L2;
    }

    llvm::Value* cond = node->GetExpr()->accept(this);
    cond = Builder.CreateIntCast(cond, llvm::Type::getInt32Ty(TheContext), false);
    cond = Builder.CreateICmpNE(cond,
         llvm::ConstantInt::get(TheContext, llvm::APInt(32, 0, false)));

    Builder.CreateCondBr(cond, L1, L2);
    
    return nullptr;
}

llvm::Value* CodeGenVisitor::visit(LabelNode *node)
{
    auto ParentBlock = Builder.GetInsertBlock()->getParent();
    uint32_t id = node->GetID();
    llvm::BasicBlock * L;
    std::string name = "L" + std::to_string(id);
    if (BlockMap.count(id)) {
        L = BlockMap.at(id);
    } else {
        L = llvm::BasicBlock::Create(TheContext, name);
        BlockMap[id] = L;
    }
    ParentBlock->getBasicBlockList().push_back(L);
    Builder.SetInsertPoint(L);
    return nullptr;
}

llvm::Value* CodeGenVisitor::visit(ABSJMPNode *node)
{
    //auto ParentBlock = Builder.GetInsertBlock()->getParent();
    uint32_t id = node->GetID();
    llvm::BasicBlock * L;
    std::string name = "L" + std::to_string(id);
    if (BlockMap.count(id)) {
        L = BlockMap.at(id);
    } else {
        L = llvm::BasicBlock::Create(TheContext, name);
        //ParentBlock->getBasicBlockList().push_back(L);
        BlockMap[id] = L;
    }
    Builder.CreateBr(L);
    return nullptr;
}

llvm::Value* CodeGenVisitor::visit(AssignmentNode *node) 
{
    std::cout << "Assignment Var: " << node->GetID() <<  std::endl;
    llvm::Value * tmpval = node->GetExpr()->accept(this);
    auto Variable = NamedValues.at(node->GetID());
    Builder.CreateStore(tmpval, Variable);
    return tmpval;
}

llvm::Value* CodeGenVisitor::visit(ReturnNode *node) 
{
	ASTNode * expr = node->GetExpr();
	if (expr) {
		llvm::Value * retval = expr->accept(this);
		Builder.CreateRet(retval);
		return retval;
	}
	Builder.CreateRetVoid();
    return nullptr;
}

llvm::Value* CodeGenVisitor::visit(BinaryOpNode *node) 
{
  llvm::Value *L = node->GetLHS()->accept(this);
  llvm::Value *R = node->GetRHS()->accept(this);
  if (!L || !R)
    return nullptr;

  switch (node->GetOp()) {
  case serialize::BinaryOpType::ADDOP:
    return Builder.CreateAdd(L, R, "addtmp");
  case serialize::BinaryOpType::MINUSOP:
    return Builder.CreateSub(L, R, "subtmp");
  case serialize::BinaryOpType::TIMESOP:
    return Builder.CreateMul(L, R, "multmp");
  case serialize::BinaryOpType::GTOP:
    L = Builder.CreateICmpUGT(L, R, "cmptmp");
    //L->dump();
    return L;
  case serialize::BinaryOpType::LTOP:
    L = Builder.CreateICmpULT(L, R, "cmptmp");
    //L->dump();
    return L;
  default:
    return LogErrorV("invalid binary operator");
  }
}

llvm::Value* CodeGenVisitor::visit(SymbolNode *node) 
{
    std::string symbol = node->GetSymbol();
    std::cout << "Use Of Var: " << symbol <<  std::endl;
    if (!NamedValues.count(symbol)) {
        return LogErrorV("Using Uninitialize Variable");
    }
    auto Val = NamedValues.at(symbol);
    return Builder.CreateLoad(Val, symbol);
}

//TODO: Support More Value Types
llvm::Value* CodeGenVisitor::visit(ValueNode *node)
{
    //llvm::Value* tmp = ConstantFP::get(TheContext, APFloat(static_cast<float>(node->GetVal())));
	// TODO: Default signed int
	if (node->GetValType() == serialize::ValueType::CONSTINT) {
		node = static_cast<IntegerNode *>(node);
		llvm::Value* tmp = llvm::ConstantInt::get(TheContext, llvm::APInt(32, node->GetVal(), false));
    //tmp->dump();
		return tmp;
	}
	return nullptr;
}

}
