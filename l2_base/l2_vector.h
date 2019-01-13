#ifndef _L2_VECTOR_H_
#define _L2_VECTOR_H_

#include "../l2_mem/l2_storage.h"
#include "l2_common_type.h"

typedef struct _l2_vector {
    void *vector_p;
    l2_vector_size size;
    l2_vector_size max_size;
    l2_mem_size single_size;
}l2_vector;

void l2_vector_create(l2_vector *vec, l2_mem_size size_of_single_elem);
void l2_vector_destroy(l2_vector *vec);
void l2_vector_append(l2_vector *vec, const void *data);
void* l2_vector_tail(const l2_vector *vec);
void* l2_vector_at(const l2_vector *vec, int pos);

#endif