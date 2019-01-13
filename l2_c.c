#include <stdio.h>
#include "l2_c.h"
#include "l2_system/l2_assert.h"
#include "l2_lexer/l2_char_stream.h"
#include "l2_lexer/l2_token_stream.h"
#include "l2_parser/l2_parse.h"


int main(int argc, char *argv[]) {

    l2_parse_initialize(stdin);

    l2_parse();

    l2_parse_finalize();

    return 0;
}