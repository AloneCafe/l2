#include "memory.h"
#include "stdlib.h"
#include "string.h"
#include "l2_parse.h"
#include "../l2_lexer/l2_token_stream.h"
#include "l2_symbol_table.h"
#include "../l2_system/l2_error.h"
#include "../l2_system/l2_assert.h"
#include "l2_eval.h"

extern char *g_l2_token_keywords[];

l2_parser *g_parser_p;

void l2_parse_finalize() {
    l2_token_stream_destroy(g_parser_p->token_stream_p);
    l2_scope_destroy(g_parser_p->global_scope_p);
    l2_gc_destroy(g_parser_p->gc_list_p);
    l2_storage_destroy(g_parser_p->storage_p);
    free(g_parser_p);
}

void l2_parse_initialize(FILE *fp) {
    g_parser_p = malloc(sizeof(l2_parser));
    g_parser_p->storage_p = l2_storage_create();
    g_parser_p->gc_list_p = l2_gc_create();
    g_parser_p->global_scope_p = l2_scope_create();
    g_parser_p->token_stream_p = l2_token_stream_create(fp);
}

boolean l2_parse_probe_next_token_by_type_and_str(l2_token_type type, char *str) {
    l2_token *t = l2_token_stream_next_token(g_parser_p->token_stream_p);
    l2_token_stream_rollback(g_parser_p->token_stream_p);
    return t->type == type && l2_string_equal_c(&t->u.str, str);
}

boolean l2_parse_probe_next_token_by_type(l2_token_type type) {
    l2_token *t = l2_token_stream_next_token(g_parser_p->token_stream_p);
    l2_token_stream_rollback(g_parser_p->token_stream_p);
    return t->type == type;
}

void l2_parse() {
    l2_parse_stmts(g_parser_p->global_scope_p);
}

/* stmts ->
 * | if ( expr ) { stmts } stmt_elsif
 * | { stmts }
 * | stmt stmts
 * | eof
 *
 * */
void l2_parse_stmts(l2_scope *scope_p) {
    l2_token *token_current_p;
    l2_scope *sub_scope_p;
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LBRACE)) {
        l2_parse_token_forward();
        sub_scope_p = l2_scope_create_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);
        l2_parse_stmts(sub_scope_p);

        if (l2_parse_probe_next_token_by_type(L2_TOKEN_RBRACE)) {
            l2_parse_token_forward();
            l2_scope_escape_scope(sub_scope_p);

        } else {
            l2_parse_token_forward();
            token_current_p = l2_parse_token_current();
            l2_parsing_error(L2_PARSING_ERROR_MISSING_BLOCK_END_MARK, token_current_p->current_line, token_current_p->current_col);
        }
    } else if (l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_KEYWORD, g_l2_token_keywords[3])) { /* "if" */
        l2_parse_token_forward();
        token_current_p = l2_parse_token_current();

        if (l2_parse_probe_next_token_by_type(L2_TOKEN_LP)) { /* ( */
            l2_parse_token_forward();
            l2_expr_info expr_info;
            expr_info = l2_eval_expr(scope_p);
            if (expr_info.val_type != L2_EXPR_VAL_TYPE_BOOL)
                l2_parsing_error(L2_PARSING_ERROR_THE_EXPR_OF_IF_STMT_MUST_BE_A_BOOL_VALUE, token_current_p->current_line, token_current_p->current_col);

            if (l2_parse_probe_next_token_by_type(L2_TOKEN_RP)) { /* ) */
                l2_parse_token_forward();

            } else {
                l2_parse_token_forward();
                token_current_p = l2_parse_token_current();
                l2_parsing_error(L2_PARSING_ERROR_MISSING_BLOCK_END_MARK, token_current_p->current_line, token_current_p->current_col);
            }

            if (expr_info.val.bool) { /* if true */
                if (l2_parse_probe_next_token_by_type(L2_TOKEN_LBRACE)) { /* { */
                    l2_parse_token_forward();
                    sub_scope_p = l2_scope_create_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);
                    l2_parse_stmts(sub_scope_p); /* parse stmts */

                    if (l2_parse_probe_next_token_by_type(L2_TOKEN_RBRACE)) { /* } */
                        l2_parse_token_forward();
                        l2_scope_escape_scope(sub_scope_p);

                        l2_absorb_stmt_elif();

                    } else {
                        l2_parse_token_forward();
                        token_current_p = l2_parse_token_current();
                        l2_parsing_error(L2_PARSING_ERROR_MISSING_BLOCK_END_MARK, token_current_p->current_line, token_current_p->current_col);
                    }

                } else {
                    l2_parse_token_forward();
                    token_current_p = l2_parse_token_current();
                    l2_parsing_error(L2_PARSING_ERROR_UNEXPECTED_TOKEN, token_current_p->current_line, token_current_p->current_col, token_current_p);
                }


            } else { /* if false */
                if (l2_parse_probe_next_token_by_type(L2_TOKEN_LBRACE)) { /* { */
                    l2_parse_token_forward();
                    sub_scope_p = l2_scope_create_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);
                    l2_absorb_stmts(sub_scope_p); /* parse stmts */

                    if (l2_parse_probe_next_token_by_type(L2_TOKEN_RBRACE)) { /* } */
                        l2_parse_token_forward();
                        l2_scope_escape_scope(sub_scope_p);

                        l2_parse_stmt_elif();

                    } else {
                        l2_parse_token_forward();
                        token_current_p = l2_parse_token_current();
                        l2_parsing_error(L2_PARSING_ERROR_MISSING_BLOCK_END_MARK, token_current_p->current_line, token_current_p->current_col);
                    }

                } else {
                    l2_parse_token_forward();
                    token_current_p = l2_parse_token_current();
                    l2_parsing_error(L2_PARSING_ERROR_UNEXPECTED_TOKEN, token_current_p->current_line, token_current_p->current_col, token_current_p);
                }

            }



        }

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_TERMINATOR)) {
        return;

    } else {
        l2_parse_stmt(scope_p);
        l2_parse_stmts(scope_p);

    }
}


