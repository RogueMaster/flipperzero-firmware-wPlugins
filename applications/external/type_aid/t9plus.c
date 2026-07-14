#include "t9plus.h"
#include <furi.h>
#include <storage/storage.h>
#include <ctype.h>
#include <string.h>

#define TAG "T9Plus"

// Maximum words per tier
#define MAX_TIER_WORDS 1000
#define MAX_WORD_LEN   32

typedef struct {
    char** words;
    size_t count;
    size_t capacity;
} WordTier;

static struct {
    WordTier tier1; // Function words
    WordTier tier2; // Common lemmas
    WordTier tier3a; // Chat/internet slang
    WordTier tier3b; // Fillers
    WordTier tier4; // Formal discourse
    bool initialized;
    bool has_load_errors; // Track if any files failed to load
    char error_message[64]; // Store error message for display
} t9plus_state = {0};

// Helper: Allocate word tier
static bool tier_alloc(WordTier* tier, size_t capacity) {
    tier->words = malloc(capacity * sizeof(char*));
    if(!tier->words) return false;
    tier->capacity = capacity;
    tier->count = 0;
    return true;
}

// Helper: Free word tier
static void tier_free(WordTier* tier) {
    if(tier->words) {
        for(size_t i = 0; i < tier->count; i++) {
            free(tier->words[i]);
        }
        free(tier->words);
        tier->words = NULL;
    }
    tier->count = 0;
    tier->capacity = 0;
}

// Helper: Add word to tier
static bool tier_add_word(WordTier* tier, const char* word) {
    if(tier->count >= tier->capacity) return false;

    tier->words[tier->count] = strdup(word);
    if(!tier->words[tier->count]) return false;

    tier->count++;
    return true;
}

