#include "psa.h"
#include <stddef.h>
#define STACK_MAX 256
// AI-generated (Claude) - prompt: "vytvor mi 2d pole s mojou hotovou precedencnou tabulkou"
// Prepis rucne vyplnenej tabulky autora (hodnoty su jeho, AI ich len previedla do kodu).

//                      i  +  -  *  /  (  )  $ +U -U REL EQ !  && ||
static const PrecRel prec_table[P_COUNT][P_COUNT] = {
    /* i   */ { N, G, G, G, G, N, G, G, N, N, G, G, N, G, G },
    /* +   */ { L, G, G, L, L, L, G, G, L, L, G, G, L, G, G },
    /* -   */ { L, G, G, L, L, L, G, G, L, L, G, G, L, G, G },
    /* *   */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G },
    /* /   */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G },
    /* (   */ { L, L, L, L, L, L, Q, N, L, L, L, L, L, L, L },
    /* )   */ { N, G, G, G, G, N, G, G, N, N, G, G, N, G, G },
    /* $   */ { L, L, L, L, L, L, N, N, L, L, L, L, L, L, L },
    /* +U  */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G },
    /* -U  */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G },
    /* REL */ { L, L, L, L, L, L, G, G, L, L, N, G, L, G, G },
    /* EQ  */ { L, L, L, L, L, L, G, G, L, L, L, N, L, G, G },
    /* !   */ { L, G, G, G, G, L, G, G, L, L, G, G, L, G, G },
    /* &&  */ { L, L, L, L, L, L, G, G, L, L, L, L, L, G, G },
    /* ||  */ { L, L, L, L, L, L, G, G, L, L, L, L, L, L, G },
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

            if(prev == P_I || prev == P_RPAREN){parser_error_exit(2, "Chyba vyraz");}

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
            if(prev == P_I || prev == P_RPAREN){parser_error_exit(2, "Chyba vyraz");}
            item = (STACK_ITEM){.idx = P_LPAREN, .token = token};
            
            
        break;

        case TOKEN_TYPE_RPAREN:
            if(prev == P_DOLLAR){parser_error_exit(2, "Chyba vyraz");}
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
            if(prev == P_LPAREN || prev == P_DOLLAR){parser_error_exit(2, "Chyba vyraz");}
            item = (STACK_ITEM){.idx = P_DOLLAR, .token = token};
            
        break;

        case TOKEN_TYPE_LT:
        case TOKEN_TYPE_LTE:
        case TOKEN_TYPE_GT:
        case TOKEN_TYPE_GTE:
            if(prev == P_REL){parser_error_exit(2, "Chyba vyraz");}
            item = (STACK_ITEM){.idx = P_REL, .token = token};
            
        break;

        case TOKEN_TYPE_EQ:
        case TOKEN_TYPE_NEQ:
            if(prev == P_EQ){parser_error_exit(2, "Chyba vyraz");}
            item = (STACK_ITEM){.idx = P_EQ, .token = token};
            
        break;

        case TOKEN_TYPE_NOT:
            if(prev == P_I || prev == P_RPAREN){parser_error_exit(2, "Chyba vyraz");}
            item = (STACK_ITEM){.idx = P_NOT, .token = token};
            
        break;

        case TOKEN_TYPE_AND:
            item = (STACK_ITEM){.idx = P_AND, .token = token};
            
        break;

        case TOKEN_TYPE_OR:
            item = (STACK_ITEM){.idx = P_OR, .token = token};
            
        break;
        default:
            parser_error_exit(2, "Chyba vyraz");
        break;
    }
    return item;
}

void insert(STACK_ITEM item){
    if(stack_count == STACK_MAX){parser_error_exit(2, "Pretecenie stack kapacity");}
    stack[stack_count++] = item;
    return;
}
STACK_ITEM stack_pop(void){
    if(stack_count == 0){
        parser_error_exit(99, "Pop z prazdneho zasobnika.");
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
    if(index < 0){parser_error_exit(99, "Prazdny zasobnik v reduce");}
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
            //pravidlo E->E znamienko E
            if(index < 1 || index + 1 != stack_count - 1 ||
                stack[index-1].idx != P_EXPR || stack[index+1].idx != P_EXPR){
                parser_error_exit(2, "Chyba v pravidle E->E op E");
            }
            temp = ast_create_node(stack[index].token);
            ast_add_parent(temp, stack[index-1].node, 0);
            ast_add_parent(temp, stack[index+1].node, 1);
            stack_pop();
            stack_pop();
            stack[stack_count-1] = (STACK_ITEM){.idx = P_EXPR, .node = temp};
        break;

        case P_RPAREN:
            //pravidlo E->(E)
            if(index < 2 || stack[index-1].idx != P_EXPR || stack[index-2].idx != P_LPAREN){
                parser_error_exit(2, "Chyba v pravidle E->(E)");
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
                parser_error_exit(2, "Chyba v unarnom pravidle");
            }
            temp = ast_create_node(stack[index].token);
            ast_add_parent(temp, stack[index+1].node, 0);
            stack_pop();
            stack[index] = (STACK_ITEM){.idx = P_EXPR, .node = temp};
        break;

        default:
            parser_error_exit(2, "Chyba v reduce");
        break;
    }
}
AST_NODE* parse_expression(){
    stack_count = 0;
    insert((STACK_ITEM){.idx = P_DOLLAR});
    PrecIndex prev = P_DOLLAR;

    while(1){
        Token tk = peek();
        // volanie funkcie: na zasobniku je len $ a identifikator, nasleduje '('
        if(prev == P_I && stack_count == 2 && tk.type == TOKEN_TYPE_LPAREN
        && stack[1].token.type == TOKEN_TYPE_IDENTIFIER){
            Token name = stack[1].token;
            next();                       // spotrebuje '('
            return call_f(name, tk);
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
                    parser_error_exit(2, "Chyba expression");
                    break;
                
            }
    }
    if(stack_count != 2 || stack[1].idx != P_EXPR){
        parser_error_exit(2, "Chyba vyraz");
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
        // volanie funkcie: na zasobniku je len $ a identifikator, nasleduje '('
        if(prev == P_I && stack_count == 2 && tk.type == TOKEN_TYPE_LPAREN
        && stack[1].token.type == TOKEN_TYPE_IDENTIFIER){
            Token name = stack[1].token;
            next();                       // spotrebuje '('
            return call_f(name, tk);
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
                    parser_error_exit(2, "Chyba expression");
                    break;
                
            }
    }
    if(stack_count != 2 || stack[1].idx != P_EXPR){
        parser_error_exit(2, "Chyba vyraz");
    }
    return stack[1].node;
}
