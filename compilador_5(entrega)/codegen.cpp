#include "codegen.hpp"
#include "ast.hpp"
#include <iostream>
#include <sstream>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

extern int yyparse();
extern FILE* yyin;
extern Program* programRoot;

// ─────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────

// Converte nome de tipo da linguagem para tipo LLVM
llvm::Type* CodeContext::getLLVMType(const std::string& typeName) {
    if (typeName == "int")    return builder.getInt32Ty();
    if (typeName == "float")  return builder.getFloatTy();
    if (typeName == "bool")   return builder.getInt1Ty();
    if (typeName == "void")   return builder.getVoidTy();
    // String e List: ponteiro para i8 por ora
    return llvm::PointerType::get(builder.getInt8Ty(), 0);
}

// Processa interpolação de string: "valor {x} ok" -> fmt="%d ok", vars=["x"]
std::pair<std::string, std::vector<std::string>>
CodeContext::parseInterpolation(const std::string& raw) {
    std::string fmt;
    std::vector<std::string> vars;

    // Remove as aspas externas se presentes
    std::string s = raw;
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        s = s.substr(1, s.size() - 2);

    size_t i = 0;
    while (i < s.size()) {
        // \n literal dentro da string
        if (s[i] == '\\' && i+1 < s.size() && s[i+1] == 'n') {
            fmt += '\n'; i += 2;
        }
        // %{ ou %} → chave literal
        else if (s[i] == '%' && i+1 < s.size() &&
                (s[i+1] == '{' || s[i+1] == '}')) {
            fmt += s[i+1]; i += 2;
        }
        // {varname} → placeholder
        else if (s[i] == '{') {
            size_t end = s.find('}', i);
            if (end != std::string::npos) {
                std::string varName = s.substr(i+1, end-i-1);
                vars.push_back(varName);
                // Tipo do placeholder depende do tipo da variável (int por padrão)
                fmt += "%d";
                i = end + 1;
            } else {
                fmt += s[i++];
            }
        }
        else {
            fmt += s[i++];
        }
    }
    return {fmt, vars};
}

// ─────────────────────────────────────────
//  GERAÇÃO DE EXPRESSÕES
// ─────────────────────────────────────────

