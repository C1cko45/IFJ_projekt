CC = gcc

CLANG = clang

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g

CLANGFLAGS = -g -O0 -Wall -Werror -Wpedantic

MAIN_SRC = main.c

SCANNER_SRCS = scanner.c dstring.c stack.c token.c

PARSER_SRCS = ast.c parser.c psa.c 

SEMANTIC_SRCS = semantic_analysis.c symtable.c

GENERATOR_SRCS = gen_def.c gen_nondef.c codegen.c

SRCS = $(MAIN_SRC) $(SCANNER_SRCS) $(PARSER_SRCS) $(SEMANTIC_SRCS)

OBJS = $(SRCS:.c=.o)

EXEC = ifj26_compiler

.PHONY: all build clean

all: build

build: $(EXEC)

$(EXEC): $(SRCS)
	$(CLANG) $(CLANGFLAGS) $(SRCS) -o $@

%.o: %.c
	$(CLANG) $(CLANGFLAGS) -c $< -o $@

clean:
	rm -f $(EXEC)
