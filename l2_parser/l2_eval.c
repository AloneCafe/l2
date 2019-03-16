#include "memory.h"
#include "l2_parse.h"
#include "l2_eval.h"
#include "../l2_system/l2_error.h"
#include "l2_symbol_table.h"
#include "stdarg.h"
#include "../l2_lexer/l2_token_stream.h"
#include "../l2_lexer/l2_cast.h"


extern char *g_l2_token_keywords[];
extern l2_parser *g_parser_p;

/* handle the div-by-zero error which possible occur */
int l2_eval_div_by_zero_filter(int val, int err_line, int err_col) {
    if (!val) l2_parsing_error(L2_PARSING_ERROR_DIVIDE_BY_ZERO, err_line, err_col);
    return val;
}

/* if symbol not exists then post a parsing error, this function must return non-null pointer where the symbol node stores at, otherwise it returns NULL */
l2_symbol_node *l2_eval_get_symbol_node(l2_scope *scope_p, char *symbol_name) {
    l2_symbol_node *symbol_node_p = l2_symbol_table_get_symbol_node_by_name_in_scope(scope_p, symbol_name);
    if (!symbol_node_p) symbol_node_p = l2_symbol_table_get_symbol_node_by_name_in_upper_scope(scope_p, symbol_name);
    return symbol_node_p;
}

boolean l2_eval_update_symbol_bool(l2_scope *scope_p, char *symbol_name, boolean bool) {
    l2_symbol_node *symbol_node_p = l2_eval_get_symbol_node(scope_p, symbol_name);
    if (!symbol_node_p) return L2_FALSE;
    symbol_node_p->symbol.type = L2_SYMBOL_TYPE_BOOL;
    symbol_node_p->symbol.u.bool = bool;
    return L2_TRUE;
}

boolean l2_eval_update_symbol_integer(l2_scope *scope_p, char *symbol_name, int integer) {
    l2_symbol_node *symbol_node_p = l2_eval_get_symbol_node(scope_p, symbol_name);
    if (!symbol_node_p) return L2_FALSE;
    symbol_node_p->symbol.type = L2_SYMBOL_TYPE_INTEGER;
    symbol_node_p->symbol.u.integer = integer;
    return L2_TRUE;
}

boolean l2_eval_update_symbol_real(l2_scope *scope_p, char *symbol_name, double real) {
    l2_symbol_node *symbol_node_p = l2_eval_get_symbol_node(scope_p, symbol_name);
    if (!symbol_node_p) return L2_FALSE;
    symbol_node_p->symbol.type = L2_SYMBOL_TYPE_REAL;
    symbol_node_p->symbol.u.real = real;
    return L2_TRUE;
}

l2_expr_info l2_eval_expr(l2_scope *scope_p) {
    return l2_eval_expr_comma(scope_p);
}


/* expr_comma ->
 * | expr_comma , expr_assign
 * | expr_assign
 * */

/* eliminate left recursion */

/* expr_comma ->
 * | expr_assign expr_comma1
 *
 * expr_comma1 ->
 * | , expr_assign expr_comma1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_comma1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    _if_type (L2_TOKEN_COMMA)
    {
        l2_expr_info right_expr_p;
        right_expr_p = l2_eval_expr_assign(scope_p);
        return l2_eval_expr_comma1(scope_p, right_expr_p);
    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_comma(l2_scope *scope_p) {
    l2_expr_info left_expr_p = l2_eval_expr_assign(scope_p);
    return l2_eval_expr_comma1(scope_p, left_expr_p);
}


/* expr_assign ->
 * | id = expr_assign
 * | id /= expr_assign
 * | id *= expr_assign
 * | id %= expr_assign
 * | id += expr_assign
 * | id -= expr_assign
 * | id <<= expr_assign
 * | id >>= expr_assign
 * | id >>>= expr_assign
 * | id &= expr_assign
 * | id ^= expr_assign
 * | id |= expr_assign
 * | expr_condition
 *
 * */
l2_expr_info l2_eval_expr_assign(l2_scope *scope_p) {
    l2_token *left_id_p;
    l2_expr_info right_expr_info, res_expr_info;
    l2_symbol_node *left_symbol_p;
    l2_token *token_opr_p;
    int opr_err_line, opr_err_col;
    int id_err_line, id_err_col;
    boolean symbol_updated = L2_TRUE;

    _if_type (L2_TOKEN_IDENTIFIER)
    {
        left_id_p = l2_parse_token_current();
        char *id_str_p = left_id_p->u.str.str_p;

        id_err_line = left_id_p->current_line;
        id_err_col = left_id_p->current_col;

        _if_type (L2_TOKEN_ASSIGN) /* = */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER: /* id = integer */
                    symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, right_expr_info.val.integer);
                    break;

                case L2_EXPR_VAL_TYPE_REAL: /* id = real */
                    symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, right_expr_info.val.real);
                    break;

                case L2_EXPR_VAL_TYPE_BOOL: /* id = true/false */
                    symbol_updated = l2_eval_update_symbol_bool(scope_p, id_str_p, right_expr_info.val.bool);
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            /* return the right expr info */
            res_expr_info = right_expr_info;
            return res_expr_info;

        }
        _elif_type (L2_TOKEN_DIV_ASSIGN) /* /= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer / l2_eval_div_by_zero_filter(right_expr_info.val.integer, opr_err_line, opr_err_col));
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.real / right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.integer / right_expr_info.val.real);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.real / right_expr_info.val.real);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_MUL_ASSIGN) /* *= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer * right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.real * right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.integer * right_expr_info.val.real);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.real * right_expr_info.val.real);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_MOD_ASSIGN) /* %= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer % l2_eval_div_by_zero_filter(right_expr_info.val.integer, opr_err_line, opr_err_col));
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between real and integer");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between real and real");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_PLUS_ASSIGN) /* += */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            char buff_s[100];

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer + right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.real + right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.integer + right_expr_info.val.real);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.real + right_expr_info.val.real);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_SUB_ASSIGN) /* -= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            char buff_s[100];

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer - right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.real - right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.integer - right_expr_info.val.real);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            symbol_updated = l2_eval_update_symbol_real(scope_p, id_str_p, res_expr_info.val.real = left_symbol_p->symbol.u.real - right_expr_info.val.real);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                            break;

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_LSHIFT_ASSIGN) /* <<= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            char buff_s[100];

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer << right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between real and integer");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between real and real");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_RSHIFT_ASSIGN) /* >>= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            char buff_s[100];

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer >> right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between real and integer");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between real and real");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_RSHIFT_UNSIGNED_ASSIGN) /* >>>= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            char buff_s[100];

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = ((unsigned int)left_symbol_p->symbol.u.integer) >> right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between real and integer");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between real and real");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_BIT_AND_ASSIGN) /* &= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            char buff_s[100];

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer & right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between real and integer");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between real and real");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_BIT_XOR_ASSIGN) /* ^= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            char buff_s[100];

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer ^ right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between real and integer");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between real and real");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _elif_type (L2_TOKEN_BIT_OR_ASSIGN) /* |= */
        {
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            char buff_s[100];

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer | right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between real and integer");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between real and real");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between real and bool");

                        //case L2_SYMBOL_TYPE_NATIVE_POINTER:
                        //    l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            return res_expr_info;

        }
        _else /* no match */
        {
            l2_parse_token_back();
            /*return l2_eval_expr_condition(scope_p);*/
            return l2_eval_expr_atom(scope_p);
        }

    }
    _else
    {
        /*return l2_eval_expr_condition(scope_p);*/
        return l2_eval_expr_atom(scope_p);
    }
}

