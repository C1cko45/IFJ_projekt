#include "gen_nondef.h"
#include <stdio.h>
#include <string.h>

static void generate_string_literal(AST_NODE *node, CodegenContext *ctx);
static void generate_valmem_expression(AST_NODE *node, CodegenContext *ctx);
static CodegenType get_expression_type(const AST_NODE *node, const CodegenContext *ctx);
static void generate_list_head_tail(AST_NODE *node, CodegenContext *ctx, int tail);
void codegen_generate_list_method_expression(AST_NODE *node, CodegenContext *ctx);
void codegen_list_range_from_stack(CodegenContext *ctx, int inclusive);
static void generate_if_expression(AST_NODE *node, CodegenContext *ctx);

static void generate_operator(TokenType type, CodegenContext *ctx)
{
    switch (type)
    {
        case TOKEN_TYPE_PLUS:
            fprintf(ctx->output, "ADDS\n");
            break;

        case TOKEN_TYPE_MINUS:
            fprintf(ctx->output, "SUBS\n");
            break;

        case TOKEN_TYPE_MUL:
            fprintf(ctx->output, "MULS\n");
            break;

        default:
            ctx->error_code = 99;
            break;
    }
}

static void generate_literal(AST_NODE *node, CodegenContext *ctx)
{
    if (node == NULL || ctx == NULL || ctx->error_code != 0)
    {
        return;
    }

    switch (node->token.type)
    {
        case TOKEN_TYPE_INT:
            fprintf(ctx->output, "PUSHS int@%lld\n", node->token.attr.int_val);
            break;

        case TOKEN_TYPE_DOUBLE:
            fprintf(ctx->output, "PUSHS float@%a\n", node->token.attr.double_val);
            break;

        case TOKEN_TYPE_BOOL:
            fprintf(ctx->output, "PUSHS bool@%s\n", node->token.attr.bool_val ? "true" : "false");
            break;

        case TOKEN_TYPE_STRING:
            generate_string_literal(node, ctx);
            break;

        case TOKEN_TYPE_UNIT:
            fprintf(ctx->output, "PUSHS nil@nil\n");
            break;

        default:
            ctx->error_code = 99;
            break;
    }
}

CodegenType codegen_from_semantic_type(DataType type)
{
    switch (type)
    {
        case INT:
            return CG_TYPE_INT;

        case DOUBLE:
            return CG_TYPE_DOUBLE;

        case STRING:
            return CG_TYPE_STRING;

        case BOOLEAN:
            return CG_TYPE_BOOL;

        case UNIT:
            return CG_TYPE_UNIT;

        case LIST_INT:
        case LIST_DOUBLE:
        case LIST_STRING:
        case LIST_BOOLEAN:
        case LIST_UNIT:
        case LIST_ANY:
            return CG_TYPE_LIST;

        case NIL:
        case ANY:
            return CG_TYPE_UNKNOWN;
    }

    return CG_TYPE_UNKNOWN;
}

static CodegenType control_branch_type(const AST_NODE *body, const CodegenContext *ctx)
{
    while (body->type == NODE_PROGRAM)
    {
        body = body->right_ch != NULL ? body->right_ch : body->left_ch;
    }

    if (body->type == NODE_CALL || body->type == NODE_VALMEM || body->type == NODE_IF)
    {
        return get_expression_type(body, ctx);
    }

    if (body->type == NODE_TOKEN_ONLY && body->token.type == KEYWORD_RETURN)
    {
        return CG_TYPE_UNKNOWN;
    }

    if (body->type == NODE_TOKEN_ONLY && body->token.type != TOKEN_TYPE_ASSIGN)
    {
        return get_expression_type(body, ctx);
    }

    return CG_TYPE_UNIT;
}

