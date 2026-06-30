/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * [License text continues...]
 */

#include <jsmn/jsmn_furi.h>

// Forward declarations of helper functions
static int jsoneq_furi(const FuriString *json, jsmntok_t *tok, const FuriString *s);
static int skip_token(const jsmntok_t *tokens, int start, int total);

/**
 * Allocates a fresh unused token from the token pool.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens,
                                   const size_t num_tokens)
{
    if (parser->toknext >= num_tokens)
    {
        return NULL;
    }
    jsmntok_t *tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
#ifdef JSMN_PARENT_LINKS
    tok->parent = -1;
#endif
    return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, const jsmntype_t type,
                            const int start, const int end)
{
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 * Now uses FuriString to access characters.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const FuriString *js,
                                jsmntok_t *tokens, const size_t num_tokens)
{
    size_t len = furi_string_size(js);
    int start = parser->pos;

    for (; parser->pos < len; parser->pos++)
    {
        char c = furi_string_get_char(js, parser->pos);
        switch (c)
        {
#ifndef JSMN_STRICT
        case ':':
#endif
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case ',':
        case ']':
        case '}':
            goto found;
        default:
            break;
        }
        if (c < 32 || c >= 127)
        {
            parser->pos = start;
            return JSMN_ERROR_INVAL;
        }
    }

#ifdef JSMN_STRICT
    // In strict mode primitive must be followed by a comma/object/array
    parser->pos = start;
    return JSMN_ERROR_PART;
#endif

found:
    if (tokens == NULL)
    {
        parser->pos--;
        return 0;
    }
    jsmntok_t *token = jsmn_alloc_token(parser, tokens, num_tokens);
    if (token == NULL)
    {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
    }
    jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
    token->parent = parser->toksuper;
#endif
    parser->pos--;
    return 0;
}

/**
 * Fills next token with JSON string.
 * Now uses FuriString to access characters.
 */
static int jsmn_parse_string(jsmn_parser *parser, const FuriString *js,
                             jsmntok_t *tokens, const size_t num_tokens)
{
    size_t len = furi_string_size(js);
    int start = parser->pos;
    parser->pos++;

    for (; parser->pos < len; parser->pos++)
    {
        char c = furi_string_get_char(js, parser->pos);
        if (c == '\"')
        {
            if (tokens == NULL)
            {
                return 0;
            }
            jsmntok_t *token = jsmn_alloc_token(parser, tokens, num_tokens);
            if (token == NULL)
            {
                parser->pos = start;
                return JSMN_ERROR_NOMEM;
            }
            jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
#ifdef JSMN_PARENT_LINKS
            token->parent = parser->toksuper;
#endif
            return 0;
        }

        if (c == '\\' && (parser->pos + 1) < len)
        {
            parser->pos++;
            char esc = furi_string_get_char(js, parser->pos);
            switch (esc)
            {
            case '\"':
            case '/':
            case '\\':
            case 'b':
            case 'f':
            case 'r':
            case 'n':
            case 't':
                break;
            case 'u':
            {
                parser->pos++;
                for (int i = 0; i < 4 && parser->pos < len; i++)
                {
                    char hex = furi_string_get_char(js, parser->pos);
                    if (!((hex >= '0' && hex <= '9') ||
                          (hex >= 'A' && hex <= 'F') ||
                          (hex >= 'a' && hex <= 'f')))
                    {
                        parser->pos = start;
                        return JSMN_ERROR_INVAL;
                    }
                    parser->pos++;
                }
                parser->pos--;
                break;
            }
            default:
                parser->pos = start;
                return JSMN_ERROR_INVAL;
            }
        }
    }
    parser->pos = start;
    return JSMN_ERROR_PART;
}

/**
 * Create JSON parser
 */
void jsmn_init_furi(jsmn_parser *parser)
{
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
}

/**
 * Parse JSON string and fill tokens.
 * Now uses FuriString for the input JSON.
 */
