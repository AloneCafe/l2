#ifndef _L2_SYMBOL_TABLE_H_
#define _L2_SYMBOL_TABLE_H_

#include "../l2_base/l2_common_type.h"
#include "../l2_base/l2_vector.h"
#include "../l2_base/l2_string.h"
#include "l2_scope.h"

typedef enum _l2_symbol_type {
    L2_SYMBOL_UNINITIALIZED,
    /* L2_SYMBOL_TYPE_NULL, */

    /* symbol datatype */
    L2_SYMBOL_TYPE_INTEGER,
    L2_SYMBOL_TYPE_REAL,
    L2_SYMBOL_TYPE_BOOL,

    L2_SYMBOL_TYPE_NATIVE_POINTER

}l2_symbol_type;

typedef struct _l2_symbol {
    l2_symbol_type type;
    char *symbol_name;
    union {
        int integer;
        double real;
        boolean bool;
        /* native pointer */
    }u;
}l2_symbol;

typedef struct _l2_symbol_node {
    struct _l2_symbol_node *next;
    l2_symbol symbol;

}l2_symbol_node;

l2_symbol_node *l2_symbol_table_create();
void l2_symbol_table_destroy(l2_symbol_node *head_p);
l2_symbol_node *l2_symbol_table_get_symbol_node_by_name_in_symbol_table(l2_symbol_node *head_p, char *symbol_name);
l2_symbol_node *l2_symbol_table_get_symbol_node_by_name_in_scope(l2_scope *scope_p, char *symbol_name);
l2_symbol_node *l2_symbol_table_get_symbol_node_by_name_in_upper_scope(l2_scope *scope_p, char *symbol_name);


boolean l2_symbol_table_add_symbol_without_initialization(l2_symbol_node **head_p, char *symbol_name);
boolean l2_symbol_table_add_symbol_integer(l2_symbol_node **head_p, char *symbol_name, int integer);
boolean l2_symbol_table_add_symbol_real(l2_symbol_node **head_p, char *symbol_name, double real);
boolean l2_symbol_table_add_symbol_bool(l2_symbol_node **head_p, char *symbol_name, boolean bool);

#endif
