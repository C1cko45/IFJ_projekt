/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Data Type
typedef enum
{         /* calloc() initializes all fields to 0/NULL */
    UNIT, // -> initial value, void type
    ANY,  // placeholder type, indicates value of any type

    // Primitive types
    INT,
    DOUBLE,
    STRING,
    BOOLEAN,

    // Composite types
    LIST
} DataType;

// Entry Type
typedef enum
{         /* calloc() initializes all fields to 0/NULL */
    NONE, // -> initial value

    FUNCTION,
    MAIN_FUNCTION, // @main-tagged function

    VAR,
    VAL // immutable, includes FUNCTION_PARAMETER; they behave the same in function's scope
} HTEntryType;

// Entry State
typedef enum
{          /* calloc() initializes all fields to 0/NULL */
    EMPTY, // -> initial value

    BUSY,
    DELETED
} HTEntryState;

// Entry Data
typedef union HTEntryData
{
    // HTEntryType == VAR|VAL
    DataType variable_data_type;

    // HTEntryType == FUNCTION
    struct
    {
        size_t param_count;
        DataType return_type;
    } function;
} HTEntryData;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * IFJ26 supports function and variable with the same identifier (doesn't lead to re-defition error)
 * -> identifier and entry_type (function/variable) both become keys
 * Although function identifiers should only be in global_scope anyway, the only type of entries there
 */
typedef struct HTEntry
{ /* calloc() initializes all fields to 0/NULL */
    HTEntryState state;

    char *identifier;       // key#1
    HTEntryType entry_type; // key#2|tag

    HTEntryData entry_data; // tagged union
} HTEntry;

/* Lexical scope */
typedef struct HTable
{ /* calloc() initializes all fields to 0/NULL */
    size_t capacity;
    size_t count;
    HTEntry *entries; // The hash array of identifiers in the scope

    struct HTable *outer_scope; // pointer to its outer scope; helps for searching through the SymTable hierarchically
} HTable;

typedef struct SymTable
{                           /* calloc() initializes all fields to 0/NULL */
    size_t scopes_capacity; // 1 (global scope) + function_count + temp_count

    // local scopes, effectively in correct IFJ26 program, are only function body blocks
    // ordered corresponds to order of function definitions in IFJ26 program
    size_t function_count;   // number of local scopes; in correct IFJ26 program should stay constant (= local_capacity)
    size_t function_current; // current active local scope index

    // temporary scopes; self-contained scopes nested inside function body blocks (blocks in while, if-else, for, ...)
    // -> within temp_scope, its outer function scope is visible
    // temp_scopes within a function scope grouped together; ordered according to order of function definitions
    // in IFJ26 program
    bool temp_active; // indicates when we're currently in a temp scope
    size_t temp_count;
    size_t temp_current;

    // Array of all scopes[1 + local_count + temp_count] (HTables)
    // scopes[0] -> global_scope
    // scopes[1..local_count] -> local_scopes
    // scopes[(local_count+1)..(local_count+temp_count)] -> temp_scopes
    struct HTable *scopes; // Array of HTables (scopes)
} SymTable;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SymTable *createSymTable(size_t capacity);
void destroySymTable(SymTable *sym_table);

void createScope(SymTable *sym_table);
void destroyScope(SymTable *sym_table);

bool isEntry(const char *key, HTEntryType entry_type, HTable *table);
HTEntry *getEntry(const char *key, HTEntryType entry_type, HTable *table);

bool isIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table);
HTEntry *getIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table);

void insertEntry(HTEntry *entry, HTable *table);
bool deleteEntry(HTEntry *entry, HTable *table);

#endif
