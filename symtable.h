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
    FUNCTION_PARAMETER, // immutable, behaves like VAL

    VAR,
    VAL // immutable
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
    // HTEntryType == VAR|VAL|FUNCTION_PARAMETER
    DataType variable_data_type;

    // HTEntryType == FUNCTION
    struct
    {
        size_t param_count;
        DataType return_type;
    } function;
} HTEntryData;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct HTEntry
{ /* calloc() initializes all fields to 0/NULL */
    HTEntryState state;

    // IFJ26 supports function and variable with the same identifier (doesn't lead to re-defition error)
    // -> identifier and entry_type (function/variable) both become keys
    char *identifier;       // key#1
    HTEntryType entry_type; // key#2|tag

    HTEntryData entry_data; // tagged union
} HTEntry;

/* Lexical scope */
typedef struct HTable
{ /* calloc() initializes all fields to 0/NULL */
    size_t capacity;
    size_t count;

    struct HTEntry *entries;
} HTable;

typedef struct SymTable
{ /* calloc() initializes all fields to 0/NULL */
    size_t capacity;
    size_t count;
    size_t current_scope;

    struct HTable *scopes;
} SymTable;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SymTable *createSymTable();
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
