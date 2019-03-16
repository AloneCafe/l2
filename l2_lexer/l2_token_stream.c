#include "stdlib.h"
#include "l2_token_stream.h"
#include "../l2_system/l2_error.h"
#include "../l2_system/l2_assert.h"
#include "../l2_system/l2_warning.h"
#include "l2_cast.h"

char *g_l2_token_keywords[] = {
        "true",
        "false",
        "var",
        "if",
        "else",
        "elif",
        "while",
        "do",
        "for",
        "break",
        "continue",
        "return",
        "procedure",
        "eval"
};

l2_token_stream *l2_token_stream_create(FILE *fp) {
    l2_token_stream *token_stream_p = malloc(sizeof(l2_token_stream));
    l2_assert(token_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_vector_create(&token_stream_p->token_vector, sizeof(l2_token));
    token_stream_p->token_vector_current_pos = 0;
    token_stream_p->char_stream_p = l2_char_stream_create(fp);
    return token_stream_p;
}

void l2_token_stream_destroy(l2_token_stream *token_stream_p) {
    l2_assert(token_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    /*
    int i;
    l2_token *tp;
    for (i = 0; i < token_stream_p->token_vector.size; i++) {
        tp = (l2_token *)l2_vector_at(&token_stream_p->token_vector, i);
        l2_assert(tp, L2_INTERNAL_ERROR_NULL_POINTER);
        if (tp->type == L2_TOKEN_IDENTIFIER || tp->type == L2_TOKEN_STRING_LITERAL)
            l2_string_destroy(&tp->u.str);
    }
    */
    l2_vector_destroy(&token_stream_p->token_vector);
    token_stream_p->token_vector_current_pos = 0;
    l2_char_stream_destroy(token_stream_p->char_stream_p);
    free(token_stream_p);
}

l2_token *l2_token_stream_next_token(l2_token_stream *token_stream_p) {
    l2_assert(token_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);

    if (token_stream_p->token_vector_current_pos < token_stream_p->token_vector.size) {
        token_stream_p->token_vector_current_pos += 1;
        return (l2_token *)l2_vector_at(&token_stream_p->token_vector, token_stream_p->token_vector_current_pos - 1);
    }


    l2_token t = { 0 };
    int fa_state = 0x0;
    char ch = 0;
    l2_string token_str_buff;
    l2_string_create(&token_str_buff);

    while(1) {
        ch = l2_char_stream_next_char(token_stream_p->char_stream_p);

        int lines = token_stream_p->char_stream_p->lines;
        int cols = token_stream_p->char_stream_p->cols;

        switch (fa_state) {
            case 0x0:
                t.current_col = cols;
                t.current_line = lines;
                t.current_pos_at_stream = token_stream_p->char_stream_p->chars_vector_current_pos;

                if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_') {
                    t.type = L2_TOKEN_IDENTIFIER;
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x1;

                } else if (ch == '0') {
                    t.type = L2_TOKEN_INTEGER_LITERAL;
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x2;

                } else if (ch >= '1' && ch <= '9') {
                    t.type = L2_TOKEN_INTEGER_LITERAL;
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x3;

                } else if (ch == '\'') {
                    t.type = L2_TOKEN_INTEGER_LITERAL;
                    fa_state = 0x4;

                } else if (ch == '\"') {
                    t.type = L2_TOKEN_STRING_LITERAL;
                    fa_state = 0x5;

                } else if (ch == '{') {
                    t.type = L2_TOKEN_LBRACE;
                    goto ret;

                } else if (ch == '}') {
                    t.type = L2_TOKEN_RBRACE;
                    goto ret;

                } else if (ch == '(') {
                    t.type = L2_TOKEN_LP;
                    goto ret;

                } else if (ch == ')') {
                    t.type = L2_TOKEN_RP;
                    goto ret;

                } else if (ch == '[') {
                    t.type = L2_TOKEN_LBRACKET;
                    goto ret;

                } else if (ch == ']') {
                    t.type = L2_TOKEN_RBRACKET;
                    goto ret;

                } else if (ch == '.') {
                    t.type = L2_TOKEN_DOT;
                    goto ret;

                } else if (ch == '!') {
                    t.type = L2_TOKEN_LOGIC_NOT;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '=') {
                        t.type = L2_TOKEN_NOT_EQUAL;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '~') {
                    t.type = L2_TOKEN_BIT_NOT;
                    goto ret;

                } else if (ch == '*') {
                    t.type = L2_TOKEN_MUL;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '=') {
                        t.type = L2_TOKEN_MUL_ASSIGN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '/') {
                    t.type = L2_TOKEN_DIV;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '=') {
                        t.type = L2_TOKEN_DIV_ASSIGN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '%') {
                    t.type = L2_TOKEN_MOD;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '=') {
                        t.type = L2_TOKEN_MOD_ASSIGN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '+') {
                    t.type = L2_TOKEN_PLUS;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '+') {
                        t.type = L2_TOKEN_INC;
                    } else if (ch == '=') {
                        t.type = L2_TOKEN_PLUS_ASSIGN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '-') {
                    t.type = L2_TOKEN_SUB;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '-') {
                        t.type = L2_TOKEN_DEC;
                    } else if (ch == '=') {
                        t.type = L2_TOKEN_SUB_ASSIGN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '>') {
                    t.type = L2_TOKEN_GREAT_THAN;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '>') {
                        t.type = L2_TOKEN_RSHIFT;
                        ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                        if (ch == '>') {
                            t.type = L2_TOKEN_RSHIFT_UNSIGNED;
                            ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                            if (ch == '=') {
                                t.type = L2_TOKEN_RSHIFT_UNSIGNED_ASSIGN;
                            } else {
                                l2_char_stream_rollback(token_stream_p->char_stream_p);
                            }
                        } else if (ch == '=') {
                            t.type = L2_TOKEN_RSHIFT_ASSIGN;
                        } else {
                            l2_char_stream_rollback(token_stream_p->char_stream_p);
                        }
                    } else if (ch == '=') {
                        t.type = L2_TOKEN_GREAT_EQUAL_THAN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '<') {
                    t.type = L2_TOKEN_LESS_THAN;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '<') {
                        t.type = L2_TOKEN_LSHIFT;
                        ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                        if (ch == '=') {
                            t.type = L2_TOKEN_LSHIFT_ASSIGN;
                        } else {
                            l2_char_stream_rollback(token_stream_p->char_stream_p);
                        }
                    } else if (ch == '=') {
                        t.type = L2_TOKEN_LESS_EQUAL_THAN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '=') {
                    t.type = L2_TOKEN_ASSIGN;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '=') {
                        t.type = L2_TOKEN_EQUAL;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '&') {
                    t.type = L2_TOKEN_BIT_AND;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '&') {
                        t.type = L2_TOKEN_LOGIC_AND;
                    } else if (ch == '=') {
                        t.type = L2_TOKEN_BIT_AND_ASSIGN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '^') {
                    t.type = L2_TOKEN_BIT_XOR;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '=') {
                        t.type = L2_TOKEN_BIT_XOR_ASSIGN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '|') {
                    t.type = L2_TOKEN_BIT_OR;
                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if (ch == '|') {
                        t.type = L2_TOKEN_LOGIC_OR;
                    } else if (ch == '=') {
                        t.type = L2_TOKEN_BIT_OR_ASSIGN;
                    } else {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                    }
                    goto ret;

                } else if (ch == '?') {
                    t.type = L2_TOKEN_QM;
                    goto ret;

                } else if (ch == ':') {
                    t.type = L2_TOKEN_COLON;
                    goto ret;

                } else if (ch == ',') {
                    t.type = L2_TOKEN_COMMA;
                    goto ret;

                } else if (ch == ';') {
                    t.type = L2_TOKEN_SEMICOLON;
                    goto ret;

                } else if (l2_char_is_blank(ch)) {

                } else if (ch == L2_EOF) {
                    t.type = L2_TOKEN_TERMINATOR;
                    goto ret;

                } else {
                    l2_parsing_error(L2_PARSING_ERROR_ILLEGAL_CHARACTER, lines, cols, ch);

                }

                break;

            case 0x1: /* handle identifier or keyword */
                if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_' || (ch >= '0' && ch <= '9')) {
                    l2_string_push_char(&token_str_buff, ch);

                } else {
                    l2_char_stream_rollback(token_stream_p->char_stream_p);
                    char *keyword_p = l2_token_stream_str_keyword(&token_str_buff);
                    if (keyword_p) {
                        t.type = L2_TOKEN_KEYWORD;
                        //l2_string_destroy(&token_str_buff);
                        t.u.c_str = keyword_p;
                    } else {
                        t.type = L2_TOKEN_IDENTIFIER;
                        l2_string_create(&t.u.str);
                        l2_string_strcpy(&t.u.str, &token_str_buff);
                    }
                    goto ret;

                }
                break;

            case 0x2: /* handle oct and hex number literal ( begin with 0 ) */
                if (ch == 'X' || ch == 'x') {
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x20;

                } else if (ch >= '0' && ch <= '9') {
                    if (ch >= '8') {
                        l2_parsing_error(L2_PARSING_ERROR_ILLEGAL_NUMBER_IN_AN_OCTAL_LITERAL, lines, cols, ch);
                    }
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x21;

                } else { /* get 0 */
                    t.u.integer = 0;
                    l2_char_stream_rollback(token_stream_p->char_stream_p);
                    goto ret;

                }
                break;

            case 0x20: /* handle hex number literal */
                if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x200;

                } else {
                    l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_HEX_LITERAL, lines, cols);

                }
                break;

            case 0x200: /* handle hex number literal */
                if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
                    l2_string_push_char(&token_str_buff, ch);

                } else {
                    t.u.integer = l2_cast_hex_str_to_int(&(l2_string_get_str_p(&token_str_buff)[2]));
                    l2_char_stream_rollback(token_stream_p->char_stream_p);
                    goto ret;

                }
                break;

            case 0x21: /* handle oct number literal */
                if (ch >= '0' && ch <= '9') {
                    if (ch >= '8') {
                        l2_parsing_error(L2_PARSING_ERROR_ILLEGAL_NUMBER_IN_AN_OCTAL_LITERAL, lines, cols, ch);
                    }
                    l2_string_push_char(&token_str_buff, ch);

                } else {
                    t.u.integer = l2_cast_octal_str_to_int(&(l2_string_get_str_p(&token_str_buff)[1]));
                    l2_char_stream_rollback(token_stream_p->char_stream_p);
                    goto ret;

                }
                break;

            case 0x3: /* handle dec number literal ( begin with 1 ~ 9 ) */
                if (ch >= '0' && ch <= '9') {
                    l2_string_push_char(&token_str_buff, ch);

                } else if (ch == '.') {
                    t.type = L2_TOKEN_REAL_LITERAL;
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x31;

                } else {
                    t.u.integer = l2_cast_decimal_str_to_int(l2_string_get_str_p(&token_str_buff));
                    l2_char_stream_rollback(token_stream_p->char_stream_p);
                    goto ret;
                }
                break;

            case 0x31: /* handle real number literal ( begin with 1 ~ 9 ) */
                if (ch >= '0' && ch <= '9') {
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x32;

                } else {
                    l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_REAL_LITERAL, lines, cols);
                }
                break;

            case 0x32: /* handle real number literal ( begin with 1 ~ 9 ) */
                if (ch >= '0' && ch <= '9') {
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x32;

                } else {
                    t.u.real = l2_cast_real_str_to_int(l2_string_get_str_p(&token_str_buff));
                    l2_char_stream_rollback(token_stream_p->char_stream_p);
                    goto ret;
                }
                break;

            case 0x4: /* handle character literal */
                if (ch == '\'') {
                    l2_parsing_error(L2_PARSING_ERROR_EMPTY_CHARACTER_LITERAL, lines, cols);

                } else if (ch == '\\') {
                    fa_state = 0x40;

                } else if (ch == L2_EOF) {
                    l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_CHARACTER_LITERAL, lines, cols);

                } else {
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x400;

                }
                break;

            case 0x40: /* handle escape character */
                if (ch == 'a') {
                    l2_string_push_char(&token_str_buff, '\a');

                } else if (ch == 'b') {
                    l2_string_push_char(&token_str_buff, '\b');

                } else if (ch == 'f') {
                    l2_string_push_char(&token_str_buff, '\f');

                } else if (ch == 'n') {
                    l2_string_push_char(&token_str_buff, '\n');

                } else if (ch == 'r') {
                    l2_string_push_char(&token_str_buff, '\r');

                } else if (ch == 't') {
                    l2_string_push_char(&token_str_buff, '\t');

                } else if (ch == 'v') {
                    l2_string_push_char(&token_str_buff, '\v');

                } else if (ch == '\\') {
                    l2_string_push_char(&token_str_buff, '\\');

                } else if (ch == '\'') {
                    l2_string_push_char(&token_str_buff, '\'');

                } else if (ch == '\"') {
                    l2_string_push_char(&token_str_buff, '\"');

                } else if (ch == '?') {
                    l2_string_push_char(&token_str_buff, '\?');

                } else if (ch >= '0' && ch <= '7') {
                    char seq[4];
                    int i;
                    for (i = 1; i < 3; i++) {
                        ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                        if (ch >= '0' && ch <= '7') {
                            seq[i] = ch;
                        } else {
                            seq[i] = '\0';
                            l2_char_stream_rollback(token_stream_p->char_stream_p);
                            break;
                        }
                    }
                    seq[i] = '\0';
                    l2_string_push_char(&token_str_buff, (char)l2_cast_octal_str_to_int(seq));
                    fa_state = 0x400;
                    break;

                } else if (ch == 'x') {
                    char seq[3];
                    seq[0] = ch = l2_char_stream_next_char(token_stream_p->char_stream_p); seq[1] = '\0';
                    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                        l2_parsing_warning(L2_PARSING_WARNING_UNKNOWN_ESCAPE_SEQUENCE, lines, cols, seq);
                        l2_string_push_char(&token_str_buff, seq[0]);
                    }

                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
                        seq[1] = ch;
                    } else {
                        seq[1] = '\0';
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                        break;
                    }
                    seq[2] = '\0';
                    l2_string_push_char(&token_str_buff, (char)l2_cast_hex_str_to_int(seq));
                    fa_state = 0x400;
                    break;

                } else if (ch == L2_EOF) {
                    l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_CHARACTER_LITERAL, lines, cols);

                } else {
                    char seq[2];
                    seq[0] = ch; seq[1] = '\0';
                    l2_parsing_warning(L2_PARSING_WARNING_UNKNOWN_ESCAPE_SEQUENCE, lines, cols, seq);
                    l2_string_push_char(&token_str_buff, ch);
                }

                fa_state = 0x400;

                break;

            case 0x400:
                if (ch == '\'') {
                    t.u.integer = l2_string_get_str_p(&token_str_buff)[0];
                    goto ret;

                } else if (ch == L2_EOF) {
                    l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_CHARACTER_LITERAL, lines, cols);

                } else {
                    l2_parsing_error(L2_PARSING_ERROR_MULTI_CHARACTER_IN_SINGLE_CHARACTER_LITERAL, lines, cols);

                }
                break;

            case 0x5: /* handle string literal */
                if (ch == '\"') {
                    l2_string_push_char(&token_str_buff, '\0');
                    goto ret;

                } else if (ch == '\\') {
                    fa_state = 0x50;

                } else if (ch == L2_EOF) {
                    l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_STRING_LITERAL, lines, cols);

                } else {
                    l2_string_push_char(&token_str_buff, ch);
                    fa_state = 0x500;
                }
                break;

            case 0x50: /* handle escape character in string literal */
                if (ch == 'a') {
                    l2_string_push_char(&token_str_buff, '\a');

                } else if (ch == 'b') {
                    l2_string_push_char(&token_str_buff, '\b');

                } else if (ch == 'f') {
                    l2_string_push_char(&token_str_buff, '\f');

                } else if (ch == 'n') {
                    l2_string_push_char(&token_str_buff, '\n');

                } else if (ch == 'r') {
                    l2_string_push_char(&token_str_buff, '\r');

                } else if (ch == 't') {
                    l2_string_push_char(&token_str_buff, '\t');

                } else if (ch == 'v') {
                    l2_string_push_char(&token_str_buff, '\v');

                } else if (ch == '\\') {
                    l2_string_push_char(&token_str_buff, '\\');

                } else if (ch == '\'') {
                    l2_string_push_char(&token_str_buff, '\'');

                } else if (ch == '\"') {
                    l2_string_push_char(&token_str_buff, '\"');

                } else if (ch == '?') {
                    l2_string_push_char(&token_str_buff, '\?');

                } else if (ch >= '0' && ch <= '7') {
                    char seq[4];
                    int i;
                    for (i = 1; i < 3; i++) {
                        ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                        if (ch >= '0' && ch <= '7') {
                            seq[i] = ch;
                        } else {
                            seq[i] = '\0';
                            l2_char_stream_rollback(token_stream_p->char_stream_p);
                            break;
                        }
                    }
                    seq[i] = '\0';
                    l2_string_push_char(&token_str_buff, (char)l2_cast_octal_str_to_int(seq));
                    fa_state = 0x500;
                    break;

                } else if (ch == 'x') {
                    char seq[3];
                    seq[0] = ch = l2_char_stream_next_char(token_stream_p->char_stream_p); seq[1] = '\0';
                    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) {
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                        l2_parsing_warning(L2_PARSING_WARNING_UNKNOWN_ESCAPE_SEQUENCE, lines, cols, seq);
                        l2_string_push_char(&token_str_buff, seq[0]);
                    }

                    ch = l2_char_stream_next_char(token_stream_p->char_stream_p);
                    if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
                        seq[1] = ch;
                    } else {
                        seq[1] = '\0';
                        l2_char_stream_rollback(token_stream_p->char_stream_p);
                        l2_string_push_char(&token_str_buff, (char)l2_cast_hex_str_to_int(seq));
                        fa_state = 0x500;
                        break;
                    }
                    seq[2] = '\0';
                    l2_string_push_char(&token_str_buff, (char)l2_cast_hex_str_to_int(seq));
                    fa_state = 0x500;
                    break;

                } else if (ch == L2_EOF) {
                    l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_STRING_LITERAL, lines, cols);

                } else {
                    char seq[2];
                    seq[0] = ch; seq[1] = '\0';
                    l2_parsing_warning(L2_PARSING_WARNING_UNKNOWN_ESCAPE_SEQUENCE, lines, cols, seq);
                    l2_string_push_char(&token_str_buff, ch);
                }

                fa_state = 0x500;
                break;

            case 0x500:
                if (ch == '\"') {
                    l2_string_create(&t.u.str);
                    l2_string_strcpy(&t.u.str, &token_str_buff);
                    goto ret;

                } else if (ch == '\\') {
                    fa_state = 0x50;

                } else if (ch == L2_EOF) {
                    l2_parsing_error(L2_PARSING_ERROR_INCOMPLETE_STRING_LITERAL, lines, cols);

                } else {
                    l2_string_push_char(&token_str_buff, ch);
                }
                break;
        }


    }

    ret:

    l2_string_destroy(&token_str_buff);

    l2_vector_append(&token_stream_p->token_vector, &t);
    token_stream_p->token_vector_current_pos += 1;
    return (l2_token *)l2_vector_tail(&token_stream_p->token_vector);
}



