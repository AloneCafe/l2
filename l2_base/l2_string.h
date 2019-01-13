#ifndef _L2_STRING_H_
#define _L2_STRING_H_

#include "l2_common_type.h"
#include "../l2_mem/l2_storage.h"

typedef struct _l2_string {
    char *str_p;
    l2_string_size len;
    l2_string_size max_len;
}l2_string;

boolean l2_string_avail(const l2_string *str);
void l2_string_create(l2_string *str);
void l2_string_destroy(l2_string *str);
void l2_string_push_char(l2_string *str, char ch);
void l2_string_strcat_c(l2_string *dest, const char *src_str_p);
void l2_string_strcat(l2_string *dest, const l2_string *src);
void l2_string_strcpy_c(l2_string *dest, const char *src_str_p);
void l2_string_strcpy(l2_string *dest, const l2_string *src);
char *l2_string_get_str_p(const l2_string *str);
l2_string_size l2_string_len(const l2_string *str);
l2_string_size l2_string_max_len(const l2_string *str);
boolean l2_string_equal(const l2_string *str1, const l2_string *str2);
boolean l2_string_equal_c(const l2_string *str, const char *src_str_p);


#endif
