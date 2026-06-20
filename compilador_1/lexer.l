%{
#include <string>
#include "parser.tab.hpp"
#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t) (yylval.token = t)
%}

%option noyywrap

%%

[ \t\n]                 ; // Ignora espaços
"int"                   return TOKEN(TINT);
"void"                  return TOKEN(TVOID);
"if"                    return TOKEN(TIF);
"else"                  return TOKEN(TELSE);
"for"                   return TOKEN(TFOR);
"while"                 return TOKEN(TWHILE);
"do"                    return TOKEN(TDO);
"switch"                return TOKEN(TSWITCH);
"case"                  return TOKEN(TCASE);
"print"                 return TOKEN(TPRINT);
"input"                 return TOKEN(TINPUT);
"return"                return TOKEN(TRETURN);
"main"                  return TOKEN(TMAIN);

[a-zA-Z_][a-zA-Z0-9_]* SAVE_TOKEN; return TID;
[0-9]+                  SAVE_TOKEN; return TINTEGER;
"\""[^\"]*"\""          SAVE_TOKEN; return TSTRING;

"="                     return TOKEN(TEQUAL);
"=="                    return TOKEN(TCEQ);
"?"                     return TOKEN(TTERNARY);
":"                     return TOKEN(TCOLON);
"=>"                    return TOKEN(TARROW);
";"                     return TOKEN(TSEMI);
"{"                     return TOKEN(TLBRACE);
"}"                     return TOKEN(TRBRACE);
"("                     return TOKEN(TLPAREN);
")"                     return TOKEN(TRPAREN);
","                     return TOKEN(TCOMMA);

.                       printf("Token desconhecido: %s\n", yytext); yyterminate();

%%
