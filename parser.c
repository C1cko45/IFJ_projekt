#include "parser.h"
#include "ast.h"
#include "token.h"
#include "psa.h"
#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"

/* ============================================================
   1. Praca s tokenmi (peek / next) a chybove hlasenie
   ============================================================ */

// pridanie funkcie peek a next na pracu s tokenom
//peek nacita token ale nspotrebuje ho
//next spotrebuje uz nacitany token alebo nacita novy

static Token saved_token;
static bool has_saved = false;
static AST_NODE *ast_root = NULL;   // koren AST, aby ho parser_error_exit vedel uvolnit

// peek: nacita dalsi token zo scannera, ale nespotrebuje ho (dalsi peek/next vrati ten isty)
//! Volana z: get_type(), parser_get_list(), parameter_list(), var_def(), val_def(), call_f(), if_statement(),
//!     statement(), function_body_block(), function_body(), function_def(), program(), psa.c: parse_expression
Token peek(void){
    if(!has_saved){
        has_saved = true;
        saved_token = scanner_get_next_token();
        if(saved_token.type == TOKEN_ERROR_LEX){parser_error_exit(1, "Lexikalna chyba: neplatny lexem (neznamy znak, chybny literal, neukonceny retazec/komentar alebo tabulator v odsadeni)");}
    }
    return saved_token;
}

// next: spotrebuje a vrati token (ulozeny z peek, inak novy zo scannera)
//! Volana z: get_type(), parser_get_list(), parameter_list(), var_def(), val_def(), call_f(), if_statement(),
//!     while_loop(), statement(), function_body_block(), function_body(), function_def(), function_list(), prolog(),
//!     program(), psa.c: parse_expression
Token next(void){
    Token t;
    if(has_saved){
        has_saved = false;
        return saved_token;
    }else{
        t = scanner_get_next_token();
        if(t.type == TOKEN_ERROR_LEX){parser_error_exit(1, "Lexikalna chyba: neplatny lexem (neznamy znak, chybny literal, neukonceny retazec/komentar alebo tabulator v odsadeni)");}
    }
    
    return t;
}

// parser_error_exit: vypise chybu na stderr a ukonci program s danym navratovym kodom
//! Volana z: get_type(), parser_get_list(), parameter_list(), var_def(), val_def(), call_f(), if_statement(),
//!     while_loop(), statement(), function_body_block(), function_def(), function_list(), program(), psa.c: viac
//!     funkcii
void parser_error_exit(int error_code, const char *message){
    fprintf(stderr, "Chyba (%d): %s\n",error_code,message);
    if(has_saved){ token_free(&saved_token); has_saved = false; }   // nespotrebovany token
    ast_free_node(ast_root);                                         // uvolni cast stromu, ktora uz je postavena
    ast_root = NULL;
    exit(error_code);
}

/* ============================================================
   2. Typy a parametre (pomocne funkcie nizkej urovne)
   ============================================================ */

// get_type: spracuje typ (String, Double, Int, Unit, Boolean, List[...]) a vrati jeho uzol
//! Volana z: parser_get_list(), parameter_list(), var_def(), val_def(), function_def()
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
            parser_error_exit(2,"Syntakticka chyba: ocakavany typ (Int, Double, String, Unit, Boolean alebo List[...])");
    }
    return temp;
}

// parser_get_list: spracuje zapis List[typ] a vrati uzol NODE_LIST (typ prvku v lavom potomkovi)
//! Volana z: get_type()
AST_NODE* parser_get_list(){
    AST_NODE* list_node = ast_create_node_type(NODE_LIST);
    Token current_token = next();                 // spotrebuje klucove slovo List
    if(current_token.type != KEYWORD_LIST){parser_error_exit(2,"Syntakticka chyba v type List: ocakavane klucove slovo List");}
    current_token = next();
    if(current_token.type !=TOKEN_TYPE_LBRACKET){parser_error_exit(2,"Syntakticka chyba v type List: za List ocakavana '['");}

    current_token = peek();
    AST_NODE* temp= get_type();
    ast_add_parent(list_node,temp,0);

    current_token = next();
    if(current_token.type != TOKEN_TYPE_RBRACKET){
        parser_error_exit(2, "Syntakticka chyba v type List: ocakavana ']'");
    }
    
    return list_node;
}