// Helper: Load words from file
static bool load_tier_from_file(const char* path, WordTier* tier) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    FURI_LOG_I(TAG, "Loading tier from: %s", path);

    bool success = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_I(TAG, "File opened successfully: %s", path);
        char buffer[MAX_WORD_LEN + 2]; // +2 for newline and null terminator
        size_t bytes_read;
        size_t pos = 0;

        while(true) {
            bytes_read = storage_file_read(file, buffer + pos, 1);
            if(bytes_read == 0) {
                // End of file - process last word if exists
                if(pos > 0) {
                    buffer[pos] = '\0';
                    // Trim whitespace
                    while(pos > 0 && isspace((unsigned char)buffer[pos - 1])) {
                        buffer[--pos] = '\0';
                    }
                    if(pos > 0 && buffer[0] != '#') {
                        tier_add_word(tier, buffer);
                    }
                }
                break;
            }

            // Check for newline
            if(buffer[pos] == '\n' || buffer[pos] == '\r') {
                buffer[pos] = '\0';
                // Trim trailing whitespace
                while(pos > 0 && isspace((unsigned char)buffer[pos - 1])) {
                    buffer[--pos] = '\0';
                }

                // Add word if not empty and not a comment
                if(pos > 0 && buffer[0] != '#') {
                    tier_add_word(tier, buffer);
                }
                pos = 0;
            } else if(pos < MAX_WORD_LEN) {
                pos++;
            } else {
                // Word too long, skip to next line
                while(bytes_read > 0 && buffer[pos] != '\n' && buffer[pos] != '\r') {
                    bytes_read = storage_file_read(file, buffer + pos, 1);
                }
                pos = 0;
            }
        }

        success = true;
        FURI_LOG_I(TAG, "Loaded %zu words from %s", tier->count, path);
        storage_file_close(file);
    } else {
        FURI_LOG_E(TAG, "Failed to open file: %s", path);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

bool t9plus_init(void) {
    if(t9plus_state.initialized) {
        FURI_LOG_W(TAG, "Already initialized");
        return true;
    }

    FURI_LOG_I(TAG, "Initializing T9+ prediction system");

    // Allocate tiers
    if(!tier_alloc(&t9plus_state.tier1, MAX_TIER_WORDS)) {
        FURI_LOG_E(TAG, "Failed to allocate tier1");
        return false;
    }
    if(!tier_alloc(&t9plus_state.tier2, MAX_TIER_WORDS)) {
        FURI_LOG_E(TAG, "Failed to allocate tier2");
        tier_free(&t9plus_state.tier1);
        return false;
    }
    if(!tier_alloc(&t9plus_state.tier3a, MAX_TIER_WORDS)) {
        FURI_LOG_E(TAG, "Failed to allocate tier3a");
        tier_free(&t9plus_state.tier1);
        tier_free(&t9plus_state.tier2);
        return false;
    }
    if(!tier_alloc(&t9plus_state.tier3b, MAX_TIER_WORDS)) {
        FURI_LOG_E(TAG, "Failed to allocate tier3b");
        tier_free(&t9plus_state.tier1);
        tier_free(&t9plus_state.tier2);
        tier_free(&t9plus_state.tier3a);
        return false;
    }
    if(!tier_alloc(&t9plus_state.tier4, MAX_TIER_WORDS)) {
        FURI_LOG_E(TAG, "Failed to allocate tier4");
        tier_free(&t9plus_state.tier1);
        tier_free(&t9plus_state.tier2);
        tier_free(&t9plus_state.tier3a);
        tier_free(&t9plus_state.tier3b);
        return false;
    }

    // Load tier files
    bool all_loaded = true;
    int failed_count = 0;

    if(!load_tier_from_file(
           "/ext/apps_data/type_aid/data/tier1_function_words.txt", &t9plus_state.tier1)) {
        all_loaded = false;
        failed_count++;
    }
    if(!load_tier_from_file(
           "/ext/apps_data/type_aid/data/tier2_lemma_list.txt", &t9plus_state.tier2)) {
        all_loaded = false;
        failed_count++;
    }
    if(!load_tier_from_file("/ext/apps_data/type_aid/data/tier3a_chat.txt", &t9plus_state.tier3a)) {
        all_loaded = false;
        failed_count++;
    }
    if(!load_tier_from_file(
           "/ext/apps_data/type_aid/data/tier3b_fillers.txt", &t9plus_state.tier3b)) {
        all_loaded = false;
        failed_count++;
    }
    if(!load_tier_from_file(
           "/ext/apps_data/type_aid/data/tier4_formal_discourse.txt", &t9plus_state.tier4)) {
        all_loaded = false;
        failed_count++;
    }

    // TEMPORARY: Add hardcoded test words if files didn't load
    if(t9plus_state.tier1.count == 0) {
        FURI_LOG_W(TAG, "Tier1 empty, adding hardcoded test words");
        tier_add_word(&t9plus_state.tier1, "the");
        tier_add_word(&t9plus_state.tier1, "that");
        tier_add_word(&t9plus_state.tier1, "this");
        tier_add_word(&t9plus_state.tier1, "to");
        tier_add_word(&t9plus_state.tier1, "it");
        tier_add_word(&t9plus_state.tier1, "is");
        tier_add_word(&t9plus_state.tier1, "in");
        tier_add_word(&t9plus_state.tier1, "and");
        tier_add_word(&t9plus_state.tier1, "have");
        tier_add_word(&t9plus_state.tier1, "we");
        tier_add_word(&t9plus_state.tier1, "were");
        tier_add_word(&t9plus_state.tier1, "will");
        tier_add_word(&t9plus_state.tier1, "would");
        tier_add_word(&t9plus_state.tier1, "hello");
        tier_add_word(&t9plus_state.tier1, "help");
        tier_add_word(&t9plus_state.tier1, "world");
        tier_add_word(&t9plus_state.tier1, "work");
        FURI_LOG_I(TAG, "Added %zu hardcoded words to tier1", t9plus_state.tier1.count);
    }

    // Set error message if files failed to load
    if(!all_loaded) {
        t9plus_state.has_load_errors = true;
        if(failed_count == 5) {
            snprintf(
                t9plus_state.error_message,
                sizeof(t9plus_state.error_message),
                "ERROR: No data files found!");
        } else {
            snprintf(
                t9plus_state.error_message,
                sizeof(t9plus_state.error_message),
                "WARNING: %d data file(s) missing",
                failed_count);
        }
        FURI_LOG_W(TAG, "%s", t9plus_state.error_message);
    } else {
        t9plus_state.has_load_errors = false;
        t9plus_state.error_message[0] = '\0';
    }

    FURI_LOG_I(
        TAG,
        "Loaded words: tier1=%zu, tier2=%zu, tier3a=%zu, tier3b=%zu, tier4=%zu",
        t9plus_state.tier1.count,
        t9plus_state.tier2.count,
        t9plus_state.tier3a.count,
        t9plus_state.tier3b.count,
        t9plus_state.tier4.count);

    t9plus_state.initialized = true;
    return true;
}

void t9plus_deinit(void) {
    if(!t9plus_state.initialized) return;

    FURI_LOG_I(TAG, "Shutting down T9+");

    tier_free(&t9plus_state.tier1);
    tier_free(&t9plus_state.tier2);
    tier_free(&t9plus_state.tier3a);
    tier_free(&t9plus_state.tier3b);
    tier_free(&t9plus_state.tier4);

    t9plus_state.initialized = false;
}

bool t9plus_is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '\'';
}

