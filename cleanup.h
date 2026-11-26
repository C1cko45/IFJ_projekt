/*
 * IFJ26
 * Autor: Pham Vu Duy Anh, xphamv00
 */

#ifndef CLEANUP_H
#define CLEANUP_H

#include "ast.h"
#include "symtable.h"

#define ERROR_UNDEF 3     // use of undef function/var/type; missing @main-tagged function
#define ERROR_REDEF 4     // illegal re-def function/var/type
#define ERROR_PARAM 5     // incorrect count/type of arguments in function call; illegal @main-tagged function params
#define ERROR_TYPE 6      // type incompat | incorrect type infer in expr, assign, init, return type, control flow
#define ERROR_IMMUTABLE 7 // assign attempt to immutable variable/function parameter
#define ERROR_OTHER 10    // other semantic errors; (illegal return, non-exhaustive match, range with 0-step, ...)
#define ERROR_INTERNAL 99 // internal program error; failed memory allocation, ...

void registerAST(AST_NODE *root_ast);
void registerSymTable(SymTable *sym_table);

void compilerError(int error_code);

#endif
