#ifndef GEN_NONDEF_H
#define GEN_NONDEF_H

#include "codegen.h"

void generate_expression(AST_NODE *node, CodegenContext *ctx);
void generate_definition(AST_NODE *node, CodegenContext *ctx);
void generate_assignment(AST_NODE *identifier, AST_NODE *expression, CodegenContext *ctx);

CodegenType codegen_expression_type(const AST_NODE *node, const CodegenContext *ctx);

void codegen_list_range_from_stack(CodegenContext *ctx, int inclusive);

void codegen_generate_list_method_expression(AST_NODE *node, CodegenContext *ctx);


#endif





//FB0