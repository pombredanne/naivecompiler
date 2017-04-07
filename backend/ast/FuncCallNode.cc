#include "FuncCallNode.h"
#include "NodeFactory.h"
#include "Compiler.h"

namespace naivescript
{

bool FuncCallNode::Parse( struct serialize::FuncCall * func_call, size_t size )  
{
    std::cout << "Parsing FuncCall: ";
    func_name = Compiler::GetCompiler()->ResolveSymbol(func_call->id);
    return_type = func_call->return_type;
    uint8_t *p_args = (uint8_t *)(func_call->args.data);
    args = NodeFactory::CreateArgumentLists(p_args, func_call->argsSize);
    children.push_back(args);
    return true;
}

llvm::Value* FuncCallNode::accept(Visitor* v)
{
    return v->visit(this);
}

}
