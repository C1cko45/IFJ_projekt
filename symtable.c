/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symtable.h"

#define ERROR_INTERNAL 99 // internal program error; failed memory allocation, ...

#define HTABLE_MAX_LOAD_FACTOR 7 // Load factor*10, to avoid floating-point conversion and arithmetic
#define HTABLE_INIT_CAPACITY 29  // Min value to satisfy load factor for initial global scope - has 19 builtin functions

#define SYMTABLE_INIT_CAPACITY 1
#define SYMTABLE_GLOBAL_SCOPE 0

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* DJB2 Hash */
static uint32_t DJBHash(const char *key)
{
    uint32_t hash = 5381;

    int c;
    while ((c = (*key++)))
        hash = hash * 33 + c;

    return hash;
}
/* BKDR Hash */
static uint32_t BKDRHash(const char *key)
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
 * Helper function for createSymTable()
 * Pushes builtin functions into SymTable's global scope
 *
 * Assumes initialized SymTable with initialized global scope
 */
static void insertBuiltins(SymTable *sym_table)
{
    HTEntry builtin_function = {.state = BUSY,
                                .identifier = "print",
                                .entry_type = FUNCTION,
                                .entry_data.function.param_count = 0,
                                .entry_data.function.return_type = UNIT};
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "head";
    builtin_function.entry_data.function.return_type = ANY;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "tail";
    builtin_function.entry_data.function.return_type = LIST;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "toList";
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "readLine";
    builtin_function.entry_data.function.return_type = STRING;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "readInt";
    builtin_function.entry_data.function.return_type = INT;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "readDouble";
    builtin_function.entry_data.function.return_type = DOUBLE;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "readBoolean";
    builtin_function.entry_data.function.return_type = BOOLEAN;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "toStr";
    builtin_function.entry_data.function.param_count = 1;
    builtin_function.entry_data.function.return_type = STRING;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "chr";
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "drop";
    builtin_function.entry_data.function.return_type = LIST;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "take";
    builtin_function.entry_data.function.return_type = LIST;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "toInt";
    builtin_function.entry_data.function.return_type = INT;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "strLength";
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "ord";
    builtin_function.entry_data.function.param_count = 2;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    builtin_function.identifier = "subStr";
    builtin_function.entry_data.function.param_count = 3;
    builtin_function.entry_data.function.return_type = STRING;
    insertEntry(&builtin_function, &sym_table->scopes[0]);

    // Dubious one, specs support range(start, end) AND range(start,end,step)
    // -> Which normally leads to re-definition by SymTable design
    // Language spec itself explicitly doesn't support it for user-defined functions...
    builtin_function.identifier = "range";
    builtin_function.entry_data.function.return_type = LIST;
    insertEntry(&builtin_function, &sym_table->scopes[0]);
}
/*
 * Returns SymTable initialized with global scope
 * Global scope is pre-filled with builtin functions
 */
