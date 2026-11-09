#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include "token.h"
#include "dstring.h"

typedef enum {
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
} ScannerState;

#define START 0

Token scanner_get_next_token(void) {
    Token token;
    int actual_state = START;

    dstring string;
    dstring_init(&string);

    while (true) {
        int character = fgetc(stdin);
        switch(actual_state) {
            case START:
                //ignorovanie medzier.
                if (character == ' ' || character == '\t') {
                    continue;
                }
                //keyword && indentifier.
                if (isalpha(character) || character == '_') {
                    actual_state = STATE_ID;
                    dstring_clear(&string);
                    dstring_add_char(&string, character);
                    break;
                }

                switch(character) {
                    //instant tokens.
                    case '(':
                        token.type = TOKEN_TYPE_LPAREN;
                        return token;
                    case ')':
                        token.type = TOKEN_TYPE_RPAREN;
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
                        token.type = TOKEN_TYPE_DOT;
                        return token;
                    case ',':
                        token.type = TOKEN_TYPE_COMMA;
                        return token;
                    case ':':
                        token.type = TOKEN_TYPE_COLON;
                        return token;
                    
                    //tokens.
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

                    //komenty.
                    case '/':
                        actual_state = STATE_SLASH;
                        break;
                    //koniec riadku.
                    case '\n':
                        token.type = TOKEN_TYPE_EOL;
                        return token;
                    // konec suboru.
                    case EOF:
                        token.type = TOKEN_TYPE_EOF;
                        return token;
                    //chyba 1.
                    default:
                        token.type = TOKEN_ERROR_LEX;
                        return token;
                }   
                break;
            case STATE_ASSIGN:
                if (character == '=') {
                    token.type = TOKEN_TYPE_EQ;
                    return token;
                } else {
                    ungetc(character, stdin);
                    token.type = TOKEN_TYPE_ASSIGN;
                    return token;
                }
                break;
            case STATE_MARK:
                if (character == '=') {
                    token.type = TOKEN_TYPE_NEQ;
                    return token;
                } else {
                    ungetc(character, stdin);
                    token.type = TOKEN_TYPE_NOT;
                    return token;
                }
                break;
            case STATE_PLUS:
                if (character == '+') {
                    token.type = TOKEN_TYPE_CONCAT;
                    return token;
                } else {
                    ungetc(character, stdin);
                    token.type = TOKEN_TYPE_PLUS;
                    return token;
                }
                break;
            case STATE_LESS:
                if (character == '=') {
                    token.type = TOKEN_TYPE_LTE;
                    return token;
                } 
                else if (character == '-') {
                    token.type = TOKEN_TYPE_ARROW;
                    return token;
                } else {
                    ungetc(character, stdin);
                    token.type = TOKEN_TYPE_LT;
                    return token;
                }
                break;
            case STATE_GREATER:
                if (character == '=') {
                    token.type = TOKEN_TYPE_GTE;
                    return token;
                } else {
                    ungetc(character, stdin);
                    token.type = TOKEN_TYPE_GT;
                    return token;
                }
                break;
            case STATE_AND:
                if (character == '&') {
                    token.type = TOKEN_TYPE_AND;
                    return token;
                } else {
                    ungetc(character, stdin);
                    token.type = TOKEN_ERROR_LEX;
                    return token;
                }
                break;
            case STATE_OR:
                if (character == '|') {
                    token.type = TOKEN_TYPE_OR;
                    return token;
                } else {
                    ungetc(character, stdin);
                    token.type = TOKEN_ERROR_LEX;
                    return token;
                }
                break;
            case STATE_MAIN:
                if (character == 'm') {
                    if (fgetc(stdin) == 'a') {
                        if (fgetc(stdin) == 'i') {
                            if (fgetc(stdin) == 'n') {
                                token.type = TOKEN_TYPE_AT_MAIN;
                                return token;
                            }
                        }
                    }
                }
                token.type = TOKEN_ERROR_LEX;
                return token;
            case STATE_SLASH:
                if (character == '/') {
                    int character;
                    do {
                        character = fgetc(stdin);
                    } while (character != '\n' && character != EOF);

                    if (character == EOF) {
                        ungetc(character, stdin);
                    }

                    token.type = TOKEN_TYPE_EOL;
                    return token;
                } 
                else if (character == '*') {
                    int comment_depth = 1;
                    bool has_newline = false;

                    while (comment_depth > 0) {
                        int character = fgetc(stdin);
                        //nekonecny blokovy komentar = lexikalna chyba 1.
                        if (character == EOF) {
                            token.type = TOKEN_ERROR_LEX;
                            return token;
                        }
                        //dalsi riadok.
                        else if (character == '\n') {
                            has_newline = true;
                        }
                        //detekcia vnorenia.
                        else if (character == '/') {
                            int next_char = fgetc(stdin);
                            if (next_char == '*') {
                                comment_depth++;
                            } else {
                                ungetc(next_char, stdin);
                            }
                        }
                        else if (character == '*') {
                            int next_char = fgetc(stdin);
                            if (next_char == '/') {
                                comment_depth--;
                            } else {
                                ungetc(next_char, stdin);
                            }
                        }
                    }

                    if (has_newline) {
                        token.type = TOKEN_TYPE_EOL;
                        return token;
                    } else {
                        actual_state = START;
                        break;
                    }

                } else {
                    ungetc(character, stdin);
                    token.type = TOKEN_TYPE_DIV;
                    return token;
                }
                break;
            case STATE_ID:
                while (isalnum(character) || character == '_') {
                    dstring_add_char(&string, character);
                    character = fgetc(stdin);
                } 
                ungetc(character, stdin);
                //TODO funckia na skontrolovanie keywordu.
        }
    }
}

int main(){

    return 0;
}