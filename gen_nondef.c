#include "gen_nondef.h"
#include <stdio.h>
#include <string.h>

static void generate_string_literal(AST_NODE *node, CodegenContext *ctx);

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

static CodegenType get_expression_type(const AST_NODE *node, const CodegenContext *ctx)
{
    if (node == NULL || ctx == NULL)
    {
        return CG_TYPE_UNKNOWN;
    }

    if (node->type == NODE_CALL)
    {
        return codegen_builtin_return_type(node);
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

    case TOKEN_TYPE_IDENTIFIER:
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
        if (node->left_ch != NULL && node->right_ch == NULL && get_expression_type(node->left_ch, ctx) == CG_TYPE_BOOL)
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

    default:
        return CG_TYPE_UNKNOWN;
    }
}

CodegenType codegen_expression_type(const AST_NODE *node, const CodegenContext *ctx)
{
    return get_expression_type(node, ctx);
}

static CodegenType get_declared_type(const AST_NODE *identifier)
{
    if (identifier == NULL || identifier->left_ch == NULL)
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

    if (fprintf(ctx->output, "PUSHS LF@%s\n", node->token.attr.str_val) < 0)
    {
        ctx->error_code = 99;
    }
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

void generate_expression(AST_NODE *node, CodegenContext *ctx)
{
    if (node == NULL || ctx == NULL || ctx->error_code != 0)
    {
        return;
    }

    if (node->type == NODE_CALL)
    {
        codegen_generate_builtin_expression(node, ctx);
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

    default:
        ctx->error_code = 99;
        break;
    }
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

    const char *name = identifier->token.attr.str_val;

    generate_expression(expression, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    CodegenType declare_type = get_declared_type(identifier);

    if (declare_type == CG_TYPE_DOUBLE)
    {
        CodegenType expression_type = get_expression_type(expression, ctx);

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

    CodegenType variable_type = get_expression_type(identifier, ctx);

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

    generate_expression(expression, ctx);

    if (ctx->error_code != 0)
    {
        return;
    }

    if (convert)
    {
        fprintf(ctx->output, "INT2FLOATS\n");
    }

    if (fprintf(ctx->output, "POPS LF@%s\n", identifier->token.attr.str_val) < 0)
    {
        ctx->error_code = 99;
    }
}

// FB0
