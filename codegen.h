#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stddef.h>
#include "ast.h"



typedef enum{
    CG_TYPE_INT,
    CG_TYPE_DOUBLE,
    CG_TYPE_STRING,
    CG_TYPE_BOOL,
    CG_TYPE_UNIT,
    CG_TYPE_LIST,
    CG_TYPE_UNKNOWN
} CodegenType;


typedef struct{
    const char *name;
    CodegenType type;
} CodegenSymbol;


typedef struct CodegenBinding CodegenBinding;


typedef struct{
    FILE *output;

    unsigned int label_counter;
    unsigned int temp_counter;

    int error_code;

    const CodegenSymbol *symbols;
    size_t symbol_count;

    int inside_function;
    int current_is_main;
    CodegenType current_return_type;

    unsigned int variable_counter;
    CodegenBinding *bindings;

    int force_list_double;
    int current_function_list_double;

} CodegenContext;


CodegenType codegen_builtin_return_type(const AST_NODE *node);
CodegenType codegen_from_semantic_type(DataType type);

void codegen_generate_builtin_expression(AST_NODE *node, CodegenContext *ctx);
void codegen_generate_call_expression(AST_NODE *node, CodegenContext *ctx);

int codegen_generate(AST_NODE *root, FILE *output);
int codegen_generate_ctx(AST_NODE *root, CodegenContext *ctx);
void codegen_generate_node(AST_NODE *node, CodegenContext *ctx);


void codegen_emit_header(CodegenContext *ctx);
void codegen_emit_label(CodegenContext *ctx, const char *label);
void codegen_emit_jump(CodegenContext *ctx, const char *label);

void generate_return(AST_NODE *node, CodegenContext *ctx);

const char *codegen_variable_name(const AST_NODE *identifier, CodegenContext *ctx);

CodegenType codegen_list_item_type(const AST_NODE *identifier, const CodegenContext *ctx);

void codegen_set_list_item_type(const AST_NODE *identifier, CodegenContext *ctx, CodegenType type);



#endif





//FB0