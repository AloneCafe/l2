#include "stdlib.h"
#include "stdio.h"
#include "l2_char_stream.h"

#include "../l2_base/l2_common_type.h"
#include "../l2_system/l2_assert.h"
#include "../l2_base/l2_vector.h"

l2_char_stream *l2_char_stream_create(FILE *fp) {
    l2_assert(fp, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_char_stream *char_stream_p = malloc(sizeof(l2_char_stream));
    char_stream_p->fp = fp;
    char_stream_p->cols = 0;
    char_stream_p->lines = 1;
    l2_vector_create(&char_stream_p->chars_vector, sizeof(char));
    char_stream_p->chars_vector_current_pos = 0;
    return char_stream_p;
}

void l2_char_stream_destroy(l2_char_stream *char_stream_p) {
    l2_assert(char_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_vector_destroy(&char_stream_p->chars_vector);
    free(char_stream_p);
}

char l2_char_stream_next_char(l2_char_stream *char_stream_p) {
    l2_assert(char_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);

    if (char_stream_p->chars_vector_current_pos < char_stream_p->chars_vector.size) {
        char_stream_p->chars_vector_current_pos += 1;
        return *(char *)l2_vector_at(&char_stream_p->chars_vector, char_stream_p->chars_vector_current_pos - 1);
    }

    char ch;
    if(!fread(&ch, 1, 1, char_stream_p->fp)) {
        return L2_EOF;
    }

    if (ch == '\n') {
        char_stream_p->lines += 1;
        char_stream_p->cols = 0;
    } else {
        char_stream_p->cols += 1;
    }

    l2_vector_append(&char_stream_p->chars_vector, &ch);
    char_stream_p->chars_vector_current_pos += 1;
    return ch;
}

void l2_char_stream_rollback(l2_char_stream *char_stream_p) {
    l2_assert(char_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(char_stream_p->chars_vector_current_pos > 0, L2_INTERNAL_ERROR_OUT_OF_RANGE);
    char_stream_p->chars_vector_current_pos -= 1;
}

int l2_char_stream_lines(const l2_char_stream *char_stream_p) {
    l2_assert(char_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    return char_stream_p->lines;
}

int l2_char_stream_cols(const l2_char_stream *char_stream_p) {
    l2_assert(char_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    return char_stream_p->cols;
}