// parameter_list: rekurzivne spracuje zoznam parametrov "meno: typ, ..." az po pravu zatvorku
//! Volana z: sama seba (rekurzia), function_def()
AST_NODE* parameter_list(){
    
    AST_NODE* root = NULL;
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    Token current_token = next();

    if(current_token.type == TOKEN_TYPE_IDENTIFIER){
        root = ast_create_node(current_token);
        last_program_node = root;

        current_token = next();
        if(current_token.type != TOKEN_TYPE_COLON){parser_error_exit(2, "Syntakticka chyba v parametroch funkcie: za nazvom parametra ocakavana ':'");}
        
        current_token = peek();
        temp = get_type();
        ast_add_parent(last_program_node,temp,0);
        
        current_token = next();
        if(current_token.type == TOKEN_TYPE_COMMA){
             
            if(peek().type == TOKEN_TYPE_RPAREN && !scanner_newline_before_token()){parser_error_exit(2, "Syntakticka chyba v parametroch funkcie: ciarka pred ')'");}
            temp = parameter_list();
            ast_add_parent(last_program_node,temp,1);
            
            return last_program_node;
        }else if(current_token.type == TOKEN_TYPE_RPAREN){
            return root;
        }else{parser_error_exit(2, "Syntakticka chyba v parametroch funkcie: za typom parametra ocakavana ',' alebo ')'");}
            return NULL;
    }else if(current_token.type == TOKEN_TYPE_RPAREN){
        return NULL;
    }else{
        parser_error_exit(2, "Syntakticka chyba v parametroch funkcie: ocakavany nazov parametra alebo ')'");
        return NULL;
    }
}

/* ============================================================
   3. Definicie premennych a priradenie
   ============================================================ */


//! Volana z: var_def(), val_def(), var_assign(), statement() (return)
static AST_NODE* value_or_if(void){
    if(peek().type == KEYWORD_IF){
        next();
        return if_statement();
    }
    return parse_expression();
}

//  ends_with_if: true, ak prikaz konci if-vyrazom - jeho posledna vetva uz spotrebovala koniec riadku
//! Volana z: statement()
static bool ends_with_if(AST_NODE* node){
    if(node == NULL){ return false; }
    if(node->type == NODE_VAR_DEF || node->type == NODE_VAL_DEF ||
       (node->type == NODE_TOKEN_ONLY && node->token.type == TOKEN_TYPE_ASSIGN)){
        return node->right_ch != NULL && node->right_ch->type == NODE_IF;
    }
    if(node->type == NODE_TOKEN_ONLY && node->token.type == KEYWORD_RETURN){
        return node->left_ch != NULL && node->left_ch->type == NODE_IF;
    }
    return false;
}