int jsmn_parse_furi(jsmn_parser *parser, const FuriString *js,
                    jsmntok_t *tokens, const unsigned int num_tokens)
{
    size_t len = furi_string_size(js);
    int r;
    int i;
    int count = parser->toknext;

    for (; parser->pos < len; parser->pos++)
    {
        char c = furi_string_get_char(js, parser->pos);
        jsmntype_t type;

        switch (c)
        {
        case '{':
        case '[':
        {
            count++;
            if (tokens == NULL)
            {
                break;
            }
            jsmntok_t *token = jsmn_alloc_token(parser, tokens, num_tokens);
            if (token == NULL)
                return JSMN_ERROR_NOMEM;
            if (parser->toksuper != -1)
            {
                jsmntok_t *t = &tokens[parser->toksuper];
#ifdef JSMN_STRICT
                if (t->type == JSMN_OBJECT)
                    return JSMN_ERROR_INVAL;
#endif
                t->size++;
#ifdef JSMN_PARENT_LINKS
                token->parent = parser->toksuper;
#endif
            }
            token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
            token->start = parser->pos;
            parser->toksuper = parser->toknext - 1;
            break;
        }
        case '}':
        case ']':
            if (tokens == NULL)
            {
                break;
            }
            type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
            if (parser->toknext < 1)
            {
                return JSMN_ERROR_INVAL;
            }
            {
                jsmntok_t *token = &tokens[parser->toknext - 1];
                for (;;)
                {
                    if (token->start != -1 && token->end == -1)
                    {
                        if (token->type != type)
                            return JSMN_ERROR_INVAL;
                        token->end = parser->pos + 1;
                        parser->toksuper = token->parent;
                        break;
                    }
                    if (token->parent == -1)
                    {
                        if (token->type != type || parser->toksuper == -1)
                        {
                            return JSMN_ERROR_INVAL;
                        }
                        break;
                    }
                    token = &tokens[token->parent];
                }
            }
#else
            {
                jsmntok_t *token;
                for (i = parser->toknext - 1; i >= 0; i--)
                {
                    token = &tokens[i];
                    if (token->start != -1 && token->end == -1)
                    {
                        if (token->type != type)
                            return JSMN_ERROR_INVAL;
                        parser->toksuper = -1;
                        token->end = parser->pos + 1;
                        break;
                    }
                }
                if (i == -1)
                    return JSMN_ERROR_INVAL;
                for (; i >= 0; i--)
                {
                    token = &tokens[i];
                    if (token->start != -1 && token->end == -1)
                    {
                        parser->toksuper = i;
                        break;
                    }
                }
            }
#endif
            break;
        case '\"':
            r = jsmn_parse_string(parser, js, tokens, num_tokens);
            if (r < 0)
                return r;
            count++;
            if (parser->toksuper != -1 && tokens != NULL)
            {
                tokens[parser->toksuper].size++;
            }
            break;
        case '\t':
        case '\r':
        case '\n':
        case ' ':
            // Whitespace - ignore
            break;
        case ':':
            parser->toksuper = parser->toknext - 1;
            break;
        case ',':
            if (tokens != NULL && parser->toksuper != -1 &&
                tokens[parser->toksuper].type != JSMN_ARRAY &&
                tokens[parser->toksuper].type != JSMN_OBJECT)
            {
#ifdef JSMN_PARENT_LINKS
                parser->toksuper = tokens[parser->toksuper].parent;
#else
                for (i = parser->toknext - 1; i >= 0; i--)
                {
                    if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT)
                    {
                        if (tokens[i].start != -1 && tokens[i].end == -1)
                        {
                            parser->toksuper = i;
                            break;
                        }
                    }
                }
#endif
            }
            break;
#ifdef JSMN_STRICT
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 't':
        case 'f':
        case 'n':
            if (tokens != NULL && parser->toksuper != -1)
            {
                const jsmntok_t *t = &tokens[parser->toksuper];
                if (t->type == JSMN_OBJECT ||
                    (t->type == JSMN_STRING && t->size != 0))
                {
                    return JSMN_ERROR_INVAL;
                }
            }
#else
        default:
#endif
            r = jsmn_parse_primitive(parser, js, tokens, num_tokens);
            if (r < 0)
                return r;
            count++;
            if (parser->toksuper != -1 && tokens != NULL)
            {
                tokens[parser->toksuper].size++;
            }
            break;
#ifdef JSMN_STRICT
        default:
            return JSMN_ERROR_INVAL;
#endif
        }
    }

    if (tokens != NULL)
    {
        for (i = parser->toknext - 1; i >= 0; i--)
        {
            if (tokens[i].start != -1 && tokens[i].end == -1)
            {
                return JSMN_ERROR_PART;
            }
        }
    }

    return count;
}

// Helper function to create a JSON object: {"key":"value"}
FuriString *get_json_furi(const FuriString *key, const FuriString *value)
{
    FuriString *result = furi_string_alloc();
    furi_string_printf(result, "{\"%s\":\"%s\"}",
                       furi_string_get_cstr(key),
                       furi_string_get_cstr(value));
    return result; // Caller responsible for furi_string_free
}

// Helper function to compare JSON keys
static int jsoneq_furi(const FuriString *json, jsmntok_t *tok, const FuriString *s)
{
    size_t s_len = furi_string_size(s);
    size_t tok_len = tok->end - tok->start;

    if (tok->type != JSMN_STRING)
        return -1;
    if (s_len != tok_len)
        return -1;

    FuriString *sub = furi_string_alloc_set(json);
    furi_string_mid(sub, tok->start, tok_len);

    int res = furi_string_cmp(sub, s);
    furi_string_free(sub);

    return (res == 0) ? 0 : -1;
}

