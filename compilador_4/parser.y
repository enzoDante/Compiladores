%{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string> // Adicionado para evitar problemas com std::string
    #include <vector>
    #include "ast.hpp"

    extern int yylex();
    void yyerror(const char *s) { printf("Erro sintático: %s\n", s); }
    
    // Raiz da AST — preenchida pelo parser, lida pelo codegen
    Program* programRoot = nullptr;
%}

%union {
    std::string*            string;
    int                     token;
    ExprNode*               expr;
    StmtNode*               stmt;
    BlockStmt*              block;
    FuncDecl*               func;
    ParamNode*              param;
    DeclStmt*               decl;
    AssignStmt*             assign;
    std::vector<StmtNode*>* stmtList;
    std::vector<FuncDecl*>* funcList;
    std::vector<ParamNode*>* paramList;
}

/* Tokens com valor string */
%token <string> TID TINTEGER TSTRING

/* Tokens simples */
%token <token> TINT TFLOAT TBOOL TSTRINGTYPE TLIST
%token <token> TVOID TIF TELSE TFOR TWHILE TDO TSWITCH TCASE TFOREACH TIN
%token <token> TPRINT TINPUT TRETURN TMAIN TCEQ TEQUAL TARROW TTERNARY TAND TOR
%token <token> TSEMI TLBRACE TRBRACE TLPAREN TRPAREN TCOMMA TCOLON
%token <token> TLBRACKET TRBRACKET
%token <token> TPLUS TMINUS TMUL TDIV
%token <token> TLT TGT TLTE TGTE TNEQ

/* Tipos retornados pelas regras */
%type <expr>      expression
%type <stmt>      statement if_stmt while_stmt do_stmt for_stmt foreach_stmt
%type <stmt>      print_stmt return_stmt
%type <block>     compound_stmt
%type <stmtList>  statements
%type <func>      func_decl
%type <funcList>  program
%type <param>     arg
%type <paramList> args arg_list
%type <string>    type
%type <assign>    assignment
%type <decl>      declaration

/* Precedência (resolve shift/reduce) */
%right TEQUAL
%left  TOR
%left  TAND
%left  TCEQ
%left  TPLUS TMINUS
%left  TMUL TDIV
%right TTERNARY
%left  TLBRACKET

%start program_root

%%

program_root:
    program   { programRoot = new Program(*$1); delete $1; }
    ;

program:
    func_decl             { $$ = new std::vector<FuncDecl*>(); $$->push_back($1); }
    | program func_decl   { $$ = $1; $$->push_back($2); }
    ;

func_decl:
    type TID TLPAREN args TRPAREN compound_stmt
        { $$ = new FuncDecl(*$1, *$2, *$4, $6); delete $1; delete $2; delete $4; }
    | TINT TMAIN TLPAREN TRPAREN compound_stmt
        { $$ = new FuncDecl("int", "main", {}, $5); }
    ;

type:
    TINT        { $$ = new std::string("int"); }
    | TFLOAT    { $$ = new std::string("float"); }
    | TBOOL     { $$ = new std::string("bool"); }
    | TSTRINGTYPE { $$ = new std::string("String"); }
    | TLIST     { $$ = new std::string("List"); }
    | TVOID     { $$ = new std::string("void"); }
    ;

args:
    /* vazio */  { $$ = new std::vector<ParamNode*>(); }
    | arg_list   { $$ = $1; }
    ;

arg_list:
    arg                    { $$ = new std::vector<ParamNode*>(); $$->push_back($1); }
    | arg_list TCOMMA arg  { $$ = $1; $$->push_back($3); }
    ;

arg:
    type TID  { $$ = new ParamNode(*$1, *$2); delete $1; delete $2; }
    ;

compound_stmt:
    TLBRACE statements TRBRACE  { $$ = new BlockStmt(*$2); delete $2; }
    ;

statements:
    /* vazio */           { $$ = new std::vector<StmtNode*>(); }
    | statements statement { $$ = $1; if ($2) $$->push_back($2); }
    ;

