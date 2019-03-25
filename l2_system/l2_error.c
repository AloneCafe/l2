#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "l2_error.h"
#include "../l2_base/l2_string.h"
#include "../l2_parser/l2_parse.h"

extern l2_parser *g_parser_p;

void l2_clean_before_abort() {
    l2_parse_finalize();
}

void l2_internal_error(const l2_internal_error_type error_type, ...) {
    va_list va;
    va_start(va, error_type);
    char *msg;
    void *ptr;

    switch (error_type) {
        case L2_INTERNAL_ERROR_NULL_POINTER:
            msg = va_arg(va, char *);
            fprintf(stderr, "l2 internal error: \n\toperation based on null pointer: %s\n", msg);
            break;

        case L2_INTERNAL_ERROR_OUT_OF_RANGE:
            msg = va_arg(va, char *);
            fprintf(stderr, "l2 internal error: \n\tout of range: %s\n", msg);
            break;

        case L2_INTERNAL_ERROR_ILLEGAL_OPERATION:
            msg = va_arg(va, char *);
            fprintf(stderr, "l2 internal error: \n\tillegal operation: %s\n", msg);
            break;

        case L2_INTERNAL_ERROR_UNREACHABLE_CODE:
            msg = va_arg(va, char *);
            fprintf(stderr, "l2 internal error: \n\tunreachable code: %s\n", msg);
            break;

        case L2_INTERNAL_ERROR_MEM_BLOCK_NOT_MANAGED:
            ptr = va_arg(va, void *);
            fprintf(stderr, "l2 internal error: \n\tmem block may be not managed: pointer - %p\n", ptr);
            break;

        case L2_INTERNAL_ERROR_MEM_BLOCK_NOT_IN_GC:
            ptr = va_arg(va, void *);
            fprintf(stderr, "l2 internal error: \n\tmem block may be not in gc: pointer - %p\n", ptr);
            break;

        default:
            msg = va_arg(va, char *);
            fprintf(stderr, "l2 internal error: \n\tunknown error: %s\n", msg);
    }

    fprintf(stderr, "l2 interpreter terminated\n");

    va_end(va);
    l2_clean_before_abort();
    exit(error_type);
}

void l2_parsing_error(const l2_parsing_error_type error_type, int lines, int cols, ...) {
    va_list va;
    va_start(va, cols);

    char illegal_char;
    char *token_str, *token_str2;
    l2_token *token_p;

    switch (error_type) {
        case L2_PARSING_ERROR_ILLEGAL_CHARACTER:
            illegal_char = (char)va_arg(va, int);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tillegal character \'%c\'(0x%X)\n", lines, cols, illegal_char, illegal_char);
            break;

        case L2_PARSING_ERROR_UNEXPECTED_TOKEN:
            token_p = va_arg(va, l2_token *);
            /*
            switch (token_p->type) {

            }*/
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tunexpected token\n", lines, cols);
            break;

        case L2_PARSING_ERROR_ILLEGAL_NUMBER_IN_AN_OCTAL_LITERAL:
            illegal_char = (char)va_arg(va, int);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tillegal number \'%c\' in an octal literal\n", lines, cols, illegal_char);
            break;

        case L2_PARSING_ERROR_INCOMPLETE_HEX_LITERAL:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tincomplete hex literal\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPLETE_REAL_LITERAL:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tincomplete real number literal\n", lines, cols);
            break;

        case L2_PARSING_ERROR_EMPTY_CHARACTER_LITERAL:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tempty character literal\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPLETE_CHARACTER_LITERAL:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tincomplete character literal, character literal should be completed with single quote mark ' in the end\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MULTI_CHARACTER_IN_SINGLE_CHARACTER_LITERAL:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tmulti-character in single character literal\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPLETE_STRING_LITERAL:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tincomplete string literal, string literal should be completed with double quote mark \" in the end\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MISSING_COLON:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tincomplete condition expression, maybe missing colon \':\'\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MISSING_SEMICOLON:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tmissing semicolon at the end of statement\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MISSING_RBRACE:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tmissing block end mark \'}\'\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MISSING_RP:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tmissing the right part of parentheses \')\'\n", lines, cols);
            break;

        case L2_PARSING_ERROR_IDENTIFIER_UNDEFINED:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tidentifier \'%s\' undefined\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_IDENTIFIER_REDEFINED:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tidentifier \'%s\' redefined\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_REFERENCE_SYMBOL_BEFORE_INITIALIZATION:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\treference symbol \'%s\' before initialization\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_INCOMPATIBLE_OPERATION:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tincompatible operation \'%s\' on ", lines, cols, token_str);
            token_str = va_arg(va, char *);
            fprintf(stderr, "%s\n", token_str);
            break;

        case L2_PARSING_ERROR_DIVIDE_BY_ZERO:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tdivide by integer zero\n", lines, cols);
            break;

        case L2_PARSING_ERROR_EXPR_NOT_BOOL:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tthe expression is not bool\n", lines, cols);
            break;

        case L2_PARSING_ERROR_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tright side of operator \'%s\' must be a bool value\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tleft side of operator \'%s\' must be a bool value\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tleft side of operator \'%s\' must be a integer value\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tright side of operator \'%s\' must be a integer value\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_INCOMPATIBLE_TYPE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\toperator \'%s\' contains incompatible type, ", lines, cols, token_str);
            token_str2 = va_arg(va, char *);
            fprintf(stderr, "<%s> \'%s\' ", token_str2, token_str);
            token_str2 = va_arg(va, char *);
            fprintf(stderr, "<%s>\n", token_str2);
            break;

        case L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_INCOMPATIBLE_TYPE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tright side of operator \'%s\' has incompatible type, ", lines, cols, token_str);
            token_str2 = va_arg(va, char *);
            fprintf(stderr, "\'%s\' <%s>\n", token_str, token_str2);
            break;

        case L2_PARSING_ERROR_INVALID_CONTINUE_IN_CURRENT_CONTEXT:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tinvalid keyword \'continue\' in current content\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INVALID_BREAK_IN_CURRENT_CONTEXT:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tinvalid keyword \'break\' in current content\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INVALID_RETURN_IN_CURRENT_CONTEXT:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tinvalid keyword \'return\' in current content\n", lines, cols);
            break;

        case L2_PARSING_ERROR_SYMBOL_IS_NOT_PROCEDURE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tperforms calling on symbol \'%s\' which is not a procedure\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_TOO_MANY_PARAMETERS:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\ttaking too many parameters to call the procedure\n", lines, cols);
            break;

        case L2_PARSING_ERROR_TOO_FEW_PARAMETERS:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\ttaking too few parameters to call the procedure\n", lines, cols);
            break;

        case L2_PARSING_ERROR_EXPR_RESULT_WITHOUT_VALUE:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\texpr without value\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPATIBLE_EXPR_TYPE:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tincompatible expr type\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPATIBLE_SYMBOL_TYPE:
            fprintf(stderr, "l2 parsing error near line %d, column %d: \n\tincompatible symbol type\n", lines, cols);
            break;

        default:
            fprintf(stderr, "l2 parsing error, an unknown error occurred\n");
    }

    fprintf(stderr, "interpreter terminated\n");

    va_end(va);
    l2_clean_before_abort();
    exit(error_type);
}