#include "string.h"
#include "l2_string.h"
#include "../l2_system/l2_assert.h"
#include "../l2_parser/l2_parse.h"

extern l2_parser *g_parser_p;

boolean l2_string_avail(const l2_string *str) {
    return str->str_p != L2_NULL_PTR;
}

void l2_string_create(l2_string *str) {
    l2_assert(str, L2_INTERNAL_ERROR_NULL_POINTER);
    str->len = 0;
    str->max_len = 100;
    str->str_p = l2_storage_mem_new(g_parser_p->storage_p, (str->max_len + 1) * sizeof(char));
}

void l2_string_destroy(l2_string *str) {
    l2_assert(str, L2_INTERNAL_ERROR_NULL_POINTER);
    str->len = 0;
    str->max_len = 0;
    l2_storage_mem_delete(g_parser_p->storage_p, str->str_p);
    str->str_p = L2_NULL_PTR;
}

void l2_string_push_char(l2_string *str, const char ch) {
    l2_assert(str, L2_INTERNAL_ERROR_NULL_POINTER);
    if (str->len >= str->max_len)
        str->str_p = l2_storage_mem_resize(g_parser_p->storage_p, str->str_p, (str->max_len + 1) * sizeof(char),
                                           str->max_len = ((((str->len - str->max_len) / 100 + 1) * 100 + 1) *
                                                           sizeof(char)));
    str->str_p[str->len++] = ch;
    str->str_p[str->len] = L2_STRING_END_MASK;
}

void l2_string_strcat_c(l2_string *dest, const char *src_str_p) {
    l2_assert(dest, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(src_str_p, L2_INTERNAL_ERROR_NULL_POINTER);
    char *p;
    for (p = (char *)src_str_p; *p != L2_STRING_END_MASK; p++) {
        l2_string_push_char(dest, *p);
    }
}

void l2_string_strcat(l2_string *dest, const l2_string *src) {
    l2_assert(dest, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(src, L2_INTERNAL_ERROR_NULL_POINTER);
    int i;
    for (i = 0; i < src->len; i++) {
        l2_string_push_char(dest, src->str_p[i]);
    }
}

void l2_string_strcpy_c(l2_string *dest, const char *src_str_p) {
    l2_assert(dest, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(src_str_p, L2_INTERNAL_ERROR_NULL_POINTER);
    dest->len = 0;
    l2_string_strcat_c(dest, src_str_p);
}

void l2_string_strcpy(l2_string *dest, const l2_string *src) {
    l2_assert(dest, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(src, L2_INTERNAL_ERROR_NULL_POINTER);
    dest->len = 0;
    l2_string_strcat(dest, src);
}

char *l2_string_get_str_p(const l2_string *str) {
    l2_assert(str, L2_INTERNAL_ERROR_NULL_POINTER);
    return str->str_p;
}

l2_string_size l2_string_len(const l2_string *str) {
    l2_assert(str, L2_INTERNAL_ERROR_NULL_POINTER);
    return str->len;
}

l2_string_size l2_string_max_len(const l2_string *str) {
    l2_assert(str, L2_INTERNAL_ERROR_NULL_POINTER);
    return str->max_len;
}

char l2_string_get_char(const l2_string *str, int i) {
    l2_assert(str, L2_INTERNAL_ERROR_NULL_POINTER);
    return str->str_p[i];
}

boolean l2_string_equal(const l2_string *str1, const l2_string *str2) {
    l2_assert(str1, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(str2, L2_INTERNAL_ERROR_NULL_POINTER);
    return str1->str_p == str2->str_p || strcmp(str1->str_p, str2->str_p) == 0 ? L2_TRUE : L2_FALSE;
}

boolean l2_string_equal_c(const l2_string *str, const char *src_str_p) {
    l2_assert(str, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(src_str_p, L2_INTERNAL_ERROR_NULL_POINTER);
    return str->str_p == src_str_p || strcmp(str->str_p, src_str_p) == 0 ? L2_TRUE : L2_FALSE;
}

