#include "ast.h"
#include "codegen.h"
#include "parser.h"
#include "semantic_analysis.h"

// TODO: dopisat ostatne suboru *.h

int main()
{
    // TODO: load file pre lexer a generator.
    FILE *output = fopen("output.txt", "w");

    AST_NODE *strom = program();

    // TODO: spustim semanticku analyzu.
    semanticAnalysis(strom);

    int x = codegen_generate(strom, output); // vrati 99 alebo 0
    ast_free_node(strom);
    fclose(output);
    exit(x);
}
