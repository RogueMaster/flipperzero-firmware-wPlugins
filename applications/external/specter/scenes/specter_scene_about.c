#include "../specter_i.h"

void specter_scene_about_on_enter(void* context) {
    SpecterApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Specter " SPECTER_VERSION);
    widget_add_string_element(
        widget, 64, 14, AlignCenter, AlignTop, FontSecondary, "NFC reader bug-sweep");

    widget_add_text_scroll_element(
        widget,
        0,
        24,
        128,
        40,
        "Specter passively senses active 13.56 MHz\n"
        "NFC reader fields nearby - hidden POS\n"
        "skimmers, rogue door readers, covert\n"
        "loggers - using the onboard NFC chip's\n"
        "external-field detector. It NEVER transmits.\n"
        " \n"
        "SWEEP - find it\n"
        "Move slowly over a terminal, door reader or\n"
        "suspicious object. The meter and clicks rise\n"
        "as you near an emitter.\n"
        "  OK        reset peak/contacts\n"
        "  hold OK   log this reading\n"
        "  LEFT      calibrate to this room\n"
        " \n"
        "FINGERPRINT - identify it\n"
        "Hold still on the emitter. Specter times the\n"
        "carrier's on/off edges and calls it:\n"
        "CONTINUOUS (carrier held up), POLLING (fixed\n"
        "poll cycle, period shown) or INTERMITTENT.\n"
        "The pulse train at the bottom is the raw\n"
        "carrier the numbers came from.\n"
        "  OK        save finding to logbook\n"
        "  hold OK   restart the measurement\n"
        " \n"
        "SITE SURVEY - clear a room\n"
        "A timed sweep. Walk the space; at the end you\n"
        "get one verdict - CLEAN, TRACE or ACTIVE\n"
        "READER - with max/avg field, contacts and\n"
        "how much of the time a carrier was up.\n"
        "Auto-logged.\n"
        " \n"
        "WATCH MODE - stand guard\n"
        "Arm it and walk away. It watches without a\n"
        "time limit, counts contacts, remembers when\n"
        "the last one was, and wakes the screen and\n"
        "sounds off the instant a reader appears. It\n"
        "ignores stealth on purpose - an alert you\n"
        "cannot see is no alert.\n"
        "  OK        re-arm (clear count and clock)\n"
        " \n"
        "LOGBOOK - keep it\n"
        "Findings are appended with a timestamp to\n"
        "apps_data/specter/logbook.txt (grouped, for\n"
        "reading here) and logbook.csv (a flat table,\n"
        "for a spreadsheet). Both live on the SD card.\n"
        "Nothing leaves the device.\n"
        " \n"
        "Calibration measures the ambient noise floor\n"
        "for 3 s and sets the threshold just above it,\n"
        "saved as the Custom sensitivity. Stand\n"
        "somewhere quiet when you run it.\n"
        " \n"
        "Stealth mode keeps the screen and LED dark\n"
        "so the Flipper does not glow while you are\n"
        "the one doing the looking.\n"
        " \n"
        "Limits: 13.56 MHz (HF) only - it cannot see\n"
        "125 kHz (LF) readers. It senses a reader's\n"
        "carrier, not what it reads. Strength is\n"
        "relative proximity, not a calibrated range.\n"
        "Cadence is sampled every 2 ms, so timings\n"
        "near that are shown with a ~ and trusted\n"
        "less. CLEAN means clean at the sensitivity\n"
        "you chose - a dormant or shielded reader\n"
        "stays invisible to any of them.\n"
        " \n"
        "Use only where you are authorised. A\n"
        "defensive, listen-only tool.\n"
        " \n"
        "by at0m-b0mb\n"
        "github.com/at0m-b0mb/Specter-FlipperZero");

    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewWidget);
}

bool specter_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void specter_scene_about_on_exit(void* context) {
    SpecterApp* app = context;
    widget_reset(app->widget);
}
