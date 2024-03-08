#include "ast.h"
#include "os.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void print_ast_expression(Ast_Expression *expr, i32 indentation);
static void print_ast_statement(Ast_Statement *statement, i32 indentation);

static char *str_ref_to_horrific_string(StringRef str_ref)
{
    char *string = (char*)os_allocate_memory(str_ref.length + 1);
    if (!string)
    {
        printf("error: out of memory\n");
        exit(EXIT_FAILURE);
    }

    memcpy(string, str_ref.location, str_ref.length);
    string[str_ref.length] = '\0';
    return string;
}

static void print_indentation(i32 indentation)
{
    for (i32 i=0; i<indentation; i++)
        putchar(' ');
}

static void print_type(Ast_Type *type)
{
    if (!type)
    {
        printf("type = void\n");
        return;
    }

    printf("type = ");
    while (type)
    {
        switch (type->token->type)
        {
            case TOKEN_KEYWORD_VOID:   printf("void");   break;
            case TOKEN_KEYWORD_CHAR:   printf("char");   break;
            case TOKEN_KEYWORD_INT:    printf("int");    break;
            case TOKEN_KEYWORD_DOUBLE: printf("double"); break;
            case '*':                  printf("*");      break;
            default: printf("type = %d (error)\n", type->token->type);
        }
        type = type->next;
    }
    printf("\n");
}

void print_ast_expression(Ast_Expression *expr, i32 indentation)
{
    if (!expr) return;

    print_indentation(indentation);
    printf("expr\n");
    print_indentation(indentation);
    if (expr->token->type == '+')       printf("+\n");
    else if (expr->token->type == '-')  printf("-\n");
    else if (expr->token->type == '*')  printf("*\n");
    else if (expr->token->type == '/')  printf("/\n");
    else if (expr->token->type == '%')  printf("%%\n");
    else if (expr->token->type == '!')  printf("!\n");
    else if (expr->token->type == TOKEN_ANDAND) printf("&&\n");
    else if (expr->token->type == TOKEN_OROR)   printf("||\n");
    else if (expr->token->type == TOKEN_EQEQ) printf("==\n");
    else if (expr->token->type == TOKEN_GE) printf(">=\n");
    else if (expr->token->type == TOKEN_LE) printf("<=\n");
    else if (expr->token->type == TOKEN_NE) printf("!=\n");
    else if (expr->token->type == '>') printf(">\n");
    else if (expr->token->type == '<') printf("<\n");
    else if (expr->token->type == TOKEN_LITERAL_INT || expr->token->type == TOKEN_LITERAL_DOUBLE)
    {
        const char *lit = str_ref_to_horrific_string(expr->token->str_ref);
        printf("%s\n", lit);
    }
    else if (expr->token->type == TOKEN_LITERAL_STRING)
    {
        const char *lit = str_ref_to_horrific_string(expr->token->str_ref);
        printf("%s\n", lit);
    }
    else if (expr->token->type == TOKEN_IDENTIFIER)
    {
        const char *ident = str_ref_to_horrific_string(expr->token->str_ref);
        printf("%s\n", ident);
        if (expr->function_invocation)
        {
            Ast_Argument *arg = expr->function_invocation->args_root;
            while (arg)
            {
                print_indentation(indentation + 2);
                printf("arg\n");
                print_ast_expression(arg->expr, indentation + 4);
                arg = arg->next;
            }
        }
    }
    else if (expr->token->type == '(')
    {
        printf("(\n");
    }
    else
    {
        printf("error\n");
    }

    indentation += 2;
    print_ast_expression(expr->left, indentation);
    print_ast_expression(expr->right, indentation);
}

void print_ast_function_invocation(Ast_Function_Invocation *function_invocation, i32 indentation)
{
    print_indentation(indentation);
    printf("function_invocation\n");
    indentation += 2;

    const char *ident = str_ref_to_horrific_string(function_invocation->ident->str_ref);
    print_indentation(indentation);
    printf("ident = %s\n", ident);

    Ast_Argument *arg = function_invocation->args_root;
    while (arg)
    {
        print_indentation(indentation);
        printf("arg\n");
        print_ast_expression(arg->expr, indentation + 2);
        arg = arg->next;
    }
}

