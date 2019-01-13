#include <stdio.h>
#include "l2_assert.h"
#include "l2_error.h"

void l2_assert_func(long val, l2_internal_error_type error_type, const char *func_str, const char *file_str, const int line) {
    char s[100];
    if (!val) {
        sprintf(s, "assertion failed, in function %s, file %s, line %d\n", func_str, file_str, line);
        l2_internal_error(error_type, s);
    }
}