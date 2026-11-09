#include <stdio.h>
#include <stdbool.h>
#include "token.h"
#include "dstring.h"

#define START 0

Token scanner_get_next_token(void) {
    Token token;
    int actual_state = START;

    while (true) {
        int character = fgetc(stdin);
        switch(actual_state) {
            case START:
                //ignorovanie medzier.
                if (character == ' ' || character == '\t') {
                    continue;
                }

                switch(character) {
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
                    case EOF:
                        token.type = TOKEN_TYPE_EOF;
                        return token;
                    default:
                        token.type = TOKEN_ERROR_LEX;
                        return token;
                }   
                break;
        }
    }
}

int main(){

    return 0;
}