// var_def: spracuje definiciu premennej "var meno [: typ] = vyraz" (vyraz cez precedencnu analyzu)
//! Volana z: statement()
AST_NODE* var_def(){
    AST_NODE* root = NULL;
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    
    root = ast_create_node_type(NODE_VAR_DEF);
    Token current_token = next();
    if(current_token.type != TOKEN_TYPE_IDENTIFIER){parser_error_exit(2, "Syntakticka chyba v definicii var: ocakavany nazov premennej");}
    temp = ast_create_node(current_token);
    ast_add_parent(root,temp,0);
    

    current_token = next();
    switch (current_token.type){
    case TOKEN_TYPE_ASSIGN:
        temp = value_or_if();        
        ast_add_parent(root,temp,1);
        break;
    case TOKEN_TYPE_COLON:
        last_program_node = temp;
        current_token = peek();
        temp = get_type( );
        ast_add_parent(last_program_node,temp,0);

        current_token = next();
        if(current_token.type != TOKEN_TYPE_ASSIGN){parser_error_exit(2, "Syntakticka chyba v definicii var: za typom ocakavane '='");}
        temp = value_or_if();        
        ast_add_parent(root,temp,1);
        break;
        
    default:
        parser_error_exit(2, "Syntakticka chyba v definicii var: za nazvom premennej ocakavane ':' alebo '='");
        break;
    }
    
    return root;
}


 
// val_def: spracuje definiciu konstanty "val meno [: typ] = vyraz" (rovnaka struktura ako var_def)
//! Volana z: statement()
AST_NODE* val_def(){
    AST_NODE* root = NULL;
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    
    root = ast_create_node_type(NODE_VAL_DEF);
    Token current_token = next();
    if(current_token.type != TOKEN_TYPE_IDENTIFIER){parser_error_exit(2, "Syntakticka chyba v definicii val: ocakavany nazov premennej");}
    temp = ast_create_node(current_token);
    ast_add_parent(root,temp,0);
    

    current_token = next();
    switch (current_token.type){
    case TOKEN_TYPE_ASSIGN:
        temp = value_or_if();        
        ast_add_parent(root,temp,1);
        break;
    case TOKEN_TYPE_COLON:
        last_program_node = temp;
        current_token = peek();
        temp = get_type();
        ast_add_parent(last_program_node,temp,0);

        current_token = next();
        if(current_token.type != TOKEN_TYPE_ASSIGN){parser_error_exit(2, "Syntakticka chyba v definicii val: za typom ocakavane '='");}
        temp = value_or_if();        
        ast_add_parent(root,temp,1);
        break;
    default:
        parser_error_exit(2, "Syntakticka chyba v definicii val: za nazvom premennej ocakavane ':' alebo '='");
        break;
    }
    
    return root;
}

// var_assign: priradenie "meno = vyraz"
//! Volana z: statement()
AST_NODE* var_assign(Token first, Token second){
    AST_NODE* root = ast_create_node(second);
    AST_NODE* temp = ast_create_node(first);
    ast_add_parent(root, temp, 0);
    temp = value_or_if();        
    ast_add_parent(root, temp, 1);
    return root;
}

// call_f: volanie funkcie "meno(...)" ako samostatny prikaz (meno aj '(' su uz spotrebovane)
//! Volana z: statement()
AST_NODE* call_f(Token first, Token second){
    if((first.type == TOKEN_TYPE_IDENTIFIER || first.type == KEYWORD_LIST) && second.type == TOKEN_TYPE_LPAREN){
        return call_with_callee(ast_create_node(first));
    }
    return NULL;
}


//   volany objekt moze byt identifikator, List alebo NODE_VALMEM (metoda zoznamu, List.range)
//! Volana z: call_f(), psa.c: apply_call (volanie vo vnutri vyrazu)
AST_NODE* call_with_callee(AST_NODE* callee){
    {
        AST_NODE* root = ast_create_node_type(NODE_CALL);
        ast_add_parent(root, callee, 0);  //pridanie volaneho objektu
        AST_NODE* argument;
        AST_NODE* temp = ast_create_node_type(NODE_ARGUMENT);
        ast_add_parent(root, temp, 1);  //pridanie vetve argumenty
        AST_NODE* temp2;
        while(1){
            switch(peek().type){
                case TOKEN_TYPE_INT:
                case TOKEN_TYPE_DOUBLE:
                case TOKEN_TYPE_STRING:
                case TOKEN_TYPE_UNIT:
                case TOKEN_TYPE_IDENTIFIER:
                case TOKEN_TYPE_BOOL:
                case TOKEN_TYPE_LPAREN:
                case TOKEN_TYPE_MINUS:
                case TOKEN_TYPE_PLUS:
                case TOKEN_TYPE_NOT:
                case KEYWORD_LIST:
                    argument = parse_expression();
                    ast_add_parent(temp, argument, 0 );
                    

                    break;
                case TOKEN_TYPE_RPAREN:
                    next();
                    return root;
                    break;
                default:
                    parser_error_exit(2,"Syntakticka chyba vo volani funkcie: neplatny argument" );
                    break;

            }
            if(peek().type == TOKEN_TYPE_RPAREN){
                next();
                return root;
            }else if(peek().type == TOKEN_TYPE_COMMA){
                next();
                if(peek().type != TOKEN_TYPE_RPAREN){
                    temp2 = ast_create_node_type(NODE_ARGUMENT);
                    ast_add_parent(temp, temp2, 1 );
                    temp = temp2;
                    continue;
                }else if(scanner_newline_before_token()){   
                    next();
                    return root;
                }else{
                    parser_error_exit(2,"Syntakticka chyba vo volani funkcie: ciarka pred ')'" );
                }

            }else{
                parser_error_exit(2,"Syntakticka chyba vo volani funkcie: za argumentom ocakavana ',' alebo ')'" );
            }

        }
    }
}

