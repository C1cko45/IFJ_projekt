/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

//->  TODO: MAKE checkVALMEMBuiltins() -- will be called when encounter VALMEM/LIST expression function calls
// Those must be handled separately, because the non-parametric ones actually accept argument in different AST_NODE
// Probably check on VALMEM's function_name node that it exists in sym_table builtins
// Then have to manually check in VALMEM's term (on which the function is called) is valid type for the function
// And then have to handle returning the right data type (should just be in sym_table really)

// TODO: cleanup module for cleanup()

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
static DataType getParamType(AST_NODE *target_node);
static void pushParamsIntoScope(AST_NODE *param_node, SymTable *sym_table);
static void allocateParams(size_t param_count, DataType **param_ptr);

static DataType getTermType(AST_NODE *term_node, SymTable *sym_table);
static void annotateAST(AST_NODE *identifier_node, HTEntry *sym_table_entry, HTEntryType entry_type);

static DataType ifExpr(AST_NODE *if_node, HTEntry *function_entry, SymTable *sym_table);
static DataType arithmExpr(AST_NODE *arithm_node, SymTable *sym_table);
static DataType boolExpr(AST_NODE *bool_node, SymTable *sym_table);
static DataType relatExpr(AST_NODE *relat_node, SymTable *sym_table);

static DataType checkRangeFunc(AST_NODE *func_node, SymTable *sym_table);
static void checkGeneratorInts(AST_NODE *for_node, SymTable *sym_table);

static void insertBuiltin(size_t builtin_count, SymTable *sym_table);
static void checkMain(AST_NODE *root, SymTable *sym_table);
static void insertAllFunctions(AST_NODE *root, SymTable *sym_table);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void functionDef(AST_NODE *function_node, SymTable *sym_table);
static DataType functionCall(AST_NODE *call_node, SymTable *sym_table);
static DataType returnStatement(AST_NODE *ret_node, HTEntry *function_entry, SymTable *sym_table);

static DataType variableDef(AST_NODE *def_node, SymTable *sym_table);
static DataType assignStatement(AST_NODE *assign_node, SymTable *sym_table);

static DataType exprStatement(AST_NODE *expr_node, SymTable *sym_table);

static DataType ifStatement(AST_NODE *if_node, HTEntry *function_entry, SymTable *sym_table);
static void elseStatement(AST_NODE *else_node, HTEntry *function_entry, SymTable *sym_table);

static DataType whileStatement(AST_NODE *while_node, HTEntry *function_entry, SymTable *sym_table);
static DataType forStatement(AST_NODE *for_node, HTEntry *function_entry, SymTable *sym_table);

static DataType programStatement(AST_NODE *program_node, HTEntry *function_entry, SymTable *sym_table);
static void topLevelStatement(AST_NODE *program_node, SymTable *sym_table);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Semantic checks for AST subtree for function definition
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
    DataType *func_ret_type = &function_entry->entry_data.function.return_type;
    while (program_node != NULL)
    {
        if (program_node->right_ch == NULL) // if last statement in function body
        {
            // Evaluate return value and compare its type with function return type
            DataType retval_type = programStatement(program_node, function_entry, sym_table);

            // If function signature doesn't have explicit return type (and function has no return statements)
            // -> Function return type inferred from its last statement
            // Checks for explicit return type when function contains return statements -> see returnStatement()
            if (*func_ret_type == NIL)
                *func_ret_type = retval_type;

            // If function def has explicit return type UNIT -> returns UNIT no matter what (return values discarded)
            if (*func_ret_type != UNIT)
            {
                if (retval_type != *func_ret_type)
                {
                    fprintf(stderr, "functionDef() -- return value type doesn't match function signature\n");
                    // TODO: cleanup()
                    exit(ERROR_TYPE);
                }
            }
        }
        else
            programStatement(program_node, function_entry, sym_table);

        program_node = program_node->right_ch;
    }

    // Exit the current local scope of function body
    popScope(sym_table);
}

/*
 * Semantic checks for function call
 * Check that function was defined before use
 * Check that function argument count and argument types match function signature
 * Annotate ASTNode with info for codegen
 * Returns function's return value type
 * TODO: IMPLEMENT POSSIBLE PARAM TYPE INT -> DOUBLE CONVERSION
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

    // Get function call's argument count
    size_t arg_count = countParams(call_node->right_ch);

    // Handle case if function call is List's builtin range(from, to)
    // Must check separately because sym_table also has range(from, to, step) and, except this, IFJ26 has no overloading
    if (strcmp(call_node->left_ch->token.attr.str_val, "range") == 0 && arg_count == 2)
        return checkRangeFunc(call_node, sym_table);

    // Check argument count matches function def's parameter count
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
        arg_type = getTermType(call_node->right_ch->left_ch, sym_table);
        if (arg_type == NIL)
        {
            fprintf(stderr, "functionCall() -- function argument of indeterminate type\n");
            // TODO: cleanup()
            exit(ERROR_PARAM);
        }

        // if function parameter has type Any, it aceppts any argument type
        if (function_entry->entry_data.function.parameters[index] == ANY)
            continue;
        // if function parameter has type List[Any], it aceppts any argument type List[T]
        if (function_entry->entry_data.function.parameters[index] == LIST_ANY)
            if (arg_type == LIST_INT || arg_type == LIST_DOUBLE || arg_type == LIST_STRING ||
                arg_type == LIST_BOOLEAN || arg_type == LIST_UNIT)
                continue;

        if (arg_type != function_entry->entry_data.function.parameters[index])
        {
            fprintf(stderr, "functionCall() -- function argument of incompatible type\n");
            // TODO: cleanup()
            exit(ERROR_PARAM);
        }
    }

    // If all is correct, annotate AST node with sym_table info for codegen
    annotateAST(call_node->left_ch, function_entry, FUNCTION);

    DataType func_ret_type = function_entry->entry_data.function.return_type;
    // If function signature has explicit return type UNIT -> returns UNIT (any other return value discarded)
    if (func_ret_type == UNIT)
        return UNIT;

    return func_ret_type;
}

/*
 * Semantic checks for return statement
 *
 * Returns NIL to indicate empty return statement
 * Else returns value of expression or function call
 */