SymTable *createSymTable()
{
    SymTable *sym_table = calloc(1, sizeof(SymTable));
    if (sym_table == NULL)
    {
        fprintf(stderr, "createSymTable() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    // Initialize symTable with global scope
    sym_table->capacity = SYMTABLE_INIT_CAPACITY;
    sym_table->count = SYMTABLE_INIT_CAPACITY;
    sym_table->current_scope = 0;
    sym_table->scopes = calloc(SYMTABLE_INIT_CAPACITY, sizeof(HTable));
    if (sym_table->scopes == NULL)
    {
        fprintf(stderr, "createSymTable() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    // Initialize global scope
    sym_table->scopes[sym_table->current_scope].count = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->current_scope].capacity = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->current_scope].entries = calloc(HTABLE_INIT_CAPACITY, sizeof(HTEntry));
    if (sym_table->scopes[sym_table->current_scope].entries == NULL)
    {
        fprintf(stderr, "createScope() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    insertBuiltins(sym_table);

    return sym_table;
}
/*
 * Destroys the whole SymTable
 * Free memory of every scope, free memory of the scopes array, set capacity and count to 0
 */
void destroySymTable(SymTable *sym_table)
{
    // Destroy all local scopes
    while (sym_table->current_scope > 0)
        destroyScope(sym_table);
    // Destroy global scope
    for (size_t index = sym_table->scopes[0].capacity - 1; index >= 0; index--)
    {
        if (sym_table->scopes[0].entries[index].state != EMPTY)
            free(sym_table->scopes[0].entries[index].identifier);
    }
    free(sym_table->scopes[0].entries);

    free(sym_table->scopes);
    sym_table->scopes = NULL;
    sym_table->count = 0;
    sym_table->capacity = 0;
    sym_table->current_scope = 0;
}
/*
 * Grows symTable, should be called only when symTable becomes full
 * Dynamic array semantics -> double the capacity
 *
 * Helper function for createScope()
 */
static void resizeSymTable(SymTable *sym_table)
{
    // Assigning to tmp variable -> if realloc() failed, we would lose access to original unfree'd memory
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

    sym_table->scopes[sym_table->current_scope + 1].count = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->current_scope + 1].capacity = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->current_scope + 1].entries = calloc(HTABLE_INIT_CAPACITY, sizeof(HTEntry));
    if (sym_table->scopes[sym_table->current_scope + 1].entries == NULL)
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
    if (sym_table->current_scope == 0)
    {
        fprintf(stderr, "destroyScope() -- attempt to destroy global scope.\n");
        // TODO: cleanup();
        exit(ERROR_INTERNAL);
    }

    for (size_t index = sym_table->scopes[sym_table->current_scope].capacity - 1; index >= 0; index--)
    {
        if (sym_table->scopes[sym_table->current_scope].entries[index].state != EMPTY)
            free(sym_table->scopes[sym_table->current_scope].entries[index].identifier);
    }
    free(sym_table->scopes[sym_table->current_scope].entries);
    sym_table->scopes[sym_table->current_scope].entries = NULL;
    sym_table->scopes[sym_table->current_scope].count = 0;
    sym_table->scopes[sym_table->current_scope].capacity = 0;

    sym_table->count--;
    sym_table->current_scope--;
}
/*
 * Grows the HTable, should only be called when count exceeds HTABLE_MAX_LOAD_FACTOR
 * Hash table resizing -> double the capacity, find the closest greater prime number (to guarantee probe coverage)
 * Rehash existing entries into the new HTable (new capacity changes order of entries)
 *
 * Helper function for insertEntry() - it's only use case
 */
static void resizeScope(HTable *scope)
{
    size_t old_capacity = scope->capacity;
    HTEntry *old_entries = scope->entries;
    size_t new_capacity = nextPrime(scope->capacity * 2);

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

    // Memory cleanup of old HTable
    for (; old_capacity > 0; old_capacity--)
    {
        if (old_entries[old_capacity - 1].state != EMPTY)
            free(old_entries[old_capacity - 1].identifier);
    }
    free(old_entries);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Searches for entry of specified entry_type (FUNCTION/VARIABLE) in HTable
 * Returns TRUE if found, FALSE otherwise
 *
 * Implemented mainly for use in isIdentifier()
 * Please use isIdentifier() for resolving identifiers
 */
bool isEntry(const char *key, HTEntryType entry_type, HTable *table)
{
    size_t index_hash = DJBHash(key);
    size_t probe_step_hash = BKDRHash(key);
    HTEntry *target_entry = &table->entries[index_hash % table->capacity];

    // Double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1)
    // table->capacity == MAX+1
    for (size_t i = 1; target_entry->state != EMPTY; i++) // while not at end of synonyms list
    {
        if (target_entry->state != DELETED) // if valid entry
        {
            // if found specified entry
            if (target_entry->entry_type == entry_type && strcmp(key, target_entry->identifier) == 0)
                return true;
            target_entry = &table->entries[(index_hash + i * probe_step_hash) % table->capacity];
        }
    }

    return false;
}
/*
 * Searches for entry of specified entry_type (FUNCTION/VARIABLE) in table
 * Returns pointer to entry if found, NULL otherwise
 *
 * Implemented mainly for use in getIdentifier()
 * Please use getIdentifier() for resolving identifiers
 */
HTEntry *getEntry(const char *key, HTEntryType entry_type, HTable *table)
{
    size_t index_hash = DJBHash(key);
    size_t probe_step_hash = BKDRHash(key);
    HTEntry *target_entry = &table->entries[index_hash % (table->capacity)];

    // Double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1)
    // scope->capacity == MAX+1
    for (size_t i = 1; target_entry->state != EMPTY; i++)
    {
        if (target_entry->state != DELETED)
        {
            // if found specified entry
            if (target_entry->entry_type == entry_type && strcmp(key, target_entry->identifier) == 0)
                return target_entry;
            target_entry = &table->entries[(index_hash + i * probe_step_hash) % table->capacity];
        }
    }

    return NULL;
}

/*
 * Resolves specified identifier based on its lexical scope
 * Identifiers are bound in the local scope they're defined in and its subblocks (nested inner local scopes)
 * -> Searches for identifier starting with current local scope towards global scope
 *
 * Function assumes correctly initialized sym_table (at least has global scope)
 * Returns boolean TRUE if found, FALSE otherwise
 */
bool isIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table)
{
    for (size_t scope_index = sym_table->current_scope; scope_index >= 0; scope_index--)
    {
        if (isEntry(identifier, entry_type, &sym_table->scopes[scope_index]))
            return true;
    }
    return false;
}
/*
 * Resolves specified identifier based on its lexical scope
 * Identifiers are bound in the local scope they're defined in and its subblocks (nested inner local scopes)
 * -> Searches for identifier starting with current local scope towards global scope
 *
 * Function assumes correctly initialized sym_table (at least has global scope)
 * Returns pointer to the identifier entry in symTable if found, NULL otherwise
 */
HTEntry *getIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table)
{
    HTEntry *target_identifier = NULL;
    for (size_t scope_index = sym_table->current_scope; scope_index >= 0; scope_index--)
    {
        target_identifier = getEntry(identifier, entry_type, &sym_table->scopes[scope_index]);
        if (target_identifier != NULL)
            return target_identifier;
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Inserts specified entry into specified table (~ scope)
 * Implemented as Double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1) (scope->capacity == MAX+1)
 *
 * Call ONLY if isEntry(entry->identifier, entry->entry_type, table) == FALSE
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

    // Find EMPTY/DELETED place in HTable (HTable synonyms list)
    for (size_t i = 1; target_entry->state == BUSY; i++)
        target_entry = &table->entries[(index_hash + i * probe_step_hash) % table->capacity];

    size_t str_len = strlen(entry->identifier) + 1; // account for '\0' that strlen() doesn't count
    target_entry->identifier = calloc(str_len, sizeof(char *));
    if (target_entry->identifier == NULL)
    {
        fprintf(stderr, "insertEntry() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    // Insert specified entry into correct HTable index
    // Avoids shallow copy - non-pointer struct fields passed by value, pointer struct fields handled separately
    strncpy(target_entry->identifier, entry->identifier, str_len); // Rest of target_entry->identifier filled with '\0'
    target_entry->entry_type = entry->entry_type;
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
