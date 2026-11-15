/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "symtable.h"

// IFJ26-specific error code
#define ERROR_INTERNAL 99

#define HTABLE_MAX_LOAD_FACTOR 7 // Load factor * 10, to avoid floating-point arithmetic and conversion size_t -> double
#define HTABLE_INIT_CAPACITY 17  // Min. 17 to satisfy load factor for initial global scope - has 10 builtin functions

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
        .capacity = SYMTABLE_INIT_CAPACITY, .count = SYMTABLE_INIT_CAPACITY, .current_scope = 0, .scopes = NULL};
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
    sym_table->current_scope = 0;
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
 * Creates and initializes new scope after current scope in SymTable
 * Resizes the SymTable if needed
 *
 * Should only be called when entering a new local scope in IFJ26 code
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
    sym_table->current_scope++;
}
/*
 * Destroys the current local scope in SymTable
 * Works only on local scopes (global scope must exist for the duration of SymTable's lifetime)
 *
 * Should only be called when exiting current local scope in IFJ26 code
 */
void destroyScope(SymTable *sym_table)
{
    if (sym_table->count == 1)
    {
        fprintf(stderr, "destroyScope() -- attempt to destroy global scope.\n");
        // TODO: cleanup();
        exit(ERROR_INTERNAL);
    }

    for (size_t capacity = sym_table->scopes[sym_table->count - 1].capacity; capacity > 0; capacity--)
    {
        if (sym_table->scopes[sym_table->count - 1].entries[capacity - 1].state != EMPTY)
            free(sym_table->scopes[sym_table->count - 1].entries[capacity - 1].identifier);
    }
    free(sym_table->scopes[sym_table->count - 1].entries);
    sym_table->scopes[sym_table->count - 1].entries = NULL;
    sym_table->scopes[sym_table->count - 1].count = 0;
    sym_table->scopes[sym_table->count - 1].capacity = 0;

    sym_table->count--;
    sym_table->current_scope--;
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

    scope->capacity = new_capacity;
    for (size_t i = 0; i < scope->count; i++)
        insertEntry(&old_entries[i], scope); // rehash

    free(old_entries);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Searches for entry of specified type (FUNCTION/VARIABLE) in specified HTable
 * Returns TRUE if found, FALSE otherwise
 */
bool isEntry(const char *key, HTEntryType entry_type, HTable *table)
{
    size_t index_hash = DJBHash(key);
    size_t probe_step_hash = BKDRHash(key);

    // Double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1)
    // scope->capacity == MAX+1
    HTEntry *target_entry = &table->entries[index_hash % (table->capacity)];
    for (size_t i = 1; target_entry->state != EMPTY && target_entry->state != DELETED; i++)
    {
        // if identifier type (function/variable) matches and then identifier matches
        if (target_entry->entry_type == entry_type && strcmp(key, target_entry->identifier) == 0)
            return true;
        target_entry = &table->entries[(index_hash + i * probe_step_hash) % table->capacity];
    }

    return false;
}
/*
 * Searches and fetches entry of specified type (FUNCTION/VARIABLE) in specified table
 * Returns NULL if identifier doesn't exist
 */
HTEntry *getEntry(const char *key, HTEntryType entry_type, HTable *table)
{
    size_t index_hash = DJBHash(key);
    size_t probe_step_hash = BKDRHash(key);
    HTEntry *target_entry = &table->entries[index_hash % (table->capacity)];

    // Double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1)
    // scope->capacity == MAX+1
    for (size_t i = 1; target_entry->state != EMPTY && target_entry->state != DELETED; i++)
    {
        // if identifier type (function/variable) matches and then identifier matches
        if (target_entry->entry_type == entry_type && strcmp(key, target_entry->identifier) == 0)
            return target_entry;
        target_entry = &table->entries[(index_hash + i * probe_step_hash) % table->capacity];
    }

    return NULL;
}

/*
 * Inserts specified entry into specified table (~ scope)
 * Implemented as Double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1) (scope->capacity == MAX+1)
 *
 * Function assumes entry(identifier+identifier_type) isn't already in specified scope
 * First invoke isEntry(entry->identifier, entry->entry_type, table) to check that assumption
 */
void insertEntry(HTEntry *entry, HTable *table)
{
    // If inserting exceeds load_factor -> grow HTable, rehash
    // COUNT+1 > MAX_COUNT
    // MAX_COUNT = (CAPACITY * 0.7 (load factor)) * 10 -- To avoid floating-points
    if ((table->count + 1) * 10 > (table->capacity * HTABLE_MAX_LOAD_FACTOR))
        resizeScope(table);

    size_t index_hash = DJBHash(entry->identifier);
    size_t probe_step_hash = BKDRHash(entry->identifier);
    HTEntry *target_entry = &table->entries[index_hash % (table->capacity)];

    // Find EMPTY place in scope (HTable synonyms list) of SymTable
    for (size_t i = 1; target_entry->state != EMPTY; i++)
        target_entry = &table->entries[(index_hash + i * probe_step_hash) % table->capacity];

    size_t str_len = strlen(entry->identifier) + 1; // account for '\0' that strlen() doesn't count
    target_entry->identifier = calloc(str_len, sizeof(char *));
    if (target_entry->identifier == NULL)
    {
        fprintf(stderr, "insertEntry() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    // The rest of target_entry->identifier filled with '\0'
    strncpy(target_entry->identifier, entry->identifier, str_len);
    target_entry->entry_type = entry->entry_type;
    // TODO: Is this legal? Does it shallow-copy or deep-copy?
    // Does it matter since scope is a pointer?
    // It should stay for the lifetime of SymTable no?
    target_entry->entry_data = entry->entry_data;
    target_entry->state = BUSY;
}
/*
 * Deletes entry from specified HTable (scope)
 * Returns TRUE if successful, FALSE otherwise
 */
bool deleteEntry(HTEntry *entry, HTable *table)
{
    HTEntry *target_entry = getEntry(entry->identifier, entry->entry_type, table);

    if (target_entry != NULL)
    {
        target_entry->state = DELETED;
        return true;
    }

    return false;
}

/*
 * Tries to resolve specified identifier based on its lexical scope
 * -> It searches for it in the innermost local scope, then the outer local scope until it finds it or not
 * Identifiers are bound in the local scope they're defined in and its subblocks (nested inner local scopes)
 *
 * Function assumes correctly initialized sym_table (at least has global scope)
 * Returns boolean TRUE if found, FALSE otherwise
 */
bool isIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table)
{
    for (size_t scope_index = sym_table->current_scope; scope_index >= 1; scope_index--)
    {
        if (isEntry(identifier, entry_type, &sym_table->scopes[scope_index]))
            return true;
    }
    return false;
}
/*
 * Tries to resolve specified identifier based on its lexical scope
 * -> It searches for it in the innermost local scope, then the outer local scope until it finds it or not
 * Identifiers are bound in the local scope they're defined in and its subblocks (nested inner local scopes)
 *
 * Function assumes correctly initialized sym_table (at least has global scope)
 * Returns pointer to the identifier entry in symTable if found, NULL otherwise
 */
HTEntry *getIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table)
{
    HTEntry *target_identifier;
    for (size_t scope_index = sym_table->current_scope; scope_index >= 1; scope_index--)
    {
        target_identifier = getEntry(identifier, entry_type, &sym_table->scopes[scope_index]);
        if (target_identifier != NULL)
            return target_identifier;
    }
    return NULL;
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
