#include "memory.h"
#include "stdlib.h"
#include "string.h"
#include "l2_parse.h"
#include "../l2_lexer/l2_token_stream.h"
#include "l2_symbol_table.h"
#include "../l2_system/l2_error.h"
#include "../l2_system/l2_assert.h"
#include "l2_eval.h"
#include "l2_scope.h"

extern char *g_l2_token_keywords[];

l2_parser *g_parser_p;

void l2_parse_finalize() {
    l2_token_stream_destroy(g_parser_p->token_stream_p);
    l2_call_stack_destroy(g_parser_p->call_stack_p);
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
    g_parser_p->call_stack_p = l2_call_stack_create();
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

void l2_absorb_stmt_var_def_list1();
void l2_absorb_stmt_elif();
void l2_absorb_formal_param_list();
void l2_absorb_stmts();


/* stmt ->
 * | { stmts }
 * | procedure id ( formal_param_list ) { stmts }
 * | while ( expr ) { stmts }
 * | do { stmts } while ( expr ) ;
 * | for ( expr ; expr ; expr ) { stmts }
 * | for ( var id = expr stmt_var_def_list1 ; expr ; expr ) { stmts }
 * | for ( var id stmt_var_def_list1 ; expr ; expr ) { stmts }
 * | break ;
 * | continue ;
 * | return ;
 * | return expr ;
 * | if ( expr ) { stmts } stmt_elif
 * | var id stmt_var_def_list1 ;
 * | var id = expr stmt_var_def_list1 ;
 * | ;
 * | expr ;
 * | eval expr;
 * |
 * */
void l2_absorb_stmt() {
    _declr_current_token_p

    _if_keyword (L2_KW_BREAK) /* "break" */
    {
        _get_current_token_p
        _if_type (L2_TOKEN_SEMICOLON)
        {
            /* absorb ';' */
        } _throw_missing_semicolon
    }
    _elif_keyword (L2_KW_CONTINUE) /* "continue" */
    {
        _get_current_token_p
        _if_type (L2_TOKEN_SEMICOLON)
        {
            /* absorb ';' */
        } _throw_missing_semicolon
    }
    _elif_keyword (L2_KW_RETURN) /* "return" */
    {
        _get_current_token_p
        _if_type (L2_TOKEN_SEMICOLON)
        {
            /* absorb ';' */
        }
        _else
        {
            l2_absorb_expr(); /* absorb return expr */

            _if_type (L2_TOKEN_SEMICOLON)
            {
                /* absorb ';' */
            } _throw_missing_semicolon
        }
    }
    _elif_keyword (L2_KW_PROCEDURE) /* "procedure" */ /* the definition of procedure */
    {
        _if_type (L2_TOKEN_IDENTIFIER) /* id */
        {

            _if_type (L2_TOKEN_LP) /* ( */
            {
                l2_absorb_formal_param_list();

                _if_type (L2_TOKEN_RP)
                {
                    /* absorb ')' */
                } _throw_missing_rp

                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    l2_absorb_stmts();

                    _if_type (L2_TOKEN_RBRACE)
                    {
                        /* absorb '}' */
                    } _throw_missing_rbrace

                } _throw_unexpected_token

            } _throw_unexpected_token

        } _throw_unexpected_token
    }
    _elif_keyword (L2_KW_FOR) /* "for" */ /* for-loop */
    {
        _if_type (L2_TOKEN_LP) /* ( */
        {
            /* handle the first expr ( allow to using definition stmt for variable )*/
            _if_keyword (L2_KW_VAR)
            {
                _if_type (L2_TOKEN_IDENTIFIER) /* id */
                {
                    _get_current_token_p

                    _if_type (L2_TOKEN_ASSIGN) /* = */
                    {
                        l2_absorb_expr(); /* absorb expr */

                        _if_type (L2_TOKEN_SEMICOLON)
                        {
                            /* absorb ';' */
                        } _throw_missing_semicolon

                    }
                    _elif_type (L2_TOKEN_SEMICOLON)
                    {
                        /* absorb ';' without initializing the variable */
                    } _throw_unexpected_token

                } _throw_unexpected_token
            }
            _elif_type (L2_TOKEN_SEMICOLON)
            {
                /* this means there is a empty expr in for-loop */
                /* also absorb ';' */
            }
            _else
            {
                l2_absorb_expr();
                _if_type (L2_TOKEN_SEMICOLON)
                {
                    /* absorb ';' */
                } _throw_missing_semicolon
            }

            _if_type (L2_TOKEN_SEMICOLON)
            {
                /* this means there is a empty expr in for-loop,
                 * and the second expr will have a bool-value with true if it is empty  */
                /* also absorb ';' */
            }
            _else
            {
                l2_absorb_expr();
                _if_type (L2_TOKEN_SEMICOLON)
                {
                    /* absorb ';' */
                } _throw_missing_semicolon
            }

            /* handle the third expr ( absorb it ) */
            /* just absorb the third expr */
            l2_absorb_expr();

            _if_type (L2_TOKEN_RP)
            {
                /* absorb ')' */
            } _throw_missing_rp

        } _throw_unexpected_token

        /* absorb the for stmt loop structure */
        _if_type (L2_TOKEN_LBRACE) /* { */
        {
            l2_absorb_stmts(); /* parse stmts, may parse break or continue*/

            _if_type (L2_TOKEN_RBRACE) /* } */
            {
                /* absorb '}' */
            }_throw_missing_rbrace

        } _throw_unexpected_token

    }
    _elif_keyword (L2_KW_DO) /* "do" */ /* do...while-loop */
    {
        _if_type (L2_TOKEN_LBRACE) /* { */
        {
            l2_absorb_stmts(); /* absorb stmts */

            _if_type (L2_TOKEN_RBRACE) /* } */
            {
                /* absorb '}' */
            } _throw_missing_rbrace

        } _throw_unexpected_token

        _if_keyword (L2_KW_WHILE) /* "while" */
        {
            _get_current_token_p

            _if_type (L2_TOKEN_LP) /* ( */
            {
                l2_absorb_expr(); /* absorb expr */
            } _throw_unexpected_token

            _if_type (L2_TOKEN_RP) /* ) */
            {
                /* absorb ')' */
            } _throw_missing_rp

            _if_type (L2_TOKEN_SEMICOLON) /* ; */
            {
                /* absorb ';' */
            } _throw_missing_semicolon

        } _throw_unexpected_token

    }
    _elif_keyword (L2_KW_WHILE) /* "while" */ /* while-loop */
    {
        _if_type (L2_TOKEN_LP) /* ( */
        {
            l2_absorb_expr(); /* absorb expr */

            _if_type (L2_TOKEN_RP)
            {
                /* absorb ')' */
            } _throw_missing_rp


            _if_type (L2_TOKEN_LBRACE) /* { */
            {
                l2_absorb_stmts(); /* absorb stmts */

                _if_type (L2_TOKEN_RBRACE)
                {
                    /* absorb '}' */
                } _throw_missing_rbrace

            } _throw_unexpected_token

        } _throw_unexpected_token

    }
    _elif_type (L2_TOKEN_LBRACE) /* { */
    {
        l2_absorb_stmts(); /* absorb stmts */

        _if_type (L2_TOKEN_RBRACE) /* } */
        {
            /* absorb '}' */
        } _throw_missing_rbrace

    }
    _elif_keyword (L2_KW_IF) /* "if" */
    {
        _get_current_token_p

        _if_type (L2_TOKEN_LP) /* ( */
        {
            l2_absorb_expr(); /* absorb expr */

            _if_type (L2_TOKEN_RP)
            {
                /* absorb ')' */
            } _throw_missing_rp

            _if_type (L2_TOKEN_LBRACE) /* { */
            {
                l2_absorb_stmts(); /* absorb stmts */

                _if_type (L2_TOKEN_RBRACE) /* } */
                {
                    l2_absorb_stmt_elif();
                } _throw_missing_rbrace

            } _throw_unexpected_token

        } _throw_unexpected_token

    }
    _elif_type (L2_TOKEN_SEMICOLON)
    {
        /* empty stmt which has only single ; */
    }
    _elif_keyword (L2_KW_VAR) /* "var" */
    {
        _if_type (L2_TOKEN_IDENTIFIER) /* id */
        {
            _if_type (L2_TOKEN_ASSIGN) /* = */
            {
                l2_absorb_expr(); /* expr */
            }
            _else /* without initialization */
            {

            }

            /* handle last part of definition list in recursion */
            l2_absorb_stmt_var_def_list1();

            _if_type (L2_TOKEN_SEMICOLON)
            {
                /* absorb ';' */
            } _throw_missing_semicolon

        } _throw_unexpected_token

    }
    _elif_keyword(L2_KW_EVAL)
    {
        l2_absorb_expr(); /* absorb expr */

        _if_type (L2_TOKEN_SEMICOLON)
        {
            /* absorb ';' */
        } _throw_missing_semicolon
    }
    _else
    {
        l2_absorb_expr(); /* absorb expr */

        _if_type (L2_TOKEN_SEMICOLON)
        {
            /* absorb ';' */
        } _throw_missing_semicolon
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

/* stmts ->
 * | stmt stmts
 * | eof
 *
 * */
l2_stmt_interrupt l2_parse_stmts(l2_scope *scope_p) {
    _declr_current_token_p
    l2_stmt_interrupt irt = { .type = L2_STMT_NO_INTERRUPT };

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_TERMINATOR)) {

    } else {
        irt = l2_parse_stmt(scope_p);

        if (irt.type != L2_STMT_NO_INTERRUPT) {
            /* absorb last stmts */
            l2_absorb_stmts();

            return irt;
        }

        /* there is no interrupt */
        irt = l2_parse_stmts(scope_p);

    }

    return irt;
}

