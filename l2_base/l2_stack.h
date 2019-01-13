#ifndef _L2_STACK_H_
#define _L2_STACK_H_

#include "../l2_mem/l2_storage.h"
#include "l2_common_type.h"

typedef struct _l2_stack {
    void *stack_p;
    l2_stack_size size;
    l2_stack_size max_size;
    l2_mem_size single_size;
}l2_stack;


void l2_stack_create(l2_stack *stack, l2_mem_size size_of_single_elem);
void l2_stack_destroy(l2_stack *stack);
void l2_stack_push_back(l2_stack *stack, const void *data);
void* l2_stack_pop(l2_stack *stack);
void* l2_stack_back(const l2_stack *stack);

#endif