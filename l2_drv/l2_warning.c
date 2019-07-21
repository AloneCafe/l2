#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "l2_warning.h"

/*
void l2_parsing_warning_en(l2_parsing_warning_type warning_type, int lines, int cols, ...) {
    va_list va;
    va_start(va, cols);

    char *escape_seq;

    switch (warning_type) {
        case L2_PARSING_WARNING_UNKNOWN_ESCAPE_SEQUENCE:
            escape_seq = va_arg(va, char *);
            fprintf(stderr, "l2 parsing warning at line %d, column %d, unknown escape sequence \'\\%s\', it will be interpreted to normal character\n", lines, cols, escape_seq);
            break;

        default:
            fprintf(stderr, "l2 parsing warning, an unknown warning occured\n");
    }

    va_end(va);
}
*/

void l2_parsing_warning(l2_parsing_warning_type warning_type, int lines, int cols, ...) {
    va_list va;
    va_start(va, cols);

    char *escape_seq;

    switch (warning_type) {
        case L2_PARSING_WARNING_UNKNOWN_ESCAPE_SEQUENCE:
            escape_seq = va_arg(va, char *);
            fprintf(stderr, "L2 脚本解释警告 (在 %d 行 %d 列附近): 未知的转义序列 \'\\%s\', 它将被解释器当做普通字符\n", lines, cols, escape_seq);
            break;

        default:
            fprintf(stderr, "L2 脚本解释警告, 出现一个未知警告\n");
    }

    va_end(va);
}