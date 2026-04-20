%{
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "lorsc/AST.h"
#include "lorsc/Diagnostics.h"

extern int yylex(void);
extern FILE* yyin;
extern int yylineno;
extern int yycolumn;

namespace lorsc {
extern DiagnosticsEngine* gDiagnostics;
extern std::string gInputFile;
extern Program* gParsedProgram;
}  // namespace lorsc

#define MAKE_LOC(Loc) lorsc::SourceLocation{(Loc).first_line, (Loc).first_column}

int yyerror(const char* msg) {
  if (lorsc::gDiagnostics != nullptr) {
    lorsc::gDiagnostics->reportParseError(lorsc::gInputFile, yylineno, yycolumn, msg);
  }
  return 0;
}
%}

%locations
%define parse.error verbose

%code requires {
#include <vector>
#include "lorsc/AST.h"
}

%union {
  char* str;
  lorsc::TypeKind type;
  lorsc::Expr* expr;
  lorsc::Stmt* stmt;
  lorsc::BlockStmt* block;
  lorsc::AssignStmt* assign;
  lorsc::FunctionDecl* function;
  std::vector<lorsc::Expr*>* expr_list;
  std::vector<lorsc::Stmt*>* stmt_list;
  std::vector<lorsc::FunctionDecl*>* fn_list;
  std::vector<lorsc::ParamDecl>* param_list;
  lorsc::Program* program;
}

%token KW_FUNC KW_VAR KW_IF KW_ELSE KW_FOR KW_RETURN
%token KW_INT KW_FLOAT KW_BOOL KW_VOID
%token KW_TRUE KW_FALSE

%token <str> IDENT
%token <str> INT_LIT FLOAT_LIT

%token PLUS MINUS STAR SLASH PERCENT
%token ASSIGN EQ NEQ LT LTE GT GTE
%token AND OR NOT
%token QUESTION COLON
%token COMMA SEMI
%token LPAREN RPAREN LBRACE RBRACE

%type <program> program
%type <fn_list> function_list
%type <function> function_decl
%type <param_list> param_list param_list_opt
%type <type> type_spec
%type <block> block
%type <stmt_list> stmt_list
%type <stmt> statement var_decl assign_stmt return_stmt for_init
%type <assign> for_post
%type <expr> expression
%type <expr_list> argument_list argument_list_opt

%destructor { free($$); } <str>
%destructor { delete $$; } <expr>
%destructor { delete $$; } <stmt>
%destructor { delete $$; } <block>
%destructor { delete $$; } <assign>
%destructor { delete $$; } <function>
%destructor { delete $$; } <expr_list>
%destructor { delete $$; } <stmt_list>
%destructor { delete $$; } <fn_list>
%destructor { delete $$; } <param_list>

%right QUESTION COLON
%left OR
%left AND
%left EQ NEQ
%left LT LTE GT GTE
%left PLUS MINUS
%left STAR SLASH PERCENT
%right NOT UMINUS

%%

program
  : function_list {
      $$ = new lorsc::Program({1, 1}, *$1);
      delete $1;
      lorsc::gParsedProgram = $$;
    }
  ;

function_list
  : function_decl {
      $$ = new std::vector<lorsc::FunctionDecl*>();
      $$->push_back($1);
    }
  | function_list function_decl {
      $1->push_back($2);
      $$ = $1;
    }
  ;

function_decl
  : KW_FUNC IDENT LPAREN param_list_opt RPAREN type_spec block {
      $$ = new lorsc::FunctionDecl(MAKE_LOC(@1), std::string($2), $4, $6, $7);
      free($2);
    }
  ;

param_list_opt
  : /* empty */ {
      $$ = new std::vector<lorsc::ParamDecl>();
    }
  | param_list {
      $$ = $1;
    }
  ;

param_list
  : IDENT type_spec {
      $$ = new std::vector<lorsc::ParamDecl>();
      $$->push_back(lorsc::ParamDecl{std::string($1), $2, MAKE_LOC(@1)});
      free($1);
    }
  | param_list COMMA IDENT type_spec {
      $1->push_back(lorsc::ParamDecl{std::string($3), $4, MAKE_LOC(@3)});
      free($3);
      $$ = $1;
    }
  ;

type_spec
  : KW_INT { $$ = lorsc::TypeKind::Int; }
  | KW_FLOAT { $$ = lorsc::TypeKind::Float; }
  | KW_BOOL { $$ = lorsc::TypeKind::Bool; }
  | KW_VOID { $$ = lorsc::TypeKind::Void; }
  ;

block
  : LBRACE stmt_list RBRACE {
      $$ = new lorsc::BlockStmt(MAKE_LOC(@1), *$2);
      delete $2;
    }
  ;

stmt_list
  : /* empty */ {
      $$ = new std::vector<lorsc::Stmt*>();
    }
  | stmt_list statement {
      $1->push_back($2);
      $$ = $1;
    }
  ;

