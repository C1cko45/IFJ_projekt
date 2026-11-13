#include "gen_def.h"
#include <stdio.h>



void generate_frame_enter(CodegenContext *ctx){
    if(ctx == NULL || ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output, "CREATEFRAME\n");
    fprintf(ctx->output, "PUSHFRAME\n");
}


void generate_frame_leave(CodegenContext *ctx){
    if(ctx == NULL || ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output, "POPFRAME\n");
}


static void generate_function(AST_NODE *node, CodegenContext *ctx){
    if(node == NULL || ctx == NULL || ctx->error_code != 0){
        return;
    }

    AST_NODE *function_node = node;

    if(node->type == NODE_MAIN){
        function_node = node->left_ch;

        if(function_node == NULL || function_node->type != NODE_FUNC_DEF){
            ctx->error_code = 99;
            return;
        }
    }

    AST_NODE *function_definicion = function_node->left_ch;
    AST_NODE *function_body = function_node->right_ch;

    if(function_definicion == NULL){
        ctx->error_code = 99;
        return;
    }

    //placeholder
}





//FB0



