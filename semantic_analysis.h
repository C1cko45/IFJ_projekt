/*
 * Implementácia prekladača pre jazyk IFJ26.
 *
 * Rozhranie sémantickej analýzy (typové kontroly nad AST).
 *
 * Autor:   Pham Vu Duy Anh
 * Login:   xphamv00
 */
#ifndef SEMANTIC_ANALYSIS_H
#define SEMANTIC_ANALYSIS_H

#include "ast.h"
#include "symtable.h"

void semanticAnalysis(AST_NODE *root_ast);

#endif