statement:
    declaration TSEMI     { $$ = $1; }
    | assignment TSEMI    { $$ = $1; }
    | if_stmt             { $$ = $1; }
    | while_stmt          { $$ = $1; }
    | do_stmt             { $$ = $1; }
    | for_stmt            { $$ = $1; }
    | foreach_stmt        { $$ = $1; }
    | print_stmt TSEMI    { $$ = $1; }
    | return_stmt TSEMI   { $$ = $1; }
    ;

declaration:
    type TID
        { $$ = new DeclStmt(*$1, *$2, nullptr); delete $1; delete $2; }
    | type TID TEQUAL expression
        { $$ = new DeclStmt(*$1, *$2, $4); delete $1; delete $2; }
    ;

assignment:
    TID TEQUAL expression
        { $$ = new AssignStmt(*$1, $3); delete $1; }
    ;

if_stmt:
    TIF TLPAREN expression TRPAREN compound_stmt
        { $$ = new IfStmt($3, $5, nullptr); }
    | TIF TLPAREN expression TRPAREN compound_stmt TELSE compound_stmt
        { $$ = new IfStmt($3, $5, $7); }
    ;

while_stmt:
    TWHILE TLPAREN expression TRPAREN compound_stmt
        { $$ = new WhileStmt($3, $5); }
    ;

do_stmt:
    TDO compound_stmt TWHILE TLPAREN expression TRPAREN TSEMI
        { $$ = new DoWhileStmt($2, $5); }
    ;

for_stmt:
    TFOR TLPAREN assignment TSEMI expression TSEMI assignment TRPAREN compound_stmt
        { $$ = new ForStmt($3, $5, $7, $9); }
    ;

foreach_stmt:
    TFOREACH TLPAREN type TID TIN TID TRPAREN compound_stmt
        { $$ = new ForeachStmt(*$3, *$4, *$6, $8); delete $3; delete $4; delete $6; }
    ;

print_stmt:
    TPRINT TLPAREN TSTRING TRPAREN
        { $$ = new PrintStmt(*$3); delete $3; }
    ;

return_stmt:
    TRETURN expression  { $$ = new ReturnStmt($2); }
    ;

expression:
    TID                              { $$ = new VarExpr(*$1); delete $1; }
    | TINTEGER                       { $$ = new IntLiteral(std::stoi(*$1)); delete $1; }
    | TSTRING                        { $$ = new StrLiteral(*$1); delete $1; }
    | assignment                     { $$ = new AssignExpr($1->name, $1->value); delete $1; }
    | expression TPLUS  expression   { $$ = new BinaryOp("+", $1, $3); }
    | expression TMINUS expression   { $$ = new BinaryOp("-", $1, $3); }
    | expression TMUL   expression   { $$ = new BinaryOp("*", $1, $3); }
    | expression TDIV   expression   { $$ = new BinaryOp("/", $1, $3); }
    | expression TCEQ   expression   { $$ = new BinaryOp("==", $1, $3); }
    | expression TAND   expression   { $$ = new BinaryOp("&&", $1, $3); }
    | expression TOR    expression   { $$ = new BinaryOp("||", $1, $3); }
    | expression TTERNARY expression TCOLON expression
                                     { $$ = new TernaryExpr($1, $3, $5); }
    | expression TLBRACKET expression TRBRACKET
                                     { $$ = new IndexExpr($1, $3); }
    | TLPAREN expression TRPAREN     { $$ = $2; }
    | expression TLT  expression   { $$ = new BinaryOp("<",  $1, $3); }
    | expression TGT  expression   { $$ = new BinaryOp(">",  $1, $3); }
    | expression TLTE expression   { $$ = new BinaryOp("<=", $1, $3); }
    | expression TGTE expression   { $$ = new BinaryOp(">=", $1, $3); }
    | expression TNEQ expression   { $$ = new BinaryOp("!=", $1, $3); }
    ;

%%
