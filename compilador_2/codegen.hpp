#pragma once
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <map>
#include <vector>
#include <string>
#include "ast.hpp"

using namespace llvm;

class CodeContext {
public:
    LLVMContext context;
    IRBuilder<> builder;
    std::unique_ptr<Module> module;
    std::map<std::string, Value*> locals;

    CodeContext() : builder(context) {
        module = std::make_unique<Module>("dante_lang", context);
    }

    // Ponto de entrada
    void generateCode(const std::string& outputFile);

    // Percorre a AST
    void      genFunction(FuncDecl* func);
    void      genStmt(StmtNode* node);
    Value*    genExpr(ExprNode* node);

    // Helpers
    Type* getLLVMType(const std::string& typeName);
    std::pair<std::string, std::vector<std::string>>
        parseInterpolation(const std::string& raw);
};
