#ifndef TOKEN_H
#define TOKEN_H

// Klucove slova jazyka IFJ26.
typedef enum
{
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

    //+ boolean, case, class, extension, for, list, match, type.
} Keyword;

// Typy rozpoznanych lexikalnych jednotiek/tokenov.
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

    // Oddelovace.
    TOKEN_TYPE_LPAREN,
    TOKEN_TYPE_RPAREN,
    TOKEN_TYPE_COLON,
    TOKEN_TYPE_COMMA,
    TOKEN_TYPE_SEMICOLON,
    TOKEN_TYPE_AT_MAIN, // @main
} TokenType;

typedef union {
    long long int_val;
    double double_val;
    char *str_val;
    Keyword keyword;
} TokenAttribute;

typedef struct
{
    TokenType type;
    TokenAttribute attr;
    int line;
} Token;

void token_free(Token *token);

#endif // TOKEN_H