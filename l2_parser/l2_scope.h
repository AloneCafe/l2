#ifndef _L2_SCOPE_H_
#define _L2_SCOPE_H_

#include "../l2_base/l2_common_type.h"
#include "../l2_base/l2_vector.h"

typedef enum _l2_scope_create_flag {
    L2_SCOPE_CREATE_SUB_SCOPE,
    L2_SCOPE_CREATE_COORDINATE_SCOPE
}l2_scope_create_flag;

typedef struct _l2_scope {
    int level; /* begin with 0 */
    struct _l2_scope *upper_p;
    struct _l2_scope *coor_p;
    struct _l2_scope *lower_p;
    struct _l2_scope *guid; /* identify the unique scope on the whole, refer to address? */
    struct _l2_symbol_node *symbol_table_p; /* the symbol table in this scope */
}l2_scope, * l2_scope_guid, l2_scope_mirror;

l2_scope *l2_scope_create();
l2_scope_guid l2_scope_create_scope(l2_scope_guid src, l2_scope_create_flag cf);
void l2_scope_lower_finalize_recursion(l2_scope *scope_lower_p);
void l2_scope_coor_finalize_recursion(l2_scope *scope_coor_p);
void l2_scope_destroy(l2_scope *global_p);

/*
l2_scope_mirror *l2_scope_create_mirror(l2_scope *global_p);
void l2_scope_destroy_mirror(l2_scope_mirror *global_mirror_p);
*/

void l2_scope_escape_scope(l2_scope_guid src);

#endif
