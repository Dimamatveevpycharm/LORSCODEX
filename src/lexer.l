%option noyywrap nounput noinput

%{
#include <cstdlib>
#include <cstring>

#include "parser.tab.hpp"

int yycolumn = 1;

static void set_loc() {
  yylloc.first_line = yylineno;
  yylloc.last_line = yylineno;
  yylloc.first_column = yycolumn;
  yylloc.last_column = yycolumn + static_cast<int>(yyleng) - 1;
}
%}

%x COMMENT

%%

[ \t\r]+         { yycolumn += static_cast<int>(yyleng); }
\n               { yylineno += 1; yycolumn = 1; }

"//".*           { yycolumn += static_cast<int>(yyleng); }
"/*"             { yycolumn += 2; BEGIN(COMMENT); }
<COMMENT>"*/"    { yycolumn += 2; BEGIN(INITIAL); }
<COMMENT>\n      { yylineno += 1; yycolumn = 1; }
<COMMENT>.       { yycolumn += 1; }

"func"           { set_loc(); yycolumn += 4; return KW_FUNC; }
"var"            { set_loc(); yycolumn += 3; return KW_VAR; }
"if"             { set_loc(); yycolumn += 2; return KW_IF; }
"else"           { set_loc(); yycolumn += 4; return KW_ELSE; }
"for"            { set_loc(); yycolumn += 3; return KW_FOR; }
"return"         { set_loc(); yycolumn += 6; return KW_RETURN; }
"int"            { set_loc(); yycolumn += 3; return KW_INT; }
"float"          { set_loc(); yycolumn += 5; return KW_FLOAT; }
"bool"           { set_loc(); yycolumn += 4; return KW_BOOL; }
"void"           { set_loc(); yycolumn += 4; return KW_VOID; }
"true"           { set_loc(); yycolumn += 4; return KW_TRUE; }
"false"          { set_loc(); yycolumn += 5; return KW_FALSE; }

"&&"             { set_loc(); yycolumn += 2; return AND; }
"||"             { set_loc(); yycolumn += 2; return OR; }
"=="             { set_loc(); yycolumn += 2; return EQ; }
"!="             { set_loc(); yycolumn += 2; return NEQ; }
"<="             { set_loc(); yycolumn += 2; return LTE; }
">="             { set_loc(); yycolumn += 2; return GTE; }
"="              { set_loc(); yycolumn += 1; return ASSIGN; }
"<"              { set_loc(); yycolumn += 1; return LT; }
">"              { set_loc(); yycolumn += 1; return GT; }
"+"              { set_loc(); yycolumn += 1; return PLUS; }
"-"              { set_loc(); yycolumn += 1; return MINUS; }
"*"              { set_loc(); yycolumn += 1; return STAR; }
"/"              { set_loc(); yycolumn += 1; return SLASH; }
"%"              { set_loc(); yycolumn += 1; return PERCENT; }
"!"              { set_loc(); yycolumn += 1; return NOT; }
"?"              { set_loc(); yycolumn += 1; return QUESTION; }
":"              { set_loc(); yycolumn += 1; return COLON; }
","              { set_loc(); yycolumn += 1; return COMMA; }
";"              { set_loc(); yycolumn += 1; return SEMI; }
"("              { set_loc(); yycolumn += 1; return LPAREN; }
")"              { set_loc(); yycolumn += 1; return RPAREN; }
"{"              { set_loc(); yycolumn += 1; return LBRACE; }
"}"              { set_loc(); yycolumn += 1; return RBRACE; }

[0-9]+"."[0-9]+([eE][+-]?[0-9]+)? {
  set_loc();
  yycolumn += static_cast<int>(yyleng);
  yylval.str = strdup(yytext);
  return FLOAT_LIT;
}

[0-9]+ {
  set_loc();
  yycolumn += static_cast<int>(yyleng);
  yylval.str = strdup(yytext);
  return INT_LIT;
}

[A-Za-z_][A-Za-z0-9_]* {
  set_loc();
  yycolumn += static_cast<int>(yyleng);
  yylval.str = strdup(yytext);
  return IDENT;
}

. {
  set_loc();
  yycolumn += static_cast<int>(yyleng);
  return yytext[0];
}

%%

