/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static size_t countParams(AST_NODE *param_node);
static DataType resolveDataType(AST_NODE *target_node);
static void pushParamsIntoScope(AST_NODE *param_node, SymTable *sym_table);
static void annotateAST(HTEntry *function_entry, AST_NODE *identifier_node);

static void insertBuiltins(SymTable *sym_table);
static void checkMain(AST_NODE *root, SymTable *sym_table);
static void insertAllFunctions(AST_NODE *root, SymTable *sym_table);

static void functionDef(AST_NODE *function_node, SymTable *sym_table);
static DataType functionCall(AST_NODE *call_node, SymTable *sym_table);

static DataType programStatement(AST_NODE *program_node, HTEntry *function_entry, SymTable *sym_table);
static void topLevelStatement(AST_NODE *program_node, SymTable *sym_table);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Pushes builtin functions into SymTable's global scope
 * Should be called after initializing a SymTable instance for the global scope and function def body local scopes
 *
 * Assumes initialized SymTable with initialized global scope
 */
static void insertBuiltins(SymTable *sym_table)
{
    if (sym_table == NULL)
    {
        fprintf(stderr, "insertBuiltins() -- passed sym_table argument is NULL.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }
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
    builtin_function.entry_data.function.return_type = LIST_ANY;
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
    builtin_function.entry_data.function.return_type = LIST_ANY;
    createIdentifier(&builtin_function, sym_table);

    builtin_function.identifier = "take";
    builtin_function.entry_data.function.return_type = LIST_ANY;
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
    builtin_function.entry_data.function.return_type = LIST_ANY;
    createIdentifier(&builtin_function, sym_table);
}

/*
 * Traverse top-level scope of AST to find @main-tagged function
 * Also checks and guarantees top-level scope contains only function definitions
 *
 * Crashes the compilation if at least one requirement for @main-tagged function and top-level scope is not met
 * Function assumes correct argument passing (if root == NULL -> end of IFJ26 program)
 */
static void checkMain(AST_NODE *root, SymTable *sym_table)
{
    if (sym_table == NULL)
    {
        fprintf(stderr, "checkMain() -- passed sym_table argument is NULL\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }
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
    if (root == NULL || sym_table == NULL)
    {
        fprintf(stderr, "insertAllFunctions() -- at least one passed argument is NULL\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    HTEntry function_entry = {0}; // Initialize HTEntry
    while (root != NULL)
    {
        if (root->left_ch->type == NODE_MAIN)
        {
            function_entry.identifier = root->left_ch->left_ch->left_ch->token.attr.str_val;
            function_entry.entry_type = FUNCTION;
            function_entry.entry_data.function.return_type = UNIT;
            function_entry.entry_data.function.param_count = 0;

            if (isIdentifier(function_entry.identifier, FUNCTION, sym_table))
            {
                fprintf(stderr, "insertAllFunctions() -- Function was already defined\n");
                // TODO: cleanup()
                exit(ERROR_REDEF);
            }
            createIdentifier(&function_entry, sym_table);
        }

        if (root->left_ch->type == NODE_FUNC_DEF)
        {
            function_entry.identifier = root->left_ch->left_ch->token.attr.str_val;
            function_entry.entry_type = FUNCTION;
            function_entry.entry_data.function.return_type = resolveDataType(root->left_ch->left_ch);
            function_entry.entry_data.function.param_count = countParams(root->left_ch->left_ch->right_ch);

            if (isIdentifier(function_entry.identifier, FUNCTION, sym_table))
            {
                fprintf(stderr, "insertAllFunctions() -- Function was previously defined\n");
                // TODO: cleanup()
                exit(ERROR_REDEF);
            }
            createIdentifier(&function_entry, sym_table);
        }

        root = root->right_ch;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Function assumes correct call sight --> function definition doesn't happen inside function body
 */
static void functionDef(AST_NODE *function_node, SymTable *sym_table)
{
    // Argument error-handling
    if (function_node == NULL || sym_table == NULL)
    {
        fprintf(stderr, "functionDef() -- at least one argument is invalid.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // Create new local scope for function body
    pushScope(sym_table);

    // Push every parameter into its function scope as immutable variable
    pushParamsIntoScope(function_node->left_ch->right_ch, sym_table);
    HTEntry *function_entry = getIdentifier(function_node->left_ch->token.attr.str_val, FUNCTION, sym_table);

    // Process function block (loop right childs until NULL -> end of function body)
    // While looping, if check for for potential return statements anywhere and compare return value with return type
    // Process last programStatement() separately -> must check whether it's return statement or expression
    // In case of expression, evaluate its return value type and compare with function return type
    AST_NODE *program_node = function_node->right_ch;
    while (program_node != NULL)
    {
        if (program_node->right_ch == NULL) // if last statement in function body
        {
            // TODO: Evaluate return value and compare its type with function return type
            DataType return_value_type = programStatement(program_node, function_entry, sym_table);
            if (return_value_type != function_entry->entry_data.function.return_type)
            {
                fprintf(stderr, "functionDef() -- return value type doesn't match function signature.\n");
                // TODO: cleanup()
                exit(ERROR_TYPE);
            }
        }

        programStatement(program_node, function_entry, sym_table);
        program_node = program_node->right_ch;
    }

    // Exit the current local scope of function body
    popScope(sym_table);
}

/*
 * Check that function was defined before use
 * Check that function argument count and argument types match function signature
 * Annotate ASTNode with info for codegen
 * Returns function's return value type
 */
static DataType functionCall(AST_NODE *call_node, SymTable *sym_table)
{
    if (sym_table == NULL || call_node == NULL)
    {
        fprintf(stderr, "functionCall() -- at least one argument is invalid.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    HTEntry *function_entry = getIdentifier(call_node->left_ch->token.attr.str_val, FUNCTION, sym_table);
    if (function_entry == NULL)
    {
        fprintf(stderr, "functionCall() -- call to undefined function.\n");
        // TODO: cleanup()
        exit(ERROR_UNDEF);
    }

    // Compare argument count with function signature
    size_t arg_count = countParams(call_node->right_ch);
    if (arg_count != function_entry->entry_data.function.param_count)
    {
        fprintf(stderr, "functionCall() -- call to undefined function.\n");
        // TODO: cleanup()
        exit(ERROR_PARAM);
    }

    // Check argument types -- can only be literals and identifiers
    while (arg_count-- > 0)
    {
        // TBD: Must re-design symtable's HTEntryData to include another struct field DataType *Parameters
        // Will contain ordered parameter types
        // Then implement in semantic_analysis a function that will walk the argument AST subtree and the HTEntry array
        ;
    }

    annotateAST(function_entry, call_node->left_ch);

    return function_entry->entry_data.function.return_type;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static DataType ifStatement(AST_NODE *if_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition (which can be any expression that evalutes to Boolean) first
    pushScope(sym_table);
    popScope(sym_table);
}

static DataType whileStatement(AST_NODE *while_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition (which can be any expression that evalutes to Boolean) first
    pushScope(sym_table);
    popScope(sym_table);
}

static DataType forStatement(AST_NODE *for_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition, among other things in this complicated mess of a construct (LMAO)
    pushScope(sym_table);
    popScope(sym_table);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Central statement dispatcher -> calls appropriate function for NODE_PROGRAM's left child (its contents)
 *
 * This function should be used for NODE_PROGRAM inside function blocks (not in top-level scope)
 * (Types of statements allowed for top-level NODE_PROGRAM differs from NODE_PROGRAM inside function scope)
 * Function assumes correct argument passing (if program_node == NULL -> end of IFJ26 program)
 */
static DataType programStatement(AST_NODE *program_node, HTEntry *function_entry, SymTable *sym_table)
{
    if (program_node == NULL || sym_table == NULL)
    {
        fprintf(stderr, "programStatement() -- at least one function argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    if (program_node->type != NODE_PROGRAM)
    {
        fprintf(stderr, "programStatement() -- passed program_node is not a statement\n");
        // TODO: cleanup()
        exit(ERROR_OTHER);
    }

    switch (program_node->left_ch->type)
    {
        case NODE_CALL:
            return functionCall(program_node->left_ch, sym_table);
        case (NODE_VAR_DEF || NODE_VAL_DEF):
            // TODO: variableDef(program_node->left_ch, sym_table);
        case NODE_WHILE:
            // TODO: return whileStatement(program_node->left_ch, sym_table);
        case NODE_FOR:
            // TODO: return forStatement(program_node->left_ch, sym_table);
        case NODE_IF:
            // TODO: return ifStatement(program_node->left_ch, sym_table);
        case NODE_VALMEM:
        case NODE_LIST:
            // TODO: return exprStatement(program_node->left_ch, sym_table);
        case NODE_TOKEN_ONLY:
            switch (program_node->left_ch->token.type)
            {
                case TOKEN_TYPE_PLUS:
                case TOKEN_TYPE_MINUS:
                case TOKEN_TYPE_MUL:
                case TOKEN_TYPE_DIV:
                    // TODO: return exprStatement(program_node->left_ch, sym_table);
                case TOKEN_TYPE_LT:
                case TOKEN_TYPE_LTE:
                case TOKEN_TYPE_GT:
                case TOKEN_TYPE_GTE:
                case TOKEN_TYPE_EQ:
                case TOKEN_TYPE_NEQ:
                case TOKEN_TYPE_AND:
                case TOKEN_TYPE_OR:
                case TOKEN_TYPE_NOT:
                    // TODO: return exprStatement(program_node->left_ch, sym_table);
                case KEYWORD_RETURN:
                    // TODO: return returnStatement(program_node->left_ch, sym_table);
                default:;
            }
        default:
            fprintf(stderr, "programStatement() -- Function contains statement of unsupported type\n");
            // TODO: cleanup()
            exit(ERROR_OTHER);
    }
}
/*
 * Top-level statement dispatcher -> calls appropriate function for NODE_PROGRAM's left child (its contents)
 *
 * This function should be used for NODE_PROGRAM in top-level scope
 * (Types of statements allowed for top-level NODE_PROGRAM differs from NODE_PROGRAM inside function scope)
 * Function assumes correct argument passing (if program_node == NULL -> end of IFJ26 program)
 */
static void topLevelStatement(AST_NODE *program_node, SymTable *sym_table)
{
    if (sym_table == NULL)
    {
        fprintf(stderr, "topLevelStatement() -- argument sym_table is NULL\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    if (program_node != NULL)
    {
        if (program_node->type != NODE_PROGRAM)
        {
            fprintf(stderr, "topLevelStatement() -- passed program_node is not a statement\n");
            // TODO: cleanup()
            exit(ERROR_OTHER);
        }

        switch (program_node->left_ch->type)
        {
            case NODE_MAIN:
                functionDef(program_node->left_ch->left_ch, sym_table);
            case NODE_FUNC_DEF:
                functionDef(program_node->left_ch->left_ch, sym_table);
            default:
                fprintf(stderr, "topLevelStatement() -- Top-level scope contains statement of unsupported type\n");
                // TODO: cleanup()
                exit(ERROR_OTHER);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Entry point of semantic analysis
 */
void semanticAnalysis(AST_NODE *root)
{
    // Create and initialize sym_table with global scope
    SymTable *sym_table = createSymTable();
    insertBuiltins(sym_table);

    // Traverses top-level scope for @main tagged function, also checks proper top-level structure (only func def's)
    checkMain(root, sym_table);
    // Fills sym_table with all func identifiers to support function call before its respective definition
    // Checks that top-level definitions don't have any function re-definitions
    insertAllFunctions(root, sym_table);

    // Semantic analysis
    // Traverse AST
    while (root != NULL)
    {
        // Process parent_node->left_child
        topLevelStatement(root, sym_table);

        root = root->right_ch;
    }

    // Free any allocated memory in sym_table
    destroySymTable(sym_table);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Returns count of function's number of parameters
 *
 * Function assumes correct argument passing (NULL means function has 0 parameters)
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
 * Returns target_node's type annotation (located at target_node->left_ch)
 *
 * Function assumes correct argument passed (target_node->left_ch == NULL -> AST node doesn't have explicit data type)
 * Please check the argument before calling this function thoroughly
 * Use mainly for getting function def's return type, variable definition's type annotation
 */
static DataType resolveDataType(AST_NODE *target_node)
{
    if (target_node == NULL)
    {
        fprintf(stderr, "resolveDataType() -- passed AST node argument is NULL.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // AST node doesn't have a type annotation
    if (target_node->left_ch != NULL)
        return NIL;

    switch (target_node->left_ch->token.type)
    {
        case KEYWORD_STRING:
            return STRING;
        case KEYWORD_DOUBLE:
            return DOUBLE;
        case KEYWORD_INT:
            return INT;
        case KEYWORD_BOOLEAN:
            return BOOLEAN;
        case KEYWORD_UNIT:
            return UNIT;
        case KEYWORD_LIST:
            switch (target_node->left_ch->left_ch->token.type)
            {
                case KEYWORD_STRING:
                    return LIST_STRING;
                case KEYWORD_DOUBLE:
                    return LIST_DOUBLE;
                case KEYWORD_INT:
                    return LIST_INT;
                case KEYWORD_BOOLEAN:
                    return LIST_BOOLEAN;
                case KEYWORD_UNIT:
                    return LIST_UNIT;
                // case KEYWORD_ANY: -- Seems unsupported by the lexer and parser
                //     return LIST_ANY;
                default:
                    fprintf(stderr, "resolveDataType() -- Function has indeterminate return type.\n");
                    // TODO: cleanup()
                    exit(ERROR_INTERNAL);
            }
        default:
            fprintf(stderr, "resolveDataType() -- called on wrong AST Node.\n");
            // TODO: cleanup()
            exit(ERROR_INTERNAL);
    }
}

/*
 * Helper function for functionDef()
 */
static void pushParamsIntoScope(AST_NODE *param_node, SymTable *sym_table)
{
    size_t param_count = countParams(param_node);

    HTEntry param_entry = {0};
    while (param_count-- > 0)
    {
        param_entry.state = BUSY;
        param_entry.identifier = param_node->token.attr.str_val;
        param_entry.entry_type = VAL;
        param_entry.entry_data.variable_data_type = resolveDataType(param_node);
        if (param_entry.entry_data.variable_data_type == NIL)
        {
            fprintf(stderr, "pushParamsIntoScope() -- function parameter doesn't have a supported type annotation.\n");
            // TODO: cleanup()
            exit(ERROR_INTERNAL);
        }

        createIdentifier(&param_entry, sym_table);

        param_node = param_node->right_ch;
    }
}

static void annotateAST(HTEntry *function_entry, AST_NODE *identifier_node)
{
    HTEntry *target_entry = calloc(1, sizeof(HTEntry));
    if (target_entry == NULL)
    {
        fprintf(stderr, "annotateAST() -- calloc() failed.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }
    size_t str_len = strlen(function_entry->identifier);
    target_entry->identifier = calloc(str_len + 1, sizeof(char));
    if (target_entry->identifier == NULL)
    {
        fprintf(stderr, "annotateAST() -- calloc() failed.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    strncpy(target_entry->identifier, function_entry->identifier, str_len);
    target_entry->entry_type = function_entry->entry_type;
    target_entry->entry_data = function_entry->entry_data;
}
