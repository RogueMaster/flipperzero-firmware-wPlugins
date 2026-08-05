// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

// On-device reference for every mark this app can put on a screen.
//
// WHY THIS EXISTS. The header learned to report faults precisely -- !PORT, !PIN,
// !FW, and before those a whole vocabulary of confidence letters and tags -- and
// that is worth nothing to someone who cannot find out what a mark means. A user
// hit !PORT on his own device and said, exactly: "I don't know what it means and
// have no way of finding out." Naming a fault in five characters is the half of
// the job the header has room for; this is the other half.
//
// FORMAT RULES, learned the hard way. The first version of this page was flowing
// prose hand-broken at a column limit, which put breaks mid-sentence ("GPS and
// the ESP are / on the SAME UART. They") and read as garbled on the device.
// Every line here is a SHORT COMPLETE PHRASE that stands on its own, ~24
// characters or fewer -- the same shape the About page uses and the reason that
// page reads cleanly. A section break is "\n \n" exactly once; doubling it
// leaves a two-line hole.
//
// Ordered fault-first on purpose: someone opening this page is nearly always
// looking at a mark they do not recognise, and it is usually the one stopping
// something from working.
#define RECON_HELP_TEXT             \
    "GPS BADGE\n"                   \
    "GPS 9  locked, 9 sats\n"       \
    "GPS    on, searching\n"        \
    "!PORT  UART clash\n"           \
    "!PIN   pin refused\n"          \
    "!FW    no relay in FW\n \n"    \
    "!PORT\n"                       \
    "GPS + ESP on one UART.\n"      \
    "They cannot share it.\n"       \
    "Fix: Settings, GPS Port\n"     \
    "GPS on LPUART 15/16,\n"        \
    "ESP on USART 13/14.\n \n"      \
    "!PIN\n"                        \
    "Board refused that pin.\n"     \
    "Not a usable GPIO, or it\n"    \
    "carries flash or the\n"        \
    "Flipper link.\n"               \
    "Fix: Settings, ESP GPS\n"      \
    "Pin. The board reports\n"      \
    "its own valid pins.\n \n"      \
    "!FW\n"                         \
    "Companion never answered.\n"   \
    "Its firmware has no GPS\n"     \
    "relay.\n"                      \
    "Fix: reflash it from\n"        \
    "ESP32 Firmware.\n \n"          \
    "NO GPS CHIP?\n"                \
    "Many ESP32 boards have\n"      \
    "no GNSS at all. Then no\n"     \
    "setting can help.\n \n"        \
    "SCAN HEADER\n"                 \
    "wifi 55/s  frames/sec\n"       \
    "bt 490     BLE adverts\n"      \
    "bt -       no BLE scan yet\n"  \
    "a2         alerts sent\n"      \
    "!r1        ESP reset\n"        \
    "!d3        RX lines lost\n"    \
    "!DEAUTH    flood nearby\n \n"  \
    "NO BEEP?\n"                    \
    "If a2 climbs but you\n"        \
    "hear nothing, the app\n"       \
    "fired and the Flipper\n"       \
    "swallowed it. Check\n"         \
    "Flipper Notifications,\n"      \
    "and Alert on hit here.\n"      \
    "Settings has Test alert.\n \n" \
    "ROW MARKS\n"                   \
    "!   CONFIRMED by SSID\n"       \
    "L   Likely\n"                  \
    "F   IE-fp class match\n"       \
    "p   OUI only. Expect\n"        \
    "    false positives.\n"        \
    "ST  acoustic sensor\n"         \
    "[hid] no SSID, unscored\n"     \
    "*   you marked it\n"           \
    "A time like 6h means a\n"      \
    "STORED hit, not live.\n \n"    \
    "FEW DETECTIONS?\n"             \
    "Check Board Mode fits\n"       \
    "your firmware, and that\n"     \
    "frames/sec is moving.\n"       \
    "On a C5, Settings Band\n"      \
    "Both sweeps 41 channels\n"     \
    "not 13, so each camera\n"      \
    "is seen a third as\n"          \
    "often. Flock is 2.4GHz.\n \n"  \
    "Detections are\n"              \
    "INDICATORS, never proof."

void recon_scene_help_on_enter(void* context) {
    ReconApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, RECON_HELP_TEXT);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_help_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void recon_scene_help_on_exit(void* context) {
    ReconApp* app = context;
    widget_reset(app->widget);
}
