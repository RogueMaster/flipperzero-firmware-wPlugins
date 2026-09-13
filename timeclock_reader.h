// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// Shared badge reader.
//
// The three technologies are scanned in a round-robin: one radio is active per
// time slice and a timer rotates NFC -> RFID -> iButton, so only one radio is
// powered at a time (light on RAM and the RF front end) while all three still
// work with no manual selection.
//   NFC (13.56 MHz): ISO14443-3A poller (MIFARE Classic/Ultralight, NTAG, ...).
//   LF RFID (125 kHz): the lfrfid worker in auto mode (EM4100, HID, Indala, ...).
//   iButton (1-Wire): the ibutton worker (DS1990A / Dallas keys, ...).
//
// The reader only reads the identifier (UID); it never writes to or emulates a
// card. Any existing badge works as an identity token - even one already used
// by another company - because nothing on it is modified.
//
// Thread-safety: radio callbacks only post ViewDispatcher custom events; every
// start/stop of the radio happens on the GUI thread inside
// timeclock_reader_handle_event(). Scenes must forward their custom events to
// that function.
// =============================================================================

#include <furi.h>
#include <gui/view_dispatcher.h>
#include "timeclock_storage.h" // TC_UID_STR_MAX, TC_TECH_MAX

typedef struct TimeclockReader TimeclockReader;

// Invoked (on the GUI thread) when a UID has been read. uid_hex is uppercase
// hex ("04A1B2C3D4"); tech is "NFC", "RFID" or "iBTN" (whichever detected it).
typedef void (*TimeclockReaderCallback)(const char* uid_hex, const char* tech, void* context);

TimeclockReader* timeclock_reader_alloc(ViewDispatcher* view_dispatcher);
void timeclock_reader_free(TimeclockReader* reader);

void timeclock_reader_set_callback(
    TimeclockReader* reader,
    TimeclockReaderCallback callback,
    void* context);

// Start reading. NFC, LF RFID and iButton are scanned in rotation (one per
// slice) with no manual reader selection, so all three badge types work side by
// side in the same deployment. continuous keeps reading after each UID (Work
// mode) instead of stopping after the first (single punch); it also uses a
// much longer slice, since Work mode can stay open for hours and rotating
// fast forever wedges the device (see timeclock_reader.c). Safe to call again
// after stop().
void timeclock_reader_start(TimeclockReader* reader, bool continuous);

// Stop and release the radio (the reader object itself stays valid).
void timeclock_reader_stop(TimeclockReader* reader);

// Forward a ViewDispatcher custom event. Returns true if the reader handled it
// (protocol detected -> start poll; UID read -> invoke callback).
bool timeclock_reader_handle_event(TimeclockReader* reader, uint32_t event);