/* formal_param_list1 ->
 * | , id formal_param_list1
 * | nil
 * */
void l2_absorb_formal_param_list1() {
    _if_type (L2_TOKEN_COMMA)
    {
        _if_type (L2_TOKEN_IDENTIFIER)
        {
            l2_absorb_formal_param_list1();
        } _throw_unexpected_token

    } _end
}

/* formal_param_list ->
 * | id formal_param_list1
 * | nil
 *
 * */
void l2_absorb_formal_param_list() {
    _if_type (L2_TOKEN_IDENTIFIER)
    {
        l2_absorb_formal_param_list1();
    } _end
}

/* stmt_var_def_list1 ->
 * | , id stmt_var_def_list1
 * | , id = expr stmt_var_def_list1
 * | nil
 * */
void l2_absorb_stmt_var_def_list1() {

    _if_type (L2_TOKEN_COMMA)
    {
        _if_type (L2_TOKEN_IDENTIFIER) /* id */
        {
            _if_type (L2_TOKEN_ASSIGN) /* = */
            {
                l2_absorb_expr(); /* expr */
            }
            _else /* without initialization */
            {

            }

            /* handle last part of definition list in recursion */
            l2_absorb_stmt_var_def_list1();

        } _throw_unexpected_token

    } _end
}

/* stmt_var_def_list1 ->
 * | , id stmt_var_def_list1
 * | , id = expr stmt_var_def_list1
 * | nil
 * */
