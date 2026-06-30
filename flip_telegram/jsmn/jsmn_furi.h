/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * [License text continues...]
 */

#ifndef JSMN_FURI_H
#define JSMN_FURI_H

#include <jsmn/jsmn_h.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif

    JSMN_API void jsmn_init_furi(jsmn_parser *parser);
    JSMN_API int jsmn_parse_furi(jsmn_parser *parser, const FuriString *js,
                                 jsmntok_t *tokens, const unsigned int num_tokens);

#ifndef JSMN_HEADER
/* Implementation in jsmn_furi.c */
#endif /* JSMN_HEADER */

#endif /* JSMN_FURI_H */

#ifndef JB_JSMN_FURI_EDIT
#define JB_JSMN_FURI_EDIT

    // Helper function to create a JSON object
    FuriString *get_json_furi(const FuriString *key, const FuriString *value);

    // Updated signatures to accept const char* key
    FuriString *get_json_value_furi(const char *key, const FuriString *json_data);
    FuriString *get_json_array_value_furi(const char *key, uint32_t index, const FuriString *json_data);
    FuriString **get_json_array_values_furi(const char *key, const FuriString *json_data, int *num_values);

    uint32_t json_token_count_furi(const FuriString *json);
/* Example usage:
char *json = "{\"key1\":\"value1\",\"key2\":\"value2\"}";
FuriString *json_data = char_to_furi_string(json);
if (!json_data)
{
    FURI_LOG_E(TAG, "Failed to allocate FuriString");
    return -1;
}
FuriString *value = get_json_value_furi("key1", json_data, json_token_count_furi(json_data));
if (value)
{
    FURI_LOG_I(TAG, "Value: %s", furi_string_get_cstr(value));
    furi_string_free(value);
}
furi_string_free(json_data);
*/
#endif /* JB_JSMN_EDIT */

#ifdef __cplusplus
}
#endif