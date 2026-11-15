#include "codegen.h"
#include "gen_def.h"
#include "gen_nondef.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



static void generate_program(AST_NODE *node, CodegenContext *ctx);
static void generate_call(AST_NODE *node, CodegenContext *ctx);
static void generate_while(AST_NODE *node, CodegenContext *ctx);
static void generate_if(AST_NODE *node, CodegenContext *ctx);



void codegen_emit_header(CodegenContext *ctx){
    if(ctx == NULL || ctx->output == NULL){
        return;
    }

    if(fputs(".IFJcode26\n"
                "DEFVAR GF@%cg_tmp\n"
                "JUMP $$main\n", ctx->output) == EOF){
        ctx->error_code = 99;
    }
}

void codegen_emit_label(CodegenContext *ctx, const char *label){
    fprintf(ctx->output, "LABEL %s\n", label);
}

void codegen_emit_jump(CodegenContext *ctx, const char *label){
    fprintf(ctx->output, "JUMP %s\n", label);
}


int codegen_generate_ctx(AST_NODE *root, CodegenContext *ctx){
    if(root == NULL || ctx == NULL || ctx->output == NULL){
        return 99;
    }

    if(ctx->error_code != 0){
        return ctx->error_code;
    }

    codegen_emit_header(ctx);

    if(ctx->error_code == 0){
        codegen_generate_node(root ,ctx);
    }

    if(ferror(ctx->output)){
        ctx->error_code = 99;
    }

    return ctx->error_code;
}


int codegen_generate(AST_NODE *root, FILE *output){
    CodegenContext ctx = {0};
    ctx.output = output;

    return codegen_generate_ctx(root, &ctx);
}


void codegen_generate_node(AST_NODE *node, CodegenContext *ctx){
    if(node == NULL || ctx->error_code != 0){
        return;
    }

    switch(node->type){
        case NODE_PROGRAM:
            generate_program(node, ctx);
            break;
    
        case NODE_MAIN:
        case NODE_FUNC_DEF:
            generate_function(node, ctx);
            break;

        case NODE_CALL:
            generate_call(node, ctx);
            break;

        case NODE_VAR_DEF:
        case NODE_VAL_DEF:
            generate_definition(node, ctx);
            break;

        case NODE_TOKEN_ONLY:
            if(node->token.type == KEYWORD_RETURN){
                generate_return(node,ctx);
            }
            
            else if(node->token.type == TOKEN_TYPE_ASSIGN){
                generate_assignment(node->left_ch, node->right_ch, ctx);
            }

            else{
                generate_expression(node, ctx);

                if(ctx->error_code == 0){
                    if(fputs("POPS GF@%cg_tmp\n", ctx->output) == EOF){
                        ctx->error_code = 99;
                    }
                }
            }
            break;

        case NODE_IF:
            generate_if(node, ctx);
            break;

        case NODE_WHILE:
            generate_while(node, ctx);
            break;

        default:
            ctx->error_code = 99;
            break;
    }
}


static void generate_program(AST_NODE *node, CodegenContext *ctx){
    if(node == NULL || ctx == NULL || ctx->error_code != 0){
        return;
    }

    codegen_generate_node(node->left_ch, ctx);

    if(ctx->error_code != 0){
        return;
    }

    if(node->right_ch != NULL){
        generate_program(node->right_ch, ctx);
    }
}


static int generate_call_arguments(AST_NODE *args, CodegenContext *ctx){
    if(ctx == NULL || ctx->error_code != 0){
        return -1;
    }

    if(args == NULL){
        return 0;
    }

    if(args->type != NODE_PROGRAM){
        generate_expression(args, ctx);
        return ctx->error_code == 0 ? 1 : -1;
    }

    int count = 0;

    for(AST_NODE *current = args; current != NULL; current = current->right_ch){
        if(current->type != NODE_PROGRAM || current->left_ch == NULL){
            ctx->error_code = 99;
            return -1;
        }

        generate_expression(current->left_ch, ctx);

        if(ctx->error_code != 0){
            return -1;
        }

        count++;
    }

    return count;
}