llvm::Value* CodeContext::genExpr(ExprNode* node) {
    if (!node) return nullptr;

    // Literal inteiro
    if (auto* n = dynamic_cast<IntLiteral*>(node)) {
        return builder.getInt32(n->value);
    }

    // Literal string
    if (auto* n = dynamic_cast<StrLiteral*>(node)) {
        std::string s = n->value;
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            s = s.substr(1, s.size() - 2);
        return builder.CreateGlobalStringPtr(s);
    }

    // Variável
    if (auto* n = dynamic_cast<VarExpr*>(node)) {
        auto it = locals.find(n->name);
        if (it == locals.end()) {
            std::cerr << "Variável não declarada: " << n->name << std::endl;
            return builder.getInt32(0);
        }
        llvm::Type* ty = localTypes.count(n->name) ? localTypes[n->name] : builder.getInt32Ty();
        return builder.CreateLoad(ty, it->second, n->name);
    }

    // Atribuição como expressão
    if (auto* n = dynamic_cast<AssignExpr*>(node)) {
        llvm::Value* val = genExpr(n->value);
        auto it = locals.find(n->name);
        if (it != locals.end())
            builder.CreateStore(val, it->second);
        return val;
    }

    // Operação binária
    if (auto* n = dynamic_cast<BinaryOp*>(node)) {
        llvm::Value* l = genExpr(n->left);
        llvm::Value* r = genExpr(n->right);

        // Normaliza qualquer i1 para i32 antes de operar
        auto toI32 = [&](llvm::Value* v) -> llvm::Value* {
            if (v->getType() == builder.getInt1Ty())
                return builder.CreateZExt(v, builder.getInt32Ty(), "zext");
            return v;
        };

        // Normaliza para i1 (para && e ||)
        auto toBool = [&](llvm::Value* v) -> llvm::Value* {
            if (v->getType() == builder.getInt1Ty()) return v;
            return builder.CreateICmpNE(v, builder.getInt32(0), "tobool");
        };

        if (n->op == "+")  return builder.CreateAdd (toI32(l), toI32(r), "add");
        if (n->op == "-")  return builder.CreateSub (toI32(l), toI32(r), "sub");
        if (n->op == "*")  return builder.CreateMul (toI32(l), toI32(r), "mul");
        if (n->op == "/")  return builder.CreateSDiv(toI32(l), toI32(r), "div");
        if (n->op == "==") return builder.CreateICmpEQ (toI32(l), toI32(r), "eq");
        if (n->op == "!=") return builder.CreateICmpNE (toI32(l), toI32(r), "neq");
        if (n->op == "<")  return builder.CreateICmpSLT(toI32(l), toI32(r), "lt");
        if (n->op == ">")  return builder.CreateICmpSGT(toI32(l), toI32(r), "gt");
        if (n->op == "<=") return builder.CreateICmpSLE(toI32(l), toI32(r), "lte");
        if (n->op == ">=") return builder.CreateICmpSGE(toI32(l), toI32(r), "gte");
        if (n->op == "&&") {
            // Converte cada lado para i1 diretamente, sem passar por i32
            auto toBool = [&](llvm::Value* v) -> llvm::Value* {
                if (v->getType() == builder.getInt1Ty()) return v;
                return builder.CreateICmpNE(v, builder.getInt32(0), "tobool");
            };
            llvm::Value* lb = toBool(l);
            llvm::Value* rb = toBool(r);
            return builder.CreateAnd(lb, rb, "and");
        }
        if (n->op == "||") {
            auto toBool = [&](llvm::Value* v) -> llvm::Value* {
                if (v->getType() == builder.getInt1Ty()) return v;
                return builder.CreateICmpNE(v, builder.getInt32(0), "tobool");
            };
            llvm::Value* lb = toBool(l);
            llvm::Value* rb = toBool(r);
            return builder.CreateOr(lb, rb, "or");
        }
    }

    // Ternário: cond ? a : b
    if (auto* n = dynamic_cast<TernaryExpr*>(node)) {
        llvm::Value* cond = genExpr(n->cond);
        llvm::Function* fn = builder.GetInsertBlock()->getParent();
        auto* thenBB = llvm::BasicBlock::Create(context, "tern.then", fn);
        auto* elseBB = llvm::BasicBlock::Create(context, "tern.else");
        auto* mergeBB = llvm::BasicBlock::Create(context, "tern.merge");

        // Normaliza condição para i1
        if (cond->getType() != builder.getInt1Ty())
            cond = builder.CreateICmpNE(cond, builder.getInt32(0), "cond");
        builder.CreateCondBr(cond, thenBB, elseBB);

        builder.SetInsertPoint(thenBB);
        llvm::Value* thenVal = genExpr(n->thenExpr);
        builder.CreateBr(mergeBB);
        thenBB = builder.GetInsertBlock();

        fn->insert(fn->end(), elseBB);
        builder.SetInsertPoint(elseBB);
        llvm::Value* elseVal = genExpr(n->elseExpr);
        builder.CreateBr(mergeBB);
        elseBB = builder.GetInsertBlock();

        fn->insert(fn->end(), mergeBB);
        builder.SetInsertPoint(mergeBB);
        auto* phi = builder.CreatePHI(builder.getInt32Ty(), 2, "tern");
        phi->addIncoming(thenVal, thenBB);
        phi->addIncoming(elseVal, elseBB);
        return phi;
    }

    std::cerr << "Expressão não suportada ainda." << std::endl;
    return builder.getInt32(0);
}

// ─────────────────────────────────────────
//  GERAÇÃO DE STATEMENTS
// ─────────────────────────────────────────

