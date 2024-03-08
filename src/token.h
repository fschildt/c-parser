#ifndef TOKEN_H
#define TOKEN_H

#include "general.h"
#include "string.h"

enum TokenType {
    // 0-255 are for ascii characters

    TOKEN_ERROR = 256,

    TOKEN_IDENTIFIER,

    TOKEN_KEYWORD_VOID,
    TOKEN_KEYWORD_CHAR,
    TOKEN_KEYWORD_INT,
    TOKEN_KEYWORD_DOUBLE,

    TOKEN_KEYWORD_RETURN,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_ELSE,
    TOKEN_KEYWORD_WHILE,

    TOKEN_UNCLOSED_COMMENT,
    TOKEN_UNCLOSED_STRING,

    TOKEN_LITERAL_STRING,
    TOKEN_LITERAL_INT,
    TOKEN_LITERAL_DOUBLE,

    TOKEN_PLUSPLUS,
    TOKEN_MINUSMINUS,

    TOKEN_EQEQ,
    TOKEN_LE,
    TOKEN_GE,
    TOKEN_NE,
    TOKEN_ANDAND,
    TOKEN_OROR,
};

typedef struct {
    i32 type;

    i32 c0, c1;
    i32 line;

    union {
        StringRef str_ref;
        i32 int_value;
        b32 bool_value;
    };
} Token;

#endif // TOKEN_H
