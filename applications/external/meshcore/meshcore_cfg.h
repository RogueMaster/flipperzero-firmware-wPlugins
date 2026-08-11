/*
 * MeshCore Config — Flipper Zero FAP by Greyrock Labs.
 *
 * Two modes over one link to the node:
 *   Configurator — read and change the node's radio settings and identity
 *   Messenger    — use the node as a radio: contacts, chats, adverts
 *
 * The radio is never on the Flipper. The node does the meshing and holds the
 * keys; this app is a UI for it, speaking the MeshCore companion protocol over
 * the hardware UART.
 *
 * Layering (see AGENTS.md):
 *   uart/            furi_hal_serial wrapper — moves bytes, knows no protocol
 *   protocol/        vendored meshcore_c, its UART binding, and the session
 *                    worker that owns the link for as long as we are connected
 *   messenger/       contact list and, later, message history
 *   scenes/          UI, one file per scene
 *   profiles/        *.json profiles from the SD card       (configurator, later)
 */
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/modules/loading.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>

#include "config/meshcore_apply.h"
#include "config/meshcore_preset_store.h"
#include "logger/meshcore_logger.h"
#include "meshcore_log.h"
#include "messenger/meshcore_contacts.h"
#include "messenger/meshcore_mailbox.h"
#include "messenger/meshcore_messages.h"
#include "messenger/meshcore_msglog.h"
#include "protocol/meshcore_session.h"
#include "scenes/meshcore_scene.h"

/* How often scenes get a tick event (used by the log view to refresh). */
#define MESHCORE_TICK_PERIOD_MS 500u

/* A single MeshCore text message is limited to 133 characters (per the
 * companion protocol; longer messages are meant to be split into [1/n] chunks).
 * We do not chunk, so the compose keyboard is capped here — better to hold at
 * one packet than to silently emit a message the mesh will not carry whole. */
#define MESHCORE_MSG_MAX 133u

/* View ids registered with the ViewDispatcher. One per UI module, not per
 * scene — several scenes reuse the same module. */
typedef enum {
    MeshCoreViewSubmenu,
    MeshCoreViewWidget,
    MeshCoreViewTextBox,
    MeshCoreViewLoading,
    MeshCoreViewTextInput,
    MeshCoreViewVarList,
    MeshCoreViewSplash,
} MeshCoreViewId;

/* What the node told us about itself. Filled by scene_connect from SELF_INFO
 * and DEVICE_INFO; the configurator editors read and modify it. */
