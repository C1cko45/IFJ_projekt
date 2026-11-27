/*
 * Implementácia prekladača pre jazyk IFJ26.
 *
 * Generovanie definícií funkcií a práce s rámcami.
 *
 * Autor:   Richard Veselka
 * Login:   xveselr00
 */
#ifndef GEN_DEF_H
#define GEN_DEF_H

#include "codegen.h"

// Vygeneruje celu funkciu
void generate_function(AST_NODE *node, CodegenContext *ctx);
void generate_frame_enter(CodegenContext *ctx);
void generate_frame_leave(CodegenContext *ctx);

#endif

// FB0