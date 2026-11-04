/**
 * @file sniffer_card_app.h
 * @brief Reading a Super Sniffer card's captured authentications off it.
 */
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <dialogs/dialogs.h>
#include <loader/loader.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include <nfc/nfc.h>

#include "sniffer_card.h"
#include "sniffer_card_nonce_log.h"
#include "sniffer_card_poller.h"
#include "scenes/scenes.h"

/** First body line under a FontPrimary title.
 *
 * Three body lines fit above a button row and no more. The buttons anchor at
 * canvas_height() - 12, so a button box covers rows 52..63 and is drawn after the text - a fourth
 * line is not clipped, it is painted over, which looks exactly like the text simply ending.
 * FontSecondary steps 11 rows, so from here the third line ends at 45 and a fourth would start at
 * 49. A screen with no buttons gets four.
 */
#define SNIFFER_CARD_TEXT_Y 16

/** Where the MFKey app lives when it is installed, spelled as the firmware's NFC app spells it
 * (`NFC_MFKEY32_APP_PATH` in `applications/main/nfc/nfc_app_i.h`).
 *
 * Checked rather than assumed: MFKey is a separate app and plenty of Flippers do not have it, in
 * which case offering to run it would be a button that fails.
 */
#define SNIFFER_CARD_MFKEY_PATH EXT_PATH("apps/NFC/mfkey.fap")

/** The longest UID these cards come in, and so the longest one that can be set on one. */
#define SNIFFER_CARD_UID_MAX 7

typedef struct {
    Gui* gui;
    DialogsApp* dialogs;
    NotificationApp* notifications;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    ByteInput* byte_input;
    Popup* popup;
    Submenu* submenu;
    Widget* widget;

    // The radio and the session over it. Both outlive every scene: the read scene starts and
    // stops the session, and allocating the Nfc instance per scene would drop the field between
    // a failed read and a retry.
    Nfc* nfc;
    SnifferCardPoller* poller;

    // What the last read came back with, copied out of the poller so the result screen does not
    // depend on the session still being alive. The verdict is part of that copy: the result
    // screen has to tell a tag that is not one of these cards from one that is and has captured
    // nothing yet, and the two differ only in how far the read got.
    SnifferCardPollerResult result;
    SnifferTrace traces[SNIFFER_CARD_TRACE_SLOTS];
    size_t trace_count;
    /** The read stopped before the card ran out of slots, so captures were left behind. */
    bool read_truncated;

    // The UID the sniffer card is to be given, however it was come by: read off the card the keys
    // are wanted for, or typed in. A typed one has no SAK, which is why nothing past the confirm
    // screen reads that field - the write checks the sniffer card's own SAK, not the target's.
    SnifferCardIdentity target;

    // Edited in place by the byte input, and deliberately not target.uid: backing out of a
    // half-typed UID must not destroy the one already in hand.
    uint8_t uid_input[SNIFFER_CARD_UID_MAX];
    size_t uid_input_len;

    /** Whether the MFKey app is installed. Resolved once: it is an SD stat through the storage
     * service, and no other app can install one while this one is in the foreground. */
    bool mfkey_installed;

    // One scratch string, reused by whichever screen is building its text, rather than an
    // allocation per screen.
    //
    // Safe to reuse because every Widget element copies what it is given - each of them does
    // furi_string_alloc_set() on construction - and because scene_manager runs a scene's on_exit,
    // and so its widget_reset(), before the next scene's on_enter. The string's contents after
    // that point belong to nobody.
    FuriString* text;
} SnifferCardApp;

typedef enum {
    SnifferCardAppViewByteInput,
    SnifferCardAppViewPopup,
    SnifferCardAppViewSubmenu,
    SnifferCardAppViewWidget,
} SnifferCardAppView;

// Submenu item indices travel down the same uint32_t channel as these, so everything here stays
// above any index a submenu in this app can produce.
typedef enum {
    SnifferCardAppCustomEventPollerDone = 100, /**< The NFC session reached a verdict. */
    /** Named for what they do, not for which button carries them. A handler given a button
     * position has to work out the action again - which on the trace result screen meant
     * re-pairing every capture and re-reading the whole nonce log to recover a decision the
     * screen had already made when it chose the label. */
    SnifferCardAppCustomEventSaveNonces,
    SnifferCardAppCustomEventRunMfkey,
    SnifferCardAppCustomEventRetry,
    SnifferCardAppCustomEventNext, /**< Right button on a screen that only moves forward. */
} SnifferCardAppCustomEvent;

/** Take the result of a finished session out of the poller and into the app. */
void sniffer_card_app_take_result(SnifferCardApp* app);

/** Hand over to MFKey and quit.
 *
 * The nonces are in a file by now, so there is nothing to pass along and nothing to come back
 * for - MFKey writes what it recovers into the user dictionary, and the next useful step is the
 * NFC app rather than this one. The firmware's NFC app re-enqueues itself here; this does not,
 * because a recovery run takes minutes and returning to a stale result screen afterwards would
 * be a surprise rather than a convenience.
 */
void sniffer_card_app_run_mfkey(SnifferCardApp* app);

/** Write a UID out as spaced hex bytes, the way the NFC app shows one. */
void sniffer_card_app_uid_text(const uint8_t* uid, size_t uid_len, FuriString* text);
