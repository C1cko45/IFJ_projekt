#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    int *data;
    int top;
    int capacity;
} Stack;

// inicializacia zasobniku.
bool stack_init(Stack *stack);

// pridanie do zasobnika (na top) PUSH.
bool stack_push(Stack *stack, int indent_value);

// odobratie zo zasobniku POP.
void stack_pop_scanner(Stack *stack);

// ziskanie hodnoty na vrchole (on top).
int stack_get_top(Stack *stack);

// uvolnenie pamete.
void stack_free(Stack *stack);

#endif // STACK_H