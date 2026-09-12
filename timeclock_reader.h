// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// Shared badge reader.
//
// NFC (13.56 MHz): uses NfcScanner to detect ANY supported protocol, then a
// poller to read the card, extracting the UID generically via NfcDevice. This
// covers ISO14443-A/B, FeliCa, ISO15693 (NFC-V) and anything else the firmware
// supports.
// LF RFID (125 kHz): the lfrfid worker in auto mode.
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
// hex ("04A1B2C3D4"); tech is "NFC" or "RFID".
typedef void (*TimeclockReaderCallback)(const char* uid_hex, const char* tech, void* context);

TimeclockReader* timeclock_reader_alloc(ViewDispatcher* view_dispatcher);
void timeclock_reader_free(TimeclockReader* reader);

void timeclock_reader_set_callback(
    TimeclockReader* reader,
    TimeclockReaderCallback callback,
    void* context);

// Start reading. use_lf selects LF RFID vs NFC; continuous keeps reading after
// each UID (Work mode) instead of stopping after the first (single punch).
// Safe to call again after stop().
void timeclock_reader_start(TimeclockReader* reader, bool use_lf, bool continuous);

// Stop and release the radio (the reader object itself stays valid).
void timeclock_reader_stop(TimeclockReader* reader);

// Forward a ViewDispatcher custom event. Returns true if the reader handled it
// (protocol detected -> start poll; UID read -> invoke callback).
bool timeclock_reader_handle_event(TimeclockReader* reader, uint32_t event);
