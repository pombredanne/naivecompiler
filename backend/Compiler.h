#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <iostream>
#include "SerializeFile.h"
#include "FunctionList.h"
#include "NodeVisitor.h"
#include "CodeGen.h"

namespace naivescript
{

class Compiler 
{
public:
    Compiler() {}
    
    static Compiler * GetCompiler() {
        if (!instance) {
            instance = new Compiler();
        }
        return instance;
    }

    void LoadData(const std::string& path) {
        file.Load(path);
        symbol_table = file.GetSymbolTable();
    };

    static const std::string& ResolveSymbol(uint32_t id) {
        return GetCompiler()->symbol_table->ResolveSymbol(id);
    }

    void Compile( void ) {
        char *body = const_cast<char *>(file.GetBody());
        func_list.Parse(reinterpret_cast<struct serialize::FunctionList*>(body),
        file.GetBodySize() );
        func_list.show();
        //genVisitor.visit(&func_list);
        genVisitor.run(&func_list);
    }
    
    static Compiler* instance;
private:
    const serialize::SymbolTable* symbol_table;
    FunctionList func_list;
    serialize::SerializeFile file;
    CodeGenVisitor genVisitor;
};

}

#endif
