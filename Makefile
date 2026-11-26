CC = gcc
CLANG = clang

CFLAGS = -std=c99 -Wall -Wextra -Wpedantic
CLANGFLAGS = -std=c99 -Wall -Wextra -Werror -Wpedantic -g -O0 -fsanitize=unsigned-integer-overflow,address,undefined \
			 -fno-omit-frame-pointer

MAIN_SRC = main.c

SCANNER_SRCS = scanner.c dstring.c stack.c token.c
PARSER_SRCS = ast.c parser.c psa.c 
SEMANTIC_SRCS = semantic_analysis.c symtable.c
GENERATOR_SRCS = gen_def.c gen_nondef.c codegen.c

CLEANUP_SRCS = cleanup.c

SRCS = $(MAIN_SRC) $(SCANNER_SRCS) $(PARSER_SRCS) $(SEMANTIC_SRCS) $(GENERATOR_SRCS) $(CLEANUP_SRCS)

OBJS = $(SRCS:.c=.o)

EXEC = ifj26_compiler

.PHONY: all build debug clean

all: build

build: $(EXEC)

debug: $(SRCS)
	$(CLANG) $(CLANGFLAGS) $(SRCS) -o $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EXEC)
