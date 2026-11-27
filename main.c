/*
 * Implementácia prekladača pre jazyk IFJ26.
 *
 * Hlavný program: spustí lexikálnu, syntaktickú a sémantickú analýzu a generovanie kódu, vráti návratový kód prekladu.
 *
 * Autor:   Deák Dominik
 * Login:   xdeakdo00
 */
#include "ast.h"
#include "codegen.h"
#include "parser.h"
#include "semantic_analysis.h"

int main(void)
{
    // volanie lex. a syntak. analyzátorov.
    AST_NODE *strom = program();
    // volanie sémantickej analýzy.
    semanticAnalysis(strom);
    // volanie generátoru.
    int x = codegen_generate(strom, stdout); // vrati 99 alebo 0
    ast_free_node(strom);
    exit(x);
}
