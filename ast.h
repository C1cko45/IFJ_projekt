#ifndef AST_H
#define AST_H
#include "token.h"

typedef enum{
    NODE_STAT,     //preojenie jednotlivych stromov
    NODE_FUNC_DEF,  
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_VARL_DEF,  //definicia var a val
    NODE_ASSIGN,    
    NODE_EXPRESSION,
    NODE_TOKEN_ONLY,
    NODE_CALL,
    NODE_MAIN
}AST_NODE_TYPE;


typedef struct{
    AST_NODE_TYPE type;
    Token token;            //ak bude type tak token bude automaticky ukazovat na NULL a opacne
    struct AST_NODE *left_ch;  
    struct AST_NODE *right_ch;
}AST_NODE;

AST_NODE* ast_create_node(Token token);
AST_NODE* ast_create_node_type(AST_NODE type);
void ast_add_parent(AST_NODE *parent, AST_NODE *child, int side); //do parent sa prida child na stranu podla side(left -> side = 0, right -> side = 1)
void ast_free_node(AST_NODE *node);


#endif