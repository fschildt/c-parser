#include "general.h"
#include "ast.h"

#include <stdio.h>

typedef struct {
    Ast_Type *type;
    Ast_Function *function;
} Ident_Info;

static b32 check_expr(Ast_Expression *expr, Ast_Type *type, Ast_Function *function, Ast_Function *functions_root);

static void report_error(Token *t, const char *message)
{
    printf("typechecker error (%d,%d): %s (found token type = %d)\n", t->line, t->c0, message, t->type);
}

static b32 lookup_ident_info(Ident_Info *info, Token *ident, Ast_Function *function, Ast_Function *functions_root)
{
    if (function)
    {
        // search for token in function declarations
        Ast_Statement *statement_it = function->statements_root;
        while (statement_it && statement_it->type == AST_DECLARATION)
        {
            Ast_Declaration *decl = &statement_it->stmt_decl;
            if (strings_equal_ref(ident->str_ref, decl->ident->str_ref))
            {
                info->type = decl->type;
                info->function = 0;
                return true;
            }
            statement_it = statement_it->next;
        }

        // search for token in function parameters
        if (function)
        {
            Ast_Parameter *param = function->params_root;
            while (param)
            {
                Token *t = param->ident;
                if (t && strings_equal_ref(t->str_ref, ident->str_ref))
                {
                    info->type = param->type;
                    info->function = 0;
                    return true;
                }
                param = param->next;
            }
        }
    }

    // search for token in function identifiers
    function = functions_root;
    while (function)
    {
        if (strings_equal_ref(ident->str_ref, function->ident->str_ref))
        {
            info->type = function->type;
            info->function = function;
            return true;
        }
        function = function->next;
    }

    report_error(ident, "identifier is not defined");
    return false;
}

static b32 type_is_void(Ast_Type *type)
{
    if (type->token->type == TOKEN_KEYWORD_VOID && type->next == 0)
    {
        return true;
    }
    return false;
}

static b32 type_is_int(Ast_Type *type)
{
    if (type->token->type == TOKEN_KEYWORD_INT && !type->next)
    {
        return true;
    }
    return false;
}

static b32 type_is_double(Ast_Type *type)
{
    if (type->token->type == TOKEN_KEYWORD_DOUBLE && !type->next)
    {
        return true;
    }
    return false;
}

static b32 type_is_string(Ast_Type *type)
{
    if (type->token->type == TOKEN_KEYWORD_CHAR &&
        type->next && type->next->token->type == '*' &&
        !type->next->next)
    {
        return true;
    }
    return false;
}

static b32 types_equal(Ast_Type *t1, Ast_Type *t2)
{
    while (t1 && t2)
    {
        if (t1->token->type != t2->token->type)
        {
            return false;
        }
        t1 = t1->next;
        t2 = t2->next;
    }
    return !t1 && !t2;
}

static b32 check_function_invocation(Ast_Function_Invocation *invocation, Ident_Info *info, Ast_Function *function, Ast_Function *functions_root)
{
    Ast_Argument *arg = invocation->args_root; 
    Ast_Parameter *param = info->function->params_root;
    if (param && arg && !arg->expr)
    {
        if (type_is_void(param->type))
        {
            return true;
        }
        report_error(param->ident, "argument is of type 'void' but function paramter is not of type 'void'");
        return false;
    }

    while (arg && param)
    {
        if (!check_expr(arg->expr, param->type, function, functions_root))
        {
            return false;
        }
        arg = arg->next;
        param = param->next;
    }
    if (arg && !param)
    {
        report_error(invocation->ident, "more arguments than parameters");
        return false;
    }
    if (param && !arg)
    {
        report_error(invocation->ident, "more parameters than arguments");
        return false;
    }

    return true;
}

static b32 check_double_literal_within_limits(Token *token)
{
    return true;
}

static b32 check_int_literal_within_limits(Token *token, b32 unary_is_negative)
{
    assert(token->type == TOKEN_LITERAL_INT);

    const char *max_positive = "2147483647";
    const char *max_negative = "2147483648";

    i32 cmp_len = 10;
    const char *cmp = unary_is_negative ? max_negative : max_positive;

    StringRef str_ref = token->str_ref;
    while (*str_ref.location == '0')
    {
        str_ref.location++;
        str_ref.length--;
    }

    if (str_ref.length > cmp_len)
    {
        report_error(token, "int literal too large");
        return false;
    }
    else if (str_ref.length < cmp_len)
    {
        return true;
    }

    assert(str_ref.length == cmp_len);

    i32 index = 0;
    while (index < str_ref.length)
    {
        if (str_ref.location[index] > cmp[index])
        {
            report_error(token, "int literal too large");
            return false;
        }
        if (str_ref.location[index] < cmp[index])
        {
            return true;
        }
        index++;
    }

    return true;
}

