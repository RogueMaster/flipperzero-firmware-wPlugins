#include "../ghosttag_i.h"

/*
 * Help and About in one screen, because the question somebody has when they
 * first open this app ("why does it say NO BOARD?") is the same question the
 * About screen exists to answer.
 *
 * No \e# / \ec / \er escapes anywhere below. The SDK documents \e# as a line
 * PREFIX meaning "bold until the next newline" - it is not a wrapper, so a
 * closing \e# is two literal characters and the '#' gets printed on screen.
 */
void ghosttag_scene_about_on_enter(void* context) {
    GhostTagApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_icon_element(widget, 2, 2, &I_ghost_10px);
    widget_add_string_element(
        widget, 16, 2, AlignLeft, AlignTop, FontPrimary, "GhostTag v" GHOSTTAG_VERSION);
    widget_add_string_element(
        widget, 16, 14, AlignLeft, AlignTop, FontSecondary, "BLE anti-stalking");

    widget_add_text_scroll_element(
        widget,
        0,
        26,
        128,
        38,
        "WHAT IT DOES\n"
        "Finds Bluetooth trackers -\n"
        "AirTags, Tiles and Samsung\n"
        "SmartTags - that stay\n"
        "with you, and warns you\n"
        "when one does.\n"
        " \n"
        "YOU NEED A BOARD\n"
        "The Flipper's own Bluetooth\n"
        "can advertise but cannot\n"
        "scan, so on its own it\n"
        "cannot see a tracker at\n"
        "all. GhostTag uses a\n"
        "BLE-capable ESP32 as its\n"
        "radio, on GPIO 13 and 14.\n"
        " \n"
        "THE OFFICIAL BOARD\n"
        "Flipper's own WiFi board\n"
        "is an ESP32-S2, and the\n"
        "S2 has no Bluetooth radio\n"
        "at all. It cannot work,\n"
        "and no firmware can fix\n"
        "that. Use an ESP32-S3,\n"
        "ESP32-C3, ESP32-C6 or a\n"
        "classic ESP32 instead.\n"
        " \n"
        "AIR CHECK (NO BOARD)\n"
        "The Flipper's own radio\n"
        "has one trick it CAN do:\n"
        "RF test mode measures raw\n"
        "energy on a channel. Air\n"
        "Check hops the three BLE\n"
        "advertising channels and\n"
        "tells you how busy the\n"
        "band is right here.\n"
        " \n"
        "That is useful context. A\n"
        "tag that persists in an\n"
        "empty garage means far\n"
        "more than one in a cafe.\n"
        "But it is ENERGY ONLY: it\n"
        "cannot read an address, a\n"
        "vendor or a payload, so\n"
        "it can never tell you a\n"
        "tracker is there. Only\n"
        "the ESP32 can do that.\n"
        " \n"
        "NO BOARD YET?\n"
        "Run Demo from the menu.\n"
        "It plays a scripted\n"
        "scenario through the real\n"
        "UI, so you can see what an\n"
        "alert looks like. Every\n"
        "demo screen is stamped\n"
        "DEMO, and nothing from a\n"
        "demo is ever written to\n"
        "the session log.\n"
        " \n"
        "READING THE DIAL\n"
        "Distance from the centre\n"
        "is real: it is the signal\n"
        "we measured. The angle is\n"
        "NOT a bearing. There is no\n"
        "compass and no antenna\n"
        "array in here. Each tag\n"
        "just keeps its own spot so\n"
        "you can watch it approach.\n"
        " \n"
        "WHAT FOLLOWING MEANS\n"
        "Only this: a known tracker\n"
        "type stayed in range for\n"
        "the whole dwell window,\n"
        "across enough sightings.\n"
        "There is no GPS here and\n"
        "no route matching, so it\n"
        "cannot tell a stalker from\n"
        "a tag sitting on a shelf\n"
        "you stood next to. Walk a\n"
        "few minutes and re-check.\n"
        " \n"
        "CONTROLS\n"
        "Radar  OK list, Left help\n"
        "Air    Left for help\n"
        "List   Up/Down, OK open\n"
        "Detail Left/Right for the\n"
        "       next tracker\n"
        "Alert  OK details,\n"
        "       Back dismiss\n"
        " \n"
        "LIMITS\n"
        "Find My tags rotate their\n"
        "address every 15 min or\n"
        "so, so a long tail can\n"
        "appear as several short\n"
        "records. A paired Apple\n"
        "device is usually its own\n"
        "owner, so it is listed but\n"
        "never graded a threat.\n"
        " \n"
        "A privacy tool. Use it on\n"
        "yourself, or with consent.\n"
        " \n"
        "by at0m-b0mb\n"
        "github.com/at0m-b0mb");

    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewAbout);
}

bool ghosttag_scene_about_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;
    /* Even here: somebody reading the help while a tag tails them still needs
     * to be told. */
    if(event.type == SceneManagerEventTypeTick) {
        ghosttag_poll_alert(app);
        return true;
    }
    return false;
}

void ghosttag_scene_about_on_exit(void* context) {
    GhostTagApp* app = context;
    widget_reset(app->widget);
}
