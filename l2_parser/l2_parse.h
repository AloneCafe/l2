#ifndef _L2_PARSE_H_
#define _L2_PARSE_H_

#define L2_VERSION "v0.3.1" /* version info */

#include "../l2_base/l2_common_type.h"
#include "../l2_lexer/l2_token_stream.h"
#include "l2_scope.h"
#include "../l2_mem/l2_gc.h"
#include "l2_eval.h"
#include "l2_call_stack.h"

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


#define _repl \
if (g_parser_p->token_stream_p->char_stream_p->fp == stdin) { \
fflush(stdout); \
}

#define _repl_head \
if (g_parser_p->token_stream_p->char_stream_p->fp == stdin) { \
fprintf(stdout, "L2 编程语言及其解释器\n当前版本: %s ", L2_VERSION); \
fprintf(stdout, "L2 解释器命令行, REPL 用户界面\n"); \
fflush(stdout); \
}

/*
#define _repl
#define _repl_head
*/

typedef struct _l2_parser {
    l2_token_stream *token_stream_p;
    l2_scope *global_scope_p;
    l2_storage *storage_p;
    l2_gc_list *gc_list_p;
    l2_call_stack *call_stack_p;
    int braces_flag;
}l2_parser;

typedef enum _l2_stmt_interrupt_type {
    L2_STMT_NOT_STMT,
    L2_STMT_NO_INTERRUPT,
    L2_STMT_INTERRUPT_BREAK,
    L2_STMT_INTERRUPT_CONTINUE,
    L2_STMT_INTERRUPT_RETURN_WITH_VAL,
    L2_STMT_INTERRUPT_RETURN_WITHOUT_VAL
}l2_stmt_interrupt_type;

typedef struct _l2_stmt_interrupt {
    l2_stmt_interrupt_type type;
    int line_of_irt_stmt;
    int col_of_irt_stmt;
    union {
        l2_expr_info ret_expr_info;
    }u;
}l2_stmt_interrupt;

void l2_parse_initialize(FILE *fp);
void l2_parse_finalize();

boolean l2_parse_probe_next_token_by_type_and_str(l2_token_type type, char *str);
boolean l2_parse_probe_next_token_by_type(l2_token_type type);

void l2_parse();
l2_stmt_interrupt l2_parse_stmts(l2_scope *scope_p);
l2_stmt_interrupt l2_parse_stmt(l2_scope *scope_p);

void l2_parse_token_forward();
l2_token *l2_parse_token_current();
void l2_parse_token_back();
//int l2_parse_token_stream_get_pos();
//void l2_parse_token_stream_set_pos(int pos);

#endif