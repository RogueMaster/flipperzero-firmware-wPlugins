#pragma once

#include <stddef.h>

#include "mp_flipper_runtime.h"

void mp_flipper_exec_str(const char* str);
void mp_flipper_exec_py_file(const char* file_path);
void mp_flipper_compile_and_save_file(const char* py_file_path, const char* mpy_file_path);
