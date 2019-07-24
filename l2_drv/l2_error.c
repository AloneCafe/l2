#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "l2_error.h"
#include "../l2_tpl/l2_string.h"
#include "../l2_parser/l2_parse.h"

extern l2_parser *g_parser_p;

void l2_clean_before_abort() {
    l2_parse_finalize();
}

void _Noreturn l2_internal_error(const l2_internal_error_type error_type, ...) {
    va_list va;
    va_start(va, error_type);
    char *msg;
    void *ptr;

    switch (error_type) {
        case L2_INTERNAL_ERROR_NULL_POINTER:
            msg = va_arg(va, char *);
            fprintf(stderr, "L2 解释器内部错误: \n\t使用空指针进行操作: %s\n", msg);
            break;

        case L2_INTERNAL_ERROR_OUT_OF_RANGE:
            msg = va_arg(va, char *);
            fprintf(stderr, "L2 解释器内部错误: \n\t超出数据范围: %s\n", msg);
            break;

        case L2_INTERNAL_ERROR_ILLEGAL_OPERATION:
            msg = va_arg(va, char *);
            fprintf(stderr, "L2 解释器内部错误: \n\t检测到非法操作: %s\n", msg);
            break;

        case L2_INTERNAL_ERROR_UNREACHABLE_CODE:
            msg = va_arg(va, char *);
            fprintf(stderr, "L2 解释器内部错误: \n\t运行至无法到达的代码段: %s\n", msg);
            break;

        case L2_INTERNAL_ERROR_MEM_BLOCK_NOT_MANAGED:
            ptr = va_arg(va, void *);
            fprintf(stderr, "L2 解释器内部错误: \n\t内存区块未托管: 指针 - %p\n", ptr);
            break;

        case L2_INTERNAL_ERROR_MEM_BLOCK_NOT_IN_GC:
            ptr = va_arg(va, void *);
            fprintf(stderr, "L2 解释器内部错误: \n\t内存区块未在 GC(垃圾回收) 链中: 指针 - %p\n", ptr);
            break;

        default:
            msg = va_arg(va, char *);
            fprintf(stderr, "L2 解释器内部错误: \n\t发生未知错误: %s\n", msg);
    }

    fprintf(stderr, "L2 解释器已异常终止\n");

    va_end(va);
    l2_clean_before_abort();
    exit(error_type);
}

