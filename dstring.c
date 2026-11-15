#include "dstring.h"
#include <stdio.h>
#include <stdlib.h>

#define START_LENGTH (size_t)8

// inicializacia dynamickeho pola, pociatocna kapacita 8, dlzka 0.
bool dstring_init(dstring *s)
{
    s->str = malloc(START_LENGTH);
    if (s->str == NULL)
    {
        return false;
    }
    s->str[0] = '\0';
    s->capacity = START_LENGTH;
    s->len = 0;
    return true;
}

// vymazanie obsahu dnamickeho pola.
void dstring_clear(dstring *s)
{
    s->len = 0;
    s->str[0] = '\0';
}

// uvolnenie pamete dynamickeho pola.
void dstring_free(dstring *s)
{
    free(s->str);
    s->str = NULL;
    s->len = 0;
    s->capacity = 0;
}

// pridanie znaku na konec pola, ak je pole plne zvacsi ho.
bool dstring_add_char(dstring *s, char c)
{
    if (s->len + 1 >= s->capacity)
    {
        size_t new_capacity = s->capacity * 2;
        char *test = realloc(s->str, new_capacity);
        if (test != NULL)
        {
            s->str = test;
            s->capacity = new_capacity;
        }
        else
        {
            return false;
        }
    }

    s->str[s->len] = c;
    s->len++;
    s->str[s->len] = '\0';
    return true;
}