static DataType returnStatement(AST_NODE *ret_node, HTEntry *function_entry, SymTable *sym_table)
{
    DataType func_ret_type = function_entry->entry_data.function.return_type;
    // if function signature has explicit return type Unit, function returns Unit no matter what
    // Doesn't check for type compatibility in that case
    if (func_ret_type == UNIT)
        return UNIT;

    // empty return allowed only in functions that have explicit return type Unit, which in this case it doesn't
    if (ret_node->left_ch == NULL)
    {
        fprintf(stderr, "returnStatement() -- return inside function with no explicit return type UNIT\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    DataType retval_type;
    // return-statement function call
    // check type compatibility with function signature
    if (ret_node->left_ch->type == NODE_CALL)
    {
        retval_type = functionCall(ret_node->left_ch, sym_table);
        if (retval_type != func_ret_type)
        {
            fprintf(stderr, "returnStatement() -- return value type doesn't match function signature\n");
            // TODO: cleanup()
            exit(ERROR_TYPE);
        }

        return retval_type;
    }

    // return-statement with expression (or wrong nodes -- handled in expr() function)
    // check type compatibility with function signature
    retval_type = exprStatement(ret_node->left_ch, sym_table);
    if (retval_type != func_ret_type)
    {
        fprintf(stderr, "returnStatement() -- return value type doesn't match function signature\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    return retval_type;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Semantic checks for AST subtree for if-statement
 * if-condition is any expression that evaluates to Boolean
 * Branches: Else-if (optional), Else (optional)
 * every branch has its own scope
 *
 * if-statement is standalone statement in a function block, if-expression is part of an expression
 * if-statement different semantics from if-expression, see ifExpr()
 *
 * if-statement returns UNIT
 */
static DataType ifStatement(AST_NODE *if_node, HTEntry *function_entry, SymTable *sym_table)
{
    // Argument error-handling
    if (if_node == NULL || function_entry == NULL || sym_table == NULL)
    {
        fprintf(stderr, "ifStatement() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    AST_NODE *cond_node = if_node->left_ch;
    if (cond_node == NULL)
    {
        fprintf(stderr, "ifStatement() -- AST condition node is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // Evaluate the condition first
    if (exprStatement(cond_node->left_ch, sym_table) != BOOLEAN)
    {
        fprintf(stderr, "ifStatement() -- condition expression is not boolean\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // Process if-branch's block
    pushScope(sym_table);

    AST_NODE *program_node = cond_node->right_ch;
    while (program_node != NULL)
    {
        programStatement(program_node, function_entry, sym_table);
        program_node = program_node->right_ch;
    }

    popScope(sym_table);

    // Process optional else-if and else branches
    if (if_node->right_ch != NULL)
        elseStatement(if_node->right_ch, function_entry, sym_table);

    return UNIT;
}

/*
 * Semantic checks for AST subtree of if-statement's else-if, else branches
 * Helper function for ifStatement(), should never be called on its own
 */
static void elseStatement(AST_NODE *else_node, HTEntry *function_entry, SymTable *sym_table)
{
    // TBD: Either else_node->left_ch == NODE_PROGRAM (else branch)
    // or it's else_node->left_ch = NODE_IF (else-if branch)
    // Argument error-handling
    if (else_node == NULL || function_entry == NULL || sym_table == NULL)
    {
        fprintf(stderr, "elseStatement() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // else_node is else-if branch
    if (else_node->left_ch->type == NODE_IF)
    {
        // process the else-if branch block
        ifStatement(else_node->left_ch, function_entry, sym_table);
        // process another else-if/else branch exists
        if (else_node->right_ch != NULL)
            elseStatement(else_node->left_ch, function_entry, sym_table);
    }

    // else_node is else branch
    // process else branch block
    pushScope(sym_table);

    AST_NODE *program_node = else_node->left_ch;
    while (program_node != NULL)
    {
        programStatement(program_node, function_entry, sym_table);
        program_node = program_node->right_ch;
    }

    popScope(sym_table);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Semantic checks for AST subtree for while loop
 * while-condition is any expression that evaluates to Boolean
 * execution body has its own scope
 * while-statement returns Unit
 */
static DataType whileStatement(AST_NODE *while_node, HTEntry *function_entry, SymTable *sym_table)
{
    // Argument error-handling
    if (while_node == NULL || function_entry == NULL || sym_table == NULL)
    {
        fprintf(stderr, "whileStatement() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    AST_NODE *cond_expr = while_node->left_ch;
    if (cond_expr == NULL)
    {
        fprintf(stderr, "whileStatement() -- while-condition doesn't exist\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // Evaluate the condition first
    if (exprStatement(cond_expr, sym_table) != BOOLEAN)
    {
        fprintf(stderr, "whileStatement() -- condition expression is not boolean\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // Process while's block
    pushScope(sym_table);

    AST_NODE *program_node = while_node->right_ch;
    while (program_node != NULL)
    {
        programStatement(program_node, function_entry, sym_table);
        program_node = program_node->right_ch;
    }

    popScope(sym_table);

    return UNIT;
}

/*
 * Semantic checks for for-loop
 *
 * for-loop returns Unit
 */
static DataType forStatement(AST_NODE *for_node, HTEntry *function_entry, SymTable *sym_table)
{
    // Argument error-handling
    if (for_node == NULL || function_entry == NULL || sym_table == NULL)
    {
        fprintf(stderr, "forStatement() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    /*
     * First pushScope() and then push the generator's new control variable into it (as immutable)
     * (The generator's control variable on the left is a new control variable that is pushed)
     * (the right side can be an int literal/int variable/list[Int] literal/list[Int] variable)
     * Must check what type of generator (3 types):
     *      from to to
     *      from to to by step --> step must not be 0 (statically can check only if step is int interal)
     *      from until until
     * Generator can contain only int literals/variables (no implicit conversion)
     * Then run expr() checks on guards (if-expressions)
     */

    // Also if variables -> must resolve in sym_table and then annotateAST
    if (for_node->left_ch == NULL || for_node->left_ch->left_ch == NULL)
    {
        fprintf(stderr, "forStatement() -- for-loop contains no generator\n");
        // TODO: cleanup()
        exit(ERROR_OTHER);
    }

    // Check for-loop generator contains only ints
    checkGeneratorInts(for_node, sym_table);

    // Evaluate every guard (if-expression condition)
    AST_NODE *cond_node = for_node->left_ch->right_ch;
    while (cond_node != NULL)
    {
        if (exprStatement(cond_node->left_ch, sym_table) != BOOLEAN)
        {
            fprintf(stderr, "forStatement() -- condition expression is not boolean\n");
            // TODO: cleanup()
            exit(ERROR_TYPE);
        }
        cond_node = cond_node->right_ch;
    }

    pushScope(sym_table);
    // push generator's control variable into the new scope
    HTEntry ctrl_var = {.state = BUSY,
                        .entry_type = VAL,
                        .entry_data.variable_data_type = INT,
                        .identifier = for_node->left_ch->token.attr.str_val};
    createIdentifier(&ctrl_var, sym_table);

    // Process for-statement block
    AST_NODE *program_node = for_node->right_ch;
    while (program_node != NULL)
    {
        programStatement(program_node, function_entry, sym_table);
        program_node = program_node->right_ch;
    }

    popScope(sym_table);

    return UNIT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Semantic checks for variable definition
 *
 * Checks for variable re-definition in the current scope
 * Evaluates right side return value type -> if can't infer ERROR_TYPE
 * If variable definition has type annotation, checks for type compatibility between left side and right side
 * If variable definition has no type annotation, infer from right side
 *
 * Creates entry in sym_table
 */
static DataType variableDef(AST_NODE *def_node, SymTable *sym_table)
{
    // Find out if variable definition is immutable/mutable
    HTEntryType var_modifier;
    if (def_node->type == NODE_VAL_DEF)
        var_modifier = VAL;

    if (def_node->type == NODE_VAR_DEF)
        var_modifier = VAR;

    // Must use isEntry() to search the current scope only -- to allow variable shadowing in subblocks
    if (isEntry(def_node->left_ch->token.attr.str_val, VAR, &sym_table->scopes[sym_table->current]))
    {
        fprintf(stderr, "variableDef() -- variable already defined\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // Get right side expression/function call return value type
    DataType rvalue_ret_type;
    AST_NODE *rvalue_node = def_node->right_ch;
    // if right side is function call
    if (rvalue_node->type == NODE_CALL)
        rvalue_ret_type = functionCall(rvalue_node, sym_table);
    else
        rvalue_ret_type = exprStatement(rvalue_node, sym_table);

    // Evaluating right side expression/function call couldn't infer type
    // Leads to ERROR_TYPE whether var def has type annotation or not
    if (rvalue_ret_type == NIL)
    {
        fprintf(stderr, "variableDef() -- couldn't infer type from right side of assignment\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // Get var def's type annotation -> if has, check type compatibility
    DataType var_type = NIL;
    if (def_node->left_ch->left_ch != NULL)
    {
        var_type = getParamType(def_node->left_ch->left_ch);
        // If not assigning Int rside to Double lside, which leads to Int->Double implicit conversion
        if (!(var_type == DOUBLE && rvalue_ret_type == INT))
        {
            if (var_type != rvalue_ret_type) // If in such cases var type and expression type don't match
            {
                fprintf(stderr, "variableDef() -- Trying to assign value of incompatible type\n");
                // TODO: cleanup()
                exit(ERROR_TYPE);
            }
        }
    }
    else // var def has no type annotation, infer from rvalue expression/function call
        var_type = rvalue_ret_type;

    // Initialize the HTEntry to insert into sym_table
    HTEntry var_entry;
    var_entry.entry_type = var_modifier;
    var_entry.state = BUSY;
    var_entry.identifier = def_node->left_ch->token.attr.str_val;
    var_entry.entry_data.variable_data_type = var_type;

    // Insert HTEntry into sym_table
    createIdentifier(&var_entry, sym_table);

    return UNIT;
}

/*
 * Semantic checks for assignment statement
 * Left side -> check variable exists, is immutable
 * Right side -> check that it's valid expression or function call, get it's return value type
 * Check type compatibility right side vs left side
 *
 * Annotates AST_NODE of the left side variable with sym_table entry info (for codegen)
 *
 * assignment statement returns UNIT
 */
static DataType assignStatement(AST_NODE *assign_node, SymTable *sym_table)
{
    // Argument error-handling
    if (assign_node == NULL || sym_table == NULL)
    {
        fprintf(stderr, "variableDef() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // use getIdentifier() to search even outer scope for the variable
    HTEntry *var_entry = getIdentifier(assign_node->left_ch->token.attr.str_val, VAR, sym_table);
    if (var_entry == NULL)
    {
        fprintf(stderr, "variableDef() -- Trying to assign to undefined variable\n");
        // TODO: cleanup()
        exit(ERROR_UNDEF);
    }
    // Assigning to immutable variable
    if (var_entry->entry_type == VAL)
    {
        fprintf(stderr, "variableDef() -- Trying to assign to immutable variable\n");
        // TODO: cleanup()
        exit(ERROR_IMMUTABLE);
    }
    // Assigning value to variable would change its data type
    DataType rvalue_ret_type;
    AST_NODE *rvalue_node = assign_node->right_ch;
    // if right side is function call
    if (rvalue_node->type == NODE_CALL)
        rvalue_ret_type = functionCall(rvalue_node, sym_table);
    else
        rvalue_ret_type = exprStatement(rvalue_node, sym_table);

    // Evaluating right side of assignment couldn't infer type
    if (rvalue_ret_type == NIL)
    {
        fprintf(stderr, "assignStatement() -- couldn't infer type from right side of assignment\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    DataType var_type = var_entry->entry_data.variable_data_type;
    if (var_type != rvalue_ret_type)
    {
        fprintf(stderr, "assignStatement() -- Trying to assign value of incompatible type\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // If no problems, annotate AST node with left side identifier for codegen
    annotateAST(assign_node->left_ch, var_entry, VAR);
    return UNIT;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Main functions for processing expressions
 *
 * Mainly a dispatcher for all types of expressions
 * Returns the whole expression's return value type
 */
static DataType exprStatement(AST_NODE *expr_node, SymTable *sym_table)
{
    // Argument error-handling
    if (expr_node == NULL || sym_table == NULL)
    {
        fprintf(stderr, "exprStatement() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    switch (expr_node->token.type)
    {
        // Term
        case TOKEN_TYPE_INT:
        case TOKEN_TYPE_DOUBLE:
        case TOKEN_TYPE_STRING:
        case TOKEN_TYPE_UNIT:
        case TOKEN_TYPE_BOOL:
        case TOKEN_TYPE_IDENTIFIER:
            return getTermType(expr_node, sym_table);

        // Arithmetic operator
        case TOKEN_TYPE_PLUS:
        case TOKEN_TYPE_MINUS:
        case TOKEN_TYPE_MUL:
        case TOKEN_TYPE_DIV:
            return arithmExpr(expr_node, sym_table);

        // Logical operator
        case TOKEN_TYPE_AND:
        case TOKEN_TYPE_OR:
        case TOKEN_TYPE_NOT:
            return boolExpr(expr_node, sym_table);

        // Relational operator
        case TOKEN_TYPE_GT:
        case TOKEN_TYPE_LT:
        case TOKEN_TYPE_LTE:
        case TOKEN_TYPE_GTE:
        case TOKEN_TYPE_EQ:
        case TOKEN_TYPE_NEQ:
            return relatExpr(expr_node, sym_table);

            // TODO: ADD VALMEM expressions
            // TODO: ADD LISTS expressions -- Doesn't work in parser anyway, check scala.txt

        default:
            fprintf(stderr, "exprStatement() -- Expression of unsupported type\n");
            // TODO: cleanup()
            exit(ERROR_INTERNAL);
    }
}

/*
 * Processes arithmetic expressions
 * Arithmetic operators support only numeric operands and operators returning numeric types (arithmetic only)
 * (with implicit conversion Int->Double)
 * Also supports '+' operator on two strings as concatenation
 *
 * Mainly a helper function for exprStatement(), from where it's called on the operator node
 * Should not be called on its own
 */
static DataType arithmExpr(AST_NODE *arithm_node, SymTable *sym_table)
{
    // Argument error-handling
    if (arithm_node == NULL || sym_table == NULL)
    {
        fprintf(stderr, "arithmExpr() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    DataType left_operand, right_operand;
    HTEntry *id_entry = NULL;
    switch (arithm_node->token.type)
    {
        // Term
        case TOKEN_TYPE_IDENTIFIER:
            // Resolve identifier first; check for var defined
            id_entry = getIdentifier(arithm_node->token.attr.str_val, VAR, sym_table);
            if (id_entry == NULL)
            {
                fprintf(stderr, "arithmExpr() -- Variable is undefined\n");
                // TODO: cleanup()
                exit(ERROR_UNDEF);
            }
            // Arithmetic operators support only numeric types and string concatenation
            if (id_entry->entry_data.variable_data_type != INT && id_entry->entry_data.variable_data_type != DOUBLE &&
                id_entry->entry_data.variable_data_type != STRING)
            {
                fprintf(stderr, "arithmExpr() -- Arithmetic expression contains invalid operand type\n");
                // TODO: cleanup()
                exit(ERROR_TYPE);
            }
            // Else annotate the term in AST with sym_table info, then return var's data type
            annotateAST(arithm_node, id_entry, id_entry->entry_type);
            return getTermType(arithm_node, sym_table);

        case TOKEN_TYPE_INT:
            return INT;
        case TOKEN_TYPE_DOUBLE:
            return DOUBLE;
        case TOKEN_TYPE_STRING: // function called on operator node, so expr is just string literal
            return STRING;

        // Arithmetic operator
        case TOKEN_TYPE_PLUS:
        case TOKEN_TYPE_MUL:
        case TOKEN_TYPE_DIV:
        case TOKEN_TYPE_MINUS:
            if (arithm_node->right_ch == NULL) // if '-' is unary operator, else processed below
                // arithmExpr() here guaranteed to either return a supported arithm expression return type or it errors
                return arithmExpr(arithm_node->left_ch, sym_table);

            left_operand = arithmExpr(arithm_node->left_ch, sym_table);
            right_operand = arithmExpr(arithm_node->right_ch, sym_table);
            break;

            // TODO: ADD VALMEM expressions
            // TODO: ADD LISTS expressions -- Doesn't work in parser anyway, check scala.txt

        default:
            fprintf(stderr, "arithmExpr() -- Expression contains invalid operator/operand\n");
            // TODO: cleanup()
            exit(ERROR_TYPE);
    }

    // Upon leaving the nesting, check type compatibility of operands

    // In case operands are strings, but operator not concatenation
    if (left_operand == STRING && right_operand == STRING && arithm_node->token.type != TOKEN_TYPE_PLUS)
    {
        fprintf(stderr, "arithmExpr() -- Arithmetic expressions don't support non-numeric types\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }
    // Handle implicit conversion INT->DOUBLE
    if ((left_operand == DOUBLE && right_operand == INT) || (left_operand == INT && right_operand == DOUBLE))
        return DOUBLE;

    // Left operand and right operand have incompatible types
    if (left_operand != right_operand)
    {
        fprintf(stderr, "arithmExpr() -- Expression on operands of incompatible type\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // If everything alright, return the expression's return value type
    return left_operand;
}

/*
 * Processes expressions wth boolean operators
 * Boolean operators must have operands of type Boolean (terms, operators returning Boolean - boolean, relational)
 *
 * Helper function for exprStatement(), from where it's called on the operator node
 * Should not be called on its own
 */
static DataType boolExpr(AST_NODE *bool_node, SymTable *sym_table)
{
    // Argument error-handling
    if (bool_node == NULL || sym_table == NULL)
    {
        fprintf(stderr, "boolExpr() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    DataType left_operand, right_operand;
    HTEntry *id_entry = NULL;
    switch (bool_node->token.type)
    {
        // Term
        case TOKEN_TYPE_IDENTIFIER:
            // Resolve identifier first; check for var defined
            id_entry = getIdentifier(bool_node->token.attr.str_val, VAR, sym_table);
            if (id_entry == NULL)
            {
                fprintf(stderr, "boolExpr() -- Variable is undefined\n");
                // TODO: cleanup()
                exit(ERROR_UNDEF);
            }
            // Boolean operators support only boolean types
            if (id_entry->entry_data.variable_data_type != BOOLEAN)
            {
                fprintf(stderr, "boolExpr() -- Boolean expression contains invalid operand type\n");
                // TODO: cleanup()
                exit(ERROR_TYPE);
            }
            // Else annotate the variable in AST with sym_table info, then return var's data type
            annotateAST(bool_node, id_entry, id_entry->entry_type);
            return getTermType(bool_node, sym_table);

        case TOKEN_TYPE_BOOL:
            return BOOLEAN;

        case TOKEN_TYPE_LT:
        case TOKEN_TYPE_LTE:
        case TOKEN_TYPE_GT:
        case TOKEN_TYPE_GTE:
        case TOKEN_TYPE_EQ:
        case TOKEN_TYPE_NEQ:
            return relatExpr(bool_node, sym_table);

        // Boolean operator
        case TOKEN_TYPE_NOT:
            // boolExpr() guaranteed to return a boolean type or it errors; no need for type checks
            return boolExpr(bool_node->left_ch, sym_table);
        case TOKEN_TYPE_AND:
        case TOKEN_TYPE_OR:
            left_operand = boolExpr(bool_node->left_ch, sym_table);
            right_operand = boolExpr(bool_node->right_ch, sym_table);
            break;

        default:
            fprintf(stderr, "boolExpr() -- Boolean expression contains invalid operator/operand\n");
            // TODO: cleanup()
            exit(ERROR_TYPE);
    }

    // Upon leaving the nesting, check type compatibility of operands

    // Left operand and right operand have incompatible types, switch-statement guarantees that these are boolean
    if (left_operand != right_operand)
    {
        fprintf(stderr, "arithmExpr() -- Expression on operands of incompatible type\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // Everything checks out, so return Boolean -- the only type it can return
    return BOOLEAN;
}

/*
 * Processes relational expression
 * Relational operators support only numeric operands or 2 string operands (compared lexicographically)
 *
 * Helper function for exprStatement(), from where it's called on the operator node
 */
static DataType relatExpr(AST_NODE *relat_node, SymTable *sym_table)
{
    // Argument error-handling
    if (relat_node == NULL || sym_table == NULL)
    {
        fprintf(stderr, "relatExpr() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    DataType left_operand, right_operand;
    HTEntry *id_entry = NULL;
    switch (relat_node->token.type)
    {
        // Term
        case TOKEN_TYPE_IDENTIFIER:
            // Resolve identifier first; check for var defined
            id_entry = getIdentifier(relat_node->token.attr.str_val, VAR, sym_table);
            if (id_entry == NULL)
            {
                fprintf(stderr, "relatExpr() -- Variable is undefined\n");
                // TODO: cleanup()
                exit(ERROR_UNDEF);
            }
            // Relational operators support only numeric types and strings comparison
            if (id_entry->entry_data.variable_data_type != INT && id_entry->entry_data.variable_data_type != DOUBLE &&
                id_entry->entry_data.variable_data_type != STRING)
            {
                fprintf(stderr, "relatExpr() -- Relational expression contains invalid operand type\n");
                // TODO: cleanup()
                exit(ERROR_TYPE);
            }
            // Else annotate the term in AST with sym_table info, then return var's data type
            annotateAST(relat_node, id_entry, id_entry->entry_type);
            return getTermType(relat_node, sym_table);

        case TOKEN_TYPE_INT:
            return INT;
        case TOKEN_TYPE_DOUBLE:
            return DOUBLE;
        case TOKEN_TYPE_STRING: // function called on operator node, so expr is just string literal
            return STRING;

        // Relational operator
        case TOKEN_TYPE_LT:
        case TOKEN_TYPE_LTE:
        case TOKEN_TYPE_GT:
        case TOKEN_TYPE_GTE:
        case TOKEN_TYPE_EQ:
        case TOKEN_TYPE_NEQ:
            left_operand = relatExpr(relat_node->left_ch, sym_table);
            right_operand = relatExpr(relat_node->right_ch, sym_table);
            break;

        default:
            fprintf(stderr, "relatExpr() -- Relational expression contains invalid operator/operand\n");
            // TODO: cleanup()
            exit(ERROR_TYPE);
    }

    // Upon leaving the nesting, check type compatibility of operands

    // Handle implicit conversion INT->DOUBLE
    if ((left_operand == DOUBLE && right_operand == INT) || (left_operand == INT && right_operand == DOUBLE))
        return BOOLEAN;

    // Left operand and right operand have incompatible types, switch-statement guarantees that these are boolean
    if (left_operand != right_operand)
    {
        fprintf(stderr, "relatExpr() -- Expression on operands of incompatible type\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // Everything checks out, so return Boolean -- the only type it can return
    return BOOLEAN;
}

/*
 * Processes rvalue if-expression in assignment statement
 * This form of if-expression has fixed form, must contain if and else branches, does not support else-if
 */
static DataType ifExpr(AST_NODE *if_node, HTEntry *function_entry, SymTable *sym_table)
{
    // Argument error-handling
    if (if_node == NULL || function_entry == NULL || sym_table == NULL)
    {
        fprintf(stderr, "ifStatement() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    AST_NODE *cond_node = if_node->left_ch;
    if (cond_node == NULL)
    {
        fprintf(stderr, "ifStatement() -- AST condition node is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // Evaluate the condition first
    if (exprStatement(cond_node->left_ch, sym_table) != BOOLEAN)
    {
        fprintf(stderr, "ifStatement() -- condition expression is not boolean\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // Process if-branch's block
    pushScope(sym_table);

    AST_NODE *ifprogram_node = cond_node->right_ch;
    DataType if_ret_type = NIL;
    while (ifprogram_node != NULL)
    {
        if_ret_type = programStatement(ifprogram_node, function_entry, sym_table);
        ifprogram_node = ifprogram_node->right_ch;
    }

    popScope(sym_table);

    // Check that if-branch's block returns valid data type
    if (if_ret_type == NIL)
    {
        fprintf(stderr, "ifExpr() -- if-branch block does not return valid type\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // Make sure the else-branch actually exists, guarantee by syntactic analysis
    if (if_node->right_ch == NULL)
    {
        fprintf(stderr, "ifExpr() -- if-expression in assignment doesn't contain else-branch\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // Process mandatory else-branch
    pushScope(sym_table);

    AST_NODE *else_program_node = if_node->right_ch;
    DataType else_ret_type = NIL;
    while (else_program_node != NULL)
    {
        else_ret_type = programStatement(else_program_node, function_entry, sym_table);
        else_program_node = else_program_node->right_ch;
    }

    popScope(sym_table);

    // Check that else-branch's block returns valid data type
    if (else_ret_type == NIL)
    {
        fprintf(stderr, "ifExpr() -- else-branch block does not return valid type\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    // In case of one branch returning INT, the other returning DOUBLE -> implicit conversion INT->DOUBLE
    if ((if_ret_type == INT && else_ret_type == DOUBLE) || (if_ret_type == DOUBLE && else_ret_type == INT))
        return DOUBLE;

    // Otherwise, if both branches' return value types don't match
    if (if_ret_type != else_ret_type)
    {
        fprintf(stderr, "ifExpr() -- if-branch and else-branch return value types incompatible\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    return if_ret_type;
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
    if (program_node == NULL || function_entry == NULL || sym_table == NULL)
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
        //
        case NODE_VAR_DEF:
        case NODE_VAL_DEF:
            return variableDef(program_node->left_ch, sym_table); // returns UNIT

        case NODE_WHILE:
            return whileStatement(program_node->left_ch, function_entry, sym_table); // returns UNIT
        //
        case NODE_FOR:
            return forStatement(program_node->left_ch, function_entry, sym_table); // returns UNIT
        //
        case NODE_IF:
            return ifStatement(program_node->left_ch, function_entry, sym_table); // returns UNIT
        //
        case NODE_VALMEM:
        case NODE_LIST:
            // TODO: return exprStatement(program_node->left_ch, sym_table);
            break;

        case NODE_TOKEN_ONLY:
            switch (program_node->left_ch->token.type)
            {
                case TOKEN_TYPE_ASSIGN:
                    return assignStatement(program_node->left_ch, sym_table); // returns UNIT

                // Term expression
                case TOKEN_TYPE_INT:
                case TOKEN_TYPE_DOUBLE:
                case TOKEN_TYPE_STRING:
                case TOKEN_TYPE_UNIT:
                case TOKEN_TYPE_BOOL:
                case TOKEN_TYPE_IDENTIFIER:

                // Arithmetic expression
                case TOKEN_TYPE_PLUS:
                case TOKEN_TYPE_MINUS:
                case TOKEN_TYPE_MUL:
                case TOKEN_TYPE_DIV:

                // Relational expression
                case TOKEN_TYPE_LT:
                case TOKEN_TYPE_LTE:
                case TOKEN_TYPE_GT:
                case TOKEN_TYPE_GTE:

                case TOKEN_TYPE_EQ:
                case TOKEN_TYPE_NEQ:

                // Logical expression
                case TOKEN_TYPE_AND:
                case TOKEN_TYPE_OR:
                case TOKEN_TYPE_NOT:
                    return exprStatement(program_node->left_ch, sym_table);

                case KEYWORD_RETURN:
                    return returnStatement(program_node->left_ch, function_entry, sym_table);

                default:
                    break;
            }

        default:
            fprintf(stderr, "programStatement() -- Function contains statement of unsupported type\n");
            // TODO: cleanup()
            exit(ERROR_OTHER);
    }

    // TODO: Here should probably be the error statements in the switch case default
    return UNIT;
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
                break;
            case NODE_FUNC_DEF:
                functionDef(program_node->left_ch, sym_table);
                break;
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

    // Traverses top-level scope for @main tagged function, also checks proper top-level structure (only func def's)
    checkMain(root, sym_table);

    // Insert all builtin functions
    for (size_t builtin_count = 18; builtin_count > 0; --builtin_count)
        insertBuiltin(builtin_count, sym_table);

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
    while (param_node != NULL && param_node->left_ch != NULL)
    {
        ++param_count;
        param_node = param_node->right_ch;
    }

    return param_count;
}
/*
 * Returns target_node's type annotation (located at target_node->left_ch)
 *
 * Function assumes correct argument passed (pass exactly the AST node you want analyzed) -> no argument check
 * Please check the argument before calling this function thoroughly
 * Mainly for getting function def's return type, variable definition's type annotation
 */
static DataType getParamType(AST_NODE *type_node)
{
    // passed argument node might be type literal or variable
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
        default:
            // TODO:
            fprintf(stderr, "getParamType() -- returning NIL?\n");
            return NIL;
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
        param_entry.entry_data.variable_data_type = getParamType(param_node->left_ch);
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

/*
 * Helper function for functionCall() to handle special case List's builtin range(start, end)
 * It's an exception of function overloading (range(start, end, step)), otherwise it's not supposed
 *
 * Function assumes func_node really is range(step, end)
 * Returns range(start, end)'s signature return type List[Int]
 */
static DataType checkRangeFunc(AST_NODE *call_node, SymTable *sym_table)
{
    // Argument error-handling
    if (call_node == NULL || sym_table == NULL)
    {
        fprintf(stderr, "checkRangeFunc() -- at least one argument is invalid\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // Get the range(start, end, step) function - They're basically the same except the step parameter
    HTEntry *function_entry = getIdentifier(call_node->token.attr.str_val, FUNCTION, sym_table);

    // Check argument types -- can only be literals and identifiers (Should be guaranteed by syntactic analysis)
    size_t arg_count = 2;
    DataType arg_type;
    for (size_t index = 0; arg_count > 0; ++index, --arg_count)
    {
        arg_type = getTermType(call_node->right_ch, sym_table);
        // Check argument is defined variable/literal of determinate and supported type
        if (arg_type == NIL)
        {
            fprintf(stderr, "checkRangeFunc() -- function argument of indeterminate type\n");
            // TODO: cleanup()
            exit(ERROR_PARAM);
        }

        // if function parameter has type Any, it aceppts any argument type
        if (function_entry->entry_data.function.parameters[index] == ANY)
            continue;
        // if function parameter has type List[Any], it aceppts any argument type List[T]
        if (function_entry->entry_data.function.parameters[index] == LIST_ANY)
            if (arg_type == LIST_INT || arg_type == LIST_DOUBLE || arg_type == LIST_STRING ||
                arg_type == LIST_BOOLEAN || arg_type == LIST_UNIT)
                continue;

        if (arg_type != function_entry->entry_data.function.parameters[index])
        {
            fprintf(stderr, "checkRangeFunc() -- function argument of incompatible type\n");
            // TODO: cleanup()
            exit(ERROR_PARAM);
        }
    }

    // if no problems, return range(start, end)'s signature return type List[Int]
    return LIST_INT;
}

/*
 * Allocates memory for HTEntry in AST_NODE and passes symtable_entry info to it
 */
static void annotateAST(AST_NODE *identifier_node, HTEntry *sym_table_entry, HTEntryType entry_type)
{
    // Argument error-handling
    if (sym_table_entry == NULL || identifier_node == NULL || entry_type == NONE)
    {
        fprintf(stderr, "annotateAST() -- at least one argument is invalid.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // Allocate new HTEntry
    HTEntry *ast_entry = calloc(1, sizeof(HTEntry));
    if (ast_entry == NULL)
    {
        fprintf(stderr, "annotateAST() -- calloc() failed.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }
    size_t str_len = strlen(sym_table_entry->identifier);
    ast_entry->identifier = calloc(str_len + 1, sizeof(char));
    if (ast_entry->identifier == NULL)
    {
        fprintf(stderr, "annotateAST() -- calloc() failed.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }
    strncpy(ast_entry->identifier, sym_table_entry->identifier, str_len);
    ast_entry->entry_type = sym_table_entry->entry_type;

    HTEntryData *ast_data = &ast_entry->entry_data;
    HTEntryData *sym_table_data = &sym_table_entry->entry_data;
    if (entry_type == VAR || entry_type == VAL)
        ast_data->variable_data_type = sym_table_data->variable_data_type;
    else // entry_type == FUNCTION
    {
        // Initialize the AST HTEntry data
        ast_data->function.return_type = sym_table_data->function.return_type;
        ast_data->function.param_count = sym_table_data->function.param_count;
        // Must handle dynamically allocated parameters[param_count] array
        ast_data->function.parameters = calloc(ast_data->function.param_count, sizeof(DataType));
        if (ast_data->function.parameters == NULL)
        {
            fprintf(stderr, "annotateAST() -- calloc() failed.\n");
            // TODO: cleanup()
            exit(ERROR_INTERNAL);
        }

        // Copy over the parameters (data types)
        for (size_t index = 0; index < ast_data->function.param_count; ++index)
            ast_data->function.parameters[index] = sym_table_data->function.parameters[index];
    }

    // Annotate AST node
    identifier_node->identifier_entry = ast_entry;
}

/*
 * Pushes builtin functions into SymTable's global scope, based on builtin_count argument
 * Should be called only after creating a new sym_table
 */
static void insertBuiltin(size_t builtin_count, SymTable *sym_table)
{
    // Argument error-handling
    if (sym_table == NULL)
    {
        fprintf(stderr, "insertBuiltin() -- argument is NULL\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    HTEntry builtin_function = {
        .state = BUSY,
        .entry_type = FUNCTION,
    };

    switch (builtin_count)
    {
        /* Ordered from 0-parameter, to most */
        // IFJ26 BASE
        // 0-parameter functions
        case 18:
            // readLine(): String
            builtin_function.identifier = "readLine";
            builtin_function.entry_data.function.param_count = 0;
            builtin_function.entry_data.function.return_type = STRING;
            builtin_function.entry_data.function.parameters = NULL;

            createIdentifier(&builtin_function, sym_table);
            break;
        case 17:
            // readInt(): Int
            builtin_function.identifier = "readInt";
            builtin_function.entry_data.function.param_count = 0;
            builtin_function.entry_data.function.return_type = INT;
            builtin_function.entry_data.function.parameters = NULL;

            createIdentifier(&builtin_function, sym_table);
            break;
        case 16:
            // readDouble(): Double
            builtin_function.identifier = "readDouble";
            builtin_function.entry_data.function.param_count = 0;
            builtin_function.entry_data.function.return_type = DOUBLE;
            builtin_function.entry_data.function.parameters = NULL;

            createIdentifier(&builtin_function, sym_table);
            break;

            createIdentifier(&builtin_function, sym_table);
            break;
        // 1-parameter functions
        case 15:
            // print(value: Any): Unit
            builtin_function.identifier = "print";
            builtin_function.entry_data.function.param_count = 1;
            builtin_function.entry_data.function.return_type = UNIT;
            allocateParams(1, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = ANY;

            createIdentifier(&builtin_function, sym_table);
            break;
        case 14:
            // toStr(value: Any): String -- LZE I ARGUMENT value: List[T]
            builtin_function.identifier = "toStr";
            builtin_function.entry_data.function.param_count = 1;
            builtin_function.entry_data.function.return_type = STRING;
            allocateParams(1, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = ANY;

            createIdentifier(&builtin_function, sym_table);
            break;
        case 13:
            // toInt(value: Double): Int
            builtin_function.identifier = "toInt";
            builtin_function.entry_data.function.param_count = 1;
            builtin_function.entry_data.function.return_type = INT;
            allocateParams(1, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = DOUBLE;

            createIdentifier(&builtin_function, sym_table);
            break;
        case 12:
            // strLength(value: String): Int
            builtin_function.identifier = "strLength";
            builtin_function.entry_data.function.param_count = 1;
            builtin_function.entry_data.function.return_type = INT;
            allocateParams(1, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = STRING;

            createIdentifier(&builtin_function, sym_table);
            break;
        case 11:
            // chr(code: Int): String
            builtin_function.identifier = "chr";
            builtin_function.entry_data.function.param_count = 1;
            builtin_function.entry_data.function.return_type = STRING;
            allocateParams(1, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = INT;

            createIdentifier(&builtin_function, sym_table);
            break;
        // 2-parameter functions
        case 10:
            // ord(value: String, index: Int): Int
            builtin_function.identifier = "ord";
            builtin_function.entry_data.function.param_count = 2;
            builtin_function.entry_data.function.return_type = INT;
            allocateParams(2, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = STRING;
            builtin_function.entry_data.function.parameters[1] = INT;

            createIdentifier(&builtin_function, sym_table);
            break;
        // 3-parameter functions
        case 9:
            // subStr(value: String, start: Int, end: Int): String
            builtin_function.identifier = "subStr";
            builtin_function.entry_data.function.param_count = 3;
            builtin_function.entry_data.function.return_type = STRING;
            allocateParams(3, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = STRING;
            builtin_function.entry_data.function.parameters[1] = INT;
            builtin_function.entry_data.function.parameters[2] = INT;

            createIdentifier(&builtin_function, sym_table);
            break;
        // BOOLTHEN extension
        case 8:
            // readBoolean(): Boolean
            builtin_function.identifier = "readBoolean";
            builtin_function.entry_data.function.param_count = 0;
            builtin_function.entry_data.function.return_type = BOOLEAN;
            builtin_function.entry_data.function.parameters = NULL;

            createIdentifier(&builtin_function, sym_table);
            break;
        // LISTS extension
        // 0-parameter functions
        case 7:
            // head(): T ~> ANY
            builtin_function.identifier = "head";
            builtin_function.entry_data.function.param_count = 0;
            builtin_function.entry_data.function.return_type = ANY;
            builtin_function.entry_data.function.parameters = NULL;

            createIdentifier(&builtin_function, sym_table);
            break;
        case 6:
            // tail(): List[T] ~> LIST_ANY
            builtin_function.identifier = "tail";
            builtin_function.entry_data.function.param_count = 0;
            builtin_function.entry_data.function.return_type = LIST_ANY;
            builtin_function.entry_data.function.parameters = NULL;

            createIdentifier(&builtin_function, sym_table);
            break;
        case 5:
            // toString(): String
            builtin_function.identifier = "toString";
            builtin_function.entry_data.function.param_count = 0;
            builtin_function.entry_data.function.return_type = STRING;
            builtin_function.entry_data.function.parameters = NULL;

            createIdentifier(&builtin_function, sym_table);
            break;
        // 1-parameter functions
        case 4:
            // drop(n): List[T]
            builtin_function.identifier = "drop";
            builtin_function.entry_data.function.param_count = 1;
            builtin_function.entry_data.function.return_type = LIST_ANY;
            allocateParams(1, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = DOUBLE; // Posibble also INT -> DOUBLE

            createIdentifier(&builtin_function, sym_table);
            break;
        case 3:
            // take(n): List[T]
            builtin_function.identifier = "take";
            builtin_function.entry_data.function.param_count = 1;
            builtin_function.entry_data.function.return_type = LIST_ANY;
            allocateParams(1, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = DOUBLE; // Posibble also INT -> DOUBLE

            createIdentifier(&builtin_function, sym_table);
            break;
        // 2-parameter functions
        case 2:
            // slice(from, until): List[T]
            builtin_function.identifier = "slice";
            builtin_function.entry_data.function.param_count = 2;
            builtin_function.entry_data.function.return_type = LIST_ANY;
            allocateParams(2, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = DOUBLE; // Posibble also INT -> DOUBLE
            builtin_function.entry_data.function.parameters[1] = DOUBLE; // Posibble also INT -> DOUBLE

            createIdentifier(&builtin_function, sym_table);
            break;
        // 3-parameter functions
        case 1:
            // range(start, end, step): List[Int]

            // Dubious one, language spec supports builtin range(start, end) AND builtin range(start,end,step)
            // -> Which normally leads to re-definition error in SymTable
            // Language spec itself explicitly doesn't support function overloading, like at all
            // So I just decided to insert range(start,end,step)
            // functionCall() resolves range(start, end) with checkRangeFunc() helper function
            // knowing that range().paramCount == 3 in SymTable, but, in reality, can be either 2 or 3
            builtin_function.identifier = "range";
            builtin_function.entry_data.function.param_count = 3;
            builtin_function.entry_data.function.return_type = LIST_INT;
            allocateParams(3, &builtin_function.entry_data.function.parameters);
            builtin_function.entry_data.function.parameters[0] = DOUBLE; // Posibble also INT -> DOUBLE
            builtin_function.entry_data.function.parameters[1] = DOUBLE; // Posibble also INT -> DOUBLE
            builtin_function.entry_data.function.parameters[2] = DOUBLE; // Posibble also INT -> DOUBLE

            createIdentifier(&builtin_function, sym_table);
            break;
        default:
            fprintf(stderr, "insertBuiltin() -- argument builtin_count has wrong value\n");
            // TODO: cleanup()
            exit(ERROR_INTERNAL);
    }
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
        // Check that top-level scope contains only statements
        if (root->type != NODE_PROGRAM)
        {
            fprintf(stderr, "checkMain() -- top-level scope contains a non-statement\n");
            // TODO: cleanup()
            exit(ERROR_OTHER);
        }
        // Check that top-level scope contains only function definitions
        if (root->left_ch->type != NODE_MAIN && root->left_ch->type != NODE_FUNC_DEF)
        {
            fprintf(stderr, "checkMain() -- top-level scope contains an unsupported statement\n");
            // TODO: cleanup()
            exit(ERROR_OTHER);
        }

        if (root->left_ch->type == NODE_MAIN)
        {
            if (main_found) // if already found @main-tagged function before
            {
                fprintf(stderr, "checkMain() -- Program contains more than one @main-tagged functions\n");
                // TODO: cleanup()
                exit(ERROR_REDEF);
            }

            main_found = true;

            // Check that @main return type is Unit
            if (root->left_ch->left_ch->left_ch->left_ch->token.type != KEYWORD_UNIT)
            {
                fprintf(stderr, "checkMain() -- @main-tagged function has wrong return type\n");
                // TODO: cleanup()
                exit(ERROR_TYPE);
            }

            // Check that @main contains no parameters
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
 * Helper function for insertBuiltin()
 * Allocates memory for param_ptr[param_count]
 *
 * Returns error param_ptr is not NULL
 */
static void allocateParams(size_t param_count, DataType **param_ptr)
{
    // Argument error-handling
    if (param_ptr == NULL)
    {
        fprintf(stderr, "allocateParams() -- param_ptr is not NULL\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }
    if (param_count == 0)
    {
        fprintf(stderr, "allocateParams() -- argument param_count is 0\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    *param_ptr = calloc(param_count, sizeof(DataType));
    if (*param_ptr == NULL)
    {
        fprintf(stderr, "allocateParams() -- calloc() failed\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }
}

/*
 * Fill sym_table's global scope with all function names in top-level scope
 *
 * Should be called right before semantic analysis (to support function calls before their respective definition)
 * Call only after checkMain() function, which guarantees correct AST structure of top-level scope
 * and correct @main function
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
    AST_NODE *function_node = NULL;
    while (root != NULL)
    {
        if (root->left_ch->type == NODE_MAIN)
            function_node = root->left_ch->left_ch;
        else
            function_node = root->left_ch;

        function_entry.identifier = function_node->left_ch->token.attr.str_val;
        if (isIdentifier(function_entry.identifier, FUNCTION, sym_table))
        {
            fprintf(stderr, "insertAllFunctions() -- Function was previously defined\n");
            // TODO: cleanup()
            exit(ERROR_REDEF);
        }

        function_entry.entry_type = FUNCTION;
        function_entry.entry_data.function.return_type = getParamType(function_node->left_ch->left_ch);
        function_entry.entry_data.function.param_count = countParams(function_node->left_ch->right_ch);

        size_t param_count = function_entry.entry_data.function.param_count;
        if (param_count != 0)
        {
            // allocate memory for parameters[countParams]
            allocateParams(param_count, &function_entry.entry_data.function.parameters);

            // while-loop every parameter until NULL, getDataType
            AST_NODE *param_node = function_node->left_ch->right_ch;
            for (size_t index = 0; param_count > 0; ++index, --param_count)
            {
                function_entry.entry_data.function.parameters[index] = getParamType(param_node->left_ch);
                if (function_entry.entry_data.function.parameters[index] == NIL)
                {
                    fprintf(stderr, "insertAllFunctions() -- function parameter doesn't have type annotation\n");
                    // TODO: cleanup()
                    exit(2); // ERROR_SYNTACTIC
                }
                param_node = param_node->right_ch;
            }
        }
        else
            function_entry.entry_data.function.parameters = NULL;

        createIdentifier(&function_entry, sym_table);

        root = root->right_ch;
    }
}

/*
 * Checks that for-loop generator contains only ints
 *
 * Helper function for forStatement()
 */
static void checkGeneratorInts(AST_NODE *for_node, SymTable *sym_table)
{
    // TODO: Maybe remake to check just one and then call 3 times from forStatement() to save LoC
    // TODO: Probably checks for List[Int] literals and variables won't work right now
    // Check generator for type compatibility -- only int literals/int variables/list[Int] literals/list[Int] variables

    AST_NODE *from_node = for_node->left_ch->left_ch->left_ch->left_ch;
    AST_NODE *to_node = for_node->left_ch->left_ch->left_ch->right_ch;

    if (from_node->token.type == TOKEN_TYPE_IDENTIFIER) // If term is a variable
    {
        DataType term_type = getTermType(from_node, sym_table);
        if (term_type != INT)
        {
            fprintf(stderr, "checkGeneratorInts() -- for-loop generator contains non-integer value\n");
            // TODO: cleanup()
            exit(ERROR_TYPE);
        }
    }
    else if (from_node->token.type != TOKEN_TYPE_INT) // term is not an int literal nor variable
    {
        fprintf(stderr, "checkGeneratorInts() -- for-loop generator contains non-integer value\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    if (to_node->token.type == TOKEN_TYPE_IDENTIFIER) // If term is a variable
    {
        DataType term_type = getTermType(to_node, sym_table);
        if (term_type != INT)
        {
            fprintf(stderr, "checkGeneratorInts() -- for-loop generator contains non-integer value\n");
            // TODO: cleanup()
            exit(ERROR_TYPE);
        }
    }
    else if (to_node->token.type != TOKEN_TYPE_INT) // term is not an int literal nor variable
    {
        fprintf(stderr, "checkGeneratorInts() -- for-loop generator contains non-integer value\n");
        // TODO: cleanup()
        exit(ERROR_TYPE);
    }

    AST_NODE *step_node = for_node->left_ch->left_ch->right_ch;
    // if generator step exists
    if (step_node != NULL)
    {
        if (step_node->token.type == TOKEN_TYPE_IDENTIFIER) // If term is a variable
        {
            DataType term_type = getTermType(step_node, sym_table);
            if (term_type != INT)
            {
                fprintf(stderr, "checkGeneratorInts() -- for-loop contains non-integer literal/variable\n");
                // TODO: cleanup()
                exit(ERROR_TYPE);
            }
        }
        else if (step_node->token.type != TOKEN_TYPE_INT) // term is not an int literal nor variable
        {
            fprintf(stderr, "checkGeneratorInts() -- for-loop contains non-integer literal/variable\n");
            // TODO: cleanup()
            exit(ERROR_TYPE);
        }
    }
}

/*
 * Return data type of term (literal/variable)
 *
 * Mainly for resolving terms in expressions (their types)
 */
static DataType getTermType(AST_NODE *term_node, SymTable *sym_table)
{
    if (sym_table == NULL || term_node == NULL)
    {
        fprintf(stderr, "getTermType() -- at least one argument is invalid.\n");
        // TODO: cleanup()
        exit(ERROR_INTERNAL);
    }

    // Term is variable
    if (term_node->token.type == TOKEN_TYPE_IDENTIFIER)
    {
        HTEntry *var_entry = getIdentifier(term_node->token.attr.str_val, VAR, sym_table);
        if (var_entry == NULL)
        {
            fprintf(stderr, "getTermType() -- undefined variable\n");
            // TODO: cleanup()
            exit(ERROR_UNDEF);
        }

        return var_entry->entry_data.variable_data_type;
    }

    // Term not variable, might be literal
    switch (term_node->token.type)
    {
        case TOKEN_TYPE_STRING:
            return STRING;
        case TOKEN_TYPE_DOUBLE:
            return DOUBLE;
        case TOKEN_TYPE_INT:
            return INT;
        case TOKEN_TYPE_BOOL:
            return BOOLEAN;
        case TOKEN_TYPE_UNIT:
            return UNIT;
        case KEYWORD_LIST:
            // TODO: NEeds to be remade, probably not KEYWORD....
            switch (term_node->left_ch->token.type)
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
        default:
            // TODO:
            fprintf(stderr, "getTermType() -- returning NIL?\n");
            return NIL;
    }
}
