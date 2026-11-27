/*
 * Implementácia prekladača pre jazyk IFJ26.
 *
 * Rozhranie syntaktického analyzátora (rekurzívny zostup).
 *
 * Autor:   Richard Sovič
 * Login:   xsovicr00
 */
#ifndef PARSER_H
#define PARSER_H
#include "ast.h"
#include "token.h"

AST_NODE *program(void); // Vola main. Vracia pointer na korenovy uzol.;
bool prolog(void);       // kontrola ci ako prva vec v kode je povinny prolog;
AST_NODE *function_list(
    void); // Kontroluje strukturu definicie funkcie. Nasledne vola funkciu a kontrolu body a detajlov definicie
           //  vracia pointer na cast AST.
AST_NODE *function_def(void);   // Kontroluje spravnost definicie funkcie. Iba def nie bllok kodu pod nou
AST_NODE *function_body(void);  // Kontroluje blok kodu pod definiciou funkcie;
AST_NODE *parameter_list(void); // Toto kontorluje spravny zapis parametrov a vracia AST parametrov
void parser_error_exit(int error_code,
                       const char *message); // vypise error a ukonci program s vymazanim vsetkych alokacii
AST_NODE *get_type(void);                    // vrati node v ktorom je type
AST_NODE *parser_get_list(void);             // vrati AST v ktorom je rzlozeny zapis List[type]

AST_NODE *var_assign(Token first, Token second);
AST_NODE *call_f(Token first, Token second);
AST_NODE *if_statement(void);
AST_NODE *while_loop(void);
AST_NODE *for_loop(void);
AST_NODE *var_def(void);
AST_NODE *val_def(void);
AST_NODE *function_body_block(void);
AST_NODE *statement(void);

Token peek(void);
Token next(void);

#endif