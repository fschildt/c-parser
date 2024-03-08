#ifndef AST_H
#define AST_H

#include "token.h"

typedef struct Ast_Expression Ast_Expression;
typedef struct Ast_Statement Ast_Statement;
typedef struct Ast_Argument Ast_Argument;
typedef struct Ast_Statement Ast_Statement;
typedef struct Ast_Parameter Ast_Parameter;
typedef struct Ast_Declaration Ast_Declaration;
typedef struct Ast_Function Ast_Function;
typedef struct Ast_Function_Invocation Ast_Function_Invocation;
typedef struct Ast_Assignment Ast_Assignment;
typedef struct Ast_If Ast_If;
typedef struct Ast_While Ast_While;
typedef struct Ast_Block Ast_Block;
typedef struct Ast_Return Ast_Return;
typedef struct Ast_Type Ast_Type;

typedef enum {
    AST_NONE,
    AST_DECLARATION,
    AST_PARAMETER,
    AST_ASSIGNMENT,
    AST_EXPRESSION,
    AST_IF,
    AST_WHILE,
    AST_BLOCK,
    AST_FUNCTION,
    AST_FUNCTION_INVOCATION,
    AST_RETURN,
} Ast_Node_Type;

struct Ast_Expression {
    Token *token;
    Ast_Expression *left;
    Ast_Expression *right;
    Ast_Function_Invocation *function_invocation;
};

struct Ast_Type {
    Token *token;
    Ast_Type *next;
};

struct Ast_Parameter {
    Ast_Type *type;
    Token *ident;
    Ast_Parameter *next;
};

struct Ast_Argument {
    Ast_Expression *expr;
    Ast_Argument *next;
};

struct Ast_Declaration {
    Ast_Type *type;
    Token *ident;
    Ast_Expression *expr;
    Ast_Declaration *next;
};

struct Ast_Function {
    Ast_Type *type;
    Token *ident;
    Ast_Statement *statements_root;
    Ast_Parameter *params_root;
    Ast_Function *next;
};

struct Ast_Assignment {
    Token *ident;
    Ast_Expression *expr;
};

struct Ast_If {
    Ast_Expression *expr;
    Ast_Statement  *statement_if;
    Ast_Statement  *statement_else;
};

struct Ast_While {
    Ast_Expression *expr;
    Ast_Statement  *statement;
};

struct Ast_Block {
    Ast_Statement *statements_root;
};

struct Ast_Return {
    Ast_Expression *expr;
};

struct Ast_Function_Invocation {
    Token *ident;
    Ast_Argument *args_root;
};

struct Ast_Statement {
    Ast_Node_Type type;
    union {
        Ast_Declaration         stmt_decl;
        Ast_Assignment          stmt_assignment;
        Ast_If                  stmt_if;
        Ast_While               stmt_while;
        Ast_Block               stmt_block;
        Ast_Return              stmt_return;
        Ast_Expression          stmt_expr;
    };
    Ast_Statement *next;
};

typedef struct {
    Ast_Function *functions_root;
} Ast;

void ast_print(Ast *ast);

#endif // AST_H
