#include <string.h>

#include "py/mperrno.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/persistentcode.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "shared/runtime/gchelper.h"

#include "mp_flipper_runtime.h"
#include "mp_flipper_compiler.h"
#include "mp_flipper_halport.h"

void mp_flipper_exec_str(const char* code) {
    nlr_buf_t nlr;

    if(nlr_push(&nlr) == 0) {
        // Compile, parse and execute the given string
        mp_lexer_t* lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, code, strlen(code), 0);
        mp_store_global(MP_QSTR___file__, MP_OBJ_NEW_QSTR(lex->source_name));
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&parse_tree, lex->source_name, true);
        mp_call_function_0(module_fun);
        nlr_pop();
    } else {
        // Uncaught exception: print it out.
        mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
    }
}

void mp_flipper_exec_py_file(const char* file_path) {
    nlr_buf_t nlr;

    if(nlr_push(&nlr) == 0) {
        do {
            // check if file exists
            if(mp_flipper_import_stat(file_path) == MP_FLIPPER_IMPORT_STAT_NO_EXIST) {
                mp_raise_OSError_with_filename(MP_ENOENT, file_path);

                break;
            }

            // Compile, parse and execute the given file
            mp_lexer_t* lex = mp_lexer_new_from_file(qstr_from_str(file_path));
            mp_store_global(MP_QSTR___file__, MP_OBJ_NEW_QSTR(lex->source_name));
            mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
            mp_obj_t module_fun = mp_compile(&parse_tree, lex->source_name, false);
            mp_call_function_0(module_fun);
        } while(false);

        nlr_pop();
    } else {
        // Uncaught exception: print it out.
        mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
    }
}
