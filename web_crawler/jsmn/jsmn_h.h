#pragma once
#include <furi.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
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
    JSMN_ERROR_NOMEM = -1,
    JSMN_ERROR_INVAL = -2,
    JSMN_ERROR_PART = -3
};

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

typedef struct
{
    unsigned int pos;     /* offset in the JSON string */
    unsigned int toknext; /* next token to allocate */
    int toksuper;         /* superior token node, e.g. parent object or array */
} jsmn_parser;

typedef struct
{
    char *key;
    char *value;
} JSON;

typedef struct
{
    FuriString *key;
    FuriString *value;
} FuriJSON;

FuriString *char_to_furi_string(const char *str);

// check memory
bool jsmn_memory_check(size_t heap_size);
