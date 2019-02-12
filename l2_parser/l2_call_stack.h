#ifndef _L2_CALL_STACK_H_
#define _L2_CALL_STACK_H_

#include "../l2_base/l2_stack.h"

typedef struct _l2_call_frame {
    int ret_pos;

}l2_call_frame;

typedef struct _l2_call_stack {
    l2_stack stack;
}l2_call_stack;

l2_call_stack *l2_call_stack_create();
void l2_call_stack_destroy(l2_call_stack *call_stack_p);

void l2_call_stack_push_frame(l2_call_stack *call_stack_p, l2_call_frame call_frame);
l2_call_frame l2_call_stack_pop_frame(l2_call_stack *call_stack_p);
int l2_call_stack_is_empty(l2_call_stack *call_stack_p);

#endif
