#ifndef DSTRING_H
#define DSTRING_H

#include <stdbool.h>
#include <stdio.h>

// struktura pre dynamicke pole.
typedef struct
{
    size_t len;
    size_t capacity;
    char *str;
} dstring;

// inicializacia noveho dynamickeho pola.
bool dstring_init(dstring *s);

// zvacsovanie nafukovacieho(dynamickeho) pola.
bool dstring_add_char(dstring *s, char c);

// vymazanie obsahju pola.
void dstring_clear(dstring *s);

// uvolnenie pameti pre dynamicke pole.
void dstring_free(dstring *s);

#endif // DSTRING_H
