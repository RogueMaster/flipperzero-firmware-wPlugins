#include "../sniffer_card_app.h"

// A macro rather than a `static const char*`, because clang-format reflows the continuation
// lines of a long concatenated initializer to line up under the `=` and the screen text stops
// being readable in the source.
//
// Written as the job in order rather than as a description of the app. Someone opening this has
// a card in one hand and no idea which of the two menu entries comes first, and that is the one
// question worth answering on a 128x64 screen.
#define SNIFFER_CARD_HOWTO_TEXT    \
    "1. Set Sniffer UID\n"         \
    "Give the sniffer card the\n"  \
    "UID of the card you want\n"   \
    "keys for - from a tag, a\n"   \
    "saved dump, or typed in.\n"   \
    "\n"                           \
    "2. Hold it on the reader\n"   \
    "At least twice. Each tap\n"   \
    "records one login attempt,\n" \
    "and a key needs two of the\n" \
    "same sector to crack.\n"      \
    "\n"                           \
    "3. Read Sniffer Card\n"       \
    "Back here, read it, Save.\n"  \
    "\n"                           \
    "4. Recover the keys\n"        \
    "MFKey on the Flipper takes\n" \
    "minutes a pair and puts\n"    \
    "them in your user dict.\n"    \
    "qUnleashed on a PC does\n"    \
    "the same far faster.\n"       \
    "Then the NFC app can read\n"  \
    "the original card.\n"         \
    "\n"                           \
    "Reading twice is safe: a\n"   \
    "pair already in the log is\n" \
    "skipped, not added again.\n"  \
    "\n"                           \
    "Works with Gen2 cards, the\n" \
    "ones answering a plain\n"     \
    "30 xx read. Gen1 and Furui\n" \
    "are not supported yet."

void sniffer_card_app_scene_info_on_enter(void* context) {
    SnifferCardApp* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, SNIFFER_CARD_HOWTO_TEXT);

    view_dispatcher_switch_to_view(app->view_dispatcher, SnifferCardAppViewWidget);
}

bool sniffer_card_app_scene_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void sniffer_card_app_scene_info_on_exit(void* context) {
    SnifferCardApp* app = context;
    widget_reset(app->widget);
}