/* ============================================================
   4. Riadiace struktury
   ============================================================ */

// if_statement: podmienka if / else
//! Volana z: sama seba (rekurzia), statement()
AST_NODE* if_statement(){
    AST_NODE* root = ast_create_node_type(NODE_IF);
    AST_NODE* podmienka = ast_create_node_type(NODE_PODMIENKA);
    AST_NODE* root_temp = root;
    AST_NODE* temp;
    ast_add_parent(root, podmienka, 0);
    AST_NODE* vyraz = parse_expression();
    ast_add_parent(podmienka, vyraz,0);
    if(next().type != KEYWORD_THEN){parser_error_exit(2,"Syntakticka chyba v if: za podmienkou ocakavane 'then'" );}
    if(peek().type == TOKEN_TYPE_EOL){ 
        temp = function_body();
        ast_add_parent(podmienka,temp,1);
        if(peek().type == KEYWORD_ELSE){
            AST_NODE* els;
            next();
            
            switch(peek().type){
                case TOKEN_TYPE_EOL:
                    els =  ast_create_node_type(NODE_ELSE);
                    temp = function_body();
                    ast_add_parent(els,temp,0);
                    ast_add_parent(root_temp,els,1);
                break;
                case KEYWORD_IF: 
                    next();
                    temp = if_statement();
                    ast_add_parent(root_temp,temp,1);
                break;
                default:
                    els = ast_create_node_type(NODE_ELSE);
                    temp = function_body();      
                    ast_add_parent(els,temp,0);
                    ast_add_parent(root_temp,els,1);
                break;
                
            }
            
            
        }
    }else{
        temp = function_body();          
        ast_add_parent(podmienka,temp,1);
        if(peek().type == KEYWORD_ELSE){
            AST_NODE* els;
            next();
            
            switch(peek().type){
                case TOKEN_TYPE_EOL:
                    els =  ast_create_node_type(NODE_ELSE);
                    temp = function_body();
                    ast_add_parent(els,temp,0);
                    ast_add_parent(root,els,1);
                break;
                case KEYWORD_IF:
                    next();
                    temp = if_statement();
                    ast_add_parent(root,temp,1);
                break;
                default:
                    
                    els = ast_create_node_type(NODE_ELSE);
                    temp = function_body();
                    ast_add_parent(els,temp,0);
                    ast_add_parent(root,els,1);
                break;
                
            }
            
            
        }
    }
    
    return root;
}

// while_loop: cyklus while
//! Volana z: statement()
AST_NODE* while_loop(){
    AST_NODE* root = ast_create_node_type(NODE_WHILE);
    AST_NODE* temp = parse_expression();
    ast_add_parent(root, temp, 0);
    if(next().type != KEYWORD_DO){parser_error_exit(2,"Syntakticka chyba vo while: za podmienkou ocakavane 'do'" );}
    temp = function_body();
    ast_add_parent(root, temp,1);

    return root;
}

