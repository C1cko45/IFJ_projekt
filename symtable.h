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
{
    INT,
    DOUBLE,
    STRING,
    BOOL,
    UNIT,
    LIST
} VarType;
typedef enum
{
    EMPTY,
    BUSY
} EntryState;

typedef struct HTEntry
{
    EntryState state;
    char *identifier; // key
    // type
    // value
} HTEntry;
/* Lexical scope */
typedef struct HTable
{
    size_t capacity;
    size_t count;
    struct HTEntry *entries;
} HTable;

typedef struct SymTable
{
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

bool isEntry(const char *key, HTable *scope);
HTEntry *getEntry(const char *key, HTable *scope);
void insertEntry(HTEntry *entry, HTable *scope);

#endif
