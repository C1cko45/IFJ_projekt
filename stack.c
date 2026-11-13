#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "stack.h"

// inicializacia stacku, pociatocna capacita 10, hodnoty 0;
bool stack_init(Stack *stack) {
    stack->capacity = 10; 
    stack->data = malloc(stack->capacity * sizeof(int));
    if (stack->data == NULL) return false;
    
    stack->top = 0;
    stack->data[0] = 0; 
    return true;
}

// pridanie do zasobnika (PUSH)
bool stack_push(Stack *stack, int indent_value) {
    stack->top++;
    if (stack->top >= stack->capacity) {
        stack->capacity *= 2;
        int *new_data = realloc(stack->data, stack->capacity * sizeof(int));
        if (new_data == NULL) return false;
        stack->data = new_data;
    }
    stack->data[stack->top] = indent_value;
    return true;
}

// odobratie zo zasobnika (POP)
void stack_pop(Stack *stack) {
    if (stack->top > 0) {
        stack->top--;
    }
}

// ziskanie hodnoty na vrchole
int stack_get_top(Stack *stack) {
    return stack->data[stack->top];
}

// uvolnenie pamate
void stack_free(Stack *stack) {
    free(stack->data);
    stack->data = NULL;
}