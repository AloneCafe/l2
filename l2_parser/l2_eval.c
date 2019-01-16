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

boolean l2_eval_update_symbol_string(l2_scope *scope_p, char *symbol_name, l2_string *string_p) {
    l2_symbol_node *symbol_node_p = l2_eval_get_symbol_node(scope_p, symbol_name);
    if (!symbol_node_p) return L2_FALSE;
    if (symbol_node_p->symbol.type == L2_SYMBOL_TYPE_STRING) /* if the symbol was a string before, make the gc ref_count of source string to decrease by 1 */
        l2_gc_decrease_ref_count(g_parser_p->gc_list_p, symbol_node_p->symbol.u.string.str_p);
    /* new string */
    symbol_node_p->symbol.type = L2_SYMBOL_TYPE_STRING;
    l2_string_create(&symbol_node_p->symbol.u.string);
    l2_string_strcpy(&symbol_node_p->symbol.u.string, string_p);
    return L2_TRUE;
}

l2_expr_info l2_eval_expr(l2_scope *scope_p) {
    return l2_eval_expr_comma(scope_p);
}

/* expr_comma ->
 * | expr_assign , expr_comma
 * | expr_assign
 * */
l2_expr_info l2_eval_expr_comma(l2_scope *scope_p) {
    l2_expr_info item_expr_p = l2_eval_expr_assign(scope_p);
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_COMMA)) {
        l2_parse_token_forward();
        return l2_eval_expr_comma(scope_p);
    } else {
        return item_expr_p;
    }
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

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_IDENTIFIER)) {
        l2_parse_token_forward();
        left_id_p = l2_parse_token_current();
        char *id_str_p = left_id_p->u.str.str_p;

        id_err_line = left_id_p->current_line;
        id_err_col = left_id_p->current_col;

        if (l2_parse_probe_next_token_by_type(L2_TOKEN_ASSIGN)) { /* = */
            l2_parse_token_forward();
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

                case L2_EXPR_VAL_TYPE_STRING: /* id = string */
                    symbol_updated = l2_eval_update_symbol_string(scope_p, id_str_p, &right_expr_info.val.str);
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
            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_DIV_ASSIGN)) { /* /= */
            l2_parse_token_forward();
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between string and integer");

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

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between string and real");

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

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "/=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_MUL_ASSIGN)) { /* *= */
            l2_parse_token_forward();
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between string and integer");

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

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between string and real");

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

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "*=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_MOD_ASSIGN)) { /* %= */
            l2_parse_token_forward();
            token_opr_p = l2_parse_token_current();
            opr_err_line = token_opr_p->current_line;
            opr_err_col = token_opr_p->current_col;

            right_expr_info = l2_eval_expr_assign(scope_p);
            left_symbol_p = l2_eval_get_symbol_node(scope_p, id_str_p);
            if (!left_symbol_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            switch (right_expr_info.val_type) {
                case L2_EXPR_VAL_TYPE_INTEGER:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between string and integer");

                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer % l2_eval_div_by_zero_filter(right_expr_info.val.integer, opr_err_line, opr_err_col));
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between real and integer");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between string and real");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between real and real");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "%=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_PLUS_ASSIGN)) { /* += */
            l2_parse_token_forward();
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
                        case L2_SYMBOL_TYPE_STRING:
                            l2_cast_decimal_to_str(right_expr_info.val.integer, buff_s);
                            l2_string_strcat_c(&left_symbol_p->symbol.u.string, buff_s);
                            res_expr_info.val.str = left_symbol_p->symbol.u.string;
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_STRING;
                            break;

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

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_cast_real_to_str(right_expr_info.val.real, buff_s);
                            l2_string_strcat_c(&left_symbol_p->symbol.u.string, buff_s);
                            res_expr_info.val.str = left_symbol_p->symbol.u.string;
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_STRING;
                            break;

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

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            if (right_expr_info.expr_type == L2_EXPR_TYPE_IN_GC) { /* gc */
                                l2_gc_decrease_ref_count(g_parser_p->gc_list_p, right_expr_info.val.str.str_p);
                            } /* otherwise, not gc */
                            l2_string_strcat(&left_symbol_p->symbol.u.string, &right_expr_info.val.str);
                            res_expr_info.val.str = left_symbol_p->symbol.u.string;
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_STRING;
                            break;

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_string_strcat_c(&left_symbol_p->symbol.u.string,  right_expr_info.val.bool ? "true" : "false" );
                            res_expr_info.val.str = left_symbol_p->symbol.u.string;
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_STRING;
                            break;

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "+=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_SUB_ASSIGN)) { /* -= */
            l2_parse_token_forward();
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
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between string and integer");

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

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between string and real");

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

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "-=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_LSHIFT_ASSIGN)) { /* <<= */
            l2_parse_token_forward();
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
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between string and integer");

                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer << right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between real and integer");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between string and real");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between real and real");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "<<=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_RSHIFT_ASSIGN)) { /* >>= */
            l2_parse_token_forward();
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
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between string and integer");

                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer >> right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between real and integer");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between string and real");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between real and real");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_RSHIFT_UNSIGNED_ASSIGN)) { /* >>>= */
            l2_parse_token_forward();
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
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between string and integer");

                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = ((unsigned int)left_symbol_p->symbol.u.integer) >> right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between real and integer");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between string and real");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between real and real");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, ">>>=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_AND_ASSIGN)) { /* &= */
            l2_parse_token_forward();
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
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between string and integer");

                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer & right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between real and integer");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between string and real");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between real and real");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "&=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_XOR_ASSIGN)) { /* ^= */
            l2_parse_token_forward();
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
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between string and integer");

                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer ^ right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between real and integer");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between string and real");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between real and real");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "^=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_OR_ASSIGN)) { /* |= */
            l2_parse_token_forward();
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
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between string and integer");

                        case L2_SYMBOL_TYPE_INTEGER:
                            symbol_updated = l2_eval_update_symbol_integer(scope_p, id_str_p, res_expr_info.val.integer = left_symbol_p->symbol.u.integer | right_expr_info.val.integer);
                            res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                            break;

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between bool and integer");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between real and integer");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between native pointer and integer");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_REAL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between string and real");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between integer and real");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between bool and real");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between real and real");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between native pointer and real");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_STRING:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between string and string");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between integer and string");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between bool and string");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between real and string");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between native pointer and string");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                case L2_EXPR_VAL_TYPE_BOOL:
                    switch (left_symbol_p->symbol.type) {
                        case L2_SYMBOL_TYPE_STRING:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between string and bool");

                        case L2_SYMBOL_TYPE_INTEGER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between integer and bool");

                        case L2_SYMBOL_TYPE_BOOL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between bool and bool");

                        case L2_SYMBOL_TYPE_REAL:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between real and bool");

                        case L2_SYMBOL_TYPE_NATIVE_POINTER:
                            l2_parsing_error(L2_PARSING_ERROR_INCOMPATIBLE_OPERATION, opr_err_line, opr_err_col, "|=", "between native pointer and bool");

                        default:
                            l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "illegal symbol type");
                    }
                    break;

                default:
                    l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
            }

            if (!symbol_updated) /* if update symbol failed */
                l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, id_err_line, id_err_col, left_id_p->u.str.str_p);

            res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
            return res_expr_info;

        } else { /* no match */
            l2_parse_token_back();
            /*return l2_eval_expr_condition(scope_p);*/
            return l2_eval_expr_atom(scope_p);
        }

    } else {
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
    l2_token *current_token_p;
    first_expr_info = l2_eval_expr_logic_or(scope_p);
    int second_pos;

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_QM)) {
        l2_parse_token_forward();
        //second_pos = l2_parse_token_stream_get_pos();

    } else {
        return first_expr_info;
    }

    if (first_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL) {
        if (first_expr_info.val.bool == L2_FALSE) { /* false */
            l2_absorb_expr(); /* absorb the second expr */
            if (l2_parse_probe_next_token_by_type(L2_TOKEN_COLON)) { /* eval the third expr */
                l2_parse_token_forward();
                return l2_eval_expr_condition(scope_p);

            } else {
                l2_parse_token_forward();
                current_token_p = l2_parse_token_current();
                l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_CONDITION_EXPRESSION, current_token_p->current_line, current_token_p->current_col);
            }

        } else { /* true */
            l2_expr_info second_expr_info = l2_eval_expr(scope_p); /* eval the second expr */
            if (l2_parse_probe_next_token_by_type(L2_TOKEN_COLON)) {
                l2_parse_token_forward();
                l2_absorb_expr_condition(); /* absorb the third expr */
                return second_expr_info;

            } else {
                l2_parse_token_forward();
                current_token_p = l2_parse_token_current();
                l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_CONDITION_EXPRESSION, current_token_p->current_line, current_token_p->current_col);
            }
        }

    } else {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        l2_parsing_error(L2_PARSING_ERROR_CONDITION_MUST_BE_A_BOOL_VALUE, current_token_p->current_line, current_token_p->current_col);
    }
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
    l2_token *current_token_p;
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LOGIC_OR)) {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_logic_and(scope_p);
        if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL) {
            if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL) {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
                new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
                new_left_expr_info.val.bool = (left_expr_info.val.bool || right_expr_info.val.bool);
                return l2_eval_expr_logic_or1(scope_p, new_left_expr_info);

            } else {
                l2_parsing_error(L2_PARSING_ERROR_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE, current_token_p->current_line, current_token_p->current_col, "||");
            }
        } else {
            l2_parsing_error(L2_PARSING_ERROR_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE, current_token_p->current_line, current_token_p->current_col, "||");
        }

    } else {
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
    l2_token *current_token_p;
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LOGIC_AND)) {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_bit_or(scope_p);
        if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL) {
            if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_BOOL) {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
                new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
                new_left_expr_info.val.bool = (left_expr_info.val.bool && right_expr_info.val.bool);
                return l2_eval_expr_logic_and1(scope_p, new_left_expr_info);

            } else {
                l2_parsing_error(L2_PARSING_ERROR_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE, current_token_p->current_line, current_token_p->current_col, "&&");
            }
        } else {
            l2_parsing_error(L2_PARSING_ERROR_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE, current_token_p->current_line, current_token_p->current_col, "&&");
        }

    } else {
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
    l2_token *current_token_p;
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_OR)) {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_bit_xor(scope_p);
        if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
            if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
                new_left_expr_info.val.integer = (left_expr_info.val.integer | right_expr_info.val.integer);
                return l2_eval_expr_bit_or1(scope_p, new_left_expr_info);

            } else {
                l2_parsing_error(L2_PARSING_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE, current_token_p->current_line, current_token_p->current_col, "|");
            }
        } else {
            l2_parsing_error(L2_PARSING_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE, current_token_p->current_line, current_token_p->current_col, "|");
        }

    } else {
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
    l2_token *current_token_p;
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_XOR)) {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_bit_and(scope_p);
        if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
            if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
                new_left_expr_info.val.integer = (left_expr_info.val.integer ^ right_expr_info.val.integer);
                return l2_eval_expr_bit_xor1(scope_p, new_left_expr_info);

            } else {
                l2_parsing_error(L2_PARSING_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE, current_token_p->current_line, current_token_p->current_col, "^");
            }
        } else {
            l2_parsing_error(L2_PARSING_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE, current_token_p->current_line, current_token_p->current_col, "^");
        }

    } else {
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
    l2_token *current_token_p;
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_AND)) {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_eq_ne(scope_p);
        if (left_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
            if (right_expr_info.val_type == L2_EXPR_VAL_TYPE_INTEGER) {
                new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
                new_left_expr_info.val.integer = (left_expr_info.val.integer & right_expr_info.val.integer);
                return l2_eval_expr_bit_and1(scope_p, new_left_expr_info);

            } else {
                l2_parsing_error(L2_PARSING_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE, current_token_p->current_line, current_token_p->current_col, "&");
            }
        } else {
            l2_parsing_error(L2_PARSING_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE, current_token_p->current_line, current_token_p->current_col, "&");
        }

    } else {
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
    l2_token *current_token_p;
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_EQUAL)) {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_gt_lt_ge_le(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer == right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        new_left_expr_info.val.bool = l2_string_equal(&left_expr_info.val.str, &right_expr_info.val.str);
                        break;

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "==", "real", "string");

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

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_NOT_EQUAL)) {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_gt_lt_ge_le(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer != right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        new_left_expr_info.val.bool = !l2_string_equal(&left_expr_info.val.str, &right_expr_info.val.str);
                        break;

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!=", "real", "string");

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

    } else {
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
    l2_token *current_token_p;

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_GREAT_EQUAL_THAN)) { /* >= */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_lshift_rshift_rshift_unsigned(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer >= right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">=", "real", "string");

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

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_GREAT_THAN)) { /* > */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_lshift_rshift_rshift_unsigned(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer > right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">", "real", "string");

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

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_LESS_EQUAL_THAN)) { /* <= */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_lshift_rshift_rshift_unsigned(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer <= right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<=", "real", "string");

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

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_LESS_THAN)) { /* < */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_lshift_rshift_rshift_unsigned(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.bool = (left_expr_info.val.integer < right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<", "real", "string");

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

    } else {
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
    l2_token *current_token_p;

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LSHIFT)) { /* << */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_plus_sub(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.integer = (left_expr_info.val.integer << right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "<<", "real", "string");

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

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_RSHIFT)) { /* >> */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_plus_sub(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.integer = (left_expr_info.val.integer >> right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>", "real", "string");

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

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_RSHIFT_UNSIGNED)) { /* >>> */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_plus_sub(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.integer = (((unsigned int)left_expr_info.val.integer) >> right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, ">>>", "real", "string");

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

    } else {
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
l2_expr_info l2_eval_expr_plus_sub1(l2_scope *scope_p, l2_expr_info left_expr_info) { /* TODO: complete this function */
    l2_expr_info right_expr_info, new_left_expr_info;
    l2_token *current_token_p;

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_PLUS)) {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_plus_sub(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER; /* TODO */

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_SUB)) {

    } else {
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
    l2_token *current_token_p;

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_MUL)) { /* * */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_single(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                        new_left_expr_info.val.integer = (left_expr_info.val.integer * right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "*", "real", "string");

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

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_DIV)) { /* / */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_single(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                        new_left_expr_info.val.integer = (left_expr_info.val.integer / right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "/", "real", "string");

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

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_MOD)) { /* % */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        right_expr_info = l2_eval_expr_single(scope_p);
        new_left_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        new_left_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (left_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        new_left_expr_info.val.integer = (left_expr_info.val.integer % right_expr_info.val.integer);
                        break;

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "integer", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "integer", "string");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "bool", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "bool", "real");

                    default:
                        l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
                }
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                switch (right_expr_info.val_type) {
                    case L2_EXPR_VAL_TYPE_INTEGER:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "string", "integer");

                    case L2_EXPR_VAL_TYPE_BOOL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "string", "bool");

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "string", "string");

                    case L2_EXPR_VAL_TYPE_REAL:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "string", "real");

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

                    case L2_EXPR_VAL_TYPE_STRING:
                        l2_parsing_error(L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "%", "real", "string");

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

    } else {
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
    l2_token *current_token_p;

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LOGIC_NOT)) { /* ! */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();

        right_expr_info = l2_eval_expr_single(scope_p);
        res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        switch (right_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!", "integer");

            case L2_EXPR_VAL_TYPE_BOOL:
                res_expr_info.val.bool = !(right_expr_info.val.bool);
                break;

            case L2_EXPR_VAL_TYPE_STRING:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!", "string");

            case L2_EXPR_VAL_TYPE_REAL:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "!", "real");

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return res_expr_info;

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_NOT)) { /* ~ */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();

        right_expr_info = l2_eval_expr_single(scope_p);
        res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        switch (right_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                res_expr_info.val.integer = ~(right_expr_info.val.integer);
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "~", "bool");

            case L2_EXPR_VAL_TYPE_STRING:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "~", "string");

            case L2_EXPR_VAL_TYPE_REAL:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "~", "real");

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return res_expr_info;

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_SUB)) { /* - */
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();

        right_expr_info = l2_eval_expr_single(scope_p);
        res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;
        switch (right_expr_info.val_type) {
            case L2_EXPR_VAL_TYPE_INTEGER:
                res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
                res_expr_info.val.integer = -(right_expr_info.val.integer);
                break;

            case L2_EXPR_VAL_TYPE_BOOL:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "-", "bool");

            case L2_EXPR_VAL_TYPE_STRING:
                l2_parsing_error(L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_IMCOMPATIBLE_TYPE, current_token_p->current_line, current_token_p->current_col, "-", "string");

            case L2_EXPR_VAL_TYPE_REAL:
                res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
                res_expr_info.val.real = -(right_expr_info.val.real);
                break;
            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }
        return res_expr_info;

    } else {
        return l2_eval_expr_atom(scope_p);
    }
}