void l2_token_stream_rollback(l2_token_stream *token_stream_p) {
    l2_assert(token_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(token_stream_p->token_vector_current_pos > 0, L2_INTERNAL_ERROR_OUT_OF_RANGE);
    token_stream_p->token_vector_current_pos -= 1;
}

char *l2_token_stream_str_keyword(l2_string *str_p) {
    int i;
    for (i = 0; i < sizeof(g_l2_token_keywords) / sizeof(char *); i++) {
        if (l2_string_equal_c(str_p, g_l2_token_keywords[i])) {
            return g_l2_token_keywords[i];
        }
    }
    return L2_NULL_PTR;
}

l2_token *l2_token_stream_current_token(l2_token_stream *token_stream_p) {
    l2_assert(token_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(token_stream_p->token_vector_current_pos > 0, L2_INTERNAL_ERROR_OUT_OF_RANGE);
    return (l2_token *)l2_vector_at(&token_stream_p->token_vector, token_stream_p->token_vector_current_pos - 1);;
}

int l2_token_stream_get_pos(l2_token_stream *token_stream_p) {
    l2_assert(token_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    return token_stream_p->token_vector_current_pos;
}

void l2_token_stream_set_pos(l2_token_stream *token_stream_p, int pos) {
    l2_assert(token_stream_p, L2_INTERNAL_ERROR_NULL_POINTER);
    l2_assert(pos > 0 && token_stream_p->token_vector_current_pos < token_stream_p->token_vector.size, L2_INTERNAL_ERROR_OUT_OF_RANGE);
    token_stream_p->token_vector_current_pos = pos;
}
