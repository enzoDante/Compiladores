#include "codegen.hpp"
#include <iostream>

// Declarando o parser do Bison e a entrada do Flex
extern int yyparse();
extern FILE* yyin;

void CodeContext::generateCode() {
    // Exemplo: Criando a função Main
    FunctionType *funcType = FunctionType::get(builder.getInt32Ty(), false);
    Function *mainFunc = Function::Create(funcType, Function::ExternalLinkage, "main", module.get());
    BasicBlock *entry = BasicBlock::Create(context, "entry", mainFunc);
    builder.SetInsertPoint(entry);

    // Aqui entraria a lógica de percorrer sua AST
    
    builder.CreateRet(builder.getInt32(0));
    module->print(outs(), nullptr); // Imprime o código LLVM IR no terminal
}

// PONTO DE ENTRADA DO SEU COMPILADOR
int main(int argc, char** argv) {
    if (argc > 1) {
        FILE* file = fopen(argv[1], "r");
        if (!file) {
            std::cerr << "Erro ao abrir o arquivo: " << argv[1] << std::endl;
            return 1;
        }
        yyin = file;
    }

    std::cout << "--- Iniciando Análise Sintática ---" << std::endl;
    
    if (yyparse() == 0) {
        std::cout << "Análise concluída com sucesso! Gerando código LLVM IR...\n" << std::endl;
        
        CodeContext contexto;
        contexto.generateCode();
    } else {
        std::cerr << "Falha na compilação devido a erros sintáticos." << std::endl;
        return 1;
    }

    return 0;
}
