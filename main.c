#include "ast.h"
#include "codegen.h"
#include "parser.h"
#include "semantic_analysis.h"

// TODO: dopisat ostatne suboru *.h

int main(void)
{
    AST_NODE *strom = program();

    semanticAnalysis(strom);

    int x = codegen_generate(strom, stdout); // vrati 99 alebo 0
    ast_free_node(strom);
    exit(x);
}