static void generate_call(AST_NODE *node, CodegenContext *ctx){
    AST_NODE *name_node = node->left_ch;

    if(name_node == NULL || name_node->type != NODE_TOKEN_ONLY ||
        name_node->token.type != TOKEN_TYPE_IDENTIFIER ||
        name_node->token.attr.str_val == NULL){
            ctx->error_code = 99;
            return;
    }

    const char *name = name_node->token.attr.str_val;

    if(strcmp(name, "print") == 0){
        AST_NODE *arg = node->right_ch;

        if(arg != NULL && arg->type == NODE_PROGRAM){
            if(arg->left_ch == NULL || arg->right_ch != NULL){
                ctx->error_code = 99;
                return;
            }

            arg = arg->left_ch;
        }

        if(arg == NULL){
            ctx->error_code = 99;
            return;
        }

        CodegenType arg_type = codegen_expression_type(arg, ctx);

        generate_expression(arg, ctx);

        if(ctx->error_code != 0){
            return;
        }

        if(arg_type == CG_TYPE_UNIT){
            fputs("POPS GF@%cg_tmp\n"
                "WRITE string@()\n", ctx->output);
        }

        else{
            fputs("POPS GF@%cg_tmp\n"
                "WRITE GF@%cg_tmp\n", ctx->output);
        }
    }

    else if(codegen_builtin_return_type(node) != CG_TYPE_UNKNOWN){
        codegen_generate_builtin_expression(node, ctx);

        if(ctx->error_code == 0){
            if(fputs("POPS GF@%cg_tmp\n", ctx->output) == EOF){
                ctx->error_code = 99;
            }
        }
    }

    else{
        int count = generate_call_arguments(node->right_ch, ctx);

        if(count < 0 || ctx->error_code != 0){
            return;
        }

        fprintf(ctx->output, "CALL $%s\n", name);

        if(fputs("POPS GF@%cg_tmp\n", ctx->output) == EOF){
            ctx->error_code = 99;
        }
    }
}


static const char *get_call_name(const AST_NODE *node){
    if(node == NULL || node->type != NODE_CALL || node->left_ch == NULL ||
        node->left_ch->type != NODE_TOKEN_ONLY || node->left_ch->token.type != TOKEN_TYPE_IDENTIFIER){
            return NULL;
    }

    return node->left_ch->token.attr.str_val;
}


CodegenType codegen_builtin_return_type(const AST_NODE *node){
    const char *name = get_call_name(node);

    if(name == NULL){
        return CG_TYPE_UNKNOWN;
    }

    if(strcmp(name, "readInt") == 0 ||
        strcmp(name, "toInt") == 0 ||
        strcmp(name, "strLength") == 0 ||
        strcmp(name, "ord") == 0){
            return CG_TYPE_INT;
    }

    if(strcmp(name, "readDouble") == 0){
        return CG_TYPE_DOUBLE;
    }

    if(strcmp(name, "readLine") == 0 ||
        strcmp(name, "toStr") == 0 ||
        strcmp(name, "chr") == 0 ||
        strcmp(name, "subStr") == 0){
            return CG_TYPE_STRING;
    }

    if(strcmp(name, "readBoolean") == 0){
        return CG_TYPE_BOOL;
    }

    return CG_TYPE_UNKNOWN;
}


static int get_builtin_arguments(AST_NODE *args, AST_NODE **out, int expected){
    if(args == NULL || out == NULL || expected < 1){
        return 0;
    }

    if(args->type != NODE_PROGRAM){
        if(expected != 1){
            return 0;
        }

        out[0] = args;
        return 1;
    }

    int count = 0;

    for(AST_NODE *current = args; current != NULL; current = current->right_ch){
        if(current->type != NODE_PROGRAM ||
            current->left_ch == NULL ||
            count >= expected){
            return 0;
        }

        out[count++] = current->left_ch;
    }

    return count == expected;
}



