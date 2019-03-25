#include "l2_scope.h"
#include "../l2_system/l2_error.h"
#include "../l2_system/l2_assert.h"
#include "l2_parse.h"
#include "l2_symbol_table.h"

extern l2_parser *g_parser_p;

l2_scope *l2_scope_create() {
    l2_scope *global_p;
    global_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_scope));
    global_p->guid = global_p;
    global_p->level = 0;
    global_p->upper_p = L2_NULL_PTR;
    global_p->lower_p = L2_NULL_PTR;
    global_p->symbol_table_p = l2_symbol_table_create();
    return global_p;
}

l2_scope_guid l2_scope_create_scope(l2_scope_guid src, l2_scope_create_flag cf, l2_scope_type scope_type) {
    l2_scope *scope_p = L2_NULL_PTR;
    switch(cf) {
        case L2_SCOPE_CREATE_SUB_SCOPE:
            if (src->lower_p) {
                scope_p = src->lower_p;
                while (scope_p->coor_p) scope_p = scope_p->coor_p;

                scope_p->coor_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_scope));
                scope_p->coor_p->guid = scope_p->coor_p;
                scope_p->coor_p->level = src->level + 1;
                scope_p->coor_p->upper_p = src;
                scope_p->coor_p->lower_p = L2_NULL_PTR;
                scope_p->coor_p->scope_type = scope_type;
                scope_p->coor_p->symbol_table_p = l2_symbol_table_create();
                return scope_p->coor_p;

            } else {
                src->lower_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_scope));
                src->lower_p->guid = src->lower_p;
                src->lower_p->level = src->level + 1;
                src->lower_p->upper_p = src;
                src->lower_p->lower_p = L2_NULL_PTR;
                src->lower_p->scope_type = scope_type;
                src->lower_p->symbol_table_p = l2_symbol_table_create();
                return src->lower_p;
            }

        case L2_SCOPE_CREATE_COORDINATE_SCOPE:
            if (!src->upper_p)
                l2_internal_error(L2_INTERNAL_ERROR_ILLEGAL_OPERATION, "can not create coordinate scope after global scope");

            scope_p = src;
            while (scope_p->coor_p) scope_p = scope_p->coor_p;

            scope_p->coor_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_scope));
            scope_p->coor_p->guid = scope_p->coor_p;
            scope_p->coor_p->level = src->level;
            scope_p->coor_p->upper_p = src->upper_p;
            scope_p->coor_p->lower_p = L2_NULL_PTR;
            scope_p->coor_p->scope_type = scope_type;
            scope_p->coor_p->symbol_table_p = l2_symbol_table_create();
            return scope_p->coor_p;

        default:
            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "run into unreachable case");
    }
}

void l2_scope_coor_finalize_recursion(l2_scope *scope_coor_p) {
    if (!scope_coor_p) return;
    l2_scope_coor_finalize_recursion(scope_coor_p->coor_p);
    l2_scope_lower_finalize_recursion(scope_coor_p->lower_p);

    l2_symbol_table_destroy(scope_coor_p->symbol_table_p);
    l2_storage_mem_delete(g_parser_p->storage_p, scope_coor_p);
}

void l2_scope_lower_finalize_recursion(l2_scope *scope_lower_p) {
    if (!scope_lower_p) return;
    l2_scope_lower_finalize_recursion(scope_lower_p->lower_p);
    l2_scope_coor_finalize_recursion(scope_lower_p->coor_p);

    l2_symbol_table_destroy(scope_lower_p->symbol_table_p);
    l2_storage_mem_delete(g_parser_p->storage_p, scope_lower_p);
}

void l2_scope_destroy(l2_scope *global_p) {
    l2_assert(global_p, L2_INTERNAL_ERROR_NULL_POINTER);

    l2_scope_lower_finalize_recursion(global_p->lower_p);
    l2_symbol_table_destroy(global_p->symbol_table_p);
    l2_storage_mem_delete(g_parser_p->storage_p, global_p);
}

/* when program escape a scope, the symbol table of this scope should be destroyed */
void l2_scope_escape_scope(l2_scope_guid src) {
    l2_assert(src, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_scope_lower_finalize_recursion(src->lower_p);
    l2_symbol_table_destroy(src->symbol_table_p);

    l2_scope *scope_upper_p = src->upper_p;
    if (!scope_upper_p) { /* global scope */
        l2_scope_destroy(src);
        return;
    }

    l2_scope *scope_p;
    if (scope_upper_p->lower_p == src) {
        scope_upper_p->lower_p = src->coor_p;

    } else {
        scope_p = scope_upper_p->lower_p;
        while (scope_p->coor_p != src) scope_p = scope_p->coor_p;
        scope_p->coor_p = src->coor_p;
    }

    l2_storage_mem_delete(g_parser_p->storage_p, src);
}

l2_scope_guid l2_scope_create_common_scope(l2_scope_guid src, l2_scope_create_flag cf) {
    return l2_scope_create_scope(src, cf, L2_SCOPE_TYPE_COMMON);
}

l2_scope_guid l2_scope_create_for_scope(l2_scope_guid src, l2_scope_create_flag cf, int loop_entry_pos) {
    l2_scope_guid res_guid = l2_scope_create_scope(src, cf, L2_SCOPE_TYPE_FOR);
    res_guid->u.loop_entry_pos = loop_entry_pos;
    return res_guid;
}

l2_scope_guid l2_scope_create_while_scope(l2_scope_guid src, l2_scope_create_flag cf, int loop_entry_pos) {
    l2_scope_guid res_guid = l2_scope_create_scope(src, cf, L2_SCOPE_TYPE_WHILE);
    res_guid->u.loop_entry_pos = loop_entry_pos;
    return res_guid;
}

l2_scope_guid l2_scope_create_do_while_scope(l2_scope_guid src, l2_scope_create_flag cf, int loop_entry_pos) {
    l2_scope_guid res_guid = l2_scope_create_scope(src, cf, L2_SCOPE_TYPE_DO_WHILE);
    res_guid->u.loop_entry_pos = loop_entry_pos;
    return res_guid;
}

l2_scope_guid l2_scope_create_procedure_scope(l2_scope_guid src, l2_scope_create_flag cf) {
    l2_scope_guid res_guid = l2_scope_create_scope(src, cf, L2_SCOPE_TYPE_PROCEDURE);
    return res_guid;
}

l2_scope_guid l2_scope_find_nearest_scope_by_type(l2_scope_guid current_scope, l2_scope_type scope_type) {
    l2_scope_guid scope;
    for (scope = current_scope; scope != L2_NULL_PTR; scope = scope->upper_p) {
        if (scope->scope_type == scope_type) return scope;
    }
    return L2_NULL_PTR;
}

l2_scope_guid l2_scope_find_nearest_loop_scope(l2_scope_guid current_scope) {
    l2_scope_guid scope;
    for (scope = current_scope; scope != L2_NULL_PTR; scope = scope->upper_p) {
        if (scope->scope_type == L2_SCOPE_TYPE_FOR || scope->scope_type == L2_SCOPE_TYPE_DO_WHILE || scope->scope_type == L2_SCOPE_TYPE_WHILE)
            return scope;
    }
    return L2_NULL_PTR;
}