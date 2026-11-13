#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtable.h"

// IFJ26-specific error codes
#define ERROR_INTERNAL 99

#define HTABLE_INIT_CAPACITY 17
#define HTABLE_MAX_LOAD_FACTOR 0.7 // Load factor; round down (rounding up will make the number over 0.7)

#define SYMTABLE_INIT_CAPACITY 1
#define SYMTABLE_GLOBAL_SCOPE 0

typedef enum
{
    INT,
    DOUBLE,
    STRING,
    BOOL,
    UNIT,
    LIST
} Type;
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
/* Scope implemented as a hash table */
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

////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Returns SymTable initialized with global scope
 * Global scope is pre-filled with builtin functions */
SymTable createSymTable()
{
    SymTable sym_table = {.capacity = SYMTABLE_INIT_CAPACITY, .count = SYMTABLE_INIT_CAPACITY, .scopes = NULL};
    sym_table.scopes = calloc(1, sizeof(HTable));
    if (sym_table.scopes == NULL)
    {
        fprintf(stderr, "initSymTable() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    createScope(&sym_table);
    // TODO: Then push the builtin functions into SymTable.scopes[SYMTABLE_GLOBAL_SCOPE]

    return sym_table;
}
void destroySymTable(SymTable *sym_table)
{
    // TODO: destroyScope() and set NULL for every SymTable.scope[x]
    // Don't have to return any value indicating status of success, just fprintf upon error
}

/* Creates and initializes new scope at the end of the SymTable; updates SymTable->count
 * Resizes the SymTable if needed */
void createScope(SymTable *sym_table)
{
    if ((sym_table->count + 1) > sym_table->capacity)
        resizeSymTable(sym_table);

    sym_table->scopes[sym_table->count].count = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->count].capacity = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->count].entries = calloc(HTABLE_INIT_CAPACITY, sizeof(HTEntry));
    if (sym_table->scopes[sym_table->count].entries == NULL)
    {
        fprintf(stderr, "createScope() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    sym_table->count++;
}
/* Destroys the last (most local) scope and updates SymTable->count
 * Works only on local scopes; global scope must exist for the duration of SymTable's lifetime */
void destroyScope(SymTable *sym_table)
{
    if (sym_table->count == 1)
    {
        fprintf(stderr, "destroyScope() -- attempt to destroy global scope.\n");
        // TODO: cleanup();
        exit(ERROR_INTERNAL);
    }

    sym_table->scopes[sym_table->count - 1].count = 0;
    sym_table->scopes[sym_table->count - 1].capacity = 0;
    free(sym_table->scopes[sym_table->count - 1].entries);
    sym_table->scopes[sym_table->count - 1].entries = NULL;

    sym_table->count--;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
int main(void)
{
    // char string[] = "asdfasdf";
    //
    // uint32_t hash = DJBHash(string);
    // printf("%u, %u\n", *string, hash);
    //
    // hash = BKDRHash(string);
    // printf("%u\n", hash);

    return 0;
}
