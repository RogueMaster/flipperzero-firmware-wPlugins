#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <input/input.h>

#define MAX_PASSCODE_SEQUENCE_LEN 64  // Max button sequence length (e.g., "up down up up left right ok back")
#define MIN_PASSCODE_BUTTONS 4        // Minimum 4 buttons for passcode
#define MAX_PASSCODE_BUTTONS 8        // Maximum 8 buttons for passcode
#define PASSCODE_HEADER_SIZE 3       // 2 bytes for passcode length + 1 byte for flags in cards.enc header
#define PASSCODE_FLAG_DISABLED 0x01  // Flag bit for passcode disabled

// Get button sequence passcode from encrypted settings storage
// Returns the button sequence as a string (e.g., "up down up up")
bool get_passcode_sequence(char* sequence_out, size_t sequence_buf_size);

// Set button sequence passcode in encrypted settings storage
// sequence: button sequence string like "up down up up" (must be 4-8 buttons)
// Returns false if sequence is invalid (wrong length) or encryption fails
bool set_passcode_sequence(const char* sequence);

// Check if passcode is set (checks if encrypted passcode exists in settings)
bool has_passcode(void);

// Verify button sequence passcode against stored encrypted value
bool verify_passcode_sequence(const char* sequence);

// Count number of buttons in a sequence string
// Returns 0 if invalid
size_t count_buttons_in_sequence(const char* sequence);

// Derive encryption key from button sequence using PBKDF2
// Used for encrypting card data with passcode
bool derive_key_from_passcode_sequence(const char* sequence, uint8_t* key_out, size_t key_len);

// Delete cards.enc file and reset passcode (security feature after too many failed attempts)
// Returns true if deletion was successful
bool delete_cards_and_reset_passcode(void);

// Reset passcode while preserving cards (if they can be decrypted with hardware encryption)
// Returns true if reset was successful
// Note: Cards encrypted with passcode-based encryption will be lost if old passcode is unknown
bool reset_passcode_preserve_cards(void);

// Get passcode_disabled flag from encrypted cards.enc header
// Returns true if passcode is disabled, false otherwise
bool get_passcode_disabled(void);

// Set passcode_disabled flag in encrypted cards.enc header
// Returns true if flag was set successfully
bool set_passcode_disabled(bool disabled);