static b32 check_expr_int(Ast_Expression *expr, b32 unary_is_negative, Ast_Function *function, Ast_Function *functions_root)
{
    assert(expr);

    i32 token_type = expr->token->type;

    // unary
    if (token_type == '+' || token_type == '-')
    {
        b32 is_unary = true;
        b32 unary_is_negative = token_type == '-';

        Ast_Expression *sub_expr = expr->left;
        while (sub_expr)
        {
            i32 token_type = sub_expr->token->type;
            if (token_type == '+')
            {
            }
            else if (token_type == '-')
            {
                unary_is_negative = !unary_is_negative;
            }
            else if (token_type == '!')
            {
                report_error(sub_expr->token, "invalid unary operator '!' in +,- unary operators");
                return false;
            }
            else
            {
                is_unary = false;
                break;
            }
            sub_expr = sub_expr->left;
        }
        if (is_unary)
        {
            b32 check = check_expr_int(expr->right, unary_is_negative, function, functions_root);
            return check;
        }
    }

    if (token_type == '!')
    {
        report_error(expr->token, "invalid unary operator '!' in int expression");
        return false;
    }

    // operators
    if (token_type == '+' ||
        token_type == '-' ||
        token_type == '*' ||
        token_type == '/' ||
        token_type == '%')
    {
        b32 left  = check_expr_int(expr->left,  false, function, functions_root);
        b32 right = check_expr_int(expr->right, false, function, functions_root);
        return left && right;
    }
    // identifier
    else if (token_type == TOKEN_IDENTIFIER)
    {
        Ident_Info ident_info;
        if (!lookup_ident_info(&ident_info, expr->token, function, functions_root))
        {
            return false;
        }
        if (!type_is_int(ident_info.type))
        {
            report_error(expr->token, "type is not int");
            return false;
        }
        if (expr->function_invocation)
        {
            if (!check_function_invocation(expr->function_invocation, &ident_info, function, functions_root))
            {
                return false;
            }
        }
        return true;
    }
    // literal
    else if (token_type == TOKEN_LITERAL_INT)
    {
        b32 limit_check = check_int_literal_within_limits(expr->token, unary_is_negative);
        return limit_check;
    }
    else if (token_type == '(')
    {
        assert(!expr->right);
        b32 check = check_expr_int(expr->left, false, function, functions_root);
        return check;
    }
    else if (token_type == TOKEN_LITERAL_DOUBLE)
    {
        report_error(expr->token, "cannot convert double to int");
        return false;
    }

    report_error(expr->token, "not an int-expression");
    return false;
}

static b32 check_expr_double(Ast_Expression *expr, b32 unary_is_negative, Ast_Function *function, Ast_Function *functions_root)
{
    assert(expr);

    i32 token_type = expr->token->type;

    // unary
    if (token_type == '+' || token_type == '-')
    {
        b32 is_unary = true;
        b32 unary_is_negative = token_type == '-';

        Ast_Expression *sub_expr = expr->left;
        while (sub_expr)
        {
            i32 token_type = sub_expr->token->type;
            if (token_type == '+')
            {
            }
            else if (token_type == '-')
            {
                unary_is_negative = !unary_is_negative;
            }
            else if (token_type == '!')
            {
                report_error(sub_expr->token, "invalid unary operator '!' in +,- unary operators");
                return false;
            }
            else
            {
                is_unary = false;
                break;
            }
            sub_expr = sub_expr->left;
        }
        if (is_unary)
        {
            b32 check = check_expr_double(expr->right, unary_is_negative, function, functions_root);
            return check;
        }

    }
    if (token_type == '!')
    {
        report_error(expr->token, "invalid unary operator '!' in double expression");
        return false;
    }

    // operators
    if (token_type == '+' ||
        token_type == '-' ||
        token_type == '*' ||
        token_type == '/' ||
        token_type == '%')
    {
        b32 left  = check_expr_double(expr->left,  false, function, functions_root);
        b32 right = check_expr_double(expr->right, false, function, functions_root);
        return left && right;
    }
    // identifier
    else if (token_type == TOKEN_IDENTIFIER)
    {
        Ident_Info ident_info;
        if (!lookup_ident_info(&ident_info, expr->token, function, functions_root))
        {
            return false;
        }
        if (!type_is_double(ident_info.type))
        {
            report_error(expr->token, "is not type double");
            return false;
        }
        if (expr->function_invocation)
        {
            if (!check_function_invocation(expr->function_invocation, &ident_info, function, functions_root))
            {
                return false;
            }
        }
        return true;
    }
    // literal
    else if (token_type == TOKEN_LITERAL_INT)
    {
        b32 limit_check = check_int_literal_within_limits(expr->token, unary_is_negative);
        return limit_check;
    }
    else if (token_type == TOKEN_LITERAL_DOUBLE)
    {
        b32 limit_check = check_double_literal_within_limits(expr->token);
        return limit_check;
    }
    else if (token_type == '(')
    {
        assert(!expr->right);
        b32 check = check_expr_double(expr->left, false, function, functions_root);
        return check;
    }

    report_error(expr->token, "not a double-expression");
    return false;
}

