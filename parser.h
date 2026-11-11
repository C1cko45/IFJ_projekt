#ifndef PARSER_H
#define PARSER_H
#include "ast.h"
#include "token.h"

AST_NODE* program();    //Vola main. Vracia pointer na korenovy uzol.; 
bool prolog();          //kontrola ci ako prva vec v kode je povinny prolog;
AST_NODE* function_list(Token token);   //Kontroluje strukturu definicie funkcie. Nasledne vola funkciu a kontrolu body a detajlov definicie
                                        // vracia pointer na cast AST.
AST_NODE* function_def();               // Kontroluje spravnost definicie funkcie. Iba def nie bllok kodu pod nou
AST_NODE* function_body();               //Kontroluje blok kodu pod definiciou funkcie;
AST_NODE* parameter_list();             //Toto kontorluje spravny zapis parametrov a vracia AST parametrov
void parser_error_exit(int error_code, const char *message); //vypise error a ukonci program s vymazanim vsetkych alokacii
AST_NODE* get_type(Token token); //vrati node v ktorom je type 
AST_NODE* parser_get_list(); //vrati AST v ktorom je rzlozeny zapis List[type]


AST_NODE* parse_expression();
AST_NODE* var_assign(Token first, Token second);
AST_NODE* call_f(Token first, Token second);
AST_NODE* if_statement();
AST_NODE* while_loop();
AST_NODE* for_loop();
AST_NODE* var_def();
AST_NODE* val_def();
AST_NODE* function_body_block();
AST_NODE* statement_list(Token token);
AST_NODE* statement(Token token);
#endif