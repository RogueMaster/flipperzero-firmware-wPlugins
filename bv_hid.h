/*
 * BioVault USB-HID keyboard output.
 *
 * Types a vault field (username / password / data) into the host as if from a
 * keyboard. This temporarily takes the Flipper's USB device from its CDC serial
 * mode (the CLI) into HID mode and restores it afterwards, so any open serial
 * session drops for the duration of the send.
 */
#pragma once

#include <stdbool.h>

typedef enum {
    BvHidOk, // typed successfully
    BvHidNoUsb, // switched to HID but no host enumerated it (unplugged / no driver)
    BvHidBusy, // USB mode switch is locked by something else
} BvHidResult;

// Type `text` (ASCII) over USB HID, then restore the previous USB mode. Blocks
// for the duration of the type (plus up to ~2s waiting for the host to attach
// the HID interface); run it off the GUI thread.
BvHidResult bv_hid_type(const char* text);
