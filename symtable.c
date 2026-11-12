#include <stdint.h>
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
uint32_t DJBHash(const char *str)
{
    uint32_t hash = 5381;
    int c;

    while ((c = (unsigned char)*str++))
        hash = hash * 33 + c;

    return hash;
}

/* BKDR Hash */
uint32_t BKDRHash(const char *str)
{
    uint32_t hash = 0;

    while (*str)
        hash = hash * 131 + (unsigned char)*str++;

    return hash;
}
