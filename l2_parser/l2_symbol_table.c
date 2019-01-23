#include "string.h"
#include "l2_symbol_table.h"
#include "../l2_system/l2_error.h"
#include "../l2_system/l2_assert.h"
#include "l2_scope.h"
#include "l2_parse.h"

extern l2_parser *g_parser_p;

l2_symbol_node *l2_symbol_table_create() {
    return L2_NULL_PTR;
}

l2_symbol_node *l2_symbol_table_get_symbol_node_by_name_in_symbol_table(l2_symbol_node *head_p, char *symbol_name) {
    if (!head_p) return L2_NULL_PTR;
    if (strcmp(symbol_name, head_p->symbol.symbol_name) == 0) {
        return head_p;
    } else {
        return l2_symbol_table_get_symbol_node_by_name_in_symbol_table(head_p->next, symbol_name);
    }
}

l2_symbol_node *l2_symbol_table_get_symbol_node_by_name_in_scope(l2_scope *scope_p, char *symbol_name) {
    return l2_symbol_table_get_symbol_node_by_name_in_symbol_table(scope_p->symbol_table_p, symbol_name);
}


l2_symbol_node *l2_symbol_table_get_symbol_node_by_name_in_upper_scope(l2_scope *scope_p, char *symbol_name) {

    l2_symbol_node *current_p = L2_NULL_PTR;
    if (!scope_p) return current_p;
    if ((current_p = l2_symbol_table_get_symbol_node_by_name_in_scope(scope_p, symbol_name)) == L2_NULL_PTR) {
        return l2_symbol_table_get_symbol_node_by_name_in_upper_scope(scope_p->upper_p, symbol_name);
    } else {
        return current_p;
    }
}

boolean l2_symbol_table_add_symbol_without_initialization(l2_symbol_node **head_p, char *symbol_name) {

    /* judge the symbol if already defined before */
    if (l2_symbol_table_get_symbol_node_by_name_in_symbol_table(*head_p, symbol_name) != L2_NULL_PTR) return L2_FALSE;
    l2_symbol_node *current_p = *head_p;

    if (*head_p == L2_NULL_PTR) {
        *head_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_symbol_node));
        (*head_p)->next = L2_NULL_PTR;
        (*head_p)->symbol.symbol_name = symbol_name;
        (*head_p)->symbol.type = L2_SYMBOL_UNINITIALIZED;
        return L2_TRUE;
    }

    while (current_p->next) {
        current_p = current_p->next;
    }
    current_p->next = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_symbol_node));
    current_p->next->next = L2_NULL_PTR;
    current_p->next->symbol.symbol_name = symbol_name;
    current_p->next->symbol.type = L2_SYMBOL_UNINITIALIZED;
    return L2_TRUE;
}

boolean l2_symbol_table_add_symbol_integer(l2_symbol_node **head_p, char *symbol_name, int integer) {

    /* judge the symbol if already defined before */
    if (l2_symbol_table_get_symbol_node_by_name_in_symbol_table(*head_p, symbol_name) != L2_NULL_PTR) return L2_FALSE;
    l2_symbol_node *current_p = *head_p;

    if (*head_p == L2_NULL_PTR) {
        *head_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_symbol_node));
        (*head_p)->next = L2_NULL_PTR;
        (*head_p)->symbol.symbol_name = symbol_name;
        (*head_p)->symbol.type = L2_SYMBOL_TYPE_INTEGER;
        (*head_p)->symbol.u.integer = integer;
        return L2_TRUE;
    }

    while (current_p->next) {
        current_p = current_p->next;
    }
    current_p->next = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_symbol_node));
    current_p->next->next = L2_NULL_PTR;
    current_p->next->symbol.symbol_name = symbol_name;
    current_p->next->symbol.type = L2_SYMBOL_TYPE_INTEGER;
    current_p->next->symbol.u.integer = integer;
    return L2_TRUE;
}

boolean l2_symbol_table_add_symbol_real(l2_symbol_node **head_p, char *symbol_name, double real) {

    /* judge the symbol if already defined before */
    if (l2_symbol_table_get_symbol_node_by_name_in_symbol_table(*head_p, symbol_name) != L2_NULL_PTR) return L2_FALSE;
    l2_symbol_node *current_p = *head_p;

    if (*head_p == L2_NULL_PTR) {
        *head_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_symbol_node));
        (*head_p)->next = L2_NULL_PTR;
        (*head_p)->symbol.symbol_name = symbol_name;
        (*head_p)->symbol.type = L2_SYMBOL_TYPE_REAL;
        (*head_p)->symbol.u.real = real;
        return L2_TRUE;
    }

    while (current_p->next) {
        current_p = current_p->next;
    }
    current_p->next = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_symbol_node));
    current_p->next->next = L2_NULL_PTR;
    current_p->next->symbol.symbol_name = symbol_name;
    current_p->next->symbol.type = L2_SYMBOL_TYPE_REAL;
    current_p->next->symbol.u.real = real;
    return L2_TRUE;
}

boolean l2_symbol_table_add_symbol_bool(l2_symbol_node **head_p, char *symbol_name, boolean bool) {

    /* judge the symbol if already defined before */
    if (l2_symbol_table_get_symbol_node_by_name_in_symbol_table(*head_p, symbol_name) != L2_NULL_PTR) return L2_FALSE;
    l2_symbol_node *current_p = *head_p;

    if (*head_p == L2_NULL_PTR) {
        *head_p = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_symbol_node));
        (*head_p)->next = L2_NULL_PTR;
        (*head_p)->symbol.symbol_name = symbol_name;
        (*head_p)->symbol.type = L2_SYMBOL_TYPE_BOOL;
        (*head_p)->symbol.u.bool = bool;
        return L2_TRUE;
    }

    while (current_p->next) {
        current_p = current_p->next;
    }
    current_p->next = l2_storage_mem_new_with_zero(g_parser_p->storage_p, sizeof(l2_symbol_node));
    current_p->next->next = L2_NULL_PTR;
    current_p->next->symbol.symbol_name = symbol_name;
    current_p->next->symbol.type = L2_SYMBOL_TYPE_BOOL;
    current_p->next->symbol.u.bool = bool;
    return L2_TRUE;
}

void l2_symbol_table_destroy(l2_symbol_node *head_p) {
    if (!head_p) return;
    l2_symbol_table_destroy(head_p->next);
    l2_storage_mem_delete(g_parser_p->storage_p, head_p);
}