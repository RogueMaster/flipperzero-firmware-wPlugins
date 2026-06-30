#pragma once

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

#include "auto_complete_config.h"

#define MAX_WORD_LENGTH 32
#define MAX_SUGGESTIONS 10
#define ALPHABET_SIZE 27 // a-z + space

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct TrieNode
    {
        struct TrieNode *children[ALPHABET_SIZE];
        bool is_end_of_word;
        char *word; // Store complete word at end nodes
    } TrieNode;

    typedef struct
    {
        TrieNode *root;
        char **suggestions;
        uint8_t suggestion_count;
    } AutoComplete;

#if defined(STORAGE_INCLUDE) && defined(STORAGE_READ)
#include STORAGE_INCLUDE
    bool auto_complete_add_dictionary(AutoComplete *context, const char *filename);
#endif

    bool auto_complete_add_word(AutoComplete *context, const char *word);
    void auto_complete_free(AutoComplete *context);
    bool auto_complete_init(AutoComplete *context);
    void auto_complete_remove_suggestions(AutoComplete *context);
    void auto_complete_remove_words(AutoComplete *context);
    bool auto_complete_search(AutoComplete *context, const char *prefix);

#ifdef __cplusplus
}
#endif

/* Example

    AutoComplete ac;

    if (!auto_complete_init(&ac))
    {
        printf("Failed to initialize autocomplete");
        return;
    }

    auto_complete_add_word(&ac, "hello");
    auto_complete_add_word(&ac, "hey");
    auto_complete_add_word(&ac, "hi");
    auto_complete_add_word(&ac, "howdy");
    auto_complete_add_word(&ac, "hola");
    auto_complete_add_word(&ac, "test");
    auto_complete_add_word(&ac, "school");

    if (!auto_complete_search(&ac, "he"))
    {
        printf("Autocomplete search failed");
        auto_complete_free(&ac);
        return;
    }

    for (uint8_t i = 0; i < ac.suggestion_count; i++)
    {
        printf("Suggestion %d: %s", i, ac.suggestions[i]);
    }

    auto_complete_free(&ac);

*/