void l2_parse_stmt_var_def_list1(l2_scope *scope_p) {

    boolean symbol_added;
    l2_expr_info right_expr_info;

    _declr_current_token_p

    _if_type (L2_TOKEN_COMMA)
    {
        _if_type (L2_TOKEN_IDENTIFIER) /* id */
        {
            _get_current_token_p

            _if_type (L2_TOKEN_ASSIGN) /* = */
            {
                right_expr_info = l2_eval_expr(scope_p); /* expr */

                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        symbol_added = l2_symbol_table_add_symbol_integer(&scope_p->symbol_table_p, current_token_p->u.str.str_p, right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_REAL:
                        symbol_added = l2_symbol_table_add_symbol_real(&scope_p->symbol_table_p, current_token_p->u.str.str_p, right_expr_info.val.real);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        symbol_added = l2_symbol_table_add_symbol_bool(&scope_p->symbol_table_p, current_token_p->u.str.str_p, right_expr_info.val.bool);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error token type");
                }
            }
            _else /* without initialization */
            {
                symbol_added = l2_symbol_table_add_symbol_without_initialization(&scope_p->symbol_table_p, current_token_p->u.str.str_p);
            }

            /* handle last part of definition list in recursion */
            l2_parse_stmt_var_def_list1(scope_p);

            if (!symbol_added)
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_REDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);

        } _throw_unexpected_token

    } _end
}

/* stmt_elif ->
 * | elif ( expr ) { stmts } stmt_elif
 * | else { stmts }
 * | nil
 *
 * */
void l2_absorb_stmt_elif() {

    _if_keyword(L2_KW_ELIF) /* "elif" */
    {
        _if_type (L2_TOKEN_LP) /* ( */
        {
            l2_absorb_expr();

            _if_type (L2_TOKEN_RP)
            {
                /* ) */
            }_throw_missing_rp


            _if_type (L2_TOKEN_LBRACE) /* { */
            {
                l2_absorb_stmts(); /* absorb stmts */

                _if_type (L2_TOKEN_RBRACE) /* } */
                {
                    l2_absorb_stmt_elif();

                } _throw_missing_rbrace

            } _throw_unexpected_token


        } _throw_unexpected_token
    }
    _elif_keyword (L2_KW_ELSE) /* "else" */
    {
        _if_type (L2_TOKEN_LBRACE) /* { */
        {
            l2_absorb_stmts(); /* absorb stmts */

            _if_type (L2_TOKEN_RBRACE)
            {
                /* } */
            } _throw_missing_rbrace

        } _throw_unexpected_token
    }
    _end
}

