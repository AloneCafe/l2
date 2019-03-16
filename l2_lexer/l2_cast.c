#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#include "l2_cast.h"
#include "../l2_system/l2_assert.h"
#include "../l2_base/l2_vector.h"
#include "../l2_base/l2_stack.h"

int l2_cast_hex_str_to_int(const char *s) {
    l2_assert(s, L2_INTERNAL_ERROR_NULL_POINTER);
    const char *p;
    int ans = 0, n = strlen(s), digit = 0, factor;
    for (p = s; *p != '\0'; p++) {
        l2_pow(factor, 8, n - 1);
        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'f') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'F') {
            digit = *p - 'A' + 10;
        }
        ans += factor * digit;
        n -= 1;
    }
    return ans;
}

int l2_cast_octal_str_to_int(const char *s) {
    l2_assert(s, L2_INTERNAL_ERROR_NULL_POINTER);
    const char *p;
    int ans = 0, n = strlen(s), factor;
    for (p = s; *p != '\0'; p++) {
        l2_pow(factor, 8, n - 1);
        ans += factor * (*p - '0');
        n -= 1;
    }
    return ans;
}

int l2_cast_decimal_str_to_int(const char *s) {
    l2_assert(s, L2_INTERNAL_ERROR_NULL_POINTER);
    return strtol(s, L2_NULL_PTR, 10);
}

double l2_cast_real_str_to_int(const char *s) {
    l2_assert(s, L2_INTERNAL_ERROR_NULL_POINTER);
    return strtod(s, L2_NULL_PTR);
}

void l2_cast_decimal_to_str(int dec_num, char *s) {
    l2_assert(s, L2_INTERNAL_ERROR_NULL_POINTER);
    sprintf(s, "%d", dec_num);
}

void l2_cast_real_to_str(double real_num, char *s) {
    l2_assert(s, L2_INTERNAL_ERROR_NULL_POINTER);
    sprintf(s, "%lf", real_num);
}

/* the stack must be destroyed by caller in the end */
l2_stack l2_cast_vector_to_stack(l2_vector *vec_p) {
    l2_stack stack;
    l2_stack_create(&stack, vec_p->single_size);
    int i;
    for (i = 0; i < vec_p->size; i++) {
        l2_stack_push_back(&stack, l2_vector_at(vec_p, i));
    }
    return stack;
}