static CodegenType get_expression_type(const AST_NODE *node, const CodegenContext *ctx)
{
    if (node == NULL || ctx == NULL)
    {
        return CG_TYPE_UNKNOWN;
    }

    if (node->type == NODE_IF)
    {
        const AST_NODE *else_body = node->right_ch;

        if (else_body == NULL)
        {
            return CG_TYPE_UNKNOWN;
        }

        if (else_body->type == NODE_ELSE)
        {
            else_body = else_body->left_ch;
        }

        CodegenType then_type = control_branch_type(node->left_ch->right_ch, ctx);

        CodegenType else_type = control_branch_type(else_body, ctx);

        if ((then_type == CG_TYPE_INT && else_type == CG_TYPE_DOUBLE) ||
            (then_type == CG_TYPE_DOUBLE && else_type == CG_TYPE_INT))
        {
            return CG_TYPE_DOUBLE;
        }

        return then_type == else_type ? then_type : CG_TYPE_UNKNOWN;
    }

    if (node->type == NODE_CALL)
    {
        if (node->left_ch != NULL && codegen_variable_type(node->left_ch, ctx) == CG_TYPE_LIST)
        {
            return codegen_list_item_type(node->left_ch, ctx);
        }

        if (node->left_ch != NULL && node->left_ch->type == NODE_TOKEN_ONLY && node->left_ch->identifier_entry != NULL)
        {
            const HTEntry *entry = node->left_ch->identifier_entry;

            if ((entry->entry_type == VAR || entry->entry_type == VAL) &&
                codegen_from_semantic_type(entry->entry_data.variable_data_type) == CG_TYPE_LIST)
            {
                return codegen_list_item_type(node->left_ch, ctx);
            }
        }

        if (node->left_ch != NULL && node->left_ch->token.type == KEYWORD_LIST)
        {
            return CG_TYPE_LIST;
        }

        if (node->left_ch != NULL && node->left_ch->type == NODE_VALMEM && node->left_ch->left_ch != NULL &&
            node->left_ch->left_ch->type == NODE_TOKEN_ONLY && node->left_ch->left_ch->token.type == KEYWORD_LIST &&
            node->left_ch->right_ch != NULL && node->left_ch->right_ch->token.attr.str_val != NULL &&
            strcmp(node->left_ch->right_ch->token.attr.str_val, "range") == 0)
        {
            return CG_TYPE_LIST;
        }

        if (node->left_ch != NULL && node->left_ch->type == NODE_VALMEM && node->left_ch->right_ch != NULL)
        {
            const char *member = node->left_ch->right_ch->token.attr.str_val;

            if (member != NULL &&
                (strcmp(member, "take") == 0 || strcmp(member, "drop") == 0 || strcmp(member, "slice") == 0))
            {
                return CG_TYPE_LIST;
            }
        }

        const AST_NODE *name = node->left_ch;

        if (name != NULL && name->token.attr.str_val != NULL && strcmp(name->token.attr.str_val, "print") == 0)
        {
            return CG_TYPE_UNIT;
        }

        if (name != NULL && name->type == NODE_TOKEN_ONLY && name->identifier_entry != NULL &&
            name->identifier_entry->entry_type == FUNCTION)
        {
            return codegen_from_semantic_type(name->identifier_entry->entry_data.function.return_type);
        }

        return codegen_builtin_return_type(node);
    }

    if (node->type == NODE_VALMEM)
    {
        if (node->right_ch == NULL || node->right_ch->token.attr.str_val == NULL)
        {
            return CG_TYPE_UNKNOWN;
        }

        const char *member = node->right_ch->token.attr.str_val;

        if (strcmp(member, "length") == 0 || strcmp(member, "toInt") == 0)
        {
            return CG_TYPE_INT;
        }

        if (strcmp(member, "floor") == 0)
        {
            return CG_TYPE_DOUBLE;
        }

        if (strcmp(member, "toString") == 0)
        {
            return CG_TYPE_STRING;
        }

        if (strcmp(member, "tail") == 0)
        {
            return CG_TYPE_LIST;
        }

        if (strcmp(member, "head") == 0)
        {
            return codegen_list_item_type(node->left_ch, ctx);
        }

        return CG_TYPE_UNKNOWN;
    }

    if (node->type != NODE_TOKEN_ONLY)
    {
        return CG_TYPE_UNKNOWN;
    }

    switch (node->token.type)
    {
        case TOKEN_TYPE_INT:
            return CG_TYPE_INT;

        case TOKEN_TYPE_DOUBLE:
            return CG_TYPE_DOUBLE;

        case TOKEN_TYPE_STRING:
            return CG_TYPE_STRING;

        case TOKEN_TYPE_BOOL:
            return CG_TYPE_BOOL;

        case TOKEN_TYPE_UNIT:
            return CG_TYPE_UNIT;

        case TOKEN_TYPE_IDENTIFIER: {
            CodegenType bound_type = codegen_variable_type(node, ctx);

            if (bound_type != CG_TYPE_UNKNOWN)
            {
                return bound_type;
            }

            if (node->identifier_entry != NULL &&
                (node->identifier_entry->entry_type == VAR || node->identifier_entry->entry_type == VAL))
            {
                return codegen_from_semantic_type(node->identifier_entry->entry_data.variable_data_type);
            }

            if (node->token.attr.str_val == NULL || ctx->symbols == NULL)
            {
                return CG_TYPE_UNKNOWN;
            }

            for (size_t i = 0; i < ctx->symbol_count; i++)
            {
                if (ctx->symbols[i].name != NULL && strcmp(ctx->symbols[i].name, node->token.attr.str_val) == 0)
                {
                    return ctx->symbols[i].type;
                }
            }

            return CG_TYPE_UNKNOWN;
        }

        case TOKEN_TYPE_PLUS:
        case TOKEN_TYPE_MINUS:
        case TOKEN_TYPE_MUL:
        case TOKEN_TYPE_DIV: {
            if (node->right_ch == NULL && node->left_ch != NULL &&
                (node->token.type == TOKEN_TYPE_PLUS || node->token.type == TOKEN_TYPE_MINUS))
            {
                CodegenType type = get_expression_type(node->left_ch, ctx);

                return (type == CG_TYPE_INT || type == CG_TYPE_DOUBLE) ? type : CG_TYPE_UNKNOWN;
            }

            if (node->left_ch == NULL || node->right_ch == NULL)
            {
                return CG_TYPE_UNKNOWN;
            }

            CodegenType left = get_expression_type(node->left_ch, ctx);

            CodegenType right = get_expression_type(node->right_ch, ctx);

            if (node->token.type == TOKEN_TYPE_PLUS && left == CG_TYPE_STRING && right == CG_TYPE_STRING)
            {
                return CG_TYPE_STRING;
            }

            if ((left != CG_TYPE_INT && left != CG_TYPE_DOUBLE) || (right != CG_TYPE_INT && right != CG_TYPE_DOUBLE))
            {
                return CG_TYPE_UNKNOWN;
            }

            if (left == CG_TYPE_DOUBLE || right == CG_TYPE_DOUBLE)
            {
                return CG_TYPE_DOUBLE;
            }

            return CG_TYPE_INT;
        }

        case TOKEN_TYPE_EQ:
        case TOKEN_TYPE_NEQ:
        case TOKEN_TYPE_LT:
        case TOKEN_TYPE_LTE:
        case TOKEN_TYPE_GT:
        case TOKEN_TYPE_GTE:
            if (node->left_ch == NULL || node->right_ch == NULL)
            {
                return CG_TYPE_UNKNOWN;
            }

            return CG_TYPE_BOOL;

        case TOKEN_TYPE_NOT:
            if (node->left_ch != NULL && node->right_ch == NULL &&
                get_expression_type(node->left_ch, ctx) == CG_TYPE_BOOL)
            {
                return CG_TYPE_BOOL;
            }

            return CG_TYPE_UNKNOWN;

        case TOKEN_TYPE_AND:
        case TOKEN_TYPE_OR:
            if (node->left_ch != NULL && node->right_ch != NULL &&
                get_expression_type(node->left_ch, ctx) == CG_TYPE_BOOL &&
                get_expression_type(node->right_ch, ctx) == CG_TYPE_BOOL)
            {
                return CG_TYPE_BOOL;
            }

            return CG_TYPE_UNKNOWN;

        case TOKEN_TYPE_CONCAT:
            return CG_TYPE_LIST;

        default:
            return CG_TYPE_UNKNOWN;
    }
}

CodegenType codegen_expression_type(const AST_NODE *node, const CodegenContext *ctx)
{
    return get_expression_type(node, ctx);
}

