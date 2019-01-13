#ifndef _L2_ASSERT_H_
#define _L2_ASSERT_H_

#include <assert.h>

#include "../l2_base/l2_common_type.h"
#include "l2_error.h"

#define l2_sys_assert(x) (assert(x))
#define l2_assert(val, error_type) (l2_assert_func((long)(val), error_type, __func__, __FILE__, __LINE__))

void l2_assert_func(long val, l2_internal_error_type error_type, const char *func_str, const char *file_str, int line);

#endif