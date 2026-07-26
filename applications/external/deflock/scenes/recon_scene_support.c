// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../views/deflock_qr_view.h"

// Bitcoin receiving address rendered as a QR on the Support screen.
//
// MUST be a self-custody address (Sparrow, Electrum, or a hardware wallet) —
// never an exchange deposit address. An exchange address is registered to a
// KYC'd legal identity, so publishing one permanently links this pseudonymous
// project to that identity via a third party, and leaves custody with a company
// that can freeze the account.
//
// Leave empty to hide the QR entirely; the screen then shows the other funding
// links instead, so an unconfigured build is harmless.
//
// Native SegWit (bc1q..., P2WPKH), signed by a Trezor Safe 3 in a dedicated
// account. Bech32 checksum verified before publishing.
#define RECON_BTC_ADDRESS "bc1qavy2wdhgpvqturn5he76mxclqr0a3vhg9sj4l8"

// BIP21 scheme prefix, so a scanning wallet treats this as a payment request
// rather than as plain text.
#define RECON_BTC_URI "bitcoin:"

// Wrap column for the on-screen address. The secondary font fits a little over
// 20 characters across the full width, and a hand-copied address must be exact,
// so it is split rather than truncated.
#define RECON_BTC_WRAP 20

void recon_scene_support_on_enter(void* context) {
    ReconApp* app = context;

    // No address configured: fall back to the text-only funding screen. Never
    // render an empty QR — a scannable code that resolves to nothing is worse
    // than no code at all.
    if(RECON_BTC_ADDRESS[0] == '\0') {
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
            "never gate a feature.\n \n"
            "github.com/sponsors/\n"
            "ReconGrunt\n"
            "ko-fi.com/recongrunt\n");
        view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
        return;
    }

    char uri[sizeof(RECON_BTC_URI) + sizeof(RECON_BTC_ADDRESS)];
    snprintf(uri, sizeof(uri), "%s%s", RECON_BTC_URI, RECON_BTC_ADDRESS);

    // Show the address under the QR too, so it can still be read off the screen
    // if the scan fails. Chunked at a fixed column rather than split once: a
    // legacy address is 34 chars, native SegWit 42, Taproot 62, and any line
    // longer than the wrap column runs off the right edge.
    char body[96];
    size_t w = 0;
    w += (size_t)snprintf(body, sizeof(body), "BTC - scan or type:");
    const char* a = RECON_BTC_ADDRESS;
    size_t alen = strlen(a);
    for(size_t i = 0; i < alen && w < sizeof(body) - 1; i += RECON_BTC_WRAP) {
        size_t chunk = alen - i;
        if(chunk > RECON_BTC_WRAP) chunk = RECON_BTC_WRAP;
        int n = snprintf(body + w, sizeof(body) - w, "\n%.*s", (int)chunk, a + i);
        if(n <= 0) break;
        w += (size_t)n;
    }

    // The QR view is shared with Share-to-DeFlock, which installs a Left/Right
    // pager. Clear it so paging here can't walk the camera list behind our back.
    deflock_qr_view_set_page_callback(app->deflock_qr_view, NULL, NULL);

    // total = 0 suppresses the "n/m" pager header; this is a single fixed payload.
    deflock_qr_view_set_content(app->deflock_qr_view, uri, 0, 0, "Support", "FlipDeFlock", body);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewDeflockQr);
}

bool recon_scene_support_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void recon_scene_support_on_exit(void* context) {
    ReconApp* app = context;
    if(RECON_BTC_ADDRESS[0] == '\0') {
        widget_reset(app->widget);
    } else {
        deflock_qr_view_set_empty(app->deflock_qr_view);
    }
}