// Skip a token and its descendants
static int skip_token(const jsmntok_t *tokens, int start, int total)
{
    if (start < 0 || start >= total)
        return -1;

    int i = start;
    if (tokens[i].type == JSMN_OBJECT)
    {
        int pairs = tokens[i].size;
        i++;
        for (int p = 0; p < pairs; p++)
        {
            i++; // skip key
            if (i >= total)
                return -1;
            i = skip_token(tokens, i, total); // skip value
            if (i == -1)
                return -1;
        }
        return i;
    }
    else if (tokens[i].type == JSMN_ARRAY)
    {
        int elems = tokens[i].size;
        i++;
        for (int e = 0; e < elems; e++)
        {
            i = skip_token(tokens, i, total);
            if (i == -1)
                return -1;
        }
        return i;
    }
    else
    {
        return i + 1;
    }
}

/**
 * Parse JSON and return the value associated with a given char* key.
 */
FuriString *get_json_value_furi(const char *key, const FuriString *json_data)
{
    if (json_data == NULL)
    {
        FURI_LOG_E("JSMM.H", "JSON data is NULL");
        return NULL;
    }
    uint32_t max_tokens = json_token_count_furi(json_data);
    if (!jsmn_memory_check(sizeof(jsmntok_t) * max_tokens))
    {
        FURI_LOG_E("JSMM.H", "Insufficient memory for JSON tokens.");
        return NULL;
    }
    // Create a temporary FuriString from key
    FuriString *key_str = furi_string_alloc();
    furi_string_cat_str(key_str, key);

    jsmn_parser parser;
    jsmn_init_furi(&parser);

    jsmntok_t *tokens = (jsmntok_t *)malloc(sizeof(jsmntok_t) * max_tokens);
    if (tokens == NULL)
    {
        FURI_LOG_E("JSMM.H", "Failed to allocate memory for JSON tokens.");
        furi_string_free(key_str);
        return NULL;
    }

    int ret = jsmn_parse_furi(&parser, json_data, tokens, max_tokens);
    if (ret < 0)
    {
        FURI_LOG_E("JSMM.H", "Failed to parse JSON: %d", ret);
        free(tokens);
        furi_string_free(key_str);
        return NULL;
    }

    if (ret < 1 || tokens[0].type != JSMN_OBJECT)
    {
        FURI_LOG_E("JSMM.H", "Root element is not an object.");
        free(tokens);
        furi_string_free(key_str);
        return NULL;
    }

    for (int i = 1; i < ret; i++)
    {
        if (jsoneq_furi(json_data, &tokens[i], key_str) == 0)
        {
            int length = tokens[i + 1].end - tokens[i + 1].start;
            FuriString *value = furi_string_alloc_set(json_data);
            furi_string_mid(value, tokens[i + 1].start, length);
            free(tokens);
            furi_string_free(key_str);
            return value;
        }
    }

    free(tokens);
    furi_string_free(key_str);
    char warning[128];
    snprintf(warning, sizeof(warning), "Failed to find the key \"%s\" in the JSON.", key);
    FURI_LOG_E("JSMM.H", warning);
    return NULL;
}

/**
 * Return the value at a given index in a JSON array for a given char* key.
 */
FuriString *get_json_array_value_furi(const char *key, uint32_t index, const FuriString *json_data)
{
    FuriString *array_str = get_json_value_furi(key, json_data);
    if (array_str == NULL)
    {
        FURI_LOG_E("JSMM.H", "Failed to get array for key");
        return NULL;
    }
    uint32_t max_tokens = json_token_count_furi(array_str);
    if (!jsmn_memory_check(sizeof(jsmntok_t) * max_tokens))
    {
        FURI_LOG_E("JSMM.H", "Insufficient memory for JSON tokens.");
        furi_string_free(array_str);
        return NULL;
    }
    jsmn_parser parser;
    jsmn_init_furi(&parser);

    jsmntok_t *tokens = (jsmntok_t *)malloc(sizeof(jsmntok_t) * max_tokens);
    if (tokens == NULL)
    {
        FURI_LOG_E("JSMM.H", "Failed to allocate memory for JSON tokens.");
        furi_string_free(array_str);
        return NULL;
    }

    int ret = jsmn_parse_furi(&parser, array_str, tokens, max_tokens);
    if (ret < 0)
    {
        FURI_LOG_E("JSMM.H", "Failed to parse JSON array: %d", ret);
        free(tokens);
        furi_string_free(array_str);
        return NULL;
    }

    if (ret < 1 || tokens[0].type != JSMN_ARRAY)
    {
        FURI_LOG_E("JSMM.H", "Value for key is not an array.");
        free(tokens);
        furi_string_free(array_str);
        return NULL;
    }

    if (index >= (uint32_t)tokens[0].size)
    {
        // FURI_LOG_E("JSMM.H", "Index %lu out of bounds for array with size %u.", index, tokens[0].size);
        free(tokens);
        furi_string_free(array_str);
        return NULL;
    }

    int elem_token = 1;
    for (uint32_t i = 0; i < index; i++)
    {
        elem_token = skip_token(tokens, elem_token, ret);
        if (elem_token == -1 || elem_token >= ret)
        {
            FURI_LOG_E("JSMM.H", "Error skipping tokens to reach element %lu.", i);
            free(tokens);
            furi_string_free(array_str);
            return NULL;
        }
    }

    jsmntok_t element = tokens[elem_token];
    int length = element.end - element.start;

    FuriString *value = furi_string_alloc_set(array_str);
    furi_string_mid(value, element.start, length);

    free(tokens);
    furi_string_free(array_str);

    return value;
}

