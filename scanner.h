/*
 * Implementácia prekladača pre jazyk IFJ26.
 *
 * Lexikálny analyzátor - rozhranie.
 *
 * Autor:   Deák Dominik
 * Login:   xdeakdo00
 */
#ifndef SCANNER_H
#define SCANNER_H

#include "token.h"
#include <stdio.h>

// inicializacia skenera.
void scanner_init(FILE *source);

// hlavna funkcia: nacita dalsi token zo vstupu.
Token scanner_get_next_token(void);

#endif // SCANNER_H