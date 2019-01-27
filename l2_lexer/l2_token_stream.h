#ifndef _L2_TOKEN_STREAM_H_
#define _L2_TOKEN_STREAM_H_

#include "../l2_base/l2_string.h"
#include "../l2_base/l2_vector.h"
#include "l2_char_stream.h"

typedef enum _l2_token_type {
    L2_TOKEN_TERMINATOR,
    L2_TOKEN_KEYWORD, /* keyword */
    L2_TOKEN_IDENTIFIER, /* identifier */
    //L2_TOKEN_DEC_NUMBER_LITERAL, /* decimal number */
    //L2_TOKEN_OCT_NUMBER_LITERAL, /* octal number */
    //L2_TOKEN_HEX_NUMBER_LITERAL, /* hexadecimal number */
    L2_TOKEN_INTEGER_LITERAL,
    L2_TOKEN_REAL_LITERAL,
    //L2_TOKEN_CHARACTER_LITERAL, /* 'c' character literal */
    L2_TOKEN_STRING_LITERAL, /* "xxxx" string literal */
    L2_TOKEN_LBRACE, /* { */
    L2_TOKEN_RBRACE, /* } */
    L2_TOKEN_LP, /* ( */
    L2_TOKEN_RP, /* ) */
    L2_TOKEN_LBRACKET, /* [ */
    L2_TOKEN_RBRACKET, /* ] */
    L2_TOKEN_DOT, /* . */
    L2_TOKEN_LOGIC_NOT, /* ! */
    L2_TOKEN_BIT_NOT, /* ~ */
    L2_TOKEN_MUL, /* * */
    L2_TOKEN_DIV, /* / */
    L2_TOKEN_MOD, /* % */
    L2_TOKEN_PLUS, /* + */
    L2_TOKEN_INC, /* ++ */
    L2_TOKEN_SUB, /* - */
    L2_TOKEN_DEC, /* -- */
    L2_TOKEN_RSHIFT, /* >> */
    L2_TOKEN_RSHIFT_UNSIGNED, /* >>> */
    L2_TOKEN_LSHIFT, /* << */
    L2_TOKEN_GREAT_THAN, /* > */
    L2_TOKEN_GREAT_EQUAL_THAN, /* >= */
    L2_TOKEN_LESS_THAN, /* < */
    L2_TOKEN_LESS_EQUAL_THAN, /* <= */
    L2_TOKEN_EQUAL, /* == */
    L2_TOKEN_NOT_EQUAL, /* != */
    L2_TOKEN_BIT_AND, /* & */
    L2_TOKEN_BIT_XOR, /* ^ */
    L2_TOKEN_BIT_OR, /* | */
    L2_TOKEN_LOGIC_AND, /* && */
    L2_TOKEN_LOGIC_OR, /* || */
    L2_TOKEN_QM, /* ? */
    L2_TOKEN_COLON, /* : */
    L2_TOKEN_ASSIGN, /* = */
    L2_TOKEN_PLUS_ASSIGN, /* += */
    L2_TOKEN_SUB_ASSIGN, /* -= */
    L2_TOKEN_MUL_ASSIGN, /* *= */
    L2_TOKEN_DIV_ASSIGN, /* /= */
    L2_TOKEN_MOD_ASSIGN, /* %= */
    L2_TOKEN_RSHIFT_ASSIGN, /* >>= */
    L2_TOKEN_RSHIFT_UNSIGNED_ASSIGN, /* >>>= */
    L2_TOKEN_LSHIFT_ASSIGN, /* <<= */
    L2_TOKEN_BIT_AND_ASSIGN, /* &= */
    L2_TOKEN_BIT_XOR_ASSIGN, /* ^= */
    L2_TOKEN_BIT_OR_ASSIGN, /* |= */
    L2_TOKEN_COMMA, /* , */
    L2_TOKEN_SEMICOLON /* ; */

}l2_token_type;

typedef enum _l2_keywords {
    L2_KW_TRUE,
    L2_KW_FALSE,
    L2_KW_VAR,
    L2_KW_IF,
    L2_KW_ELSE,
    L2_KW_ELIF,
    L2_KW_WHILE,
    L2_KW_DO,
    L2_KW_FOR,
    L2_KW_BREAK,
    L2_KW_CONTINUE,
    L2_KW_RETURN,
    L2_KW_PROCEDURE

}l2_keyword;

typedef struct _l2_token {
    l2_token_type type;
    union {
        char *c_str;
        l2_string str;
        double real;
        int integer;
    }u;
    int current_pos_at_stream;
    int current_line;
    int current_col;
}l2_token;

typedef struct _l2_token_stream {
    l2_vector token_vector;
    int token_vector_current_pos;
    l2_char_stream *char_stream_p;
}l2_token_stream;

l2_token_stream *l2_token_stream_create(FILE *fp);
void l2_token_stream_destroy(l2_token_stream *token_stream_p);

l2_token *l2_token_stream_next_token(l2_token_stream *token_stream_p);
l2_token *l2_token_stream_current_token(l2_token_stream *token_stream_p);
int l2_token_stream_get_pos(l2_token_stream *token_stream_p);
void l2_token_stream_set_pos(l2_token_stream *token_stream_p, int pos);
void l2_token_stream_rollback(l2_token_stream *token_stream_p);

char *l2_token_stream_str_keyword(l2_string *str_p);


#endif