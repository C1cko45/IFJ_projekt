#include "dstring.h"
#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

#define START_LENGTH (size_t)8

// inicializacia dynamickeho pola, pociatocna kapacita 8, dlzka 0.
bool dstring_init(dstring *s)
{
    if (s == NULL)
        return false;

    s->str = NULL;
    s->capacity = 0;
    s->len = 0;

    s->str = malloc(START_LENGTH);
    if (s->str == NULL)
        return false;

    s->str[0] = '\0';
    s->capacity = START_LENGTH;
    return true;
}

// vymazanie obsahu dnamickeho pola.
void dstring_clear(dstring *s)
{
    if (s == NULL || s->str == NULL)
        return;

    s->len = 0;
    s->str[0] = '\0';
}

// uvolnenie pamete dynamickeho pola.

void dstring_free(dstring *s)
{
    if (s == NULL)
        return;

    free(s->str);
    s->str = NULL;
    s->len = 0;
    s->capacity = 0;
}

// pridanie znaku na konec pola, ak je pole plne zvacsi ho.

bool dstring_add_char(dstring *s, char c)
{
    if (s == NULL || s->str == NULL ||
        s->capacity == 0 || s->len >= s->capacity)
        return false;

    if (s->len == s->capacity - 1)
    {
        if (s->capacity > SIZE_MAX / 2)
            return false;

        size_t new_capacity = s->capacity * 2;
        char *new_str = realloc(s->str, new_capacity);

        if (new_str == NULL)
            return false;

        s->str = new_str;
        s->capacity = new_capacity;
    }

    s->str[s->len++] = c;
    s->str[s->len] = '\0';

    return true;
}
