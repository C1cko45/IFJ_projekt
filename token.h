/*
 * Implementácia prekladača pre jazyk IFJ26.
 *
 * Typy tokenov a štruktúra tokenu (výstup lexikálneho analyzátora).
 *
 * Autor:   Deák Dominik
 * Login:   xdeakdo00
 */
#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h> // rozsirenie boolthen;

// Typy rozpoznanych lexikalnych jednotiek/tokenov + keywords.
typedef enum
{
    // Riadiace tokeny.
    TOKEN_TYPE_EOF,
    TOKEN_TYPE_EOL,
    TOKEN_TYPE_INDENT,
    TOKEN_TYPE_DEDENT,

    // identifikatory a klucove slova.
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_KEYWORD,

    // Literaly.
    TOKEN_TYPE_INT,
    TOKEN_TYPE_DOUBLE,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_UNIT,
    TOKEN_TYPE_BOOL, // rozsirenie boolthen;

    // Operatory.
    TOKEN_TYPE_PLUS,
    TOKEN_TYPE_MINUS,
    TOKEN_TYPE_MUL,
    TOKEN_TYPE_DIV,
    TOKEN_TYPE_ASSIGN,

    // Relacne operatory.
    TOKEN_TYPE_EQ,
    TOKEN_TYPE_NEQ,
    TOKEN_TYPE_LT,
    TOKEN_TYPE_LTE,
    TOKEN_TYPE_GT,
    TOKEN_TYPE_GTE,

    // oddelovace z rozsirenia.
    TOKEN_TYPE_NOT,    // !   (BOOLTHEN)
    TOKEN_TYPE_AND,    // &&  (BOOLTHEN)
    TOKEN_TYPE_OR,     // ||  (BOOLTHEN)
    TOKEN_TYPE_ARROW,  // <-  (CONTROL)
    TOKEN_TYPE_CONCAT, // ++  (LISTS)
    TOKEN_TYPE_DOT,    // .   (VALMEM)

    // Oddelovace.
    TOKEN_TYPE_LPAREN,
    TOKEN_TYPE_RPAREN,
    TOKEN_TYPE_LBRACKET, // [  rozsirenie lists;
    TOKEN_TYPE_RBRACKET, // ]  rozsirenie lists;
    TOKEN_TYPE_COLON,
    TOKEN_TYPE_COMMA,
    TOKEN_TYPE_SEMICOLON,
    TOKEN_TYPE_AT_MAIN, // @main

    // chyba 1.
    TOKEN_ERROR_LEX,
    // token pre parser.
    TOKEN_TYPE_STATEMENT,

    // keywords:
    KEYWORD_DEF,
    KEYWORD_DO,
    KEYWORD_ELSE,
    KEYWORD_IF,
    KEYWORD_IMPORT,
    KEYWORD_RETURN,
    KEYWORD_THEN,
    KEYWORD_VAL,
    KEYWORD_WHILE,
    KEYWORD_VAR,

    KEYWORD_STRING,
    KEYWORD_DOUBLE,
    KEYWORD_INT,
    KEYWORD_UNIT,

    // rozsirenie BOOLTHEN:
    KEYWORD_BOOLEAN,

    // rozsirenie CONTROL:
    KEYWORD_FOR,
    KEYWORD_TO,
    KEYWORD_UNTIL,
    KEYWORD_BY,

    // rozsirenie LISTS:
    KEYWORD_LIST,
} TokenType;

typedef union
{
    long long int_val;
    double double_val;
    char *str_val;
    bool bool_val; // rozsirenie boolthen;
} TokenAttribute;

typedef struct
{
    TokenType type;
    TokenAttribute attr;
    // int line;
} Token;

void token_free(Token *token);

const char *token_type_name(TokenType type);

#endif // TOKEN_H