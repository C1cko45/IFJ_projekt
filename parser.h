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

#endif