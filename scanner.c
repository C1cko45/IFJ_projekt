#include "dstring.h"
#include "stack.h"
#include "token.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// struktura pre stavy.
typedef enum
{
    START = 0,
    STATE_LESS,
    STATE_GREATER,
    STATE_ASSIGN,
    STATE_AND,
    STATE_OR,
    STATE_MARK, // '!'
    STATE_PLUS,
    STATE_MAIN, // @
    STATE_SLASH,
    STATE_ID,
    STATE_STRING,
    STATE_STRING_ESCAPE,
    STATE_DOT,
    STATE_DIGIT_DOT,
    STATE_ZERO,
    STATE_DIGIT,
    STATE_DOUBLE,
    STATE_EXP,
    STATE_EXP_SIGN,
    STATE_IS_EXP,
    STATE_UNDERSCORE,
    STATE_DOUBLE_UNDERSCORE,
    STATE_EXP_UNDERSCORE,
    STATE_INDENT,
    STATE_DEDENT,
} ScannerState;

// struktura kde budu vsetky keywords.
typedef struct
{
    const char *text;
    TokenType keyword_type;
} KeywordMap;

// tabulka keywords.
static const KeywordMap keyword_table[] = {
    {"def", KEYWORD_DEF},       {"do", KEYWORD_DO},         {"else", KEYWORD_ELSE},       {"if", KEYWORD_IF},
    {"import", KEYWORD_IMPORT}, {"return", KEYWORD_RETURN}, {"then", KEYWORD_THEN},       {"val", KEYWORD_VAL},
    {"while", KEYWORD_WHILE},   {"var", KEYWORD_VAR},       {"String", KEYWORD_STRING},   {"Double", KEYWORD_DOUBLE},
    {"Int", KEYWORD_INT},       {"Unit", KEYWORD_UNIT},     {"Boolean", KEYWORD_BOOLEAN}, {"for", KEYWORD_FOR},
    {"List", KEYWORD_LIST},     {"to", KEYWORD_TO},         {"until", KEYWORD_UNTIL},     {"by", KEYWORD_BY},
};
//deklaracia scanner_init
void scanner_init(FILE *source) {
    (void)source;
}

// vypocet slov v tabulke.
#define KEYWORD_COUNT (sizeof(keyword_table) / sizeof(keyword_table[0]))

// pomocna funckia na ziskanie zhody. ak je vrati true a keyword ak nieje vrati false.
bool is_keyword(const char *word, TokenType *out_keyword)
{
    for (size_t index_word = 0; index_word < KEYWORD_COUNT; index_word++)
    {
        if (strcmp(word, keyword_table[index_word].text) == 0)
        {
            *out_keyword = keyword_table[index_word].keyword_type;
            return true;
        }
    }
    return false;
}

