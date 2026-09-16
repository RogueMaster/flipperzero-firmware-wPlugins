#pragma once

#define FLIP_CRYPT_USER_DATA_PATH APP_DATA_PATH("saved")

void save_result(const char* text, char* file_name);
void save_result_generic(const char* filename, const char* text);
char* load_result_generic(const char* filename);
