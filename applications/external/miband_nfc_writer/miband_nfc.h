/**
 * @file miband_nfc.h
 * @brief Public header for Mi Band NFC Writer application
 * 
 * This is the main public header file for the Mi Band NFC Writer application.
 * It defines the opaque application structure type that is used throughout
 * the codebase.
 * 
 * The actual structure definition is in miband_nfc_i.h (internal header)
 * to maintain encapsulation and prevent external code from directly
 * accessing internal application state.
 */

#pragma once

/**
 * @brief Opaque application structure
 * 
 * This forward declaration creates an opaque type for the application structure.
 * External code can hold pointers to MiBandNfcApp but cannot access its
 * internal members directly.
 * 
 * The full structure definition is in miband_nfc_i.h and includes:
 * - GUI components (view dispatcher, scene manager, views)
 * - NFC components (scanner, poller, listener)
 * - Application state (loaded data, current operation, etc.)
 * 
 * This pattern provides encapsulation and allows the internal structure
 * to change without affecting external code.
 */
typedef struct MiBandNfcApp MiBandNfcApp;
