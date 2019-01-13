#ifndef _L2_PARSE_H_
#define _L2_PARSE_H_

#include "../l2_base/l2_common_type.h"
#include "../l2_lexer/l2_token_stream.h"
#include "l2_scope.h"
#include "../l2_mem/l2_gc.h"

typedef struct _l2_parser {
    l2_token_stream *token_stream_p;
    l2_scope *global_scope_p;
    l2_storage *storage_p;
    l2_gc_list *gc_list_p;
}l2_parser;

void l2_parse_initialize(FILE *fp);
void l2_parse_finalize();

boolean l2_parse_probe_next_token_by_type_and_str(l2_token_type type, char *str);
boolean l2_parse_probe_next_token_by_type(l2_token_type type);

void l2_parse();
void l2_parse_stmts(l2_scope *scope_p);
void l2_parse_stmt(l2_scope *scope_p);

void l2_parse_token_forward();
l2_token *l2_parse_token_current();
void l2_parse_token_back();
int l2_parse_token_stream_get_pos();
void l2_parse_token_stream_set_pos(int pos);

#endif