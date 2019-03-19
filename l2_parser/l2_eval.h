#ifndef _L2_EVAL_H_
#define _L2_EVAL_H_

#include "../l2_lexer/l2_token_stream.h"
#include "l2_parse.h"
#include "l2_symbol_table.h"

typedef enum _l2_expr_val_type {
    L2_EXPR_VAL_NOT_EXPR, /* not expr */
    L2_EXPR_VAL_NO_VAL,

    L2_EXPR_VAL_TYPE_INTEGER,
    L2_EXPR_VAL_TYPE_REAL,
    L2_EXPR_VAL_TYPE_BOOL,

    L2_EXPR_VAL_TYPE_PROCEDURE
}l2_expr_val_type;

typedef struct _l2_expr_info {
    l2_expr_val_type val_type;
    union _val_union {
        boolean bool;
        double real;
        int integer;
        l2_procedure procedure;
    }val;
}l2_expr_info;

l2_expr_info l2_eval_expr(l2_scope *scope_p);
l2_expr_info l2_eval_expr_comma(l2_scope *scope_p);
l2_expr_info l2_eval_expr_assign(l2_scope *scope_p);
l2_expr_info l2_eval_expr_condition(l2_scope *scope_p);
l2_expr_info l2_eval_expr_logic_or(l2_scope *scope_p);
l2_expr_info l2_eval_expr_logic_and(l2_scope *scope_p);
l2_expr_info l2_eval_expr_bit_or(l2_scope *scope_p);
l2_expr_info l2_eval_expr_bit_xor(l2_scope *scope_p);
l2_expr_info l2_eval_expr_bit_and(l2_scope *scope_p);
l2_expr_info l2_eval_expr_eq_ne(l2_scope *scope_p);
l2_expr_info l2_eval_expr_gt_lt_ge_le(l2_scope *scope_p);
l2_expr_info l2_eval_expr_lshift_rshift_rshift_unsigned(l2_scope *scope_p);
l2_expr_info l2_eval_expr_plus_sub(l2_scope *scope_p);
l2_expr_info l2_eval_expr_mul_div_mod(l2_scope *scope_p);
l2_expr_info l2_eval_expr_single(l2_scope *scope_p);
l2_expr_info l2_eval_expr_atom(l2_scope *scope_p);

boolean l2_absorb_expr();
boolean l2_absorb_expr_comma();
boolean l2_absorb_expr_assign();
boolean l2_absorb_expr_condition();
boolean l2_absorb_expr_logic_or();
boolean l2_absorb_expr_logic_and();
boolean l2_absorb_expr_bit_or();
boolean l2_absorb_expr_bit_xor();
boolean l2_absorb_expr_bit_and();
boolean l2_absorb_expr_eq_ne();
boolean l2_absorb_expr_gt_lt_ge_le();
boolean l2_absorb_expr_lshift_rshift_rshift_unsigned();
boolean l2_absorb_expr_plus_sub();
boolean l2_absorb_expr_mul_div_mod();
boolean l2_absorb_expr_single();
boolean l2_absorb_expr_atom();

void l2_parse_real_param_list(l2_scope *scope_p, l2_vector *symbol_vec_p);
void l2_parse_real_param_list1(l2_scope *scope_p, l2_vector *symbol_vec_p);

boolean l2_eval_update_symbol_bool(l2_scope *scope_p, char *symbol_name, boolean bool);
boolean l2_eval_update_symbol_integer(l2_scope *scope_p, char *symbol_name, int integer);
boolean l2_eval_update_symbol_real(l2_scope *scope_p, char *symbol_name, double real);


#endif