void _Noreturn l2_parsing_error(const l2_parsing_error_type error_type, int lines, int cols, ...) {

    va_list va;
    va_start(va, cols);

    char illegal_char;
    char *token_str, *token_str2;
    l2_token *token_p;

    switch (error_type) {
        case L2_PARSING_ERROR_ILLEGAL_CHARACTER:
            illegal_char = (char)va_arg(va, int);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t解析到非法字符 \'%c\'(0x%X)\n", lines, cols, illegal_char, illegal_char);
            break;

        case L2_PARSING_ERROR_UNEXPECTED_TOKEN:
            token_p = va_arg(va, l2_token *);
            /*
            switch (token_p->type) {

            }*/
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t解析到非法词法元素\n", lines, cols);
            break;

        case L2_PARSING_ERROR_ILLEGAL_NUMBER_IN_AN_OCTAL_LITERAL:
            illegal_char = (char)va_arg(va, int);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t在八进制字面量中包含了非法数字 \'%c\'\n", lines, cols, illegal_char);
            break;

        case L2_PARSING_ERROR_INCOMPLETE_HEX_LITERAL:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t不完整的十六进制字面量\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPLETE_REAL_LITERAL:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t不完整的实数 (real) 字面量\n", lines, cols);
            break;

        case L2_PARSING_ERROR_EMPTY_CHARACTER_LITERAL:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t字符字面量不能为空\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPLETE_CHARACTER_LITERAL:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t不完整的字符字面量, 字符字面量应该以单引号 ' 结束\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MULTI_CHARACTER_IN_SINGLE_CHARACTER_LITERAL:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t在单个字符字面量中出现多个字符\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPLETE_STRING_LITERAL:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t不完整的字符串字面量, 字符串字面量应该以双引号 \" 结束\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MISSING_COLON:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t不完整的条件表达式, 可能缺少冒号 \':\'\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MISSING_SEMICOLON:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t在语句末尾可能缺少了分号\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MISSING_RBRACE:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t缺少块结束符, 右花括号 }\n", lines, cols);
            break;

        case L2_PARSING_ERROR_MISSING_RP:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t缺少右圆括号 \')\'\n", lines, cols);
            break;

        case L2_PARSING_ERROR_IDENTIFIER_UNDEFINED:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t标识符 \'%s\' 未定义\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_IDENTIFIER_REDEFINED:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t标识符 \'%s\' 重定义\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_REFERENCE_SYMBOL_BEFORE_INITIALIZATION:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t符号 \'%s\' 在初始化之前被引用了\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_INCOMPATIBLE_OPERATION:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t不兼容的操作 \'%s\' 在 ", lines, cols, token_str);
            token_str = va_arg(va, char *);
            fprintf(stderr, "%s 之上\n", token_str);
            break;

        case L2_PARSING_ERROR_DIVIDE_BY_ZERO:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t试图除以整数 0\n", lines, cols);
            break;

        case L2_PARSING_ERROR_EXPR_NOT_BOOL:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t表达式不是布尔类型 (bool)\n", lines, cols);
            break;

        case L2_PARSING_ERROR_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t操作符 \'%s\' 的右侧必须是布尔类型 (bool) 的值\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_BOOL_VALUE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t操作符 \'%s\' 的左侧必须是布尔类型 (bool) 的值\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_LEFT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t操作符 \'%s\' 的左侧必须是整数型 (integer) 的值\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_RIGHT_SIDE_OF_OPERATOR_MUST_BE_A_INTEGER_VALUE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t操作符 \'%s\' 的右侧必须是整数型 (integer) 的值\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_DUALISTIC_OPERATOR_CONTAINS_INCOMPATIBLE_TYPE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t操作符 \'%s\' 作用于不兼容的类型之上, ", lines, cols, token_str);
            token_str2 = va_arg(va, char *);
            fprintf(stderr, "<%s> \'%s\' ", token_str2, token_str);
            token_str2 = va_arg(va, char *);
            fprintf(stderr, "<%s>\n", token_str2);
            break;

        case L2_PARSING_ERROR_UNITARY_OPERATOR_CONTAINS_INCOMPATIBLE_TYPE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t操作符 \'%s\' 的右侧具有不兼容的类型, ", lines, cols, token_str);
            token_str2 = va_arg(va, char *);
            fprintf(stderr, "\'%s\' <%s>\n", token_str, token_str2);
            break;

        case L2_PARSING_ERROR_INVALID_CONTINUE_IN_CURRENT_CONTEXT:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t在当前的上下文中出现无效的关键字 \'continue\'\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INVALID_BREAK_IN_CURRENT_CONTEXT:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t在当前的上下文中出现无效的关键字 \'break\'\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INVALID_RETURN_IN_CURRENT_CONTEXT:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t在当前的上下文中出现无效的关键字 \'return\'\n", lines, cols);
            break;

        case L2_PARSING_ERROR_SYMBOL_IS_NOT_PROCEDURE:
            token_str = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t符号 \'%s\' 并非过程, 无法进行调用\n", lines, cols, token_str);
            break;

        case L2_PARSING_ERROR_TOO_MANY_PARAMETERS:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t调用过程的参数太多\n", lines, cols);
            break;

        case L2_PARSING_ERROR_TOO_FEW_PARAMETERS:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t调用过程的参数太少\n", lines, cols);
            break;

        case L2_PARSING_ERROR_EXPR_RESULT_WITHOUT_VALUE:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t表达式并不具有任何值\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPATIBLE_EXPR_TYPE:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t不兼容的表达式值类型\n", lines, cols);
            break;

        case L2_PARSING_ERROR_INCOMPATIBLE_SYMBOL_TYPE:
            fprintf(stderr, "L2 脚本解释错误 (在 %d 行 %d 列附近): \n\t不兼容的符号类型\n", lines, cols);
            break;

        default:
            fprintf(stderr, "L2 脚本解释错误, 出现一个未知错误\n");
    }

    fprintf(stderr, "L2 解释器已异常终止\n");

    va_end(va);
    l2_clean_before_abort();
    exit(error_type);
}