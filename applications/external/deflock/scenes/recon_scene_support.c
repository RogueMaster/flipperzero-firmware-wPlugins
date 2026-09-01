// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"
#include "../helpers/plugin_host.h"
#include "../plugins/qr_plugin_api.h"
#include "../views/deflock_qr_view.h"

// Donation addresses rendered as QR codes on the Support screen, one page per
// currency (Up/Down or Left/Right to switch). Values mirror README.md's
// funding table -- keep the two in sync.
//
// Every address MUST be self-custody (Sparrow, Electrum, or a hardware
// wallet) -- never an exchange deposit address. An exchange address is
// registered to a KYC'd legal identity, so publishing one permanently links
// this pseudonymous project to that identity via a third party, and leaves
// custody with a company that can freeze the account.
//
// Leave an `address` empty to drop that one entry; if every entry ends up
// empty the screen falls back to plain text, so an unconfigured build is
// harmless.
typedef struct {
    const char* name; // shown top-right, e.g. "Bitcoin"
    const char* note; // short format/network note, e.g. "Native SegWit"
    const char* uri_prefix; // BIP21-style scheme; "" if the address is already a full URI
    const char* address;
} RECON_DonateEntry;

// bc1q...        : Native SegWit (P2WPKH), signed by a Trezor Safe 3 in a
//                  dedicated account. Bech32 checksum verified before publishing.
// 0x48...        : ERC-20/EVM address, 40 hex chars. Verified against
//                  etherscan.io before publishing.
// ltc1q...       : Native SegWit, verified against blockchair.com before publishing.
// bitcoincash:.. : CashAddr format already carries its own URI scheme, so it
//                  gets no separate prefix below.
static const RECON_DonateEntry RECON_DONATE[] = {
    {"Bitcoin", "Native SegWit", "bitcoin:", "bc1qavy2wdhgpvqturn5he76mxclqr0a3vhg9sj4l8"},
    {"Ethereum", "ERC-20 chain", "ethereum:", "0x481be1838e6B51B1a4013633877Bd967E2484694"},
    {"Litecoin", "Native SegWit", "litecoin:", "ltc1qpj3ppsgcdvx5yara9suljeq83t32macr8pe2yt"},
    {"Bitcoin Cash", "CashAddr", "", "bitcoincash:qzl3a9emduev23nuh6wvk2nzwl5gguc9egmg83ae5a"},
};
#define RECON_DONATE_COUNT (sizeof(RECON_DONATE) / sizeof(RECON_DONATE[0]))

// Wrap column for the on-screen address. The secondary font fits a little over
// 20 characters across the full width, and a hand-copied address must be exact,
// so it is split rather than truncated.
#define RECON_DONATE_WRAP 20

// Indices into RECON_DONATE that have a non-empty address, built on_enter so a
// blanked-out entry is skipped rather than shown as a dead QR.
static int g_don_map[RECON_DONATE_COUNT];
static int g_don_count;
static int g_don_idx;

/** QR encoder plugin, mapped in only while this screen is open. NULL when it
 *  could not be loaded -- the view then shows its "QR n/a" fallback. */
static PluginHost* g_qr_plugin = NULL;

