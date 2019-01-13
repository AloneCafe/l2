#include "l2_scope.h"
#include "../l2_system/l2_error.h"
#include "../l2_system/l2_assert.h"
#include "l2_parse.h"
#include "l2_symbol_table.h"

extern l2_parser *g_parser_p;

l2_scope *l2_scope_create() {
    l2_scope *global_p;
    global_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_scope));
    l2_vector_create(&global_p->lower_p_vec, sizeof(l2_scope_guid));
    global_p->guid = global_p;
    global_p->level = 0;
    global_p->upper_p = L2_NULL_PTR;
    global_p->symbol_table_p = l2_symbol_table_create();
    return global_p;
}

l2_scope_guid l2_scope_create_scope(l2_scope_guid src, l2_scope_create_flag cf) {
    l2_scope *scope_p = L2_NULL_PTR;
    switch(cf) {
        case L2_SCOPE_CREATE_SUB_SCOPE:
            scope_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_scope));
            l2_vector_create(&scope_p->lower_p_vec, sizeof(l2_scope_guid));
            scope_p->guid = scope_p;
            scope_p->level = src->level + 1;
            scope_p->upper_p = src;
            l2_vector_append(&src->lower_p_vec, &scope_p);
            break;

        case L2_SCOPE_CREATE_COORDINATE_SCOPE:
            if (!src->upper_p)
                l2_internal_error(L2_INTERNAL_ERROR_ILLEGAL_OPERATION, "can not create coordinate scope after global scope");
            scope_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_scope));
            l2_vector_create(&scope_p->lower_p_vec, sizeof(l2_scope_guid));
            scope_p->guid = scope_p;
            scope_p->level = src->upper_p->level + 1;
            scope_p->upper_p = src->upper_p;
            l2_vector_append(&src->upper_p->lower_p_vec, &scope_p);
            break;

        default:
            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "run into unreachable case");
    }

    return scope_p;
}

l2_scope_guid l2_scope_get_lower_scope_by_pos(l2_scope *link_p, int pos) {
    l2_assert(link_p, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(pos >= 0 && pos < link_p->lower_p_vec.size, L2_INTERNAL_ERROR_OUT_OF_RANGE);
    return *(l2_scope_guid *)l2_vector_at(&link_p->lower_p_vec, pos);
}

void l2_scope_link_finalize_recursion(l2_vector *scope_link_vec_p) {
    if (scope_link_vec_p->size <= 0) {
        //l2_vector_destroy(scope_link_vec_p);
        return;
    }

    l2_scope *scope_p;
    int i;
    for (i = 0; i < scope_link_vec_p->size; i++) {
        scope_p = *(l2_scope_guid *)l2_vector_at(scope_link_vec_p, i);
        l2_scope_link_finalize_recursion(&scope_p->lower_p_vec);
        l2_vector_destroy(&scope_p->lower_p_vec);
        l2_symbol_table_destroy(scope_p->symbol_table_p);
        l2_storage_mem_delete(g_parser_p->storage_p, scope_p);
    }

}

void l2_scope_destroy(l2_scope *global_p) {
    l2_assert(global_p, L2_INTERNAL_ERROR_NULL_POINTER);

    l2_scope_link_finalize_recursion(&global_p->lower_p_vec);
    l2_vector_destroy(&global_p->lower_p_vec);
    l2_symbol_table_destroy(global_p->symbol_table_p);
    l2_storage_mem_delete(g_parser_p->storage_p, global_p);
}




