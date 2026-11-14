#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "scanner.h"
#include "parser.h"
#include "ast.h"
#include "token.h"

/* ---------- farby (len ak vystup ide do terminalu) ---------- */
static int use_color = 0;
#define C_RESET  (use_color ? "\033[0m"  : "")
#define C_DIM    (use_color ? "\033[2m"  : "")
#define C_BLUE   (use_color ? "\033[34m" : "")
#define C_GREEN  (use_color ? "\033[32m" : "")
#define C_YELLOW (use_color ? "\033[33m" : "")
#define C_CYAN   (use_color ? "\033[36m" : "")

static const char *node_type_name(AST_NODE_TYPE type) {
    switch (type) {
        case NODE_PROGRAM:    return "PROGRAM";
        case NODE_FUNC_DEF:   return "FUNC_DEF";
        case NODE_MAIN:       return "MAIN";
        case NODE_CALL:       return "CALL";
        case NODE_IF:         return "IF";
        case NODE_WHILE:      return "WHILE";
        case NODE_FOR:        return "FOR";
        case NODE_VAR_DEF:    return "VAR_DEF";
        case NODE_VAL_DEF:    return "VAL_DEF";
        case NODE_TOKEN_ONLY: return "TOKEN";
        case NODE_VALMEM:     return "VALMEM";
        case NODE_LIST:       return "LIST";
        case NODE_ARGUMENT:   return "ARGUMENT";
        case NODE_PODMIENKA:  return "PODMIENKA";
        case NODE_ELSE:       return "ELSE";
        default:              return "UNKNOWN_NODE";
    }
}

/* Skratene meno tokenu: odstrani prefix TOKEN_TYPE_ */
static const char *short_token_name(TokenType type) {
    const char *name = token_type_name(type);
    const char *prefix = "TOKEN_TYPE_";
    if (strncmp(name, prefix, strlen(prefix)) == 0) return name + strlen(prefix);
    return name;
}

#define MAX_PREFIX 1024

static void print_node(const AST_NODE *node, const char *prefix,
                       const char *branch, const char *label) {
    printf("%s%s%s", C_DIM, prefix, branch);
    printf("%s%s%s", C_YELLOW, label, C_RESET);

    if (node->type == NODE_TOKEN_ONLY) {
        printf("%s%s%s", C_GREEN, short_token_name(node->token.type), C_RESET);
        if (node->token.type == TOKEN_TYPE_IDENTIFIER && node->token.attr.str_val)
            printf(" %s\"%s\"%s", C_CYAN, node->token.attr.str_val, C_RESET);
    } else {
        printf("%s%s%s", C_BLUE, node_type_name(node->type), C_RESET);
    }
    printf("\n");
}

static void print_tree(const AST_NODE *node, const char *prefix, int is_root) {
    const AST_NODE *kids[2] = { node->left_ch, node->right_ch };
    const char *labels[2]   = { "L: ", "R: " };

    int count = 0;
    for (int i = 0; i < 2; i++) if (kids[i]) count++;

    int seen = 0;
    for (int i = 0; i < 2; i++) {
        if (!kids[i]) continue;
        seen++;
        int last = (seen == count);

        print_node(kids[i], prefix, last ? "└── " : "├── ", labels[i]);

        char next[MAX_PREFIX];
        snprintf(next, sizeof next, "%s%s", prefix, last ? "    " : "│   ");
        print_tree(kids[i], next, 0);
    }
    (void)is_root;
}

static void print_ast(const AST_NODE *root) {
    if (!root) { printf("(prazdny strom)\n"); return; }
    printf("%s%s%s\n", C_BLUE, node_type_name(root->type), C_RESET);
    print_tree(root, "", 1);
}

int main(int argc, char *argv[]) {
    FILE *input = NULL;
    use_color = isatty(STDOUT_FILENO);

    if (argc > 1) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            fprintf(stderr, "Chyba pri otvarani suboru %s\n", argv[1]);
            return 99;
        }
        scanner_init(input);
    } else {
        scanner_init(stdin);
    }

    AST_NODE *root = program();   // pri syntaktickej chybe ukonci proces s kodom 2

    printf("\n%s=========== AST ===========%s\n", C_DIM, C_RESET);
    print_ast(root);
    printf("%s===========================%s\n", C_DIM, C_RESET);
    printf("%sL = lavy potomok, R = pravy potomok%s\n", C_DIM, C_RESET);

    if (input) fclose(input);
    return 0;
}