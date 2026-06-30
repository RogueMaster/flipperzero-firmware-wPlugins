#pragma once

#ifndef TOYPADEMU_H
#define TOYPADEMU_H

#include <furi.h>

#define MAX_TOKENS 7
#define NUM_BOXES  7 // The number of boxes (7 boxes always)

#ifdef __cplusplus
extern "C" {
#endif

// Quick Swap mode option flag
extern bool quick_swap;

/**
 * @brief      Token struct representing a minifigure or vehicle token.
 */
typedef struct {
    unsigned char index;
    unsigned int id;
    unsigned int pad;
    unsigned char uid[7];
    uint8_t token[180]; // token data, used for Vehicle upgrades.
    char name[16];
} Token;

/**
 * @brief      Box information struct, for drawing and placing tokens. and keeping track of indexes for tokens.
 */
typedef struct BoxInfo {
    const uint8_t x; // X-coordinate
    const uint8_t y; // Y-coordinate
    bool isFilled; // Indicates if the box is filled with a Token (minifig / vehicle)
    int index; // The index of the token in the box
} BoxInfo;

/**
 * @brief      ToyPad Emulator struct containing currently placed tokens and the TEA encryption key.
 */
typedef struct {
    Token* tokens[MAX_TOKENS];
    uint8_t tea_key[16];
} ToyPadEmu;

/**
 * @brief      Get the singleton instance of the ToyPadEmu
 * @return     The ToyPadEmu instance pointer
 */
extern ToyPadEmu* emulator;

/**
 * @brief      Remove a token from the toypad emulator
 * @param      index  The index of the token to remove
 * @return     true if successful, false otherwise
 */
bool ToyPadEmu_remove(int index);

/**
 * @brief      Clear the box info, and free the left over token memory.
 */
void ToyPadEmu_clear();

/**
 * @brief      Remove all tokens from the toypad emulator, afterwards clears the box info (ToyPadEmu_clear).
 */
void ToyPadEmu_remove_all_tokens();

/**
 * @brief      Get the number of tokens currently placed on the toypad
 * @return     The token count
 */
uint8_t ToyPadEmu_get_token_count();

/**
 * @brief      Place a token on the toypad emulator at the selected box.
 * @param      token        The allocated token to place
 * @param      selectedBox  The box to place the token in
 * @return     true if successful, false otherwise
 */
bool ToyPadEmu_place(Token* token, int selectedBox);

/**
 * @brief      Place multiple tokens on the toypad emulator according to the box info.
 * @param      tokens  The array of tokens to place
 * @param      boxes   The box info array indicating where to place each token on which index
 */
void ToyPadEmu_place_tokens(Token* tokens[MAX_TOKENS], BoxInfo boxes[NUM_BOXES]);

/**
 * @brief      Remove old token with the same UID from the toypad emulator, used when placing a new token in (ToyPadEmu_place_token).
 * @param      token  The token to check and remove if an old one exists
 */
void ToyPadEmu_remove_old_token(Token* token);

/**
 * @brief      Get the number of tokens with the same ID currently placed on the toypad
 * @param      id    The ID to check for
 * @return     The count of tokens with the same ID
 */
int ToyPadEmu_get_token_count_by_id(unsigned int id);

/**
 * @brief      Check if the token is a vehicle
 * @param      token  The token to check
 * @return     true if vehicle, false otherwise
 */
static inline bool is_vehicle(const Token* token) {
    return !token->id;
}

/**
 * @brief      Check if the token is a minifigure
 * @param      token  The token to check
 * @return     true if minifigure, false otherwise
 */
static inline bool is_minifig(const Token* token) {
    return token->id != 0;
}

/**
 * @brief      Free the ToyPadEmu instance and its resources.
 */
void ToyPadEmu_free();

/**
 * @brief      Get a token from the ToyPad emulator by its array slot index.
 * @param      index  The index of the token
 * @return     The token pointer, or NULL if not found
 */
Token* ToyPadEmu_get_token(int index);

/**
 * @brief      Searches through all placed tokens to find a token whose token->index matches the given index.
 * @param      index  The index to search for
 * @return     The token pointer, or NULL if not found
 */
Token* ToyPadEmu_find_token_by_index(int index);

/**
 * @brief      Create a minifigure token
 * @param      id    The id of the minifigure
 * @return     The created and allocated token pointer
 */
Token* createCharacter(int id);

/**
 * @brief      Create a vehicle token
 * @param      id        The id of the vehicle
 * @param      upgrades  The upgrades array for the vehicle
 * @return     The created and allocated token pointer
 */
Token* createVehicle(int id, uint32_t upgrades[2]);

#ifdef __cplusplus
}
#endif

#endif // TOYPADEMU_H