CC=gcc
COMMON_FLAGS=-std=c99 #-D OS_LINUX
DEBUG_FLAGS=-g -Wall
RELEASE_FLAGS=-D NDEBUG -O3

SOURCES=src/main.c src/os.c src/memory_manager.c src/lexer.c src/parser.c src/typer.c src/string.c src/ast.c

.PHONY: default debug release

default: debug

debug: 
	$(CC) $(COMMON_FLAGS) $(DEBUG_FLAGS) $(SOURCES) -o c-frontend

release:
	$(CC) $(COMMON_FLAGS) $(RELEASE_FLAGS) $(SOURCES) -o c-frontend

