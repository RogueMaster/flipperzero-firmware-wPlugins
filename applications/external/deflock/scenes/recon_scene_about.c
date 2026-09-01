// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"

#define RECON_ABOUT_TEXT              \
    "FlipDeFlock " RECON_VERSION "\n" \
    "Snoop onto them as\n"            \
    "they snoop onto us.\n \n"        \
    "Passive Flock / ALPR scan.\n"    \
    "Pairs your Flipper with an\n"    \
    "ESP32 to survey the radio\n"     \
    "for surveillance gear.\n"        \
    "FLOCK / ALPR DETECT\n"           \
    "Finds Flock Safety / ALPR\n"     \
    "cameras via an ESP32\n"          \
    "(Companion FW or Marauder).\n"   \
    "Matches OUIs, phone-home\n"      \
    "probes, SSID names & probe\n"    \
    "IE fingerprints. OUI-only\n"     \
    "hits are 'Possible' - verify\n"  \
    "by eye, never assume.\n \n"      \
    "LOCATOR (Companion)\n"           \
    "Hunt a marked device by\n"       \
    "signal: a hot/cold meter\n"      \
    "that climbs as you get\n"        \
    "closer. Works without GPS.\n"    \
    "No compass arrow (that needs\n"  \
    "a directional antenna).\n \n"    \
    "FLOCK MAP\n"                     \
    "On-device map of detected\n"     \
    "cameras around your GPS fix.\n"  \
    " \n"                             \
    "REPORTS\n"                       \
    "Marked finds export to\n"        \
    "Markdown, DeFlock GeoJSON,\n"    \
    "KML, CSV & WiGLE under\n"        \
    "apps_data/flipdeflock/\n"        \
    "reports. Share-to-DeFlock\n"     \
    "shows a QR to submit from\n"     \
    "your phone (no network).\n \n"   \
    "SIGNATURES\n"                    \
    "Drop apps_data/flipdeflock/\n"   \
    "signatures.json to add OUIs,\n"  \
    "SSID & IE-fp signatures.\n"      \
    "Load-only, offline,\n"           \
    "fail-safe.\n \n"                 \
    "WIRING\n"                        \
    "ESP32 on USART\n"                \
    "(pin13 TX / pin14 RX).\n"        \
    "GPS on LPUART (pin15/16)\n"      \
    "so both run together.\n \n"      \
    "BOARD MODE (Settings)\n"         \
    "Marauder: keep your board\n"     \
    "as-is, no flashing - Flock\n"    \
    "detect, GPS & reports.\n"        \
    "Companion: flash our FW\n"       \
    "(via 'ESP32 Firmware') to\n"     \
    "add Locator, BLE Flock\n"        \
    "detection & dual-band\n"         \
    "Flock scanning.\n \n"            \
    "THANKS\n"                        \
    "Thank you to @h00die for\n"      \
    "actively contributing to\n"      \
    "the project!\n \n"               \
    "SUPPORT\n"                       \
    "Free forever - donations\n"      \
    "never gate a feature.\n"         \
    "BTC + more: Support menu\n \n"   \
    "Companion FW + OUI data:\n"      \
    "see esp32_companion/ and\n"      \
    "deflock.org. ALPR passive;\n"    \
    "lawful authorized use only."

void recon_scene_about_on_enter(void* context) {
    ReconApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);
    FuriString* s = furi_string_alloc();
    furi_string_printf(
        s,
        "Mode: %s\n \n%s",
        app->settings.backend == EspBackendGeneric ? "Marauder (Flock)" : "Companion (all)",
        RECON_ABOUT_TEXT);
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
    furi_string_free(s);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void recon_scene_about_on_exit(void* context) {
    ReconApp* app = context;
    widget_reset(app->widget);
}