// Build the current entry's payload and push it to the shared QR view. Same
// shape as recon_scene_deflock_handoff.c's per-camera render.
static void recon_scene_support_show(ReconApp* app) {
    const RECON_DonateEntry* e = &RECON_DONATE[g_don_map[g_don_idx]];

    char uri[16 + 96];
    snprintf(uri, sizeof(uri), "%s%s", e->uri_prefix, e->address);

    // No header line here (e.g. "BTC - scan or type:") -- the currency name and
    // note are already shown top-right, and the screen has room for at most 3
    // text lines below the QR (see the gap comment in deflock_qr_view.c). Every
    // current address wraps to exactly 3 chunks at RECON_DONATE_WRAP=20, so
    // spending one of those 3 lines on a redundant label would cut the last
    // chunk of every entry, right when the QR fails to scan and the address is
    // the only way to actually donate.
    //
    // NOTE: snprintf() returns what it WOULD have written, so accumulating the
    // raw return value overshoots `w` past the buffer on truncation and the next
    // iteration's `sizeof(body) - w` underflows as a size_t. Clamp every append.
    // The loop guard alone is not enough -- it catches the NEXT iteration, not an
    // overshoot within the current one, and BCH's 54-char CashAddr (the longest
    // entry, prefix included) puts this close to the edge.
    char body[128];
    size_t w = 0;
    const char* a = e->address;
    size_t alen = strlen(a);
    for(size_t i = 0; i < alen && w < sizeof(body) - 1; i += RECON_DONATE_WRAP) {
        size_t chunk = alen - i;
        if(chunk > RECON_DONATE_WRAP) chunk = RECON_DONATE_WRAP;
        int n = snprintf(body + w, sizeof(body) - w, "%s%.*s", w ? "\n" : "", (int)chunk, a + i);
        if(n <= 0) break;
        size_t avail = sizeof(body) - w - 1;
        w += ((size_t)n > avail) ? avail : (size_t)n;
    }

    deflock_qr_view_set_content(
        app->deflock_qr_view, uri, g_don_idx, g_don_count, e->name, e->note, body);
}

// Forward the QR view's Up/Down/Left/Right paging into the scene as a custom
// event carrying the signed delta (-1/+1). Casting to uint32_t and back
// round-trips.
static void recon_scene_support_page_cb(void* context, int delta) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)delta);
}

void recon_scene_support_on_enter(void* context) {
    ReconApp* app = context;

    g_don_count = 0;
    for(size_t i = 0; i < RECON_DONATE_COUNT; i++) {
        if(RECON_DONATE[i].address[0] != '\0') g_don_map[g_don_count++] = (int)i;
    }
    if(g_don_idx >= g_don_count) g_don_idx = 0;

    // No address configured at all: fall back to the text-only funding screen.
    // Never render an empty QR -- a scannable code that resolves to nothing is
    // worse than no code at all.
    if(g_don_count == 0) {
        widget_reset(app->widget);
        widget_add_text_scroll_element(
            app->widget,
            0,
            0,
            128,
            64,
            "SUPPORT\n \n"
            "FlipDeFlock is free and\n"
            "stays free. Donations\n"
            "never gate a feature.\n");
        view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
        return;
    }

    // Map the encoder in for the lifetime of this screen. A failure here is not
    // fatal: set_api(NULL) makes the view draw the "QR n/a" text fallback, and
    // the address is still readable and hand-enterable below it.
    const QrPluginApi* qr_api = NULL;
    g_qr_plugin = plugin_host_load(QR_PLUGIN_APP_ID, QR_PLUGIN_API_VERSION, (const void**)&qr_api);
    deflock_qr_view_set_api(app->deflock_qr_view, qr_api);

    deflock_qr_view_set_page_callback(app->deflock_qr_view, recon_scene_support_page_cb, app);

    recon_scene_support_show(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewDeflockQr);
}

bool recon_scene_support_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(g_don_count > 0) {
            int next = g_don_idx + (int)event.event;
            if(next < 0) next = g_don_count - 1;
            if(next >= g_don_count) next = 0;
            g_don_idx = next;
            recon_scene_support_show(app);
        }
        consumed = true;
    }
    return consumed;
}

void recon_scene_support_on_exit(void* context) {
    ReconApp* app = context;
    if(g_don_count == 0) {
        widget_reset(app->widget);
    } else {
        // Drop the borrowed pointer BEFORE unmapping the plugin it points into,
        // so a later redraw of a stale model can never call through freed code.
        deflock_qr_view_set_api(app->deflock_qr_view, NULL);
        plugin_host_free(g_qr_plugin);
        g_qr_plugin = NULL;
    }
    g_don_idx = 0;
}
