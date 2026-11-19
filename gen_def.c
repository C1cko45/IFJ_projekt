#include "gen_def.h"
#include "gen_nondef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



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

    generate_parameters_reverse(param->right_ch, ctx);

    if(ctx->error_code != 0){
        return;
    }

    const char *name = codegen_variable_name(param, ctx);

    if(ctx->error_code != 0){
        return;
    }

    fprintf(ctx->output, "DEFVAR LF@%s\n"
                        "POPS LF@%s\n", name, name);

    int expects_double = (param->left_ch != NULL && param->left_ch->token.type == KEYWORD_DOUBLE) ||
        (param->identifier_entry != NULL &&
         (param->identifier_entry->entry_type == VAR || param->identifier_entry->entry_type == VAL) &&
         param->identifier_entry->entry_data.variable_data_type == DOUBLE);

    if(expects_double){
        unsigned int id = ctx->label_counter++;

        fprintf(ctx->output, "TYPE GF@%%cg_tmp LF@%s\n"
                            "JUMPIFNEQ $$cg_param_ok_%u GF@%%cg_tmp string@int\n"
                            "INT2FLOAT LF@%s LF@%s\n"
                            "LABEL $$cg_param_ok_%u\n", name, id, name, name, id);
    }
}


static CodegenType get_function_return_type(AST_NODE *node){
    if(node != NULL && node->type == NODE_LIST){
        return CG_TYPE_LIST;
    }

    if(node == NULL || node->type != NODE_TOKEN_ONLY){
        return CG_TYPE_UNKNOWN;
    }

    switch(node->token.type){
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


static void generate_function_result(AST_NODE *expression, CodegenContext *ctx){
    int previous_force = ctx->force_list_double;

    if(ctx->current_function_list_double && expression != NULL && expression->type == NODE_CALL &&
       expression->left_ch != NULL && expression->left_ch->token.type == KEYWORD_LIST){
            ctx->force_list_double = 1;
    }

    generate_expression(expression, ctx);

    ctx->force_list_double = previous_force;
}


static void generate_nonunit_body(AST_NODE *body, CodegenContext *ctx, CodegenType expected_type){
    if(body == NULL || ctx == NULL || ctx->error_code != 0){
        return;
    }

    AST_NODE *last = body;

    if(body->type == NODE_PROGRAM){
        AST_NODE *current = body;

        while(current->right_ch != NULL){
            if(current->type != NODE_PROGRAM || current->left_ch == NULL ||
               current->right_ch->type != NODE_PROGRAM){
                    ctx->error_code = 99;
                    return;
            }

            codegen_generate_node(current->left_ch, ctx);

            if(ctx->error_code != 0){
                return;
            }

            current = current->right_ch;
        }

        if(current->type != NODE_PROGRAM){
            ctx->error_code = 99;
            return;
        }

        last = current->left_ch;
    }

    if(last == NULL){
        ctx->error_code = 99;
        return;
    }

    if(last->type == NODE_TOKEN_ONLY &&
       last->token.type == KEYWORD_RETURN){
        generate_return(last, ctx);
        return;
    }

    if(last->type == NODE_IF){
        CodegenType actual_type = codegen_expression_type(last, ctx);

        if(actual_type == CG_TYPE_UNKNOWN){
            codegen_generate_node(last, ctx);
        }

        else{
            generate_function_result(last, ctx);

            if(expected_type == CG_TYPE_DOUBLE && actual_type == CG_TYPE_INT){
                fputs("INT2FLOATS\n", ctx->output);
            }
        }

        return;
    }

    if(last->type != NODE_TOKEN_ONLY && last->type != NODE_CALL && last->type != NODE_VALMEM){
        ctx->error_code = 99;
        return;
    }

    CodegenType actual_type =
        codegen_expression_type(last, ctx);

    int convert =
        expected_type == CG_TYPE_DOUBLE &&
        actual_type == CG_TYPE_INT;

    if(actual_type == CG_TYPE_UNKNOWN ||
       (actual_type != expected_type && !convert)){
        ctx->error_code = 99;
        return;
    }

    generate_function_result(last, ctx);

    if(ctx->error_code != 0){
        return;
    }

    if(convert){
        fputs("INT2FLOATS\n", ctx->output);
    }
}


typedef struct LocalDeclName{
    const char *name;
    const HTEntry *entry;
    struct LocalDeclName *next;
} LocalDeclName;


static int parameter_has_name(const AST_NODE *param, const AST_NODE *identifier){
    while(param != NULL){
        if(param->identifier_entry != NULL && identifier->identifier_entry != NULL){
            if(param->identifier_entry == identifier->identifier_entry){
                return 1;
            }
        }

        else if(param->identifier_entry == NULL && identifier->identifier_entry == NULL &&
                strcmp(param->token.attr.str_val, identifier->token.attr.str_val) == 0){
            return 1;
        }

        param = param->right_ch;
    }

    return 0;
}


static void collect_local_declarations(AST_NODE *node, const AST_NODE *params, LocalDeclName **declared, CodegenContext *ctx){
    if(node == NULL || ctx == NULL || ctx->error_code != 0){
        return;
    }

    if(node->type == NODE_FUNC_DEF || node->type == NODE_MAIN){
        return;
    }

    if(node->type == NODE_FOR){
        const char *name = codegen_variable_name(node->left_ch, ctx);

        fprintf(ctx->output, "DEFVAR LF@%s\n"
                            "DEFVAR LF@%%cg_for_end_%s\n"
                            "DEFVAR LF@%%cg_for_step_%s\n", name, name, name);
    }

    if(node->type == NODE_VAR_DEF || node->type == NODE_VAL_DEF){
        AST_NODE *identifier = node->left_ch;

        if(identifier == NULL || identifier->type != NODE_TOKEN_ONLY ||
           identifier->token.type != TOKEN_TYPE_IDENTIFIER ||
           identifier->token.attr.str_val == NULL ||
           identifier->token.attr.str_val[0] == '\0'){
                ctx->error_code = 99;
                return;
        }

        const char *name = identifier->token.attr.str_val;

        if(parameter_has_name(params, identifier)){
            ctx->error_code = 99;
            return;
        }

        for(LocalDeclName *item = *declared; item != NULL;  item = item->next){
            if(identifier->identifier_entry != NULL && item->entry == identifier->identifier_entry){
                ctx->error_code = 99;
                return;
            }

            if(identifier->identifier_entry == NULL && item->entry == NULL &&
            strcmp(item->name, name) == 0){
                ctx->error_code = 99;
                return;
            }
        }

        LocalDeclName *item = malloc(sizeof(*item));

        if(item == NULL){
            ctx->error_code = 99;
            return;
        }

        item->name = name;
        item->entry = identifier->identifier_entry;
        item->next = *declared;
        *declared = item;

        const char *internal_name = codegen_variable_name(identifier, ctx);

        if(ctx->error_code != 0){
            return;
        }

        fprintf(ctx->output, "DEFVAR LF@%s\n", internal_name);
    }

    collect_local_declarations(node->left_ch, params, declared, ctx);

    collect_local_declarations(node->right_ch, params, declared, ctx);
}


static void generate_local_declarations(AST_NODE *body, const AST_NODE *params, CodegenContext *ctx){
    LocalDeclName *declared = NULL;

    collect_local_declarations(body, params, &declared, ctx);

    while(declared != NULL){
        LocalDeclName *next = declared->next;
        free(declared);
        declared = next;
    }
}


static CodegenType infer_function_return_type(const AST_NODE *body, const CodegenContext *ctx){
    if(body == NULL){
        return CG_TYPE_UNKNOWN;
    }

    const AST_NODE *last = body;

    while(last != NULL && last->type == NODE_PROGRAM){
        if(last->right_ch != NULL){
            last = last->right_ch;
        }

        else{
            last = last->left_ch;
        }
    }

    if(last == NULL){
        return CG_TYPE_UNKNOWN;
    }

    switch(last->type){
        case NODE_VAR_DEF:
        case NODE_VAL_DEF:
        case NODE_WHILE:
        case NODE_FOR:
            return CG_TYPE_UNIT;

        case NODE_IF: {
            CodegenType type = codegen_expression_type(last, ctx);

            if(type == CG_TYPE_UNKNOWN){
                return CG_TYPE_UNIT;
            }

            return type;
        }

        case NODE_CALL:
        case NODE_VALMEM:
            return codegen_expression_type(last, ctx);

        case NODE_TOKEN_ONLY:

            if(last->token.type == TOKEN_TYPE_ASSIGN){
                return CG_TYPE_UNIT;
            }

            if(last->token.type == KEYWORD_RETURN){
                return CG_TYPE_UNKNOWN;
            }

            return codegen_expression_type(last, ctx);

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

    if(function_node->type != NODE_FUNC_DEF || function_definicion->type != NODE_TOKEN_ONLY ||
        function_definicion->token.type != TOKEN_TYPE_IDENTIFIER ||
        function_definicion->token.attr.str_val == NULL || function_body == NULL){
            ctx->error_code = 99;
            return;
    }

    AST_NODE *return_type = function_definicion->left_ch;

    CodegenType declare_type = CG_TYPE_UNKNOWN;

    if(function_definicion->identifier_entry != NULL &&
    function_definicion->identifier_entry->entry_type == FUNCTION){
        declare_type = codegen_from_semantic_type(function_definicion->identifier_entry->entry_data.function.return_type);
    }

    if(declare_type == CG_TYPE_UNKNOWN){
        declare_type = get_function_return_type(return_type);
    }

    if(declare_type == CG_TYPE_UNKNOWN && return_type == NULL){
        declare_type = infer_function_return_type(function_body, ctx);
    }

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

    int previous_function_list_double = ctx->current_function_list_double;

    int semantic_list_double = 0;

    if(function_definicion->identifier_entry != NULL && function_definicion->identifier_entry->entry_type == FUNCTION){
        semantic_list_double = function_definicion->identifier_entry->entry_data.function.return_type == LIST_DOUBLE;
    }

    int ast_list_double = return_type != NULL && return_type->type == NODE_LIST && return_type->left_ch != NULL &&
        return_type->left_ch->type == NODE_TOKEN_ONLY && return_type->left_ch->token.type == KEYWORD_DOUBLE;

    ctx->current_function_list_double = declare_type == CG_TYPE_LIST && (semantic_list_double || ast_list_double);

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
        generate_local_declarations(function_body, function_definicion->right_ch, ctx);
    }

    if(ctx->error_code == 0){
        if(is_main || declare_type == CG_TYPE_UNIT){
            codegen_generate_node(function_body, ctx);
        }

        else{
            generate_nonunit_body(function_body, ctx, declare_type);
        }
    }

    if(ctx->error_code == 0){
        if(!is_main && declare_type == CG_TYPE_UNIT){
            fputs("PUSHS nil@nil\n", ctx->output);
        }

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

    ctx->current_function_list_double = previous_function_list_double;
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

        generate_function_result(value, ctx);

        if(ctx->error_code != 0){
            return;
        }

        if(convert){
            fputs("INT2FLOATS\n", ctx->output);
        }
    }

    if(ctx->current_return_type == CG_TYPE_UNIT && !ctx->current_is_main){
        fputs("PUSHS nil@nil\n", ctx->output);
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



