#pragma once

#include <furi.h>

// Maximum number of suggestions to provide
#define T9PLUS_MAX_SUGGESTIONS 3

// Maximum word length for suggestions
#define T9PLUS_MAX_WORD_LENGTH 32

/**
 * @brief Initialize the T9+ prediction system
 * 
 * Loads vocabulary tiers from data files in the app's assets.
 * Must be called before using any prediction functions.
 * 
 * @return true if initialization successful, false otherwise
 */
bool t9plus_init(void);

/**
 * @brief Clean up and free resources used by T9+ system
 */
void t9plus_deinit(void);

/**
 * @brief Get word suggestions based on current input
 * 
 * @param input Current partial word (e.g., "hel")
 * @param suggestions Array to store up to 3 suggestions (must be pre-allocated)
 * @param max_suggestions Maximum number of suggestions to return (typically 3)
 * @return Number of suggestions actually returned (0-3)
 */
uint8_t t9plus_get_suggestions(
    const char* input,
    char suggestions[T9PLUS_MAX_SUGGESTIONS][T9PLUS_MAX_WORD_LENGTH],
    uint8_t max_suggestions);

/**
 * @brief Check if a character is valid for word prediction
 * 
 * @param c Character to check
 * @return true if character is alphanumeric or apostrophe
 */
bool t9plus_is_word_char(char c);

/**
 * @brief Get error message if files failed to load
 * 
 * @return Pointer to error message string, or NULL if no errors
 */
const char* t9plus_get_error_message(void);
