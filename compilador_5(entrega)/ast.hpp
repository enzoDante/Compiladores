#pragma once
#include <string>
#include <vector>
#include <memory>

// Forward declarations
struct Node;
struct ExprNode;
struct StmtNode;

using NodePtr = Node*;
using ExprPtr = ExprNode*;
using StmtPtr = StmtNode*;

// ─────────────────────────────────────────
//  BASE
// ─────────────────────────────────────────

struct Node {
    virtual ~Node() = default;
};

struct ExprNode : Node {};
struct StmtNode : Node {};

// ─────────────────────────────────────────
//  EXPRESSÕES
// ─────────────────────────────────────────

// Literal inteiro: 42
struct IntLiteral : ExprNode {
    int value;
    IntLiteral(int v) : value(v) {}
};

// Literal string: "hello"
struct StrLiteral : ExprNode {
    std::string value;
    StrLiteral(const std::string& v) : value(v) {}
};

// Variável: x
struct VarExpr : ExprNode {
    std::string name;
    VarExpr(const std::string& n) : name(n) {}
};

// Operação binária: a + b, a == b, a && b
struct BinaryOp : ExprNode {
    std::string op;
    ExprPtr left;
    ExprPtr right;
    BinaryOp(const std::string& op, ExprPtr l, ExprPtr r)
        : op(op), left(l), right(r) {}
};

// Atribuição como expressão: x = 5
struct AssignExpr : ExprNode {
    std::string name;
    ExprPtr value;
    AssignExpr(const std::string& n, ExprPtr v) : name(n), value(v) {}
};

// Operador ternário: cond ? a : b
struct TernaryExpr : ExprNode {
    ExprPtr cond;
    ExprPtr thenExpr;
    ExprPtr elseExpr;
    TernaryExpr(ExprPtr c, ExprPtr t, ExprPtr e)
        : cond(c), thenExpr(t), elseExpr(e) {}
};

// Acesso a lista: arr[i]
struct IndexExpr : ExprNode {
    ExprPtr list;
    ExprPtr index;
    IndexExpr(ExprPtr l, ExprPtr i) : list(l), index(i) {}
};

// ─────────────────────────────────────────
//  STATEMENTS
// ─────────────────────────────────────────

// Bloco: { stmt1; stmt2; }
struct BlockStmt : StmtNode {
    std::vector<StmtPtr> stmts;
    BlockStmt(std::vector<StmtPtr> s) : stmts(std::move(s)) {}
};

// Declaração: int x  ou  int x = expr
struct DeclStmt : StmtNode {
    std::string type;
    std::string name;
    ExprPtr init; // pode ser nullptr
    DeclStmt(const std::string& t, const std::string& n, ExprPtr i = nullptr)
        : type(t), name(n), init(i) {}
};

// Atribuição como statement: x = expr;
struct AssignStmt : StmtNode {
    std::string name;
    ExprPtr value;
    AssignStmt(const std::string& n, ExprPtr v) : name(n), value(v) {}
};

// Print: print("msg {var}");
struct PrintStmt : StmtNode {
    std::string format; // a string com {var}
    PrintStmt(const std::string& f) : format(f) {}
};

// Return: return expr;
struct ReturnStmt : StmtNode {
    ExprPtr value;
    ReturnStmt(ExprPtr v) : value(v) {}
};

// If / if-else
struct IfStmt : StmtNode {
    ExprPtr cond;
    StmtPtr thenBlock;
    StmtPtr elseBlock; // pode ser nullptr
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e = nullptr)
        : cond(c), thenBlock(t), elseBlock(e) {}
};

// While
struct WhileStmt : StmtNode {
    ExprPtr cond;
    StmtPtr body;
    WhileStmt(ExprPtr c, StmtPtr b) : cond(c), body(b) {}
};

// Do-while
struct DoWhileStmt : StmtNode {
    StmtPtr body;
    ExprPtr cond;
    DoWhileStmt(StmtPtr b, ExprPtr c) : body(b), cond(c) {}
};

// For
struct ForStmt : StmtNode {
    AssignStmt* init;
    ExprPtr cond;
    AssignStmt* step;
    StmtPtr body;
    ForStmt(AssignStmt* i, ExprPtr c, AssignStmt* s, StmtPtr b)
        : init(i), cond(c), step(s), body(b) {}
};

// Foreach: foreach(int x in lista)
struct ForeachStmt : StmtNode {
    std::string type;
    std::string varName;
    std::string listName;
    StmtPtr body;
    ForeachStmt(const std::string& t, const std::string& v,
                const std::string& l, StmtPtr b)
        : type(t), varName(v), listName(l), body(b) {}
};

// Expressão como statement (ex: atribuição)
struct ExprStmt : StmtNode {
    ExprPtr expr;
    ExprStmt(ExprPtr e) : expr(e) {}
};

// ─────────────────────────────────────────
//  FUNÇÃO E PROGRAMA
// ─────────────────────────────────────────

struct ParamNode : Node {
    std::string type;
    std::string name;
    ParamNode(const std::string& t, const std::string& n) : type(t), name(n) {}
};

struct FuncDecl : Node {
    std::string returnType;
    std::string name;
    std::vector<ParamNode*> params;
    BlockStmt* body;
    FuncDecl(const std::string& rt, const std::string& n,
              std::vector<ParamNode*> p, BlockStmt* b)
        : returnType(rt), name(n), params(std::move(p)), body(b) {}
};

struct Program : Node {
    std::vector<FuncDecl*> functions;
    Program(std::vector<FuncDecl*> f) : functions(std::move(f)) {}
};
