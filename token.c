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
