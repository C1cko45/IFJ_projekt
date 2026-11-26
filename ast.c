#include "ast.h"
#include <stdio.h>
#include <stdlib.h>

// ast_create_node: vytvori listovy uzol NODE_TOKEN_ONLY, ktory nesie token (identifikator, literal, operator, typ...)
//   pri chybe alokacie ukonci program s kodom 99
AST_NODE* ast_create_node(Token token){

    AST_NODE *new_node = (AST_NODE*) malloc (sizeof(AST_NODE));
    if (new_node == NULL){
        fprintf(stderr, "Interna chyba (99): nedostatok pamate pri vytvarani uzla AST\n");
        exit(99);
    }
    new_node->type = NODE_TOKEN_ONLY;
    new_node->token = token;
    new_node->identifier_entry = NULL;   // nastavi az semanticka analyza (generator testuje != NULL)

    new_node->right_ch = NULL;
    new_node->left_ch = NULL;

    return new_node;
}

// ast_create_node_type: vytvori vnutorny uzol daneho typu (NODE_IF, NODE_CALL, NODE_PROGRAM...) s prazdnym tokenom
//   pri chybe alokacie ukonci program s kodom 99
AST_NODE* ast_create_node_type(AST_NODE_TYPE type){

    AST_NODE *new_node = (AST_NODE*) malloc (sizeof(AST_NODE));
    if (new_node == NULL){
        fprintf(stderr, "Interna chyba (99): nedostatok pamate pri vytvarani uzla AST\n");
        exit(99);
    }
    new_node->type = type;
    Token empty_token = {0};
    new_node->token = empty_token;
    new_node->identifier_entry = NULL;   // nastavi az semanticka analyza (generator testuje != NULL)

    new_node->right_ch = NULL;
    new_node->left_ch = NULL;

    return new_node;
}

// ast_add_parent: pripoji uzol child k uzlu parent - side 0 = lavy potomok, side 1 = pravy potomok
//   (ak parent je NULL, nic sa nestane)
void ast_add_parent(AST_NODE *parent, AST_NODE *child, int side){
    if (parent == NULL) {
        return;
    }

    if (side == 0){
        parent->left_ch = child;        //vlozime do left_ch pointer na nas novy child

    if (side == 0)
    {
        parent->left_ch = child; // vlozime do left_ch pointer na nas novy child
    }
    else
    {
        parent->right_ch = child;
    }
}

// ast_free_node: rekurzivne uvolni uzol, oboch potomkov a retazec v tokene (identifikator / string)
void ast_free_node(AST_NODE *node) {
    // nie je co uvolnit
    if (node == NULL) {
        return;
    }

    ast_free_node(node->left_ch);
    ast_free_node(node->right_ch);

    // Free HTEntry
    if (node->identifier_entry != NULL)
    {
        free(node->identifier_entry->identifier);
        node->identifier_entry->identifier = NULL;
        if (node->identifier_entry->entry_type == FUNCTION)
        {
            free(node->identifier_entry->entry_data.function.parameters);
            node->identifier_entry->entry_data.function.parameters = NULL;
        }
    }
    free(node->identifier_entry);
    node->identifier_entry = NULL;

    if (node->type == NODE_TOKEN_ONLY)
    {
        token_free(&node->token);
    }

    free(node);
}
