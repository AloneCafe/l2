#ifndef _L2_GC_H_
#define _L2_GC_H_

#include "../l2_base/l2_common_type.h"

typedef struct _l2_gc_mem_node {
    void *managed_mem_p;
    int ref_count;
    struct _l2_gc_mem_node *next;
}l2_gc_mem_node;

typedef struct _l2_gc_list {
    int size;
    struct _l2_gc_mem_node *head_p;
}l2_gc_list;


l2_gc_list *l2_gc_create();
void l2_gc_destroy(l2_gc_list *gc_list_p);
void l2_gc_append(l2_gc_list *gc_list_p, void *managed_mem_p);
void l2_gc_check_and_collect(l2_gc_list *gc_list_p);
void l2_gc_increase_ref_count(l2_gc_list *gc_list_p, void *managed_mem_p);
void l2_gc_decrease_ref_count(l2_gc_list *gc_list_p, void *managed_mem_p);
void l2_gc_free_gc_mem_link(l2_gc_mem_node *gc_mem_link_p);

#endif
