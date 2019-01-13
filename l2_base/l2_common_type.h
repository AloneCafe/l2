#ifndef _L2_COMMON_TYPE_H_
#define _L2_COMMON_TYPE_H_

#include <stdint.h>

#define L2_NULL_PTR ((void *)0)
#define L2_STRING_END_MASK 0
#define L2_TRUE ((boolean)1)
#define L2_FALSE ((boolean)0)
#define L2_EOF (-1)

#define l2_char_is_blank(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')

typedef unsigned int l2_mem_size;
typedef unsigned int l2_string_size;
typedef unsigned int l2_stack_size;
typedef unsigned int l2_vector_size;

typedef char boolean;



#endif