static b32 check_expr_bool(Ast_Expression *expr, Ast_Function *function, Ast_Function *functions_root)
{
    assert(expr);

    i32 type = expr->token->type;
    // unary
    if (type == '!')
    {
        struct Ast_Expression *sub_expr = expr->left;
        while (sub_expr)
        {
            if (sub_expr->token->type != '!')
            {
                report_error(sub_expr->token, "unary operator is not '!' in +,- unary operators");
                return false;
            }
            sub_expr = sub_expr->left;
        }
        b32 check = check_expr_bool(expr->right, function, functions_root);
        return check;
    }
    // operators for bool (TOKEN_EQEQ does not work with bools!)
    else if (type == TOKEN_ANDAND ||
             type == TOKEN_OROR)
    {
        b32 left  = check_expr_bool(expr->left,  function, functions_root);
        b32 right = check_expr_bool(expr->right, function, functions_root);
        return left && right;
    }
    // operators for numbers only
    else if (type == TOKEN_EQEQ ||
             type == TOKEN_NE ||
             type == TOKEN_LE ||
             type == TOKEN_GE ||
             type == '>' ||
             type == '<')
    {
        b32 left  = check_expr_double(expr->left,  false, function, functions_root);
        b32 right = check_expr_double(expr->right, false, function, functions_root);
        return left && right;
    }
    // identifier
    else if (type == TOKEN_IDENTIFIER)
    {
        Ident_Info ident_info;
        if (!lookup_ident_info(&ident_info, expr->token, function, functions_root))
        {
            return false;
        }
        if (expr->function_invocation)
        {
            if (!check_function_invocation(expr->function_invocation, &ident_info, function, functions_root))
            {
                return false;
            }
        }
        return true;
    }
    else if (type == '(')
    {
        b32 check = check_expr_bool(expr->left, function, functions_root);
        return check;
    }
    report_error(expr->token, "not a bool-expression");
    return false;
}

static b32 check_expr_string(Ast_Expression *expr, Ast_Function *function, Ast_Function *functions_root)
{
    if (expr->token->type == TOKEN_IDENTIFIER)
    {
        Ident_Info info;
        if (!lookup_ident_info(&info, expr->token, function, functions_root))
        {
            return false;
        }
        if (!type_is_string(info.type))
        {
            report_error(expr->token, "identifier is not of type string");
            return false;
        }
        return true;
    }
    else if (expr->token->type == TOKEN_LITERAL_STRING)
    {
        return true;
    }
    report_error(expr->token, "is not type string");
    return false;
}

static b32 check_expr(Ast_Expression *expr, Ast_Type *type, Ast_Function *function, Ast_Function *functions_root)
{
    assert(expr);

    while (expr->token->type == '(')
    {
        expr = expr->left;
    }

    if (type_is_int(type))
    {
        b32 check = check_expr_int(expr, false, function, functions_root);
        return check;
    }
    else if (type_is_double(type))
    {
        b32 check = check_expr_double(expr, false, function, functions_root);
        return check;
    }
    else if (type_is_string(type))
    {
        b32 check = check_expr_string(expr, function, functions_root);
        return check;
    }
    else
    {
        assert(0);
    }

    report_error(expr->token, "expression is no type at all");
    return false;
}

