#include "ast.h"
#include "general.h"
#include "parser.h"
#include "lexer.h"
#include "memory_manager.h"
#include "string.h"
#include "token.h"
#include "os.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *filename;
    Memory_Manager memory_manager;
} Parser;

static Parser g_parser;

#define GET_MEMORY(size) (memory_manager_alloc(&g_parser.memory_manager, (size)))

static b32 parse_statement(Ast_Statement **statement);
static b32 parse_statements(Ast_Statement **statements_root);
static b32 parse_expression(Ast_Expression **ast_expression, b32 is_in_parenthesis);


static void report_error(Token *t, const char *message)
{
    if (t->type == TOKEN_UNCLOSED_COMMENT)
    {
        printf("parser error (%d,%d): unclosed comment\n", t->line, t->c0);
    }
    else if (t->type == TOKEN_UNCLOSED_STRING)
    {
        printf("parser error (%d,%d): unclosed string\n", t->line, t->c0);
    }
    else
    {
        printf("parser error (%d,%d): %s (found token type = %d)\n", t->line, t->c0, message, t->type);
    }
}

static b32 is_literal(i32 token_type)
{
    b32 result = token_type == TOKEN_LITERAL_STRING ||
                 token_type == TOKEN_LITERAL_INT ||
                 token_type == TOKEN_LITERAL_DOUBLE;
    return result;
}

static b32 is_type_keyword(i32 token_type)
{
    b32 result = token_type == TOKEN_KEYWORD_VOID ||
                 token_type == TOKEN_KEYWORD_CHAR ||
                 token_type == TOKEN_KEYWORD_INT ||
                 token_type == TOKEN_KEYWORD_DOUBLE;
    return result;
}

static b32 parse_type(Ast_Type **type)
{
    Token *token = lexer_peek_token(0);
    if (!is_type_keyword(token->type))
    {
        return false;
    }
    *type = GET_MEMORY(sizeof(Ast_Type));
    (*type)->token = token;
    (*type)->next = 0;
    lexer_eat_token();

    token = lexer_peek_token(0);
    while (token->type == '*')
    {
        (*type)->next = GET_MEMORY(sizeof(Ast_Type));
        (*type)->next->token = token;
        (*type)->next->next = 0;
        lexer_eat_token();
        token = lexer_peek_token(0);
        type = &(*type)->next;
    }

    return true;
}

static b32 ident_already_defined_in_function(StringRef ident_str, Ast_Parameter *params_root, Ast_Statement *statements_root)
{
    // search in parameters
    Ast_Parameter *param = params_root;
    while (param)
    {
        if (param->ident)
        {
            if (strings_equal_ref(param->ident->str_ref, ident_str))
            {
                return true;
            }
        }
        param = param->next;
    }

    // search in declarations
    Ast_Statement *stmt = statements_root;
    while (stmt && stmt->type == AST_DECLARATION)
    {
        if (strings_equal_ref(stmt->stmt_decl.ident->str_ref, ident_str))
        {
            return true;
        }
        stmt = stmt->next;
    }

    return false;
}

static b32 parse_function_invocation(Ast_Function_Invocation *invocation)
{
    Token *token;
    invocation->args_root = 0;

    token = lexer_peek_token(0);
    assert(token->type == TOKEN_IDENTIFIER);
    invocation->ident = token;
    lexer_eat_token();

    token = lexer_peek_token(0);
    assert(token->type == '(');
    lexer_eat_token();

    token = lexer_peek_token(0);
    if (token->type == ')')
    {
        lexer_eat_token();
        return true;
    }

    Ast_Argument **arg = &invocation->args_root;
    for (;;)
    {
        *arg = GET_MEMORY(sizeof(Ast_Argument));
        (*arg)->expr = 0;
        (*arg)->next = 0;

        if (!parse_expression(&(*arg)->expr, false))
        {
            return false;
        }

        token = lexer_peek_token(0);
        if (token->type != ',')
        {
            break;
        }

        lexer_eat_token();
        arg = &(*arg)->next;
    }

    if (token->type != ')')
    {
        report_error(token, "')' after last function-call argument expected");
        return false;
    }
    lexer_eat_token();

    return true;
}

