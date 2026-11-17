/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#include <stdio.h>

#include "ast.h"
#include "semantic_analysis.h"
#include "symtable.h"

#define ERROR_SYNTACTIC 2

#define ERROR_UNDEF 3     // use of undef function/var/type; missing @main-tagged function
#define ERROR_REDEF 4     // illegal re-def function/var/type
#define ERROR_PARAM 5     // incorrect count/type of arguments in function call; illegal @main-tagged function params
#define ERROR_TYPE 6      // type incompat | incorrect type infer in expr, assign, init, return type, control flow
#define ERROR_IMMUTABLE 7 // assign attempt to immutable variable/function parameter
#define ERROR_OTHER 10    // other semantic errors; (illegal return, non-exhaustive match, range with 0-step, ...)
#define ERROR_INTERNAL 99 // internal program error; failed memory allocation, ...

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Initialize symTable first with createSymTable();
 *
 * First traverse AST and check for @main function
 * Might as well fill the global scope in SymTable with function identifiers
 *
 */

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void semanticAnalysis(AST_NODE *root_ast)
{
    // Create and initialize sym_table with global scope
    SymTable *sym_table = createSymTable();
    insertBuiltins(sym_table);

    // Semantic analysis
    AST_NODE *parent_node = root_ast;
    // Traverse AST PreOrder
    while (parent_node != NULL)
    {
        // Process parent_node ~ Check correct AST_NODE_TYPE
        // Should never happen, correct AST structure of top-level guaranteed by syntactic analysis
        if (parent_node != NODE_PROGRAM && parent_node->left_ch->type != NODE_FUNC_DEF)
        {
            fprintf(stderr, "ERROR_SYNTACTIC\n");
            // cleanup();
            exit(ERROR_SYNTACTIC);
        }

        // Process parent_node->left_child
        // Should always be function definition statement
        // nodeDispatcher(parent_node->left_ch);

        // Process parent_node->right_child
        parent_node = parent_node->right_ch;
    }

    // Free any allocated memory in sym_table
    destroySymTable(sym_table);
}
