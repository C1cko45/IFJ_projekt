/*
 * IFJ26
 * Author: Pham Vu Duy Anh, xphamv00
 */

#include "semantic_analysis.h"
#include "symtable.h"

#define SEM_ERROR_UNDEF 3 // use of undef function/var/type; missing @main-tagged function
#define SEM_ERROR_REDEF 4 // illegal re-def function/var/type
#define SEM_ERROR_PARAM 5 // incorrect count/type of arguments in function call; illegal @main-tagged function params
#define SEM_ERROR_TYPE 6  // type incompat | incorrect type infer in expr, assign, init, return type, control flow
#define SEM_ERROR_IMMUTABLE 7 // assign attempt to immutable variable/function parameter
#define SEM_ERROR_OTHER 10    // other semantic errors; (illegal return, non-exhaustive match, range with 0-step, ...)
#define ERROR_INTERNAL 99     // internal program error; failed memory allocation, ...

/*
 * First traverse AST and check for @main function ->
 *
 */
