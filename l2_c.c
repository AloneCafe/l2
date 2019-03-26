#include <stdio.h>
#include <stdlib.h>
#include "l2_system/l2_assert.h"
#include "l2_lexer/l2_char_stream.h"
#include "l2_lexer/l2_token_stream.h"
#include "l2_parser/l2_parse.h"

typedef enum _l2_init_env_error_type {
    L2_INIT_ENV_NO_ERROR,
    L2_INIT_ENV_ERROR_INVALID_OPTION,
    L2_INIT_ENV_ERROR_CANNOT_OPEN_SOURCE_FILE
}l2_init_env_error_type;

typedef enum _l2_interpreter_input_type {
    L2_INTERPRETER_INPUT_TYPE_REPL, /* read-eval-print-loop */
    L2_INTERPRETER_INPUT_TYPE_SINGLE_SOURCE_FILE /* interpreter will execute according to the code which is reading from single source file */

}l2_interpreter_input_type;

typedef struct l2_env_args {
    l2_interpreter_input_type input_type;
    FILE *source_file_p;

}l2_env_args;

int l2_init_env(int argc, char *argv[], l2_env_args *env_args_p) {

    if (argc <= 1) {
        env_args_p->input_type = L2_INTERPRETER_INPUT_TYPE_REPL;
        return L2_INIT_ENV_NO_ERROR;
    }

    int i;
    for (i = 1; i < argc; i++) {
        char *args = argv[i];

        int cp; /* char pos in every string */
        int state = 0; /* state variable */
        for (cp = 0; args[cp] != '\0'; cp++) {
            char cc = args[cp]; /* current char */
            switch (state) {
                case 0:
                    if (cc == '-') {
                        state = 1; /* parse option string */
                    } else {
                        cp -= 1; /* rollback a character */
                        state = 2; /* parse path string */
                    }
                    break;

                case 1: /* parse option string */
                    switch (cc) {
                        case 'v': /* print version info */
                            if (args[cp + 1] != '\0') { /* judge the next char */
                                fprintf(stderr, "invalid option: %s\n", argv[i]);
                                return L2_INIT_ENV_ERROR_INVALID_OPTION;
                            }

                            fprintf(stdout, "l2 programming language & interpreter\ncurrent version: %s\n", L2_VERSION);
                            exit(0);

                        case 'h': /* print help info */
                            if (args[cp + 1] != '\0') { /* judge the next char */
                                fprintf(stderr, "invalid option: %s\n'-h' to get help\n", argv[i]);
                                return L2_INIT_ENV_ERROR_INVALID_OPTION;
                            }

                            fprintf(stdout,
                                    "l2 programming language & interpreter\n"
                                    "\nUsage: %s [options] <source_file>\n"
                                    "Options:\n"
                                    "-v: print the version info\n"
                                    "-h: print the help info\n"
                            , argv[0]);
                            exit(0);

                        default:
                            fprintf(stderr, "invalid option: %s\n'-h' to get help\n", argv[i]);
                            return L2_INIT_ENV_ERROR_INVALID_OPTION;
                    }
                    break;

                case 2: /* parse path string */
                    env_args_p->input_type = L2_INTERPRETER_INPUT_TYPE_SINGLE_SOURCE_FILE;
                    env_args_p->source_file_p = fopen(argv[i], "r");
                    if (!env_args_p->source_file_p) {
                        fprintf(stderr, "fatal: can not open source file: %s\n", argv[i]);
                        return L2_INIT_ENV_ERROR_CANNOT_OPEN_SOURCE_FILE;
                    }
                    break;
            }
        }

    }

    return L2_INIT_ENV_NO_ERROR;
}

int main(int argc, char *argv[]) {

    l2_env_args env_args;
    int ret_code = l2_init_env(argc, argv, &env_args);
    if (ret_code != L2_INIT_ENV_NO_ERROR)
        return ret_code;

    switch (env_args.input_type) {
        case L2_INTERPRETER_INPUT_TYPE_SINGLE_SOURCE_FILE:
            l2_parse_initialize(env_args.source_file_p);
            break;

        case L2_INTERPRETER_INPUT_TYPE_REPL:
            l2_parse_initialize(stdin);
            break;

        default:
            exit(-1);
    }

    l2_parse();

    l2_parse_finalize();

    return 0;
}