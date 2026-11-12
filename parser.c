#include "parser.h"
#include "ast.h"
#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"
// pridanie funkcie peek a next na pracu s tokenom
//peek nacita token ale nspotrebuje ho
//next spotrebuje uz nacitany token alebo nacita novy

static Token saved_token;
static bool has_saved = false;

static Token peek(void){
    if(!has_saved){
        has_saved = true;
        saved_token = scanner_get_next_token();
    }
    return saved_token;
}

static Token next(void){
    if(has_saved){
        has_saved = false;
        return saved_token;
    }
    return scanner_get_next_token();
}

void parser_error_exit(int error_code, const char *message){
    fprintf(stderr, "Chyba (%d): %s\n",error_code,message);
    exit(error_code);
}

bool prolog(){
    Token current_token = next(); 
    if (current_token.type != KEYWORD_IMPORT) return false;
    
    current_token = next();       
    if (current_token.type != TOKEN_TYPE_IDENTIFIER) return false;
    
    current_token = next();       
    if (current_token.type != TOKEN_TYPE_DOT) return false;
    
    current_token = next();       
    if (current_token.type != TOKEN_TYPE_MUL) return false;
    
    current_token = next();
    if (current_token.type != TOKEN_TYPE_EOL) return false;

    return true;
}

AST_NODE* function_list(){
    AST_NODE* root = NULL;
    Token current_token = next();
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pointre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = root;   //ukazatel na posledny node z ktoreho vychadzame

    if(current_token.type == TOKEN_TYPE_AT_MAIN){
        root = ast_create_node_type(NODE_MAIN);
        current_token = next();
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
    Token current_token = next();
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    if(current_token.type == TOKEN_TYPE_IDENTIFIER){
        root = ast_create_node(current_token);
        last_program_node = root; 
    }else{
        parser_error_exit(2, "Syntakticka chyba,function_def1");
    }
    current_token = next();
    if(current_token.type != TOKEN_TYPE_LPAREN){
        parser_error_exit(2, "Syntakticka chyba,function_def2");
    }
    temp = parameter_list();
    ast_add_parent(last_program_node,temp,1);

    current_token = next();
    if(current_token.type == TOKEN_TYPE_COLON){
        current_token = peek();
        temp = get_type();
        ast_add_parent(last_program_node,temp,0);
        current_token = next();
    }
    if(current_token.type == TOKEN_TYPE_ASSIGN){
        return root;
    }else{
        parser_error_exit(2, "Syntakticka chyba,function_def3");
    }

    

    return root;

}

AST_NODE* parse_expression(){
    Token current_token = next();
    AST_NODE* root = ast_create_node(current_token);
    return root;
}
AST_NODE* var_assign(Token first, Token second){
    AST_NODE* root = NULL;
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
}
AST_NODE* call_f(Token first, Token second){
    return NULL;
}
AST_NODE* if_statement(){
    return NULL;
}
AST_NODE* while_loop(){
    return NULL;
}
AST_NODE* for_loop(){
    return NULL;
}


AST_NODE* var_def(){
    AST_NODE* root = NULL;
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    
    root = ast_create_node_type(NODE_VAR_DEF);
    Token current_token = next();
    if(current_token.type != TOKEN_TYPE_IDENTIFIER){parser_error_exit(2, "Syntakticka chyba,var_def");}
    temp = ast_create_node(current_token);
    ast_add_parent(root,temp,0);
    

    current_token = next();
    switch (current_token.type){
    case TOKEN_TYPE_ASSIGN:
        temp = parse_expression();
        ast_add_parent(root,temp,1);
        break;
    case TOKEN_TYPE_COLON:
        last_program_node = temp;
        current_token = peek();
        temp = get_type( );
        ast_add_parent(last_program_node,temp,0);

        current_token = next();
        if(current_token.type != TOKEN_TYPE_ASSIGN){parser_error_exit(2, "Syntakticka chyba,var_def");}
        temp = parse_expression();
        ast_add_parent(root,temp,1);
        break;
        break;
    default:
        parser_error_exit(2, "Syntakticka chyba,var_def");
        break;
    }
    
    return root;
}
AST_NODE* val_def(){
    AST_NODE* root = NULL;
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    
    root = ast_create_node_type(NODE_VAL_DEF);
    Token current_token = next();
    if(current_token.type != TOKEN_TYPE_IDENTIFIER){parser_error_exit(2, "Syntakticka chyba,var_def");}
    temp = ast_create_node(current_token);
    ast_add_parent(root,temp,0);
    

    current_token = next();
    switch (current_token.type){
    case TOKEN_TYPE_ASSIGN:
        temp = parse_expression();
        ast_add_parent(root,temp,1);
        break;
    case TOKEN_TYPE_COLON:
        last_program_node = temp;
        current_token = peek();
        temp = get_type();
        ast_add_parent(last_program_node,temp,0);

        current_token = next();
        if(current_token.type != TOKEN_TYPE_ASSIGN){parser_error_exit(2, "Syntakticka chyba,var_def");}
        temp = parse_expression();
        ast_add_parent(root,temp,1);
        break;
        break;
    default:
        parser_error_exit(2, "Syntakticka chyba,var_def");
        break;
    }
    
    return root;
}

AST_NODE* function_body(){
    AST_NODE* root = NULL;
    Token current_token = peek();
    if(current_token.type == TOKEN_TYPE_EOL){
        next();
        root = function_body_block();
    }else{
        root = parse_expression();
    }

    return root;
}
AST_NODE* function_body_block(){
    //vytvorime prvy node program bloku
    AST_NODE* root = ast_create_node_type(NODE_PROGRAM);
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    Token current_token = next();
    if(current_token.type != TOKEN_TYPE_INDENT){parser_error_exit(2, "Syntakticka chyba, function_body_block1");}
    
    //pre pripad prazdneho bloku. Blok nemoze byt prazdny.
    current_token = peek();
    if(current_token.type == TOKEN_TYPE_DEDENT){parser_error_exit(2, "Syntakticka chyba, function_body_block2");}
    last_program_node = root;
    
    //Loop ktory kontroluje lubovolny pocet statementov
    while (current_token.type != TOKEN_TYPE_DEDENT){
        temp = statement();
        ast_add_parent(last_program_node,temp,0);
        current_token = peek();
        if(current_token.type == TOKEN_TYPE_EOF){
            next();
            parser_error_exit(2, "Chyba lexera. Koniec programu EOL ale neuzavrety blok kodu.");
        }
        
        //vytvorenie noveho uzla, nappojenie na predchadzajuci programovy uzol a nastavenie uzla ako posledny
        if(current_token.type != TOKEN_TYPE_DEDENT){
            temp = ast_create_node_type(NODE_PROGRAM);
            ast_add_parent(last_program_node,temp,1);
            last_program_node = temp;
        }else{
            next();
        }
        
    }
    
    
    return root;
}

AST_NODE* statement(){
    Token current_token = next();
    AST_NODE* root = NULL;
    switch (current_token.type){
        case KEYWORD_IF:
            //podmienka IF
            break;
        case KEYWORD_WHILE:
            //cyklus while
            break;
        case KEYWORD_VAR:
            root = var_def();
            break;
        case KEYWORD_VAL:
            root = val_def();
            break;
        case TOKEN_TYPE_IDENTIFIER:
            Token current_token_second = next();
            switch (current_token_second.type){
                case TOKEN_TYPE_ASSIGN:
                    root = var_assign(current_token, current_token_second);
                    break;
                case TOKEN_TYPE_LPAREN:
                    root = call_f(current_token, current_token_second);
                    break;
                default:
                    parser_error_exit(2, "Syntakticka chyba v statement: neočakávaný token za identifikátorom");
                    break;
            }

            //var_assign + call_f
            break;
        case KEYWORD_FOR:
            //For loop
            break;
        
        case KEYWORD_RETURN:
            //return
            break;
        case TOKEN_TYPE_EOL:
            //medzera treba nacitat  novy 
            break;
        
        default:
            //expression
            break;
        }    
    current_token = next();
    if(current_token.type != TOKEN_TYPE_EOL){parser_error_exit(2, "Syntakticka chyba, statementx");}
    return root;
}

AST_NODE* parameter_list(){
    
    AST_NODE* root = NULL;
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    Token current_token = next();

    if(current_token.type == TOKEN_TYPE_IDENTIFIER){
        root = ast_create_node(current_token);
        last_program_node = root;

        current_token = next();
        if(current_token.type != TOKEN_TYPE_COLON){parser_error_exit(2, "Syntakticka chyba, parameter_list2");}
        
        current_token = peek();
        temp = get_type();
        ast_add_parent(last_program_node,temp,0);
        
        current_token = next();
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
AST_NODE* get_type( ){
    AST_NODE* temp = NULL;
    switch(peek().type){
        case KEYWORD_STRING:
            temp = ast_create_node (next());
            break;
        case KEYWORD_DOUBLE:
            temp = ast_create_node (next());
            break;
        case KEYWORD_INT:
            temp = ast_create_node (next());
            break;
        case KEYWORD_UNIT:
            temp = ast_create_node (next());
            break;
        case KEYWORD_BOOLEAN:
            temp = ast_create_node (next());
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
    Token current_token = next();
    if(current_token.type !=TOKEN_TYPE_LBRACKET){parser_error_exit(2,"Syntakticka chyba,parser_get_list");}

    current_token = peek();
    AST_NODE* temp= get_type();
    ast_add_parent(list_node,temp,0);

    current_token = next();
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
    Token current_token = peek();  //volanie lexera
    while (current_token.type != TOKEN_TYPE_EOF){            //kontrola ci po prologu nasleduju definicie funkcii. Kazda funkcia ma v sebe zvysok pravidiel
        
        switch(current_token.type){
            case TOKEN_TYPE_AT_MAIN:
                temp = function_list();
                ast_add_parent(last_program_node, temp, 0);
                break;
            case KEYWORD_DEF:
                temp = function_list();
                ast_add_parent(last_program_node, temp, 0);   
                break; 
            case TOKEN_TYPE_EOL:
                next();
                current_token = peek();
                continue;
            default:
                parser_error_exit(2,"Syntakticka chyba,program");
        }

        current_token = peek();
        if(current_token.type != TOKEN_TYPE_EOF){
            AST_NODE* program_node = ast_create_node_type(NODE_PROGRAM);
            ast_add_parent(last_program_node, program_node, 1);
            last_program_node = program_node;
        }
        
    }
    return root;
}