static void emit_builtin_ord(CodegenContext *ctx){
    unsigned int id = ctx->label_counter++;

    fputs("CREATEFRAME\n"
        "PUSHFRAME\n"
        "DEFVAR LF@%cg_text\n"
        "DEFVAR LF@%cg_index\n"
        "DEFVAR LF@%cg_length\n"
        "DEFVAR LF@%cg_result\n"
        "DEFVAR LF@%cg_check\n"
        "POPS LF@%cg_index\n"
        "POPS LF@%cg_text\n"
        "STRLEN LF@%cg_length LF@%cg_text\n"
        "MOVE LF@%cg_result int@-1\n"
        "LT LF@%cg_check LF@%cg_index int@0\n", ctx->output);

    fprintf(ctx->output,
        "JUMPIFEQ $$cg_ord_end_%u LF@%%cg_check bool@true\n", id);

    fputs(
        "LT LF@%cg_check LF@%cg_index LF@%cg_length\n", ctx->output);

    fprintf(ctx->output,
        "JUMPIFEQ $$cg_ord_end_%u LF@%%cg_check bool@false\n", id);

    fputs(
        "STRI2INT LF@%cg_result LF@%cg_text LF@%cg_index\n", ctx->output);

    fprintf(ctx->output, "LABEL $$cg_ord_end_%u\n", id);

    fputs("PUSHS LF@%cg_result\n"
        "POPFRAME\n", ctx->output);
}



static void emit_builtin_substr(CodegenContext *ctx){
    unsigned int id = ctx->label_counter++;

    fputs("CREATEFRAME\n"
        "PUSHFRAME\n"
        "DEFVAR LF@%cg_text\n"
        "DEFVAR LF@%cg_start\n"
        "DEFVAR LF@%cg_end\n"
        "DEFVAR LF@%cg_length\n"
        "DEFVAR LF@%cg_index\n"
        "DEFVAR LF@%cg_char\n"
        "DEFVAR LF@%cg_result\n"
        "DEFVAR LF@%cg_check\n"
        "POPS LF@%cg_end\n"
        "POPS LF@%cg_start\n"
        "POPS LF@%cg_text\n"
        "STRLEN LF@%cg_length LF@%cg_text\n", ctx->output);

    fputs("LT LF@%cg_check LF@%cg_start int@0\n", ctx->output);
    
    fprintf(ctx->output, "JUMPIFEQ $$cg_sub_invalid_%u LF@%%cg_check bool@true\n", id);

    fputs("LT LF@%cg_check LF@%cg_end int@0\n", ctx->output);
    
    fprintf(ctx->output, "JUMPIFEQ $$cg_sub_invalid_%u LF@%%cg_check bool@true\n", id);

    fputs("GT LF@%cg_check LF@%cg_start LF@%cg_end\n", ctx->output);
   
    fprintf(ctx->output, "JUMPIFEQ $$cg_sub_invalid_%u LF@%%cg_check bool@true\n", id);

    fputs("LT LF@%cg_check LF@%cg_start LF@%cg_length\n", ctx->output);
   
    fprintf(ctx->output, "JUMPIFEQ $$cg_sub_invalid_%u LF@%%cg_check bool@false\n", id);

    fputs("GT LF@%cg_check LF@%cg_end LF@%cg_length\n", ctx->output);
    
    fprintf(ctx->output, "JUMPIFEQ $$cg_sub_invalid_%u LF@%%cg_check bool@true\n", id);

    fputs("MOVE LF@%cg_result string@\n"
        "MOVE LF@%cg_index LF@%cg_start\n", ctx->output);

    fprintf(ctx->output, "LABEL $$cg_sub_loop_%u\n", id);

    fputs("LT LF@%cg_check LF@%cg_index LF@%cg_end\n", ctx->output);

    fprintf(ctx->output, "JUMPIFEQ $$cg_sub_finish_%u LF@%%cg_check bool@false\n", id);

    fputs("GETCHAR LF@%cg_char LF@%cg_text LF@%cg_index\n"
        "CONCAT LF@%cg_result LF@%cg_result LF@%cg_char\n"
        "ADD LF@%cg_index LF@%cg_index int@1\n", ctx->output);

    fprintf(ctx->output, "JUMP $$cg_sub_loop_%u\n"
                        "LABEL $$cg_sub_invalid_%u\n"
                        "EXIT int@40\n"
                        "LABEL $$cg_sub_finish_%u\n", id, id, id);

    fputs("PUSHS LF@%cg_result\n"
        "POPFRAME\n", ctx->output);
}


