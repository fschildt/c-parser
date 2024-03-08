#ifndef LEXER_H
#define LEXER_H

#include "general.h"
#include "string.h"
#include "token.h"

void lexer_init(const char *file_as_string);
Token* lexer_peek_token(i32 lookahead);
void lexer_eat_token();

#endif // LEXER_H
