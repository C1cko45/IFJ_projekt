#include <stdio.h>
#include <string.h>

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
    char *identifier; // key
    // type
    // value
};

struct HTable
{
    size_t capacity;
    size_t count;
    HTEntry *entries;
};

struct SymTable
{
    size_t capacity;
    size_t count;
    HTable *scopes;
};

/* DJB2 Hash */
unsigned long DJBHash(const char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = (unsigned char)*str++))
        hash = hash * 33 + c;

    return hash;
}

/* BKDR Hash */
unsigned int BKDRHash(const char *str)
{
    unsigned int hash = 0;

    while (*str)
        hash = hash * 131 + (unsigned char)*str++;

    return hash;
}
