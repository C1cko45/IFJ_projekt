#include "stack.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// inicializacia stacku, pociatocna capacita 10, hodnoty 0;
/*
bool stack_init(Stack *stack)
{
    stack->capacity = 10;
    stack->data = malloc(stack->capacity * sizeof(int));
    if (stack->data == NULL)
        return false;

    stack->top = 0;
    stack->data[0] = 0;
    return true;
}
*/
bool stack_init(Stack *stack)
{
    if (stack == NULL)
        return false;

    stack->top = 0;
    stack->capacity = 0;
    stack->data = malloc(10 * sizeof(int));

    if (stack->data == NULL)
        return false;

    stack->capacity = 10;
    stack->data[0] = 0;

    return true;
}

// pridanie do zasobnika (PUSH)
/*
bool stack_push(Stack *stack, int indent_value)
{
    stack->top++;
    if (stack->top >= stack->capacity)
    {
        stack->capacity *= 2;
        int *new_data = realloc(stack->data, stack->capacity * sizeof(int));
        if (new_data == NULL)
            return false;
        stack->data = new_data;
    }
    stack->data[stack->top] = indent_value;
    return true;
}
*/

bool stack_push(Stack *stack, int indent_value)
{
    if (stack == NULL || stack->data == NULL)
        return false;

    if (stack->top + 1 >= stack->capacity)
    {
        int new_capacity = stack->capacity * 2;
        int *new_data = realloc(stack->data, new_capacity * sizeof(int));

        if (new_data == NULL)
            return false;

        stack->data = new_data;
        stack->capacity = new_capacity;
    }

    stack->top++;
    stack->data[stack->top] = indent_value;

    return true;
}

// odobratie zo zasobnika (POP)
void stack_pop_scanner(Stack *stack)
{
    if (stack->top > 0)
    {
        stack->top--;
    }
}

// ziskanie hodnoty na vrchole
int stack_get_top(Stack *stack)
{
    return stack->data[stack->top];
}

// uvolnenie pamate
/*
void stack_free(Stack *stack)
{
    free(stack->data);
    stack->data = NULL;
}
*/
void stack_free(Stack *stack)
{
    if (stack == NULL)
        return;

    free(stack->data);
    stack->data = NULL;
    stack->top = 0;
    stack->capacity = 0;
}