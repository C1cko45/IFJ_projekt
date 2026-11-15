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

// peek: nacita dalsi token zo scannera, ale nespotrebuje ho (dalsi peek/next vrati ten isty)
//! Volana z: get_type(), parser_get_list(), parameter_list(), var_def(), val_def(), call_f(), if_statement(),
//!     statement(), function_body_block(), function_body(), function_def(), program(), psa.c: parse_expression
Token peek(void){
    if(!has_saved){
        has_saved = true;
        saved_token = scanner_get_next_token();
    }
    return saved_token;
}

//TODO NA konci odobrat vypis tokenov
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
    }
    
    return t;
}

// parser_error_exit: vypise chybu na stderr a ukonci program s danym navratovym kodom
// TODO: parser_error_exit musi pred exit uvolnit vsetky alokacie (AST uzly, retazce identifikatorov cez token_free) - ast_free_node zatial neexistuje v ast.c
//! Volana z: get_type(), parser_get_list(), parameter_list(), var_def(), val_def(), call_f(), if_statement(),
//!     while_loop(), statement(), function_body_block(), function_def(), function_list(), program(), psa.c: viac
//!     funkcii
void parser_error_exit(int error_code, const char *message){
    fprintf(stderr, "Chyba (%d): %s\n",error_code,message);
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
            parser_error_exit(2,"Syntakticka chyba,get_type");
    }
    return temp;
}

// parser_get_list: spracuje zapis List[typ] a vrati uzol NODE_LIST (typ prvku v lavom potomkovi)
//! Volana z: get_type()
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

// TODO: parameter_list - kontrola, ci su mena parametrov v zozname unikatne (ak to nerobi semanticka cast)
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

/* ============================================================
   3. Definicie premennych a priradenie
   ============================================================ */


// TODO: var_def - PSA (parse_expression) musi vediet skoncit aj na dalsich tokenoch (COMMA, EOF, DEDENT) podla toho, kde sa vyraz moze vyskytnut
// var_def: spracuje definiciu premennej "var meno [: typ] = vyraz" (vyraz cez precedencnu analyzu)
//! Volana z: statement()
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
        
    default:
        parser_error_exit(2, "Syntakticka chyba,var_def");
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
    if(current_token.type != TOKEN_TYPE_IDENTIFIER){parser_error_exit(2, "Syntakticka chyba,val_def");}
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
        if(current_token.type != TOKEN_TYPE_ASSIGN){parser_error_exit(2, "Syntakticka chyba,val_def");}
        temp = parse_expression();
        ast_add_parent(root,temp,1);
        break;
    default:
        parser_error_exit(2, "Syntakticka chyba,val_def");
        break;
    }
    
    return root;
}

// TODO: var_assign - po '=' zavolat parse_expression(), vytvorit uzol priradenia (meno premennej + vyraz) a VRATIT ho (teraz vracia NULL)
// TODO: var_assign - odstranit nepouzite premenne a '(void)' riadok, ked bude funkcia hotova
//TODO
// var_assign: priradenie "meno = vyraz" (zatial nedokoncene)
//! Volana z: statement()
AST_NODE* var_assign(Token first, Token second){
    AST_NODE* root = ast_create_node(second);
    AST_NODE* temp = ast_create_node(first);
    ast_add_parent(root, temp, 0);
    temp = parse_expression();
    ast_add_parent(root, temp, 1);
    return root;
}

// TODO: call_f - spracovat argumenty (vyraz, ciarka, ...) az po ')', vratit uzol NODE_CALL; PSA musi skoncit vyraz na ',' a na ')' bez parovej '('
// TODO: call_f - volanie funkcie moze byt aj sucast vyrazu (napr. x = f(1) + 2), nielen samostatny prikaz - dorobit do PSA
//TODO
// call_f: volanie funkcie "meno(...)" (zatial nedokoncene)
//! Volana z: statement(), psa.c: parse_expression (volanie na zaciatku vyrazu)
AST_NODE* call_f(Token first, Token second){
    if(first.type == TOKEN_TYPE_IDENTIFIER && second.type == TOKEN_TYPE_LPAREN){
        AST_NODE* root = ast_create_node_type(NODE_CALL);
        ast_add_parent(root, ast_create_node(first), 0);  //pridanie nazvu funkcie 
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
                    argument = parse_expression();
                    ast_add_parent(temp, argument, 0 );
                    

                    break;
                case TOKEN_TYPE_RPAREN:
                    next();
                    return root;
                    break;
                default:
                    parser_error_exit(2,"Chyba v call_f" );
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
                }else{
                    parser_error_exit(2,"Chyba v call_f2" );
                }
                
            }else{
                parser_error_exit(2,"Chyba v call_f2" );
            }

        }
    }
    return NULL;
}