/* stmt_elif ->
 * | elif ( expr ) { stmts } stmt_elif
 * | else { stmts }
 * | nil
 *
 * */
l2_stmt_interrupt l2_parse_stmt_elif(l2_scope *scope_p) { /* the scopes in if..elif..else structure is coordinate each other */
    _declr_current_token_p
    l2_scope *sub_scope_p = L2_NULL_PTR;

    l2_stmt_interrupt irt = { .type = L2_STMT_NO_INTERRUPT };

    _if_keyword(L2_KW_ELIF) /* "elif" */
    {
        _get_current_token_p

        _if_type (L2_TOKEN_LP) /* ( */
        {
            l2_expr_info expr_info;
            expr_info = l2_eval_expr(scope_p);

            if (expr_info.val_type != L2_EXPR_VAL_TYPE_BOOL)
                l2_parsing_error(L2_PARSING_ERROR_EXPR_NOT_BOOL, current_token_p->current_line,
                                 current_token_p->current_col);

            _if_type (L2_TOKEN_RP)
            {
                /* ) */
            } _throw_missing_rp

            if (expr_info.val.bool) { /* elif true */
                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    sub_scope_p = l2_scope_create_common_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);
                    irt = l2_parse_stmts(sub_scope_p); /* parse stmts */

                    _if_type (L2_TOKEN_RBRACE) /* } */
                    {
                        l2_scope_escape_scope(sub_scope_p);
                        l2_absorb_stmt_elif();

                    } _throw_missing_rbrace

                } _throw_unexpected_token

            } else { /* elif false */
                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    l2_absorb_stmts(); /* parse stmts */

                    _if_type (L2_TOKEN_RBRACE) /* } */
                    {
                        irt = l2_parse_stmt_elif(scope_p);

                    } _throw_missing_rbrace

                } _throw_unexpected_token
            }

        } _throw_unexpected_token
    }
    _elif_keyword (L2_KW_ELSE) /* "else" */
    {
        _if_type (L2_TOKEN_LBRACE) /* { */
        {
            sub_scope_p = l2_scope_create_common_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);
            irt = l2_parse_stmts(sub_scope_p); /* parse stmts */

            _if_type (L2_TOKEN_RBRACE) /* } */
            {
                l2_scope_escape_scope(sub_scope_p);

            } _throw_missing_rbrace

        } _throw_unexpected_token
    }
    _end

    return irt;
}

/* TODO implement keywords: break, continue, return & return by value */



/* stmt ->
 * | { stmts }
 * | procedure id ( formal_param_list ) { stmts }
 * | while ( expr ) { stmts }
 * | do { stmts } while ( expr ) ;
 * | for ( expr ; expr ; expr ) { stmts }
 * | for ( var id = expr stmt_var_def_list1 ; expr ; expr ) { stmts }
 * | for ( var id stmt_var_def_list1 ; expr ; expr ) { stmts }
 * | break ;
 * | continue ;
 * | return ;
 * | return expr ;
 * | if ( expr ) { stmts } stmt_elif
 * | var id stmt_var_def_list1 ;
 * | var id = expr stmt_var_def_list1 ;
 * | ;
 * | expr ;
 * | eval expr;
 * */