statement
  : var_decl SEMI { $$ = $1; }
  | assign_stmt SEMI { $$ = $1; }
  | expression SEMI { $$ = new lorsc::ExprStmt(MAKE_LOC(@1), $1); }
  | return_stmt SEMI { $$ = $1; }
  | block { $$ = $1; }
  | KW_IF expression block {
      $$ = new lorsc::IfStmt(MAKE_LOC(@1), $2, $3, nullptr);
    }
  | KW_IF expression block KW_ELSE block {
      $$ = new lorsc::IfStmt(MAKE_LOC(@1), $2, $3, $5);
    }
  | KW_FOR expression block {
      $$ = new lorsc::ForStmt(MAKE_LOC(@1), nullptr, $2, nullptr, $3);
    }
  | KW_FOR for_init SEMI expression SEMI for_post block {
      $$ = new lorsc::ForStmt(MAKE_LOC(@1), $2, $4, $6, $7);
    }
  ;

for_init
  : /* empty */ { $$ = nullptr; }
  | var_decl { $$ = $1; }
  | assign_stmt { $$ = $1; }
  ;

for_post
  : /* empty */ { $$ = nullptr; }
  | assign_stmt { $$ = dynamic_cast<lorsc::AssignStmt*>($1); }
  ;

var_decl
  : KW_VAR IDENT type_spec {
      $$ = new lorsc::VarDeclStmt(MAKE_LOC(@1), std::string($2), $3, nullptr);
      free($2);
    }
  | KW_VAR IDENT type_spec ASSIGN expression {
      $$ = new lorsc::VarDeclStmt(MAKE_LOC(@1), std::string($2), $3, $5);
      free($2);
    }
  ;

assign_stmt
  : IDENT ASSIGN expression {
      $$ = new lorsc::AssignStmt(MAKE_LOC(@1), std::string($1), $3);
      free($1);
    }
  ;

return_stmt
  : KW_RETURN {
      $$ = new lorsc::ReturnStmt(MAKE_LOC(@1), nullptr);
    }
  | KW_RETURN expression {
      $$ = new lorsc::ReturnStmt(MAKE_LOC(@1), $2);
    }
  ;

argument_list_opt
  : /* empty */ {
      $$ = new std::vector<lorsc::Expr*>();
    }
  | argument_list {
      $$ = $1;
    }
  ;

argument_list
  : expression {
      $$ = new std::vector<lorsc::Expr*>();
      $$->push_back($1);
    }
  | argument_list COMMA expression {
      $1->push_back($3);
      $$ = $1;
    }
  ;

expression
  : IDENT {
      $$ = new lorsc::VariableExpr(MAKE_LOC(@1), std::string($1));
      free($1);
    }
  | INT_LIT {
      $$ = new lorsc::IntLiteralExpr(MAKE_LOC(@1), std::stoll($1));
      free($1);
    }
  | FLOAT_LIT {
      $$ = new lorsc::FloatLiteralExpr(MAKE_LOC(@1), std::stod($1));
      free($1);
    }
  | KW_TRUE {
      $$ = new lorsc::BoolLiteralExpr(MAKE_LOC(@1), true);
    }
  | KW_FALSE {
      $$ = new lorsc::BoolLiteralExpr(MAKE_LOC(@1), false);
    }
  | IDENT LPAREN argument_list_opt RPAREN {
      $$ = new lorsc::CallExpr(MAKE_LOC(@1), std::string($1), *$3);
      free($1);
      delete $3;
    }
  | LPAREN expression RPAREN {
      $$ = $2;
    }
  | MINUS expression %prec UMINUS {
      $$ = new lorsc::UnaryExpr(MAKE_LOC(@1), lorsc::UnaryOpKind::Negate, $2);
    }
  | NOT expression {
      $$ = new lorsc::UnaryExpr(MAKE_LOC(@1), lorsc::UnaryOpKind::Not, $2);
    }
  | expression PLUS expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::Add, $1, $3);
    }
  | expression MINUS expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::Sub, $1, $3);
    }
  | expression STAR expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::Mul, $1, $3);
    }
  | expression SLASH expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::Div, $1, $3);
    }
  | expression PERCENT expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::Mod, $1, $3);
    }
  | expression LT expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::Less, $1, $3);
    }
  | expression LTE expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::LessEq, $1, $3);
    }
  | expression GT expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::Greater, $1, $3);
    }
  | expression GTE expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::GreaterEq, $1, $3);
    }
  | expression EQ expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::Eq, $1, $3);
    }
  | expression NEQ expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::NotEq, $1, $3);
    }
  | expression AND expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::LogicalAnd, $1, $3);
    }
  | expression OR expression {
      $$ = new lorsc::BinaryExpr(MAKE_LOC(@2), lorsc::BinaryOpKind::LogicalOr, $1, $3);
    }
  | expression QUESTION expression COLON expression {
      $$ = new lorsc::TernaryExpr(MAKE_LOC(@2), $1, $3, $5);
    }
  ;

%%
