#include "psa.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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
//Koniec geenerovaneho kodu.


static STACK_ITEM stack[STACK_MAX];
static int stack_count = 0;
static int paren_count = 0;
STACK_ITEM get_next_item(Token token, PrecIndex prev){
    STACK_ITEM item = {0};
    switch (token.type){
        case TOKEN_TYPE_INT:
        case TOKEN_TYPE_DOUBLE:
        case TOKEN_TYPE_STRING:
        case TOKEN_TYPE_IDENTIFIER:
        case TOKEN_TYPE_BOOL:
        case TOKEN_TYPE_UNIT:
        case KEYWORD_LIST:          // [AI] List(...) ako operand (LISTS)

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

void insert(STACK_ITEM item){
    if(stack_count == STACK_MAX){parser_error_exit(2, "Syntakticka chyba: vyraz je prilis zlozity (pretecenie zasobnika PSA)");}
    stack[stack_count++] = item;
    return;
}
STACK_ITEM stack_pop(void){
    if(stack_count == 0){
        parser_error_exit(99, "Interna chyba PSA: pop z prazdneho zasobnika");
    }
    stack_count--;
    return stack[stack_count];
}
static int stack_top_position(void){
    for (int i = stack_count-1; i>=0; i-- ){
        if(stack[i].idx < P_COUNT ){
            return i;
        }
    }
    return -1;
}
void reduce(){
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
            if(stack[index].idx == P_DOT){
                temp = ast_create_node_type(NODE_VALMEM);   // clen hodnoty: lavy = prijemca, pravy = nazov clena
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
            // [AI] pravidlo E->() : scanner posiela "()" ako '(' a ')', vytvori sa literal typu Unit
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

// [AI] psa_save / psa_restore: odlozenie zasobnika PSA pred rekurzivnym volanim parse_expression
//   (argumenty volania a horna hranica rozsahu sa parsuju vnorene a parse_expression zasobnik vynuluje)
static STACK_ITEM* psa_save(int *saved_count, int *saved_paren){
    STACK_ITEM *backup = malloc(sizeof(STACK_ITEM) * (stack_count > 0 ? stack_count : 1));
    if(backup == NULL){ parser_error_exit(99, "Interna chyba PSA: chyba alokacie pamate"); }
    for(int i = 0; i < stack_count; i++){ backup[i] = stack[i]; }
    *saved_count = stack_count;
    *saved_paren = paren_count;
    return backup;
}
static void psa_restore(STACK_ITEM *backup, int saved_count, int saved_paren){
    for(int i = 0; i < saved_count; i++){ stack[i] = backup[i]; }
    stack_count = saved_count;
    paren_count = saved_paren;
    free(backup);
}

// [AI] apply_call: '(' hned za operandom = volanie funkcie, index zoznamu alebo volanie metody (aj vo vnutri vyrazu)
//   najprv sa zredukuje operand (i, E.id, (E)) -> volany objekt, potom sa spracuju argumenty -> NODE_CALL ako E
static void apply_call(void){
    int top = stack_top_position();
    while(top >= 0 && (stack[top].idx == P_I || stack[top].idx == P_DOT || stack[top].idx == P_RPAREN)){
        reduce();
        top = stack_top_position();
    }
    if(stack_count < 2 || stack[stack_count-1].idx != P_EXPR){
        parser_error_exit(2, "Syntakticka chyba vo vyraze: '(' priamo za operandom");
    }
    AST_NODE *callee = stack[stack_count-1].node;
    next();                                   // spotrebuje '('

    int saved_count, saved_paren;
    STACK_ITEM *backup = psa_save(&saved_count, &saved_paren);
    AST_NODE *call = call_with_callee(callee);
    psa_restore(backup, saved_count, saved_paren);

    stack[stack_count-1] = (STACK_ITEM){.idx = P_EXPR, .node = call};
}

// [AI] apply_range: rozsah "(od to do)" / "(od until do)" v zatvorke, napr. (1 to 5).toList
//   vytvori uzol to/until (lavy = od, pravy = do) - rovnaky tvar ako rozsah v cykle for
static void apply_range(void){
    // zredukuje vyraz "od" az po otvaraciu zatvorku
    while(prec_table[stack[stack_top_position()].idx][P_RPAREN] == G){
        reduce();
    }
    if(stack_count < 3 || stack[stack_count-1].idx != P_EXPR || stack[stack_count-2].idx != P_LPAREN){
        parser_error_exit(2, "Syntakticka chyba vo vyraze: rozsah 'to'/'until' musi byt v tvare (od to do)");
    }
    AST_NODE *from = stack[stack_count-1].node;
    AST_NODE *range = ast_create_node(next());    // spotrebuje to/until

    int saved_count, saved_paren;
    STACK_ITEM *backup = psa_save(&saved_count, &saved_paren);
    AST_NODE *to = parse_expression();            // skonci na ')' (vo vnorenom volani je to koniec vyrazu)
    psa_restore(backup, saved_count, saved_paren);

    ast_add_parent(range, from, 0);
    ast_add_parent(range, to, 1);
    stack[stack_count-1] = (STACK_ITEM){.idx = P_EXPR, .node = range};
}

AST_NODE* parse_expression(){
    stack_count = 0;
    paren_count = 0;
    insert((STACK_ITEM){.idx = P_DOLLAR});
    PrecIndex prev = P_DOLLAR;

    while(1){
        Token tk = peek();
        // [AI] '(' hned za operandom = volanie / index zoznamu / metoda - aj vo vnutri vyrazu (f(x) + 1, l.drop(1))
        if(tk.type == TOKEN_TYPE_LPAREN && (prev == P_I || prev == P_RPAREN)){
            apply_call();
            prev = P_RPAREN;              // vysledok volania sa sprava ako uzavrety operand
            continue;
        }
        // [AI] to/until vo vnutri zatvorky = rozsah (od to do); mimo zatvoriek ostava koncom vyrazu (cyklus for)
        if((tk.type == KEYWORD_TO || tk.type == KEYWORD_UNTIL) && paren_count > 0){
            apply_range();
            prev = P_I;
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

//funkcia ak sa uz token ziedol pri desifrovani ktora funkcia to je ale mi ho este potrebujeme

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
        // [AI] '(' hned za operandom = volanie / index zoznamu / metoda - aj vo vnutri vyrazu (f(x) + 1, l.drop(1))
        if(tk.type == TOKEN_TYPE_LPAREN && (prev == P_I || prev == P_RPAREN)){
            apply_call();
            prev = P_RPAREN;              // vysledok volania sa sprava ako uzavrety operand
            continue;
        }
        // [AI] to/until vo vnutri zatvorky = rozsah (od to do); mimo zatvoriek ostava koncom vyrazu (cyklus for)
        if((tk.type == KEYWORD_TO || tk.type == KEYWORD_UNTIL) && paren_count > 0){
            apply_range();
            prev = P_I;
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