/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdint.h>
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum
{         /* calloc() initializes all fields to 0/NULL */
    UNIT, // -> initial value

    INT,
    DOUBLE,
    STRING,
    BOOL,

    LIST
} DataType;
typedef struct FunctionParameter
{
    char *identifier;
    DataType data_type;

} FunctionParameter;

typedef enum
{         /* calloc() initializes all fields to 0/NULL */
    NONE, // -> initial value

    FUNCTION,
    VAR,
    VAL
} HTEntryType;
typedef enum
{          /* calloc() initializes all fields to 0/NULL */
    EMPTY, // -> initial value

    BUSY,
    DELETED
} HTEntryState;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef union HTEntryData
{
    DataType variable_data_type;

    struct
    {
        size_t param_count;
        FunctionParameter *params;
    } function;
} HTEntryData;
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
    size_t current;

    struct HTable *scopes;
} SymTable;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t DJBHash(const char *key);
uint32_t BKDRHash(const char *key);

SymTable createSymTable();
void resizeSymTable(SymTable *sym_table);
void destroySymTable(SymTable *sym_table);

void createScope(SymTable *sym_table);
void resizeScope(HTable *scope);
void destroyScope(SymTable *sym_table);

bool isEntry(const char *key, HTEntryType entry_type, HTable *table);
HTEntry *getEntry(const char *key, HTEntryType entry_type, HTable *table);

void insertEntry(HTEntry *entry, HTable *table);
bool deleteEntry(HTEntry *entry, HTable *table);

bool isIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table);
HTEntry *getIdentifier(const char *identifier, HTEntryType entry_type, SymTable *sym_table);

#endif