static b32 expression_is_unary(Ast_Expression *expr)
{
    b32 is_unary = true;
    while (expr)
    {
        i32 token_type = expr->token->type;
        if ((token_type != '+' && token_type != '-' && token_type != '!'))
        {
            is_unary = false;
            break;
        }
        expr = expr->left;
    }
    return is_unary;
}

static i32 get_possible_operator_precedence(i32 token_type, b32 is_unary)
{
    i32 precedence;

    switch (token_type)
    {
        case TOKEN_OROR:
            precedence = 1;
        break;

        case TOKEN_ANDAND:
            precedence = 2;
        break;

        case TOKEN_EQEQ:
        case TOKEN_NE:
            precedence = 3;
        break;

        case TOKEN_GE:
        case TOKEN_LE:
        case '>':
        case '<':
            precedence = 4;
        break;

        case '+':
        case '-':
        case '!':
            if (is_unary) precedence = 7;
            else          precedence = 5;

        break;

        case '*':
        case '/':
        case '%':
            precedence = 6;
        break;

        default:
            precedence = 0;
        break;
    }
    
    return precedence;
}

static b32 parse_expression(Ast_Expression **root, b32 is_in_parenthesis)
{
    Ast_Expression **curr = root;
    i32 prev_precedence = -1;

    while (1)
    {
        // an iteration starts at the unallocated right node of an operator (root behaves the same)
        // a valid expression iteration is:
        // 1) unary operator
        // 2) operand + operator

        Token *token;
        Ast_Expression *operand_expr = 0;
        Token *operator = 0;
        b32 is_unary = false;

        // 1) unary operator
        token = lexer_peek_token(0);
        if (token->type == '+' || token->type == '-' || token->type == '!')
        {
            operator = token;
            is_unary = true;
        }
        // 2) process operand / prepare for operator
        else if (token->type == TOKEN_IDENTIFIER || is_literal(token->type) || token->type == '(')
        {
            // operand_expr
            operand_expr = GET_MEMORY(sizeof(Ast_Expression));
            memset(operand_expr, 0, sizeof(Ast_Expression));
            operand_expr->token = token;

            // just parenthesis
            if (token->type == '(')
            {
                lexer_eat_token();
                if (!parse_expression(&operand_expr->left, true))
                {
                    return false;
                }
                lexer_eat_token();
            }
            else if (token->type == TOKEN_IDENTIFIER)
            {
                Token *token1 = lexer_peek_token(1);
                if (token1->type == '(')
                {
                    operand_expr->function_invocation = GET_MEMORY(sizeof(Ast_Function_Invocation));
                    memset(operand_expr->function_invocation, 0, sizeof(Ast_Function_Invocation));
                    if (!parse_function_invocation(operand_expr->function_invocation))
                    {
                        return false;
                    }
                }
                else
                {
                    lexer_eat_token();
                }
            }
            else
            {
                lexer_eat_token();
            }
            operator = lexer_peek_token(0);
        }
        else if (is_in_parenthesis && token->type == ')')
        {
            return true;
        }
        else
        {
            report_error(token, "not an expression");
            return false;
        }

        i32 precedence = get_possible_operator_precedence(operator->type, is_unary);
        if (precedence == 0)
        {
            *curr = operand_expr;
            return true;
        }
        lexer_eat_token(); // operator viable and can be eaten

        if (precedence > prev_precedence)
        {
            *curr = GET_MEMORY(sizeof(Ast_Expression));
            memset(*curr, 0, sizeof(Ast_Expression));

            (*curr)->token = operator;
            (*curr)->left = operand_expr; // is 0 for unary

            curr = &(*curr)->right;
            prev_precedence = precedence;
        }
        else
        {
            *curr = operand_expr;

            // find the operator node that is to be substituted: go rightwards from root by precedence
            curr = root;
            b32 curr_is_unary = expression_is_unary(*curr);
            i32 curr_precedence = get_possible_operator_precedence((*curr)->token->type, curr_is_unary);
            while (precedence > curr_precedence)
            {
                curr = &(*curr)->right;
                b32 curr_is_unary = expression_is_unary(*curr);
                i32 curr_token_type = (*curr)->token->type;
                curr_precedence = get_possible_operator_precedence(curr_token_type, curr_is_unary);
            }

            // substitute
            Ast_Expression *substitute = GET_MEMORY(sizeof(struct Ast_Expression));
            substitute->token = operator;
            substitute->left = *curr;
            substitute->right = 0;
            substitute->function_invocation = 0;
            *curr = substitute;

            // update loop variables
            curr = &substitute->right;
            prev_precedence = precedence;
        }
    }
    return true;
}