/* expr_condition ->
 * | expr_logic_or ? expr : expr_condition
 * | expr_logic_or
 * */
l2_expr_info l2_eval_expr_condition(l2_scope *scope_p) {
    l2_expr_info first_expr_info;

    _declr_current_token_p

    first_expr_info = l2_eval_expr_logic_or(scope_p);
    int second_pos;

    _if_type (L2_TOKEN_QM) {
        //second_pos = l2_parse_token_stream_get_pos();
        _get_current_token_p

    } _else {
        return first_expr_info;
    }

    _if (first_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL) {
        if (first_expr_info.val.bool == L2_FALSE) { /* false */
            l2_absorb_expr(); /* absorb the second expr */
            _if_type (L2_TOKEN_COLON) /* eval the third expr */
            {
                return l2_eval_expr_condition(scope_p);

            } _throw_missing_colon

        } else { /* true */
            l2_expr_info second_expr_info = l2_eval_expr(scope_p); /* eval the second expr */
            _if_type (L2_TOKEN_COLON)
            {
                l2_absorb_expr_condition(); /* absorb the third expr */
                return second_expr_info;

            } _throw_missing_colon
        }

    } _throw_expr_not_bool
}

/* expr_logic_or ->
 * | expr_logic_or || expr_logic_and
 * | expr_logic_and
 * */

/* eliminate left recursion */

/* expr_logic_or ->
 * | expr_logic_and expr_logic_or1
 *
 * expr_logic_or1 ->
 * | || expr_logic_and expr_logic_or1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_logic_or1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p
    _if_type (L2_TOKEN_LOGIC_OR)
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_logic_and(scope_p);
        _if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL)
        {
            _if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL)
            {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
                new_left_expr_info.val.bool = (left_expr_info.val.bool || right_expr_info.val.bool);
                return l2_eval_expr_logic_or1(scope_p, new_left_expr_info);

            } _throw_right_expr_not_bool("||")

        } _throw_left_expr_not_bool("||")

    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_logic_or(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_logic_and(scope_p);
    return l2_eval_expr_logic_or1(scope_p, left_expr_info);
}

/* expr_logic_and ->
 * | expr_logic_and && expr_bit_or
 * | expr_bit_or
 * */

/* eliminate left recursion */

/* expr_logic_and ->
 * | expr_bit_or expr_logic_and1
 *
 * expr_logic_and1 ->
 * | && expr_bit_or expr_logic_and1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_logic_and1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p
    _if_type (L2_TOKEN_LOGIC_AND)
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_bit_or(scope_p);
        _if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL) {
            _if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL) {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
                new_left_expr_info.val.bool = (left_expr_info.val.bool && right_expr_info.val.bool);
                return l2_eval_expr_logic_and1(scope_p, new_left_expr_info);

            } _throw_right_expr_not_bool("&&")

        } _throw_left_expr_not_bool("&&")

    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_logic_and(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_bit_or(scope_p);
    return l2_eval_expr_logic_and1(scope_p, left_expr_info);
}

/* expr_bit_or ->
 * | expr_bit_or | expr_bit_xor
 * | expr_bit_xor
 * */

/* eliminate left recursion */

/* expr_bit_or ->
 * | expr_bit_xor expr_bit_or1
 *
 * expr_bit_or1 ->
 * | | expr_bit_xor expr_bit_or1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_bit_or1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p
    _if_type (L2_TOKEN_BIT_OR)
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_bit_xor(scope_p);
        _if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
            _if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                new_left_expr_info.val.integer = (left_expr_info.val.integer | right_expr_info.val.integer);
                return l2_eval_expr_bit_or1(scope_p, new_left_expr_info);

            } _throw_right_expr_not_bool("|")

        } _throw_left_expr_not_bool("|")

    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_bit_or(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_bit_xor(scope_p);
    return l2_eval_expr_bit_or1(scope_p, left_expr_info);
}

/* expr_bit_xor ->
 * | expr_bit_xor ^ expr_bit_and
 * | expr_bit_and
 * */

/* eliminate left recursion */

/* expr_bit_xor ->
 * | expr_bit_and expr_bit_xor1
 *
 * expr_bit_xor1 ->
 * | ^ expr_bit_and expr_bit_xor1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_bit_xor1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p
    _if_type (L2_TOKEN_BIT_XOR)
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_bit_and(scope_p);
        _if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
            _if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                new_left_expr_info.val.integer = (left_expr_info.val.integer ^ right_expr_info.val.integer);
                return l2_eval_expr_bit_xor1(scope_p, new_left_expr_info);

            } _throw_right_expr_not_bool("^")

        } _throw_left_expr_not_bool("^")

    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_bit_xor(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_bit_and(scope_p);
    return l2_eval_expr_bit_xor1(scope_p, left_expr_info);
}

/* expr_bit_and ->
 * | expr_bit_and & expr_eq_ne
 * | expr_eq_ne
 * */

/* eliminate left recursion */