/* ============================================================
   4. Riadiace struktury (zatial nedokoncene)
   ============================================================ */

// TODO: if_statement - spracovat 'if vyraz then' + blok (INDENT ... DEDENT), volitelne 'else' (lavy faktor gramatiky), vytvorit NODE_IF
// TODO: if_statement - blok musi byt znovupouzitelny (teraz ho vie len function_body_block pre telo funkcie)
//TODO
// if_statement: podmienka if / else
//! Volana z: sama seba (rekurzia), statement()
AST_NODE* if_statement(){
    AST_NODE* root = ast_create_node_type(NODE_IF);
    AST_NODE* podmienka = ast_create_node_type(NODE_PODMIENKA);
    ast_add_parent(root, podmienka, 0);
    AST_NODE* vyraz = parse_expression();
    ast_add_parent(podmienka, vyraz,0);
    if(next().type != KEYWORD_THEN){parser_error_exit(2,"Chyba v if_statement" );}
    AST_NODE* temp = function_body();
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
                temp = function_body();
            break;
            
        }
        
        
    }
    return root;
}

// TODO: while_loop - spracovat 'while vyraz do' + blok, vytvorit NODE_WHILE
//TODO
// while_loop: cyklus while
//! Volana z: statement()
AST_NODE* while_loop(){
    AST_NODE* root = ast_create_node_type(NODE_WHILE);
    AST_NODE* temp = parse_expression();
    ast_add_parent(root, temp, 0);
    if(next().type != KEYWORD_DO){parser_error_exit(2,"Chyba v while_loop" );}
    temp = function_body();
    ast_add_parent(root, temp,1);

    return root;
}

// TODO: for_loop - rozsirenie CONTROL: spracovat hlavicku for (token ARROW '<-', rozsah, pripadne guard) + blok, vytvorit NODE_FOR
//TODO
// for_loop: cyklus for
//! Volana z: nikde zatial (ma ju volat statement() vo vetve KEYWORD_FOR)
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
            if(next().type != TOKEN_TYPE_INDENT){parser_error_exit(2,"Chyba v for_loop" );}
            if(peek().type != TOKEN_TYPE_IDENTIFIER){parser_error_exit(2,"Chyba v for_loop" );}
            //Vytvorenie a pridanie uzla s nazvom premmennej k root
            nazov_p = ast_create_node(next());
            if(next().type != TOKEN_TYPE_ARROW){parser_error_exit(2,"Chyba v for_loop");}
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
            
            if(next().type != TOKEN_TYPE_EOL){parser_error_exit(2,"Chyba v for_loop" );}
            last_temp = nazov_p;
            while(peek().type == KEYWORD_IF){
                next();
                temp = ast_create_node_type(NODE_PODMIENKA);  
                ast_add_parent(temp, parse_expression(),0);
                ast_add_parent(last_temp, temp,1);
                last_temp = temp;
                if(next().type != TOKEN_TYPE_EOL){parser_error_exit(2,"Chyba v for_loop");}
            }
            if(next().type != TOKEN_TYPE_DEDENT){parser_error_exit(2,"Chyba v for_loop" );}
            if(next().type != KEYWORD_DO){parser_error_exit(2,"Chyba v for_loop" );}
            break;





        case TOKEN_TYPE_IDENTIFIER:
            if(peek().type != TOKEN_TYPE_IDENTIFIER){parser_error_exit(2,"Chyba v for_loop" );}
            //Vytvorenie a pridanie uzla s nazvom premmennej k root
            nazov_p = ast_create_node(next());
            if(next().type != TOKEN_TYPE_ARROW){parser_error_exit(2,"Chyba v for_loop");}
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
            if(next().type != KEYWORD_DO){parser_error_exit(2,"Chyba v for_loop" );}
            break;
        default:
            parser_error_exit(2,"Chyba v for_loop" );
            break;
    }
    ast_add_parent(root,function_body(),1); 
        
    return root;
}

/* ============================================================
   5. Prikazy a bloky kodu
   ============================================================ */

