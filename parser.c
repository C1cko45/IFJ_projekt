#include "parser.h"
#include "ast.h"
#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"


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
    Token current_token = token;
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pointre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = root;   //ukazatel na posledny node z ktoreho vychadzame

    if(current_token.type == TOKEN_TYPE_AT_MAIN){
        root = ast_create_node_type(NODE_MAIN);
        current_token = scanner_get_next_token();
        if(current_token.type == KEYWORD_DEF){
            temp = ast_create_node_type(NODE_FUNC_DEF);
            ast_add_parent(root, temp, 0);
            last_program_node = temp;
           
        }else{
            parser_error_exit(2, "Syntakticka chyba,function_list");
        }

    }else if (current_token.type == KEYWORD_DEF){
        root = ast_create_node_type(NODE_FUNC_DEF); 
        last_program_node = root;
    }else{
        parser_error_exit(2, "Syntakticka chyba,function_list");
    }
    
    temp = function_def();
    ast_add_parent(last_program_node, temp, 0);     //pridavanie definicie funkcie do laveho uzla 


    temp = function_body();
    ast_add_parent(last_program_node, temp, 1);     //pridavanie tela funkcie do raveho uzla 
    return root;

}

AST_NODE* function_def(){
    AST_NODE* root = NULL;
    Token current_token = scanner_get_next_token();
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    if(current_token.type == TOKEN_TYPE_IDENTIFIER){
        root = ast_create_node(current_token);
        last_program_node = root; 
    }else{
        parser_error_exit(2, "Syntakticka chyba,function_def1");
    }
    current_token = scanner_get_next_token();
    if(current_token.type != TOKEN_TYPE_LPAREN){
        parser_error_exit(2, "Syntakticka chyba,function_def2");
    }
    temp = parameter_list();
    ast_add_parent(last_program_node,temp,1);

    current_token = scanner_get_next_token();
    if(current_token.type == TOKEN_TYPE_COLON){
        current_token = scanner_get_next_token();
        temp = get_type(current_token);
        ast_add_parent(last_program_node,temp,0);
        current_token = scanner_get_next_token();
    }
    if(current_token.type == TOKEN_TYPE_ASSIGN){
        return root;
    }else{
        parser_error_exit(2, "Syntakticka chyba,function_def3");
    }

    

    return root;

}

AST_NODE* function_body(){
    return NULL;
}
AST_NODE* parameter_list(){
    
    AST_NODE* root = NULL;
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    Token current_token = scanner_get_next_token();

    if(current_token.type == TOKEN_TYPE_IDENTIFIER){
        root = ast_create_node(current_token);
        last_program_node = root;

        current_token = scanner_get_next_token();
        if(current_token.type != TOKEN_TYPE_COLON){parser_error_exit(2, "Syntakticka chyba, parameter_list2");}
        
        current_token = scanner_get_next_token();
        temp = get_type(current_token);
        ast_add_parent(last_program_node,temp,0);
        
        current_token = scanner_get_next_token();
        if(current_token.type == TOKEN_TYPE_COMMA){
            temp = parameter_list();
            ast_add_parent(last_program_node,temp,1);
            
            return last_program_node;
        }else if(current_token.type == TOKEN_TYPE_RPAREN){
            return root;
        }else{parser_error_exit(2, "Syntakticka chyba, parameter_list3");}
            return NULL;
    }else if(current_token.type == TOKEN_TYPE_RPAREN){
        return NULL;
    }else{
        parser_error_exit(2, "Syntakticka chyba, parameter_list1");
        return NULL;
    }
}
AST_NODE* get_type(Token token){
    AST_NODE* temp = NULL;
    switch(token.type){
        case KEYWORD_STRING:
            temp = ast_create_node (token);
            break;
        case KEYWORD_DOUBLE:
            temp = ast_create_node (token);
            break;
        case KEYWORD_INT:
            temp = ast_create_node (token);
            break;
        case KEYWORD_UNIT:
            temp = ast_create_node (token);
            break;
        case KEYWORD_BOOLEAN:
            temp = ast_create_node (token);
            break;
        case KEYWORD_LIST:
            temp = parser_get_list();
            break;
        default:
            parser_error_exit(2,"Syntakticka chyba,get_type");
    }
    return temp;
}

AST_NODE* parser_get_list(){
    AST_NODE* list_node = ast_create_node_type(NODE_LIST);
    Token current_token = scanner_get_next_token();
    if(current_token.type !=TOKEN_TYPE_LBRACKET){parser_error_exit(2,"Syntakticka chyba,parser_get_list");}

    current_token = scanner_get_next_token();
    AST_NODE* temp= get_type(current_token);
    ast_add_parent(list_node,temp,0);

    current_token = scanner_get_next_token();
    if(current_token.type != TOKEN_TYPE_RBRACKET){
        parser_error_exit(2, "Syntakticka chyba, parser_get_list: ocakavana ']'");
    }
    
    return list_node;
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
                parser_error_exit(2,"Syntakticka chyba,program");
        }

        current_token = scanner_get_next_token();
        if(current_token.type != TOKEN_TYPE_EOF){
            AST_NODE* program_node = ast_create_node_type(NODE_PROGRAM);
            ast_add_parent(last_program_node, program_node, 1);
            last_program_node = program_node;
        }
        
    }
    return root;
}


