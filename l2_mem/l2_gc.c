#include "stdlib.h"
#include "l2_gc.h"
#include "l2_storage.h"
#include "../l2_system/l2_error.h"
#include "../l2_parser/l2_parse.h"

extern l2_parser *g_parser_p;

void l2_gc_free_gc_mem_link(l2_gc_mem_node *gc_mem_link_p) {
    if (gc_mem_link_p == L2_NULL_PTR) return;
    l2_gc_free_gc_mem_link(gc_mem_link_p->next);
    l2_storage_mem_delete(g_parser_p->storage_p, gc_mem_link_p->managed_mem_p);
    free(gc_mem_link_p);
}

l2_gc_list *l2_gc_create() {
    l2_gc_list *gc_list_p = malloc(sizeof(l2_gc_list));
    gc_list_p->head_p = L2_NULL_PTR;
    gc_list_p->size = 0;
    return gc_list_p;
}

void l2_gc_destroy(l2_gc_list *gc_list_p) {
    gc_list_p->size = 0;
    l2_gc_free_gc_mem_link(gc_list_p->head_p);
    gc_list_p->head_p = L2_NULL_PTR;
    free(gc_list_p);
}

l2_gc_mem_node *l2_gc_get_mem_node(l2_gc_list *gc_list_p, void *managed_mem_p) {
    l2_gc_mem_node *gc_mem_node_p = L2_NULL_PTR;

    for (gc_mem_node_p = gc_list_p->head_p; gc_mem_node_p != L2_NULL_PTR; gc_mem_node_p = gc_mem_node_p->next) {
        if (gc_mem_node_p->managed_mem_p == managed_mem_p) {
            break;
        }
    }
    return gc_mem_node_p;
}

void l2_gc_increase_ref_count(l2_gc_list *gc_list_p, void *managed_mem_p) {
    l2_gc_mem_node *gc_mem_node_p = l2_gc_get_mem_node(gc_list_p, managed_mem_p);
    if (!gc_mem_node_p) l2_internal_error(L2_INTERNAL_ERROR_MEM_BLOCK_NOT_IN_GC, managed_mem_p);
    gc_mem_node_p->ref_count += 1;
}

void l2_gc_decrease_ref_count(l2_gc_list *gc_list_p, void *managed_mem_p) {
    l2_gc_mem_node *gc_mem_node_prev_p, *gc_mem_node_p, *gc_mem_node_next_p;

    if (gc_list_p->head_p) {
        /* handle the first */
        if (gc_list_p->head_p->managed_mem_p == managed_mem_p) {
            gc_list_p->head_p->ref_count -= 1;
            if (gc_list_p->head_p->ref_count <= 0) {
                l2_storage_mem_delete(g_parser_p->storage_p, gc_list_p->head_p->managed_mem_p);
                gc_mem_node_next_p = gc_list_p->head_p->next;
                free(gc_list_p->head_p);
                gc_list_p->head_p = gc_mem_node_next_p;
                gc_list_p->size -= 1;
            }
            return;
        }

        /* handle the last */
        gc_mem_node_prev_p = gc_list_p->head_p;
        for (gc_mem_node_p = gc_list_p->head_p->next; gc_mem_node_p != L2_NULL_PTR; gc_mem_node_prev_p = gc_mem_node_prev_p->next, gc_mem_node_p = gc_mem_node_p->next) {
            if (gc_mem_node_p->managed_mem_p == managed_mem_p) {
                gc_mem_node_p->ref_count -= 1;
                if (gc_mem_node_p->ref_count <= 0) {
                    gc_mem_node_next_p = gc_mem_node_p->next;
                    l2_storage_mem_delete(g_parser_p->storage_p, gc_mem_node_p->managed_mem_p);
                    free(gc_mem_node_p);
                    gc_mem_node_prev_p->next = gc_mem_node_next_p;
                    gc_list_p->size -= 1;
                }
                return;
            }
        }
    } else {
        l2_internal_error(L2_INTERNAL_ERROR_MEM_BLOCK_NOT_IN_GC, managed_mem_p);
    }
}

void l2_gc_append(l2_gc_list *gc_list_p, void *managed_mem_p) {
    l2_gc_mem_node *gc_mem_node_p;
    for (gc_mem_node_p = gc_list_p->head_p; gc_mem_node_p != L2_NULL_PTR; gc_mem_node_p = gc_mem_node_p->next);
    gc_mem_node_p = malloc(sizeof(l2_gc_mem_node));
    gc_mem_node_p->managed_mem_p = managed_mem_p;
    gc_mem_node_p->next = L2_NULL_PTR;
    gc_mem_node_p->ref_count = 1;
}

void l2_gc_check_and_collect(l2_gc_list *gc_list_p) {
    l2_gc_mem_node *gc_mem_node_prev_p, *gc_mem_node_p, *gc_mem_node_next_p;

    if (!gc_list_p->head_p) return;
    /* handle the first */
    while (gc_list_p->head_p->ref_count <= 0) {
        l2_storage_mem_delete(g_parser_p->storage_p, gc_list_p->head_p->managed_mem_p);
        gc_mem_node_next_p = gc_list_p->head_p->next;
        free(gc_list_p->head_p);
        gc_list_p->head_p = gc_mem_node_next_p;
        gc_list_p->size -= 1;
    }

    /* handle the last */
    gc_mem_node_prev_p = gc_list_p->head_p;
    for (gc_mem_node_p = gc_list_p->head_p->next; gc_mem_node_p != L2_NULL_PTR; gc_mem_node_prev_p = gc_mem_node_prev_p->next, gc_mem_node_p = gc_mem_node_p->next) {
        if (gc_mem_node_p->ref_count == 0) {
            gc_mem_node_next_p = gc_mem_node_p->next;
            l2_storage_mem_delete(g_parser_p->storage_p, gc_mem_node_p->managed_mem_p);
            free(gc_mem_node_p);
            gc_mem_node_prev_p->next = gc_mem_node_next_p;
            gc_list_p->size -= 1;

            /* relocate the current */
            gc_mem_node_p = gc_mem_node_next_p;
            continue;
        }
    }
}