const char* t9plus_get_error_message(void) {
    if(!t9plus_state.initialized) {
        return "T9+ not initialized";
    }

    if(t9plus_state.has_load_errors) {
        return t9plus_state.error_message;
    }

    return NULL; // No error
}

// Helper: Case-insensitive prefix match
static bool starts_with_ci(const char* word, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    size_t word_len = strlen(word);

    if(word_len < prefix_len) {
        return false;
    }

    for(size_t i = 0; i < prefix_len; i++) {
        char w = tolower((unsigned char)word[i]);
        char p = tolower((unsigned char)prefix[i]);
        if(w != p) {
            return false;
        }
    }

    return true;
}

// Helper: Search tier for matches
static void search_tier(
    const WordTier* tier,
    const char* prefix,
    char suggestions[T9PLUS_MAX_SUGGESTIONS][T9PLUS_MAX_WORD_LENGTH],
    uint8_t* found_count,
    uint8_t max_suggestions) {
    FURI_LOG_I(TAG, "search_tier: searching %zu words for prefix '%s'", tier->count, prefix);
    size_t matches_in_tier = 0;

    for(size_t i = 0; i < tier->count && *found_count < max_suggestions; i++) {
        if(starts_with_ci(tier->words[i], prefix)) {
            FURI_LOG_I(TAG, "  MATCH: '%s' matches '%s'", tier->words[i], prefix);
            strncpy(suggestions[*found_count], tier->words[i], T9PLUS_MAX_WORD_LENGTH - 1);
            suggestions[*found_count][T9PLUS_MAX_WORD_LENGTH - 1] = '\0';
            (*found_count)++;
            matches_in_tier++;
        }
    }

    if(matches_in_tier > 0) {
        FURI_LOG_I(TAG, "search_tier: found %zu matches", matches_in_tier);
    } else {
        FURI_LOG_I(TAG, "search_tier: no matches found");
    }
}

