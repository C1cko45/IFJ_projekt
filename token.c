/*
 * Implementácia prekladača pre jazyk IFJ26.
 *
 * Pomocné funkcie pre tokeny.
 *
 * Autor:   Deák Dominik
 * Login:   xdeakdo00
 */
#include "token.h"
#include <stdlib.h>

void token_free(Token *token)
{
    if (token->type == TOKEN_TYPE_STRING || token->type == TOKEN_TYPE_IDENTIFIER)
    {
        free(token->attr.str_val);
        token->attr.str_val = NULL;
    }
}

const char *token_type_name(TokenType type)
{
    switch (type)
    {
        case TOKEN_TYPE_EOF:
            return "TOKEN_TYPE_EOF";
        case TOKEN_TYPE_EOL:
            return "TOKEN_TYPE_EOL";
        case TOKEN_TYPE_INDENT:
            return "TOKEN_TYPE_INDENT";
        case TOKEN_TYPE_DEDENT:
            return "TOKEN_TYPE_DEDENT";

        // Identifikatory a klucove slova.
        case TOKEN_TYPE_IDENTIFIER:
            return "TOKEN_TYPE_IDENTIFIER";
        case TOKEN_TYPE_KEYWORD:
            return "TOKEN_TYPE_KEYWORD";

        // Literaly.
        case TOKEN_TYPE_INT:
            return "TOKEN_TYPE_INT";
        case TOKEN_TYPE_DOUBLE:
            return "TOKEN_TYPE_DOUBLE";
        case TOKEN_TYPE_STRING:
            return "TOKEN_TYPE_STRING";
        case TOKEN_TYPE_UNIT:
            return "TOKEN_TYPE_UNIT";
        case TOKEN_TYPE_BOOL:
            return "TOKEN_TYPE_BOOL";

        // Operatory.
        case TOKEN_TYPE_PLUS:
            return "TOKEN_TYPE_PLUS";
        case TOKEN_TYPE_MINUS:
            return "TOKEN_TYPE_MINUS";
        case TOKEN_TYPE_MUL:
            return "TOKEN_TYPE_MUL";
        case TOKEN_TYPE_DIV:
            return "TOKEN_TYPE_DIV";
        case TOKEN_TYPE_ASSIGN:
            return "TOKEN_TYPE_ASSIGN";

        // Relacne operatory.
        case TOKEN_TYPE_EQ:
            return "TOKEN_TYPE_EQ";
        case TOKEN_TYPE_NEQ:
            return "TOKEN_TYPE_NEQ";
        case TOKEN_TYPE_LT:
            return "TOKEN_TYPE_LT";
        case TOKEN_TYPE_LTE:
            return "TOKEN_TYPE_LTE";
        case TOKEN_TYPE_GT:
            return "TOKEN_TYPE_GT";
        case TOKEN_TYPE_GTE:
            return "TOKEN_TYPE_GTE";

        // Operatory z rozsireni.
        case TOKEN_TYPE_NOT:
            return "TOKEN_TYPE_NOT";
        case TOKEN_TYPE_AND:
            return "TOKEN_TYPE_AND";
        case TOKEN_TYPE_OR:
            return "TOKEN_TYPE_OR";
        case TOKEN_TYPE_ARROW:
            return "TOKEN_TYPE_ARROW";
        case TOKEN_TYPE_CONCAT:
            return "TOKEN_TYPE_CONCAT";
        case TOKEN_TYPE_DOT:
            return "TOKEN_TYPE_DOT";

        // Oddelovace.
        case TOKEN_TYPE_LPAREN:
            return "TOKEN_TYPE_LPAREN";
        case TOKEN_TYPE_RPAREN:
            return "TOKEN_TYPE_RPAREN";
        case TOKEN_TYPE_LBRACKET:
            return "TOKEN_TYPE_LBRACKET";
        case TOKEN_TYPE_RBRACKET:
            return "TOKEN_TYPE_RBRACKET";
        case TOKEN_TYPE_COLON:
            return "TOKEN_TYPE_COLON";
        case TOKEN_TYPE_COMMA:
            return "TOKEN_TYPE_COMMA";
        case TOKEN_TYPE_SEMICOLON:
            return "TOKEN_TYPE_SEMICOLON";
        case TOKEN_TYPE_AT_MAIN:
            return "TOKEN_TYPE_AT_MAIN";

        // Chyba a parser.
        case TOKEN_ERROR_LEX:
            return "TOKEN_ERROR_LEX";
        case TOKEN_TYPE_STATEMENT:
            return "TOKEN_TYPE_STATEMENT";

        // Keywords.
        case KEYWORD_DEF:
            return "KEYWORD_DEF";
        case KEYWORD_DO:
            return "KEYWORD_DO";
        case KEYWORD_ELSE:
            return "KEYWORD_ELSE";
        case KEYWORD_IF:
            return "KEYWORD_IF";
        case KEYWORD_IMPORT:
            return "KEYWORD_IMPORT";
        case KEYWORD_RETURN:
            return "KEYWORD_RETURN";
        case KEYWORD_THEN:
            return "KEYWORD_THEN";
        case KEYWORD_VAL:
            return "KEYWORD_VAL";
        case KEYWORD_WHILE:
            return "KEYWORD_WHILE";
        case KEYWORD_VAR:
            return "KEYWORD_VAR";
        case KEYWORD_STRING:
            return "KEYWORD_STRING";
        case KEYWORD_DOUBLE:
            return "KEYWORD_DOUBLE";
        case KEYWORD_INT:
            return "KEYWORD_INT";
        case KEYWORD_UNIT:
            return "KEYWORD_UNIT";
        case KEYWORD_BOOLEAN:
            return "KEYWORD_BOOLEAN";
        case KEYWORD_FOR:
            return "KEYWORD_FOR";
        case KEYWORD_TO:
            return "KEYWORD_TO";
        case KEYWORD_UNTIL:
            return "KEYWORD_UNTIL";
        case KEYWORD_BY:
            return "KEYWORD_BY";
        case KEYWORD_LIST:
            return "KEYWORD_LIST";

        default:
            return "UNKNOWN_TOKEN";
    }
}