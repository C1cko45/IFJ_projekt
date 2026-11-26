#include "psa.h"
#include <stddef.h>
#include <stdio.h>

#include <string.h>
#include <stdbool.h>
#define STACK_MAX 256
// AI-generated (Claude) - prompt: "vytvor mi 2d pole s mojou hotovou precedencnou tabulkou"
// Prepis rucne vyplnenej tabulky autora (hodnoty su jeho, AI ich len previedla do kodu).

//              i  +  -  *  /  (  )  $ +U -U REL EQ !  && || ++  .
static const PrecRel prec_table[P_COUNT][P_COUNT] = {
    /* i   */ { N, G, G, G, G, N, G, G, N, N, G, G, N, G, G, G, G },
    /* +   */ { L, G, G, L, L, L, G, G, L, L, G, G, L, G, G, G, L },
    /* -   */ { L, G, G, L, L, L, G, G, L, L, G, G, L, G, G, G, L },
    /* *   */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G, G, L },
    /* /   */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G, G, L },
    /* (   */ { L, L, L, L, L, L, Q, N, L, L, L, L, L, L, L, L, L },
    /* )   */ { N, G, G, G, G, N, G, G, N, N, G, G, N, G, G, G, G },
    /* $   */ { L, L, L, L, L, L, N, N, L, L, L, L, L, L, L, L, L },
    /* +U  */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G, G, L },
    /* -U  */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G, G, L },
    /* REL */ { L, L, L, L, L, L, G, G, L, L, N, G, L, G, G, L, L },
    /* EQ  */ { L, L, L, L, L, L, G, G, L, L, L, N, L, G, G, L, L },
    /* !   */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G, G, L },
    /* &&  */ { L, L, L, L, L, L, G, G, L, L, L, L, L, G, G, L, L },
    /* ||  */ { L, L, L, L, L, L, G, G, L, L, L, L, L, L, G, L, L },
    /* ++  */ { L, G, G, L, L, L, G, G, L, L, G, G, L, G, G, G, L },
    /* .   */ { L, G, G, G, G, N, G, G, N, N, G, G, N, G, G, G, G },
};
//Koniec generovaneho kodu.


static STACK_ITEM stack[STACK_MAX];   // zasobnik precedencnej analyzy (terminaly aj nonterminaly E)
static int stack_count = 0;           // pocet poloziek na zasobniku
static int paren_count = 0;           // pocet otvorenych '(' v aktualnom vyraze - ')' navyse ukoncuje vyraz (argumenty)

