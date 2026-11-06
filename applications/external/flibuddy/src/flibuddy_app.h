/* The FliBuddy FAP: one custom view and the glue between the transports,
 * the NFC reader, the hello handshake (link_select.h), the screen reducer
 * (app_state.h) and the screens (ui_frame.h, ui_draw.h).
 *
 * The app runs the transports its Connection setting uses: USB, Bluetooth
 * or both (Auto). Each has a slot; link_select picks the slot whose host
 * said hello first, and only that host's lookups and results count and
 * only it gets scans and intents.
 *
 * Every state change goes through flibuddy_reduce(), which performs the
 * reducer's effect (send a scan or an intent, save the settings, switch the
 * transport, exit), plays its feedback cue, runs the NFC reader on the
 * screens that read (app_reader_wanted) and rebuilds the view's frame.
 *
 * Threads: the GUI thread owns everything below except a slot's rx, which
 * that transport's worker fills and hands over through rx_queue, read_dump,
 * which the NFC worker fills before it posts FliBuddyEventTagRead,
 * FliBuddyEventReadFailed or FliBuddyEventTagUnsupported and then ends, and the view's frame, which the
 * GUI service draws under the view model's lock. Every source file that
 * uses the SDK includes an SDK header itself, so that the host test build
 * (flipper/tests/Makefile) leaves it out.
 */
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification.h>

#include "app_state.h"
#include "intents.h"
#include "link_select.h"
#include "nfc_read.h"
#include "proto_decode.h"
#include "transport.h"

/* The fap_version of application.fam (tools/tests/test_flipper_app.py). */
#define FLIBUDDY_VERSION   "1.0"
#define FLIBUDDY_HELLO_APP "flibuddy-fap/" FLIBUDDY_VERSION

typedef enum {
    FliBuddyViewMain,
} FliBuddyView;

typedef enum {
    FliBuddyEventRx = 100, /* messages wait in rx_queue */
    FliBuddyEventTagDetected, /* NFC worker */
    FliBuddyEventTagRead, /* NFC worker, read_dump is filled */
    FliBuddyEventReadFailed, /* NFC worker */
    FliBuddyEventTagUnsupported, /* NFC worker */
} FliBuddyEvent;

typedef struct FliBuddyApp FliBuddyApp;

/* A message as a transport worker hands it to the GUI thread. */
typedef struct {
    TransportKind via;
    uint32_t session; /* the slot's session it arrived in */
    ProtoMessage msg;
} FliBuddyRx;

/* One transport: its own decode buffer (its worker's), and a session
 * counter bumped when its port closes, so lines it queued before are
 * dropped without touching another transport's. */
typedef struct {
    FliBuddyApp* app;
    TransportKind kind;
    Transport* transport; /* transport_none() when not running */
    bool port_open;
    volatile uint32_t session;
    FliBuddyRx rx; /* this transport's worker only */
} FliBuddySlot;

_Static_assert(LINK_SLOTS == TRANSPORT_KINDS, "one link per transport");

struct FliBuddyApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* view; /* model: UiFrame */
    NotificationApp* notifications;

    FliBuddySlot slot[TRANSPORT_KINDS]; /* indexed by TransportKind */
    FuriMessageQueue* rx_queue; /* FliBuddyRx */
    FliBuddyRx rx_msg; /* GUI thread */
    char* tx_buf; /* PROTO_LINE_MAX */

    TagReader* reader;
    TagDump* read_dump;

    LinkSelect links;
    char host_app[PROTO_HELLO_FIELD_MAX + 1]; /* from the active host's hello */
    uint32_t anim; /* ticks, for the animations */
    AppState state;
    SingleSlotIntentStore intents;
};

/* Stamps the event with the clock, applies it to the state, performs its
 * effect and feedback, syncs the reader and redraws. */
void flibuddy_reduce(FliBuddyApp* app, const AppEvent* event);

/* Shorthand for events without data. */
void flibuddy_reduce_type(FliBuddyApp* app, AppEventType type);
