#ifndef SCANNER_H
#define SCANNER_H

#include <stdio.h>
#include "token.h"

void scanner_init(FILE *f);
Token scanner_get_next_token(void);
const char *token_type_name(TokenType type);   // text pre ladiaci vypis

#endif // SCANNER_H