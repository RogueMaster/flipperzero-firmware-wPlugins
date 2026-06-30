#include "auto_complete.h"
#include MEMORY_INCLUDE

// Get index for character (a-z maps to 0-25, space maps to 26)
static int char_to_index(char c)
{
    c = tolower(c);
    if (c >= 'a' && c <= 'z')
    {
        return c - 'a';
    }
    if (c == ' ')
    {
        return 26; // Space gets index 26
    }
    return -1; // Invalid character
}

// Recursively collect all words from node
static void collect_words(TrieNode *node, AutoComplete *context)
{
    if (!node || context->suggestion_count >= MAX_SUGGESTIONS)
        return;

    if (node->is_end_of_word && node->word)
    {
        const size_t len = strlen(node->word);

        // Reallocate suggestions array to fit one more suggestion
        char **new_suggestions = (char **)MEMORY_REALLOC(context->suggestions,
                                                         (context->suggestion_count + 1) * sizeof(char *));
        if (!new_suggestions)
            return;

        context->suggestions = new_suggestions;
        context->suggestions[context->suggestion_count] = (char *)MEMORY_ALLOC(len + 1);
        if (context->suggestions[context->suggestion_count])
        {
            snprintf(context->suggestions[context->suggestion_count], len + 1, "%s", node->word);
            context->suggestion_count++;
        }
        if (context->suggestion_count >= MAX_SUGGESTIONS)
            return;
    }

    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        if (node->children[i])
        {
            collect_words(node->children[i], context);
            if (context->suggestion_count >= MAX_SUGGESTIONS)
                return;
        }
    }
}

// Create new trie node
static TrieNode *create_node(void)
{
    TrieNode *node = (TrieNode *)MEMORY_ALLOC(sizeof(TrieNode));
    if (!node)
        return NULL;

    node->is_end_of_word = false;
    node->word = NULL;
    for (int i = 0; i < ALPHABET_SIZE; i++)
    {
        node->children[i] = NULL;
    }
    return node;
}

// Recursively free trie nodes
static void free_trie(TrieNode *node)
{
    if (node)
    {
        for (int i = 0; i < ALPHABET_SIZE; i++)
        {
            if (node->children[i])
            {
                free_trie(node->children[i]);
            }
        }

        if (node->word)
        {
            MEMORY_FREE(node->word);
            node->word = NULL;
        }

        MEMORY_FREE(node);
        node = NULL;
    }
}

#if defined(STORAGE_INCLUDE) && defined(STORAGE_READ)
bool auto_complete_add_dictionary(AutoComplete *context, const char *filename)
{
    if (!context || !filename)
        return false;

    char *buffer = (char *)MEMORY_ALLOC(STORAGE_MAX_READ_SIZE);
    if (!buffer)
        return false;

    size_t bytes_read = STORAGE_READ(filename, buffer, STORAGE_MAX_READ_SIZE);
    if (bytes_read == 0)
    {
        MEMORY_FREE(buffer);
        return false;
    }

    char *start = buffer;
    char *end = buffer;
    int count = 0;

    while (*end && count < MAX_SUGGESTIONS)
    {
        if (*end == '\n' || *end == '\r')
        {
            *end = '\0'; // Null-terminate the word
            if (strlen(start) > 0)
            {
                auto_complete_add_word(context, start);
                count++;
            }
            end++;
            start = end;
        }
        else
        {
            end++;
        }
    }

    MEMORY_FREE(buffer);
    return true;
}
#endif

// Add word to dictionary
bool auto_complete_add_word(AutoComplete *context, const char *word)
{
    const size_t len = strlen(word);

    if (!context || !word || len == 0 || !context->root)
    {
        return false;
    }
    TrieNode *current = context->root;

    for (int i = 0; word[i] != '\0'; i++)
    {
        int idx = char_to_index(word[i]);
        if (idx == -1)
            continue; // Skip invalid chars

        if (current->children[idx] == NULL)
        {
            current->children[idx] = create_node();
            if (!current->children[idx])
            {
                return false;
            }
        }
        current = current->children[idx];
    }

    current->is_end_of_word = true;

    // Store complete word at end node
    current->word = (char *)MEMORY_ALLOC(len + 1);
    if (current->word)
    {
        snprintf(current->word, len + 1, "%s", word);
        return true;
    }

    return false;
}

// Free all memory
void auto_complete_free(AutoComplete *context)
{
    if (context && context->root)
    {
        free_trie(context->root);
        context->root = NULL;

        // Free suggestions
        if (context->suggestions)
        {
            for (uint8_t i = 0; i < context->suggestion_count; i++)
            {
                if (context->suggestions[i])
                {
                    MEMORY_FREE(context->suggestions[i]);
                    context->suggestions[i] = NULL;
                }
            }
            MEMORY_FREE(context->suggestions);
            context->suggestions = NULL;
        }
        context->suggestion_count = 0;
    }
}

// Initialize autocomplete context
bool auto_complete_init(AutoComplete *context)
{
    if (!context)
        return false;

    context->root = create_node();
    context->suggestion_count = 0;
    context->suggestions = NULL;

    return context->root != NULL;
}

// Remove all current suggestions
void auto_complete_remove_suggestions(AutoComplete *context)
{
    if (context)
    {
        for (uint8_t i = 0; i < context->suggestion_count; i++)
        {
            if (context->suggestions[i])
            {
                MEMORY_FREE(context->suggestions[i]);
                context->suggestions[i] = NULL;
            }
        }
        MEMORY_FREE(context->suggestions);
        context->suggestions = NULL;
        context->suggestion_count = 0;
    }
}

// Remove all words from dictionary
void auto_complete_remove_words(AutoComplete *context)
{
    if (context)
    {
        free_trie(context->root);
        context->root = create_node();
    }
}

// Search for completions
bool auto_complete_search(AutoComplete *context, const char *prefix)
{
    if (!context || !prefix || !context->root)
        return false;

    // Free previous suggestions
    auto_complete_remove_suggestions(context);

    TrieNode *current = context->root;

    // Navigate to prefix end
    for (int i = 0; prefix[i] != '\0'; i++)
    {
        int idx = char_to_index(prefix[i]);
        if (idx == -1 || current->children[idx] == NULL)
        {
            return false; // Prefix not found
        }
        current = current->children[idx];
    }

    // Collect all words with this prefix
    collect_words(current, context);
    return true;
}
