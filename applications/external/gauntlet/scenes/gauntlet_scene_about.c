#include "../gauntlet_i.h"

void gauntlet_scene_about_on_enter(void* context) {
    GauntletApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    FuriString* s = furi_string_alloc();
    furi_string_cat_str(s, "\e#Gauntlet " GAUNTLET_VERSION "\n");
    furi_string_cat_str(s, "An on-device CTF box\n\n");
    furi_string_cat_str(
        s,
        "Load challenge packs from the\n"
        "SD card and solve RFID, IR and\n"
        "cipher puzzles right on your\n"
        "Flipper. Capture flags, earn\n"
        "points, climb the ranks.\n\n");

    furi_string_cat_str(s, "\e#Controls\n");
    furi_string_cat_str(
        s,
        "In a challenge:\n"
        "  OK    - enter the flag\n"
        "  Right - open the Toolkit\n"
        "  Left  - show/hide the hint\n"
        "  Up/Dn - scroll the text\n\n"
        "In the Toolkit:\n"
        "  L/R   - switch decoder\n"
        "  Up/Dn - scroll / ROT shift\n"
        "  OK    - use output as flag\n\n");

    furi_string_cat_str(s, "\e#Toolkit\n");
    furi_string_cat_str(
        s,
        "ROT/Caesar (all 25 shifts),\n"
        "Base64, Hex, Binary, Morse,\n"
        "Atbash and Reverse - enough to\n"
        "crack most cipher tasks with no\n"
        "laptop in the room.\n\n");

    furi_string_cat_str(s, "\e#Make your own packs\n");
    furi_string_cat_str(
        s,
        "A pack is a plain-text .pack\n"
        "file. Drop it in:\n"
        "  apps_data/gauntlet/packs/\n\n"
        "Minimal example:\n"
        "  PACK My Workshop\n"
        "  CHALLENGE Warmup\n"
        "  CATEGORY cipher\n"
        "  POINTS 100\n"
        "  PROMPT Shift it back.\n"
        "  DATA Wkh iodj lv RIRIR\n"
        "  HINT ROT by 3\n"
        "  FLAG the flag is ROROR\n\n"
        "Flags are hashed, so a solver\n"
        "reading the file still can't\n"
        "read the answer if you ship\n"
        "ANSWER <hash> instead of FLAG.\n"
        "Full docs in the README.\n\n");

    furi_string_cat_str(s, "\e#Ethics\n");
    furi_string_cat_str(
        s,
        "Gauntlet is for learning and\n"
        "authorised practice. Solve only\n"
        "packs you own or are given.\n\n");

    furi_string_cat_str(s, "by at0m-b0mb\n");
    furi_string_cat_str(s, "github.com/at0m-b0mb/\nGauntlet-FlipperZero\n");

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
    furi_string_free(s);

    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewWidget);
}

bool gauntlet_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gauntlet_scene_about_on_exit(void* context) {
    GauntletApp* app = context;
    widget_reset(app->widget);
}
