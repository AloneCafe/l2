#include "stdlib.h"
#include "memory.h"

#include "../l2_system/l2_assert.h"
#include "l2_storage.h"

void l2_storage_free_mem_link(l2_mem_link *mem_link_p) {
    if (mem_link_p == L2_NULL_PTR) return;
    free(mem_link_p->mem_p);
    l2_storage_free_mem_link(mem_link_p->next);
    free(mem_link_p);
}

void l2_storage_destroy(l2_storage *storage_p) {
    storage_p->size = 0;
    l2_storage_free_mem_link(storage_p->head_p);
    storage_p->head_p = L2_NULL_PTR;
}

l2_storage *l2_storage_create() {
    l2_storage *storage_p = malloc(sizeof(l2_storage));
    storage_p->size = 0;
    storage_p->head_p = L2_NULL_PTR;
    return storage_p;
}

void *l2_storage_mem_new(l2_storage *storage_p, l2_mem_size mem_size) {

    if (storage_p->head_p) {
        l2_mem_link *mem_link;
        for (mem_link = storage_p->head_p; mem_link->next != L2_NULL_PTR; mem_link = mem_link->next);
        mem_link->next = malloc(sizeof(l2_mem_link));
        mem_link->next->next = L2_NULL_PTR;
        l2_sys_assert(mem_link->next->mem_p = malloc(mem_size));
        storage_p->size += 1;
        return mem_link->next->mem_p;

    } else {
        storage_p->head_p = malloc(sizeof(l2_mem_link));
        storage_p->head_p->next = L2_NULL_PTR;
        l2_sys_assert(storage_p->head_p->mem_p = malloc(mem_size));
        storage_p->size += 1;
        return storage_p->head_p->mem_p;
    }
}

void *l2_storage_mem_new_with_zero(l2_storage *storage_p, l2_mem_size mem_size) {
    if (storage_p->head_p) {
        l2_mem_link *mem_link;
        for (mem_link = storage_p->head_p; mem_link->next != L2_NULL_PTR; mem_link = mem_link->next);
        mem_link->next = malloc(sizeof(l2_mem_link));
        mem_link->next->next = L2_NULL_PTR;
        l2_sys_assert(mem_link->next->mem_p = calloc(mem_size, 1));
        storage_p->size += 1;
        return mem_link->next->mem_p;

    } else {
        storage_p->head_p = malloc(sizeof(l2_mem_link));
        storage_p->head_p->next = L2_NULL_PTR;
        l2_sys_assert(storage_p->head_p->mem_p = calloc(mem_size, 1));
        storage_p->size += 1;
        return storage_p->head_p->mem_p;
    }
}

void *l2_storage_mem_renew(l2_storage *storage_p, void *old_void_ptr, l2_mem_size mem_renew_size) {
    if (storage_p->head_p == L2_NULL_PTR)
        l2_internal_error(L2_INTERNAL_ERROR_MEM_BLOCK_NOT_MANAGED, old_void_ptr);

    void *new_void_ptr = L2_NULL_PTR;
    l2_mem_link *mem_link;

    for (mem_link = storage_p->head_p; mem_link != L2_NULL_PTR; mem_link = mem_link->next) {
        if (old_void_ptr == mem_link->mem_p) {
            l2_sys_assert(new_void_ptr = realloc(old_void_ptr, mem_renew_size));
            mem_link->mem_p = new_void_ptr;
            return new_void_ptr;
        }
    }

    l2_internal_error(L2_INTERNAL_ERROR_MEM_BLOCK_NOT_MANAGED, old_void_ptr);
}

void *l2_storage_mem_resize(l2_storage *storage_p, void *old_void_ptr, l2_mem_size mem_old_size, l2_mem_size mem_resize) {
    if (storage_p->head_p == L2_NULL_PTR)
        l2_internal_error(L2_INTERNAL_ERROR_MEM_BLOCK_NOT_MANAGED, old_void_ptr);

    void *new_void_ptr = L2_NULL_PTR;
    void *temp_p;
    l2_mem_link *mem_link;

    for (mem_link = storage_p->head_p; mem_link != L2_NULL_PTR; mem_link = mem_link->next) {
        if (old_void_ptr == mem_link->mem_p) {
            temp_p = malloc(mem_old_size);
            memcpy(temp_p, mem_link->mem_p, mem_old_size > mem_resize ? mem_resize : mem_old_size);
            l2_sys_assert(new_void_ptr = realloc(old_void_ptr, mem_resize));
            memcpy(new_void_ptr, temp_p, mem_old_size > mem_resize ? mem_resize : mem_old_size);
            mem_link->mem_p = new_void_ptr;
            free(temp_p);
            return new_void_ptr;
        }
    }
    l2_internal_error(L2_INTERNAL_ERROR_MEM_BLOCK_NOT_MANAGED, old_void_ptr);
}

void l2_storage_mem_delete(l2_storage *storage_p, void *void_ptr) {

    l2_mem_link *prev_link, *mem_link, *next_link;
    if (storage_p->head_p == L2_NULL_PTR)
        l2_internal_error(L2_INTERNAL_ERROR_MEM_BLOCK_NOT_MANAGED, void_ptr);
    else if (void_ptr == storage_p->head_p->mem_p) {
        free(storage_p->head_p->mem_p);
        next_link = storage_p->head_p->next;
        free(storage_p->head_p);
        storage_p->head_p = next_link;
        storage_p->size -= 1;
        return;
    }

    prev_link = storage_p->head_p;
    for (mem_link = storage_p->head_p->next; mem_link != L2_NULL_PTR; prev_link = prev_link->next, mem_link = mem_link->next) {
        if (void_ptr == mem_link->mem_p) {
            free(mem_link->mem_p);
            next_link = mem_link->next;
            free(mem_link);
            prev_link->next = next_link;
            storage_p->size -= 1;
            return;
        }
    }

    l2_internal_error(L2_INTERNAL_ERROR_MEM_BLOCK_NOT_MANAGED, void_ptr);
}

void l2_storage_mem_copy(l2_storage *storage_p, void *dest_void_ptr, void *src_void_ptr, int size) {
    memcpy(dest_void_ptr, src_void_ptr, size);
}