void codegen_generate_builtin_expression(AST_NODE *node, CodegenContext *ctx){
    if(ctx == NULL || ctx->error_code != 0){
        return;
    }

    if(ctx->output == NULL || codegen_builtin_return_type(node) == CG_TYPE_UNKNOWN){
        ctx->error_code = 99;
        return;
    }

    const char *name = get_call_name(node);


    const char *read_type = NULL;
    const char *fallback = NULL;

    if(strcmp(name, "readInt") == 0){
        read_type = "int";
        fallback = "int@0";
    }

    else if(strcmp(name, "readDouble") == 0){
        read_type = "float";
        fallback = "float@0x0p+0";
    }

    else if(strcmp(name, "readLine") == 0){
        read_type = "string";
        fallback = "string@";
    }

    else if(strcmp(name, "readBoolean") == 0){
        read_type = "bool";
        fallback = "bool@false";
    }

    if(read_type != NULL){
        if(node->right_ch != NULL){
            ctx->error_code = 99;
            return;
        }

        unsigned int id = ctx->label_counter++;

        fprintf(ctx->output, "READ GF@%%cg_tmp %s\n"
                            "JUMPIFNEQ $$cg_read_ok_%u GF@%%cg_tmp nil@nil\n"
                            "MOVE GF@%%cg_tmp %s\n"
                            "LABEL $$cg_read_ok_%u\n"
                            "PUSHS GF@%%cg_tmp\n", read_type, id, fallback, id);
        
        return;
    }

    if(strcmp(name, "ord") == 0 || strcmp(name, "subStr") == 0){
        int is_ord = strcmp(name, "ord") == 0;

        int count = is_ord ? 2 : 3;

        AST_NODE *args[3] = {NULL, NULL, NULL};

        if(!get_builtin_arguments(node->right_ch, args, count)){
            ctx->error_code = 99;
            return;
        }

        if(codegen_expression_type(args[0], ctx) != CG_TYPE_STRING){
            ctx->error_code = 99;
            return;
        }

        for(int i = 1; i < count; i++){
            if(codegen_expression_type(args[i], ctx) != CG_TYPE_INT){
                ctx->error_code = 99;
                return;
            }
        }

        for(int i = 0; i < count; i++){
            generate_expression(args[i], ctx);

            if(ctx->error_code != 0){
                return;
            }
        }

        if(is_ord){
            emit_builtin_ord(ctx);
        }
        else{
            emit_builtin_substr(ctx);
        }

        return;
    }


    AST_NODE *arg = node->right_ch;

    if(arg != NULL && arg->type == NODE_PROGRAM){
        if(arg->left_ch == NULL || arg->right_ch != NULL){
            ctx->error_code = 99;
            return;
        }

        arg = arg->left_ch;
    }

    if(arg == NULL){
        ctx->error_code = 99;
        return;
    }

    CodegenType type = codegen_expression_type(arg, ctx);

    if(type == CG_TYPE_UNKNOWN || (strcmp(name, "toInt") == 0 && type != CG_TYPE_DOUBLE) ||
        (strcmp(name, "strLength") == 0 && type != CG_TYPE_STRING) ||
        (strcmp(name, "chr") == 0 && type != CG_TYPE_INT) ||
        (strcmp(name, "toStr") == 0 && type == CG_TYPE_LIST)){
            ctx->error_code = 99;
            return;
    }

    generate_expression(arg, ctx);

    if(ctx->error_code != 0){
        return;
    }

    if(strcmp(name, "toInt") == 0){
        fputs("FLOAT2INTS\n", ctx->output);
    }

    else if(strcmp(name, "strLength") == 0){
        fputs("POPS GF@%cg_tmp\n"
                "STRLEN GF@%cg_tmp GF@%cg_tmp\n"
                "PUSHS GF@%cg_tmp\n", ctx->output);
    }

    else if(strcmp(name, "chr") == 0){
        fputs("INT2CHARS\n", ctx->output);
    }

    else if(strcmp(name, "toStr") == 0){
        switch(type){
            case CG_TYPE_INT:
                fputs("INT2STRS\n", ctx->output);
                break;

            case CG_TYPE_DOUBLE:
                fputs("FLOAT2STRS\n", ctx->output);
                break;

            case CG_TYPE_BOOL:{
                unsigned int id = ctx->label_counter++;

                fprintf(ctx->output, "PUSHS bool@false\n"
                                    "JUMPIFEQS $$cg_bool_false_%u\n"
                                    "PUSHS string@true\n"
                                    "JUMP $$cg_bool_end_%u\n"
                                    "LABEL $$cg_bool_false_%u\n"
                                    "PUSHS string@false\n"
                                    "LABEL $$cg_bool_end_%u\n", id, id, id, id);
                break;
            }

            case CG_TYPE_STRING:
                //placeholder
                break;

            case CG_TYPE_UNIT:
                fputs("POPS GF@%cg_tmp\n"
                    "PUSHS string@()\n", ctx->output);
                break;

            default:
                    ctx->error_code = 99;
                    break;
            
        }
    }
}


