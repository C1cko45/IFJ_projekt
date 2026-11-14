#include "gen_def.h"
#include "gen_nondef.h"
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


static void generate_parameters_reverse(AST_NODE *param, CodegenContext *ctx){
    if(param == NULL || ctx == NULL || ctx->error_code != 0){
        return;
    }

    if(param->type != NODE_TOKEN_ONLY || param->token.type != TOKEN_TYPE_IDENTIFIER ||
        param->token.attr.str_val == NULL){
            ctx->error_code = 99;
            return;
    }

    generate_parameters_reverse(param->right_ch, ctx);

    if(ctx->error_code != 0){
        return;
    }

    const char *name = param->token.attr.str_val;

    fprintf(ctx->output, "DEFVAR LF@%s\n", name);
    fprintf(ctx->output, "POPS LF@%s\n", name);
}

static CodegenType get_function_return_type(AST_NODE *node){
    if(node == NULL || node->type != NODE_TOKEN_ONLY || node->token.type != TOKEN_TYPE_KEYWORD){
        return CG_TYPE_UNKNOWN;
    }

    switch(node->token.attr.keyword){
        case KEYWORD_INT:
            return CG_TYPE_INT;

        case KEYWORD_DOUBLE:
            return CG_TYPE_DOUBLE;

        case KEYWORD_STRING:
            return CG_TYPE_STRING;

        case KEYWORD_BOOLEAN:
            return CG_TYPE_BOOL;

        case KEYWORD_UNIT:
            return CG_TYPE_UNIT;

        default:
            return CG_TYPE_UNKNOWN;
    }
}


void generate_function(AST_NODE *node, CodegenContext *ctx){
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

    if(function_node->type != NODE_FUNC_DEF ||
        function_definicion->type != NODE_TOKEN_ONLY ||
        function_definicion->token.type != TOKEN_TYPE_IDENTIFIER ||
        function_definicion->token.attr.str_val == NULL ||
        function_body == NULL){
            ctx->error_code = 99;
            return;
    }

    AST_NODE *return_type = function_definicion->left_ch;

    CodegenType declare_type = get_function_return_type(return_type);

    if(declare_type == CG_TYPE_UNKNOWN){
        ctx->error_code = 99;
        return;
    }

    int is_main = (node->type == NODE_MAIN);

    if(is_main && declare_type != CG_TYPE_UNIT){
        ctx->error_code = 99;
        return;
    }

    int old_inside_function = ctx->inside_function;
    int old_is_main = ctx->current_is_main;
    CodegenType old_return_type = ctx->current_return_type;

    ctx->inside_function = 1;
    ctx->current_is_main = is_main;
    ctx->current_return_type = declare_type;

    if(is_main){
        fprintf(ctx->output, "LABEL $$main\n");
    }

    else{
        fprintf(ctx->output, "LABEL $%s\n", function_definicion->token.attr.str_val);
    }

    generate_frame_enter(ctx);

    generate_parameters_reverse(function_definicion->right_ch, ctx);

    if(ctx->error_code == 0){
        codegen_generate_node(function_body, ctx);
    }

    if(ctx->error_code == 0){
        generate_frame_leave(ctx);

        if(is_main){
            fprintf(ctx->output, "EXIT int@0\n");
        }

        else{
            fprintf(ctx->output, "RETURN\n");
        }
    }

    ctx->inside_function = old_inside_function;
    ctx->current_is_main = old_is_main;
    ctx->current_return_type = old_return_type;
}


void generate_return(AST_NODE *node, CodegenContext *ctx){
    if(node == NULL || ctx == NULL || ctx->error_code != 0){
        return;
    }

    if(!ctx->inside_function){
        ctx->error_code = 99;
        return;
    }

    AST_NODE *value = node->left_ch;

    if(node->right_ch != NULL){
        ctx->error_code = 99;
        return;
    }

    if(ctx->current_return_type == CG_TYPE_UNIT){
        if(value != NULL){
            generate_expression(value, ctx);

            if(ctx->error_code != 0){
                return;
            }

            fputs("POPS GF@%cg_tmp\n", ctx->output);
        }
    }

    else{
        if(value == NULL){
            ctx->error_code = 99;
            return;
        }

        CodegenType actual_type = codegen_expression_type(value, ctx);

        int convert = ctx->current_return_type == CG_TYPE_DOUBLE && actual_type == CG_TYPE_INT;

        if(actual_type != ctx->current_return_type && !convert){
            ctx->error_code = 99;
            return;
        }

        if(convert){
            fputs("INT2FLOATS\n", ctx->output);
        }
    }

    generate_frame_leave(ctx);

    if(ctx->current_is_main){
        fputs("EXIT int@0\n", ctx->output);
    }

    else{
        fputs("RETURN\n", ctx->output);
    }
}




//FB0