/* expr_atom ->
 * | id
 * | //not implement// id ( real_param_list )
 * | //not implement// id [ expr ]
 * | integer_literal
 * | string_literal
 * | real_literal
 * | keyword: true / false
 * |
 *
 * */
l2_expr_info l2_eval_expr_atom(l2_scope *scope_p) {

    l2_token *token_first_p;
    l2_symbol_node *symbol_node_p;
    l2_expr_info res_expr_info;

    if (l2_parse_probe_next_token_by_type(L2_TOKEN_IDENTIFIER)) {
        l2_parse_token_forward();
        token_first_p = l2_parse_token_current();
        char *id_str_p = token_first_p->u.str.str_p;
        symbol_node_p = l2_eval_get_symbol_node(scope_p, id_str_p);
        if (!symbol_node_p) l2_parsing_error(L2_PARSING_ERROR_IDENTIFIER_UNDEFINED, token_first_p->current_line, token_first_p->current_col, token_first_p->u.str.str_p);

        switch (symbol_node_p->symbol.type) { /* package symbol into expr node */
            case L2_SYMBOL_TYPE_STRING:
                res_expr_info.val_type = L2_EXPR_VAL_TYPE_STRING;
                res_expr_info.val.str = symbol_node_p->symbol.u.string;
                break;

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

            case L2_SYMBOL_TYPE_NATIVE_POINTER:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an native pointer (package algorithm is not implement yet)");

            default:
                l2_internal_error(L2_INTERNAL_ERROR_UNREACHABLE_CODE, "eval return an error expression val-type");
        }

        res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_INTEGER_LITERAL)) {
        l2_parse_token_forward();
        token_first_p = l2_parse_token_current();
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_INTEGER;
        res_expr_info.val.integer = token_first_p->u.integer;
        res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_STRING_LITERAL)) {
        l2_parse_token_forward();
        token_first_p = l2_parse_token_current();
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_STRING;
        l2_string_create(&res_expr_info.val.str);
        l2_string_strcpy(&res_expr_info.val.str, &token_first_p->u.str);
        res_expr_info.expr_type = L2_EXPR_TYPE_IN_GC; /* string into gc */
        l2_gc_append(g_parser_p->gc_list_p, res_expr_info.val.str.str_p);

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_REAL_LITERAL)) {
        l2_parse_token_forward();
        token_first_p = l2_parse_token_current();
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_REAL;
        res_expr_info.val.real = token_first_p->u.real;
        res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;

    } else if (l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_KEYWORD, g_l2_token_keywords[0])) { /* "true" */
        l2_parse_token_forward();
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        res_expr_info.val.bool = L2_TRUE;
        res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;

    } else if (l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_KEYWORD, g_l2_token_keywords[1])) { /* "false" */
        l2_parse_token_forward();
        res_expr_info.val_type = L2_EXPR_VAL_TYPE_BOOL;
        res_expr_info.val.bool = L2_FALSE;
        res_expr_info.expr_type = L2_EXPR_TYPE_NOT_IN_GC;

    } else {
        l2_parse_token_forward();
        token_first_p = l2_parse_token_current();
        l2_parsing_error(L2_PARSING_ERROR_UNEXPECTED_TOKEN, token_first_p->current_line, token_first_p->current_col, token_first_p);
    }


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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_COMMA)) {
        l2_parse_token_forward();
        l2_absorb_expr_assign();
        l2_absorb_expr_comma1();
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_IDENTIFIER)) {
        l2_parse_token_forward();
        if (l2_parse_probe_next_token_by_type(L2_TOKEN_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_DIV_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_MUL_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_MOD_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_PLUS_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_SUB_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_LSHIFT_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_RSHIFT_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_RSHIFT_UNSIGNED_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_AND_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_XOR_ASSIGN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_OR_ASSIGN)
        ) {
            l2_parse_token_forward();
            l2_absorb_expr_assign();
        } else {
            l2_parse_token_back();
            l2_absorb_expr_condition();
        }

    } else {
        l2_absorb_expr_condition();
    }
}

