%{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string> // Adicionado para evitar problemas com std::string
    extern int yylex();
    void yyerror(const char *s) { printf("Erro sintático: %s\n", s); }
%}

%union {
    std::string *string;
    int token;
}

/* Definição de Tokens */
%token <string> TID TINTEGER TSTRING
%token <token> TINT TVOID TIF TELSE TFOR TWHILE TDO TSWITCH TCASE
%token <token> TPRINT TINPUT TRETURN TMAIN TCEQ TEQUAL TARROW
%token <token> TSEMI TLBRACE TRBRACE TLPAREN TRPAREN TCOMMA TTERNARY TCOLON

%start program

%%

program:
    func_decl
    | program func_decl
    ;

func_decl:
    type TID TLPAREN args TRPAREN compound_stmt
    | TINT TMAIN TLPAREN TRPAREN compound_stmt
    ;

type: TINT | TVOID ;

args: /* vazio */ | arg_list ;
arg_list: TID | arg_list TCOMMA TID ;

compound_stmt:
    TLBRACE statements TRBRACE
    ;

statements:
    statement
    | statements statement
    | /* vazio */
    ;

statement:
    declaration TSEMI
    | assignment TSEMI
    | if_stmt
    | while_stmt
    | for_stmt          /* Agora o Bison vai encontrar a regra aqui embaixo! */
    | print_stmt TSEMI
    | return_stmt TSEMI
    ;

declaration: type TID ;

assignment: TID TEQUAL expression ;

if_stmt:
    TIF TLPAREN expression TRPAREN compound_stmt
    | TIF TLPAREN expression TRPAREN compound_stmt TELSE compound_stmt
    ;

while_stmt: TWHILE TLPAREN expression TRPAREN compound_stmt ;

/* --- NOVA REGRA ADICIONADA PARA RESOLVER O ERRO --- */
for_stmt: 
    TFOR TLPAREN assignment TSEMI expression TSEMI assignment TRPAREN compound_stmt 
    ;

print_stmt: TPRINT TLPAREN expression TRPAREN ;

return_stmt: TRETURN expression ;

expression:
    TID
    | TINTEGER
    | TSTRING
    | assignment /* Permite atribuições dentro do for, ex: i = 0 */
    | expression TCEQ expression
    | expression TTERNARY expression TCOLON expression /* Operador Ternário */
    ;

%%
