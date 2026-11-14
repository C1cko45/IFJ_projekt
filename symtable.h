/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdint.h>
#include <stdlib.h>

typedef struct HTEntry HTEntry;
typedef struct HTable HTable;
typedef struct SymTable SymTable;

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
