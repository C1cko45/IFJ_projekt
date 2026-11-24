CC = gcc

CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Werror

CLANG = clang

CLANGFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O1 -g -fuse-ld=mold
CLANG_RUNTIME_ANALYSIS = -std=c99 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined \
						 -fno-omit-frame-pointer

MAIN_SRC = test_main.c

SCANNER_SRCS = scanner.c dstring.c stack.c token.c
PARSER_SRCS = ast.c parser.c psa.c 
SEMANTIC_SRCS = semantic_analysis.c symtable.c
GENERATOR_SRCS = gen_def.c gen_nondef.c codegen.c

SRCS = $(MAIN_SRC) $(SCANNER_SRCS) $(PARSER_SRCS)

OBJS = $(SRCS:.c=.o)

EXEC = ifj26_compiler

.PHONY: all build static runtime clean

all: build

build: $(EXEC)

# use Clang's static analysis tool with aggressive warnings
debug: $(SRCS)
	$(CLANG) $(CLANGFLAGS) $(SRCS) -o $(EXEC)

# use Clang's runtime analysis tool with aggressive warnings
runtime: $(SRCS)
	$(CLANG) $(CLANG_RUNTIME_ANALYSIS) $(SRCS) -o $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EXEC)