void CodeContext::genStmt(StmtNode* node) {
    if (!node) return;

    // Bloco
    if (auto* n = dynamic_cast<BlockStmt*>(node)) {
        for (auto* s : n->stmts) genStmt(s);
        return;
    }

    // Declaração: int x  ou  int x = expr
    if (auto* n = dynamic_cast<DeclStmt*>(node)) {
        llvm::Type* ty = getLLVMType(n->type);
        llvm::Value* alloca = builder.CreateAlloca(ty, nullptr, n->name);
        locals[n->name] = alloca;
        localTypes[n->name] = ty;
        if (n->init) {
            llvm::Value* val = genExpr(n->init);
            builder.CreateStore(val, alloca);
        }
        return;
    }

    // Atribuição: x = expr
    if (auto* n = dynamic_cast<AssignStmt*>(node)) {
        llvm::Value* val = genExpr(n->value);
        auto it = locals.find(n->name);
        if (it == locals.end()) {
            std::cerr << "Variável não declarada: " << n->name << std::endl;
            return;
        }
        builder.CreateStore(val, it->second);
        return;
    }

    // Print com interpolação
    if (auto* n = dynamic_cast<PrintStmt*>(node)) {
        llvm::FunctionCallee printfFunc = module->getOrInsertFunction(
            "printf",
            llvm::FunctionType::get(builder.getInt32Ty(),
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0), true)
        );

        auto [fmt, vars] = parseInterpolation(n->format);
        llvm::Value* fmtVal = builder.CreateGlobalStringPtr(fmt);
        std::vector<llvm::Value*> args = {fmtVal};

        for (auto& varName : vars) {
            auto it = locals.find(varName);
            if (it != locals.end()) {
                llvm::Value* loaded = builder.CreateLoad(builder.getInt32Ty(), it->second, varName);
                args.push_back(loaded);
            } else {
                std::cerr << "Variável não encontrada no print: " << varName << std::endl;
                args.push_back(builder.getInt32(0));
            }
        }
        builder.CreateCall(printfFunc, args);
        return;
    }

    // Return
    if (auto* n = dynamic_cast<ReturnStmt*>(node)) {
        llvm::Value* val = genExpr(n->value);
        builder.CreateRet(val);
        return;
    }

    // If / if-else
    if (auto* n = dynamic_cast<IfStmt*>(node)) {
        llvm::Value* cond = genExpr(n->cond);
        llvm::Function* fn = builder.GetInsertBlock()->getParent();

        auto* thenBB  = llvm::BasicBlock::Create(context, "if.then", fn);
        auto* elseBB  = llvm::BasicBlock::Create(context, "if.else");
        auto* mergeBB = llvm::BasicBlock::Create(context, "if.end");

        // Normaliza condição para i1
        if (cond->getType() != builder.getInt1Ty())
            cond = builder.CreateICmpNE(cond, builder.getInt32(0), "cond");
        builder.CreateCondBr(cond, thenBB, n->elseBlock ? elseBB : mergeBB);

        builder.SetInsertPoint(thenBB);
        genStmt(n->thenBlock);
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(mergeBB);

        if (n->elseBlock) {
            fn->insert(fn->end(), elseBB);
            builder.SetInsertPoint(elseBB);
            genStmt(n->elseBlock);
            if (!builder.GetInsertBlock()->getTerminator())
                builder.CreateBr(mergeBB);
        }

        fn->insert(fn->end(), mergeBB);
        builder.SetInsertPoint(mergeBB);
        return;
    }

    // While
    if (auto* n = dynamic_cast<WhileStmt*>(node)) {
        llvm::Function* fn = builder.GetInsertBlock()->getParent();
        auto* condBB  = llvm::BasicBlock::Create(context, "while.cond", fn);
        auto* bodyBB  = llvm::BasicBlock::Create(context, "while.body", fn);
        auto* afterBB = llvm::BasicBlock::Create(context, "while.end",  fn);

        builder.CreateBr(condBB);
        builder.SetInsertPoint(condBB);
        llvm::Value* cond = genExpr(n->cond);
        // Normaliza condição para i1
        if (cond->getType() != builder.getInt1Ty())
            cond = builder.CreateICmpNE(cond, builder.getInt32(0), "cond");
        builder.CreateCondBr(cond, bodyBB, afterBB);

        builder.SetInsertPoint(bodyBB);
        genStmt(n->body);
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(condBB);

        builder.SetInsertPoint(afterBB);
        return;
    }

    // Do-while
    if (auto* n = dynamic_cast<DoWhileStmt*>(node)) {
        llvm::Function* fn = builder.GetInsertBlock()->getParent();
        auto* bodyBB  = llvm::BasicBlock::Create(context, "do.body",  fn);
        auto* afterBB = llvm::BasicBlock::Create(context, "do.end",   fn);

        builder.CreateBr(bodyBB);
        builder.SetInsertPoint(bodyBB);
        genStmt(n->body);
        llvm::Value* cond = genExpr(n->cond);
        // Normaliza condição para i1
        if (cond->getType() != builder.getInt1Ty())
            cond = builder.CreateICmpNE(cond, builder.getInt32(0), "cond");
        builder.CreateCondBr(cond, bodyBB, afterBB);

        builder.SetInsertPoint(afterBB);
        return;
    }

    // For
    if (auto* n = dynamic_cast<ForStmt*>(node)) {
        llvm::Function* fn = builder.GetInsertBlock()->getParent();
        auto* condBB  = llvm::BasicBlock::Create(context, "for.cond", fn);
        auto* bodyBB  = llvm::BasicBlock::Create(context, "for.body", fn);
        auto* stepBB  = llvm::BasicBlock::Create(context, "for.step", fn);
        auto* afterBB = llvm::BasicBlock::Create(context, "for.end",  fn);

        genStmt(n->init);
        builder.CreateBr(condBB);

        builder.SetInsertPoint(condBB);
        llvm::Value* cond = genExpr(n->cond);
        // Normaliza condição para i1
        if (cond->getType() != builder.getInt1Ty())
            cond = builder.CreateICmpNE(cond, builder.getInt32(0), "cond");
        builder.CreateCondBr(cond, bodyBB, afterBB);

        builder.SetInsertPoint(bodyBB);
        genStmt(n->body);
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(stepBB);

        builder.SetInsertPoint(stepBB);
        genStmt(n->step);
        builder.CreateBr(condBB);

        builder.SetInsertPoint(afterBB);
        return;
    }

    std::cerr << "Statement não suportado ainda." << std::endl;
}

