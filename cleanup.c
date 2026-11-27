/*
 * Implementácia prekladača pre jazyk IFJ26.
 *
 * Ukončenie prekladu s chybovým kódom a uvoľnenie AST a tabuľky symbolov.
 *
 * Autor:   Pham Vu Duy Anh
 * Login:   xphamv00
 */

#include "cleanup.h"
#include "ast.h"
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Static pointers to memory that needs cleanup upon abnormal termination
// Initialized with registerAST()/registerSymTable() during semantic analysis setup
static AST_NODE *cleanup_ast_root = NULL;
static SymTable *cleanup_sym_table = NULL;

static void cleanup(void);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Stores AST root pointer here to cleanup_ast_root, to have the root tracked
 * For the purpose of cleaning up memory during abormal program termination
 */
void registerAST(AST_NODE *root_ast)
{
    cleanup_ast_root = root_ast;
}

/*
 * Stores AST sym_table here to cleanup_sym_table, to have the root tracked
 * For the purpose of cleaning up memory during abormal program termination
 */
void registerSymTable(SymTable *sym_table)
{
    cleanup_sym_table = sym_table;
}

/*
 * General-purpose exit()-style function to abnormally terminate the program when encountering IFJ26 error
 * First calls cleanup() that frees any allocated memory during semantic analysis
 * Then exits prgoram with return value error_code
 */
void compilerError(int error_code)
{
    cleanup();
    exit(error_code);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * Cleanup function that de-allocates all memory that is allocated during semantic analysis
 * and needs to be free'd when abnormally terminated
 */
static void cleanup(void)
{
    if (cleanup_sym_table != NULL)
    {
        destroySymTable(cleanup_sym_table);
        cleanup_sym_table = NULL;
    }

    if (cleanup_ast_root != NULL)
    {
        ast_free_node(cleanup_ast_root);
        cleanup_ast_root = NULL;
    }
}
