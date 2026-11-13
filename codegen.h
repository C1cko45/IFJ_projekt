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


typedef struct{
    FILE *output;

    unsigned int label_counter;
    unsigned int temp_counter;

    int error_code;

    const CodegenSymbol *symbols;
    size_t symbol_count;

} CodegenContext;


int codegen_generate(AST_NODE *root, FILE *output);


void codegen_emit_header(CodegenContext *ctx);
void codegen_emit_label(CodegenContext *ctx, const char *label);
void codegen_emit_jump(CodegenContext *ctx, const char *label);


#endif





//FB0