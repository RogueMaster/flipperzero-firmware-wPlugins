/*
 * BioVault USB-HID keyboard output.
 * Types a vault field into the host, briefly switching USB from CDC to HID
 * and back (drops any open serial session for the duration).
 */
#pragma once

#include <stdbool.h>

typedef enum {
    BvHidOk, // typed successfully
    BvHidNoUsb, // HID mode but no host enumerated it
    BvHidBusy, // USB mode switch locked elsewhere
} BvHidResult;

// Type `text` (ASCII) over USB HID, then restore previous USB mode. Blocks
// (up to ~2s for host attach); run off the GUI thread.
BvHidResult bv_hid_type(const char* text);
