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
 * | stmt stmts
 * | eof
 *
 * */
void l2_parse_stmts(l2_scope *scope_p) {
    l2_token *current_token_p;
    l2_scope *sub_scope_p;
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_TERMINATOR)) {
        return;

    } else {
        l2_parse_stmt(scope_p);
        l2_parse_stmts(scope_p);

    }
}

/* stmts ->
 * | stmt stmts
 * | eof
 *
 * */
void l2_absorb_stmts() {
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_TERMINATOR)) {
        return;

    } else {
        l2_absorb_stmt();
        l2_absorb_stmts();

    }
}

/* stmt ->
 * | { stmts }
 * | procedure id ( formal_param_list ) { stmts }
 * | while ( expr ) { stmts }
 * | do { stmts } while ( expr ) ;
 * | for ( expr ; expr ; expr ) { stmts }
 * | if ( expr ) { stmts } stmt_elsif
 * | var id ;
 * | var id = expr ;
 * | ;
 * | expr ;
 * */
void l2_absorb_stmt() {

}

/* stmt ->
 * | { stmts }
 * | procedure id ( formal_param_list ) { stmts }
 * | while ( expr ) { stmts }
 * | do { stmts } while ( expr ) ;
 * | for ( expr ; expr ; expr ) { stmts }
 * | if ( expr ) { stmts } stmt_elsif
 * | var id ;
 * | var id = expr ;
 * | ;
 * | expr ;
 * */
void l2_parse_stmt(l2_scope *scope_p) {

    l2_token *current_token_p;
    l2_expr_info right_expr_p;
    boolean symbol_added;
    l2_procedure procedure;

    l2_scope *sub_scope_p;

    _if_keyword (L2_KW_PROCEDURE) /* "procedure" */
    {
        _if_type (L2_TOKEN_IDENTIFIER) /* id */
        {
            _get_current_token_p
            procedure.entry_pos = l2_token_stream_get_pos(g_parser_p->token_stream_p);

            _if_type (L2_TOKEN_LP) /* ( */
            {
                /* todo */

            } _throw_unexpected_token

        } _throw_unexpected_token

    }
    _elif_type (L2_TOKEN_LBRACE) /* { */
    {
        /* while parse a sub stmts block, a new sub scope should be also created */
        sub_scope_p = l2_scope_create_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);

        l2_parse_stmts(sub_scope_p); /* stmts */

        _if_type (L2_TOKEN_RBRACE)
        {
            l2_scope_escape_scope(sub_scope_p);

        } _throw_missing_rbrace

    }
    _elif_keyword (L2_KW_IF) /* "if" */
    {
        _get_current_token_p

        _if_type (L2_TOKEN_LP) /* ( */
        {
            l2_expr_info expr_info;
            expr_info = l2_eval_expr(scope_p);

            if (expr_info.val_type != L2_EXPR_VAL_TYPE_BOOL)
                l2_parsing_error(L2_PARSING_ERROR_THE_EXPR_OF_IF_STMT_MUST_BE_A_BOOL_VALUE, current_token_p->current_line, current_token_p->current_col);

            _if_type (L2_TOKEN_RP) {
                /* ) */
            } _throw_missing_rp

            if (expr_info.val.bool) { /* if true */
                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    sub_scope_p = l2_scope_create_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);
                    l2_parse_stmts(sub_scope_p); /* parse stmts */

                    _if_type (L2_TOKEN_RBRACE) /* } */
                    {
                        l2_scope_escape_scope(sub_scope_p);
                        l2_absorb_stmt_elif();
                    } _throw_missing_rbrace

                } _throw_unexpected_token

            } else { /* if false */
                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    sub_scope_p = l2_scope_create_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);
                    l2_absorb_stmts(); /* parse stmts */

                    _if_type (L2_TOKEN_RBRACE) /* } */
                    {
                        l2_scope_escape_scope(sub_scope_p);
                        l2_parse_stmt_elif();
                    } _throw_missing_rbrace

                } _throw_unexpected_token
            }

        } _throw_unexpected_token

    }
    _elif_type (L2_TOKEN_SEMICOLON)
    {
        /* empty stmt which has only single ; */
    }
    _elif_keyword (L2_KW_VAR)
    {
        _if_type (L2_TOKEN_IDENTIFIER)
        {
            _get_current_token_p

            _if_type (L2_TOKEN_ASSIGN)
            {
                right_expr_p = l2_eval_expr(scope_p); /* expr */

                switch (right_expr_p.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        symbol_added = l2_symbol_table_add_symbol_integer(&scope_p->symbol_table_p, current_token_p->u.str.str_p, right_expr_p.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_REAL:
                        symbol_added = l2_symbol_table_add_symbol_real(&scope_p->symbol_table_p, current_token_p->u.str.str_p, right_expr_p.val.real);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        symbol_added = l2_symbol_table_add_symbol_bool(&scope_p->symbol_table_p, current_token_p->u.str.str_p, right_expr_p.val.bool);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error token type");
                }

                _if_type (L2_TOKEN_SEMICOLON)
                {
                    /* absorb ';' */
                } _throw_missing_semicolon

            }
            _elif_type (L2_TOKEN_SEMICOLON)
            {
                symbol_added = l2_symbol_table_add_symbol_without_initialization(&scope_p->symbol_table_p, current_token_p->u.str.str_p);

            } _throw_unexpected_token

            if (!symbol_added)
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_REDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);

        } _throw_unexpected_token

    }
    _else
    {
        l2_eval_expr(scope_p); /* expr */

        _if_type (L2_TOKEN_SEMICOLON)
        {
            /* absorb ';' */
        } _throw_missing_semicolon

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

