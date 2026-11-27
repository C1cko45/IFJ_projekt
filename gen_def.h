#ifndef GEN_DEF_H
#define GEN_DEF_H

#include "codegen.h"



//Vygeneruje celu funkciu
void generate_function(AST_NODE *node, CodegenContext *ctx);
void generate_frame_enter(CodegenContext *ctx);
void generate_frame_leave(CodegenContext *ctx);

#endif




//FB0