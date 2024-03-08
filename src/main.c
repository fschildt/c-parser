#include "parser.h"
#include "typer.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("error: no filepath specified\n");
        return false;
    }
    const char *filepath = argv[1];

    Ast ast;

    if (!parse_file(filepath, &ast)) {
        return 0;
    }

    if (!check_ast(&ast)) {
        return 0;
    }

    ast_print(&ast);

    return 0;
}
