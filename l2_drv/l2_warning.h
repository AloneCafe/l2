#ifndef _L2_WARNING_H_
#define _L2_WARNING_H_

#include "../l2_tpl/l2_common_type.h"

typedef enum _l2_parsing_warning_type {
    L2_PARSING_WARNING_UNKNOWN_ESCAPE_SEQUENCE
}l2_parsing_warning_type;

/* void l2_parsing_warning_en(l2_parsing_warning_type warning_type, int lines, int cols, ...); */
void l2_parsing_warning(l2_parsing_warning_type warning_type, int lines, int cols, ...);

#endif