// get_next_item: prevedie token na symbol precedencnej tabulky (P_I, P_PLUS, ..., P_DOLLAR = koniec vyrazu)
//   prev = predchadzajuci symbol; podla neho rozlisi unarne/binarne +/- a odhali dva operandy za sebou
STACK_ITEM get_next_item(Token token, PrecIndex prev){
    STACK_ITEM item = {0};
    switch (token.type){
        case TOKEN_TYPE_INT:
        case TOKEN_TYPE_DOUBLE:
        case TOKEN_TYPE_STRING:
        case TOKEN_TYPE_IDENTIFIER:
        case TOKEN_TYPE_BOOL:
        case TOKEN_TYPE_UNIT:
        case KEYWORD_LIST:          

            if(prev == P_I || prev == P_RPAREN){parser_error_exit(2, "Syntakticka chyba vo vyraze: dva operandy za sebou bez operatora");}

            item = (STACK_ITEM){.idx = P_I, .token = token};

        break;

        case TOKEN_TYPE_PLUS:
            if(prev == P_I || prev == P_RPAREN ){
                item = (STACK_ITEM){.idx = P_PLUS, .token = token};
            }else{
                item = (STACK_ITEM){.idx = P_UPLUS, .token = token};
            }
            
        break;

        case TOKEN_TYPE_MINUS:
            if(prev == P_I || prev == P_RPAREN ){
                item = (STACK_ITEM){.idx = P_MINUS, .token = token};
            }else{
                item = (STACK_ITEM){.idx = P_UMINUS, .token = token};
            }
            
        break;

        case TOKEN_TYPE_MUL:
            item = (STACK_ITEM){.idx = P_MUL, .token = token};
            
        break;

        case TOKEN_TYPE_DIV:
            item = (STACK_ITEM){.idx = P_DIV, .token = token};
            
        break;

        case TOKEN_TYPE_LPAREN:
            if(prev == P_I || prev == P_RPAREN){parser_error_exit(2, "Syntakticka chyba vo vyraze: '(' priamo za operandom");}
            item = (STACK_ITEM){.idx = P_LPAREN, .token = token};
            
            
        break;

        case TOKEN_TYPE_RPAREN:
            if(prev == P_DOLLAR){parser_error_exit(2, "Syntakticka chyba vo vyraze: ')' bez vyrazu");}
                if(paren_count > 0){
                    item = (STACK_ITEM){.idx = P_RPAREN, .token = token};
                }else{
                    item = (STACK_ITEM){.idx = P_DOLLAR, .token = token};
            }
            
        break;

        case TOKEN_TYPE_EOL:
        case TOKEN_TYPE_SEMICOLON:
        case KEYWORD_THEN: 
        case KEYWORD_DO:
        case TOKEN_TYPE_COMMA:
        case KEYWORD_TO:
        case KEYWORD_UNTIL:
        case KEYWORD_BY:
        case KEYWORD_IF:
        case KEYWORD_ELSE:
            if(prev == P_LPAREN || prev == P_DOLLAR){parser_error_exit(2, "Syntakticka chyba vo vyraze: vyraz chyba alebo je neukonceny");}
            item = (STACK_ITEM){.idx = P_DOLLAR, .token = token};
            
        break;

        case TOKEN_TYPE_LT:
        case TOKEN_TYPE_LTE:
        case TOKEN_TYPE_GT:
        case TOKEN_TYPE_GTE:
            if(prev == P_REL){parser_error_exit(2, "Syntakticka chyba vo vyraze: dva relacne operatory za sebou");}
            item = (STACK_ITEM){.idx = P_REL, .token = token};
            
        break;

        case TOKEN_TYPE_EQ:
        case TOKEN_TYPE_NEQ:
            if(prev == P_EQ){parser_error_exit(2, "Syntakticka chyba vo vyraze: dva operatory == / != za sebou");}
            item = (STACK_ITEM){.idx = P_EQ, .token = token};
            
        break;

        case TOKEN_TYPE_NOT:
            if(prev == P_I || prev == P_RPAREN){parser_error_exit(2, "Syntakticka chyba vo vyraze: '!' priamo za operandom");}
            item = (STACK_ITEM){.idx = P_NOT, .token = token};
            
        break;

        case TOKEN_TYPE_CONCAT:
            item = (STACK_ITEM){.idx = P_CONCAT, .token = token};

        break;

        case TOKEN_TYPE_DOT:
            if(prev != P_I && prev != P_RPAREN){parser_error_exit(2, "Syntakticka chyba vo vyraze: '.' musi nasledovat za hodnotou");}
            item = (STACK_ITEM){.idx = P_DOT, .token = token};

        break;

        case TOKEN_TYPE_AND:
            item = (STACK_ITEM){.idx = P_AND, .token = token};
            
        break;

        case TOKEN_TYPE_OR:
            item = (STACK_ITEM){.idx = P_OR, .token = token};
            
        break;
        default:
            parser_error_exit(2, "Syntakticka chyba vo vyraze: neocakavany token");
        break;
    }
    return item;
}

