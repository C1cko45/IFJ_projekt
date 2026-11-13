#ifndef GEN_NONDEF_H
#define GEN_NONDEF_H

#include "codegen.h"

void generate_expression(AST_NODE *node, CodegenContext *ctx);
void generate_definition(AST_NODE *node, CodegenContext *ctx);
void generate_assignment(AST_NODE *identifier, AST_NODE *expression, CodegenContext *ctx);



#endif





//FB0