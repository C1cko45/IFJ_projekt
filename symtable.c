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

static uint32_t DJBHash(const char *key);
static uint32_t BKDRHash(const char *key);

static bool isPrime(size_t number);
static size_t nextPrime(size_t number);

static void resizeSymTable(SymTable *sym_table);
static void resizeScope(HTable *scope);

static void insertEntry(HTEntry *entry, HTable *table);

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
 * Returns SymTable initialized (but empty) with global scope
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

    // Initialize symTable's struct fields
    sym_table->function_count = 0;
    sym_table->function_current = 0;
    sym_table->temp_count = 0;
    sym_table->temp_active = false;
    sym_table->temp_current = 0;

    // Initialize global scope
    sym_table->scopes[GLOBAL_SCOPE_INDEX].entries = calloc(HTABLE_INIT_CAPACITY, sizeof(HTEntry));
    if (sym_table->scopes[GLOBAL_SCOPE_INDEX].entries == NULL)
    {
        fprintf(stderr, "createScope() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }
    sym_table->scopes[GLOBAL_SCOPE_INDEX].count = HTABLE_INIT_CAPACITY;
    sym_table->scopes[GLOBAL_SCOPE_INDEX].capacity = HTABLE_INIT_CAPACITY;
    sym_table->scopes[GLOBAL_SCOPE_INDEX].outer_scope_index = 0;

    return sym_table;
}
/*
 * Destroys the whole SymTable
 * ~> Free memory of every scope, every scopes Hash array, set capacity and count to 0
 */
void destroySymTable(SymTable *sym_table)
{
    // Destroy all existing scopes
    // This particular way avoids size_t underflow, that would cause infinite loop otherwise
    size_t scopes_count = 1 + sym_table->function_count + sym_table->temp_count;
    while (scopes_count > 0)
        destroyScope(&sym_table->scopes[--scopes_count]);

    free(sym_table->scopes);
    sym_table->scopes = NULL;
    sym_table->scopes_capacity = 0;
    sym_table->function_count = 0;
    sym_table->function_current = 0;
    sym_table->temp_count = 0;
    sym_table->temp_current = 0;
    sym_table->temp_active = false;
}
/*
 * Grows symTable, should be called only when symTable becomes full
 * Dynamic array semantics -> double the capacity
 *
 * Helper function for createScope()
 */
static void resizeSymTable(SymTable *sym_table)
{
    // Assigning to tmp variable -> if realloc() failed, we would lose access to original unfreed memory
    HTable *new_scopes = realloc(sym_table->scopes, sizeof(HTable) * sym_table->scopes_capacity * 2);
    if (new_scopes == NULL)
    {
        fprintf(stderr, "resizeSymTable() -- realloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }

    sym_table->scopes = new_scopes;
    sym_table->scopes_capacity *= 2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Creates and initializes function scope after function_current scope in SymTable
 * Resizes the SymTable if needed
 *
 * Mainly helper function used in semantic analysis when initializing SymTable
 * -> At start of semantic analysis, it goes through the whole program to count num of functions
 *  and creates that many function scopes in SymTable, that num should not change for duration of SymTable's lifetime
 */
void createFunctionScope(SymTable *sym_table)
{
    // scopes_count + 1 > scopes_capacity -> grow SymTable
    if ((sym_table->function_count + sym_table->temp_count + 1) + 1 > sym_table->scopes_capacity)
        resizeSymTable(sym_table);

    // Initialize created scope
    sym_table->scopes[sym_table->function_current + 1].entries = calloc(HTABLE_INIT_CAPACITY, sizeof(HTEntry));
    if (sym_table->scopes[sym_table->function_current + 1].entries == NULL)
    {
        fprintf(stderr, "createFunctionScope() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }
    sym_table->scopes[sym_table->function_current + 1].count = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->function_current + 1].capacity = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->function_current + 1].outer_scope_index = GLOBAL_SCOPE_INDEX;

    // Update symTable function_scopes info
    sym_table->function_count++;
    sym_table->function_current++;
}
/*
 * Creates and initializes temp scope after temp_current scope in SymTable
 * Resizes the SymTable if needed
 *
 * Mainly helper function for enterTempScope() -> should not be invoked on its own
 */
void createTempScope(SymTable *sym_table)
{
    // scopes_count + 1 > scopes_capacity -> grow SymTable
    if ((sym_table->function_count + sym_table->temp_count + 1) + 1 > sym_table->scopes_capacity)
        resizeSymTable(sym_table);

    // Initialize created scope
    sym_table->scopes[sym_table->temp_current + 1].entries = calloc(HTABLE_INIT_CAPACITY, sizeof(HTEntry));
    if (sym_table->scopes[sym_table->temp_current + 1].entries == NULL)
    {
        fprintf(stderr, "createTempScope() -- calloc() failed.\n");
        // TODO: cleanup(); -- must free AST memory
        exit(ERROR_INTERNAL);
    }
    sym_table->scopes[sym_table->temp_current + 1].count = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->temp_current + 1].capacity = HTABLE_INIT_CAPACITY;
    sym_table->scopes[sym_table->temp_current + 1].outer_scope_index = GLOBAL_SCOPE_INDEX;

    // Update symTable tem_scopes info
    sym_table->temp_count++;
    sym_table->temp_current++;
}
/*
 * Destroys specified scope in SymTable
 *
 * Mainly a helper function for destroySymTable()
 * -> Should only be called then
 */
void destroyScope(HTable *scope)
{
    // free memory of all non-empty entries in HTable
    // This particular way avoids size_t underflow, that would cause infinite loop otherwise
    while (scope->capacity > 0)
        if (scope->entries[scope->capacity - 1].state != EMPTY)
            free(scope->entries[--scope->capacity].identifier);

    free(scope->entries);
    scope->entries = NULL;
    scope->count = 0;
    scope->capacity = 0;
    scope->outer_scope_index = 0;
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
    for (size_t index = 0; index < scope->count; index++)
        insertEntry(&old_entries[index], scope); // rehash

    // Memory cleanup of old HTable
    // This particular way avoids size_t underflow, that would cause infinite loop otherwise
    while (old_capacity > 0)
        if (old_entries[old_capacity - 1].state != EMPTY)
            free(old_entries[--old_capacity].identifier);

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
static bool isEntry(const char *key, HTEntryType entry_type, HTable *table)
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
static HTEntry *getEntry(const char *key, HTEntryType entry_type, HTable *table)
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
 * Resolves identifier based on lexical scope rules of IFJ26
 * Identifiers are valid from place of definition until end of block (current scope) including its subblocks
 * -> Searches for identifier starting with current local scope towards global scope
 *
 * Function assumes correctly initialized sym_table (at least has global scope)
 * Returns boolean TRUE if found, FALSE otherwise
 */
bool isIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table)
{
    if (entry_type == FUNCTION)
        return isEntry(identifier, entry_type, &sym_table->scopes[GLOBAL_SCOPE_INDEX]);

    if (entry_type != NONE) // entry_type == VAR||VAL
    {                       // Check every scope except global scope (where only function identifiers should be)
        if (sym_table->temp_active)
        {
            for (size_t index = sym_table->temp_current; index > 0; index = sym_table->scopes[index].outer_scope_index)
            {
                if (isEntry(identifier, entry_type, &sym_table->scopes[index]))
                    return true;
            }
        }

        for (size_t index = sym_table->function_current; index > 0; index = sym_table->scopes[index].outer_scope_index)
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
    if (entry_type == FUNCTION)
        return getEntry(identifier, entry_type, &sym_table->scopes[GLOBAL_SCOPE_INDEX]);

    if (entry_type != NONE) // entry_type == VAR||VAL
    {                       // Check every scope except global scope (where only function identifiers should be)
        HTEntry *target_identifier = NULL;
        if (sym_table->temp_active)
        {
            for (size_t index = sym_table->temp_current; index > 0; index = sym_table->scopes[index].outer_scope_index)
            {
                target_identifier = getEntry(identifier, entry_type, &sym_table->scopes[index]);
                if (target_identifier != NULL)
                    return target_identifier;
            }
        }

        for (size_t index = sym_table->function_current; index > 0; index = sym_table->scopes[index].outer_scope_index)
        {
            target_identifier = getEntry(identifier, entry_type, &sym_table->scopes[index]);
            if (target_identifier != NULL)
                return target_identifier;
        }
    }

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Inserts specified entry into specified table (~ scope)
 * Uses double-hash function: h(k, i) = (h1(k) + i*h2(k)) % (MAX+1) (scope->capacity == MAX+1)
 *
 * Assumes no function/variable re-definition in current scope
 * -> CALL ONLY if isEntry(entry->identifier, entry->entry_type, table) == FALSE
 * Mainly a helper function for insertIdentifier() -> to push identifiers into symTable, use insertIdentifier()
 */
static void insertEntry(HTEntry *entry, HTable *table)
{
    // If inserting exceeds load_factor (0.7) -> grow HTable, rehash
    // COUNT+1 > MAX_COUNT
    // MAX_COUNT = (CAPACITY * 0.7) * 10 -- To avoid floating-points
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
 *
 * Should only be called when deleting the whole SymTable (normal SymTable use-case does not require deleting entries)
 * Returns TRUE if successful, FALSE otherwis
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