void print_ast_return(Ast_Return *ast_return, i32 indentation)
{
    print_indentation(indentation);
    printf("return\n");
    indentation += 2;

    // return expr
    print_ast_expression(ast_return->expr, indentation);
}

void print_ast_block(Ast_Block *ast_block, i32 indentation)
{
    print_indentation(indentation);
    printf("block\n");
    indentation += 2;

    Ast_Statement *statement = ast_block->statements_root;
    while (statement)
    {
        print_ast_statement(statement, indentation);
        statement = statement->next;
    }
}

void print_ast_while(Ast_While *ast_while, i32 indentation)
{
    print_indentation(indentation);
    printf("while\n");
    indentation += 2;

    print_ast_expression(ast_while->expr, indentation);
    print_ast_statement(ast_while->statement, indentation);
}

void print_ast_if(Ast_If *ast_if, i32 indentation)
{
    print_indentation(indentation);
    printf("if\n");
    indentation += 2;
    
    print_ast_expression(ast_if->expr, indentation);
    print_ast_statement(ast_if->statement_if, indentation);
    print_ast_statement(ast_if->statement_else, indentation);
}

void print_ast_assignment(Ast_Assignment *assign, i32 indentation)
{
    print_indentation(indentation);
    printf("assignment\n");
    indentation += 2;

    const char *ident = str_ref_to_horrific_string(assign->ident->str_ref);
    print_indentation(indentation);
    printf("ident = %s\n", ident);

    print_ast_expression(assign->expr, indentation);
}

void print_ast_statement(Ast_Statement *statement, i32 indentation)
{
    if (!statement) return;

    Ast_Node_Type type = statement->type;
    if (type == AST_ASSIGNMENT)   
        print_ast_assignment(&statement->stmt_assignment, indentation);
    else if (type == AST_IF)     
        print_ast_if(&statement->stmt_if, indentation);
    else if (type == AST_WHILE)     
        print_ast_while(&statement->stmt_while, indentation);
    else if (type == AST_BLOCK) 
        print_ast_block(&statement->stmt_block, indentation);
    else if (type == AST_EXPRESSION && statement->stmt_expr.function_invocation)
        print_ast_function_invocation(statement->stmt_expr.function_invocation, indentation);
    else if (type == AST_RETURN)           
        print_ast_return(&statement->stmt_return, indentation);
    else
    {
        print_indentation(indentation);
        printf("statement: ERROR\n");
    }
}

void print_ast_declaration(Ast_Declaration *decl, i32 indentation)
{
    print_indentation(indentation);
    printf("decl\n");
    indentation += 2;

    // type
    print_indentation(indentation);
    print_type(decl->type);

    // ident
    const char *ident = str_ref_to_horrific_string(decl->ident->str_ref);
    print_indentation(indentation);
    printf("ident = %s\n", ident);

    // expr
    print_ast_expression(decl->expr,indentation);
}

void print_ast_parameter(Ast_Parameter *param, i32 indentation)
{
    print_indentation(indentation);
    printf("param\n");
    indentation += 2;
    print_indentation(indentation);

    // type
    Ast_Type *type = param->type;
    print_type(type);

    // ident
    if (param->ident)
    {
        const char *ident = str_ref_to_horrific_string(param->ident->str_ref);
        print_indentation(indentation);
        printf("ident = %s\n", ident);
    }
}

void print_function(Ast_Function *function, i32 indentation)
{
    print_indentation(indentation);
    printf("function\n");
    indentation += 2;

    // type
    Ast_Type *type = function->type;
    print_indentation(indentation);
    print_type(type);

    // ident
    const char *ident = str_ref_to_horrific_string(function->ident->str_ref);
    print_indentation(indentation);
    printf("ident = %s\n", ident);

    // params
    Ast_Parameter *param = function->params_root;
    while (param)
    {
        print_ast_parameter(param, indentation);
        param = param->next;
    }

    Ast_Statement *statement = function->statements_root;
    while (statement && statement->type == AST_DECLARATION)
    {
        print_ast_declaration(&statement->stmt_decl, indentation);
        statement = statement->next;
    }
    while (statement)
    {
        print_ast_statement(statement, indentation);
        statement = statement->next;
    }
}

void ast_print(Ast *ast)
{
    Ast_Function *function = ast->functions_root;
    while (function)
    {
        print_function(function, 0);
        function = function->next;
    }
}