// insert: vlozi polozku na vrchol zasobnika PSA
void insert(STACK_ITEM item){
    if(stack_count == STACK_MAX){parser_error_exit(2, "Syntakticka chyba: vyraz je prilis zlozity (pretecenie zasobnika PSA)");}
    stack[stack_count++] = item;
    return;
}
// stack_pop: odstrani a vrati polozku z vrcholu zasobnika PSA
STACK_ITEM stack_pop(void){
    if(stack_count == 0){
        parser_error_exit(99, "Interna chyba PSA: pop z prazdneho zasobnika");
    }
    stack_count--;
    return stack[stack_count];
}
// stack_top_position: index najvyssieho terminalu na zasobniku (nonterminal E sa preskakuje), -1 ak ziadny nie je
static int stack_top_position(void){
    for (int i = stack_count-1; i>=0; i-- ){
        if(stack[i].idx < P_COUNT ){
            return i;
        }
    }
    return -1;
}
// reduce: aplikuje pravidlo podla najvyssieho terminalu a vytvori uzol AST:
//   E -> i | E op E | E.id (NODE_VALMEM) | (E) | () (literal Unit) | unarny_op E
void reduce(void){
    int index = stack_top_position();
    if(index < 0){parser_error_exit(99, "Interna chyba PSA: redukcia na prazdnom zasobniku");}
    AST_NODE* temp = NULL;

    switch (stack[index].idx){
        case P_I:
            //pravidlo E->i
            temp = ast_create_node(stack[index].token);
            stack[index] = (STACK_ITEM){.idx = P_EXPR, .node = temp};
        break;

        case P_PLUS:
        case P_MINUS:
        case P_MUL:
        case P_DIV:
        case P_REL:
        case P_EQ:
        case P_AND:
        case P_OR:
        case P_CONCAT:
        case P_DOT:
            //pravidlo E->E znamienko E  (pre P_DOT je to E.id)
            if(index < 1 || index + 1 != stack_count - 1 ||
                stack[index-1].idx != P_EXPR || stack[index+1].idx != P_EXPR){
                parser_error_exit(2, "Syntakticka chyba vo vyraze: binarnemu operatoru chyba operand");
            }

            /*
            if(stack[index].idx == P_DOT){
                temp = ast_create_node_type(NODE_VALMEM);   // clen hodnoty: lavy = prijemca, pravy = nazov clena
            }else{
                temp = ast_create_node(stack[index].token);
            }
            */
            if(stack[index].idx == P_DOT){
                AST_NODE *member = stack[index+1].node;

    // Za bodkou musi byt nazov clena
                if(member == NULL || member->type != NODE_TOKEN_ONLY || member->token.type != TOKEN_TYPE_IDENTIFIER || member->token.attr.str_val == NULL){
                    parser_error_exit(2, "Syntakticka chyba: za '.' sa ocakava nazov clena");
                }

                const char *name = member->token.attr.str_val;

    // Cleny bez zatvoriek
                bool no_args = strcmp(name, "head") == 0 ||
                            strcmp(name, "tail") == 0 ||
                            strcmp(name, "toString") == 0 ||
                            strcmp(name, "toInt") == 0 ||
                            strcmp(name, "floor") == 0 ||
                            strcmp(name, "length") == 0 ||
                            strcmp(name, "toList") == 0;

    // Metody vyzadujuce zatvorky
                bool needs_args = strcmp(name, "drop") == 0 ||
                                strcmp(name, "take") == 0 ||
                                strcmp(name, "slice") == 0 ||
                                strcmp(name, "range") == 0;

                bool has_parens = peek().type == TOKEN_TYPE_LPAREN;

                if(no_args && has_parens){
                    parser_error_exit(2, "Syntakticka chyba: bezparametricky clen nesmie mat zatvorky");
                }

                if(needs_args && !has_parens){
                    parser_error_exit(2, "Syntakticka chyba: metoda vyzaduje zatvorky");
                }

                temp = ast_create_node_type(NODE_VALMEM);
            }else{
                temp = ast_create_node(stack[index].token);
            }

            ast_add_parent(temp, stack[index-1].node, 0);
            ast_add_parent(temp, stack[index+1].node, 1);
            stack_pop();
            stack_pop();
            stack[stack_count-1] = (STACK_ITEM){.idx = P_EXPR, .node = temp};
        break;

        case P_RPAREN:
            //pravidlo E->() : scanner posiela "()" ako '(' a ')', vytvori sa literal typu Unit
            if(index >= 1 && stack[index-1].idx == P_LPAREN){
                Token unit = {.type = TOKEN_TYPE_UNIT};
                stack_pop();
                stack[stack_count-1] = (STACK_ITEM){.idx = P_EXPR, .node = ast_create_node(unit)};
                break;
            }
            //pravidlo E->(E)
            if(index < 2 || stack[index-1].idx != P_EXPR || stack[index-2].idx != P_LPAREN){
                parser_error_exit(2, "Syntakticka chyba vo vyraze: nespravne uzatvorkovanie");
            }
            stack[index-2] = stack[index-1];
            stack_pop();
            stack_pop();
        break;

        case P_NOT:
        case P_UPLUS:
        case P_UMINUS:
            //pravidlo E->unarny op E
            if(index + 1 != stack_count - 1 || stack[index+1].idx != P_EXPR){
                parser_error_exit(2, "Syntakticka chyba vo vyraze: unarnemu operatoru chyba operand");
            }
            temp = ast_create_node(stack[index].token);
            ast_add_parent(temp, stack[index+1].node, 0);
            stack_pop();
            stack[index] = (STACK_ITEM){.idx = P_EXPR, .node = temp};
        break;

        default:
            parser_error_exit(2, "Syntakticka chyba vo vyraze: neplatna redukcia");
        break;
    }
}