#define START 0
// hlavna funcia scanneru.
static Token get_token_internal(void)
{
    Token token;
    dstring string;
    static Stack stack;

    int actual_state = START;
    // indikator noveho riadku.
    static bool is_at_new_line = true;
    // counter medzier na zaciatku riadku.
    static int space_counter = 0;
    // kontrola jednotnej inicializacie stacku.
    static bool stack_is_initialized = false;
    // kontrola vratenia vsetkych ukoncenych indentov. (dedentov)
    static bool dedent_in_progress = false;
    // kontrola ci sa automat nachadza v zatvorke.
    static int paren_depth = 0;

    if (!stack_is_initialized)
    {
        stack_init(&stack);
        stack_is_initialized = true;
    }
    // kontrola viacerych dedentov.
    if (dedent_in_progress)
    {
        int stack_top = stack_get_top(&stack);
        if (space_counter < stack_top)
        {
            actual_state = STATE_DEDENT;
        }
        else if (space_counter == stack_top)
        {
            dedent_in_progress = false;
            actual_state = START;
        }
        else
        {
            token.type = TOKEN_ERROR_LEX;
            return token;
        }
    }
    else if (is_at_new_line)
    {
        actual_state = STATE_INDENT;
        space_counter = 0;
        is_at_new_line = false;
    }

    while (true)
    {
        int character = fgetc(stdin);
        switch (actual_state)
        {
        case START:
            // ignorovanie medzier.
            if (character == ' ' || character == '\t')
            {
                continue;
            }
            // keyword && indentifier.
            if (isalpha(character) || character == '_')
            {
                actual_state = STATE_ID;
                dstring_init(&string);
                dstring_add_char(&string, character);
                break;
            }
            // ak je nula prepni na stav nula.
            if (character == '0')
            {
                actual_state = STATE_ZERO;
                dstring_init(&string);
                dstring_add_char(&string, character);
                break;
            }
            // ak je cislo od 1 do 9 prepni na srav cislo.
            if (character >= '1' && character <= '9')
            {
                actual_state = STATE_DIGIT;
                dstring_init(&string);
                dstring_add_char(&string, character);
                break;
            }
            switch (character)
            {
            // instant tokens.
            case '(':
                token.type = TOKEN_TYPE_LPAREN;
                paren_depth++;
                return token;
            case ')':
                token.type = TOKEN_TYPE_RPAREN;
                if (paren_depth > 0) {
                    paren_depth--;
                }
                return token;
            case '[':
                token.type = TOKEN_TYPE_LBRACKET;
                return token;
            case ']':
                token.type = TOKEN_TYPE_RBRACKET;
                return token;
            case '*':
                token.type = TOKEN_TYPE_MUL;
                return token;
            case '-':
                token.type = TOKEN_TYPE_MINUS;
                return token;
            case ';':
                token.type = TOKEN_TYPE_SEMICOLON;
                return token;
            case '.':
                actual_state = STATE_DOT;
                dstring_init(&string);
                dstring_add_char(&string, character);
                break;
            case ',':
                token.type = TOKEN_TYPE_COMMA;
                return token;
            case ':':
                token.type = TOKEN_TYPE_COLON;
                return token;

            // tokens.
            case '=':
                actual_state = STATE_ASSIGN;
                break;
            case '!':
                actual_state = STATE_MARK;
                break;
            case '+':
                actual_state = STATE_PLUS;
                break;
            case '<':
                actual_state = STATE_LESS;
                break;
            case '>':
                actual_state = STATE_GREATER;
                break;
            case '&':
                actual_state = STATE_AND;
                break;
            case '|':
                actual_state = STATE_OR;
                break;
            case '@':
                actual_state = STATE_MAIN;
                break;
            // string.
            case '"':
                actual_state = STATE_STRING;
                dstring_init(&string);
                break;
            // komenty.
            case '/':
                actual_state = STATE_SLASH;
                break;
            // koniec riadku.
            case '\n':
                token.type = TOKEN_TYPE_EOL;
                if (paren_depth > 0) {
                    break;
                }
                is_at_new_line = true;
                return token;
            // konec suboru. + kontrola zabudnutych dedentov.
            case EOF:
                if (stack_get_top(&stack) > 0)
                {   
                    stack_pop_scanner(&stack);
                    token.type = TOKEN_TYPE_DEDENT;
                    return token;
                }
                token.type = TOKEN_TYPE_EOF;
                return token;
            // chyba 1.
            default:
                token.type = TOKEN_ERROR_LEX;
                return token;
            }
            break;
        case STATE_ASSIGN:
            if (character == '=')
            {
                token.type = TOKEN_TYPE_EQ;
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_ASSIGN;
                return token;
            }
            break;
        case STATE_MARK:
            if (character == '=')
            {
                token.type = TOKEN_TYPE_NEQ;
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_NOT;
                return token;
            }
            break;
        case STATE_PLUS:
            if (character == '+')
            {
                token.type = TOKEN_TYPE_CONCAT;
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_PLUS;
                return token;
            }
            break;
        case STATE_LESS:
            if (character == '=')
            {
                token.type = TOKEN_TYPE_LTE;
                return token;
            }
            else if (character == '-')
            {
                token.type = TOKEN_TYPE_ARROW;
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_LT;
                return token;
            }
            break;
        case STATE_GREATER:
            if (character == '=')
            {
                token.type = TOKEN_TYPE_GTE;
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_GT;
                return token;
            }
            break;
        case STATE_AND:
            if (character == '&')
            {
                token.type = TOKEN_TYPE_AND;
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_ERROR_LEX;
                return token;
            }
            break;
        case STATE_OR:
            if (character == '|')
            {
                token.type = TOKEN_TYPE_OR;
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_ERROR_LEX;
                return token;
            }
            break;
        case STATE_MAIN:
            if (character == 'm')
            {
                if (fgetc(stdin) == 'a')
                {
                    if (fgetc(stdin) == 'i')
                    {
                        if (fgetc(stdin) == 'n')
                        {
                            token.type = TOKEN_TYPE_AT_MAIN;
                            return token;
                        }
                    }
                }
            }
            token.type = TOKEN_ERROR_LEX;
            return token;
        // obycajny komentar.
        case STATE_SLASH:
            if (character == '/')
            {
                int character;
                do
                {
                    character = fgetc(stdin);
                } while (character != '\n' && character != EOF);

                if (character == EOF)
                {
                    ungetc(character, stdin);
                }
                if (paren_depth > 0) {
                    break;
                }
                token.type = TOKEN_TYPE_EOL;
                is_at_new_line = true;
                return token;
            }
            else if (character == '*')
            {
                int comment_depth = 1;
                bool has_newline = false;

                while (comment_depth > 0)
                {
                    int character = fgetc(stdin);
                    // nekonecny blokovy komentar = lexikalna chyba 1.
                    if (character == EOF)
                    {
                        token.type = TOKEN_ERROR_LEX;
                        return token;
                    }
                    // dalsi riadok.
                    else if (character == '\n')
                    {
                        has_newline = true;
                    }
                    // detekcia vnorenia.
                    else if (character == '/')
                    {
                        int next_char = fgetc(stdin);
                        if (next_char == '*')
                        {
                            comment_depth++;
                        }
                        else
                        {
                            ungetc(next_char, stdin);
                        }
                    }
                    else if (character == '*')
                    {
                        int next_char = fgetc(stdin);
                        if (next_char == '/')
                        {
                            comment_depth--;
                        }
                        else
                        {
                            ungetc(next_char, stdin);
                        }
                    }
                }
                // ak ma novy riadok vratime EOL.
                if (has_newline)
                {
                    token.type = TOKEN_TYPE_EOL;
                    is_at_new_line = true;
                    return token;
                }
                else
                {
                    // ak nema novy riadok vratime sa spet na zaciatok a hladame novy token.
                    actual_state = START;
                    break;
                }
            }
            else
            { // inak je to deleno.
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_DIV;
                return token;
            }
            break;
        case STATE_ID:
            while (isalnum(character) || character == '_')
            {
                dstring_add_char(&string, character);
                character = fgetc(stdin);
            }
            ungetc(character, stdin);
            // skontrolovanie keywordu.
            TokenType keyword;
            if (strcmp(string.str, "true") == 0)
            {
                token.type = TOKEN_TYPE_BOOL;
                token.attr.bool_val = true;
                dstring_free(&string);
                return token;
            }
            else if (strcmp(string.str, "false") == 0)
            {
                token.type = TOKEN_TYPE_BOOL;
                token.attr.bool_val = false;
                dstring_free(&string);
                return token;
            }
            else if (is_keyword(string.str, &keyword))
            {
                dstring_free(&string);
                token.type = keyword;
                return token;
            }
            else
            {
                token.attr.str_val = string.str;
                token.type = TOKEN_TYPE_IDENTIFIER;
                return token;
            }
            break;
        case STATE_STRING:
            if (character == '"')
            {
                token.type = TOKEN_TYPE_STRING;
                token.attr.str_val = string.str;
                return token;
            }
            else if (character == '\\')
            {
                // skok na escape sekvencie.
                actual_state = STATE_STRING_ESCAPE;
                break;
            }
            else if (character == '\n' || character == EOF)
            {
                token.type = TOKEN_ERROR_LEX;
                dstring_free(&string);
                return token;
            }
            else
            {
                dstring_add_char(&string, character);
                break;
            }
        case STATE_STRING_ESCAPE:
            if (character == '"')
            {
                dstring_add_char(&string, '"');
                actual_state = STATE_STRING;
            }
            else if (character == '\\')
            {
                dstring_add_char(&string, '\\');
                actual_state = STATE_STRING;
            }
            else if (character == 'n')
            {
                dstring_add_char(&string, '\n');
                actual_state = STATE_STRING;
            }
            else if (character == 'r')
            {
                dstring_add_char(&string, '\r');
                actual_state = STATE_STRING;
            }
            else if (character == 't')
            {
                dstring_add_char(&string, '\t');
                actual_state = STATE_STRING;
            }
            else
            {
                token.type = TOKEN_ERROR_LEX;
                dstring_free(&string);
                return token;
            }
            break;
        // TODO: OPRAVIT
        case STATE_ZERO:
            if (character == '.')
            {
                actual_state = STATE_DIGIT_DOT;
                // dstring_add_char(&string, '.');
                break;
            }
            else if (character == 'e' || character == 'E')
            {
                actual_state = STATE_EXP;
                dstring_add_char(&string, character);
                break;
            }
            else if (character == 'd' || character == 'D')
            {
                token.type = TOKEN_TYPE_DOUBLE;
                token.attr.double_val = strtod(string.str, NULL);
                dstring_free(&string);
                return token;
            }
            else if ((character >= '0' && character <= '9') || character == '_')
            {
                token.type = TOKEN_ERROR_LEX;
                dstring_free(&string);
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_INT;
                token.attr.int_val = atol(string.str);
                dstring_free(&string);
                return token;
            }
            break;
        case STATE_DIGIT:
            if (character >= '0' && character <= '9')
            {
                dstring_add_char(&string, character);
                break;
            }
            else if (character == 'd' || character == 'D')
            {
                token.type = TOKEN_TYPE_DOUBLE;
                token.attr.double_val = strtod(string.str, NULL);
                dstring_free(&string);
                return token;
            }
            else if (character == 'e' || character == 'E')
            {
                actual_state = STATE_EXP;
                dstring_add_char(&string, character);
                break;
            }
            else if (character == '.')
            {
                actual_state = STATE_DIGIT_DOT;
                break;
            }
            else if (character == '_')
            {
                actual_state = STATE_UNDERSCORE;
                // dstring add nieje pretoze sa snazim vyhnut problemu s prevodom cisla na int alebo double.
                break;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_INT;
                token.attr.int_val = atol(string.str);
                dstring_free(&string);
                return token;
            }
            break;
        case STATE_DIGIT_DOT:
            if (character >= '0' && character <= '9')
            {
                actual_state = STATE_DOUBLE;
                dstring_add_char(&string, '.');
                dstring_add_char(&string, character);
                break;
            }
            else
            {
                ungetc(character, stdin);
                ungetc('.', stdin);
                token.attr.int_val = atol(string.str);
                token.type = TOKEN_TYPE_INT;
                dstring_free(&string);
                return token;
            }
        case STATE_DOT:
            if (character >= '0' && character <= '9')
            {
                actual_state = STATE_DOUBLE;
                dstring_add_char(&string, character);
                break;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_DOT;
                dstring_free(&string);
                return token;
            }
            break;
        // TODO:
        case STATE_DOUBLE:
            if (character >= '0' && character <= '9')
            {
                dstring_add_char(&string, character);
                break;
            }
            else if (character == '_')
            {
                actual_state = STATE_DOUBLE_UNDERSCORE;
                break;
            }
            else if (character == 'e' || character == 'E')
            {
                dstring_add_char(&string, character);
                actual_state = STATE_EXP;
                break;
            }
            else if (character == 'd' || character == 'D')
            {
                token.type = TOKEN_TYPE_DOUBLE;
                token.attr.double_val = strtod(string.str, NULL);
                dstring_free(&string);
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_DOUBLE;
                token.attr.double_val = strtod(string.str, NULL);
                dstring_free(&string);
                return token;
            }
            break;
        case STATE_UNDERSCORE:
            if (character >= '0' && character <= '9')
            {
                actual_state = STATE_DIGIT;
                dstring_add_char(&string, character);
                break;
            }
            else
            {
                token.type = TOKEN_ERROR_LEX;
                dstring_free(&string);
                return token;
            }
            break;
        case STATE_DOUBLE_UNDERSCORE:
            if (character >= '0' && character <= '9')
            {
                actual_state = STATE_DOUBLE;
                dstring_add_char(&string, character);
                break;
            }
            else
            {
                token.type = TOKEN_ERROR_LEX;
                dstring_free(&string);
                return token;
            }
            break;
        case STATE_EXP:
            if (character == '+' || character == '-')
            {
                actual_state = STATE_EXP_SIGN;
                dstring_add_char(&string, character);
                break;
            }
            else if (character >= '0' && character <= '9')
            {
                actual_state = STATE_IS_EXP;
                dstring_add_char(&string, character);
                break;
            }
            else
            {
                token.type = TOKEN_ERROR_LEX;
                dstring_free(&string);
                return token;
            }
            break;
        case STATE_EXP_SIGN:
            if (character >= '0' && character <= '9')
            {
                actual_state = STATE_IS_EXP;
                dstring_add_char(&string, character);
                break;
            }
            else
            {
                token.type = TOKEN_ERROR_LEX;
                dstring_free(&string);
                return token;
            }
            break;
        case STATE_IS_EXP:
            if (character >= '0' && character <= '9')
            {
                dstring_add_char(&string, character);
                break;
            }
            else if (character == '_')
            {
                actual_state = STATE_EXP_UNDERSCORE;
                break;
            }
            else if (character == 'd' || character == 'D')
            {
                token.type = TOKEN_TYPE_DOUBLE;
                token.attr.double_val = strtod(string.str, NULL);
                dstring_free(&string);
                return token;
            }
            else
            {
                ungetc(character, stdin);
                token.type = TOKEN_TYPE_DOUBLE;
                token.attr.double_val = strtod(string.str, NULL);
                dstring_free(&string);
                return token;
            }
            break;
        case STATE_EXP_UNDERSCORE:
            if (character >= '0' && character <= '9')
            {
                actual_state = STATE_IS_EXP;
                dstring_add_char(&string, character);
                break;
            }
            else
            {
                token.type = TOKEN_ERROR_LEX;
                dstring_free(&string);
                return token;
            }
            break;
        case STATE_INDENT:
            if (character == ' ')
            {
                space_counter++;
            }
            //pridane
            else if(character == '\t')
            {
                token.type = TOKEN_ERROR_LEX;
                return token;
            }
            else if (character == '\n')
            {
                space_counter = 0;
                is_at_new_line = true;
                token.type = TOKEN_TYPE_EOL;
                return token;
            }
            else
            {
                ungetc(character, stdin);
                int stack_top = stack_get_top(&stack);
                if (space_counter > stack_top)
                {
                    stack_push(&stack, space_counter);
                    token.type = TOKEN_TYPE_INDENT;
                    return token;
                }
                else if (space_counter < stack_top)
                {
                    dedent_in_progress = true;
                    actual_state = STATE_DEDENT;
                    break;
                }
                else
                {
                    actual_state = START;
                    break;
                }
            }
            break;
        case STATE_DEDENT:
            ungetc(character, stdin);
            stack_pop_scanner(&stack);
            token.type = TOKEN_TYPE_DEDENT;
            return token;
        }
    }
}

// obal nad get_token_internal.
Token scanner_get_next_token(void) {
    static TokenType last_type = TOKEN_TYPE_EOL;
    static bool has_pending = false;
    static Token pending;

    Token token;
    if (has_pending) {
        token = pending;
        has_pending = false;
    } else {
        token = get_token_internal();
        // DEDENT/EOF prisiel bez predchadzajuceho EOL
        if ((token.type == TOKEN_TYPE_DEDENT || token.type == TOKEN_TYPE_EOF) &&
            last_type != TOKEN_TYPE_EOL && last_type != TOKEN_TYPE_DEDENT)
        {
            pending = token;
            has_pending = true;
            token.type = TOKEN_TYPE_EOL;
        }
    }

    last_type = token.type;
    return token;
}