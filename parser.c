#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"

void print_ast(AST_NODE *node, int depth) {
    if (node == NULL) return;

    // Odsadenie pre lepšiu viditeľnosť úrovní stromu
    for (int i = 0; i < depth; i++) printf("  ");

    printf("Node type: %d\n", node->type);

    // Rekurzívne vypíšeme ľavú a pravú vetvu
    if (node->left_ch || node->right_ch) {
        for (int i = 0; i < depth; i++) printf("  ");
        printf("--- LEFT ---\n");
        print_ast(node->left_ch, depth + 1);

        for (int i = 0; i < depth; i++) printf("  ");
        printf("--- RIGHT ---\n");
        print_ast(node->right_ch, depth + 1);
    }
}
void parser_error_exit(int error_code, const char *message){
    fprintf(stderr, "Chyba (%d): %s\n",error_code,message);
    exit(error_code);
}

bool prolog(){
    Token current_token = scanner_get_next_token(); 
    if (current_token.type != KEYWORD_IMPORT) return false;
    
    current_token = scanner_get_next_token();       
    if (current_token.type != TOKEN_TYPE_IDENTIFIER) return false;
    
    current_token = scanner_get_next_token();       
    if (current_token.type != TOKEN_TYPE_DOT) return false;
    
    current_token = scanner_get_next_token();       
    if (current_token.type != TOKEN_TYPE_MUL) return false;
    
    current_token = scanner_get_next_token();
    if (current_token.type != TOKEN_TYPE_EOL) return false;

    return true;
}

AST_NODE* function_list(Token token){
    AST_NODE* root = NULL;
    Token current_token = {0};
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = root;   //ukazatel na posledny node z ktoreho vychadzame

    if(token.type == TOKEN_TYPE_AT_MAIN){
        root = ast_create_node_type(NODE_MAIN);
        current_token = scanner_get_next_token();
        if(token.type == KEYWORD_DEF){
            temp = ast_create_node_type(NODE_FUNC_DEF);
            ast_add_parent(root, temp, 0);
            last_program_node = temp;
           
        }else{
            parser_error_exit(2, "Syntakticka chyba v definicii funkcie");
        }

    }else if (token.type == KEYWORD_DEF){
        root = ast_create_node_type(NODE_FUNC_DEF); 
    }else{
        parser_error_exit(2, "Syntakticka chyba v definicii funkcie");
    }
    
    temp = function_def();
    ast_add_parent(last_program_node, temp, 0);     //pridavanie definicie funkcie do laveho uzla 
    temp = function_body();
    ast_add_parent(last_program_node, temp, 1);     //pridavanie tela funkcie do raveho uzla 
    return root;

}

AST_NODE* function_def(){
    Token current_token = scanner_get_next_token();
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    if(current_token.type == TOKEN_TYPE_IDENTIFIER){
        last_program_node = ast_create_node(current_token);
    }else{
        parser_error_exit(2, "Syntakticka chyba");
    }
    current_token = scanner_get_next_token();
    if(current_token.type != TOKEN_TYPE_LPAREN){
        parser_error_exit(2, "Syntakticka chyba");
    }
    temp = parameter_list();
    ast_add_parent(last_program_node,temp,1);
    current_token = scanner_get_next_token();
    if(current_token.type != TOKEN_TYPE_RPAREN){
        parser_error_exit(2, "Syntakticka chyba");
    }
}
AST_NODE* function_body(){

}



AST_NODE* program(){
    AST_NODE* root = ast_create_node_type(NODE_PROGRAM);
    if (prolog() == false){
        parser_error_exit(2,"Syntakticka chyba v prologu.");
    }

    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = root;
    Token current_token = scanner_get_next_token();  //volanie lexera
    while (current_token.type != TOKEN_TYPE_EOF){            //kontrola ci po prologu nasleduju definicie funkcii. Kazda funkcia ma v sebe zvysok pravidiel
        
        switch(current_token.type){
            case TOKEN_TYPE_AT_MAIN:
                temp = function_list(current_token);
                ast_add_parent(last_program_node, temp, 0);
                break;
            case KEYWORD_DEF:
                temp = function_list(current_token);
                ast_add_parent(last_program_node, temp, 0);   
                break; 
            case TOKEN_TYPE_EOL:
                current_token = scanner_get_next_token();
                continue;
            default:
                parser_error_exit(2,"Syntakticka chyba");
        }

        current_token = scanner_get_next_token();
        if(current_token.type != TOKEN_TYPE_EOF){
            AST_NODE* program_node = ast_create_node_type(NODE_PROGRAM);
            ast_add_parent(last_program_node, program_node, 1);
            last_program_node = program_node;
        }
        
    }
    print_ast(root, 0);
    return root;
}


