#include <stdio.h>

#include "symtable.h"

#define HT_MAX_SIZE 13

typedef enum
{
    EMPTY,
    BUSY,
    DELETED
} EntryState;

struct HTEntry
{
    EntryState state;

    char *identifier;
    // type
    // value
};

struct SymTable
{
    size_t capacity;
    size_t size;

    HTEntry **scopes;
};

/* DJB2 Hash */
unsigned long DJBHash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c

    return hash;
}

/* BKDR Hash */
unsigned int BKDRHash(unsigned char *str, unsigned int length)
{
    unsigned int seed = 131;
    unsigned int hash = 0;

    for (unsigned int i = 0; i < length; str++, i++)
        hash = (hash * seed) + (*str);

    return hash;
}