static b32 parse_assignment(Ast_Assignment *ast_assignment)
{
    Token *token;

    token = lexer_peek_token(0);
    if (token->type != TOKEN_IDENTIFIER)
    {
        report_error(token, "identifier for assignment expected");
        return false;
    }
    ast_assignment->ident = token;
    lexer_eat_token();

    token = lexer_peek_token(0);
    if (token->type != '=')
    {
        report_error(token, "'=' for assignment expected");
        return false;
    }
    lexer_eat_token();

    b32 expr_parsed = parse_expression(&ast_assignment->expr, false);
    if (!expr_parsed)
    {
        return false;
    }

    token = lexer_peek_token(0);
    if (token->type != ';')
    {
        report_error(token, "';' at the end of assignment expected");
        return false;
    }
    lexer_eat_token();

    return true;
}

static b32 parse_while(Ast_While *ast_while)
{
    Token *token;

    // while
    token = lexer_peek_token(0);
    if (token->type != TOKEN_KEYWORD_WHILE)
    {
        report_error(token, "while keyword expected (internal error)");
        return false;
    }
    lexer_eat_token();

    // (
    token = lexer_peek_token(0);
    if (token->type != '(')
    {
        report_error(token, "'(' expected before while keyword");
        return false;
    }
    lexer_eat_token();

    // expression
    if (!parse_expression(&ast_while->expr, false))
    {
        return false;
    }

    // )
    token = lexer_peek_token(0);
    if (token->type != ')')
    {
        report_error(token, "')' expected after while expression");
        return false;
    }
    lexer_eat_token();

    // statement
    if (!parse_statement(&ast_while->statement))
    {
        return false;
    }

    return true;
}

static b32 parse_if(Ast_If *ast_if)
{
    Token *token = lexer_peek_token(0);
    // if
    if (token->type != TOKEN_KEYWORD_IF)
    {
        report_error(token, "if keyword expected (internal error)");
        return false;
    }
    lexer_eat_token();

    // (
    token = lexer_peek_token(0);
    if (token->type != '(')
    {
        report_error(token, "'(' expected before if expression");
        return false;
    }
    lexer_eat_token();

    // if-expression
    if (!parse_expression(&ast_if->expr, false))
    {
        return false;
    }
    
    // )
    token = lexer_peek_token(0);
    if (token->type != ')')
    {
        report_error(token, "')' expected after if expression");
        return false;
    }
    lexer_eat_token();

    // if-statement
    if (!parse_statement(&ast_if->statement_if))
    {
        return false;
    }

    // else
    token = lexer_peek_token(0);
    if (token->type != TOKEN_KEYWORD_ELSE)
    {
        return true;
    }
    lexer_eat_token();

    // else-statement
    if (!parse_statement(&ast_if->statement_else))
    {
        return false;
    }

    return true;
}

