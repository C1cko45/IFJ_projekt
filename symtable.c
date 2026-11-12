#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "symtable.h"

#define HTABLE_MAX_LOAD 0.7 // Load factor; round down (rounding up will make the number over 0.7)

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
uint32_t DJBHash(const char *key)
{
    uint32_t hash = 5381;

    int c;
    while ((c = (unsigned char)*key++))
        hash = hash * 33 + c;

    return hash;
}

/* BKDR Hash */
uint32_t BKDRHash(const char *key)
{
    uint32_t hash = 0;

    while (*key)
        hash = hash * 131 + (unsigned char)*key++;

    return hash;
}
