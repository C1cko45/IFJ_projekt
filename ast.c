#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
AST_NODE *ast_create_node(Token token)
{

    AST_NODE *new_node = (AST_NODE *)malloc(sizeof(AST_NODE));
    if (new_node == NULL)
    {
        fprintf(stderr, "CHYBA"); // vymysliet vypis funkcie
        exit(99);
    }
    new_node->type = NODE_TOKEN_ONLY;
    new_node->token = token;

    new_node->right_ch = NULL;
    new_node->left_ch = NULL;

    return new_node;
}
AST_NODE *ast_create_node_type(AST_NODE_TYPE type)
{

    AST_NODE *new_node = (AST_NODE *)malloc(sizeof(AST_NODE));
    if (new_node == NULL)
    {
        fprintf(stderr, "CHYBA"); // vymysliet vypis funkcie
        exit(99);
    }
    new_node->type = type;
    Token empty_token = {0};
    new_node->token = empty_token;

    new_node->right_ch = NULL;
    new_node->left_ch = NULL;

    return new_node;
}

void ast_add_parent(AST_NODE *parent, AST_NODE *child, int side)
{
    if (parent == NULL)
    {
        return;
    }

    if (side == 0)
    {
        parent->left_ch = child; // vlozime do left_ch pointer na nas novy child
    }
    else
    {
        parent->right_ch = child;
    }
}

void ast_free_node(AST_NODE *node)
{
    // nieje co uvolnit.
    if (node == NULL)
    {
        return;
    }

    ast_free_node(node->left_ch);
    ast_free_node(node->right_ch);

    if (node->type == NODE_TOKEN_ONLY)
    {
        token_free(&node->token);
    }

    free(node);
}
