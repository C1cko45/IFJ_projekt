#define _POSIX_C_SOURCE 200809L
#include "scanner.h"
#include <stdlib.h>
#include <string.h>

static FILE *input_file = NULL;

#define T(x) { #x, x }
typedef struct { const char *name; TokenType type; } TokenEntry;

static const TokenEntry TABLE[] = {
    T(TOKEN_TYPE_EOF), T(TOKEN_TYPE_EOL), T(TOKEN_TYPE_INDENT), T(TOKEN_TYPE_DEDENT),
    T(TOKEN_TYPE_IDENTIFIER), T(TOKEN_TYPE_KEYWORD),
    T(TOKEN_TYPE_INT), T(TOKEN_TYPE_DOUBLE), T(TOKEN_TYPE_STRING),
    T(TOKEN_TYPE_UNIT), T(TOKEN_TYPE_BOOL),
    T(TOKEN_TYPE_PLUS), T(TOKEN_TYPE_MINUS), T(TOKEN_TYPE_MUL), T(TOKEN_TYPE_DIV),
    T(TOKEN_TYPE_ASSIGN),
    T(TOKEN_TYPE_EQ), T(TOKEN_TYPE_NEQ), T(TOKEN_TYPE_LT), T(TOKEN_TYPE_LTE),
    T(TOKEN_TYPE_GT), T(TOKEN_TYPE_GTE),
    T(TOKEN_TYPE_NOT), T(TOKEN_TYPE_AND), T(TOKEN_TYPE_OR), T(TOKEN_TYPE_ARROW),
    T(TOKEN_TYPE_CONCAT), T(TOKEN_TYPE_DOT),
    T(TOKEN_TYPE_LPAREN), T(TOKEN_TYPE_RPAREN), T(TOKEN_TYPE_LBRACKET),
    T(TOKEN_TYPE_RBRACKET), T(TOKEN_TYPE_COLON), T(TOKEN_TYPE_COMMA),
    T(TOKEN_TYPE_SEMICOLON), T(TOKEN_TYPE_AT_MAIN),
    T(TOKEN_ERROR_LEX),
    T(KEYWORD_DEF), T(KEYWORD_DO), T(KEYWORD_ELSE), T(KEYWORD_IF),
    T(KEYWORD_IMPORT), T(KEYWORD_RETURN), T(KEYWORD_THEN), T(KEYWORD_VAL),
    T(KEYWORD_WHILE), T(KEYWORD_VAR),
    T(KEYWORD_STRING), T(KEYWORD_DOUBLE), T(KEYWORD_INT), T(KEYWORD_UNIT),
    T(KEYWORD_BOOLEAN), T(KEYWORD_FOR), T(KEYWORD_LIST),
    T(KEYWORD_TO), T(KEYWORD_UNTIL), T(KEYWORD_BY),
};
#define TABLE_SIZE (sizeof(TABLE) / sizeof(TABLE[0]))

const char *token_type_name(TokenType type) {
    for (size_t i = 0; i < TABLE_SIZE; i++)
        if (TABLE[i].type == type) return TABLE[i].name;
    return "UNKNOWN_TOKEN";
}

void scanner_init(FILE *f) {
    input_file = (f != NULL) ? f : stdin;
}

Token scanner_get_next_token(void) {
    Token token = {0};
    token.type = TOKEN_ERROR_LEX;

    if (input_file == NULL) input_file = stdin;

    char buffer[128];
    if (fscanf(input_file, "%127s", buffer) != 1) {
        token.type = TOKEN_TYPE_EOF;
        return token;
    }

    // Identifikator s menom: TOKEN_TYPE_IDENTIFIER:meno
    const char *id_prefix = "TOKEN_TYPE_IDENTIFIER:";
    if (strncmp(buffer, id_prefix, strlen(id_prefix)) == 0) {
        token.type = TOKEN_TYPE_IDENTIFIER;
        token.attr.str_val = strdup(buffer + strlen(id_prefix));
        return token;
    }

    for (size_t i = 0; i < TABLE_SIZE; i++) {
        if (strcmp(buffer, TABLE[i].name) == 0) {
            token.type = TABLE[i].type;
            return token;
        }
    }
    return token;   // TOKEN_ERROR_LEX
}

void token_free(Token *token) {
    if (token && token->type == TOKEN_TYPE_IDENTIFIER && token->attr.str_val) {
        free(token->attr.str_val);
        token->attr.str_val = NULL;
    }
}