// for_loop: cyklus for
//! Volana z: statement()
AST_NODE* for_loop(){
    AST_NODE* root = ast_create_node_type(NODE_FOR);
    AST_NODE* temp;
    AST_NODE* limit_typ;
    AST_NODE* limit;
    AST_NODE* nazov_p;
    AST_NODE* last_temp;
    switch(peek().type){
        case TOKEN_TYPE_EOL:
            next();
            if(next().type != TOKEN_TYPE_INDENT){parser_error_exit(2,"Syntakticka chyba vo for: viacriadkova hlavicka musi byt odsadena" );}
            if(peek().type != TOKEN_TYPE_IDENTIFIER){parser_error_exit(2,"Syntakticka chyba vo for: ocakavany nazov riadiacej premennej" );}
            //Vytvorenie a pridanie uzla s nazvom premmennej k root
            nazov_p = ast_create_node(next());
            if(next().type != TOKEN_TYPE_ARROW){parser_error_exit(2,"Syntakticka chyba vo for: za premennou ocakavane '<-'");}
            ast_add_parent(root,nazov_p,0); 
            limit = ast_create_node_type(NODE_LIMIT);
            ast_add_parent(nazov_p,limit,0 );
            
            temp = parse_expression();
            switch(peek().type){
                case KEYWORD_TO:
                case KEYWORD_UNTIL:
                    limit_typ = ast_create_node(next());
                    ast_add_parent(limit,limit_typ,0);
                    ast_add_parent(limit_typ, temp,0);
                    temp = parse_expression();
                    ast_add_parent(limit_typ, temp,1);
                    if(peek().type == KEYWORD_BY){
                        next();
                        temp = parse_expression();
                        ast_add_parent(limit, temp,1);
                    }
                    break;
                default:
                    ast_add_parent(limit, temp, 0);
                    break;
            }
            
            if(next().type != TOKEN_TYPE_EOL){parser_error_exit(2,"Syntakticka chyba vo for: za generatorom ocakavany koniec riadku" );}
            last_temp = nazov_p;
            while(peek().type == KEYWORD_IF){
                next();
                temp = ast_create_node_type(NODE_PODMIENKA);  
                ast_add_parent(temp, parse_expression(),0);
                ast_add_parent(last_temp, temp,1);
                last_temp = temp;
                if(next().type != TOKEN_TYPE_EOL){parser_error_exit(2,"Syntakticka chyba vo for: za strazou (if) ocakavany koniec riadku");}
            }
            if(next().type != TOKEN_TYPE_DEDENT){parser_error_exit(2,"Syntakticka chyba vo for: 'do' musi byt zarovnane s 'for'" );}
            if(next().type != KEYWORD_DO){parser_error_exit(2,"Syntakticka chyba vo for: ocakavane 'do'" );}
            break;





        case TOKEN_TYPE_IDENTIFIER:
            if(peek().type != TOKEN_TYPE_IDENTIFIER){parser_error_exit(2,"Syntakticka chyba vo for: ocakavany nazov riadiacej premennej" );}
            //Vytvorenie a pridanie uzla s nazvom premmennej k root
            nazov_p = ast_create_node(next());
            if(next().type != TOKEN_TYPE_ARROW){parser_error_exit(2,"Syntakticka chyba vo for: za premennou ocakavane '<-'");}
            ast_add_parent(root,nazov_p,0); 
            limit = ast_create_node_type(NODE_LIMIT);
            ast_add_parent(nazov_p,limit,0 );
            
            temp = parse_expression();
            switch(peek().type){
                case KEYWORD_TO:
                case KEYWORD_UNTIL:
                    limit_typ = ast_create_node(next());
                    ast_add_parent(limit,limit_typ,0);
                    ast_add_parent(limit_typ, temp,0);
                    temp = parse_expression();
                    ast_add_parent(limit_typ, temp,1);
                    if(peek().type == KEYWORD_BY){
                        next();
                        temp = parse_expression();
                        ast_add_parent(limit, temp,1);
                    }
                    break;
                default:
                    ast_add_parent(limit, temp, 0);
                    break;
            }
            
            last_temp = nazov_p;
            while(peek().type == KEYWORD_IF){
                next();
                temp = ast_create_node_type(NODE_PODMIENKA);
                ast_add_parent(temp, parse_expression(),0);
                ast_add_parent(last_temp, temp,1);
                last_temp = temp;
            }
            if(next().type != KEYWORD_DO){parser_error_exit(2,"Syntakticka chyba vo for: ocakavane 'do'" );}
            break;
        default:
            parser_error_exit(2,"Syntakticka chyba vo for: za 'for' ocakavana premenna alebo koniec riadku" );
            break;
    }
    ast_add_parent(root,function_body(),1); 
        
    return root;
}

/* ============================================================
   5. Prikazy a bloky kodu
   ============================================================ */

