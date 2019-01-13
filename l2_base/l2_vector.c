#include "l2_vector.h"
#include "../l2_system/l2_assert.h"
#include "../l2_parser/l2_parse.h"

extern l2_parser *g_parser_p;

void l2_vector_create(l2_vector *vec, l2_mem_size size_of_single_elem) {
    l2_assert(vec, L2_INTERNAL_ERROR_NULL_POINTER);
    vec->size = 0;
    vec->max_size = 100;
    vec->vector_p = l2_storage_mem_new(g_parser_p->storage_p, size_of_single_elem * vec->max_size);
    vec->single_size = size_of_single_elem;
}

void l2_vector_destroy(l2_vector *vec) {
    l2_assert(vec, L2_INTERNAL_ERROR_NULL_POINTER);
    vec->size = 0;
    vec->max_size = 0;
    l2_storage_mem_delete(g_parser_p->storage_p, vec->vector_p);
    vec->vector_p = L2_NULL_PTR;
}

void l2_vector_append(l2_vector *vec, const void *data) {
    l2_assert(vec, L2_INTERNAL_ERROR_NULL_POINTER);
    int i;
    if (vec->size >= vec->max_size)
        vec->vector_p = l2_storage_mem_resize(g_parser_p->storage_p, vec->vector_p, vec->max_size * vec->single_size, vec->max_size =
                ((vec->size - vec->max_size) / 100 + 1) * 100 * vec->single_size);
    for (i = 0; i < vec->single_size; i++) {
        ((char *)(vec->vector_p))[vec->size * vec->single_size + i] = ((char *)data)[i];
    }
    vec->size += 1;
}

void *l2_vector_tail(const l2_vector *vec) {
    l2_assert(vec, L2_INTERNAL_ERROR_NULL_POINTER);
    return (char *)vec->vector_p + ((vec->size - 1) * vec->single_size);
}

void *l2_vector_at(const l2_vector *vec, int pos) {
    l2_assert(vec, L2_INTERNAL_ERROR_NULL_POINTER);
    return (char *)vec->vector_p + (pos * vec->single_size);
}


