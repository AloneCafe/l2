#ifndef _L2_CHAR_STREAM_H_
#define _L2_CHAR_STREAM_H_

#include <stdio.h>

#include "../l2_tpl/l2_common_type.h"
#include "../l2_tpl/l2_vector.h"

typedef struct _l2_char_stream {
    FILE *fp;
    int lines;
    int cols;
    l2_vector chars_vector;
    int chars_vector_current_pos;
}l2_char_stream;

l2_char_stream *l2_char_stream_create(FILE *fp);
void l2_char_stream_destroy(l2_char_stream *char_stream_p);

char l2_char_stream_next_char(l2_char_stream *char_stream_p);
void l2_char_stream_rollback(l2_char_stream *char_stream_p);
int l2_char_stream_lines(const l2_char_stream *char_stream_p);
int l2_char_stream_cols(const l2_char_stream *char_stream_p);



#endif