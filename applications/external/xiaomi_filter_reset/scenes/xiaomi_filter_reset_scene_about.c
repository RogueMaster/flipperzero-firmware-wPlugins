// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset_scene_about.c
 * @brief About / help scene.
 */
#include "../xiaomi_filter_reset_i.h"

// Hand-wrapped for the Flipper's ~128px-wide screen. The text-scroll widget breaks
// over-long lines mid-word, so every line here is pre-wrapped at word boundaries and
// kept short enough to fit; wide all-caps tokens (NTAG213, GPL-3.0-or-later) sit on
// their own short lines. A leading "\e#" makes a line bold until its "\n" (headers).
#define XIAOMI_FILTER_RESET_ABOUT_TEXT   \
    "\e#Xiaomi Filter Reset\n"           \
    "Clears the filter-life\n"           \
    "counter on Xiaomi air\n"            \
    "purifier filter tags, so\n"         \
    "the appliance reports\n"            \
    "100% again.\n"                      \
    "\n"                                 \
    "\e#How to use\n"                    \
    "1. Choose \"Reset filter life\".\n" \
    "2. Hold the filter's NFC\n"         \
    "tag against the back of\n"          \
    "the Flipper.\n"                     \
    "3. Wait for the confirmation.\n"    \
    "\n"                                 \
    "\"Check filter life\" is a\n"       \
    "read-only probe: it shows\n"        \
    "the state and writes\n"             \
    "nothing to the tag.\n"              \
    "\n"                                 \
    "\e#How it works\n"                  \
    "The filter carries an\n"            \
    "NTAG213 tag. Its\n"                 \
    "password is derived from\n"         \
    "the tag UID; the app\n"             \
    "authenticates and writes\n"         \
    "zeros to the\n"                     \
    "usage-counter page.\n"              \
    "\n"                                 \
    "\e#Notes\n"                         \
    "Resetting does not clean\n"         \
    "the filter. Replace or\n"           \
    "clean the media for air\n"          \
    "quality; use this only to\n"        \
    "silence a premature\n"              \
    "\"replace filter\" warning.\n"      \
    "Use on filters you own.\n"          \
    "\n"                                 \
    "\e#Credits\n"                       \
    "Reverse engineering:\n"             \
    "flamingo-tech and\n"                \
    "unethical.info.\n"                  \
    "\n"                                 \
    "License:\n"                         \
    "GPL-3.0-or-later"

void xiaomi_filter_reset_scene_about_on_enter(void* context) {
    XiaomiFilterResetApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, XIAOMI_FILTER_RESET_ABOUT_TEXT);
    view_dispatcher_switch_to_view(app->view_dispatcher, XiaomiFilterResetViewWidget);
}

bool xiaomi_filter_reset_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void xiaomi_filter_reset_scene_about_on_exit(void* context) {
    XiaomiFilterResetApp* app = context;
    widget_reset(app->widget);
}