// statement: spracuje jeden prikaz podla prveho tokenu a skonci kontrolou ukoncenia EOL
//! Volana z: function_body_block()
AST_NODE* statement(){
    Token current_token = peek();
    AST_NODE* root = NULL;
    switch (current_token.type){
        case KEYWORD_IF:
            next(); 
            root = if_statement();
            break;
        case KEYWORD_WHILE:
            next(); 
            root = while_loop();
            break;
        case KEYWORD_VAR:
            next(); 
            root = var_def();
            break;
        case KEYWORD_VAL:
            next(); 
            root = val_def();
            break;
        case TOKEN_TYPE_IDENTIFIER: {
            next();                                      // spotrebuje identifikator
            Token current_token_second = peek();         // len nahliadne na dalsi token
            switch (current_token_second.type){
                case TOKEN_TYPE_ASSIGN:
                    next(); 
                    root = var_assign(current_token, current_token_second);
                    break;
                case TOKEN_TYPE_LPAREN:
                    next(); 
                    root = call_f(current_token, current_token_second);
                    break;
                default:
                //pripad kedy je na riadku iba b+1 
                    root = parse_expression_from(current_token);
                    break;
            }

            //var_assign + call_f
            break;
        }
        case KEYWORD_FOR:
            next();
            root = for_loop();
            break;
        
        case KEYWORD_RETURN:
            root = ast_create_node(next());
            
            if(peek().type != TOKEN_TYPE_EOL && peek().type != TOKEN_TYPE_SEMICOLON && peek().type != KEYWORD_ELSE){
                AST_NODE* temp = value_or_if();      
                ast_add_parent(root,temp,0);
            }
            
            break;
        case TOKEN_TYPE_EOL: 

            break;
        
        default:
            root = parse_expression();
            break;
        }    
    if(current_token.type == KEYWORD_WHILE || current_token.type == KEYWORD_IF || current_token.type == KEYWORD_FOR ||
        ends_with_if(root)){   

    }else if(peek().type != KEYWORD_ELSE){   
        current_token = next();
        if(current_token.type != TOKEN_TYPE_EOL && current_token.type != TOKEN_TYPE_SEMICOLON){
            parser_error_exit(2, "Syntakticka chyba: za prikazom ocakavany koniec riadku alebo ';'");
        }
    }
    
    return root;
}

