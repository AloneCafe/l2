#ifndef _L2_CAST_H_
#define _L2_CAST_H_

#include "../l2_tpl/l2_common_type.h"
#include "../l2_tpl/l2_vector.h"
#include "../l2_tpl/l2_stack.h"

#define l2_pow(r, a, n) { int i, ans = 1; for (i = 0; i < (n); i++) ans *= (a); (r) = ans; }

int l2_cast_octal_str_to_int(const char *s);
int l2_cast_hex_str_to_int(const char *s);
int l2_cast_decimal_str_to_int(const char *s);
double l2_cast_real_str_to_int(const char *s);
void l2_cast_decimal_to_str(int dec_num, char *s);
void l2_cast_real_to_str(double real_num, char *s);

l2_stack l2_cast_vector_to_stack(l2_vector *vec_p);

#endif
