#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

b32 parse_file(const char *filepath, Ast *ast);

#endif // PARSER_H