static CodegenType get_declared_type(const AST_NODE *identifier, const CodegenContext *ctx)
{
    if (identifier == NULL)
    {
        return CG_TYPE_UNKNOWN;
    }

    CodegenType bound = codegen_variable_type(identifier, ctx);

    if (bound != CG_TYPE_UNKNOWN)
    {
        return bound;
    }

    if (identifier->left_ch == NULL)
    {
        return CG_TYPE_UNKNOWN;
    }

    const AST_NODE *type_node = identifier->left_ch;

    if (type_node->type == NODE_LIST)
    {
        return CG_TYPE_LIST;
    }

    if (type_node->type != NODE_TOKEN_ONLY)
    {
        return CG_TYPE_UNKNOWN;
    }

    switch (type_node->token.type)
    {
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

static void generate_numeric_operation(AST_NODE *node, CodegenContext *ctx)
{
    if (node == NULL || ctx == NULL || ctx->error_code != 0)
    {
        return;
    }

    if (node->right_ch == NULL && node->left_ch != NULL &&
        (node->token.type == TOKEN_TYPE_PLUS || node->token.type == TOKEN_TYPE_MINUS))
    {
        CodegenType type = get_expression_type(node->left_ch, ctx);

        if (type != CG_TYPE_INT && type != CG_TYPE_DOUBLE)
        {
            ctx->error_code = 99;
            return;
        }

        generate_expression(node->left_ch, ctx);

        if (ctx->error_code != 0)
        {
            return;
        }

        if (node->token.type == TOKEN_TYPE_MINUS)
        {
            fputs("POPS GF@%cg_tmp\n", ctx->output);

            if (type == CG_TYPE_INT)
            {
                fputs("PUSHS int@0\n", ctx->output);
            }

            else
            {
                fputs("PUSHS float@0x0p+0\n", ctx->output);
            }

            fputs("PUSHS GF@%cg_tmp\nSUBS\n", ctx->output);
        }

        return;
    }

    if (node->left_ch == NULL || node->right_ch == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    CodegenType left_type = get_expression_type(node->left_ch, ctx);

    CodegenType right_type = get_expression_type(node->right_ch, ctx);

    if (node->token.type == TOKEN_TYPE_PLUS && left_type == CG_TYPE_STRING && right_type == CG_TYPE_STRING)
    {

        generate_expression(node->left_ch, ctx);
        generate_expression(node->right_ch, ctx);

        if (ctx->error_code != 0)
        {
            return;
        }

        fputs("POPS GF@%cg_str_right\n"
              "POPS GF@%cg_str_left\n"
              "CONCAT GF@%cg_str_left GF@%cg_str_left GF@%cg_str_right\n"
              "PUSHS GF@%cg_str_left\n",
              ctx->output);

        return;
    }

    if ((left_type != CG_TYPE_INT && left_type != CG_TYPE_DOUBLE) ||
        (right_type != CG_TYPE_INT && right_type != CG_TYPE_DOUBLE))
    {
        ctx->error_code = 99;
        return;
    }

    generate_expression(node->left_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    if (left_type == CG_TYPE_INT && right_type == CG_TYPE_DOUBLE)
    {
        fprintf(ctx->output, "INT2FLOATS\n");
    }

    generate_expression(node->right_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    if (left_type == CG_TYPE_DOUBLE && right_type == CG_TYPE_INT)
    {
        fprintf(ctx->output, "INT2FLOATS\n");
    }

    if (node->token.type == TOKEN_TYPE_DIV)
    {
        if (left_type == CG_TYPE_INT && right_type == CG_TYPE_INT)
        {
            fprintf(ctx->output, "IDIVS\n");
        }

        else
        {
            fprintf(ctx->output, "DIVS\n");
        }
    }

    else
    {
        generate_operator(node->token.type, ctx);
    }
}

static void generate_comparison(AST_NODE *node, CodegenContext *ctx)
{
    if (node == NULL || ctx == NULL || ctx->error_code != 0)
    {
        return;
    }

    if (node->left_ch == NULL || node->right_ch == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    CodegenType left_type = get_expression_type(node->left_ch, ctx);

    CodegenType right_type = get_expression_type(node->right_ch, ctx);

    int left_numeric = (left_type == CG_TYPE_INT || left_type == CG_TYPE_DOUBLE);

    int right_numeric = (right_type == CG_TYPE_INT || right_type == CG_TYPE_DOUBLE);

    int numeric_pair = (left_numeric && right_numeric);

    int same_type = (left_type == right_type);

    int equality = (node->token.type == TOKEN_TYPE_EQ || node->token.type == TOKEN_TYPE_NEQ);

    if (left_type == CG_TYPE_UNKNOWN || right_type == CG_TYPE_UNKNOWN || (!numeric_pair && !same_type) ||
        (!equality && !numeric_pair && left_type != CG_TYPE_STRING))
    {
        ctx->error_code = 99;
        return;
    }

    generate_expression(node->left_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    if (left_type == CG_TYPE_INT && right_type == CG_TYPE_DOUBLE)
    {
        fprintf(ctx->output, "INT2FLOATS\n");
    }

    generate_expression(node->right_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    if (left_type == CG_TYPE_DOUBLE && right_type == CG_TYPE_INT)
    {
        fprintf(ctx->output, "INT2FLOATS\n");
    }

    switch (node->token.type)
    {
        case TOKEN_TYPE_LT:
            fprintf(ctx->output, "LTS\n");
            break;

        case TOKEN_TYPE_GT:
            fprintf(ctx->output, "GTS\n");
            break;

        case TOKEN_TYPE_EQ:
            fprintf(ctx->output, "EQS\n");
            break;

        case TOKEN_TYPE_NEQ:
            fprintf(ctx->output, "EQS\nNOTS\n");
            break;

        case TOKEN_TYPE_LTE:
            fprintf(ctx->output, "GTS\nNOTS\n");
            break;

        case TOKEN_TYPE_GTE:
            fprintf(ctx->output, "LTS\nNOTS\n");
            break;

        default:
            ctx->error_code = 99;
            break;
    }
}

static void generate_variable(AST_NODE *node, CodegenContext *ctx)
{
    if (ctx == NULL || ctx->error_code != 0)
    {
        return;
    }

    if (node == NULL || ctx->output == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    if (node->type != NODE_TOKEN_ONLY || node->token.type != TOKEN_TYPE_IDENTIFIER ||
        node->token.attr.str_val == NULL || node->token.attr.str_val[0] == '\0')
    {
        ctx->error_code = 99;
        return;
    }

    const char *name = codegen_variable_name(node, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    fprintf(ctx->output, "PUSHS LF@%s\n", name);
}

static void generate_string_literal(AST_NODE *node, CodegenContext *ctx)
{
    if (node == NULL || ctx == NULL || ctx->error_code != 0)
    {
        return;
    }

    const unsigned char *str = (const unsigned char *)node->token.attr.str_val;

    if (str == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    fprintf(ctx->output, "PUSHS string@");

    while (*str != '\0')
    {
        if (*str <= 32 || *str == '#' || *str == '\\' || *str >= 127)
        {
            fprintf(ctx->output, "\\%03u", (unsigned int)*str);
        }

        else
        {
            fputc(*str, ctx->output);
        }

        str++;
    }

    fprintf(ctx->output, "\n");
}

static void generate_boolean_operation(AST_NODE *node, CodegenContext *ctx)
{
    if (node == NULL || ctx == NULL || ctx->error_code != 0)
    {
        return;
    }

    if (get_expression_type(node, ctx) != CG_TYPE_BOOL)
    {
        ctx->error_code = 99;
        return;
    }

    if (node->token.type == TOKEN_TYPE_NOT)
    {
        generate_expression(node->left_ch, ctx);

        if (ctx->error_code == 0)
        {
            fputs("NOTS\n", ctx->output);
        }

        return;
    }

    int is_and = node->token.type == TOKEN_TYPE_AND;
    unsigned int id = ctx->label_counter++;

    generate_expression(node->left_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    const char *short_value = is_and ? "false" : "true";

    fprintf(ctx->output,
            "PUSHS bool@%s\n"
            "JUMPIFEQS $$bool_short_%u\n",
            short_value, id);

    generate_expression(node->right_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    fprintf(ctx->output,
            "JUMP $$bool_end_%u\n"
            "LABEL $$bool_short_%u\n"
            "PUSHS bool@%s\n"
            "LABEL $$bool_end_%u\n",
            id, id, short_value, id);
}

static void generate_valmem_expression(AST_NODE *node, CodegenContext *ctx)
{
    const char *member = node->right_ch->token.attr.str_val;

    if (strcmp(member, "toList") == 0)
    {
        AST_NODE *range = node->left_ch;

        if (range == NULL || range->type != NODE_TOKEN_ONLY ||
            (range->token.type != KEYWORD_TO && range->token.type != KEYWORD_UNTIL))
        {
            ctx->error_code = 99;
            return;
        }

        generate_expression(range->left_ch, ctx);

        if (ctx->error_code != 0)
        {
            return;
        }

        generate_expression(range->right_ch, ctx);

        if (ctx->error_code != 0)
        {
            return;
        }

        fputs("PUSHS int@1\n", ctx->output);

        codegen_list_range_from_stack(ctx, range->token.type == KEYWORD_TO);

        return;
    }

    if (strcmp(member, "head") == 0 || strcmp(member, "tail") == 0)
    {
        generate_list_head_tail(node, ctx, strcmp(member, "tail") == 0);
        return;
    }

    if (strcmp(member, "toString") == 0)
    {
        AST_NODE name = {0};
        AST_NODE arg = {0};
        AST_NODE call = {0};

        name.type = NODE_TOKEN_ONLY;
        name.token.type = TOKEN_TYPE_IDENTIFIER;
        name.token.attr.str_val = "toStr";

        arg.type = NODE_ARGUMENT;
        arg.left_ch = node->left_ch;

        call.type = NODE_CALL;
        call.left_ch = &name;
        call.right_ch = &arg;

        codegen_generate_builtin_expression(&call, ctx);
        return;
    }

    generate_expression(node->left_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    if (strcmp(member, "length") == 0)
    {
        fputs("POPS GF@%cg_tmp\n"
              "STRLEN GF@%cg_tmp GF@%cg_tmp\n"
              "PUSHS GF@%cg_tmp\n",
              ctx->output);
    }

    else if (strcmp(member, "toInt") == 0)
    {
        fputs("FLOAT2INTS\n", ctx->output);
    }

    else if (strcmp(member, "floor") == 0)
    {
        unsigned int id = ctx->label_counter++;

        fputs("POPS GF@%cg_tmp\n"
              "PUSHS GF@%cg_tmp\n"
              "FLOAT2INTS\n"
              "INT2FLOATS\n"
              "POPS GF@%cg_str_left\n"
              "LT GF@%cg_str_right GF@%cg_tmp GF@%cg_str_left\n",
              ctx->output);

        fprintf(ctx->output,
                "JUMPIFEQ $$cg_floor_end_%u GF@%%cg_str_right bool@false\n"
                "SUB GF@%%cg_str_left GF@%%cg_str_left float@0x1p+0\n"
                "LABEL $$cg_floor_end_%u\n",
                id, id);

        fputs("PUSHS GF@%cg_str_left\n", ctx->output);
    }

    else
    {
        ctx->error_code = 99;
    }
}

static void generate_list_concat(AST_NODE *node, CodegenContext *ctx)
{
    unsigned int id = ctx->label_counter++;

    generate_expression(node->left_ch, ctx);
    if (ctx->error_code != 0)
        return;

    generate_expression(node->right_ch, ctx);
    if (ctx->error_code != 0)
        return;

    fputs("CREATEFRAME\nPUSHFRAME\n"
          "DEFVAR LF@%cg_left\nDEFVAR LF@%cg_right\n"
          "DEFVAR LF@%cg_ll\nDEFVAR LF@%cg_rl\n"
          "DEFVAR LF@%cg_size\nDEFVAR LF@%cg_dst\n"
          "DEFVAR LF@%cg_i\nDEFVAR LF@%cg_pos\n"
          "DEFVAR LF@%cg_item\nDEFVAR LF@%cg_cmp\n"
          "POPS LF@%cg_right\nPOPS LF@%cg_left\n"
          "ARRAYGET LF@%cg_ll LF@%cg_left int@0\n"
          "ARRAYGET LF@%cg_rl LF@%cg_right int@0\n"
          "ADD LF@%cg_size LF@%cg_ll LF@%cg_rl\n"
          "ADD LF@%cg_pos LF@%cg_size int@1\n"
          "NEWARRAY LF@%cg_dst LF@%cg_pos\n"
          "ARRAYSET LF@%cg_dst int@0 LF@%cg_size\n"
          "MOVE LF@%cg_i int@1\n"
          "MOVE LF@%cg_pos int@1\n",
          ctx->output);

    fprintf(ctx->output, "LABEL $$cg_concat_l_%u\n", id);

    fputs("GT LF@%cg_cmp LF@%cg_i LF@%cg_ll\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_concat_r_init_%u "
            "LF@%%cg_cmp bool@true\n",
            id);

    fputs("ARRAYGET LF@%cg_item LF@%cg_left LF@%cg_i\n"
          "ARRAYSET LF@%cg_dst LF@%cg_pos LF@%cg_item\n"
          "ADD LF@%cg_i LF@%cg_i int@1\n"
          "ADD LF@%cg_pos LF@%cg_pos int@1\n",
          ctx->output);

    fprintf(ctx->output, "JUMP $$cg_concat_l_%u\n", id);

    fprintf(ctx->output, "LABEL $$cg_concat_r_init_%u\n", id);

    fputs("MOVE LF@%cg_i int@1\n", ctx->output);

    fprintf(ctx->output, "LABEL $$cg_concat_r_%u\n", id);

    fputs("GT LF@%cg_cmp LF@%cg_i LF@%cg_rl\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_concat_done_%u "
            "LF@%%cg_cmp bool@true\n",
            id);

    fputs("ARRAYGET LF@%cg_item LF@%cg_right LF@%cg_i\n"
          "ARRAYSET LF@%cg_dst LF@%cg_pos LF@%cg_item\n"
          "ADD LF@%cg_i LF@%cg_i int@1\n"
          "ADD LF@%cg_pos LF@%cg_pos int@1\n",
          ctx->output);

    fprintf(ctx->output,
            "JUMP $$cg_concat_r_%u\n"
            "LABEL $$cg_concat_done_%u\n",
            id, id);

    fputs("PUSHS LF@%cg_dst\n"
          "POPFRAME\n",
          ctx->output);
}

static void generate_list_head_tail(AST_NODE *node, CodegenContext *ctx, int tail)
{
    unsigned int id = ctx->label_counter++;

    generate_expression(node->left_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    fputs("CREATEFRAME\nPUSHFRAME\n"
          "DEFVAR LF@%cg_src\n"
          "DEFVAR LF@%cg_len\n"
          "DEFVAR LF@%cg_value\n"
          "POPS LF@%cg_src\n"
          "ARRAYGET LF@%cg_len LF@%cg_src int@0\n",
          ctx->output);

    fprintf(ctx->output,
            "JUMPIFNEQ $$cg_ht_ok_%u LF@%%cg_len int@0\n"
            "EXIT int@40\n"
            "LABEL $$cg_ht_ok_%u\n",
            id, id);

    if (!tail)
    {
        fputs("ARRAYGET LF@%cg_value LF@%cg_src int@1\n"
              "PUSHS LF@%cg_value\n"
              "POPFRAME\n",
              ctx->output);

        return;
    }

    fputs("DEFVAR LF@%cg_newlen\n"
          "DEFVAR LF@%cg_result\n"
          "DEFVAR LF@%cg_i\n"
          "DEFVAR LF@%cg_j\n"
          "DEFVAR LF@%cg_cmp\n"

          "SUB LF@%cg_newlen LF@%cg_len int@1\n"
          "NEWARRAY LF@%cg_result LF@%cg_len\n"
          "ARRAYSET LF@%cg_result int@0 LF@%cg_newlen\n"
          "MOVE LF@%cg_i int@1\n",
          ctx->output);

    fprintf(ctx->output, "LABEL $$cg_tail_loop_%u\n", id);

    fputs("GT LF@%cg_cmp LF@%cg_i LF@%cg_newlen\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_tail_done_%u "
            "LF@%%cg_cmp bool@true\n",
            id);

    fputs("ADD LF@%cg_j LF@%cg_i int@1\n"
          "ARRAYGET LF@%cg_value LF@%cg_src LF@%cg_j\n"
          "ARRAYSET LF@%cg_result LF@%cg_i LF@%cg_value\n"
          "ADD LF@%cg_i LF@%cg_i int@1\n",
          ctx->output);

    fprintf(ctx->output,
            "JUMP $$cg_tail_loop_%u\n"
            "LABEL $$cg_tail_done_%u\n",
            id, id);

    fputs("PUSHS LF@%cg_result\n"
          "POPFRAME\n",
          ctx->output);
}

static void emit_list_slice_from_stack(CodegenContext *ctx, int mode)
{
    unsigned int id = ctx->label_counter++;

    fputs("CREATEFRAME\nPUSHFRAME\n"
          "DEFVAR LF@%cg_src\n"
          "DEFVAR LF@%cg_len\n"
          "DEFVAR LF@%cg_start\n"
          "DEFVAR LF@%cg_stop\n"
          "DEFVAR LF@%cg_count\n"
          "DEFVAR LF@%cg_size\n"
          "DEFVAR LF@%cg_dst\n"
          "DEFVAR LF@%cg_i\n"
          "DEFVAR LF@%cg_si\n"
          "DEFVAR LF@%cg_di\n"
          "DEFVAR LF@%cg_value\n"
          "DEFVAR LF@%cg_cmp\n",
          ctx->output);

    if (mode == 3)
    {
        fputs("POPS LF@%cg_stop\n"
              "POPS LF@%cg_start\n"
              "POPS LF@%cg_src\n",
              ctx->output);
    }

    else if (mode == 1)
    {
        fputs("POPS LF@%cg_stop\n"
              "POPS LF@%cg_src\n"
              "MOVE LF@%cg_start int@0\n",
              ctx->output);
    }

    else
    {
        fputs("POPS LF@%cg_start\n"
              "POPS LF@%cg_src\n",
              ctx->output);
    }

    fputs("ARRAYGET LF@%cg_len LF@%cg_src int@0\n", ctx->output);

    if (mode == 2)
    {
        fputs("MOVE LF@%cg_stop LF@%cg_len\n", ctx->output);
    }

    fputs("LT LF@%cg_cmp LF@%cg_start int@0\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_slice_start_nonneg_%u "
            "LF@%%cg_cmp bool@false\n",
            id);

    fputs("MOVE LF@%cg_start int@0\n", ctx->output);

    fprintf(ctx->output, "LABEL $$cg_slice_start_nonneg_%u\n", id);

    fputs("GT LF@%cg_cmp LF@%cg_start LF@%cg_len\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_slice_start_clamped_%u "
            "LF@%%cg_cmp bool@false\n",
            id);

    fputs("MOVE LF@%cg_start LF@%cg_len\n", ctx->output);

    fprintf(ctx->output, "LABEL $$cg_slice_start_clamped_%u\n", id);

    fputs("LT LF@%cg_cmp LF@%cg_stop int@0\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_slice_end_nonneg_%u "
            "LF@%%cg_cmp bool@false\n",
            id);

    fputs("MOVE LF@%cg_stop int@0\n", ctx->output);

    fprintf(ctx->output, "LABEL $$cg_slice_end_nonneg_%u\n", id);

    fputs("GT LF@%cg_cmp LF@%cg_stop LF@%cg_len\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_slice_end_clamped_%u "
            "LF@%%cg_cmp bool@false\n",
            id);

    fputs("MOVE LF@%cg_stop LF@%cg_len\n", ctx->output);

    fprintf(ctx->output, "LABEL $$cg_slice_end_clamped_%u\n", id);

    fputs("LT LF@%cg_cmp LF@%cg_stop LF@%cg_start\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_slice_ordered_%u "
            "LF@%%cg_cmp bool@false\n",
            id);

    fputs("MOVE LF@%cg_stop LF@%cg_start\n", ctx->output);

    fprintf(ctx->output, "LABEL $$cg_slice_ordered_%u\n", id);

    fputs("SUB LF@%cg_count LF@%cg_stop LF@%cg_start\n"
          "ADD LF@%cg_size LF@%cg_count int@1\n"
          "NEWARRAY LF@%cg_dst LF@%cg_size\n"
          "ARRAYSET LF@%cg_dst int@0 LF@%cg_count\n"
          "MOVE LF@%cg_i int@0\n",
          ctx->output);

    fprintf(ctx->output, "LABEL $$cg_slice_loop_%u\n", id);

    fputs("LT LF@%cg_cmp LF@%cg_i LF@%cg_count\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_slice_done_%u "
            "LF@%%cg_cmp bool@false\n",
            id);

    fputs("ADD LF@%cg_si LF@%cg_i LF@%cg_start\n"
          "ADD LF@%cg_si LF@%cg_si int@1\n"
          "ADD LF@%cg_di LF@%cg_i int@1\n"
          "ARRAYGET LF@%cg_value LF@%cg_src LF@%cg_si\n"
          "ARRAYSET LF@%cg_dst LF@%cg_di LF@%cg_value\n"
          "ADD LF@%cg_i LF@%cg_i int@1\n",
          ctx->output);

    fprintf(ctx->output,
            "JUMP $$cg_slice_loop_%u\n"
            "LABEL $$cg_slice_done_%u\n",
            id, id);

    fputs("PUSHS LF@%cg_dst\n"
          "POPFRAME\n",
          ctx->output);
}

void codegen_generate_list_method_expression(AST_NODE *node, CodegenContext *ctx)
{
    AST_NODE *access = node->left_ch;

    const char *method = access->right_ch->token.attr.str_val;

    int mode = strcmp(method, "take") == 0 ? 1 : strcmp(method, "drop") == 0 ? 2 : 3;

    generate_expression(access->left_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    AST_NODE *arg = node->right_ch;

    for (; arg != NULL && arg->left_ch != NULL; arg = arg->right_ch)
    {
        generate_expression(arg->left_ch, ctx);

        if (ctx->error_code != 0)
        {
            return;
        }
    }

    emit_list_slice_from_stack(ctx, mode);
}

void codegen_list_range_from_stack(CodegenContext *ctx, int inclusive)
{
    unsigned int id = ctx->label_counter++;

    fputs("CREATEFRAME\n"
          "PUSHFRAME\n"
          "DEFVAR LF@%cg_start\n"
          "DEFVAR LF@%cg_end\n"
          "DEFVAR LF@%cg_step\n"
          "DEFVAR LF@%cg_cur\n"
          "DEFVAR LF@%cg_count\n"
          "DEFVAR LF@%cg_alloc\n"
          "DEFVAR LF@%cg_result\n"
          "DEFVAR LF@%cg_i\n"
          "DEFVAR LF@%cg_neg\n"
          "DEFVAR LF@%cg_cmp\n"
          "POPS LF@%cg_step\n"
          "POPS LF@%cg_end\n"
          "POPS LF@%cg_start\n",
          ctx->output);

    fprintf(ctx->output,
            "JUMPIFNEQ $$cg_range_valid_%u LF@%%cg_step int@0\n"
            "EXIT int@40\n"
            "LABEL $$cg_range_valid_%u\n",
            id, id);

    fputs("LT LF@%cg_neg LF@%cg_step int@0\n"
          "MOVE LF@%cg_cur LF@%cg_start\n"
          "MOVE LF@%cg_count int@0\n",
          ctx->output);

    fprintf(ctx->output,
            "LABEL $$cg_range_count_%u\n"
            "JUMPIFEQ $$cg_range_count_negative_%u "
            "LF@%%cg_neg bool@true\n",
            id, id);

    fprintf(ctx->output,
            "%s LF@%%cg_cmp LF@%%cg_cur LF@%%cg_end\n"
            "JUMPIFEQ $$cg_range_count_done_%u "
            "LF@%%cg_cmp bool@%s\n"
            "JUMP $$cg_range_count_next_%u\n"
            "LABEL $$cg_range_count_negative_%u\n",
            inclusive ? "GT" : "LT", id, inclusive ? "true" : "false", id, id);

    fprintf(ctx->output,
            "%s LF@%%cg_cmp LF@%%cg_cur LF@%%cg_end\n"
            "JUMPIFEQ $$cg_range_count_done_%u "
            "LF@%%cg_cmp bool@%s\n"
            "LABEL $$cg_range_count_next_%u\n",
            inclusive ? "LT" : "GT", id, inclusive ? "true" : "false", id);

    fputs("ADD LF@%cg_count LF@%cg_count int@1\n"
          "ADD LF@%cg_cur LF@%cg_cur LF@%cg_step\n",
          ctx->output);

    fprintf(ctx->output,
            "JUMP $$cg_range_count_%u\n"
            "LABEL $$cg_range_count_done_%u\n",
            id, id);

    fputs("ADD LF@%cg_alloc LF@%cg_count int@1\n"
          "NEWARRAY LF@%cg_result LF@%cg_alloc\n"
          "ARRAYSET LF@%cg_result int@0 LF@%cg_count\n"
          "MOVE LF@%cg_cur LF@%cg_start\n"
          "MOVE LF@%cg_i int@1\n",
          ctx->output);

    fprintf(ctx->output, "LABEL $$cg_range_fill_%u\n", id);

    fputs("GT LF@%cg_cmp LF@%cg_i LF@%cg_count\n", ctx->output);

    fprintf(ctx->output,
            "JUMPIFEQ $$cg_range_done_%u "
            "LF@%%cg_cmp bool@true\n",
            id);

    fputs("ARRAYSET LF@%cg_result LF@%cg_i LF@%cg_cur\n"
          "ADD LF@%cg_cur LF@%cg_cur LF@%cg_step\n"
          "ADD LF@%cg_i LF@%cg_i int@1\n",
          ctx->output);

    fprintf(ctx->output,
            "JUMP $$cg_range_fill_%u\n"
            "LABEL $$cg_range_done_%u\n",
            id, id);

    fputs("PUSHS LF@%cg_result\n"
          "POPFRAME\n",
          ctx->output);
}

void generate_expression(AST_NODE *node, CodegenContext *ctx)
{
    if (node == NULL || ctx == NULL || ctx->error_code != 0)
    {
        return;
    }

    if (node->type == NODE_IF)
    {
        generate_if_expression(node, ctx);
        return;
    }

    if (node->type == NODE_CALL)
    {
        codegen_generate_call_expression(node, ctx);
        return;
    }

    if (node->type == NODE_VALMEM)
    {
        generate_valmem_expression(node, ctx);
        return;
    }

    if (node->type != NODE_TOKEN_ONLY)
    {
        ctx->error_code = 99;
        return;
    }

    Token *token = &node->token;

    switch (token->type)
    {
        case TOKEN_TYPE_PLUS:
        case TOKEN_TYPE_MINUS:
        case TOKEN_TYPE_MUL:
        case TOKEN_TYPE_DIV:
            generate_numeric_operation(node, ctx);
            break;

        case TOKEN_TYPE_INT:
        case TOKEN_TYPE_DOUBLE:
        case TOKEN_TYPE_BOOL:
        case TOKEN_TYPE_STRING:
        case TOKEN_TYPE_UNIT:
            generate_literal(node, ctx);
            break;

        case TOKEN_TYPE_IDENTIFIER:
            generate_variable(node, ctx);
            break;

        case TOKEN_TYPE_EQ:
        case TOKEN_TYPE_NEQ:
        case TOKEN_TYPE_LT:
        case TOKEN_TYPE_LTE:
        case TOKEN_TYPE_GT:
        case TOKEN_TYPE_GTE:
            generate_comparison(node, ctx);
            break;

        case TOKEN_TYPE_NOT:
        case TOKEN_TYPE_AND:
        case TOKEN_TYPE_OR:
            generate_boolean_operation(node, ctx);
            break;

        case TOKEN_TYPE_CONCAT:
            generate_list_concat(node, ctx);
            break;

        default:
            ctx->error_code = 99;
            break;
    }
}

static void generate_control_branch_value(AST_NODE *body, CodegenType result_type, CodegenContext *ctx)
{
    if (body == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    while (body->type == NODE_PROGRAM && body->right_ch != NULL)
    {
        codegen_generate_node(body->left_ch, ctx);

        if (ctx->error_code != 0)
        {
            return;
        }

        body = body->right_ch;
    }

    if (body->type == NODE_PROGRAM)
    {
        body = body->left_ch;
    }

    if (body == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    CodegenType actual_type = control_branch_type(body, ctx);

    int is_return = body->type == NODE_TOKEN_ONLY && body->token.type == KEYWORD_RETURN;

    int is_value =
        body->type == NODE_CALL || body->type == NODE_VALMEM ||
        (body->type == NODE_IF && actual_type != CG_TYPE_UNKNOWN) ||
        (body->type == NODE_TOKEN_ONLY && body->token.type != TOKEN_TYPE_ASSIGN && body->token.type != KEYWORD_RETURN);

    if (is_value)
    {
        generate_expression(body, ctx);
    }

    else
    {
        codegen_generate_node(body, ctx);

        if (ctx->error_code != 0)
        {
            return;
        }

        if (!is_return)
        {
            fputs("PUSHS nil@nil\n", ctx->output);
        }
    }

    if (ctx->error_code != 0)
    {
        return;
    }

    if (result_type == CG_TYPE_DOUBLE && actual_type == CG_TYPE_INT)
    {
        fputs("INT2FLOATS\n", ctx->output);
    }
}

static void generate_if_expression(AST_NODE *node, CodegenContext *ctx)
{
    if (node == NULL || node->left_ch == NULL || node->right_ch == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    AST_NODE *pair = node->left_ch;
    AST_NODE *else_body = node->right_ch;

    if (else_body->type == NODE_ELSE)
    {
        else_body = else_body->left_ch;
    }

    if (pair->left_ch == NULL || pair->right_ch == NULL || else_body == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    CodegenType result_type = codegen_expression_type(node, ctx);
    int previous_force = ctx->force_list_double;
    int force_list_double =
        result_type == CG_TYPE_LIST && (ctx->force_list_double || ctx->current_function_list_double);

    unsigned int id = ctx->label_counter++;

    generate_expression(pair->left_ch, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    fprintf(ctx->output,
            "PUSHS bool@false\n"
            "JUMPIFEQS $$if_expr_else_%u\n",
            id);

    codegen_enter_scope(ctx);
    ctx->force_list_double = force_list_double;
    generate_control_branch_value(pair->right_ch, result_type, ctx);
    codegen_leave_scope(ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    fprintf(ctx->output,
            "JUMP $$if_expr_end_%u\n"
            "LABEL $$if_expr_else_%u\n",
            id, id);

    codegen_enter_scope(ctx);
    ctx->force_list_double = force_list_double;
    generate_control_branch_value(else_body, result_type, ctx);
    codegen_leave_scope(ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    fprintf(ctx->output, "LABEL $$if_expr_end_%u\n", id);
    ctx->force_list_double = previous_force;
}

void generate_definition(AST_NODE *node, CodegenContext *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->error_code != 0)
    {
        return;
    }

    if (node == NULL || ctx->output == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    if (node->type != NODE_VAR_DEF && node->type != NODE_VAL_DEF)
    {
        ctx->error_code = 99;
        return;
    }

    AST_NODE *identifier = node->left_ch;
    AST_NODE *expression = node->right_ch;

    if (identifier == NULL || expression == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    if (identifier->type != NODE_TOKEN_ONLY || identifier->token.type != TOKEN_TYPE_IDENTIFIER ||
        identifier->token.attr.str_val == NULL || identifier->token.attr.str_val[0] == '\0')
    {
        ctx->error_code = 99;
        return;
    }

    codegen_declare_variable(identifier, ctx);

    const char *name = codegen_variable_name(identifier, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    if (expression->type == NODE_CALL && expression->left_ch != NULL && expression->left_ch->token.type == KEYWORD_LIST)
    {

        CodegenType element_type = codegen_list_item_type(identifier, ctx);

        if (element_type == CG_TYPE_UNKNOWN)
        {
            for (AST_NODE *arg = expression->right_ch; arg != NULL && arg->left_ch != NULL; arg = arg->right_ch)
            {
                CodegenType type = codegen_expression_type(arg->left_ch, ctx);

                if (element_type == CG_TYPE_UNKNOWN || type == CG_TYPE_DOUBLE)
                {
                    element_type = type;
                }
            }
        }

        codegen_set_list_item_type(identifier, ctx, element_type);
    }

    if (codegen_list_item_type(identifier, ctx) == CG_TYPE_UNKNOWN)
    {
        CodegenType item_type = codegen_list_item_type(expression, ctx);

        if (item_type != CG_TYPE_UNKNOWN)
        {
            codegen_set_list_item_type(identifier, ctx, item_type);
        }
    }

    int previous_force = ctx->force_list_double;

    if (codegen_list_item_type(identifier, ctx) == CG_TYPE_DOUBLE &&
        (expression->type == NODE_IF ||
         (expression->type == NODE_CALL && expression->left_ch != NULL &&
          expression->left_ch->type == NODE_TOKEN_ONLY && expression->left_ch->token.type == KEYWORD_LIST)))
    {
        ctx->force_list_double = 1;
    }

    generate_expression(expression, ctx);

    ctx->force_list_double = previous_force;

    if (ctx->error_code != 0)
    {
        return;
    }

    CodegenType declare_type = get_declared_type(identifier, ctx);
    CodegenType expression_type = get_expression_type(expression, ctx);

    if (declare_type == CG_TYPE_UNKNOWN)
    {
        declare_type = expression_type;
    }

    codegen_set_variable_type(identifier, ctx, declare_type);

    if (declare_type == CG_TYPE_DOUBLE)
    {
        if (expression_type == CG_TYPE_INT)
        {
            fprintf(ctx->output, "INT2FLOATS\n");
        }

        else if (expression_type != CG_TYPE_DOUBLE)
        {
            ctx->error_code = 99;
            return;
        }
    }

    if (fprintf(ctx->output, "POPS LF@%s\n", name) < 0)
    {
        ctx->error_code = 99;
    }
}

void generate_assignment(AST_NODE *identifier, AST_NODE *expression, CodegenContext *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->error_code != 0)
    {
        return;
    }

    if (identifier == NULL || expression == NULL || ctx->output == NULL)
    {
        ctx->error_code = 99;
        return;
    }

    if (identifier->type != NODE_TOKEN_ONLY || identifier->token.type != TOKEN_TYPE_IDENTIFIER ||
        identifier->token.attr.str_val == NULL || identifier->token.attr.str_val[0] == '\0')
    {
        ctx->error_code = 99;
        return;
    }

    CodegenType variable_type = get_declared_type(identifier, ctx);

    if (variable_type == CG_TYPE_UNKNOWN)
    {
        variable_type = get_expression_type(identifier, ctx);
    }

    CodegenType expression_type = get_expression_type(expression, ctx);

    if (variable_type == CG_TYPE_UNKNOWN || expression_type == CG_TYPE_UNKNOWN)
    {
        ctx->error_code = 99;
        return;
    }

    int convert = variable_type == CG_TYPE_DOUBLE && expression_type == CG_TYPE_INT;

    if (variable_type != expression_type && !convert)
    {
        ctx->error_code = 99;
        return;
    }

    int previous_force = ctx->force_list_double;

    if (variable_type == CG_TYPE_LIST && codegen_list_item_type(identifier, ctx) == CG_TYPE_DOUBLE &&
        (expression->type == NODE_IF ||
         (expression->type == NODE_CALL && expression->left_ch != NULL &&
          expression->left_ch->type == NODE_TOKEN_ONLY && expression->left_ch->token.type == KEYWORD_LIST)))
    {
        ctx->force_list_double = 1;
    }

    generate_expression(expression, ctx);
    ctx->force_list_double = previous_force;

    if (ctx->error_code != 0)
    {
        return;
    }

    if (convert)
    {
        fprintf(ctx->output, "INT2FLOATS\n");
    }

    const char *name = codegen_variable_name(identifier, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    fprintf(ctx->output, "POPS LF@%s\n", name);
}

// FB0
