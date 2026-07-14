#ifndef _CJSON_HELPERS_H_
#define _CJSON_HELPERS_H_

#include <furi.h>
#include <storage/storage.h>
#include <cJSON/cJSON.h>
#include <401_err.h>

l401_err json_read(const char* filename, cJSON** json);
l401_err json_read_hex_array(cJSON* json_array, uint8_t** dst, size_t* len);

#endif /* end of include guard: _CJSON_HELPERS_H_ */
