CC = gcc
CLANG = clang

CFLAGS = -std=c99 -Wall -Wextra -Wpedantic
CLANGFLAGS = -std=c99 -Wall -Wextra -Werror -Wpedantic -g -O0

MAIN_SRC = main.c

TEST_MAIN_SRC = test_main.c

SCANNER_SRCS = scanner.c dstring.c stack.c token.c
PARSER_SRCS = ast.c parser.c psa.c 
SEMANTIC_SRCS = semantic_analysis.c symtable.c
GENERATOR_SRCS = gen_def.c gen_nondef.c codegen.c

CLEANUP_SRCS = cleanup.c

SRCS = $(MAIN_SRC) $(SCANNER_SRCS) $(PARSER_SRCS) $(SEMANTIC_SRCS) $(GENERATOR_SRCS) $(CLEANUP_SRCS)

SRCS_PRINT = $(MAIN_SRC) $(SCANNER_SRCS) $(PARSER_SRCS)

OBJS = $(SRCS:.c=.o)

EXEC = ifj26_compiler

.PHONY: all print build debug clean

all: build

build: $(EXEC)

debug: $(SRCS)
	$(CLANG) $(CLANGFLAGS) $(SRCS) -o $(EXEC)

print: $(SRCS)
	$(CLANG) $(CLANGFLAGS) $(SRCS_PRINT) -o $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EXEC)