/* expr_condition ->
 * | expr_logic_or ? expr : expr_condition
 * | expr_logic_or
 * */
void l2_absorb_expr_condition() {
    l2_token *current_token_p;

    l2_absorb_expr_logic_or();
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_QM)) {
        l2_parse_token_forward();
        l2_absorb_expr();
        if (l2_parse_probe_next_token_by_type(L2_TOKEN_COLON)) {
            l2_parse_token_forward();
            l2_absorb_expr_condition();

        } else {
            l2_parse_token_forward();
            current_token_p = l2_parse_token_current();
            l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_CONDITION_EXPRESSION, current_token_p->current_line, current_token_p->current_col);
        }
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LOGIC_OR)) {
        l2_parse_token_forward();
        l2_absorb_expr_logic_and();
        l2_absorb_expr_logic_or1();
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LOGIC_AND)) {
        l2_parse_token_forward();
        l2_absorb_expr_bit_or();
        l2_absorb_expr_logic_and1();
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_OR)) {
        l2_parse_token_forward();
        l2_absorb_expr_bit_xor();
        l2_absorb_expr_bit_or1();
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_XOR)) {
        l2_parse_token_forward();
        l2_absorb_expr_bit_and();
        l2_absorb_expr_bit_xor1();
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_AND)) {
        l2_parse_token_forward();
        l2_absorb_expr_eq_ne();
        l2_absorb_expr_bit_and1();
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_EQUAL)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_NOT_EQUAL))
    {
        l2_parse_token_forward();
        l2_absorb_expr_gt_lt_ge_le();
        l2_absorb_expr_eq_ne1();

    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_GREAT_EQUAL_THAN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_GREAT_THAN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_LESS_EQUAL_THAN)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_LESS_THAN))
    {
        l2_parse_token_forward();
        l2_absorb_expr_lshift_rshift_rshift_unsigned();
        l2_absorb_expr_gt_lt_ge_le1();

    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LSHIFT)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_RSHIFT)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_RSHIFT_UNSIGNED))
    {
        l2_parse_token_forward();
        l2_absorb_expr_plus_sub();
        l2_absorb_expr_lshift_rshift_rshift_unsigned1();
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_PLUS)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_SUB))
    {
        l2_parse_token_forward();
        l2_absorb_expr_mul_div_mod();
        l2_absorb_expr_plus_sub1();
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_MUL)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_DIV)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_MOD))
    {
        l2_parse_token_forward();
        l2_absorb_expr_single();
        l2_absorb_expr_mul_div_mod1();
    }
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
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LOGIC_NOT)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_BIT_NOT)
        || l2_parse_probe_next_token_by_type(L2_TOKEN_SUB))
    {
        l2_parse_token_forward();
        l2_absorb_expr_single();

    } else {
        l2_absorb_expr_atom();
    }
}