static b32 parse_block(Ast_Block *ast_block)
{
    Token *token;

    // {
    token = lexer_peek_token(0);
    if (token->type != '{')
    {
        report_error(token, "'{' expected for beginning of block (internal error)");
        return false;
    }
    lexer_eat_token();

    // statements
    if (!parse_statements(&ast_block->statements_root))
    {
        return false;
    }

    // }
    token = lexer_peek_token(0);
    if (token->type != '}')
    {
        report_error(token, "not a statement and not '}' for end of block");
        return false;
    }
    lexer_eat_token();

    return true;
}

static b32 parse_return(Ast_Return *ast_return)
{
    Token *token = lexer_peek_token(0);
    if (token->type != TOKEN_KEYWORD_RETURN)
    {
        report_error(token, "return keyword expected (internal error)");
        return false;
    }
    lexer_eat_token();

    token = lexer_peek_token(0);
    // ;
    if (token->type == ';')
    {
        lexer_eat_token();
        return true;
    }

    // expr ;
    if (!parse_expression(&ast_return->expr, false))
    {
        return false;
    }
    token  = lexer_peek_token(0);
    if (token->type != ';')
    {
        report_error(token, "missing ';' after at the end of return statement");
        return false;
    }
    lexer_eat_token();
    return true;
}

static void allocate_and_init_statement(Ast_Statement **statement, Ast_Node_Type type)
{
    *statement = GET_MEMORY(sizeof(Ast_Statement));
    memset(*statement, 0, sizeof(Ast_Statement));
    (*statement)->type = type;
}

static b32 parse_statement(Ast_Statement **statement)
{
    Token *token = lexer_peek_token(0);
    if (token->type == '{')
    {
        allocate_and_init_statement(statement, AST_BLOCK);
        b32 parsed = parse_block(&(*statement)->stmt_block);
        return parsed;
    }
    else if (token->type == TOKEN_KEYWORD_WHILE)
    {
        allocate_and_init_statement(statement, AST_WHILE);
        b32 parsed = parse_while(&(*statement)->stmt_while);
        return parsed;
    }
    else if (token->type == TOKEN_KEYWORD_IF)
    {
        allocate_and_init_statement(statement, AST_IF);
        b32 parsed = parse_if(&(*statement)->stmt_if);
        return parsed;
    }
    else if (token->type == TOKEN_IDENTIFIER)
    {
        Token *token1 = lexer_peek_token(1);
        // ident = expr;
        if (token1->type == '=')
        {
            allocate_and_init_statement(statement, AST_ASSIGNMENT);
            b32 parsed = parse_assignment(&(*statement)->stmt_assignment);
            return parsed;
        }
        // Func(...);
        else if (token1->type == '(')
        {
            allocate_and_init_statement(statement, AST_EXPRESSION);
            if (!parse_function_invocation((*statement)->stmt_expr.function_invocation))
            {
                return false;
            }

            token = lexer_peek_token(0);
            if (token->type != ';')
            {
                report_error(token, "';' expected after function call statement (in parse_statement)");
                return false;
            }
            lexer_eat_token();

            return true;
        }
        else
        {
            report_error(token1, "invalid statement after an identifier has been found (n_parse_statement)");
            return false;
        }
    }
    else if (token->type == TOKEN_KEYWORD_RETURN)
    {
        allocate_and_init_statement(statement, AST_RETURN);
        b32 parsed = parse_return(&(*statement)->stmt_return);
        return parsed;
    }

    report_error(token, "not a statement");
    return false;
}