typedef struct {
    bool valid;

    /* DEVICE_INFO */
    char model[MC_MAX_MODEL];
    char fw_ver[24];
    int8_t fw_ver_code;

    /* SELF_INFO */
    char name[MC_MAX_TEXT];
    /* This node's own 32-byte identity, from SELF_INFO. What "My card" shows and
     * what other nodes key their contact of us by. Zero until connected. */
    uint8_t public_key[32];
    uint32_t freq_khz; /* radio_freq, kHz — 869525 == 869.525 MHz */
    /* radio_bw is in Hz, not kHz: the reference client encodes set_radio's
     * bandwidth as bw_kHz * 1000, so 250 kHz goes over the wire as 250000.
     * The two radio fields genuinely use different scales. */
    uint32_t bw_hz;
    uint8_t sf;
    uint8_t cr;
    uint8_t tx_power; /* dBm */
    uint8_t max_tx_power; /* dBm */
    uint8_t role_type; /* advert type byte; see the Role caveat in AGENTS.md */
} MeshCoreNodeInfo;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    /* UI modules */
    Submenu* submenu;
    Widget* widget;
    TextBox* text_box;
    Loading* loading;
    TextInput* text_input;
    VariableItemList* var_list;
    /* A hand-drawn View for the animated splash — the standard modules cannot
     * do a per-frame canvas animation, so this one owns its own draw. */
    View* splash_view;
    FuriTimer* splash_timer;

    /* Transport + protocol */
    MeshCoreLog* log;
    MeshCoreSession* session;
    MeshCoreNodeInfo node;

    /* Messenger */
    MeshCoreContacts contacts;
    MeshCoreMessages messages;
    MeshCoreMailbox* mailbox;
    /* The durable copy of `messages` on the SD card: loaded into the ring at
     * startup, appended to as mail arrives and is sent. */
    MeshCoreMsgLog* msglog;
    /* Which conversation scene_chat is showing. Held as the peer's key rather
     * than a row number, because a contacts refresh can reorder the list. */
    uint8_t chat_peer[32];
    char chat_peer_name[MC_NAME_LEN + 1];
    /* The chat scene serves both a direct conversation and a channel. For a
     * channel, chat_is_channel is true and chat_channel_idx says which; the
     * peer key is unused and chat_peer_name holds the channel name. */
    bool chat_is_channel;
    uint8_t chat_channel_idx;
    /* Backing buffer for the compose TextInput. MC_MAX_TEXT sizes the buffer,
     * but a single MeshCore message is capped at MESHCORE_MSG_MAX characters —
     * the keyboard is limited to that so an over-length line cannot go out
     * un-split (the spec asks for [1/n] chunking above this, which we do not do,
     * so we hold the line at one packet's worth). */
    char compose_buf[MC_MAX_TEXT];
    /* Backing buffer for the Identity editor's TextInput (the node name). A
     * name is at most MC_NAME_LEN; a separate buffer keeps it from clobbering a
     * half-typed message. */
    char identity_buf[MC_NAME_LEN + 1];
    /* Backing buffer for the Add-contact "Import from link" keyboard. Holds a
     * meshcore://contact/add?… share link; MC_MAX_TEXT covers a 64-hex key plus
     * a name, and its own buffer keeps it clear of a half-typed message. */
    char import_buf[MC_MAX_TEXT];

    /* Logger — owns its own session, on whichever port the node is on. */
    MeshCoreLogger* logger;

    /* Radio presets: built-ins always, card presets once Profiles has been
     * opened. Held on the app so the list survives moving between Profiles and
     * Apply without re-reading the card. */
    MeshCorePresetStore presets;
    size_t preset_index;
    /* One MeshCoreApplyResult per step; the enum lives in the apply scene
     * because nothing else has an opinion about it. */
    uint8_t apply_result[MeshCoreApplyStepCount];
    /* The node's clock, from CURR_TIME. Contact ages are relative to this and
     * not to the Flipper's RTC, because that is the timebase last_advert is
     * expressed in. Zero means "not read yet". */
    uint32_t node_time;

    /* Scene named by the launch argument, opened once the dispatcher is
     * running rather than before it. Entering a scene that starts a worker
     * before view_dispatcher_run() means that worker posts into a queue nobody
     * is serving yet, which the logger scene reliably crashes on.
     * MeshCoreSceneNum means "no argument". */
    MeshCoreSceneId launch_scene;

    /* Worker thread owned by whichever scene is currently running one. */
    FuriThread* worker;
    volatile bool worker_stop;
    /* Set by the worker, read by the GUI thread after the completion event.
     * Always a string literal, so there is no ownership to worry about. */
    const char* worker_error;

    /* TextBox stores the pointer it is given rather than copying, and the GUI
     * thread may be drawing from it at any moment. Two buffers let the log
     * view build the next snapshot without touching the one on screen. */
    FuriString* text_buf[2];
    uint8_t text_buf_slot;
} MeshCoreApp;

/* Bring the shared session up if it is not already: start the UART and do the
 * APP_START / DEVICE_QUERY handshake, filling app->node. A no-op when already
 * connected. Returns NULL on success or a short reason to show.
 *
 * This is what lets the messenger and configurator connect themselves — the
 * user should not have to visit a Connect screen before using them. It blocks
 * on the link, so call it from a worker thread, never the GUI thread. Defined
 * in scenes/meshcore_scene_connect.c. */
const char* meshcore_connect_ensure(MeshCoreApp* app);

/* Allocate the splash's hand-drawn View (draw/input callbacks + model). Defined
 * in scenes/meshcore_scene_splash.c, called from the app's view setup. */
View* meshcore_scene_splash_view_alloc(MeshCoreApp* app);