uint8_t t9plus_get_suggestions(
    const char* input,
    char suggestions[T9PLUS_MAX_SUGGESTIONS][T9PLUS_MAX_WORD_LENGTH],
    uint8_t max_suggestions) {
    FURI_LOG_I(TAG, "=== get_suggestions called ===");
    FURI_LOG_I(TAG, "Input buffer: '%s'", input ? input : "(null)");

    if(!t9plus_state.initialized) {
        FURI_LOG_W(TAG, "Not initialized!");
        return 0;
    }

    if(!input || strlen(input) == 0) {
        FURI_LOG_I(TAG, "Empty input, returning 0");
        return 0;
    }

    if(max_suggestions > T9PLUS_MAX_SUGGESTIONS) {
        max_suggestions = T9PLUS_MAX_SUGGESTIONS;
    }

    // Extract the last word from input - find start of last word
    const char* last_word_start = input;
    size_t input_len = strlen(input);

    FURI_LOG_I(TAG, "Input length: %zu", input_len);

    // Scan backwards from end to find last word boundary
    if(input_len > 0) {
        // Start from end of string
        const char* p = input + input_len - 1;

        // Skip trailing whitespace
        while(p >= input && (*p == ' ' || *p == '\n' || *p == '\r')) {
            p--;
        }

        // Now find the start of this word
        const char* word_end = p + 1;
        while(p >= input && *p != ' ' && *p != '\n' && *p != '\r') {
            p--;
        }
        last_word_start = p + 1;

        // Check if we have a valid word
        if(last_word_start >= word_end) {
            FURI_LOG_I(TAG, "No word found in input");
            return 0;
        }
    }

    // Create a temporary buffer for the last word
    char last_word[MAX_WORD_LEN];
    size_t word_len = 0;
    const char* p = last_word_start;
    while(*p && *p != ' ' && *p != '\n' && *p != '\r' && word_len < MAX_WORD_LEN - 1) {
        last_word[word_len++] = *p++;
    }
    last_word[word_len] = '\0';

    // Skip if last word is empty
    if(word_len == 0) {
        FURI_LOG_I(TAG, "Extracted word is empty");
        return 0;
    }

    FURI_LOG_I(TAG, "Searching for prefix: '%s' (length: %zu)", last_word, word_len);
    FURI_LOG_I(
        TAG,
        "Tier sizes: tier1=%zu, tier2=%zu, tier3a=%zu, tier3b=%zu, tier4=%zu",
        t9plus_state.tier1.count,
        t9plus_state.tier2.count,
        t9plus_state.tier3a.count,
        t9plus_state.tier3b.count,
        t9plus_state.tier4.count);

    uint8_t found = 0;

    // Search tiers in priority order: tier1, tier3a, tier3b, tier2, tier4
    FURI_LOG_I(TAG, "Searching tier1...");
    search_tier(&t9plus_state.tier1, last_word, suggestions, &found, max_suggestions);
    FURI_LOG_I(TAG, "After tier1: found=%d", found);

    if(found < max_suggestions) {
        FURI_LOG_I(TAG, "Searching tier3a...");
        search_tier(&t9plus_state.tier3a, last_word, suggestions, &found, max_suggestions);
        FURI_LOG_I(TAG, "After tier3a: found=%d", found);
    }
    if(found < max_suggestions) {
        FURI_LOG_I(TAG, "Searching tier3b...");
        search_tier(&t9plus_state.tier3b, last_word, suggestions, &found, max_suggestions);
        FURI_LOG_I(TAG, "After tier3b: found=%d", found);
    }
    if(found < max_suggestions) {
        FURI_LOG_I(TAG, "Searching tier2...");
        search_tier(&t9plus_state.tier2, last_word, suggestions, &found, max_suggestions);
        FURI_LOG_I(TAG, "After tier2: found=%d", found);
    }
    if(found < max_suggestions) {
        FURI_LOG_I(TAG, "Searching tier4...");
        search_tier(&t9plus_state.tier4, last_word, suggestions, &found, max_suggestions);
        FURI_LOG_I(TAG, "After tier4: found=%d", found);
    }

    FURI_LOG_I(TAG, "=== Returning %d suggestions ===", found);
    if(found > 0) {
        for(uint8_t i = 0; i < found; i++) {
            FURI_LOG_I(TAG, "  Suggestion %d: '%s'", i, suggestions[i]);
        }
    }

    return found;
}
