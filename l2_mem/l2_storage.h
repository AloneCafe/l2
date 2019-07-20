#ifndef _L2_MEM_H_
#define _L2_MEM_H_

#include "../l2_tpl/l2_common_type.h"

typedef struct _l2_mem_link {
    void *mem_p;
    struct _l2_mem_link *next;
}l2_mem_link;

typedef struct _l2_storage {
    l2_mem_link *head_p;
    int size;

}l2_storage;

l2_storage *l2_storage_create();
void l2_storage_destroy(l2_storage *storage_p);

void *l2_storage_mem_new(l2_storage *storage_p, l2_mem_size mem_size);
void *l2_storage_mem_new_with_zero(l2_storage *storage_p, l2_mem_size mem_size);
void *l2_storage_mem_renew(l2_storage *storage_p, void *old_void_ptr, l2_mem_size mem_renew_size);
void *l2_storage_mem_resize(l2_storage *storage_p, void *old_void_ptr, l2_mem_size mem_old_size, l2_mem_size mem_resize);
void l2_storage_mem_delete(l2_storage *storage_p, void *void_ptr);
void l2_storage_mem_copy(l2_storage *storage_p, void *dest_void_ptr, void *src_void_ptr, int size);

#endif