// parse_expression: precedencna syntakticka analyza vyrazu od aktualneho tokenu po prvy ukoncovaci token
//   (EOL, ';', then, do, else, if, ',', ')' bez parovej '(' ...); ukoncovaci token nespotrebuje, vrati koren AST vyrazu
AST_NODE* parse_expression(void){
    if(peek().type == KEYWORD_IF){      // if-vyraz (CONTROL), napr. val y = if x < 0 then 0 - x else x
        next();
        return if_statement();
    }
    stack_count = 0;
    paren_count = 0;
    insert((STACK_ITEM){.idx = P_DOLLAR});
    PrecIndex prev = P_DOLLAR;

    while(1){
        Token tk = peek();
        // '(' hned za operandom = volanie / index zoznamu / metoda - aj vo vnutri vyrazu (f(x) + 1, l.drop(1))
        // to/until vo vnutri zatvorky = rozsah (od to do), napr. (1 to 5).toList; mimo zatvoriek konci vyraz (cyklus for)
        bool is_call = tk.type == TOKEN_TYPE_LPAREN && (prev == P_I || prev == P_RPAREN);
        bool is_range = (tk.type == KEYWORD_TO || tk.type == KEYWORD_UNTIL) && paren_count > 0;
        if(is_call || is_range){
            // lava strana: volany objekt (i, E.id, (E)), resp. dolna hranica rozsahu az po '('
            while(is_call ? (stack[stack_top_position()].idx == P_I || stack[stack_top_position()].idx == P_DOT ||
                             stack[stack_top_position()].idx == P_RPAREN)
                          : (prec_table[stack[stack_top_position()].idx][P_RPAREN] == G)){
                reduce();
            }
            if(stack[stack_count-1].idx != P_EXPR || (is_range && stack[stack_count-2].idx != P_LPAREN)){
                parser_error_exit(2, "Syntakticka chyba vo vyraze: chybne volanie alebo rozsah (od to do)");
            }
            AST_NODE* left = stack[stack_count-1].node;
            Token op = next();                      // spotrebuje '(' alebo to/until

            // argumenty / horna hranica sa parsuju rekurzivne cez parse_expression -> zasobnik odlozit a obnovit
            STACK_ITEM backup[STACK_MAX];
            int backup_count = stack_count;
            int backup_paren = paren_count;
            for(int i = 0; i < backup_count; i++){ backup[i] = stack[i]; }

            AST_NODE* result;
            if(is_call){
                // call_f spracuje argumenty; namiesto nazvu funkcie sa ako volany objekt pouzije lava strana
                Token name = {.type = TOKEN_TYPE_IDENTIFIER};
                result = call_f(name, op);
                ast_free_node(result->left_ch);
                result->left_ch = left;
            }else{
                result = ast_create_node(op);       // uzol to/until (lavy = od, pravy = do) ako v cykle for
                ast_add_parent(result, left, 0);
                ast_add_parent(result, parse_expression(), 1);   // skonci na ')' (vo vnorenom volani koniec vyrazu)
            }

            for(int i = 0; i < backup_count; i++){ stack[i] = backup[i]; }
            stack_count = backup_count;
            paren_count = backup_paren;

            stack[stack_count-1] = (STACK_ITEM){.idx = P_EXPR, .node = result};
            prev = is_call ? P_RPAREN : P_I;        // vysledok sa sprava ako uzavrety operand
            continue;
        }
        STACK_ITEM item = get_next_item(peek(),prev);

        PrecIndex first = stack[stack_top_position()].idx;
        PrecIndex second = item.idx;

        if(first == P_DOLLAR && second == P_DOLLAR ){
            break;
        }

        switch(prec_table[first][second]){

                case L:
                case Q:
                    insert(item);
                    if(second == P_LPAREN){ paren_count++; }
                    if(second == P_RPAREN){ paren_count--; }
                    next();
                    prev = second;
                    break;

                case G:
                    reduce();
                    break;

                case N:
                default:
                    parser_error_exit(2, "Syntakticka chyba vo vyraze: nepovolena postupnost (napr. a < b < c alebo chybajuca ')')");
                    break;

            }
    }
    if(stack_count != 2 || stack[1].idx != P_EXPR){
        parser_error_exit(2, "Syntakticka chyba vo vyraze: vyraz sa nepodarilo zredukovat (chybajuci operand alebo ')')");
    }
    return stack[1].node;
}

