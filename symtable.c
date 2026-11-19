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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ERROR_INTERNAL 99 // internal program error; failed memory allocation, ...

#define HTABLE_MAX_LOAD_FACTOR 7 // Load factor*10, to avoid floating-point conversion and arithmetic
#define HTABLE_INIT_CAPACITY 29  // Min value to satisfy load factor for initial global scope - has 19 builtin functions

#define SYMTABLE_INIT_CAPACITY 1
#define GLOBAL_SCOPE_INDEX 0

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t DJBHash(const char *key);
static uint32_t BKDRHash(const char *key);

static bool isPrime(size_t number);
static size_t nextPrime(size_t number);

static void resizeSymTable(SymTable *sym_table);
static void resizeScope(HTable *scope);

static void insertEntry(HTEntry *entry, HTable *table);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Returns SymTable with initialized (but empty) global scope
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

    // Create symTable's global scope
    sym_table->scopes = calloc(SYMTABLE_INIT_CAPACITY, sizeof(HTable));
    if (sym_table->scopes == NULL)
    {
        fprintf(stderr, "createSymTable() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    // Initialize symTable
    sym_table->capacity = 1;
    sym_table->current = 0;

    // Initialize global scope
    sym_table->scopes[GLOBAL_SCOPE_INDEX].entries = calloc(HTABLE_INIT_CAPACITY, sizeof(HTEntry));
    if (sym_table->scopes[GLOBAL_SCOPE_INDEX].entries == NULL)
    {
        fprintf(stderr, "createSymTable() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }
    sym_table->scopes[GLOBAL_SCOPE_INDEX].count = HTABLE_INIT_CAPACITY;
    sym_table->scopes[GLOBAL_SCOPE_INDEX].capacity = HTABLE_INIT_CAPACITY;

    return sym_table;
}
/*
 * Destroys the whole SymTable
 * ~> Free memory of every scope's hash array, free the stack, set stack's to uninitialized
 */
void destroySymTable(SymTable *sym_table)
{
    if (sym_table == NULL)
    {
        fprintf(stderr, "destroySymTable() -- passed argument sym_table is NULL.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    // Destroy all existing scopes
    // This particular way avoids size_t underflow, that would cause infinite loop otherwise
    sym_table->capacity = sym_table->current + 1;
    while (sym_table->capacity > 0)
        popScope(sym_table);

    free(sym_table->scopes);
    sym_table->scopes = NULL;
    sym_table->capacity = 0;
    sym_table->current = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Creates, initializes and pushes new current scope onto sym_table's stack
 * Resizes sym_table if needed
 */
void pushScope(SymTable *sym_table)
{
    if (sym_table == NULL)
    {
        fprintf(stderr, "pushScope() -- passed argument sym_table is NULL.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    // scopes_count + 1 > scopes_capacity -> grow SymTable
    if ((sym_table->current + 1) + 1 > sym_table->capacity)
        resizeSymTable(sym_table);

    // Initialize created scope
    sym_table->scopes[sym_table->current + 1].entries = calloc(HTABLE_INIT_CAPACITY, sizeof(HTEntry));
    if (sym_table->scopes[sym_table->current + 1].entries == NULL)
    {
        fprintf(stderr, "pushScope() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }
    sym_table->scopes[sym_table->current + 1].count = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->current + 1].capacity = HTABLE_INIT_CAPACITY;

    // Update sym_table's stack pointer
    sym_table->current++;
}
/*
 * Pops current scope off the sym_table's stack
 */
void popScope(SymTable *sym_table)
{
    if (sym_table == NULL)
    {
        fprintf(stderr, "popScope() -- passed argument sym_table is NULL.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    HTable *scope = &sym_table->scopes[sym_table->current];
    // free memory of all non-empty entries in scope (HTable)
    // This particular way avoids size_t underflow, that would cause infinite loop otherwise
    while (scope->capacity > 0)
        if (scope->entries[scope->capacity - 1].state != EMPTY)
            free(scope->entries[--scope->capacity].identifier);

    free(scope->entries);
    scope->entries = NULL;
    scope->count = 0;
    scope->capacity = 0;

    // Update sym_table's stack pointer
    sym_table->current--;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Searches for entry of specified entry_type (FUNCTION/VARIABLE) in HTable
 * Returns TRUE if found, FALSE otherwise
 *
 * Mainly used for checking for function/variable re-definition (searches local scope only)
 * For resolving identifiers, see isIdentifier()
 */
bool isEntry(const char *key, HTEntryType entry_type, HTable *table)
{
    if (key == NULL || table == NULL || entry_type == NONE)
    {
        fprintf(stderr, "isEntry() -- at least one argument is invalid.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    size_t index_hash = DJBHash(key);
    size_t probe_step_hash = BKDRHash(key);
    HTEntry *target_entry = &table->entries[index_hash % table->capacity];

    // Double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1)
    // table->capacity == MAX+1
    for (size_t i = 1; target_entry->state != EMPTY; ++i) // while not at end of synonyms list
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
 * Mainly used for checking for function/variable re-definition (searches local scope only)
 * For resolving identifiers, see isIdentifier()
 */
HTEntry *getEntry(const char *key, HTEntryType entry_type, HTable *table)
{
    if (key == NULL || table == NULL || entry_type == NONE)
    {
        fprintf(stderr, "getEntry() -- at least one argument is invalid.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    size_t index_hash = DJBHash(key);
    size_t probe_step_hash = BKDRHash(key);
    HTEntry *target_entry = &table->entries[index_hash % (table->capacity)];

    // Double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1)
    // scope->capacity == MAX+1
    for (size_t i = 1; target_entry->state != EMPTY; ++i)
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Creates identifier entry in SymTable's current scope
 * Check for possible variable/function re-definition with isEntry() before use
 */
void createIdentifier(HTEntry *entry, SymTable *sym_table)
{
    if (entry == NULL || sym_table == NULL)
    {
        fprintf(stderr, "createIdentifier() -- at least one argument is invalid.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    insertEntry(entry, &sym_table->scopes[sym_table->current]);
}
/*
 * Finds specified local immutable variable and updates its data type
 *
 * Should be called when assigning value to a mutable variable that previously held value of different data type
 * Function assumes existence of specified variable identifier visible in current scope and that it is mutable
 *
 * Returns TRUE if found such a variable and changed data type, otherwise FALSE
 */
bool changeVarType(const char *identifier, DataType target_type, SymTable *sym_table)
{
    if (identifier == NULL || sym_table == NULL || target_type == ANY)
    {
        fprintf(stderr, "changeVarType() -- at least one argument is invalid.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    HTEntry *target_entry = getIdentifier(identifier, VAR, sym_table);

    if (target_entry != NULL)
    {
        target_entry->entry_data.variable_data_type = target_type;
        return true;
    }

    return false;
}

/*
 * Resolves identifier based on lexical scope rules of IFJ26
 * Identifiers are valid from place of definition until end of block (current scope) including its subblocks
 * -> Searches for identifier starting with current local scope towards global scope
 *
 * Function assumes correctly initialized sym_table (at least has global scope)
 * Returns TRUE if found, FALSE otherwise
 */
bool isIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table)
{
    if (identifier == NULL || sym_table == NULL || entry_type == NONE)
    {
        fprintf(stderr, "isIdentifier() -- at least one argument is invalid.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    if (entry_type == FUNCTION)
        return isEntry(identifier, entry_type, &sym_table->scopes[GLOBAL_SCOPE_INDEX]);

    if (entry_type != NONE) // entry_type == VAR||VAL
    {
        // Check every scope except global scope (which should contain only function names)
        // IFJ26 doesn't support global variables
        for (size_t index = sym_table->current; index > 0; --index)
        {
            if (isEntry(identifier, entry_type, &sym_table->scopes[index]))
                return true;
        }
    }

    return false;
}
/*
 * Resolves identifier based on lexical scope rules of IFJ26
 * Identifiers are valid from place of definition until end of block (current scope) including its subblocks
 * -> Searches for identifier starting with current local scope towards global scope
 *
 * Function assumes correctly initialized sym_table (at least has global scope)
 * Returns pointer to the identifier entry in symTable if found, NULL otherwise
 */
HTEntry *getIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table)
{
    if (identifier == NULL || sym_table == NULL || entry_type == NONE)
    {
        fprintf(stderr, "isIdentifier() -- at least one argument is invalid.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    if (entry_type == FUNCTION)
        return getEntry(identifier, entry_type, &sym_table->scopes[GLOBAL_SCOPE_INDEX]);

    if (entry_type != NONE) // entry_type == VAR||VAL
    {
        // Check every scope except global scope (which should contain only function names)
        // IFJ26 doesn't support global variables
        HTEntry *target_identifier = NULL;
        for (size_t index = sym_table->current; index > 0; --index)
        {
            target_identifier = getEntry(identifier, entry_type, &sym_table->scopes[index]);
            if (target_identifier != NULL)
                return target_identifier;
        }
    }

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* DJB2 Hash */
static uint32_t DJBHash(const char *key)
{
    if (key == NULL)
    {
        fprintf(stderr, "DJBHash() -- Passed argument is NULL.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    uint32_t hash = 5381;

    int c;
    while ((c = (*key++)))
        hash = hash * 33 + c;

    return hash;
}
/* BKDR Hash */
static uint32_t BKDRHash(const char *key)
{
    if (key == NULL)
    {
        fprintf(stderr, "DJBHash() -- Passed argument is NULL.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    uint32_t hash = 0;

    while (*key)
        hash = hash * 131 + (*key++);

    return hash;
}

/*
 * nextPrime() helper function
 * Returns boolean indicating if number is a prime
 * Implements trial-division principle
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

/*
 * Grows symTable, should be called only when symTable becomes full
 * Dynamic array semantics -> double the capacity
 *
 * Mainly helper function for pushScope()
 */
static void resizeSymTable(SymTable *sym_table)
{
    if (sym_table == NULL)
    {
        fprintf(stderr, "resizeSymTable() -- argument sym_table is NULL.\n");
        // TODO: cleanup();
        exit(ERROR_INTERNAL);
    }

    // Assigning to tmp variable -> if realloc() failed, we would lose access to original unfreed memory
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

/*
 * Grow hash table -> double the capacity, find the closest greater prime number (to guarantee full probe coverage)
 * Rehash existing entries into the new HTable (new capacity changes order of existing entries)
 *
 * Should only be called when count exceeds HTABLE_MAX_LOAD_FACTOR
 * Helper function for insertEntry() - it's only use case
 */
static void resizeScope(HTable *scope)
{
    if (scope == NULL)
    {
        fprintf(stderr, "resizeScope() -- passed argument scope is NULL.\n");
        // TODO: cleanup();
        exit(ERROR_INTERNAL);
    }

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
    for (size_t index = 0; index < scope->count; ++index)
        insertEntry(&old_entries[index], scope); // rehash

    // Memory cleanup of old HTable
    // This particular way avoids size_t underflow, that would cause infinite loop otherwise
    while (old_capacity > 0)
        if (old_entries[old_capacity - 1].state != EMPTY)
            free(old_entries[--old_capacity].identifier);

    free(old_entries);
}

/*
 * Inserts entry into table (~ scope)
 * Uses double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1) (scope->capacity == MAX+1)
 *
 * Check for possible variable/function re-definition with isEntry() before use
 * Mainly a helper function for createIdentifier()
 * To push identifiers into symTable, use createIdentifier()
 */
static void insertEntry(HTEntry *entry, HTable *table)
{
    if (entry == NULL || table == NULL)
    {
        fprintf(stderr, "insertEntry() -- at least one argument is invalid.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    // If inserting exceeds load_factor (0.7) -> grow HTable, rehash
    // COUNT+1 > MAX_COUNT
    // MAX_COUNT = (CAPACITY * 0.7) * 10 -- To avoid floating-points
    if ((table->count + 1) * 10 > (table->capacity * HTABLE_MAX_LOAD_FACTOR))
        resizeScope(table);

    size_t index_hash = DJBHash(entry->identifier);
    size_t probe_step_hash = BKDRHash(entry->identifier);
    HTEntry *target_entry = &table->entries[index_hash % (table->capacity)];

    // Find EMPTY/DELETED place in HTable (HTable synonyms list)
    for (size_t i = 1; target_entry->state == BUSY; ++i)
        target_entry = &table->entries[(index_hash + i * probe_step_hash) % table->capacity];

    size_t str_len = strlen(entry->identifier) + 1; // account for '\0' that strlen() doesn't count
    target_entry->identifier = calloc(str_len, sizeof(char));
    if (target_entry->identifier == NULL)
    {
        fprintf(stderr, "insertEntry() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    // Insert specified entry into correct HTable index
    // Avoids shallow copy -- non-pointer struct fields passed by value, pointer struct fields handled separately
    strncpy(target_entry->identifier, entry->identifier, str_len); // Rest of target_entry->identifier filled with '\0'
    target_entry->entry_type = entry->entry_type;
    target_entry->entry_data = entry->entry_data;
    target_entry->state = BUSY;
}
