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
static void generate_for(AST_NODE *node, CodegenContext *ctx);
static void generate_for_list(AST_NODE *node, CodegenContext *ctx);
static void generate_list_constructor(AST_NODE *node, CodegenContext *ctx);
static void generate_list_index(AST_NODE *node, CodegenContext *ctx);



struct CodegenBinding {
    const HTEntry *entry;
    CodegenType list_item_type;
    char name[40];
    struct CodegenBinding *next;
};



static CodegenType codegen_element_type_from_datatype(DataType type){
    switch(type){
        case LIST_INT:
            return CG_TYPE_INT;

        case LIST_DOUBLE:
            return CG_TYPE_DOUBLE;

        case LIST_STRING:
            return CG_TYPE_STRING;

        case LIST_BOOLEAN:
            return CG_TYPE_BOOL;

        case LIST_UNIT:
            return CG_TYPE_UNIT;

        default:
            return CG_TYPE_UNKNOWN;
    }
}


static CodegenType codegen_declared_list_item_type(const AST_NODE *identifier){
    if(identifier == NULL || identifier->left_ch == NULL ||
        identifier->left_ch->type != NODE_LIST || identifier->left_ch->left_ch == NULL){
            return CG_TYPE_UNKNOWN;
    }

    switch(identifier->left_ch->left_ch->token.type){
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


void codegen_set_list_item_type(const AST_NODE *identifier, CodegenContext *ctx, CodegenType type){
    if(identifier == NULL || identifier->identifier_entry == NULL || type == CG_TYPE_UNKNOWN){
        return;
    }

    codegen_variable_name(identifier, ctx);

    for(CodegenBinding *binding = ctx->bindings; binding != NULL; binding = binding->next){
        if(binding->entry == identifier->identifier_entry){
            binding->list_item_type = type;
            return;
        }
    }
}


CodegenType codegen_list_item_type(const AST_NODE *identifier, const CodegenContext *ctx){
    if(identifier == NULL || ctx == NULL){
        return CG_TYPE_UNKNOWN;
    }

    const HTEntry *entry = identifier->identifier_entry;

    if(identifier->type == NODE_CALL && identifier->left_ch != NULL &&
       identifier->left_ch->type == NODE_TOKEN_ONLY && identifier->left_ch->token.type == TOKEN_TYPE_IDENTIFIER){
            entry = identifier->left_ch->identifier_entry;
    }

    if(entry != NULL){
        DataType semantic_type;

        if(entry->entry_type == VAR || entry->entry_type == VAL){
            semantic_type = entry->entry_data.variable_data_type;
        }

        else if(entry->entry_type == FUNCTION){

            semantic_type = entry->entry_data.function.return_type;
        }

        else{
            semantic_type = ANY;
        }

        CodegenType element_type = codegen_element_type_from_datatype(semantic_type);

        if(element_type != CG_TYPE_UNKNOWN){
            return element_type;
        }
    }

    if(identifier->type == NODE_VALMEM && identifier->right_ch != NULL){

        const char *member = identifier->right_ch->token.attr.str_val;

        if(member != NULL){
            if(strcmp(member, "tail") == 0){
                return codegen_list_item_type(identifier->left_ch, ctx);
            }

            if(strcmp(member, "toList") == 0){
                return CG_TYPE_INT;
            }
        }
    }

    if(identifier->type == NODE_CALL && identifier->left_ch != NULL){
        const AST_NODE *callee = identifier->left_ch;

        if(callee->type == NODE_VALMEM && callee->right_ch != NULL){
            const AST_NODE *receiver = callee->left_ch;

            const char *method = callee->right_ch->token.attr.str_val;

            if(method != NULL){
                if(strcmp(method, "range") == 0 && receiver != NULL &&
                   receiver->type == NODE_TOKEN_ONLY && receiver->token.type == KEYWORD_LIST){
                    return CG_TYPE_INT;
                }

                if(strcmp(method, "take") == 0 || strcmp(method, "drop") == 0 || strcmp(method, "slice") == 0){
                    return codegen_list_item_type(receiver, ctx);
                }
            }
        }

        if(callee->type == NODE_TOKEN_ONLY && callee->token.type == KEYWORD_LIST){

            CodegenType item_type = CG_TYPE_UNKNOWN;

            for(const AST_NODE *arg = identifier->right_ch; arg != NULL && arg->left_ch != NULL; arg = arg->right_ch){
                CodegenType current = codegen_expression_type(arg->left_ch, ctx);

                if(current == CG_TYPE_UNKNOWN){
                    return CG_TYPE_UNKNOWN;
                }

                if(item_type == CG_TYPE_UNKNOWN){
                    item_type = current;
                }

                else if((item_type == CG_TYPE_INT && current == CG_TYPE_DOUBLE) ||
                    (item_type == CG_TYPE_DOUBLE && current == CG_TYPE_INT)){
                        item_type = CG_TYPE_DOUBLE;
                }

                else if(item_type != current){
                    return CG_TYPE_UNKNOWN;
                }
            }

            return item_type;
        }
    }

    if(identifier->type == NODE_TOKEN_ONLY && identifier->token.type == TOKEN_TYPE_CONCAT){
        CodegenType left = codegen_list_item_type(identifier->left_ch, ctx);

        CodegenType right = codegen_list_item_type(identifier->right_ch, ctx);

        return left == right ? left : CG_TYPE_UNKNOWN;
    }

    
    CodegenType declared = codegen_declared_list_item_type(identifier);

    if(declared != CG_TYPE_UNKNOWN){
        return declared;
    }

    if(identifier != NULL && identifier->identifier_entry != NULL){
        for(CodegenBinding *binding = ctx->bindings; binding != NULL; binding = binding->next){
            if(binding->entry == identifier->identifier_entry){
                return binding->list_item_type;
            }
        }
    }

    return CG_TYPE_UNKNOWN;
}


const char *codegen_variable_name(const AST_NODE *identifier, CodegenContext *ctx){
    if(identifier->identifier_entry == NULL){
        return identifier->token.attr.str_val;
    }

    CodegenType element_type = codegen_declared_list_item_type(identifier);

    for(CodegenBinding *binding = ctx->bindings; binding != NULL; binding = binding->next){
        if(binding->entry == identifier->identifier_entry){
            if(element_type != CG_TYPE_UNKNOWN){
                binding->list_item_type = element_type;
            }

            return binding->name;
        }
    }

    CodegenBinding *binding = malloc(sizeof(*binding));

    if(binding == NULL){
        ctx->error_code = 99;
        return NULL;
    }

    binding->entry = identifier->identifier_entry;
    binding->list_item_type = element_type;

    snprintf(binding->name, sizeof(binding->name), "%%cg_v%u", ctx->variable_counter++);

    binding->next = ctx->bindings;
    ctx->bindings = binding;

    return binding->name;
}


static void codegen_emit_list_to_string_runtime(CodegenContext *ctx){
    fputs("LABEL $$cg_list_to_str\n"
        "CREATEFRAME\nPUSHFRAME\n"
        "DEFVAR LF@%cg_a\n"
        "DEFVAR LF@%cg_len\n"
        "DEFVAR LF@%cg_i\n"
        "DEFVAR LF@%cg_v\n"
        "DEFVAR LF@%cg_ty\n"
        "DEFVAR LF@%cg_s\n"
        "DEFVAR LF@%cg_out\n"
        "DEFVAR LF@%cg_cmp\n"

        "POPS LF@%cg_a\n"
        "ARRAYGET LF@%cg_len LF@%cg_a int@0\n"
        "MOVE LF@%cg_out string@List(\n"
        "MOVE LF@%cg_i int@1\n"

        "LABEL $$cg_lst_loop\n"
        "GT LF@%cg_cmp LF@%cg_i LF@%cg_len\n"
        "JUMPIFEQ $$cg_lst_done LF@%cg_cmp bool@true\n"

        "JUMPIFEQ $$cg_lst_no_sep LF@%cg_i int@1\n"
        "CONCAT LF@%cg_out LF@%cg_out string@,\\032\n"
        "LABEL $$cg_lst_no_sep\n"

        "ARRAYGET LF@%cg_v LF@%cg_a LF@%cg_i\n"
        "TYPE LF@%cg_ty LF@%cg_v\n"

        "JUMPIFEQ $$cg_lst_int LF@%cg_ty string@int\n"
        "JUMPIFEQ $$cg_lst_float LF@%cg_ty string@float\n"
        "JUMPIFEQ $$cg_lst_bool LF@%cg_ty string@bool\n"
        "JUMPIFEQ $$cg_lst_nil LF@%cg_ty string@nil\n"
        "JUMPIFEQ $$cg_lst_array LF@%cg_ty string@array\n"

        "MOVE LF@%cg_s LF@%cg_v\n"
        "JUMP $$cg_lst_append\n"

        "LABEL $$cg_lst_int\n"
        "INT2STR LF@%cg_s LF@%cg_v\n"
        "JUMP $$cg_lst_append\n"

        "LABEL $$cg_lst_float\n"
        "FLOAT2STR LF@%cg_s LF@%cg_v\n"
        "JUMP $$cg_lst_append\n"

        "LABEL $$cg_lst_bool\n"
        "JUMPIFEQ $$cg_lst_false LF@%cg_v bool@false\n"
        "MOVE LF@%cg_s string@true\n"
        "JUMP $$cg_lst_append\n"

        "LABEL $$cg_lst_false\n"
        "MOVE LF@%cg_s string@false\n"
        "JUMP $$cg_lst_append\n"

        "LABEL $$cg_lst_nil\n"
        "MOVE LF@%cg_s string@()\n"
        "JUMP $$cg_lst_append\n"

        "LABEL $$cg_lst_array\n"
        "PUSHS LF@%cg_v\n"
        "CALL $$cg_list_to_str\n"
        "POPS LF@%cg_s\n"

        "LABEL $$cg_lst_append\n"
        "CONCAT LF@%cg_out LF@%cg_out LF@%cg_s\n"
        "ADD LF@%cg_i LF@%cg_i int@1\n"
        "JUMP $$cg_lst_loop\n"

        "LABEL $$cg_lst_done\n"
        "CONCAT LF@%cg_out LF@%cg_out string@)\n"
        "PUSHS LF@%cg_out\n"
        "POPFRAME\n"
        "RETURN\n", ctx->output);
}


void codegen_emit_header(CodegenContext *ctx){
    if(ctx == NULL || ctx->output == NULL){
        return;
    }

    if(fputs(".IFJcode26\n"
                "DEFVAR GF@%cg_tmp\n"
                "DEFVAR GF@%cg_str_left\n"
                "DEFVAR GF@%cg_str_right\n"
                "JUMP $$main\n", ctx->output) == EOF){
        ctx->error_code = 99;
    }

    if(ctx->error_code == 0){
        codegen_emit_list_to_string_runtime(ctx);
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

    while(ctx->bindings != NULL){
        CodegenBinding *next = ctx->bindings->next;
        free(ctx->bindings);
        ctx->bindings = next;
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

        case NODE_VALMEM:
            generate_expression(node, ctx);

            if(ctx->error_code == 0){
                fputs("POPS GF@%cg_tmp\n", ctx->output);
            }
            break;

        case NODE_FOR:
            generate_for(node, ctx);
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
    int count = 0;

    if(args == NULL){
        return 0;
    }

    if(args->type != NODE_ARGUMENT && args->type != NODE_PROGRAM){
        generate_expression(args, ctx);
        return 1;
    }

    for(AST_NODE *arg = args; arg != NULL; arg = arg->right_ch){
        if(arg->left_ch == NULL){
            break;
        }

        generate_expression(arg->left_ch, ctx);

        if(ctx->error_code != 0){
            return -1;
        }

        count++;
    }

    return count;
}


static void generate_list_range_call(AST_NODE *node, CodegenContext *ctx){
    int count = 0;

    for(AST_NODE *arg = node->right_ch; arg != NULL && arg->left_ch != NULL; arg = arg->right_ch){
        generate_expression(arg->left_ch, ctx);

        if(ctx->error_code != 0){
            return;
        }

        count++;
    }

    if(count == 2){
        fputs("PUSHS int@1\n", ctx->output);
    }

    else if(count != 3){
        ctx->error_code = 99;
        return;
    }

    codegen_list_range_from_stack(ctx, 0);
}


void codegen_generate_call_expression(AST_NODE *node, CodegenContext *ctx){
    if(node->left_ch != NULL && node->left_ch->type == NODE_VALMEM && node->left_ch->right_ch != NULL){
        const char *method = node->left_ch->right_ch->token.attr.str_val;

        if(method != NULL && (strcmp(method, "take") == 0 || strcmp(method, "drop") == 0 || strcmp(method, "slice") == 0)){
            codegen_generate_list_method_expression(node, ctx);
            return;
        }
    }

    if(node->left_ch != NULL && node->left_ch->type == NODE_VALMEM &&
    node->left_ch->left_ch != NULL && node->left_ch->left_ch->token.type == KEYWORD_LIST){
        generate_list_range_call(node, ctx);
        return;
    }
    
    if(node->left_ch != NULL && node->left_ch->token.type == KEYWORD_LIST){
        generate_list_constructor(node, ctx);
        return;
    }

    const HTEntry *entry = node->left_ch->identifier_entry;

    if(entry != NULL && (entry->entry_type == VAR || entry->entry_type == VAL) && codegen_from_semantic_type(entry->entry_data.variable_data_type) == CG_TYPE_LIST){
        generate_list_index(node, ctx);
        return;
    }

    const char *name = node->left_ch->token.attr.str_val;

    if(strcmp(name, "print") == 0){
        AST_NODE *arg = node->right_ch;

        if(arg->type == NODE_ARGUMENT || arg->type == NODE_PROGRAM){
            arg = arg->left_ch;
        }

        CodegenType type = codegen_expression_type(arg, ctx);

        generate_expression(arg, ctx);

        if(ctx->error_code != 0){
            return;
        }

        if(type == CG_TYPE_LIST){
            fputs("CALL $$cg_list_to_str\n"
                "POPS GF@%cg_tmp\n"
                "WRITE GF@%cg_tmp\n", ctx->output);  
        }

        else if(type == CG_TYPE_UNIT){
            fputs("POPS GF@%cg_tmp\n"
                "WRITE string@()\n", ctx->output);
        }

        else{
            fputs("POPS GF@%cg_tmp\n"
                "WRITE GF@%cg_tmp\n", ctx->output);
        }

        fputs("PUSHS nil@nil\n", ctx->output);
        return;
    }

    if(codegen_builtin_return_type(node) != CG_TYPE_UNKNOWN){
        codegen_generate_builtin_expression(node, ctx);
        return;
    }

    generate_call_arguments(node->right_ch, ctx);

    if(ctx->error_code == 0){
        fprintf(ctx->output, "CALL $%s\n", name);
    }
}


static void generate_call(AST_NODE *node, CodegenContext *ctx){
    codegen_generate_call_expression(node, ctx);

    if(ctx->error_code == 0){
        fputs("POPS GF@%cg_tmp\n", ctx->output);
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
    int count = 0;

    if(args == NULL){
        return 0;
    }

    if(args->type != NODE_ARGUMENT && args->type != NODE_PROGRAM){
        out[0] = args;
        return expected == 1;
    }

    for(AST_NODE *arg = args; arg != NULL; arg = arg->right_ch){
        if(arg->left_ch == NULL){
            break;
        }

        if(count < expected){
            out[count] = arg->left_ch;
        }

        count++;
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
        if(node->right_ch != NULL &&
        !(node->right_ch->type == NODE_ARGUMENT && node->right_ch->left_ch == NULL)){
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

    if(arg != NULL && (arg->type == NODE_PROGRAM || arg->type == NODE_ARGUMENT)){
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
        (strcmp(name, "chr") == 0 && type != CG_TYPE_INT)){
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

            case CG_TYPE_LIST:
                fputs("CALL $$cg_list_to_str\n", ctx->output);
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
    AST_NODE *pair = node->left_ch;
    unsigned int id = ctx->label_counter++;

    generate_expression(pair->left_ch, ctx);

    fprintf(ctx->output, "PUSHS bool@false\n"
                        "JUMPIFEQS $$if_else_%u\n", id);

    codegen_generate_node(pair->right_ch, ctx);

    fprintf(ctx->output, "JUMP $$if_end_%u\n"
                        "LABEL $$if_else_%u\n", id, id);

    if(node->right_ch != NULL){
        AST_NODE *branch = node->right_ch;

        if(branch->type == NODE_ELSE){
            branch = branch->left_ch;
        }

        codegen_generate_node(branch, ctx);
    }

    fprintf(ctx->output, "LABEL $$if_end_%u\n", id);
}


static void generate_for_list(AST_NODE *node, CodegenContext *ctx){
    AST_NODE *iterator = node->left_ch;
    AST_NODE *source = iterator->left_ch->left_ch;

    const char *name = codegen_variable_name(iterator, ctx);

    if(ctx->error_code != 0){
        return;
    }

    unsigned int id = ctx->label_counter++;

    generate_expression(source, ctx);

    if(ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output,"POPS LF@%%cg_for_end_%s\n"
                        "MOVE LF@%%cg_for_step_%s int@0\n"
                        "LABEL $$cg_for_list_check_%u\n"
                        "ARRAYGET GF@%%cg_tmp LF@%%cg_for_end_%s int@0\n"
                        "LT GF@%%cg_tmp LF@%%cg_for_step_%s GF@%%cg_tmp\n"
                        "JUMPIFEQ $$cg_for_list_end_%u GF@%%cg_tmp bool@false\n"
                        "ADD GF@%%cg_tmp LF@%%cg_for_step_%s int@1\n"
                        "ARRAYGET LF@%s LF@%%cg_for_end_%s GF@%%cg_tmp\n", name, name, id, name, name, id, name, name, name);

    for(AST_NODE *guard = iterator->right_ch; guard != NULL; guard = guard->right_ch){

        generate_expression(guard->left_ch, ctx);

        if(ctx->error_code != 0){
            return;
        }

        fprintf(ctx->output, "PUSHS bool@false\n"
                            "JUMPIFEQS $$cg_for_list_next_%u\n", id);
    }

    codegen_generate_node(node->right_ch, ctx);

    if(ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output, "LABEL $$cg_for_list_next_%u\n"
                        "ADD LF@%%cg_for_step_%s LF@%%cg_for_step_%s int@1\n"
                        "JUMP $$cg_for_list_check_%u\n"
                        "LABEL $$cg_for_list_end_%u\n", id, name, name, id, id);
}


static void generate_list_index(AST_NODE *node, CodegenContext *ctx){
    const char *name = codegen_variable_name(node->left_ch, ctx);

    if(ctx->error_code != 0){
        return;
    }

    generate_expression(node->right_ch->left_ch, ctx);

    if(ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output, "PUSHS LF@%s\n", name);

    unsigned int id = ctx->label_counter++;

    fputs("CREATEFRAME\n"
        "PUSHFRAME\n"
        "DEFVAR LF@%cg_list\n"
        "DEFVAR LF@%cg_index\n"
        "DEFVAR LF@%cg_len\n"
        "DEFVAR LF@%cg_value\n"
        "DEFVAR LF@%cg_test\n"

        "POPS LF@%cg_list\n"
        "POPS LF@%cg_index\n"

        "ARRAYGET LF@%cg_len LF@%cg_list int@0\n"
        "LT LF@%cg_test LF@%cg_index int@0\n", ctx->output);

    fprintf(ctx->output, "JUMPIFEQ $$cg_list_index_bad_%u "
                        "LF@%%cg_test bool@true\n", id);

    fputs("LT LF@%cg_test LF@%cg_index LF@%cg_len\n", ctx->output);

    fprintf(ctx->output, "JUMPIFEQ $$cg_list_index_bad_%u "
                        "LF@%%cg_test bool@false\n", id);

    fputs("ADD LF@%cg_index LF@%cg_index int@1\n"
        "ARRAYGET LF@%cg_value LF@%cg_list LF@%cg_index\n"
        "PUSHS LF@%cg_value\n"
        "POPFRAME\n", ctx->output);

    fprintf(ctx->output, "JUMP $$cg_list_index_end_%u\n"
                        "LABEL $$cg_list_index_bad_%u\n"
                        "EXIT int@40\n"
                        "LABEL $$cg_list_index_end_%u\n", id, id, id);
}


static void generate_list_constructor(AST_NODE *node, CodegenContext *ctx){
    size_t count = 0;
    int needs_double = ctx->force_list_double;
    ctx->force_list_double = 0;

    for(AST_NODE *arg = node->right_ch; arg != NULL && arg->left_ch != NULL; arg = arg->right_ch){
        if(codegen_expression_type(arg->left_ch, ctx) == CG_TYPE_DOUBLE){
            needs_double = 1;
        }
    }

    for(AST_NODE *arg = node->right_ch; arg != NULL && arg->left_ch != NULL; arg = arg->right_ch){
        CodegenType type = codegen_expression_type(arg->left_ch, ctx);

        generate_expression(arg->left_ch, ctx);

        if(ctx->error_code != 0){
            return;
        }

        if(needs_double && type == CG_TYPE_INT){
            fputs("INT2FLOATS\n", ctx->output);
        }

        count++;
    }

    fputs("CREATEFRAME\n"
        "PUSHFRAME\n"
        "DEFVAR LF@%cg_list\n"
        "DEFVAR LF@%cg_item\n", ctx->output);

    fprintf(ctx->output, "NEWARRAY LF@%%cg_list int@%zu\n"
                        "ARRAYSET LF@%%cg_list int@0 int@%zu\n", count + 1, count);

    for(size_t i = count; i > 0; i--){
        fprintf(ctx->output, "POPS LF@%%cg_item\n"
                            "ARRAYSET LF@%%cg_list int@%zu LF@%%cg_item\n", i);
    }

    fputs("PUSHS LF@%cg_list\n"
        "POPFRAME\n", ctx->output);
}


static void generate_for(AST_NODE *node, CodegenContext *ctx){
    AST_NODE *iterator = node->left_ch;
    AST_NODE *limit = iterator->left_ch;
    AST_NODE *range = limit->left_ch;

    if(range->token.type != KEYWORD_TO && range->token.type != KEYWORD_UNTIL){
        generate_for_list(node, ctx);
        return;
    }

    if(range->token.type != KEYWORD_TO && range->token.type != KEYWORD_UNTIL){
        ctx->error_code = 99;
        return;
    }

    AST_NODE *start = range->left_ch;
    AST_NODE *end = range->right_ch;
    AST_NODE *step = limit->right_ch;

    const char *name = codegen_variable_name(iterator, ctx);

    unsigned int id = ctx->label_counter++;

    int until = range->token.type == KEYWORD_UNTIL;

    generate_expression(start, ctx);

    fprintf(ctx->output, "POPS LF@%s\n", name);

    generate_expression(end, ctx);

    fprintf(ctx->output, "POPS LF@%%cg_for_end_%s\n", name);

    if(step != NULL){
        generate_expression(step, ctx);
        fprintf(ctx->output, "POPS LF@%%cg_for_step_%s\n", name);
    }

    else{
        fprintf(ctx->output, "MOVE LF@%%cg_for_step_%s int@1\n", name);
    }

    
    fprintf(ctx->output, "JUMPIFNEQ $$for_valid_%u LF@%%cg_for_step_%s int@0\n"
                        "EXIT int@40\n"
                        "LABEL $$for_valid_%u\n", id, name, id);

    fprintf(ctx->output, "LABEL $$for_check_%u\n"
                        "LT GF@%%cg_tmp LF@%%cg_for_step_%s int@0\n"
                        "JUMPIFEQ $$for_negative_%u GF@%%cg_tmp bool@true\n", id, name, id);

    fprintf(ctx->output, "%s GF@%%cg_tmp LF@%s LF@%%cg_for_end_%s\n"
                        "JUMPIFEQ $$for_end_%u GF@%%cg_tmp bool@%s\n"
                        "JUMP $$for_guards_%u\n", until ? "LT" : "GT", name, name, id, until ? "false" : "true", id);

    fprintf(ctx->output, "LABEL $$for_negative_%u\n", id);

    fprintf(ctx->output, "%s GF@%%cg_tmp LF@%s LF@%%cg_for_end_%s\n"
                        "JUMPIFEQ $$for_end_%u GF@%%cg_tmp bool@%s\n"
                        "LABEL $$for_guards_%u\n", until ? "GT" : "LT", name, name, id, until ? "false" : "true", id);

    for(AST_NODE *guard = iterator->right_ch; guard != NULL; guard = guard->right_ch){
        generate_expression(guard->left_ch, ctx);

        fprintf(ctx->output, "PUSHS bool@false\n"
                            "JUMPIFEQS $$for_next_%u\n", id);
    }

    codegen_generate_node(node->right_ch, ctx);

    fprintf(ctx->output, "LABEL $$for_next_%u\n"
                        "ADD LF@%s LF@%s LF@%%cg_for_step_%s\n"
                        "JUMP $$for_check_%u\n"
                        "LABEL $$for_end_%u\n", id, name, name, name, id, id);
}




//FB0