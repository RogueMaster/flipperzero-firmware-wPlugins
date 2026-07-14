#pragma once

#include "ccid_emulator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocate a new CcidCard and parse the .ccid file at `path`.
 *
 * @param  storage  open Storage* handle
 * @param  path     full path to the .ccid file on the SD card
 * @return          heap-allocated CcidCard on success, NULL on failure
 */
CcidCard* ccid_card_load(Storage* storage, const char* path);

/**
 * Free a card previously returned by ccid_card_load().
 */
void ccid_card_free(CcidCard* card);

/**
 * Write the built-in sample card file to the SD card if it does not already
 * exist.  Creates the parent directories as needed.
 */
void ccid_card_write_sample(Storage* storage);

#ifdef __cplusplus
}
#endif
