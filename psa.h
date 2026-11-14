
#ifndef PSA_H
#define PSA_H
#include "parser.h"

typedef enum {
    P_I, P_PLUS, P_MINUS, P_MUL, P_DIV, P_LPAREN, P_RPAREN, P_DOLLAR,
    P_UPLUS, P_UMINUS, P_REL, P_EQ, P_NOT, P_AND, P_OR, P_COUNT, P_EXPR
} PrecIndex;

// N = prazdne (chyba), L = <, G = >, Q = =
typedef enum { N, L, G, Q } PrecRel;

typedef struct{
    PrecIndex idx;      //
    Token token;
    AST_NODE *node;
}STACK_ITEM;

AST_NODE* parse_expression();
STACK_ITEM get_next_item(Token token, PrecIndex prev);
void insert(STACK_ITEM);
#endif