/* expr_bit_and ->
 * | expr_eq_ne expr_bit_and1
 *
 * expr_bit_and1 ->
 * | & expr_eq_ne expr_bit_and1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_bit_and1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p
    _if_type (L2_TOKEN_BIT_AND)
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_eq_ne(scope_p);
        _if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
            _if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                new_left_expr_info.val.integer = (left_expr_info.val.integer & right_expr_info.val.integer);
                return l2_eval_expr_bit_and1(scope_p, new_left_expr_info);

            } _throw_right_expr_not_bool("&")

        } _throw_left_expr_not_bool("&")

    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_bit_and(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_eq_ne(scope_p);
    return l2_eval_expr_bit_and1(scope_p, left_expr_info);
}

/* expr_eq_ne ->
 * | expr_eq_ne != expr_gt_lt_ge_le
 * | expr_eq_ne == expr_gt_lt_ge_le
 * | expr_gt_lt_ge_le
 * */

/* eliminate left recursion */

/* expr_eq_ne ->
 * | expr_gt_lt_ge_le expr_eq_ne1
 *
 * expr_eq_ne1 ->
 * | != expr_gt_lt_ge_le expr_eq_ne1
 * | == expr_gt_lt_ge_le expr_eq_ne1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_eq_ne1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p
    _if_type (L2_TOKEN_EQUAL) /* == */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_gt_lt_ge_le(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer == right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = ((double)left_expr_info.val.integer == right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        new_left_expr_info.val.bool = (left_expr_info.val.bool == right_expr_info.val.bool);
                        break;

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.real == (double)right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = (left_expr_info.val.real == right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_eq_ne1(scope_p, new_left_expr_info);

    }
    _elif_type (L2_TOKEN_NOT_EQUAL) /* != */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_gt_lt_ge_le(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer != right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = ((double)left_expr_info.val.integer != right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        new_left_expr_info.val.bool = (left_expr_info.val.bool != right_expr_info.val.bool);
                        break;

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.real != (double)right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = (left_expr_info.val.real != right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_eq_ne1(scope_p, new_left_expr_info);

    }
    _else
    {
        return left_expr_info;
    }
}


l2_expr_info l2_eval_expr_eq_ne(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_gt_lt_ge_le(scope_p);
    return l2_eval_expr_eq_ne1(scope_p, left_expr_info);
}

/* expr_gt_lt_ge_le ->
 * | expr_gt_lt_ge_le >= expr_lshift_rshift_rshift_unsigned
 * | expr_gt_lt_ge_le > expr_lshift_rshift_rshift_unsigned
 * | expr_gt_lt_ge_le <= expr_lshift_rshift_rshift_unsigned
 * | expr_gt_lt_ge_le < expr_lshift_rshift_rshift_unsigned
 * | expr_lshift_rshift_rshift_unsigned
 * */

/* eliminate left recursion */

/* expr_gt_lt_ge_le ->
 * | expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 *
 * expr_eq_ne1 ->
 * | >= expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 * | > expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 * | <= expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 * | < expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_gt_lt_ge_le1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p

    _if_type (L2_TOKEN_GREAT_EQUAL_THAN) /* >= */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_lshift_rshift_rshift_unsigned(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer >= right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = ((double)left_expr_info.val.integer >= right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.real >= (double)right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = (left_expr_info.val.real >= right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_gt_lt_ge_le1(scope_p, new_left_expr_info);

    }
    _elif_type (L2_TOKEN_GREAT_THAN) /* > */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_lshift_rshift_rshift_unsigned(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer > right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = ((double)left_expr_info.val.integer > right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.real > (double)right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = (left_expr_info.val.real > right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_gt_lt_ge_le1(scope_p, new_left_expr_info);

    }
    _elif_type (L2_TOKEN_LESS_EQUAL_THAN) /* <= */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_lshift_rshift_rshift_unsigned(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer <= right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = ((double)left_expr_info.val.integer <= right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.real <= (double)right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = (left_expr_info.val.real <= right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_gt_lt_ge_le1(scope_p, new_left_expr_info);

    }
    _elif_type (L2_TOKEN_LESS_THAN) /* < */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_lshift_rshift_rshift_unsigned(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer < right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = ((double)left_expr_info.val.integer < right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.real < (double)right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val.bool = (left_expr_info.val.real < right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_gt_lt_ge_le1(scope_p, new_left_expr_info);

    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_gt_lt_ge_le(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_lshift_rshift_rshift_unsigned(scope_p);
    return l2_eval_expr_gt_lt_ge_le1(scope_p, left_expr_info);
}

/* expr_lshift_rshift_rshift_unsigned ->
 * | expr_lshift_rshift_rshift_unsigned << expr_plus_sub
 * | expr_lshift_rshift_rshift_unsigned >> expr_plus_sub
 * | expr_lshift_rshift_rshift_unsigned >>> expr_plus_sub
 * | expr_plus_sub
 * */

/* eliminate left recursion */

/* expr_lshift_rshift_rshift_unsigned ->
 * | expr_plus_sub expr_lshift_rshift_rshift_unsigned1
 *
 * expr_lshift_rshift_rshift_unsigned1 ->
 * | << expr_plus_sub expr_lshift_rshift_rshift_unsigned1
 * | >> expr_plus_sub expr_lshift_rshift_rshift_unsigned1
 * | >>> expr_plus_sub expr_lshift_rshift_rshift_unsigned1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_lshift_rshift_rshift_unsigned1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p

    _if_type (L2_TOKEN_LSHIFT) /* << */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_plus_sub(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.integer = (left_expr_info.val.integer << right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "integer", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "real", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "real", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_lshift_rshift_rshift_unsigned1(scope_p, new_left_expr_info);

    }
    _elif_type (L2_TOKEN_RSHIFT) /* >> */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_plus_sub(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.integer = (left_expr_info.val.integer >> right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "integer", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "real", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "real", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_lshift_rshift_rshift_unsigned1(scope_p, new_left_expr_info);

    }
    _elif_type (L2_TOKEN_RSHIFT_UNSIGNED) /* >>> */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_plus_sub(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.integer = (((unsigned int)left_expr_info.val.integer) >> right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "integer", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "real", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "real", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_lshift_rshift_rshift_unsigned1(scope_p, new_left_expr_info);

    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_lshift_rshift_rshift_unsigned(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_plus_sub(scope_p);
    return l2_eval_expr_lshift_rshift_rshift_unsigned1(scope_p, left_expr_info);
}

/* expr_plus_sub ->
 * | expr_plus_sub + expr_mul_div_mod
 * | expr_plus_sub - expr_mul_div_mod
 * | expr_mul_div_mod
 * */

/* eliminate left recursion */

/* expr_plus_sub ->
 * | expr_mul_div_mod expr_plus_sub1
 *
 * expr_plus_sub1 ->
 * | + expr_mul_div_mod expr_plus_sub1
 * | - expr_mul_div_mod expr_plus_sub1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_plus_sub1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p

    _if_type (L2_TOKEN_PLUS) /* + */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_plus_sub(scope_p);

        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                        new_left_expr_info.val.integer = left_expr_info.val.integer + right_expr_info.val.integer;
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "+", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = left_expr_info.val.integer + right_expr_info.val.real;
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "+", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "+", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "+", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = left_expr_info.val.real + right_expr_info.val.integer;
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "+", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = left_expr_info.val.real + right_expr_info.val.real;
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_plus_sub1(scope_p, new_left_expr_info);

    }
    _elif_type (L2_TOKEN_SUB) /* - */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_plus_sub(scope_p);

        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                        new_left_expr_info.val.integer = left_expr_info.val.integer - right_expr_info.val.integer;
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "-", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = left_expr_info.val.integer - right_expr_info.val.real;
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "-", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "-", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "-", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = left_expr_info.val.real - right_expr_info.val.integer;
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "-", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = left_expr_info.val.real - right_expr_info.val.real;
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_plus_sub1(scope_p, new_left_expr_info);

    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_plus_sub(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_mul_div_mod(scope_p);
    return l2_eval_expr_plus_sub1(scope_p, left_expr_info);
}

/* expr_mul_div_mod ->
 * | expr_mul_div_mod * expr_single
 * | expr_mul_div_mod / expr_single
 * | expr_mul_div_mod % expr_single
 * | expr_single
 * */

/* eliminate left recursion */

/* expr_mul_div_mod ->
 * | expr_single expr_mul_div_mod1
 *
 * expr_mul_div_mod1 ->
 * | * expr_single expr_mul_div_mod1
 * | / expr_single expr_mul_div_mod1
 * | % expr_single expr_mul_div_mod1
 * | nil
 *
 * */
l2_expr_info l2_eval_expr_mul_div_mod1(l2_scope *scope_p, l2_expr_info left_expr_info) {
    l2_expr_info right_expr_info, new_left_expr_info;
    _declr_current_token_p

    _if_type (L2_TOKEN_MUL) /* * */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_single(scope_p);
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                        new_left_expr_info.val.integer = (left_expr_info.val.integer * right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = (left_expr_info.val.integer * right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = (left_expr_info.val.real * right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = (left_expr_info.val.real * right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_lshift_rshift_rshift_unsigned1(scope_p, new_left_expr_info);

    }
    _elif_type (L2_TOKEN_DIV) /* / */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_single(scope_p);
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                        new_left_expr_info.val.integer = (left_expr_info.val.integer / right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = (left_expr_info.val.integer / right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = (left_expr_info.val.real / right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                        new_left_expr_info.val.real = (left_expr_info.val.real / right_expr_info.val.real);
                        break;

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_lshift_rshift_rshift_unsigned1(scope_p, new_left_expr_info);

    }
    _elif_type (L2_TOKEN_MOD) /* % */
    {
        _get_current_token_p
        right_expr_info = l2_eval_expr_single(scope_p);
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.integer = (left_expr_info.val.integer % right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "integer", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "bool", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "bool", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "real", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "real", "bool");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "real", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return l2_eval_expr_lshift_rshift_rshift_unsigned1(scope_p, new_left_expr_info);

    }
    _else
    {
        return left_expr_info;
    }
}

l2_expr_info l2_eval_expr_mul_div_mod(l2_scope *scope_p) {
    l2_expr_info left_expr_info = l2_eval_expr_single(scope_p);
    return l2_eval_expr_mul_div_mod1(scope_p, left_expr_info);
}

/* expr_single ->
 * | ! expr_single
 * | ~ expr_single
 * | - expr_single (anywhere, -- > -)
 * | expr_atom
 *
 * */
l2_expr_info l2_eval_expr_single(l2_scope *scope_p) {
    l2_expr_info res_expr_info, right_expr_info;
    _declr_current_token_p

    _if_type (L2_TOKEN_LOGIC_NOT) /* ! */
    {
        _get_current_token_p

        right_expr_info = l2_eval_expr_single(scope_p);
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (right_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!", "integer");

            case L2_EXPR_VAL_TYPE_BOOL:
                res_expr_info.val.bool = !(right_expr_info.val.bool);
                break;

            case L2_EXPR_VAL_TYPE_REAL:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!", "real");

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return res_expr_info;

    }
    _elif_type (L2_TOKEN_BIT_NOT) /* ~ */
    {
        _get_current_token_p

        right_expr_info = l2_eval_expr_single(scope_p);
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (right_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                res_expr_info.val.integer = ~(right_expr_info.val.integer);
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "~", "bool");

            case L2_EXPR_VAL_TYPE_REAL:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "~", "real");

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return res_expr_info;

    }
    _elif_type (L2_TOKEN_SUB) /* - */
    {
        _get_current_token_p

        right_expr_info = l2_eval_expr_single(scope_p);
        switch (right_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                res_expr_info.val.integer = -(right_expr_info.val.integer);
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "-", "bool");

            case L2_EXPR_VAL_TYPE_REAL:
                res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                res_expr_info.val.real = -(right_expr_info.val.real);
                break;
            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return res_expr_info;

    }
    _else
    {
        return l2_eval_expr_atom(scope_p);
    }
}

/* real_param_list1 ->
 * | , id real_param_list1
 * | nil
 * */
void l2_parse_real_param_list1(l2_scope *scope_p, l2_vector *symbol_vec_p) {
    _declr_current_token_p
    _if_type (L2_TOKEN_COMMA)
    {
        _if_type (L2_TOKEN_IDENTIFIER)
        {
            _get_current_token_p
            char *id_str_p = current_token_p->u.str.str_p;
            l2_symbol_node *symbol_node_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!symbol_node_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);
            /* symbol_node_p is not null */
            l2_vector_append(symbol_vec_p, &(symbol_node_p->symbol));
            l2_parse_real_param_list1(scope_p, symbol_vec_p);

        } _throw_unexpected_token

    } _end
}

/* real_param_list ->
 * | id real_param_list1
 * | nil
 *
 * */
void l2_parse_real_param_list(l2_scope *scope_p, l2_vector *symbol_vec_p) {
    _declr_current_token_p
    _if_type (L2_TOKEN_IDENTIFIER)
    {
        _get_current_token_p
        char *id_str_p = current_token_p->u.str.str_p;
        l2_symbol_node *symbol_node_p = l2_eval_get_symbol_node(scope_p, id_str_p);
        if (!symbol_node_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);
        /* symbol_node_p is not null */
        l2_vector_append(symbol_vec_p, &(symbol_node_p->symbol));
        l2_parse_real_param_list1(scope_p, symbol_vec_p);

    } _end
}

/* formal_param_list1 ->
 * | , id formal_param_list1
 * | nil
 * */
/* symbol_vec_p: including the symbols of real parameters */
/* symbol_pos: the current identifier count */
void l2_parse_formal_param_list1(l2_scope *scope_p, l2_vector *symbol_vec_p, int symbol_pos) {
    _declr_current_token_p
    _if_type (L2_TOKEN_COMMA)
    {
        _if_type (L2_TOKEN_IDENTIFIER)
        {
            _get_current_token_p
            /* if (id count < real params count) */
            if (symbol_pos < symbol_vec_p->size) {

                l2_symbol symbol = *(l2_symbol *)l2_vector_at(symbol_vec_p, symbol_pos);
                symbol.symbol_name = current_token_p->u.str.str_p;

                boolean add_symbol_result = l2_symbol_table_add_symbol(&scope_p->symbol_table_p, symbol);

                if (!add_symbol_result) {
                    l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_REDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);
                }

                l2_parse_formal_param_list1(scope_p, symbol_vec_p, symbol_pos + 1);

            } else { /* id count >= real params count */
                _if_type (L2_TOKEN_IDENTIFIER)
                {
                    l2_parsing_error(L2_PARSING_ERROR_TOO_FEW_PARAMETERS, current_token_p->current_line, current_token_p->current_col);
                }
                _else
                {
                    /* symbol_pos == symbol_vec_p->size ( means real parameters list is over ) */
                    /* and next token is not identifier ( means formal parameters list is over ) */
                    return;
                }

            }

        } _throw_unexpected_token

    } _end
}

/* formal_param_list ->
 * | id formal_param_list1
 * | nil
 *
 * */
void l2_parse_formal_param_list(l2_scope *scope_p, l2_vector *symbol_vec_p) {
    _declr_current_token_p
    _get_current_token_p
    int symbol_pos = 0;

    _if_type (L2_TOKEN_IDENTIFIER)
    {
        _get_current_token_p
        if (symbol_pos < symbol_vec_p->size) {

            l2_symbol symbol = *(l2_symbol *)l2_vector_at(symbol_vec_p, symbol_pos);
            symbol.symbol_name = current_token_p->u.str.str_p;

            boolean add_symbol_result = l2_symbol_table_add_symbol(&scope_p->symbol_table_p, symbol);

            if (!add_symbol_result) {
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_REDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);
            }

            l2_parse_formal_param_list1(scope_p, symbol_vec_p, symbol_pos + 1);
        }
        else {
            _if_type (L2_TOKEN_IDENTIFIER)
            {
                l2_parsing_error(L2_PARSING_ERROR_TOO_FEW_PARAMETERS, current_token_p->current_line, current_token_p->current_col);
            }
            _else
            {
                /* symbol_pos == symbol_vec_p->size ( means real parameters list is over ) */
                /* and next token is not identifier ( means formal parameters list is over ) */
                return;
            }
        }

    } _end

    if (symbol_pos < symbol_vec_p->size) {
        l2_parsing_error(L2_PARSING_ERROR_TOO_MANY_PARAMETERS, current_token_p->current_line, current_token_p->current_col);
    }
}


/* expr_atom ->
 * | id
 * | id ( real_param_list )
 * | //not implement// id [ expr ]
 * | integer_literal
 * | string_literal
 * | real_literal
 * | keyword: true / false
 * |
 *
 * */
l2_expr_info l2_eval_expr_atom(l2_scope *scope_p) {

    _declr_current_token_p
    l2_symbol_node *symbol_node_p;
    l2_expr_info res_expr_info;

    _if_type (L2_TOKEN_IDENTIFIER) /* id */
    {
        _get_current_token_p

        _if_type (L2_TOKEN_LP) /* '(' */
        {
            /* TODO handle procedure calling */
            //symbol_node_p->symbol.u.procedure
            symbol_node_p = l2_eval_get_symbol_node(scope_p, current_token_p->u.str.str_p);
            if (!symbol_node_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);

            l2_vector symbol_vec;
            l2_vector_create(&symbol_vec, sizeof(l2_symbol));
            l2_parse_real_param_list(scope_p, &symbol_vec);

            _if_type (L2_TOKEN_RP)
            {
                /* absorb ')' */
            } _throw_missing_rp


            /* judge the symbol type ( procedure ) */
            if (symbol_node_p->symbol.type == L2_SYMBOL_TYPE_PROCEDURE) {

                /* put all of call procedure informations into a single call_frame */
                l2_call_frame call_frame;
                call_frame.param_list.symbol_vec = symbol_vec;
                call_frame.ret_pos = l2_token_stream_get_pos(g_parser_p->token_stream_p);

                l2_call_stack_push_frame(g_parser_p->call_stack_p, call_frame);

                /* perform procedure call, take parser into a new token stream position */
                l2_token_stream_set_pos(g_parser_p->token_stream_p, symbol_node_p->symbol.u.procedure.entry_pos);

                /* create new sub scope */
                l2_scope *procedure_scope_p = l2_scope_create_common_scope(symbol_node_p->symbol.u.procedure.upper_scope_p, L2_SCOPE_CREATE_SUB_SCOPE);

                /* TODO enter into procedure */
                _if_type (L2_TOKEN_LP) /* ( */
                {
                    l2_parse_formal_param_list(procedure_scope_p, &symbol_vec);

                    _if_type (L2_TOKEN_RP)
                    {
                        /* absorb ')' */
                    } _throw_missing_rp

                    _if_type (L2_TOKEN_LBRACE) /* { */
                    {
                        /* TODO the function maybe return a interrupt */
                        l2_stmt_interrupt irt = l2_parse_stmts(procedure_scope_p);

                        switch (irt.type) {
                            case L2_STMT_INTERRUPT_CONTINUE:
                                l2_parsing_error(L2_PARSING_ERROR_INVALID_CONTINUE_IN_CURRENT_CONTEXT, irt.line_of_irt_stmt, irt.col_of_irt_stmt);

                            case L2_STMT_INTERRUPT_BREAK:
                                l2_parsing_error(L2_PARSING_ERROR_INVALID_BREAK_IN_CURRENT_CONTEXT, irt.line_of_irt_stmt, irt.col_of_irt_stmt);

                            case L2_STMT_INTERRUPT_RETURN_WITH_VAL:
                                /* the copy with light transfer */
                                memcpy(&res_expr_info, &irt.u.ret_expr_info, sizeof(res_expr_info));
                                break;

                            case L2_STMT_INTERRUPT_RETURN_WITHOUT_VAL:
                                /* no return value */
                                res_expr_info.val_type = L2_EXPR_VAL_NO_VAL;
                                break;

                            case L2_STMT_NO_INTERRUPT:
                                /* no interrupt means no return stmt */
                                res_expr_info.val_type = L2_EXPR_VAL_NO_VAL;
                                break;
                        }

                        _if_type (L2_TOKEN_RBRACE)
                        {
                            /* absorb '}' */
                        } _throw_missing_rbrace

                    } _throw_unexpected_token

                } _throw_unexpected_token

                /* procedure execution complete, restore call frame*/
                call_frame = l2_call_stack_pop_frame(g_parser_p->call_stack_p);
                l2_token_stream_set_pos(g_parser_p->token_stream_p, call_frame.ret_pos);
                l2_vector_destroy(&symbol_vec);

            } else { /* symbol is not procedure, it will not call the procedure */
                l2_vector_destroy(&symbol_vec);
                l2_parsing_error(L2_PARSING_ERROR_SYMBOL_IS_NOT_PROCEDURE, current_token_p->current_line, current_token_p->current_col, current_token_p->u.str.str_p);
            }

            return res_expr_info;
        }
        _else
        {
            char *id_str_p = current_token_p->u.str.str_p;
            symbol_node_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!symbol_node_p)
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, current_token_p->current_line,
                                 current_token_p->current_col, current_token_p->u.str.str_p);

            switch (symbol_node_p->symbol.type) { /* package symbol into expr node */
                case L2_SYMBOL_TYPE_INTEGER:
                    res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                    res_expr_info.val.integer = symbol_node_p->symbol.u.integer;
                    break;

                case L2_SYMBOL_TYPE_BOOL:
                    res_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
                    res_expr_info.val.bool = symbol_node_p->symbol.u.bool;
                    break;

                case L2_SYMBOL_TYPE_REAL:
                    res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                    res_expr_info.val.real = symbol_node_p->symbol.u.real;
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }
        }

    }
    _elif_type (L2_TOKEN_INTEGER_LITERAL)
    {
        _get_current_token_p
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        res_expr_info.val.integer = current_token_p->u.integer;

    }
    _elif_type (L2_TOKEN_REAL_LITERAL)
    {
        _get_current_token_p
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
        res_expr_info.val.real = current_token_p->u.real;

    }
    _elif_keyword (L2_KW_TRUE) /* "true" */
    {
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        res_expr_info.val.bool = L2_TRUE;

    }
    _elif_keyword (L2_KW_FALSE) /* "false" */
    {
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        res_expr_info.val.bool = L2_FALSE;

    } _throw_unexpected_token

    return res_expr_info;
}


/* ---------------------------------------------------------------------------
 * the function implement for expression absorb,
 * using for parsing expression without scope or symbol table and take no effect to them
 * ---------------------------------------------------------------------------
 * */

void l2_absorb_expr() {
    l2_absorb_expr_comma();
}

/* expr_comma ->
 * | expr_comma , expr_assign
 * | expr_assign
 * */

/* eliminate left recursion */

/* expr_comma ->
 * | expr_assign expr_comma1
 *
 * expr_comma1 ->
 * | , expr_assign expr_comma1
 * | nil
 *
 * */
void l2_absorb_expr_comma1() {
    _if_type (L2_TOKEN_COMMA)
    {
        l2_absorb_expr_assign();
        l2_absorb_expr_comma1();

    } _end
}

void l2_absorb_expr_comma() {
    l2_absorb_expr_assign();
    l2_absorb_expr_comma1();
}

/* expr_assign ->
 * | id = expr_assign
 * | id /= expr_assign
 * | id *= expr_assign
 * | id %= expr_assign
 * | id += expr_assign
 * | id -= expr_assign
 * | id <<= expr_assign
 * | id >>= expr_assign
 * | id >>>= expr_assign
 * | id &= expr_assign
 * | id ^= expr_assign
 * | id |= expr_assign
 * | expr_condition
 *
 * */
void l2_absorb_expr_assign() {
    _if_type (L2_TOKEN_IDENTIFIER)
    {
        _if_type(L2_TOKEN_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_DIV_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_MUL_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_MOD_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_PLUS_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_SUB_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_LSHIFT_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_RSHIFT_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_RSHIFT_UNSIGNED_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_BIT_AND_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_BIT_XOR_ASSIGN) { l2_absorb_expr_assign(); }
        _elif_type(L2_TOKEN_BIT_OR_ASSIGN) { l2_absorb_expr_assign(); }
        _else
        {
            l2_parse_token_back();
            l2_absorb_expr_condition();
        }
    }
    _else
    {
        l2_absorb_expr_condition();
    }
}

/* expr_condition ->
 * | expr_logic_or ? expr : expr_condition
 * | expr_logic_or
 * */
void l2_absorb_expr_condition() {
    _declr_current_token_p
    l2_absorb_expr_logic_or();
    _if_type(L2_TOKEN_QM)
    {
        l2_absorb_expr();

        _if_type(L2_TOKEN_COLON)
        {
            l2_absorb_expr_condition();

        } _throw_missing_colon
    } _end
}

/* expr_logic_or ->
 * | expr_logic_or || expr_logic_and
 * | expr_logic_and
 * */

/* eliminate left recursion */

/* expr_logic_or ->
 * | expr_logic_and expr_logic_or1
 *
 * expr_logic_or1 ->
 * | || expr_logic_and expr_logic_or1
 * | nil
 *
 * */
void l2_absorb_expr_logic_or1() {
    _if_type (L2_TOKEN_LOGIC_OR)
    {
        l2_absorb_expr_logic_and();
        l2_absorb_expr_logic_or1();
    } _end
}

void l2_absorb_expr_logic_or() {
    l2_absorb_expr_logic_and();
    l2_absorb_expr_logic_or1();
}

/* expr_logic_and ->
 * | expr_logic_and && expr_bit_or
 * | expr_bit_or
 * */

/* eliminate left recursion */

/* expr_logic_and ->
 * | expr_bit_or expr_logic_and1
 *
 * expr_logic_and1 ->
 * | && expr_bit_or expr_logic_and1
 * | nil
 *
 * */
void l2_absorb_expr_logic_and1() {
    _if_type (L2_TOKEN_LOGIC_AND)
    {
        l2_absorb_expr_bit_or();
        l2_absorb_expr_logic_and1();
    } _end
}

void l2_absorb_expr_logic_and() {
    l2_absorb_expr_bit_or();
    l2_absorb_expr_logic_and1();
}

/* expr_bit_or ->
 * | expr_bit_or | expr_bit_xor
 * | expr_bit_xor
 * */

/* eliminate left recursion */

/* expr_bit_or ->
 * | expr_bit_xor expr_bit_or1
 *
 * expr_bit_or1 ->
 * | | expr_bit_xor expr_bit_or1
 * | nil
 *
 * */
void l2_absorb_expr_bit_or1() {
    _if_type (L2_TOKEN_BIT_OR)
    {
        l2_absorb_expr_bit_xor();
        l2_absorb_expr_bit_or1();
    } _end
}

void l2_absorb_expr_bit_or() {
    l2_absorb_expr_bit_xor();
    l2_absorb_expr_bit_or1();
}

/* expr_bit_xor ->
 * | expr_bit_xor ^ expr_bit_and
 * | expr_bit_and
 * */

/* eliminate left recursion */

/* expr_bit_xor ->
 * | expr_bit_and expr_bit_xor1
 *
 * expr_bit_xor1 ->
 * | ^ expr_bit_and expr_bit_xor1
 * | nil
 *
 * */
void l2_absorb_expr_bit_xor1() {
    _if_type (L2_TOKEN_BIT_XOR)
    {
        l2_absorb_expr_bit_and();
        l2_absorb_expr_bit_xor1();
    } _end
}

void l2_absorb_expr_bit_xor() {
    l2_absorb_expr_bit_and();
    l2_absorb_expr_bit_xor1();
}

/* expr_bit_and ->
 * | expr_bit_and & expr_eq_ne
 * | expr_eq_ne
 * */

/* eliminate left recursion */

/* expr_bit_and ->
 * | expr_eq_ne expr_bit_and1
 *
 * expr_bit_and1 ->
 * | & expr_eq_ne expr_bit_and1
 * | nil
 *
 * */
void l2_absorb_expr_bit_and1() {
    _if_type (L2_TOKEN_BIT_AND)
    {
        l2_absorb_expr_eq_ne();
        l2_absorb_expr_bit_and1();
    } _end
}

void l2_absorb_expr_bit_and() {
    l2_absorb_expr_eq_ne();
    l2_absorb_expr_bit_and1();
}

/* expr_eq_ne ->
 * | expr_eq_ne != expr_gt_lt_ge_le
 * | expr_eq_ne == expr_gt_lt_ge_le
 * | expr_gt_lt_ge_le
 * */

/* eliminate left recursion */

/* expr_eq_ne ->
 * | expr_gt_lt_ge_le expr_eq_ne1
 *
 * expr_eq_ne1 ->
 * | != expr_gt_lt_ge_le expr_eq_ne1
 * | == expr_gt_lt_ge_le expr_eq_ne1
 * | nil
 *
 * */
void l2_absorb_expr_eq_ne1() {
    _if_type (L2_TOKEN_EQUAL)
    {
        l2_absorb_expr_gt_lt_ge_le();
        l2_absorb_expr_eq_ne1();
    }
    _elif_type (L2_TOKEN_NOT_EQUAL)
    {
        l2_absorb_expr_gt_lt_ge_le();
        l2_absorb_expr_eq_ne1();
    }
    _end
}


void l2_absorb_expr_eq_ne() {
    l2_absorb_expr_gt_lt_ge_le();
    l2_absorb_expr_eq_ne1();

}

/* expr_gt_lt_ge_le ->
 * | expr_gt_lt_ge_le >= expr_lshift_rshift_rshift_unsigned
 * | expr_gt_lt_ge_le > expr_lshift_rshift_rshift_unsigned
 * | expr_gt_lt_ge_le <= expr_lshift_rshift_rshift_unsigned
 * | expr_gt_lt_ge_le < expr_lshift_rshift_rshift_unsigned
 * | expr_lshift_rshift_rshift_unsigned
 * */

/* eliminate left recursion */

/* expr_gt_lt_ge_le ->
 * | expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 *
 * expr_eq_ne1 ->
 * | >= expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 * | > expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 * | <= expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 * | < expr_lshift_rshift_rshift_unsigned expr_gt_lt_ge_le1
 * | nil
 *
 * */
void l2_absorb_expr_gt_lt_ge_le1() {
    _if_type (L2_TOKEN_GREAT_EQUAL_THAN)
    {
        l2_absorb_expr_lshift_rshift_rshift_unsigned();
        l2_absorb_expr_gt_lt_ge_le1();
    }
    _elif_type (L2_TOKEN_GREAT_THAN)
    {
        l2_absorb_expr_lshift_rshift_rshift_unsigned();
        l2_absorb_expr_gt_lt_ge_le1();
    }
    _elif_type (L2_TOKEN_LESS_EQUAL_THAN)
    {
        l2_absorb_expr_lshift_rshift_rshift_unsigned();
        l2_absorb_expr_gt_lt_ge_le1();
    }
    _elif_type (L2_TOKEN_LESS_THAN)
    {
        l2_absorb_expr_lshift_rshift_rshift_unsigned();
        l2_absorb_expr_gt_lt_ge_le1();
    }
    _end
}

void l2_absorb_expr_gt_lt_ge_le() {
    l2_absorb_expr_lshift_rshift_rshift_unsigned();
    l2_absorb_expr_gt_lt_ge_le1();
}

/* expr_lshift_rshift_rshift_unsigned ->
 * | expr_lshift_rshift_rshift_unsigned << expr_plus_sub
 * | expr_lshift_rshift_rshift_unsigned >> expr_plus_sub
 * | expr_lshift_rshift_rshift_unsigned >>> expr_plus_sub
 * | expr_plus_sub
 * */

/* eliminate left recursion */

/* expr_lshift_rshift_rshift_unsigned ->
 * | expr_plus_sub expr_lshift_rshift_rshift_unsigned1
 *
 * expr_lshift_rshift_rshift_unsigned1 ->
 * | << expr_plus_sub expr_lshift_rshift_rshift_unsigned1
 * | >> expr_plus_sub expr_lshift_rshift_rshift_unsigned1
 * | >>> expr_plus_sub expr_lshift_rshift_rshift_unsigned1
 * | nil
 *
 * */
void l2_absorb_expr_lshift_rshift_rshift_unsigned1() {
    _if_type (L2_TOKEN_LSHIFT)
    {
        l2_absorb_expr_plus_sub();
        l2_absorb_expr_lshift_rshift_rshift_unsigned1();
    }
    _elif_type (L2_TOKEN_RSHIFT)
    {
        l2_absorb_expr_plus_sub();
        l2_absorb_expr_lshift_rshift_rshift_unsigned1();
    }
    _elif_type (L2_TOKEN_RSHIFT_UNSIGNED)
    {
        l2_absorb_expr_plus_sub();
        l2_absorb_expr_lshift_rshift_rshift_unsigned1();
    }
    _end
}

void l2_absorb_expr_lshift_rshift_rshift_unsigned() {
    l2_absorb_expr_plus_sub();
    l2_absorb_expr_lshift_rshift_rshift_unsigned1();
}

/* expr_plus_sub ->
 * | expr_plus_sub + expr_mul_div_mod
 * | expr_plus_sub - expr_mul_div_mod
 * | expr_mul_div_mod
 * */

/* eliminate left recursion */

/* expr_plus_sub ->
 * | expr_mul_div_mod expr_plus_sub1
 *
 * expr_plus_sub1 ->
 * | + expr_mul_div_mod expr_plus_sub1
 * | - expr_mul_div_mod expr_plus_sub1
 * | nil
 *
 * */
void l2_absorb_expr_plus_sub1() {
    _if_type (L2_TOKEN_PLUS)
    {
        l2_absorb_expr_mul_div_mod();
        l2_absorb_expr_plus_sub1();
    }
    _elif_type (L2_TOKEN_SUB)
    {
        l2_absorb_expr_mul_div_mod();
        l2_absorb_expr_plus_sub1();
    }
    _end
}

void l2_absorb_expr_plus_sub() {
    l2_absorb_expr_mul_div_mod();
    l2_absorb_expr_plus_sub1();
}

/* expr_mul_div_mod ->
 * | expr_mul_div_mod * expr_single
 * | expr_mul_div_mod / expr_single
 * | expr_mul_div_mod % expr_single
 * | expr_single
 * */

/* eliminate left recursion */

/* expr_mul_div_mod ->
 * | expr_single expr_mul_div_mod1
 *
 * expr_mul_div_mod1 ->
 * | * expr_single expr_mul_div_mod1
 * | / expr_single expr_mul_div_mod1
 * | % expr_single expr_mul_div_mod1
 * | nil
 *
 * */
void l2_absorb_expr_mul_div_mod1() {
    _if_type (L2_TOKEN_MUL)
    {
        l2_absorb_expr_single();
        l2_absorb_expr_mul_div_mod1();
    }
    _elif_type (L2_TOKEN_DIV)
    {
        l2_absorb_expr_single();
        l2_absorb_expr_mul_div_mod1();
    }
    _elif_type (L2_TOKEN_MOD)
    {
        l2_absorb_expr_single();
        l2_absorb_expr_mul_div_mod1();
    }
    _end
}

void l2_absorb_expr_mul_div_mod() {
    l2_absorb_expr_single();
    l2_absorb_expr_mul_div_mod1();
}

/* expr_single ->
 * | ! expr_single
 * | ~ expr_single
 * | - expr_single (anywhere, -- > -)
 * | expr_atom
 *
 * */
void l2_absorb_expr_single() {
    _if_type (L2_TOKEN_LOGIC_NOT)
    {
        l2_absorb_expr_single();
    }
    _elif_type (L2_TOKEN_BIT_NOT)
    {
        l2_absorb_expr_single();
    }
    _elif_type (L2_TOKEN_SUB)
    {
        l2_absorb_expr_single();
    }
    _else
    {
        l2_absorb_expr_atom();
    }
}

/* real_param_list1 ->
 * | , id real_param_list1
 * | nil
 * */
void l2_absorb_real_param_list1() {
    _declr_current_token_p
    _if_type (L2_TOKEN_COMMA)
    {
        _if_type (L2_TOKEN_IDENTIFIER)
        {
            l2_absorb_real_param_list1();

        } _throw_unexpected_token

    } _end
}

/* real_param_list ->
 * | id real_param_list1
 * | nil
 *
 * */
void l2_absorb_real_param_list() {
    _if_type (L2_TOKEN_IDENTIFIER)
    {
        l2_absorb_real_param_list1();
    } _end
}

/* expr_atom ->
 * | ( expr )
 * | id ( real_param_list )
 * | // not implement // id expr_array
 * | id
 * | string_literal
 * | real_literal
 * | integer_literal
 * | keyword: true/false
 *
 * */
void l2_absorb_expr_atom() {
    _if_type (L2_TOKEN_LP)
    {
        l2_absorb_expr();
        _if_type (L2_TOKEN_RP)
        { }
        _throw_missing_rp
    }
    _elif_type (L2_TOKEN_IDENTIFIER)
    {
        _if_type (L2_TOKEN_LP)
        {
            l2_absorb_real_param_list();
            _if_type (L2_TOKEN_RP)
            { }
            _throw_missing_rp
        } _end
    }
    _elif_type (L2_TOKEN_INTEGER_LITERAL)
    { }
    _elif_type (L2_TOKEN_REAL_LITERAL)
    { }
    _elif_type (L2_TOKEN_STRING_LITERAL)
    { }
    _elif_keyword (L2_KW_TRUE)
    { }
    _elif_keyword (L2_KW_FALSE)
    { }
    _throw_unexpected_token
}
