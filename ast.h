#ifndef AST_H
#define AST_H
#include "token.h"

typedef enum{
    NODE_PROGRAM,     //preojenie jednotlivych stromov
    NODE_FUNC_DEF, 
    NODE_MAIN, 
    NODE_CALL,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_VAR_DEF,
    NODE_VAL_DEF,  
    NODE_TOKEN_ONLY,
    NODE_VALMEN,
    NODE_LIST    //nody sa mozu pridat alebo ubrat podla potreby
    
}AST_NODE_TYPE;


typedef struct AST_NODE {
    AST_NODE_TYPE type;
    Token token;            //ak bude type tak token bude automaticky ukazovat na NULL a opacne
    struct AST_NODE *left_ch;  
    struct AST_NODE *right_ch;
}AST_NODE;

AST_NODE* ast_create_node(Token token);
AST_NODE* ast_create_node_type(AST_NODE_TYPE type);
void ast_add_parent(AST_NODE *parent, AST_NODE *child, int side); //do parent sa prida child na stranu podla side(left -> side = 0, right -> side = 1)
void ast_free_node(AST_NODE *node);


#endif