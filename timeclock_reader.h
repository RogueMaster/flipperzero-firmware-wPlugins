// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// Shared badge reader.
//
// Three technologies are supported:
//   NFC (13.56 MHz): ISO14443-3A poller (MIFARE Classic/Ultralight, NTAG, ...).
//   LF RFID (125 kHz): the lfrfid worker in auto mode (EM4100, HID, Indala, ...).
//   iButton (1-Wire): the ibutton worker (DS1990A / Dallas keys, ...).
//
// timeclock_reader_start() round-robins all three on a timer (one radio at a
// time, lighter on RAM/RF than all three at once) for short single-shot scans
// (Punch, register, replace chip), where a session lasts a few seconds at
// most. timeclock_reader_start_fixed() instead runs exactly one technology,
// picked by the caller, with no rotation timer at all - meant for a scan that
// stays open for a long time (Work mode). Rotating forever there proved
// fragile in practice (the repeated radio alloc/free eventually wedged the
// device even at a slow rate), so the long-running case trades automatic
// technology detection for a Left/Right technology picker in the UI instead.
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

// A single radio technology, for timeclock_reader_start_fixed().
typedef enum {
    TimeclockReaderTechNfc = 0,
    TimeclockReaderTechRfid,
    TimeclockReaderTechIButton,
    TimeclockReaderTechCount,
} TimeclockReaderTech;

// Short display label for a technology ("NFC" / "RFID" / "iBTN").
const char* timeclock_reader_tech_label(TimeclockReaderTech tech);

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

// Start reading using only the given technology - no rotation, no timer, the
// same radio stays allocated until stop() is called. Always continuous (the
// caller keeps scanning until it decides to leave). Safe to call again after
// stop(), and safe to call directly to switch technology without stopping
// first (it stops the previous radio itself).
void timeclock_reader_start_fixed(TimeclockReader* reader, TimeclockReaderTech tech);

// Stop and release the radio (the reader object itself stays valid).
void timeclock_reader_stop(TimeclockReader* reader);

// Forward a ViewDispatcher custom event. Returns true if the reader handled it
// (protocol detected -> start poll; UID read -> invoke callback).
bool timeclock_reader_handle_event(TimeclockReader* reader, uint32_t event);
