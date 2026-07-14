#pragma once

#include "ccid_emulator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start CCID emulation.
 *
 * - Saves the current USB interface.
 * - Sets CCID callbacks.
 * - Switches USB to CCID mode with the chosen VID/PID preset.
 * - Inserts the virtual smartcard.
 *
 * @param app  application state (must have a loaded card)
 */
void ccid_handler_start(CcidEmulatorApp* app);

/**
 * Stop CCID emulation.
 *
 * - Removes the virtual smartcard.
 * - Restores the previous USB interface.
 */
void ccid_handler_stop(CcidEmulatorApp* app);

#ifdef __cplusplus
}
#endif
