#ifndef DSTRING_H
#define DSTRING_H

#include <stdio.h>
#include <stdbool.h>

typedef struct 
{
    size_t len;
    size_t capacity;
    char *str;
} dstring;


bool dstring_init(dstring *s);

bool dstring_add_char(dstring *s, char c);

void dstring_clear(dstring *s);

void dstring_free(dstring *s);


#endif // DSTRING_H