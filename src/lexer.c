// for nothings state-machine-approach see https://nothings.org/computer/lexing.html

#include "lexer.h"
#include "memory_manager.h"
#include <string.h>

#define TOKEN_CACHE_SIZE 2 // 1 + max(x) in lexer_peek_token(x)

typedef struct {
    i32 start_index;
    i32 cnt_cached;
    Token* token[TOKEN_CACHE_SIZE];
} Token_Cache;

typedef struct {
    const char *parse_point;
    i32 current_line;
    i32 current_char;
    Token_Cache token_cache;
    Memory_Manager memory_manager;
} Lexer;

static Lexer g_lexer;

static b32 is_alphabetical(char c) {
    return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

static b32 is_numerical(char c) {
    return c>='0' && c<='9';
}

static b32 is_alphanumerical(char c) {
    return is_alphabetical(c) || is_numerical(c);
}

static Token* get_token() {
    Token *token = memory_manager_alloc(&g_lexer.memory_manager, sizeof(Token));

    const char *p = g_lexer.parse_point;
    i32 current_line = g_lexer.current_line;
    i32 current_char = g_lexer.current_char;

    b32 unclosed_comment = false;
    b32 could_skip = true;
    while (could_skip)
    {
        // single-line comment
        if (p[0] == '/' && p[1] == '/')
        {
            p += 2;
            current_char += 2;

            // skip everything in line
            while ((p[0] != '\0' && p[0] != '\n') || (p[0] == '\r' && p[1] != '\n'))
            {
                p++;
            }

            // handle end of line
            if (p[0] == '\n')
            {
                p++;
                current_line++;
                current_char = 1;
            }
            else if (p[0] == '\r' && p[1] == '\n')
            {
                p += 2;
                current_line++;
                current_char = 1;
            }
            continue;
        }

        // multiline comment
        else if (p[0] == '/' && p[1] == '*')
        {
            p += 2;
            current_char += 2;

            // skip everything in comment
            while (p[0] != '\0' && !(p[0] == '*' && p[1] == '/'))
            {

                // handle newlines
                if (p[0] == '\n')
                {
                    p++;
                    current_line++;
                    current_char = 1;
                }
                else if (p[0] == '\r' && p[1] == '\n')
                {
                    p += 2;
                    current_line++;
                    current_char = 1;
                }

                // handle all other characters
                else
                {
                    p++;
                    current_char++;
                }
            }

            // handle end of comment
            if (p[0] == '*' && p[1] == '/')
            {
                p += 2;
                current_char += 2;
            }
            else
            {
                unclosed_comment = true;
                break;
            }
            continue;
        }

        // whitespace
        else if (p[0] == ' ' || p[0] == '\t')
        {
            p++;
            current_char++;
        }
        else if (p[0] == '\n')
        {
            p++;
            current_char = 1;
            current_line++;
        }
        else if (p[0] == '\r' && p[1] == '\n')
        {
            p += 2;
            current_char = 1;
            current_line++;
        }

        else
            could_skip = 0;
    }

    const char *token_start = p;
    char c = *p;

    // unclosed comment
    if (unclosed_comment)
    {
        token->type = TOKEN_UNCLOSED_COMMENT;
    }
    // alphanumericals
    else if (is_alphabetical(c) || c == '_')
    {
        p++;

        char c = *p;
        while (is_alphanumerical(c) || c == '_')
        {
            p++;
            c = *p;
        }

        i32 token_length = p - token_start;
        p--;

        // if, else, while
        if (strings_equal(token_start, token_length, "if", strlen("if")))
        {
            token->type = TOKEN_KEYWORD_IF;
        }
        else if (strings_equal(token_start, token_length, "else", strlen("else")))
        {
            token->type = TOKEN_KEYWORD_ELSE;
        }
        else if (strings_equal(token_start, token_length, "while", strlen("while")))
        {
            token->type = TOKEN_KEYWORD_WHILE;
        }

        // void, int, bool, string, double
        else if (strings_equal(token_start, token_length, "void", strlen("void")))
        {
            token->type = TOKEN_KEYWORD_VOID;
        }
        else if (strings_equal(token_start, token_length, "int", strlen("int")))
        {
            token->type = TOKEN_KEYWORD_INT;
        }
        else if (strings_equal(token_start, token_length, "char", strlen("char")))
        {
            token->type = TOKEN_KEYWORD_CHAR;
        }
        else if (strings_equal(token_start, token_length, "double", strlen("double")))
        {
            token->type = TOKEN_KEYWORD_DOUBLE;
        }

        else if (strings_equal(token_start, token_length, "return", strlen("return")))
        {
            token->type = TOKEN_KEYWORD_RETURN;
        }
        else
        {
            token->type = TOKEN_IDENTIFIER;
        }

        token->str_ref.location = token_start;
        token->str_ref.length = token_length;
    }

    // int/double literal
    else if (is_numerical(c))
    {
        p++;
        while (is_numerical(*p))
        {
            p++;
        }

        b8 is_double = false;
        if (*p == '.')
        {
            is_double = true;
            p++;
            while (is_numerical(*p))
            {
                p++;
            }
        }

        i32 token_length = p - token_start;
        token->type = is_double ? TOKEN_LITERAL_DOUBLE : TOKEN_LITERAL_INT;
        token->str_ref.location = token_start;
        token->str_ref.length = token_length;
        p--;
    }

    // string literal
    else if (c == '\"')
    {
        // TODO: many characters are not allowed in string
        p++;
        while (*p != '\0' && *p != '\"')
        {
            p++;
        }

        if (*p == '\"')
        {
            i32 token_length = p - token_start + 1;
            token->type = TOKEN_LITERAL_STRING;
            token->str_ref.location = token_start;
            token->str_ref.length = token_length;
        }
        else
        {
            token->type = TOKEN_UNCLOSED_STRING;
        }
    }

    // /, *, %
    else if (c == '/') token->type = '/';
    else if (c == '*') token->type = '*';
    else if (c == '%') token->type = '%';

    // + and ++
    else if (c == '+')
    {
        if (p[1] == '+')
        {
            token->type = TOKEN_PLUSPLUS;
            p++;
        }
        else
            token->type = '+';
    }

    // - and --
    else if (c == '-')
    {
        if (p[1] == '-')
        {
            token->type = TOKEN_MINUSMINUS;
            p++;
        }
        else
            token->type = '-';
    }

    // > and >=
    else if (c == '>')
    {
        if (p[1] == '=')
        {
            p++;
            token->type = TOKEN_GE;
        }
        else
            token->type = '>';
    }

    // < and <=
    else if (c == '<')
    {
        if (p[1] == '=')
        {
            p++;
            token->type = TOKEN_LE;
        }
        else
            token->type = '<';
    }

    // = and ==
    else if (c == '=')
    {
        if (p[1] == '=')
        {
            p++;
            token->type = TOKEN_EQEQ;
        }
        else token->type = '=';
    }

    // ! and !=
    else if (c == '!')
    {
        if (p[1] == '=')
        {
            p++;
            token->type = TOKEN_NE;
        }
        else token->type = '!';
    }

    // &&
    else if (c == '&')
    {
        if (p[1] == '&')
        {
            p++;
            token->type = TOKEN_ANDAND;
        }
        else token->type = '&';
    }

    // ||
    else if (c == '|')
    {
        if (p[1] == '|')
        {
            p++;
            token->type = TOKEN_OROR;
        }
        else token->type = '|';
    }
    
    // ., ;
    else if (c == ';') token->type = ';';
    else if (c == '.') token->type = '.';
    else if (c == ',') token->type = ',';

    // parenthesis (), braces {}, brackets []
    else if (c == '{') token->type = '{';
    else if (c == '}') token->type = '}';
    else if (c == '(') token->type = '(';
    else if (c == ')') token->type = ')';
    else if (c == '[') token->type = '[';
    else if (c == ']') token->type = ']';

    else if (c == '\0') token->type = '\0';
    else token->type = TOKEN_ERROR;

    i32 token_length = p - token_start + 1;

    // update token location
    i32 c0 = current_char;
    token->c0 = c0;
    token->c1 = c0 + token_length - 1;
    token->line = current_line;

    // update lexer position
    g_lexer.parse_point = p + 1;
    g_lexer.current_char = current_char + token_length;
    g_lexer.current_line = current_line;

    return token;
}

void lexer_eat_token()
{
    if (g_lexer.token_cache.start_index == TOKEN_CACHE_SIZE-1)
        g_lexer.token_cache.start_index = 0;
    else
        g_lexer.token_cache.start_index++;

    g_lexer.token_cache.cnt_cached--;
}

Token* lexer_peek_token(i32 lookahead)
{
    i32 cnt_cached  = g_lexer.token_cache.cnt_cached;
    i32 start_index = g_lexer.token_cache.start_index;

    // find index
    i32 index = start_index + lookahead;
    if (index >= TOKEN_CACHE_SIZE)
    {
        index -= TOKEN_CACHE_SIZE;
    }

    // possibly insert new token to cache
    if (lookahead == cnt_cached)
    {
        Token *token = get_token();
        g_lexer.token_cache.token[index] = token;
        g_lexer.token_cache.cnt_cached++;
    }

    return g_lexer.token_cache.token[index];
}

void lexer_init(const char *file_as_string) {
    g_lexer.parse_point = file_as_string;
    g_lexer.current_line = 1;
    g_lexer.current_char = 1;

    g_lexer.token_cache.start_index = 0;
    g_lexer.token_cache.cnt_cached = 0;

    memory_manager_init(&g_lexer.memory_manager, MEGABYTES(1));
}

