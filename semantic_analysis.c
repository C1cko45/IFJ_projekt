/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#include <stdbool.h>
#include <stdio.h>

#include "ast.h"
#include "semantic_analysis.h"
#include "symtable.h"
#include "token.h"

#define ERROR_UNDEF 3     // use of undef function/var/type; missing @main-tagged function
#define ERROR_REDEF 4     // illegal re-def function/var/type
#define ERROR_PARAM 5     // incorrect count/type of arguments in function call; illegal @main-tagged function params
#define ERROR_TYPE 6      // type incompat | incorrect type infer in expr, assign, init, return type, control flow
#define ERROR_IMMUTABLE 7 // assign attempt to immutable variable/function parameter
#define ERROR_OTHER 10    // other semantic errors; (illegal return, non-exhaustive match, range with 0-step, ...)
#define ERROR_INTERNAL 99 // internal program error; failed memory allocation, ...

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Pushes builtin functions into SymTable's global scope
 * Should be called after initializing a SymTable instance for the global scope and function def body local scopes
 *
 * Assumes initialized SymTable with initialized global scope
 */
static void insertBuiltins(SymTable *sym_table)
{
    HTEntry builtin_function = {.state = BUSY,
                                .identifier = "print",
                                .entry_type = FUNCTION,
                                .entry_data.function.param_count = 0,
                                .entry_data.function.return_type = UNIT};
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "head";
    builtin_function.entry_data.function.return_type = ANY;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "tail";
    builtin_function.entry_data.function.return_type = LIST;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "toList";
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "readLine";
    builtin_function.entry_data.function.return_type = STRING;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "readInt";
    builtin_function.entry_data.function.return_type = INT;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "readDouble";
    builtin_function.entry_data.function.return_type = DOUBLE;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "readBoolean";
    builtin_function.entry_data.function.return_type = BOOLEAN;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "toStr";
    builtin_function.entry_data.function.param_count = 1;
    builtin_function.entry_data.function.return_type = STRING;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "chr";
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "drop";
    builtin_function.entry_data.function.return_type = LIST;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "take";
    builtin_function.entry_data.function.return_type = LIST;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "toInt";
    builtin_function.entry_data.function.return_type = INT;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "strLength";
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "ord";
    builtin_function.entry_data.function.param_count = 2;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "subStr";
    builtin_function.entry_data.function.param_count = 3;
    builtin_function.entry_data.function.return_type = STRING;
    createIdentifier(&builtin_function, sym_table);

    // Dubious one, language spec supports builtin range(start, end) AND builtin range(start,end,step)
    // -> Which normally leads to re-definition error in SymTable
    // Language spec itself explicitly doesn't support function overloading, like at all
    // So I just decided to insert range(start,end,step), will have to handle this builtin function separately
    // knowing that range().paramCount == 3 in SymTable, but, in reality, can be either 2 or 3
    builtin_function.identifier = "range";
    builtin_function.entry_data.function.return_type = LIST;
    createIdentifier(&builtin_function, sym_table);
}

/*
 * Returns count of function's number of parameters
 */
static size_t countParams(AST_NODE *param_node)
{
    size_t param_count = 0;
    while (param_node != NULL)
    {
        ++param_count;
        param_node = param_node->right_ch;
    }

    return param_count;
}

/*
 * Returns function's return type
 *
 * If it's passed the wrong argument node, crashes the whole compilation
 */
