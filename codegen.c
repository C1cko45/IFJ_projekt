#include "codegen.h"
#include "gen_def.h"
#include "gen_nondef.h"

#include <stdio.h>
#include <stdlib.h>



static void generate_node(AST_NODE *node, CodegenContext *ctx);
static void generate_program(AST_NODE *node, CodegenContext *ctx);

static void generate_call(AST_NODE *node, CodegenContext *ctx);

static void generate_if(AST_NODE *node, CodegenContext *ctx);
static void generate_while(AST_NODE *node, CodegenContext *ctx);
static void generate_for(AST_NODE *node, CodegenContext *ctx);

static void generate_val_def(AST_NODE *node, CodegenContext *ctx);
static void generate_var_def(AST_NODE *node, CodegenContext *ctx);

static void generate_token(AST_NODE *node, CodegenContext *ctx);

static void generate_valmem(AST_NODE *node, CodegenContext *ctx);
static void generate_list(AST_NODE *node, CodegenContext *ctx);

static void generate_expression(AST_NODE *node, CodegenContext *ctx);



void codegen_emit_header(CodegenContext *ctx){
    fprintf(ctx->output, ".IFJcode26\n");
}

void codegen_emit_label(CodegenContext *ctx, const char *label){
    fprintf(ctx->output, "LABEL %s\n", label);
}

void codegen_emit_jump(CodegenContext *ctx, const char *label){
    fprintf(ctx->output, "JUMP %s\n", label);
}


int codegen_generate(AST_NODE *root, FILE *output){
    if(root == NULL || output == NULL){
        return 99;
    }

    CodegenContext ctx = {
        .output = output,
        .label_counter = 0,
        .temp_counter = 0,
        .error_code = 0
    };

    codegen_emit_header(&ctx);
    generate_node(root, &ctx);

    return ctx.error_code;
}


static void generate_node(AST_NODE *node, CodegenContext *ctx){
    if(node == NULL || ctx->error_code != 0){
        return;
    }

    switch(node->type){
        case NODE_PROGRAM:
            generate_program(node, ctx);
            break;
    
        case NODE_MAIN:
            generate_function(node, ctx);
            break;

        case NODE_FUNC_DEF:
            generate_function(node, ctx);
            break;

        case NODE_CALL:
            generate_call(node, ctx);
            break;

        case NODE_IF:
            generate_if(node, ctx);
            break;

        case NODE_WHILE:
            generate_while(node, ctx);
            break;

        case NODE_FOR:
            generate_for(node ,ctx);
            break;

        case NODE_VAR_DEF:
        case NODE_VAL_DEF:
            generate_definition(node, ctx);
            break;

        case NODE_TOKEN_ONLY:
            generate_token(node, ctx);
            break;

        case NODE_VALMEM:
            generate_valmem(node, ctx);
            break;

        case NODE_LIST:
            generate_list(node, ctx);
            break;

        default:
        //placeholder
            break;
    }
}


void generate_program(AST_NODE *node, CodegenContext *ctx){
    if(node == NULL || ctx->error_code != 0){
        return;
    }

    generate_node(node->left_ch, ctx);

    if(ctx->error_code != 0){
        return;
    }

    generate_program(node->right_ch, ctx);
}





//FB0