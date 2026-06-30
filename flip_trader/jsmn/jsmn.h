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

#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif

    /**
     * JSON type identifier. Basic types are:
     * 	o Object
     * 	o Array
     * 	o String
     * 	o Other primitive: number, boolean (true/false) or null
     */
    typedef enum
    {
        JSMN_UNDEFINED = 0,
        JSMN_OBJECT = 1 << 0,
        JSMN_ARRAY = 1 << 1,
        JSMN_STRING = 1 << 2,
        JSMN_PRIMITIVE = 1 << 3
    } jsmntype_t;

    enum jsmnerr
    {
        /* Not enough tokens were provided */
        JSMN_ERROR_NOMEM = -1,
        /* Invalid character inside JSON string */
        JSMN_ERROR_INVAL = -2,
        /* The string is not a full JSON packet, more bytes expected */
        JSMN_ERROR_PART = -3
    };

    /**
     * JSON token description.
     * type		type (object, array, string etc.)
     * start	start position in JSON data string
     * end		end position in JSON data string
     */
    typedef struct
    {
        jsmntype_t type;
        int start;
        int end;
        int size;
#ifdef JSMN_PARENT_LINKS
        int parent;
#endif
    } jsmntok_t;

    /**
     * JSON parser. Contains an array of token blocks available. Also stores
     * the string being parsed now and current position in that string.
     */
    typedef struct
    {
        unsigned int pos;     /* offset in the JSON string */
        unsigned int toknext; /* next token to allocate */
        int toksuper;         /* superior token node, e.g. parent object or array */
    } jsmn_parser;

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

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */

/* Custom Helper Functions */
#ifndef JB_JSMN_EDIT
#define JB_JSMN_EDIT
/* Added in by JBlanked on 2024-10-16 for use in Flipper Zero SDK*/

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <furi.h>

// Helper function to create a JSON object
char *jsmn(const char *key, const char *value);
// Helper function to compare JSON keys
int jsoneq(const char *json, jsmntok_t *tok, const char *s);

// Return the value of the key in the JSON data
char *get_json_value(char *key, char *json_data, uint32_t max_tokens);

// Revised get_json_array_value function
char *get_json_array_value(char *key, uint32_t index, char *json_data, uint32_t max_tokens);

// Revised get_json_array_values function with correct token skipping
char **get_json_array_values(char *key, char *json_data, uint32_t max_tokens, int *num_values);
#endif /* JB_JSMN_EDIT */