static DataType functionReturnType(AST_NODE *ret_type_node)
{
    switch (ret_type_node->token.type)
    {
        case KEYWORD_STRING:
            return STRING;
        case KEYWORD_DOUBLE:
            return DOUBLE;
        case KEYWORD_INT:
            return INT;
        case KEYWORD_BOOLEAN:
            return BOOLEAN;
        case KEYWORD_LIST:
            return LIST;
        case KEYWORD_UNIT:
            return UNIT;
        default:
            fprintf(stderr, "functionReturnType() -- called on wrong AST Node.\n");
            // TODO: cleanup()
            exit(ERROR_INTERNAL);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Traverse top-level scope of AST to find @main-tagged function
 * Also checks and guarantees top-level scope contains only function definitions
 *
 * Crashes the compilation if at least one requirement for @main-tagged function and top-level scope is not met
 */
static void checkMain(AST_NODE *root, SymTable *sym_table)
{
    bool main_found = false;
    while (root != NULL)
    {
        if (root->type != NODE_PROGRAM)
        {
            fprintf(stderr, "checkMain() -- top-level scope contains a non-statement\n");
            // TODO: cleanup()
            exit(ERROR_OTHER);
        }
        if (root->left_ch->type != NODE_MAIN || root->left_ch->type != NODE_FUNC_DEF)
        {
            fprintf(stderr, "checkMain() -- top-level scope contains an unsupported statement\n");
            // TODO: cleanup()
            exit(ERROR_OTHER);
        }

        if (root->left_ch->type == NODE_MAIN)
        {
            if (main_found)
            {
                fprintf(stderr, "checkMain() -- Program contains more than one @main-tagged functions\n");
                // TODO: cleanup()
                exit(ERROR_REDEF);
            }

            main_found = true;

            if (root->left_ch->left_ch->left_ch->left_ch->token.type != KEYWORD_UNIT)
            {
                fprintf(stderr, "checkMain() -- @main-tagged function has wrong return type\n");
                // TODO: cleanup()
                exit(ERROR_TYPE);
            }

            if (root->left_ch->left_ch->left_ch->right_ch != NULL)
            {
                fprintf(stderr, "checkMain() -- non-parametric @main-tagged function contains parameters\n");
                // TODO: cleanup()
                exit(ERROR_PARAM);
            }
        }

        root = root->right_ch;
    }

    if (!main_found)
    {
        fprintf(stderr, "checkMain() -- no @main-tagged function in program\n");
        // TODO: cleanup()
        exit(ERROR_UNDEF);
    }
}
/*
 * Fill sym_table's global scope with all function names in top-level scope
 *
 * Should be called before semantic analysis (to support function calls before their respective definition)
 */
static void insertAllFunctions(AST_NODE *root, SymTable *sym_table)
{
    HTEntry function_entry = {0};
    while (root != NULL)
    {
        if (root->left_ch->type == NODE_MAIN)
        {
            function_entry.identifier = root->left_ch->left_ch->left_ch->token.attr.str_val;
            function_entry.entry_type = FUNCTION;
            function_entry.entry_data.function.return_type = UNIT;
            function_entry.entry_data.function.param_count = 0;
            createIdentifier(&function_entry, sym_table);
        }

        if (root->left_ch->type == NODE_FUNC_DEF)
        {
            function_entry.identifier = root->left_ch->left_ch->token.attr.str_val;
            function_entry.entry_type = FUNCTION;
            function_entry.entry_data.function.return_type = functionReturnType(root->left_ch->left_ch->left_ch);
            function_entry.entry_data.function.param_count = countParams(root->left_ch->left_ch->right_ch);
            createIdentifier(&function_entry, sym_table);
        }

        root = root->right_ch;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Central statement dispatcher -> calls appropriate function for NODE_PROGRAM's left child (its contents)
 *
 * This function should be used for NODE_PROGRAM inside function blocks (not in top-level scope)
 * (Types of statements allowed for top-level NODE_PROGRAM differs from NODE_PROGRAM inside function scope)
 */
static void programStatement(AST_NODE *program_node)
{
    switch (program_node->left_ch->type)
    {
        default:
            return;
    }
}
/*
 * Top-level statement dispatcher -> calls appropriate function for NODE_PROGRAM's left child (its contents)
 *
 * This function should be used for NODE_PROGRAM in top-level scope
 * (Types of statements allowed for top-level NODE_PROGRAM differs from NODE_PROGRAM inside function scope)
 */
static void topLevelStatement(AST_NODE *program_node)
{
    switch (program_node->left_ch->type)
    {
        default:
            return;
    }
}

static void functionDefinition(AST_NODE *function_node, SymTable *sym_table)
{
    pushScope(sym_table);
    // TODO: Just need to find identifier in symTable and that's it...
    // Also need to push params as immutable variables into function scope
    popScope(sym_table);
}

static void ifStatement(AST_NODE *if_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition (which can be any expression that evalutes to Boolean) first
    pushScope(sym_table);
    popScope(sym_table);
}
static void elseifStatement(AST_NODE *elseif_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition (which can be any expression that evalutes to Boolean) first
    pushScope(sym_table);
    popScope(sym_table);
}
static void elseStatement(AST_NODE *else_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition (which can be any expression that evalutes to Boolean) first
    pushScope(sym_table);
    popScope(sym_table);
}

static void whileStatement(AST_NODE *while_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition (which can be any expression that evalutes to Boolean) first
    pushScope(sym_table);
    popScope(sym_table);
}

static void forStatement(AST_NODE *for_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition, among other things in this complicated mess of a construct (LMAO)
    pushScope(sym_table);
    popScope(sym_table);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Entry point of semantic analysis
 */
void semanticAnalysis(AST_NODE *root)
{
    // Create and initialize sym_table with global scope
    SymTable *sym_table = createSymTable();
    insertBuiltins(sym_table);

    checkMain(root, sym_table);
    insertAllFunctions(root, sym_table);

    // Semantic analysis
    // Traverse AST PreOrder
    while (root != NULL)
    {
        // Process parent_node->left_child
        programStatement(root->left_ch);

        root = root->right_ch;
    }

    // Free any allocated memory in sym_table
    destroySymTable(sym_table);
}
