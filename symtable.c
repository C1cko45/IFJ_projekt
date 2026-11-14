/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "symtable.h"

// IFJ26-specific error code
#define ERROR_INTERNAL 99

#define HTABLE_MAX_LOAD_FACTOR 0.7 // Load factor; round down (rounding up will make the number over 0.7)
#define HTABLE_INIT_CAPACITY 17    // Minimum 17 to satisfy load factor - initial global scope has 10 builtin functions

#define SYMTABLE_INIT_CAPACITY 1
#define SYMTABLE_GLOBAL_SCOPE 0

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* DJB2 Hash */
uint32_t DJBHash(const char *key)
{
    uint32_t hash = 5381;

    int c;
    while ((c = (*key++)))
        hash = hash * 33 + c;

    return hash;
}
/* BKDR Hash */
uint32_t BKDRHash(const char *key)
{
    uint32_t hash = 0;

    while (*key)
        hash = hash * 131 + (*key++);

    return hash;
}

/*
 * nextPrime() helper function
 * Returns boolean indicating if number is a prime
 * Works on trial-division principle
 */
static bool isPrime(size_t number)
{
    if (number < 2)
        return false;
    if (number == 2)
        return true;
    if (number % 2 == 0)
        return false;

    for (size_t i = 3; i * i <= number; i += 2)
    {
        if (number % i == 0)
            return false;
    }
    return true;
}
/*
 * resizeScope() helper function
 * Returns smallest prime greater than number
 * In context of resizeScope, parameter number is always a multiple of 2 and therefore not a prime
 * We leverage this info about number -> number+1 is odd, and we skip checking multiples of 2
 */
static size_t nextPrime(size_t number)
{
    if (number <= 1)
        return 2;

    while (!isPrime(++number))
    {
        // Exhausted every possible value in range of size_t
        if (number > SIZE_MAX - 2)
        {
            fprintf(stderr, "nextPrime() -- searching for next prime failed, exhausted size_t value range.\n");
            // TODO: cleanup();
            exit(ERROR_INTERNAL);
        }
        number += 2;
    }

    return number;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Returns SymTable initialized with global scope
 * Global scope is pre-filled with builtin functions
 */
SymTable createSymTable()
{
    SymTable sym_table = {
        .capacity = SYMTABLE_INIT_CAPACITY, .count = SYMTABLE_INIT_CAPACITY, .current = 0, .scopes = NULL};
    sym_table.scopes = calloc(1, sizeof(HTable));
    if (sym_table.scopes == NULL)
    {
        fprintf(stderr, "createSymTable() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    createScope(&sym_table);
    // TODO: Then push the builtin functions into SymTable.scopes[SYMTABLE_GLOBAL_SCOPE]

    return sym_table;
}
/*
 * Destroys the whole SymTable
 * Free memory of every scope, free memory of the scopes array, set capacity and count to 0
 */
void destroySymTable(SymTable *sym_table)
{
    while (sym_table->count > 0)
    {
        free(sym_table->scopes[sym_table->count - 1].entries);
        sym_table->scopes[sym_table->count - 1].entries = NULL;
        sym_table->count--;
    }

    free(sym_table->scopes);
    sym_table->scopes = NULL;
    sym_table->count = 0;
    sym_table->capacity = 0;
    sym_table->current = 0;
}
/*
 * Grows symTable, should be called only when symTable becomes full
 * Dynamic array semantics -> double the capacity
 */
void resizeSymTable(SymTable *sym_table)
{
    // Assigning to tmp variable as upon realloc failure we would lose the original pointer to then unchanged memory
    HTable *new_scopes = realloc(sym_table->scopes, sizeof(HTable) * sym_table->capacity * 2);
    if (new_scopes == NULL)
    {
        fprintf(stderr, "resizeSymTable() -- realloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    sym_table->scopes = new_scopes;
    sym_table->capacity *= 2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Creates and initializes new scope at the end of the SymTable; updates SymTable->count
 * Resizes the SymTable if needed
 */
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
/*
 * Destroys the last (most local) scope and updates SymTable->count
 * Works only on local scopes, global scope must exist for the duration of SymTable's lifetime
 */
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
/*
 * Grows the HTable, should only be called when count exceeds HTABLE_MAX_LOAD_FACTOR
 * Hash table semantics -> double the capacity, find the closest greater prime number (to guarantee probe coverage)
 * Rehash existing entries into the new HTable (new capacity changes order of entries)
 */
void resizeScope(HTable *scope)
{
    size_t new_capacity = nextPrime(scope->capacity * 2);
    HTEntry *old_entries = scope->entries;

    scope->entries = calloc(new_capacity, sizeof(HTEntry));
    if (scope->entries == NULL)
    {
        fprintf(stderr, "resizeScope() -- calloc() failed.\n");
        // TODO: cleanup();
        exit(ERROR_INTERNAL);
    }

    for (size_t i = 0; i < scope->count; i++)
    {
        insertEntry(&old_entries[i], scope); // rehash
    }
    free(old_entries);
    scope->capacity = new_capacity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool isEntry(const char *key, HTable *scope)
{
    // TODO:
    return true;
}

HTEntry *getEntry(const char *key, HTable *scope)
{
    // TODO:
    return NULL;
}

void insertEntry(HTEntry *entry, HTable *scope)
{
    // TODO:
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
