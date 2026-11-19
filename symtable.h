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
{
    NIL, // -> initial value, arbitrary sym_table type specifically to indicate unspecified function return type
    ANY, // Placeholder for function return type annotations; this cannot be assigned to a variable

    UNIT, // Indicates lack of value, values should default to this type

    // Primitive types
    INT,
    DOUBLE,
    STRING,
    BOOLEAN,

    // Composite types
    LIST_INT,
    LIST_DOUBLE,
    LIST_STRING,
    LIST_BOOLEAN,
    LIST_UNIT,
    LIST_ANY // Placeholder for function return type annotations; this cannot be assigned to a variable
} DataType;

// Entry Type
typedef enum
{
    NONE, // -> initial value

    FUNCTION,

    VAR,
    VAL // immutable, includes FUNCTION_PARAMETER; they behave the same in function's scope
} HTEntryType;

// Entry State
typedef enum
{
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
        DataType *parameters;
        DataType return_type; // return_type == NIL -> no explicit return type in function signature
    } function;
} HTEntryData;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * IFJ26 supports function and variable with the same identifier (doesn't lead to re-defition error)
 * -> identifier and entry_type (function/variable) both become keys
 * Although function identifiers should only be in global_scope anyway, the only type of entries there
 */
typedef struct HTEntry
{
    HTEntryState state;

    char *identifier;       // key#1
    HTEntryType entry_type; // key#2|tag

    HTEntryData entry_data; // tagged union
} HTEntry;

/*
 * Lexical scope
 * Scope is implemented as a hash table with open addressing (double-hashing)
 */
typedef struct HTable
{
    size_t capacity;
    size_t count;
    HTEntry *entries; // scope's hash array of identifiers
} HTable;

/*
 * Symbol Table implemented as a stack of scopes
 * the stack is implemented as a dynamic array
 */
typedef struct SymTable
{
    size_t capacity; // stack's max size
    size_t current;  // stack pointer

    struct HTable *scopes; // Stack of scopes
} SymTable;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SymTable *createSymTable();
void destroySymTable(SymTable *sym_table);

void pushScope(SymTable *sym_table);
void popScope(SymTable *sym_table);

bool isEntry(const char *key, HTEntryType entry_type, HTable *table);
HTEntry *getEntry(const char *key, HTEntryType entry_type, HTable *table);

void createIdentifier(HTEntry *entry, SymTable *sym_table);
bool changeVarType(const char *identifier, DataType target_type, SymTable *sym_table);
bool isIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table);
HTEntry *getIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table);

#endif