static void generate_while(AST_NODE *node, CodegenContext *ctx){
    if(ctx == NULL || ctx->error_code != 0){
        return;
    }

    if(node == NULL || ctx->output == NULL || node->left_ch == NULL || node->right_ch == NULL){
        ctx->error_code = 99;
        return;
    }

    AST_NODE *condition = node->left_ch;
    AST_NODE *body = node->right_ch;

    unsigned int id = ctx->label_counter++;

    fprintf(ctx->output, "LABEL $$while_start_%u\n", id);

    generate_expression(condition, ctx);

    if(ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output, "PUSHS bool@false\n");
    fprintf(ctx->output, "JUMPIFEQS $$while_end_%u\n", id);

    codegen_generate_node(body, ctx);

    if(ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output, "JUMP $$while_start_%u\n", id);
    fprintf(ctx->output, "LABEL $$while_end_%u\n", id);

}


static void generate_if(AST_NODE *node, CodegenContext *ctx){
    if(node == NULL || ctx == NULL || ctx->error_code != 0){
        return;
    }

    AST_NODE *pair = node->left_ch;

    if(pair == NULL || pair->type != NODE_PROGRAM || pair->left_ch == NULL ||
        pair->right_ch == NULL){
            ctx->error_code = 99;
            return;
    }

    unsigned int id = ctx->label_counter++;

    generate_expression(pair->left_ch, ctx);

    if(ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output, "PUSHS bool@false\n"
                        "JUMPIFEQS $$if_else_%u\n", id);

    codegen_generate_node(pair->right_ch, ctx);

    if(ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output, "JUMP $$if_end_%u\n"
                        "LABEL $$if_else_%u\n", id, id);

    if(node->right_ch != NULL){
        codegen_generate_node(node->right_ch, ctx);
        
        if(ctx->error_code != 0){
            return;
        }
    }

    fprintf(ctx->output, "LABEL $$if_end_%u\n", id);
}





//FB0