static b32 parse_statements(Ast_Statement **statements_root)
{
    Ast_Statement **curr = statements_root;

    // workaround for declarations
    while (*curr)
    {
        curr = &(*curr)->next;
    }

    while (1)
    {
        b32 parsed = true;
        Token *token = lexer_peek_token(0);
        if (token->type == '{')
        {
            allocate_and_init_statement(curr, AST_BLOCK);
            parsed = parse_block(&(*curr)->stmt_block);
        }
        else if (token->type == TOKEN_KEYWORD_WHILE)
        {
            allocate_and_init_statement(curr, AST_WHILE);
            parsed = parse_while(&(*curr)->stmt_while);
        }
        else if (token->type == TOKEN_KEYWORD_IF)
        {
            allocate_and_init_statement(curr, AST_IF);
            parsed = parse_if(&(*curr)->stmt_if);
        }
        else if (token->type == TOKEN_KEYWORD_RETURN)
        {
            allocate_and_init_statement(curr, AST_RETURN);
            parsed = parse_return(&(*curr)->stmt_return);
        }
        else if (token->type == TOKEN_IDENTIFIER)
        {
            Token *token1 = lexer_peek_token(1);
            // assignment
            if (token1->type == '=')
            {
                allocate_and_init_statement(curr, AST_ASSIGNMENT);
                parsed = parse_assignment(&(*curr)->stmt_assignment);
            }
            // function invocation
            else if (token1->type == '(')
            {
                allocate_and_init_statement(curr, AST_EXPRESSION);
                if (!parse_function_invocation((*curr)->stmt_expr.function_invocation))
                {
                    return false;
                }
                
                token = lexer_peek_token(0);
                if (token->type != ';')
                {
                    report_error(token, "';' expected after function call statement (in parse_statements)");
                    return false;
                }
                lexer_eat_token();
                parsed = true;
            }
            else
            {
                report_error(token1, "invalid statement after an identifier has been found (in parse_statements)");
                parsed = false;
            }
        }
        else
        {
            break;
        }

        if (!parsed)
        {
            return false;
        }

        curr = &(*curr)->next;
    }
    return true;
}

static b32 parse_declarations(Ast_Statement **statements_root, Ast_Function *function)
{
    Ast_Statement **statement_it = statements_root;

    Token *token = lexer_peek_token(0);
    while (is_type_keyword(token->type))
    {
        Ast_Type *type = 0;
        Token *ident;

        if (!parse_type(&type))
        {
            return false;
        }

        // identifier
        token = lexer_peek_token(0);
        if (token->type != TOKEN_IDENTIFIER)
        {
            report_error(token, "identifier expected for declaration");
            return false;
        }
        if (ident_already_defined_in_function(token->str_ref, function->params_root, function->statements_root))
        {
            report_error(token, "ident is already defined");
            return false;
        }
        ident = token;
        lexer_eat_token();

        allocate_and_init_statement(statement_it, AST_DECLARATION);
        Ast_Declaration *decl = &(*statement_it)->stmt_decl;
        decl->type = type;
        decl->ident = ident;
        decl->expr = 0;
        decl->next = 0;

        // ;
        token = lexer_peek_token(0);
        if (token->type == ';')
        {
            lexer_eat_token();
        }
        // =
        else
        {
            if (token->type != '=')
            {
                report_error(token, "'=' expected for declaration");
                return false;
            }
            lexer_eat_token();

            // expr
            if (!parse_expression(&decl->expr, false))
            {
                return false;
            }

            // ;
            token = lexer_peek_token(0);
            if (token->type != ';')
            {
                report_error(token, "';' expected at the end of the declaration");
                return false;
            }
            lexer_eat_token();
        }

        token = lexer_peek_token(0);
        statement_it = &(*statement_it)->next;
    }
    return true;
}


static void allocate_and_zero_param(Ast_Parameter **param)
{
    *param = GET_MEMORY(sizeof(Ast_Parameter));
    memset(*param, 0, sizeof(Ast_Parameter));
}