// parse_expression_from: rovnako ako parse_expression, ale prvy token vyrazu uz spotreboval parser
//   (statement() musel nahliadnut za identifikator, aby rozlisil priradenie od vyrazu)
AST_NODE* parse_expression_from(Token token){
    stack_count = 0;
    paren_count = 0;
    insert((STACK_ITEM){.idx = P_DOLLAR});
    PrecIndex prev = P_DOLLAR;
    STACK_ITEM item = get_next_item(token, prev);
    insert(item);
    prev = item.idx;
    while(1){
        Token tk = peek();
        // '(' hned za operandom = volanie / index zoznamu / metoda - aj vo vnutri vyrazu (f(x) + 1, l.drop(1))
        // to/until vo vnutri zatvorky = rozsah (od to do), napr. (1 to 5).toList; mimo zatvoriek konci vyraz (cyklus for)
        bool is_call = tk.type == TOKEN_TYPE_LPAREN && (prev == P_I || prev == P_RPAREN);
        bool is_range = (tk.type == KEYWORD_TO || tk.type == KEYWORD_UNTIL) && paren_count > 0;
        if(is_call || is_range){
            // lava strana: volany objekt (i, E.id, (E)), resp. dolna hranica rozsahu az po '('
            while(is_call ? (stack[stack_top_position()].idx == P_I || stack[stack_top_position()].idx == P_DOT ||
                             stack[stack_top_position()].idx == P_RPAREN)
                          : (prec_table[stack[stack_top_position()].idx][P_RPAREN] == G)){
                reduce();
            }
            if(stack[stack_count-1].idx != P_EXPR || (is_range && stack[stack_count-2].idx != P_LPAREN)){
                parser_error_exit(2, "Syntakticka chyba vo vyraze: chybne volanie alebo rozsah (od to do)");
            }
            AST_NODE* left = stack[stack_count-1].node;
            Token op = next();                      // spotrebuje '(' alebo to/until

            // argumenty / horna hranica sa parsuju rekurzivne cez parse_expression -> zasobnik odlozit a obnovit
            STACK_ITEM backup[STACK_MAX];
            int backup_count = stack_count;
            int backup_paren = paren_count;
            for(int i = 0; i < backup_count; i++){ backup[i] = stack[i]; }

            AST_NODE* result;
            if(is_call){
                // call_f spracuje argumenty; namiesto nazvu funkcie sa ako volany objekt pouzije lava strana
                Token name = {.type = TOKEN_TYPE_IDENTIFIER};
                result = call_f(name, op);
                ast_free_node(result->left_ch);
                result->left_ch = left;
            }else{
                result = ast_create_node(op);       // uzol to/until (lavy = od, pravy = do) ako v cykle for
                ast_add_parent(result, left, 0);
                ast_add_parent(result, parse_expression(), 1);   // skonci na ')' (vo vnorenom volani koniec vyrazu)
            }

            for(int i = 0; i < backup_count; i++){ stack[i] = backup[i]; }
            stack_count = backup_count;
            paren_count = backup_paren;

            stack[stack_count-1] = (STACK_ITEM){.idx = P_EXPR, .node = result};
            prev = is_call ? P_RPAREN : P_I;        // vysledok sa sprava ako uzavrety operand
            continue;
        }
        STACK_ITEM item = get_next_item(peek(),prev);

        PrecIndex first = stack[stack_top_position()].idx;
        PrecIndex second = item.idx;

        if(first == P_DOLLAR && second == P_DOLLAR ){
            break;
        }

        switch(prec_table[first][second]){

                case L:
                case Q:
                    insert(item);
                    if(second == P_LPAREN){ paren_count++; }
                    if(second == P_RPAREN){ paren_count--; }
                    next();
                    prev = second;
                    break;

                case G:
                    reduce();
                    break;

                case N:
                default:
                    parser_error_exit(2, "Syntakticka chyba vo vyraze: nepovolena postupnost (napr. a < b < c alebo chybajuca ')')");
                    break;

            }
    }
    if(stack_count != 2 || stack[1].idx != P_EXPR){
        parser_error_exit(2, "Syntakticka chyba vo vyraze: vyraz sa nepodarilo zredukovat (chybajuci operand alebo ')')");
    }
    return stack[1].node;
}