/* expr_atom ->
 * | ( expr )
 * | // not implement // id ( real_param_list )
 * | // not implement // id expr_array
 * | id
 * | string_literal
 * | real_literal
 * | integer_literal
 * | keyword: true/false
 *
 * */
void l2_absorb_expr_atom() {
    l2_token *current_token_p;
    if (l2_parse_probe_next_token_by_type(L2_TOKEN_LP)) {
        l2_parse_token_forward();

        l2_absorb_expr();
        if (l2_parse_probe_next_token_by_type(L2_TOKEN_RP)) {
            l2_parse_token_forward();

        } else {
            l2_parse_token_forward();
            current_token_p = l2_parse_token_current();
            l2_parsing_error(L2_PARSING_ERROR_MAY_LOSE_RIGHT_PAIR_OF_PARENTHESES, current_token_p->current_line, current_token_p->current_col);
        }

    } else if (l2_parse_probe_next_token_by_type(L2_TOKEN_IDENTIFIER)
           || l2_parse_probe_next_token_by_type(L2_TOKEN_INTEGER_LITERAL)
           || l2_parse_probe_next_token_by_type(L2_TOKEN_REAL_LITERAL)
           || l2_parse_probe_next_token_by_type(L2_TOKEN_STRING_LITERAL)
           || l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_KEYWORD, g_l2_token_keywords[0]) /* "true" */
           || l2_parse_probe_next_token_by_type_and_str(L2_TOKEN_KEYWORD, g_l2_token_keywords[1])) /* "false" */
    {
        l2_parse_token_forward();

    } else {
        l2_parse_token_forward();
        current_token_p = l2_parse_token_current();
        l2_parsing_error(L2_PARSING_ERROR_UNEXPECTED_TOKEN, current_token_p->current_line, current_token_p->current_col, current_token_p);
    }
}