// ─────────────────────────────────────────
//  GERAÇÃO DE FUNÇÕES
// ─────────────────────────────────────────

void CodeContext::genFunction(FuncDecl* func) {
    // Monta tipos dos parâmetros
    std::vector<llvm::Type*> paramTypes;
    for (auto* p : func->params)
        paramTypes.push_back(getLLVMType(p->type));

    llvm::Type* retType = getLLVMType(func->returnType);
    llvm::FunctionType* ft = llvm::FunctionType::get(retType, paramTypes, false);
    llvm::Function* fn = llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, func->name, module.get());

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", fn);
    builder.SetInsertPoint(entry);

    // Mapeia parâmetros para locals
    locals.clear();
    size_t idx = 0;
    for (auto& arg : fn->args()) {
        ParamNode* p = func->params[idx++];
        arg.setName(p->name);
        llvm::Value* alloca = builder.CreateAlloca(arg.getType(), nullptr, p->name);
        builder.CreateStore(&arg, alloca);
        locals[p->name] = alloca;
    }

    // Gera o corpo
    genStmt(func->body);

    // Se a função não terminou com return, adiciona um padrão
    if (!builder.GetInsertBlock()->getTerminator()) {
        if (func->returnType == "void")
            builder.CreateRetVoid();
        else
            builder.CreateRet(builder.getInt32(0));
    }

    llvm::verifyFunction(*fn);
}

// ─────────────────────────────────────────
//  PONTO DE ENTRADA DO CODEGEN
// ─────────────────────────────────────────

void CodeContext::generateCode(const std::string& outputFile) {
    if (!programRoot) {
        std::cerr << "AST vazia, nada a gerar." << std::endl;
        return;
    }

    for (auto* func : programRoot->functions)
        genFunction(func);

    // Salva o IR em arquivo
    std::error_code EC;
    llvm::raw_fd_ostream dest(outputFile, EC, llvm::sys::fs::OF_None);
    if (EC) {
        std::cerr << "Erro ao abrir arquivo de saída: " << EC.message() << std::endl;
        return;
    }
    module->print(dest, nullptr);
    dest.flush();
    std::cerr << "LLVM IR gerado: " << outputFile << std::endl;
}

// ─────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: ./dantec arquivo.dante" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    if (filename.find(".dante") == std::string::npos) {
        std::cerr << "Erro: apenas arquivos .dante são aceitos." << std::endl;
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        std::cerr << "Erro ao abrir o arquivo: " << argv[1] << std::endl;
        return 1;
    }
    yyin = file;

    std::cerr << "--- Iniciando Análise Sintática ---" << std::endl;
    if (yyparse() != 0) {
        std::cerr << "Falha na compilação." << std::endl;
        return 1;
    }
    std::cerr << "Análise concluída! Gerando código..." << std::endl;

    std::string base = filename.substr(0, filename.find(".dante"));
    std::string llFile  = base + ".ll";
    std::string objFile = base + ".o";
    std::string outFile = base;

    CodeContext ctx;
    ctx.generateCode(llFile);

    std::string cmd1 = "llc -filetype=obj " + llFile + " -o " + objFile;
    std::string cmd2 = "gcc -no-pie " + objFile + " -o " + outFile;

    std::cerr << "Compilando IR..." << std::endl;
    if (system(cmd1.c_str()) != 0) { std::cerr << "Erro no llc." << std::endl; return 1; }
    if (system(cmd2.c_str()) != 0) { std::cerr << "Erro no gcc." << std::endl; return 1; }

    std::cerr << "✓ Executável gerado: ./" << outFile << std::endl;
    return 0;
}