static b32 check_ident_is_not_used_in_expr(Token *ident, Ast_Expression *expr)
{
    if (!expr)
    {
        return true;
    }

    if (expr->function_invocation)
    {
        Ast_Argument *arg = expr->function_invocation->args_root;
        while (arg)
        {
            if (!check_ident_is_not_used_in_expr(ident, arg->expr))
            {
                return false;
            }
            arg = arg->next;
        }
    }
    else if (expr->token->type == TOKEN_IDENTIFIER && strings_equal_ref(expr->token->str_ref, ident->str_ref))
    {
        report_error(expr->token, "identifier is not initialized");
        return false;
    }

    b32 check_left  = check_ident_is_not_used_in_expr(ident, expr->left);
    b32 check_right = check_ident_is_not_used_in_expr(ident, expr->right);
    return check_left && check_right;
}

static b32 check_ident_is_initialized_when_used_in_statement(Ast_Statement *statement, Token *ident, b32 *has_been_initted)
{
    if (statement->type == AST_ASSIGNMENT)
    {
        Ast_Assignment *ast_assignment = &statement->stmt_assignment;
        if (!check_ident_is_not_used_in_expr(ident, ast_assignment->expr))
        {
            return false;
        }
        if (strings_equal_ref(ident->str_ref, ast_assignment->ident->str_ref))
        {
            *has_been_initted = true;
            return true;
        }
    }
    else if (statement->type == AST_IF)
    {
        Ast_If *ast_if = &statement->stmt_if;

        if (!check_ident_is_not_used_in_expr(ident, ast_if->expr))
        {
            return false;
        }

        if (!check_ident_is_initialized_when_used_in_statement(ast_if->statement_if, ident, has_been_initted))
        {
            return false;
        }

        if (ast_if->statement_else &&
            !check_ident_is_initialized_when_used_in_statement(ast_if->statement_if, ident, has_been_initted))
        {
            return false;
        }
    }
    else if (statement->type == AST_WHILE)
    {
        Ast_While *ast_while = &statement->stmt_while;

        if (!check_ident_is_not_used_in_expr(ident, ast_while->expr))
        {
            return false;
        }

        if (!check_ident_is_initialized_when_used_in_statement(ast_while->statement, ident, has_been_initted))
        {
            return false;
        }
    }
    else if (statement->type == AST_BLOCK)
    {
        Ast_Block *ast_block = &statement->stmt_block;

        Ast_Statement *statement = ast_block->statements_root;
        while (statement)
        {
            if (!check_ident_is_initialized_when_used_in_statement(statement, ident, has_been_initted))
            {
                return false;
            }

            if (*has_been_initted)
            {
                return true;
            }

            statement = statement->next;
        }
    }
    else if (statement->type == AST_RETURN)
    {
        Ast_Return *ast_return = &statement->stmt_return;

        if (ast_return->expr &&
            !check_ident_is_not_used_in_expr(ident, ast_return->expr))
        {
            return false;
        }
    }
    else if (statement->type == AST_EXPRESSION)
    {
        Ast_Function_Invocation *function_invoc = statement->stmt_expr.function_invocation;
        if (function_invoc)
        {
            Ast_Argument *arg = function_invoc->args_root;
            while (arg)
            {
                if (arg->expr && !check_ident_is_not_used_in_expr(ident, arg->expr))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

static b32 check_ident_is_initialized_when_used_in_statements(Ast_Declaration *decl, Ast_Statement *statements_root)
{
    Ast_Declaration *decl_it = decl;
    while (decl_it)
    {
        if (decl_it->expr)
        {
            if (!check_ident_is_not_used_in_expr(decl->ident, decl_it->expr))
            {
                return false;
            }
        }
        decl_it = decl_it->next;
    }

    Ast_Statement *statement = statements_root;
    b32 ident_now_initted = false;
    while (statement && !ident_now_initted)
    {
        if (!check_ident_is_initialized_when_used_in_statement(statement, decl->ident, &ident_now_initted))
        {
            return false;
        }
        statement = statement->next;
    }

    return true;
}

static b32 check_statement(Ast_Statement *statement, Ast_Function *function, Ast_Function *functions_root)
{
    switch (statement->type)
    {
        case AST_DECLARATION:
        {
            Ast_Declaration *decl = &statement->stmt_decl;
            if (decl->expr)
            {
                if (!check_expr(decl->expr, decl->type, function, functions_root))
                {
                    return false;
                }
                if (!check_ident_is_not_used_in_expr(decl->ident, decl->expr))
                {
                    return false;
                }
            }
            else
            {
                if (!check_ident_is_initialized_when_used_in_statements(decl, statement))
                {
                    return false;
                }
            }
        }
        break;

        case AST_ASSIGNMENT:
        {
             Ast_Assignment *assignment = &statement->stmt_assignment;
             Ident_Info ident_info;
             if (!lookup_ident_info(&ident_info, assignment->ident, function, functions_root))
             {
                return false;
             }
             b32 check = check_expr(assignment->expr, ident_info.type, function, functions_root);
             return check;
        }
        break;

        case AST_IF:
        {
            Ast_If *ast_if = &statement->stmt_if;
            b32 check_if_expr = check_expr_bool(ast_if->expr, function, functions_root);
            b32 check_if_statement = check_statement(ast_if->statement_if, function, functions_root);
            if (ast_if->statement_else)
            {
                b32 check_else_statement = check_statement(ast_if->statement_else, function, functions_root);
                b32 check = check_if_expr && check_if_statement && check_else_statement;
                return check;
            }
            b32 check = check_if_expr && check_if_statement;
            return check;
        }
        break;

        case AST_WHILE:
        {
            Ast_While *ast_while = &statement->stmt_while;
            b32 check_expr = check_expr_bool(ast_while->expr, function, functions_root);
            b32 check_stmt = check_statement(ast_while->statement, function, functions_root);
            b32 check = check_expr && check_stmt;
            return check;
        }
        break;

        case AST_BLOCK:
        {
            Ast_Statement *s = statement->stmt_block.statements_root;
            while (s)
            {
                if (!check_statement(s, function, functions_root))
                {
                    return false;
                }
                s = s->next;
            }
            return true;
        }
        break;

        case AST_RETURN:
        {
            Ast_Return *ast_return = &statement->stmt_return;
            if (type_is_void(function->type))
            {
                if (ast_return->expr)
                {
                    report_error(function->ident, "function type is void but return statement has expression");
                    return false;
                }
                return true;
            }
            if (!ast_return->expr)
            {
                report_error(function->ident, "function type is not void but return has no expression");
                return false;
            }
            return check_expr(ast_return->expr, function->type, function, functions_root);
        }
        break;

        case AST_EXPRESSION:
        {
            Ast_Function_Invocation *function_invocation = statement->stmt_expr.function_invocation;
            if (function_invocation)
            {
                Ident_Info ident_info;
                if (!lookup_ident_info(&ident_info, function_invocation->ident, function, functions_root))
                {
                    return false;
                }
                b32 check = check_function_invocation(function_invocation, &ident_info, function, functions_root);
                return check;
            }
            return true;
        }
        break;

        default: assert(0);
    }
    return true;
}

static b32 check_statement_definitely_returns(Ast_Statement *statement)
{
    if (statement->type == AST_RETURN)
    {
        return true;
    }
    else if (statement->type == AST_BLOCK)
    {
        b32 has_return = false;
        Ast_Statement *s = statement->stmt_block.statements_root;
        while (s && !has_return)
        {
            has_return = check_statement_definitely_returns(statement);
            s = statement->next;
        }
        return has_return;
    }
    else if (statement->type == AST_IF)
    {
        b32 if_has_return = check_statement_definitely_returns(statement->stmt_if.statement_if);
        b32 else_has_return = false;
        if (statement->stmt_if.statement_else)
        {
            else_has_return = check_statement_definitely_returns(statement->stmt_if.statement_else);
        }
        return if_has_return && else_has_return;
    }

    return false;
}

static b32 check_statements_definitely_return(Ast_Statement *statements_root)
{
    b32 have_return = false;
    Ast_Statement *statement = statements_root;
    while (statement && !have_return)
    {
        have_return = check_statement_definitely_returns(statement);
        statement = statement->next;
    }
    return have_return;
}

static b32 check_function(Ast_Function *function, Ast_Function *functions_root)
{
    // typecheck statements
    Ast_Statement *statement = function->statements_root;
    while (statement)
    {
        if (!check_statement(statement, function, functions_root))
        {
            return false;
        }
        statement = statement->next;
    }

    // check return always reachable
    if (!type_is_void(function->type))
    {
        if (!check_statements_definitely_return(function->statements_root))
        {
            report_error(function->ident, "function does not definitely have return");
            return false;
        }
    }

    return true;
}

b32 check_ast(Ast *ast)
{
    Ast_Function *functions_root = ast->functions_root;

    Ast_Function *function = functions_root;
    while (function)
    {
        if (!check_function(function, ast->functions_root))
        {
            return false;
        }
        function = function->next;
    }
    return true;
}

