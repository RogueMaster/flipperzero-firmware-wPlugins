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

#include <stddef.h>
#include <jsmn/jsmn_h.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef JSMN_H
#define JSMN_H

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif
    /**
     * Create JSON parser over an array of tokens
     */
    JSMN_API void jsmn_init(jsmn_parser *parser);

    /**
     * Run JSON parser. It parses a JSON data string into and array of tokens, each
     * describing a single JSON object.
     */
    JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                            jsmntok_t *tokens, const unsigned int num_tokens);

#ifndef JSMN_HEADER
/* Implementation has been moved to jsmn.c */
#endif /* JSMN_HEADER */

#endif /* JSMN_H */

/* Custom Helper Functions */
#ifndef JB_JSMN_EDIT
#define JB_JSMN_EDIT
    /* Added in by JBlanked on 2024-10-16 for use in Flipper Zero SDK*/

    // Helper function to create a JSON object
    char *get_json(const char *key, const char *value);
    // Helper function to compare JSON keys
    int jsoneq(const char *json, jsmntok_t *tok, const char *s);

    // Return the value of the key in the JSON data
    char *get_json_value(const char *key, const char *json_data);

    // Revised get_json_array_value function
    char *get_json_array_value(const char *key, uint32_t index, const char *json_data);

    // Revised get_json_array_values function with correct token skipping
    char **get_json_array_values(const char *key, const char *json_data, int *num_values);

    int json_token_count(const char *json);
#endif /* JB_JSMN_EDIT */

#ifdef __cplusplus
}
#endif