#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdint.h>

typedef struct HTEntry HTEntry;
typedef struct HTable HTable;
typedef struct SymTable SymTable;

uint32_t DJBHash(const char *key);
uint32_t BKDRHash(const char *key);

SymTable createSymTable();
void destroySymTable(SymTable *sym_table);

void createScope(SymTable *sym_table);
void destroyScope(SymTable *sym_table);

void resizeSymTable(SymTable *sym_table);
void resizeScope(HTable *scope);

#endif
