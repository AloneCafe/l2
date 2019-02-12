#include "l2_call_stack.h"
#include "l2_parse.h"

extern l2_parser *g_parser_p;

l2_call_stack *l2_call_stack_create() {
    l2_call_stack *call_stack_p;
    call_stack_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_call_stack));

    l2_stack_create(&call_stack_p->stack, sizeof(l2_call_frame));

    return call_stack_p;
}

void l2_call_stack_destroy(l2_call_stack *call_stack_p) {
    l2_stack_destroy(&call_stack_p->stack);
    l2_storage_mem_delete(g_parser_p->storage_p, call_stack_p);
}

void l2_call_stack_push_frame(l2_call_stack *call_stack_p, l2_call_frame call_frame) {
    l2_stack_push_back(&call_stack_p->stack, &call_frame);
}

l2_call_frame l2_call_stack_pop_frame(l2_call_stack *call_stack_p) {
    return *(l2_call_frame *)l2_stack_pop(&call_stack_p->stack);
}

int l2_call_stack_is_empty(l2_call_stack *call_stack_p) {
    return call_stack_p->stack.size > 0 ? 1 : 0;
}


