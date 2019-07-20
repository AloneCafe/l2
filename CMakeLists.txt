cmake_minimum_required(VERSION 3.12)
project(l2 C)

set(CMAKE_C_STANDARD 90)

add_executable(l2
        l2_mem/l2_gc.c
        l2_mem/l2_gc.h
        l2_mem/l2_storage.c
        l2_mem/l2_storage.h
        l2_tpl/l2_common_type.c
        l2_tpl/l2_common_type.h
        l2_tpl/l2_stack.c
        l2_tpl/l2_stack.h
        l2_tpl/l2_string.c
        l2_tpl/l2_string.h
        l2_tpl/l2_vector.c
        l2_tpl/l2_vector.h
        l2_parser/l2_char_stream.c
        l2_parser/l2_char_stream.h
        l2_parser/l2_token_stream.c
        l2_parser/l2_token_stream.h
        l2_drv/l2_assert.c
        l2_drv/l2_assert.h
        l2_drv/l2_error.c
        l2_drv/l2_error.h
        l2_drv/l2_warning.c
        l2_drv/l2_warning.h
        l2_c.c
        l2_parser/l2_scope.c
        l2_parser/l2_scope.h
        l2_parser/l2_cast.c
        l2_parser/l2_cast.h
        l2_parser/l2_symbol_table.c
        l2_parser/l2_symbol_table.h
        l2_parser/l2_parse.c
        l2_parser/l2_parse.h
        l2_parser/l2_eval.c l2_parser/l2_eval.h l2_parser/l2_call_stack.c l2_parser/l2_call_stack.h)
