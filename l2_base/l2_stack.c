#include "l2_stack.h"
#include "../l2_system/l2_assert.h"
#include "../l2_parser/l2_parse.h"

extern l2_parser *g_parser_p;

boolean l2_stack_avail(const l2_stack *stack) {
    return stack->stack_p != L2_NULL_PTR;
}

void l2_stack_create(l2_stack *stack, const l2_mem_size size_of_single_elem) {
    l2_assert(stack, L2_INTERNAL_ERROR_NULL_POINTER);
    stack->size = 0;
    stack->max_size = 100;
    stack->stack_p = l2_storage_mem_new(g_parser_p->storage_p, size_of_single_elem * stack->max_size);
    stack->single_size = size_of_single_elem;
}

void l2_stack_destroy(l2_stack *stack) {
    l2_assert(stack, L2_INTERNAL_ERROR_NULL_POINTER);
    stack->size = 0;
    stack->max_size = 0;
    l2_storage_mem_delete(g_parser_p->storage_p, stack->stack_p);
    stack->stack_p = L2_NULL_PTR;
}

void l2_stack_push_back(l2_stack *stack, const void *data) {
    l2_assert(stack, L2_INTERNAL_ERROR_NULL_POINTER);
    int i;
    if (stack->size >= stack->max_size)
        stack->stack_p = l2_storage_mem_resize(g_parser_p->storage_p, stack->stack_p, stack->max_size * stack->single_size, (stack->max_size += 100) * stack->single_size);
    for (i = 0; i < stack->single_size; i++) {
        ((char *)(stack->stack_p))[stack->size * stack->single_size + i] = ((char *)data)[i];
    }
    stack->size += 1;
}

void *l2_stack_pop(l2_stack *stack) {
    l2_assert(stack, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(stack->size > 0, L2_INTERNAL_ERROR_OUT_OF_RANGE);
    return (char *)stack->stack_p + (--stack->size * stack->single_size);
}

void *l2_stack_back(const l2_stack *stack) {
    l2_assert(stack, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(stack->size > 0, L2_INTERNAL_ERROR_OUT_OF_RANGE);
    return (char *)stack->stack_p + ((stack->size - 1) * stack->single_size);
}