l2_stmt_interrupt l2_parse_stmt(l2_scope *scope_p) {

    _declr_current_token_p
    l2_expr_info right_expr_info;
    boolean symbol_added;
    l2_scope *sub_scope_p = L2_NULL_PTR;

    l2_stmt_interrupt irt = { .type = L2_STMT_NO_INTERRUPT };

    l2_scope *loop_scope_p;

    _if_keyword (L2_KW_BREAK) /* "break" */
    {
        _get_current_token_p
        _if_type (L2_TOKEN_SEMICOLON)
        {
            irt.type = L2_STMT_INTERRUPT_BREAK;
            irt.col_of_irt_stmt = current_token_p->current_col;
            irt.line_of_irt_stmt = current_token_p->current_line;

        } _throw_missing_semicolon
/*
        if (scope_p->scope_type == L2_SCOPE_TYPE_COMMON)
            l2_parsing_error(L2_PARSING_ERROR_INVALID_BREAK_IN_CURRENT_CONTEXT, current_token_p->current_line, current_token_p->current_col);
*/
        return irt;
    }
    _elif_keyword (L2_KW_CONTINUE) /* "continue" */
    {
        _get_current_token_p
        _if_type (L2_TOKEN_SEMICOLON)
        {
            irt.type = L2_STMT_INTERRUPT_CONTINUE;
            irt.col_of_irt_stmt = current_token_p->current_col;
            irt.line_of_irt_stmt = current_token_p->current_line;

        } _throw_missing_semicolon

/*
        if (scope_p->scope_type == L2_SCOPE_TYPE_COMMON)
            l2_parsing_error(L2_PARSING_ERROR_INVALID_CONTINUE_IN_CURRENT_CONTEXT, current_token_p->current_line, current_token_p->current_col);
*/
        return irt;
    }
    _elif_keyword (L2_KW_RETURN) /* "return" */
    {
        _get_current_token_p
        _if_type (L2_TOKEN_SEMICOLON)
        {
            irt.type = L2_STMT_INTERRUPT_RETURN_WITHOUT_VAL; /* has no return value */
            irt.col_of_irt_stmt = current_token_p->current_col;
            irt.line_of_irt_stmt = current_token_p->current_line;
        }
        _else
        {
            irt.type = L2_STMT_INTERRUPT_RETURN_WITH_VAL; /* has return value */
            irt.col_of_irt_stmt = current_token_p->current_col;
            irt.line_of_irt_stmt = current_token_p->current_line;
            irt.u.ret_expr_info = l2_eval_expr(scope_p);

            _if_type (L2_TOKEN_SEMICOLON)
            {
                /* absorb ';' */
            } _throw_missing_semicolon
        }

        return irt;
    }
    _elif_keyword (L2_KW_PROCEDURE) /* "procedure" */ /* the definition of procedure */
    {
        l2_procedure procedure;
        _if_type (L2_TOKEN_IDENTIFIER) /* id */
        {
            _get_current_token_p
            procedure.entry_pos = l2_token_stream_get_pos(g_parser_p->token_stream_p);
            procedure.upper_scope_p = scope_p;

            _if_type (L2_TOKEN_LP) /* ( */
            {
                l2_absorb_formal_param_list();

                _if_type (L2_TOKEN_RP)
                {
                    /* absorb ')' */
                } _throw_missing_rp

                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    l2_absorb_stmts();

                    _if_type (L2_TOKEN_RBRACE)
                    {
                        /* absorb '}' */
                        /* store the procedure information as a symbol into symbol table */
                        symbol_added = l2_symbol_table_add_symbol_procedure(&scope_p->symbol_table_p, current_token_p->u.str.str_p, procedure);

                    } _throw_missing_rbrace

                } _throw_unexpected_token

            } _throw_unexpected_token

        } _throw_unexpected_token

        if (!symbol_added)
            l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_REDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);

    }
    _elif_keyword (L2_KW_FOR) /* "for" */ /* for-loop */
    {
        _get_current_token_p
        l2_expr_info second_expr_info;
        int loop_entry_pos, third_expr_entry_pos;
        l2_symbol_node *for_init_symbol_table_p = l2_symbol_table_create();

        _if_type (L2_TOKEN_LP) /* ( */
        {
            /* handle the first expr ( allow to using definition stmt for variable )*/
            _if_keyword (L2_KW_VAR)
            {
                _if_type (L2_TOKEN_IDENTIFIER) /* id */
                {
                    _get_current_token_p

                    _if_type (L2_TOKEN_ASSIGN) /* = */
                    {
                        right_expr_info = l2_eval_expr(scope_p); /* expr */

                        switch (right_expr_info.val_type) {
                            case L2_EXPR_VAL_TYPE_INTEGER:
                                symbol_added = l2_symbol_table_add_symbol_integer(&for_init_symbol_table_p, current_token_p->u.str.str_p, right_expr_info.val.integer);
                                break;

                            case L2_EXPR_VAL_TYPE_REAL:
                                symbol_added = l2_symbol_table_add_symbol_real(&for_init_symbol_table_p, current_token_p->u.str.str_p, right_expr_info.val.real);
                                break;

                            case L2_EXPR_VAL_TYPE_BOOL:
                                symbol_added = l2_symbol_table_add_symbol_bool(&for_init_symbol_table_p, current_token_p->u.str.str_p, right_expr_info.val.bool);
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
                        /* absorb ';' without initializing the variable */
                        symbol_added = l2_symbol_table_add_symbol_without_initialization(&for_init_symbol_table_p, current_token_p->u.str.str_p);

                    } _throw_unexpected_token

                    if (!symbol_added)
                        l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_REDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);

                } _throw_unexpected_token
            }
            _elif_type (L2_TOKEN_SEMICOLON)
            {
                /* this means there is a empty expr in for-loop */
                /* also absorb ';' */
            }
            _else
            {
                l2_eval_expr(scope_p);
                _if_type (L2_TOKEN_SEMICOLON)
                {
                    /* absorb ';' */
                } _throw_missing_semicolon
            }

            /* handle the second expr ( bool-expr that takes effects to the execution flow of loop ) */
            /* the expr belongs to the sub scope,
             * because maybe there is a variable definition at the parsing of the first expr,
             * the variable which is already in sub scope with effects
             * */
            loop_entry_pos = l2_token_stream_get_pos(g_parser_p->token_stream_p);
            __for_loop_entry__:

            /* in this form of variable definition, variable could only take effects to the sub scope */
            sub_scope_p = l2_scope_create_for_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE, loop_entry_pos);

            /* symbol table copy */
            l2_symbol_table_copy(&sub_scope_p->symbol_table_p, for_init_symbol_table_p);

            _if_type (L2_TOKEN_SEMICOLON)
            {
                /* this means there is a empty expr in for-loop,
                 * and the second expr will have a bool-value with true if it is empty  */

                second_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
                second_expr_info.val.bool = L2_TRUE;

                /* also absorb ';' */
            }
            _else
            {
                second_expr_info = l2_eval_expr(sub_scope_p);
                _if_type (L2_TOKEN_SEMICOLON)
                {
                    /* absorb ';' */
                } _throw_missing_semicolon
            }

            /* handle the third expr ( absorb and record its pos ) */
            third_expr_entry_pos = l2_token_stream_get_pos(g_parser_p->token_stream_p);

            /* the loop entry pos is, the pos of third expr */
            sub_scope_p->u.loop_entry_pos = third_expr_entry_pos;

            /* just absorb the third expr */
            l2_absorb_expr();

            _if_type (L2_TOKEN_RP)
            {
                /* absorb ')' */
            } _throw_missing_rp

        } _throw_unexpected_token

        /* TODO due to check the code logic */
        if (second_expr_info.val.bool) { /* for true */
            _if_type (L2_TOKEN_LBRACE) /* { */
            {
                irt = l2_parse_stmts(sub_scope_p); /* parse stmts, may parse break or continue*/

                _if_type (L2_TOKEN_RBRACE) /* } */
                {
                    l2_scope_escape_scope(sub_scope_p);

                } _throw_missing_rbrace

                /* handle stmts interrupt */
                switch (irt.type) {
                    case L2_STMT_INTERRUPT_RETURN_WITHOUT_VAL:
                    case L2_STMT_INTERRUPT_RETURN_WITH_VAL:
                        return irt;

                    case L2_STMT_INTERRUPT_BREAK:
                        break;

                    case L2_STMT_INTERRUPT_CONTINUE:
                        /* evaluate the third expr */
                        l2_token_stream_set_pos(g_parser_p->token_stream_p, third_expr_entry_pos);
                        l2_eval_expr(sub_scope_p);
                        /* | */
                        /* V */

                    case L2_STMT_NO_INTERRUPT:
                        l2_token_stream_set_pos(g_parser_p->token_stream_p, loop_entry_pos);
                        goto __for_loop_entry__;
                }

            } _throw_unexpected_token

        } else { /* for false */
            _if_type (L2_TOKEN_LBRACE) /* { */
            {
                l2_absorb_stmts(); /* parse stmts */

                _if_type (L2_TOKEN_RBRACE)
                {
                    /* absorb '}' */
                    l2_scope_escape_scope(sub_scope_p);

                } _throw_missing_rbrace

            } _throw_unexpected_token
        }

        /* when run over the for-loop, the initialization symbol table should be destroyed */
        l2_symbol_table_destroy(for_init_symbol_table_p);
    }
    _elif_keyword (L2_KW_DO) /* "do" */ /* do...while-loop */
    {
        l2_expr_info expr_info;

        int loop_entry_pos = l2_token_stream_get_pos(g_parser_p->token_stream_p);
        __do_loop_entry__: /* the mark of loop entry */

        _if_type (L2_TOKEN_LBRACE) /* { */
        {
            sub_scope_p = l2_scope_create_do_while_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE, loop_entry_pos);
            irt = l2_parse_stmts(sub_scope_p); /* parse stmts */

            _if_type (L2_TOKEN_RBRACE) /* } */
            {
                l2_scope_escape_scope(sub_scope_p);

            } _throw_missing_rbrace

        } _throw_unexpected_token

        _if_keyword (L2_KW_WHILE) /* "while" */
        {
            _get_current_token_p

            _if_type (L2_TOKEN_LP) /* ( */
            {
                /* handle stmts interrupt */
                switch (irt.type) {
                    case L2_STMT_INTERRUPT_RETURN_WITHOUT_VAL:
                    case L2_STMT_INTERRUPT_RETURN_WITH_VAL:
                        l2_absorb_expr();
                        _if_type (L2_TOKEN_RP)
                        {
                            /* absorb ')' */
                        } _throw_missing_rp

                        _if_type (L2_TOKEN_SEMICOLON)
                        {
                            /*  absorb ';'*/
                        } _throw_missing_semicolon
                        return irt;

                    case L2_STMT_INTERRUPT_BREAK:
                        l2_absorb_expr();
                        _if_type (L2_TOKEN_RP)
                        {
                            /* absorb ')' */
                        } _throw_missing_rp

                        _if_type (L2_TOKEN_SEMICOLON)
                        {
                            /*  absorb ';' */
                        } _throw_missing_semicolon
                        break;

                    case L2_STMT_INTERRUPT_CONTINUE:
                        l2_absorb_expr();
                        _if_type (L2_TOKEN_RP)
                        {
                            /* absorb ')' */
                        } _throw_missing_rp

                        _if_type (L2_TOKEN_SEMICOLON)
                        {
                            /*  absorb ';' */
                        } _throw_missing_semicolon

                        l2_token_stream_set_pos(g_parser_p->token_stream_p, loop_entry_pos);
                        goto __do_loop_entry__;

                    case L2_STMT_NO_INTERRUPT:
                        expr_info = l2_eval_expr(scope_p);

                        _if_type (L2_TOKEN_RP)
                        {
                            /* absorb ')' */
                        } _throw_missing_rp

                        _if_type (L2_TOKEN_SEMICOLON)
                        {
                            /*  absorb ';'*/
                        } _throw_missing_semicolon

                        if (expr_info.val_type != L2_EXPR_VAL_TYPE_BOOL)
                            l2_parsing_error(L2_PARSING_ERROR_EXPR_NOT_BOOL, current_token_p->current_line, current_token_p->current_col);

                        if (expr_info.val.bool) { /* while true */
                            l2_token_stream_set_pos(g_parser_p->token_stream_p, loop_entry_pos);
                            goto __do_loop_entry__;
                        }
                }

            } _throw_unexpected_token

        } _throw_unexpected_token
    }
    _elif_keyword (L2_KW_WHILE) /* "while" */ /* while-loop */
    {
        l2_expr_info expr_info;
        _get_current_token_p
        int loop_entry_pos = l2_token_stream_get_pos(g_parser_p->token_stream_p);

        __while_loop_entry__: /* the mark of loop entry */

        _if_type (L2_TOKEN_LP) /* ( */
        {
            expr_info = l2_eval_expr(scope_p);

            _if_type (L2_TOKEN_RP)
            {
                /* absorb ')' */
            } _throw_missing_rp

            if (expr_info.val_type != L2_EXPR_VAL_TYPE_BOOL)
                l2_parsing_error(L2_PARSING_ERROR_EXPR_NOT_BOOL, current_token_p->current_line, current_token_p->current_col);

            if (expr_info.val.bool) { /* while true */
                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    sub_scope_p = l2_scope_create_while_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE, loop_entry_pos);
                    irt = l2_parse_stmts(sub_scope_p); /* parse stmts */

                    _if_type (L2_TOKEN_RBRACE) /* } */
                    {
                        l2_scope_escape_scope(sub_scope_p);

                    } _throw_missing_rbrace

                    /* handle stmts interrupt */
                    switch (irt.type) {
                        case L2_STMT_INTERRUPT_RETURN_WITHOUT_VAL:
                        case L2_STMT_INTERRUPT_RETURN_WITH_VAL:
                            return irt;

                        case L2_STMT_INTERRUPT_BREAK:
                            break;

                        case L2_STMT_INTERRUPT_CONTINUE:
                        case L2_STMT_NO_INTERRUPT:
                            l2_token_stream_set_pos(g_parser_p->token_stream_p, loop_entry_pos);
                            goto __while_loop_entry__;
                    }

                } _throw_unexpected_token

            } else { /* while false */
                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    l2_absorb_stmts(); /* parse stmts */

                    _if_type (L2_TOKEN_RBRACE)
                    {
                        /* absorb '}' */
                    } _throw_missing_rbrace

                } _throw_unexpected_token
            }

        } _throw_unexpected_token
    }
    _elif_type (L2_TOKEN_LBRACE) /* { */
    {
        /* while parse a sub stmts block, a new sub scope should be also created */
        sub_scope_p = l2_scope_create_common_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);

        irt = l2_parse_stmts(sub_scope_p); /* stmts */

        _if_type (L2_TOKEN_RBRACE) /* } */
        {
            l2_scope_escape_scope(sub_scope_p);

        } _throw_missing_rbrace

        return irt;

    }
    _elif_keyword (L2_KW_IF) /* "if" */
    {
        _get_current_token_p

        _if_type (L2_TOKEN_LP) /* ( */
        {
            l2_expr_info expr_info;
            expr_info = l2_eval_expr(scope_p);

            if (expr_info.val_type != L2_EXPR_VAL_TYPE_BOOL)
                l2_parsing_error(L2_PARSING_ERROR_EXPR_NOT_BOOL, current_token_p->current_line, current_token_p->current_col);

            _if_type (L2_TOKEN_RP) {
                /* ) */
            } _throw_missing_rp

            if (expr_info.val.bool) { /* if true */
                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    sub_scope_p = l2_scope_create_common_scope(scope_p, L2_SCOPE_CREATE_SUB_SCOPE);
                    irt = l2_parse_stmts(sub_scope_p); /* parse stmts */

                    _if_type (L2_TOKEN_RBRACE) /* } */
                    {
                        l2_scope_escape_scope(sub_scope_p);
                        l2_absorb_stmt_elif();
                    } _throw_missing_rbrace

                } _throw_unexpected_token

            } else { /* if false */
                _if_type (L2_TOKEN_LBRACE) /* { */
                {
                    l2_absorb_stmts(); /* parse stmts */

                    _if_type (L2_TOKEN_RBRACE) /* } */
                    {
                        irt = l2_parse_stmt_elif(scope_p);
                    } _throw_missing_rbrace

                } _throw_unexpected_token
            }

            return irt;

        } _throw_unexpected_token

    }
    _elif_type (L2_TOKEN_SEMICOLON)
    {
        /* empty stmt which has only single ; */
    }
    _elif_keyword (L2_KW_VAR) /* "var" */
    {
        _if_type (L2_TOKEN_IDENTIFIER) /* id */
        {
            _get_current_token_p

            _if_type (L2_TOKEN_ASSIGN) /* = */
            {
                right_expr_info = l2_eval_expr(scope_p); /* expr */

                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        symbol_added = l2_symbol_table_add_symbol_integer(&scope_p->symbol_table_p, current_token_p->u.str.str_p, right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_REAL:
                        symbol_added = l2_symbol_table_add_symbol_real(&scope_p->symbol_table_p, current_token_p->u.str.str_p, right_expr_info.val.real);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        symbol_added = l2_symbol_table_add_symbol_bool(&scope_p->symbol_table_p, current_token_p->u.str.str_p, right_expr_info.val.bool);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error token type");
                }
            }
            _else
            {
                symbol_added = l2_symbol_table_add_symbol_without_initialization(&scope_p->symbol_table_p, current_token_p->u.str.str_p);
            }

            /* handle last part of definition list in recursion */
            l2_parse_stmt_var_def_list1(scope_p);

            _if_type (L2_TOKEN_SEMICOLON)
            {
                /* absorb ';' */
            } _throw_missing_semicolon

            if (!symbol_added)
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_REDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);

        } _throw_unexpected_token

    }
    _elif_keyword(L2_KW_EVAL)
    {
        right_expr_info = l2_eval_expr(scope_p); /* expr */

        _if_type (L2_TOKEN_SEMICOLON)
        {
            /* absorb ';' */
        } _throw_missing_semicolon

        switch (right_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                fprintf(stdout, "%d", right_expr_info.val.integer);
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                fprintf(stdout, "%lf", right_expr_info.val.real);
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                fprintf(stdout, "%s", right_expr_info.val.bool ? "true" : "false");
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error token type");
        }
    }
    _else
    {
        l2_eval_expr(scope_p); /* expr */

        _if_type (L2_TOKEN_SEMICOLON)
        {
            /* absorb ';' */
        } _throw_missing_semicolon
    }

    irt.type = L2_STMT_NO_INTERRUPT;
    return irt;
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