/* stmt ->
 * | var id ;
 * | var id = expr ;
 * | ;
 * | expr ;
 * */
void l2_parse_stmt(l2_scope *scope_p) {

    l2_token *token_current_p;
    l2_expr_info right_expr_p;
    boolean symbol_added;

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_SEMICOLON)) { /* empty stmt which has only single ; */
        l2_parse_token_forward();

    } else if (l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_KEYWORD, g_l2_token_keywords[2])) { /* var */
        l2_parse_token_forward();
        if (l2_parse_probe_next_token_by_type(L2_TOKEN_IDENTIFIER)) { /* id */
            l2_parse_token_forward();
            token_current_p = l2_parse_token_current();
            if (l2_parse_probe_next_token_by_type(L2_TOKEN_ASSIGN)) { /* = */
                l2_parse_token_forward();
                right_expr_p = l2_eval_expr(scope_p); /* expr */

                switch (right_expr_p.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        symbol_added = l2_symbol_table_add_symbol_integer(&scope_p->symbol_table_p, token_current_p->u.str.str_p, right_expr_p.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_REAL:
                        symbol_added = l2_symbol_table_add_symbol_real(&scope_p->symbol_table_p, token_current_p->u.str.str_p, right_expr_p.val.real);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        symbol_added = l2_symbol_table_add_symbol_bool(&scope_p->symbol_table_p, token_current_p->u.str.str_p, right_expr_p.val.bool);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error token type");
                }

                /* absorb ';' */
                if (l2_parse_probe_next_token_by_type(L2_TOKEN_SEMICOLON)) {
                    l2_parse_token_forward();

                } else {
                    l2_parse_token_forward();
                    token_current_p = l2_parse_token_current();
                    l2_parsing_error(L2_PARSING_ERROR_MISSING_SEMICOLON, token_current_p->current_line, token_current_p->current_col);
                }

            } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_SEMICOLON)) {
                l2_parse_token_forward();
                symbol_added = l2_symbol_table_add_symbol_without_initialization(&scope_p->symbol_table_p, token_current_p->u.str.str_p);

            } else {
                l2_parse_token_forward();
                token_current_p = l2_parse_token_current();
                l2_parsing_error(L2_PARSING_ERROR_UNEXPECTED_TOKEN, token_current_p->current_line, token_current_p->current_col, token_current_p);
            }

            if (!symbol_added)
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_REDEFINED, token_current_p->current_line, token_current_p->current_col, token_current_p->u.str.str_p);

        } else {
            l2_parse_token_forward();
            token_current_p = l2_parse_token_current();
            l2_parsing_error(L2_PARSING_ERROR_UNEXPECTED_TOKEN, token_current_p->current_line, token_current_p->current_col, token_current_p);
        }
    } else {
        l2_eval_expr(scope_p); /* expr */

        /* absorb ';' */
        if (l2_parse_probe_next_token_by_type(L2_TOKEN_SEMICOLON)) {
            l2_parse_token_forward();

        } else {
            l2_parse_token_forward();
            token_current_p = l2_parse_token_current();
            l2_parsing_error(L2_PARSING_ERROR_MISSING_SEMICOLON, token_current_p->current_line, token_current_p->current_col);
        }
        return;
    }
}

void l2_parse_token_forward() {
    l2_token_stream_next_token(g_parser_p->token_stream_p);
}

void l2_parse_token_back() {
    l2_token_stream_rollback(g_parser_p->token_stream_p);
}

l2_token *l2_parse_token_current() {
    return l2_token_stream_current_token(g_parser_p->token_stream_p);
}

