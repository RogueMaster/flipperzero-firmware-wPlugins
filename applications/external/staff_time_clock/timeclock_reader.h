// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// Shared badge reader.
//
// Three technologies are supported, one at a time, picked by the caller:
//   NFC (13.56 MHz): ISO14443-3A poller (MIFARE Classic/Ultralight, NTAG, ...).
//   LF RFID (125 kHz): the lfrfid worker in auto mode (EM4100, HID, Indala, ...).
//   iButton (1-Wire): the ibutton worker (DS1990A / Dallas keys, ...).
//
// There is no automatic rotation between technologies and no timer involved
// anywhere in this file. An earlier version rotated all three on a timer so
// the user never had to pick one; every variant of that (fast, slow, paused
// after each read) eventually wedged the device on a long-running scan (Work
// mode) - repeatedly tearing down and recreating a radio (LF RFID and
// iButton each spin up their own worker thread) is enough alloc/free churn
// to eventually fail even at a slow rate. This is back to the original,
// never-reported-unstable design: the scene picks a TimeclockReaderTech
// (Left/Right in the UI) and the reader stays on it until told otherwise.
//
// The reader only reads the identifier (UID); it never writes to or emulates a
// card. Any existing badge works as an identity token - even one already used
// by another company - because nothing on it is modified.
//
// Thread-safety: radio worker threads only post a ViewDispatcher custom event
// back (READER_EVENT_UID); starting and stopping a radio always happens on
// the GUI thread, called directly by the scene (timeclock_reader_start_fixed
// / timeclock_reader_stop). Scenes must forward their custom events to
// timeclock_reader_handle_event().
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

// Start reading using only the given technology; the same radio stays
// allocated until stop() is called. continuous keeps reading after each UID
// (Work mode) instead of stopping after the first (single scan: Punch,
// register, replace chip) - for the NFC poller specifically this controls
// whether it is told to stop its own internal loop on a read, which matters
// for a clean shutdown (see timeclock_reader.c). Safe to call again after
// stop(), and safe to call directly to switch technology without stopping
// first (it stops the previous radio itself).
void timeclock_reader_start_fixed(
    TimeclockReader* reader,
    TimeclockReaderTech tech,
    bool continuous);

// Stop and release the radio (the reader object itself stays valid).
void timeclock_reader_stop(TimeclockReader* reader);

// Forward a ViewDispatcher custom event. Returns true if the reader handled it
// (a UID was read -> invoke the callback).
bool timeclock_reader_handle_event(TimeclockReader* reader, uint32_t event);
