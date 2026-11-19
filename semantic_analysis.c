/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#include <endian.h>
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
static DataType evaluateExpr(AST_NODE *expr_node, SymTable *sym_table);
static void annotateAST(HTEntry *function_entry, AST_NODE *identifier_node);

static void insertBuiltins(SymTable *sym_table);
static void checkMain(AST_NODE *root, SymTable *sym_table);
static void insertAllFunctions(AST_NODE *root, SymTable *sym_table);

static void functionDef(AST_NODE *function_node, SymTable *sym_table);
static DataType functionCall(AST_NODE *call_node, SymTable *sym_table);

static DataType whileStatement(AST_NODE *while_node, HTEntry *function_entry, SymTable *sym_table);
static void forStatement(AST_NODE *for_node, SymTable *sym_table);

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
            AST_NODE *function_node = root->left_ch->left_ch;
            function_entry.identifier = function_node->left_ch->token.attr.str_val;
            function_entry.entry_type = FUNCTION;
            function_entry.entry_data.function.return_type = resolveDataType(function_node->left_ch);
            if (function_entry.entry_data.function.return_type != UNIT)
            {
                fprintf(stderr, "insertAllFunctions() -- @main function has wrong return type\n");
                // TODO: cleanup()
                exit(ERROR_TYPE);
            }
            function_entry.entry_data.function.param_count = countParams(function_node->left_ch->right_ch);
            if (function_entry.entry_data.function.param_count != 0)
            {
                fprintf(stderr, "insertAllFunctions() -- @main function contains parameters\n");
                // TODO: cleanup()
                exit(ERROR_PARAM);
            }
            function_entry.entry_data.function.parameters = NULL;

            if (isIdentifier(function_entry.identifier, FUNCTION, sym_table))
            {
                fprintf(stderr, "insertAllFunctions() -- @main function was already defined\n");
                // TODO: cleanup()
                exit(ERROR_REDEF);
            }
            createIdentifier(&function_entry, sym_table);
        }

        if (root->left_ch->type == NODE_FUNC_DEF)
        {
            AST_NODE *function_node = root->left_ch;
            function_entry.identifier = function_node->left_ch->token.attr.str_val;
            function_entry.entry_type = FUNCTION;
            function_entry.entry_data.function.return_type = resolveDataType(function_node->left_ch);
            function_entry.entry_data.function.param_count = countParams(function_node->left_ch->right_ch);

            size_t param_count = function_entry.entry_data.function.param_count;
            if (param_count != 0)
            {
                // allocate array parameters[countParams]
                // while-loop right childs until NULL, resolveDataType?
                function_entry.entry_data.function.parameters = calloc(param_count, sizeof(DataType));
                if (function_entry.entry_data.function.parameters == NULL)
                {
                    fprintf(stderr, "insertAllFunctions() -- calloc() failed\n");
                    // TODO: cleanup()
                    exit(ERROR_INTERNAL);
                }

                AST_NODE *param_node = function_node->left_ch->right_ch;
                for (size_t index = 0; param_count > 0; ++index, --param_count)
                {
                    function_entry.entry_data.function.parameters[index] = resolveDataType(param_node);
                    param_node = param_node->right_ch;
                }
            }
            else
                function_entry.entry_data.function.parameters = NULL;

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
        fprintf(stderr, "functionDef() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // Create new local scope for function body
    pushScope(sym_table);

    // update sym_table's function_entry to include info about each parameter's type
    HTEntry *function_entry = getIdentifier(function_node->left_ch->token.attr.str_val, FUNCTION, sym_table);

    // Push every parameter into its function scope as immutable variable
    pushParamsIntoScope(function_node->left_ch->right_ch, sym_table);

    /*
     * Process function body
     * check that last statement in function body has correct return value
     * If Expr -> Evaluate its return value and compare against function return type
     * if VAR def | assign | for-loop | while-loop | -> return value is '()'
     * if Function call -> Evaluate its return value and compare against function return type
     * if if-statement
     * -- if if-statement is expression -> treat as case Expr
     * -- if if-statement not expression ->
     *          allowed as last stat when function return type explicitly Unit
     *          or all branches end with return -> return '()'
     *          or function return type not specified and all branches return '()' -> return '()'
     * Other return statements elsewhere in function body evaluated as they are processed during AST traversal
     *
     * If function body contains return statement, function signature must have return type -> otherwise ERROR 6
     * Else return type annotation not necessary, inferred from last statement/expression in function body
     */
    AST_NODE *program_node = function_node->right_ch;
    while (program_node != NULL)
    {
        if (program_node->right_ch == NULL) // if last statement in function body
        {
            // Evaluate return value and compare its type with function return type
            DataType retval_type = programStatement(program_node, function_entry, sym_table);
            DataType *func_ret_type = &function_entry->entry_data.function.return_type;

            // If function signature doesn't have explicit return type (and function has no return statements)
            // -> Function return type inferred from its last statement
            // Checks for explicit return type when function contains return statements -> see returnStatement()
            if (*func_ret_type == NIL)
                *func_ret_type = retval_type;

            if (retval_type != *func_ret_type)
            {
                fprintf(stderr, "functionDef() -- return value type doesn't match function signature\n");
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
    // Argument error-handling
    if (sym_table == NULL || call_node == NULL)
    {
        fprintf(stderr, "functionCall() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // Check for call to undefined function
    HTEntry *function_entry = getIdentifier(call_node->left_ch->token.attr.str_val, FUNCTION, sym_table);
    if (function_entry == NULL)
    {
        fprintf(stderr, "functionCall() -- call to undefined function\n");
        // TODO: cleanup()
        exit(ERROR_UNDEF);
    }

    // Check argument count matches function def's parameter count
    size_t arg_count = countParams(call_node->right_ch);
    if (arg_count != function_entry->entry_data.function.param_count)
    {
        fprintf(stderr, "functionCall() -- argument count doesn't match function signature\n");
        // TODO: cleanup()
        exit(ERROR_PARAM);
    }

    // Check argument types -- can only be literals and identifiers (Should be guaranteed by syntactic analysis)
    DataType arg_type;
    for (size_t index = 0; arg_count > 0; ++index, --arg_count)
    {
        arg_type = resolveDataType(call_node->right_ch);
        if (arg_type == NIL)
        {
            fprintf(stderr, "functionCall() -- contains argument of indeterminate type\n");
            // TODO: cleanup()
            exit(ERROR_PARAM);
        }

        if (arg_type != function_entry->entry_data.function.parameters[index])
        {
            fprintf(stderr, "functionCall() -- contains argument of incompatible type\n");
            // TODO: cleanup()
            exit(ERROR_PARAM);
        }
    }

    // If all is correct, annotate AST node with sym_table info for codegen
    annotateAST(function_entry, call_node->left_ch);

    return function_entry->entry_data.function.return_type;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static DataType ifStatement(AST_NODE *if_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition (which can be any expression that evalutes to Boolean) first
    // Probably run this in a while loop, get number of branches by traversing right childs until NULL
    // If you encounter another ifStatement, invoke programStatement(), preserve scope hierarchy
    pushScope(sym_table);
    popScope(sym_table);
    return UNIT; // TODO:placeholder value
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * while-condition is any expression that evaluates to Boolean (return value)
 * execution body has its own scope
 * while-statement's return value is Unit
 */
static DataType whileStatement(AST_NODE *while_node, HTEntry *function_entry, SymTable *sym_table)
{
    // Evaluate the condition first
    if (evaluateExpr(while_node->left_ch, sym_table) != BOOLEAN)
    {
        fprintf(stderr, "whileStatement() -- condition expression is not boolean\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    pushScope(sym_table);

    // Process while's body
    AST_NODE *program_node = while_node->right_ch;
    while (program_node != NULL)
    {
        programStatement(program_node, function_entry, sym_table);
        program_node = program_node->right_ch;
    }

    popScope(sym_table);
    return UNIT;
}

static void forStatement(AST_NODE *for_node, SymTable *sym_table)
{
    // TODO: Must evaluate the condition, among other things in this complicated mess of a construct (LMAO)
    pushScope(sym_table);
    popScope(sym_table);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void variableDef(AST_NODE *def_node, SymTable *sym_table)
{
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
            return functionCall(program_node->left_ch, sym_table); // returns function's return type (per IFJ26 rules)
        case NODE_VAR_DEF:
        case NODE_VAL_DEF:
            // TODO: variableDef(program_node->left_ch, sym_table);
            // return Unit;
        case NODE_WHILE:
            return whileStatement(program_node->left_ch, function_entry, sym_table); // returns UNIT
        case NODE_FOR:
            // TODO: forStatement(program_node->left_ch, sym_table);
            // return Unit;
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

    // AST node might have type annotation
    AST_NODE *type_node = target_node->left_ch;
    if (type_node != NULL)
    {
        switch (type_node->token.type)
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
                switch (type_node->left_ch->token.type)
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
                    default:;
                }
            default:;
        }
    }

    // passed argument node might be type literal
    type_node = target_node;
    if (type_node == NULL)
    {
        switch (type_node->token.type)
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
                switch (type_node->left_ch->token.type)
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
                    default:;
                }
            default:;
        }
    }

    fprintf(stderr, "resolveDataType() -- AST node either doesn't have type annotation or isn't a type literal.\n");
    // TODO: cleanup()
    exit(ERROR_INTERNAL);
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

static DataType evaluateExpr(AST_NODE *expr_node, SymTable *sym_table)
{
}