// TODO: statement - za IF, WHILE a FOR volat if_statement(), while_loop(), for_loop() (teraz sa len spotrebuje klucove slovo a nasleduje chyba EOL)
// DONE statement - po bloku (if/while/for) konci prikaz tokenom DEDENT, nie EOL; kontrola EOL na konci funkcie sa na ne nesmie vztahovat
// TODO: statement - prikaz zacinajuci identifikatorom, za ktorym nie je '=' ani '(' (napr. 'a + 1', 'a.b'), dnes konci chybou 2; identifikator treba odovzdat do PSA
// TODO: statement - KEYWORD_RETURN: spracovat 'return [vyraz]'
// TODO: statement - case TOKEN_TYPE_EOL: rozhodnut, ako sa spracuje prazdny riadok (dnes sa v nom spotrebuje EOL a vrati sa NULL)
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
            if(peek().type != TOKEN_TYPE_EOL){
                AST_NODE* temp = parse_expression();
                ast_add_parent(root,temp,0);
            }
            
            break;
        case TOKEN_TYPE_EOL:
            
            break;
        
        default:
            root = parse_expression();
            break;
        }    
    // TODO: statement - odstranit ladiaci vypis DEBUG (fprintf) pred odovzdanim
    if(current_token.type == KEYWORD_WHILE || current_token.type == KEYWORD_IF || current_token.type == KEYWORD_FOR){
        
    }else{
        current_token = next();
        if(current_token.type != TOKEN_TYPE_EOL){
            fprintf(stderr, "DEBUG: ocakavany EOL, prisiel typ %d\n", current_token.type);
            parser_error_exit(2, "Syntakticka chyba, statementx");
        }
    }
    
    return root;
}

// TODO: function_body_block - po ukonceni bloku skontrolovat EOL/DEDENT za poslednym prikazom a vyriesit vnorene bloky (if/while/for) cez rovnaku funkciu
// TODO: function_body_block - uvolnit/osetrit pripad, ked statement() vrati NULL (prazdny riadok) pri ast_add_parent
// function_body_block: blok prikazov medzi INDENT a DEDENT, prikazy su zretazene cez NODE_PROGRAM
//! Volana z: function_body()
AST_NODE* function_body_block(){
    //vytvorime prvy node program bloku
    AST_NODE* root = ast_create_node_type(NODE_PROGRAM);
    AST_NODE* temp = NULL;     //sem sa docasne ukladaju vratene pontre na uzly ktore treba pripojit na koren
    AST_NODE* last_program_node = NULL;   //ukazatel na posledny node z ktoreho vychadzame
    Token current_token = next();
    if(current_token.type != TOKEN_TYPE_INDENT){parser_error_exit(2, "Syntakticka chyba, function_body_block1");}
    
    //pre pripad prazdneho bloku. Blok nemoze byt prazdny.
    current_token = peek();
    while(current_token.type == TOKEN_TYPE_EOL){       // prazdne riadky / komentare na zaciatku bloku
        next();
        current_token = peek();
    }
    if(current_token.type == TOKEN_TYPE_DEDENT){parser_error_exit(2, "Syntakticka chyba, function_body_block2");}
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

// TODO: function_body - po jedinom vyraze (telo na jednom riadku) skontrolovat ukoncenie riadku (EOL), dnes sa nespotrebuje
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
        root = parse_expression();
    }

    return root;
}

/* ============================================================
   6. Funkcie
   ============================================================ */

// DONE function_def - kod za 'return root;' v if/else je nedosiahnutelny, odstranit
// DONE function_def - skontrolovat, ci je navratovy typ povinny (podla zadania)
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
        parser_error_exit(2, "Syntakticka chyba,function_def1");
    }
    current_token = next();
    if(current_token.type != TOKEN_TYPE_LPAREN){
        parser_error_exit(2, "Syntakticka chyba,function_def2");
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
    if(current_token.type != TOKEN_TYPE_ASSIGN){parser_error_exit(2, "Syntakticka chyba,function_def3");}

    return root;

}

// TODO: function_list - nepouzite premenne a prazdne riadky na konci uprat (kozmetika)
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

/* ============================================================
   7. Cely program
   ============================================================ */

// prolog: skontroluje povinny prolog "import meno.*" ukonceny EOL
//! Volana z: program()
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

 // program: vstupna funkcia parsera - prolog a potom zoznam definicii funkcii, vracia koren AST
//! Volana z: main.c: main
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