static b32 parse_function_parameters(Ast_Parameter **params_root)
{
    Ast_Parameter **param = params_root;
    Token *token;

    // void
    token = lexer_peek_token(0);
    if (token->type == ')')
    {
        allocate_and_zero_param(param);
        return true;
    }
    if (token->type == TOKEN_KEYWORD_VOID)
    {
        Token *token1 = lexer_peek_token(1);
        if (token1->type == ')')
        {
            allocate_and_zero_param(param);
            (*param)->type = GET_MEMORY(sizeof(Ast_Type));
            (*param)->type->token = token;
            (*param)->type->next = 0;
            lexer_eat_token();
            return true;
        }
    }

    // real arguments
    while (1)
    {
        Ast_Type *type;
        Token *ident = 0;

        Token *token = lexer_peek_token(0);
        if (!is_type_keyword(token->type))
        {
            report_error(token, "not a valid parameter type");
            return false;
        }
        parse_type(&type);

        token = lexer_peek_token(0);
        if (token->type != TOKEN_IDENTIFIER)
        {
            report_error(token, "identifier expected after parameter type");
            return false;
        }
        Ast_Parameter *param_defined_searcher = *params_root;
        while (param_defined_searcher)
        {
            if (strings_equal_ref(param_defined_searcher->ident->str_ref, token->str_ref))
            {
                report_error(token, "parameter is already defined");
                return false;
            }
            param_defined_searcher = param_defined_searcher->next;
        }
        ident = token;
        lexer_eat_token();

        allocate_and_zero_param(param);
        (*param)->type = type;
        (*param)->ident = ident;

        token = lexer_peek_token(0);
        if (token->type != ',')
        {
            break;
        }

        lexer_eat_token(); // eat ','
        param = &(*param)->next;
    }
    return true;
}

static b32 parse_functions(Ast_Function **functions_root)
{
    Ast_Function **function = functions_root;

    Token *token = lexer_peek_token(0);
    while (is_type_keyword(token->type)) // global variables not existing yet
    {
        Ast_Type *type;
        Token *ident;

        if (!parse_type(&type))
        {
            return false;
        }

        // ident
        ident = lexer_peek_token(0);
        if (ident->type != TOKEN_IDENTIFIER)
        {
            report_error(ident, "identifier expected for function declaration");
            return false;
        }
        lexer_eat_token();

        // verify that ident is not already defined as a function
        Ast_Function *function_it = *functions_root;
        while (function_it)
        {
            if (strings_equal_ref(function_it->ident->str_ref, ident->str_ref))
            {
                report_error(ident, "function identifier is already defined");
                return false;
            }
            function_it = function_it->next;
        }

        token = lexer_peek_token(0);
        if (token->type != '(')
        {
            report_error(token, "'(' expected for function declaration");
            return false;
        }
        lexer_eat_token();

        *function = GET_MEMORY(sizeof(Ast_Function));
        memset(*function, 0, sizeof(Ast_Function));
        (*function)->type = type;
        (*function)->ident = ident;

        if (!parse_function_parameters(&(*function)->params_root))
        {
            return false;
        }

        // )
        token = lexer_peek_token(0);
        if (token->type != ')')
        {
            report_error(token, "not a function parameter");
            return false;
        }
        lexer_eat_token();

        // {
        token = lexer_peek_token(0);
        if (token->type != '{')
        {
            report_error(token, "'{' expected for function declaration");
            return false;
        }
        lexer_eat_token();

        // declarations
        if (!parse_declarations(&(*function)->statements_root, *function))
        {
            return false;
        }

        // statements
        if (!parse_statements(&(*function)->statements_root))
        {
            return false;
        }
        
        // }
        token = lexer_peek_token(0);
        if (token->type != '}')
        {
            report_error(token, "'}' expected for function declaration");
            return false;
        }
        lexer_eat_token();

        token = lexer_peek_token(0);
        function = &(*function)->next;
    }

    return true;
}

b32 parse_file(const char *filepath, Ast *ast)
{
    char *source_code = os_read_file_as_string(filepath);
    if (!source_code)
    {
        return false;
    }
    lexer_init(source_code);

    g_parser.filename = filepath;
    memory_manager_init(&g_parser.memory_manager, MEGABYTES(1));

    Token *token;
    ast->functions_root = 0;
    if (!parse_functions(&ast->functions_root))
    {
        return false;
    }

    // eof
    token = lexer_peek_token(0);
    if (token->type != '\0')
    {
        report_error(token, "eof expected");
        return false;
    }
    lexer_eat_token();

    return true;
}

