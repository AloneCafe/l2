#ifndef _L2_PARSE_H_
#define _L2_PARSE_H_

#include "../l2_base/l2_common_type.h"
#include "../l2_lexer/l2_token_stream.h"
#include "l2_scope.h"
#include "../l2_mem/l2_gc.h"

#define token_keyword

#define _if_keyword(kw) \
if (l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_KEYWORD, g_l2_token_keywords[(kw)])) { l2_parse_token_forward(); \

#define _elif_keyword(kw) \
} else if (l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_KEYWORD, g_l2_token_keywords[(kw)])) { l2_parse_token_forward(); \

#define _if_id(c_str) \
if (l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_IDENTIFIER, (c_str))) { l2_parse_token_forward(); \

#define _elif_id(c_str) \
} else if (l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_IDENTIFIER, (c_str))) { l2_parse_token_forward(); \

#define _if_type(type) \
if (l2_parse_probe_next_token_by_type((type))) { l2_parse_token_forward(); \

#define _elif_type(type) \
} else if (l2_parse_probe_next_token_by_type((type))) { l2_parse_token_forward();

#define _throw_unexpected_token _throw (L2_PARSING_ERROR_UNEXPECTED_TOKEN, __token_current_p)
#define _throw_missing_rp _throw (L2_PARSING_ERROR_MISSING_RP)
#define _throw_missing_semicolon _throw (L2_PARSING_ERROR_MISSING_SEMICOLON)
#define _throw_missing_rbrace _throw (L2_PARSING_ERROR_MISSING_RBRACE)
#define _throw_missing_colon _throw (L2_PARSING_ERROR_MISSING_COLON)

#define _declr_current_token_p l2_token *current_token_p;
#define _get_current_token_p current_token_p = l2_parse_token_current();

#define _throw_expr_not_bool \
} else l2_parsing_error(L2_PARSING_ERROR_EXPR_NOT_BOOL, current_token_p->current_line, current_token_p->current_col);

#define _throw_right_expr_not_bool(c_str) \
} else l2_parsing_error(L2_PARSING_ERROR_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE, current_token_p->current_line, current_token_p->current_col, c_str);

#define _throw_left_expr_not_bool(c_str) \
} else l2_parsing_error(L2_PARSING_ERROR_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE, current_token_p->current_line, current_token_p->current_col, c_str);

#define _throw(parsing_err_type, ...) \
} else { \
l2_parse_token_forward(); \
l2_token *__token_current_p = l2_parse_token_current(); \
l2_parsing_error((parsing_err_type), __token_current_p->current_line, __token_current_p->current_col, ##__VA_ARGS__); \
}

#define _else \
} else

#define _end \
}

#define _if(cond) \
if ((cond)) {

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