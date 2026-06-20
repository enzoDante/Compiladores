#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <map>
#include <vector>

using namespace llvm;

class CodeContext {
public:
    LLVMContext context;
    IRBuilder<> builder;
    std::unique_ptr<Module> module;
    std::map<std::string, Value*> locals;

    CodeContext() : builder(context) {
        module = std::make_unique<Module>("minha_lang", context);
    }
    
    void generateCode();
};