/**
 * Extract all object values from a JSON array associated with a given char* key.
 */
FuriString **get_json_array_values_furi(const char *key, const FuriString *json_data, int *num_values)
{
    *num_values = 0;
    // Convert key to FuriString and call get_json_value_furi
    FuriString *array_str = get_json_value_furi(key, json_data);
    if (array_str == NULL)
    {
        FURI_LOG_E("JSMM.H", "Failed to get array for key");
        return NULL;
    }

    uint32_t max_tokens = json_token_count_furi(array_str);
    if (!jsmn_memory_check(sizeof(jsmntok_t) * max_tokens))
    {
        FURI_LOG_E("JSMM.H", "Insufficient memory for JSON tokens.");
        furi_string_free(array_str);
        return NULL;
    }
    jsmn_parser parser;
    jsmn_init_furi(&parser);

    jsmntok_t *tokens = (jsmntok_t *)malloc(sizeof(jsmntok_t) * max_tokens);
    if (tokens == NULL)
    {
        FURI_LOG_E("JSMM.H", "Failed to allocate memory for JSON tokens.");
        furi_string_free(array_str);
        return NULL;
    }

    int ret = jsmn_parse_furi(&parser, array_str, tokens, max_tokens);
    if (ret < 0)
    {
        FURI_LOG_E("JSMM.H", "Failed to parse JSON array: %d", ret);
        free(tokens);
        furi_string_free(array_str);
        return NULL;
    }

    if (tokens[0].type != JSMN_ARRAY)
    {
        FURI_LOG_E("JSMM.H", "Value for key is not an array.");
        free(tokens);
        furi_string_free(array_str);
        return NULL;
    }

    int array_size = tokens[0].size;
    FuriString **values = (FuriString **)malloc(array_size * sizeof(FuriString *));
    if (values == NULL)
    {
        FURI_LOG_E("JSMM.H", "Failed to allocate memory for array of values.");
        free(tokens);
        furi_string_free(array_str);
        return NULL;
    }

    int actual_num_values = 0;
    int current_token = 1;
    for (int i = 0; i < array_size; i++)
    {
        if (current_token >= ret)
        {
            FURI_LOG_E("JSMM.H", "Unexpected end of tokens while traversing array.");
            break;
        }

        jsmntok_t element = tokens[current_token];

        int length = element.end - element.start;
        FuriString *value = furi_string_alloc_set(array_str);
        furi_string_mid(value, element.start, length);

        values[actual_num_values] = value;
        actual_num_values++;

        // Skip this element and its descendants
        current_token = skip_token(tokens, current_token, ret);
        if (current_token == -1)
        {
            FURI_LOG_E("JSMM.H", "Error skipping tokens after element %d.", i);
            break;
        }
    }

    *num_values = actual_num_values;
    if (actual_num_values < array_size)
    {
        FuriString **reduced_values = (FuriString **)realloc(values, actual_num_values * sizeof(FuriString *));
        if (reduced_values != NULL)
        {
            values = reduced_values;
        }
    }

    free(tokens);
    furi_string_free(array_str);
    return values;
}

uint32_t json_token_count_furi(const FuriString *json)
{
    if (json == NULL)
    {
        return JSMN_ERROR_INVAL;
    }

    jsmn_parser parser;
    jsmn_init_furi(&parser);

    // Pass NULL for tokens and 0 for num_tokens to get the token count only
    int ret = jsmn_parse_furi(&parser, json, NULL, 0);
    return ret; // If ret >= 0, it represents the number of tokens needed.
}