// function_body_block: blok prikazov medzi INDENT a DEDENT, prikazy su zretazene cez NODE_PROGRAM
//! Volana z: function_body()
AST_NODE* function_body_block(){
    //vytvorime prvy node program bloku
    AST_NODE* root = ast_create_node_type(NODE_PROGRAM);
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    Token current_token = next();
    if(current_token.type != TOKEN_TYPE_INDENT){parser_error_exit(2, "Syntakticka chyba: ocakavany odsadeny blok");}
    
    //pre pripad prazdneho bloku. Blok nemoze byt prazdny.
    current_token = peek();
    while(current_token.type == TOKEN_TYPE_EOL){       // prazdne riadky / komentare na zaciatku bloku
        next();
        current_token = peek();
    }
    if(current_token.type == TOKEN_TYPE_DEDENT){parser_error_exit(2, "Syntakticka chyba: blok nemoze byt prazdny");}
    last_program_node = root;
    
    //Loop ktory kontroluje lubovolny pocet statementov
    while (current_token.type != TOKEN_TYPE_DEDENT){
        temp = statement();
        ast_add_parent(last_program_node,temp,0);
        current_token = peek();
        while(current_token.type == TOKEN_TYPE_EOL){   // prazdne riadky / komentare za prikazom
            next();
            current_token = peek();
        }
        if(current_token.type == TOKEN_TYPE_EOF){
            next();
            parser_error_exit(2, "Syntakticka chyba: neocakavany koniec suboru vo vnutri bloku");
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

// function_body: telo funkcie - bud blok (za EOL) alebo jediny vyraz
//! Volana z: if_statement(), while_loop(), function_list()
AST_NODE* function_body(){
    AST_NODE* root = NULL;
    Token current_token = peek();
    if(current_token.type == TOKEN_TYPE_EOL){
        next();
        while(peek().type == TOKEN_TYPE_EOL){ next(); }   // prazdne riadky pred INDENT
        root = function_body_block();
    }else{
        root = ast_create_node_type(NODE_PROGRAM);
        ast_add_parent(root, statement(), 0);
    }

    return root;
}

/* ============================================================
   6. Funkcie
   ============================================================ */

// function_def: hlavicka funkcie "meno(parametre) [: typ] =" (bez tela)
//! Volana z: function_list()
AST_NODE* function_def(){
    AST_NODE* root = NULL;
    Token current_token = next();
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    if(current_token.type == TOKEN_TYPE_IDENTIFIER){
        root = ast_create_node(current_token);
        last_program_node = root; 
    }else{
        parser_error_exit(2, "Syntakticka chyba v definicii funkcie: za 'def' ocakavany nazov funkcie");
    }
    current_token = next();
    if(current_token.type != TOKEN_TYPE_LPAREN){
        parser_error_exit(2, "Syntakticka chyba v definicii funkcie: za nazvom ocakavana '('");
    }
    temp = parameter_list();
    ast_add_parent(last_program_node,temp,1);

    //TOKEN_TYPE_RPAREN kontorluje funkcia parameter_list() sama v sebe
    current_token = next();
    if(current_token.type == TOKEN_TYPE_COLON){
        current_token = peek();
        temp = get_type();
        ast_add_parent(last_program_node,temp,0);
        current_token = next();
    }
    if(current_token.type != TOKEN_TYPE_ASSIGN){parser_error_exit(2, "Syntakticka chyba v definicii funkcie: za hlavickou ocakavane '=' alebo ': typ ='");}

    return root;

}

// function_list: jedna definicia funkcie ([@main] def ...) = hlavicka + telo
//! Volana z: program()
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
        
        }else if(current_token.type == TOKEN_TYPE_EOL){ //spotrebovanie EOL tokenu aby mohol byt na inom riadku ako def
            current_token = next();
            while(current_token.type == TOKEN_TYPE_EOL){ current_token = next(); }   // aj viac prazdnych riadkov
            if(current_token.type == KEYWORD_DEF){
            temp = ast_create_node_type(NODE_FUNC_DEF);
            ast_add_parent(root, temp, 0);
            last_program_node = temp;      
            }else{
                parser_error_exit(2, "Syntakticka chyba: za @main ocakavane 'def'");
            }
        }else{
            parser_error_exit(2, "Syntakticka chyba: za @main ocakavane 'def'");
        }

    }else if (current_token.type == KEYWORD_DEF){
        root = ast_create_node_type(NODE_FUNC_DEF); 
        last_program_node = root;
    }else{
        parser_error_exit(2, "Syntakticka chyba: ocakavana definicia funkcie (def alebo @main)");
    }
    
    temp = function_def();
    ast_add_parent(last_program_node, temp, 0);     //pridavanie definicie funkcie do laveho uzla 

    

    temp = function_body();
    ast_add_parent(last_program_node, temp, 1);     //pridavanie tela funkcie do raveho uzla 
    return root;

}

/* ============================================================
   7. Cely program
   ============================================================ */

// prolog: skontroluje povinny prolog "import meno.*" ukonceny EOL
//! Volana z: program()
bool prolog(){
    Token current_token = next();
    while(current_token.type == TOKEN_TYPE_EOL){ current_token = next(); }   // pred prologom mozu byt komentare a prazdne riadky
    if (current_token.type != KEYWORD_IMPORT) return false;
    
    current_token = next();       
    if (current_token.type != TOKEN_TYPE_IDENTIFIER) return false;
    token_free(&current_token);
    
    current_token = next();       
    if (current_token.type != TOKEN_TYPE_DOT) return false;
    
    current_token = next();       
    if (current_token.type != TOKEN_TYPE_MUL) return false;
    
    current_token = next();
    if (current_token.type != TOKEN_TYPE_EOL) return false;

    return true;
}

 // program: vstupna funkcia parsera - prolog a potom zoznam definicii funkcii, vracia koren AST
//! Volana z: main.c: main
AST_NODE* program(){
    AST_NODE* root = ast_create_node_type(NODE_PROGRAM);
    ast_root = root;
    if (prolog() == false){
        parser_error_exit(2,"Syntakticka chyba: chybajuci alebo chybny prolog 'import ifj26.*'");
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
                parser_error_exit(2,"Syntakticka chyba: na najvyssej urovni moze byt iba definicia funkcie (def alebo @main)");
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