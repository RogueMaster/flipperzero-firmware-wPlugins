#include "../faraday_i.h"

/* The grade scale, read out of the grading engine rather than retyped.
 *
 * fdy_rating_blurb() existed, was unit-tested, and was shown to nobody: the
 * plain-English meaning of a grade lived only in the source, so the one screen
 * whose job is to explain the app never explained the thing the app produces.
 * Building the table from fdy_grade.c's own functions also means this text
 * cannot drift away from the engine the way a hand-written list would. */
static void faraday_about_append_scale(FuriString* out) {
    /* The engine's own thresholds, by name - see fdy_grade.h. */
    static const int16_t db[FdyRatingCount] = {
        FDY_DB_APLUS, FDY_DB_A, FDY_DB_B, FDY_DB_C, FDY_DB_D, 0};
    static const uint8_t pct[FdyRatingCount] = {
        FDY_PCT_APLUS, FDY_PCT_A, FDY_PCT_B, FDY_PCT_C, FDY_PCT_D, 0};

    furi_string_cat_printf(out, "\e#The grades\n");
    furi_string_cat_printf(out, "dB is Sub-GHz, %% is NFC.\n");
    for(uint8_t r = 0; r < (uint8_t)FdyRatingCount; r++) {
        FdyRating g = (FdyRating)r;
        if(g == FdyRatingF) {
            furi_string_cat_printf(
                out, "\e#%s  under %d dB / %d%%\n", fdy_rating_letter(g), db[4], pct[4]);
            // screen-ok: "A+  under 10 dB / 20%"
        } else {
            furi_string_cat_printf(
                out, "\e#%s  %d dB+ / %d%%+\n", fdy_rating_letter(g), db[r], pct[r]);
            // screen-ok: "A+  60 dB+ / 98%+"
        }
        furi_string_cat_printf(out, "%s\n", fdy_rating_blurb(g));
    }
    furi_string_cat_printf(out, "\n");
}

void faraday_scene_about_on_enter(void* context) {
    FaradayApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    /* Built at runtime rather than being one long literal, so the grade table
     * can come from the grading engine. Every line below is word-wrapped to
     * the 120 px the scrolling text element actually has: it wraps on WIDTH,
     * not on word boundaries, so a line that overflows comes back with its
     * last word sliced in half. tools_check_text.py enforces this. */
    FuriString* text = furi_string_alloc();
    furi_string_cat_printf(
        text,
        "\e#Faraday " FARADAY_VERSION "\n"
        "\n"
        "Prove your signal-blocking\n"
        "pouch actually works.\n"
        "\n"
        "\e#What goes in the bag\n"
        "The two tests are\n"
        "opposites. Get this the\n"
        "wrong way round and the\n"
        "grade is meaningless.\n"
        "\n"
        "\e#Sub-GHz: bag the FOB\n"
        "The fob is the transmitter.\n"
        "Seal the FOB, keep the\n"
        "Flipper outside listening.\n"
        "\n"
        "\e#NFC: bag the FLIPPER\n"
        "The reader's field comes\n"
        "from outside. The Flipper\n"
        "is standing in for the card\n"
        "in your wallet, so seal the\n"
        "FLIPPER.\n"
        "\n"
        "\e#The test\n"
        "Every test is two captures\n"
        "and a grade:\n"
        "1. BASELINE - the signal\n"
        "   in the open air.\n"
        "2. SHIELDED - the same\n"
        "   signal, pouch sealed.\n"
        "The gap is the attenuation.\n"
        "\n");

    faraday_about_append_scale(text);

    furi_string_cat_printf(
        text,
        "\e#Sub-GHz (key fob)\n"
        "The CC1101 measures\n"
        "your fob's carrier in real\n"
        "dBm. Press the fob in the\n"
        "open, lock it, seal the fob in\n"
        "the pouch, press it again.\n"
        "The dB drop is graded A+ to\n"
        "F.\n"
        "\n"
        "\e#NFC (card)\n"
        "Hold the Flipper in a\n"
        "reader's 13.56 MHz field\n"
        "(a phone doing NFC works),\n"
        "lock the baseline, then seal\n"
        "the Flipper in the pouch and\n"
        "measure again. The score\n"
        "is how much of the field the\n"
        "pouch keeps out.\n"
        "\n"
        "\e#Find my band\n"
        "A fob transmits on ONE\n"
        "band, and which one\n"
        "depends on where the car\n"
        "was sold: 315 MHz across\n"
        "much of North America,\n"
        "433.92 across Europe,\n"
        "868 and 915 elsewhere.\n"
        "\n"
        "Hold the fob against the\n"
        "Flipper and keep pressing.\n"
        "It sweeps all four, and if\n"
        "one is clearly loudest it\n"
        "offers to save it. If\n"
        "nothing stands out it says\n"
        "so rather than guessing -\n"
        "naming the wrong band\n"
        "would make every later\n"
        "reading a measurement of\n"
        "noise.\n"
        "\n"
        "\e#Leak hunt\n"
        "A grade tells you a pouch\n"
        "leaks. This finds WHERE.\n"
        "Seal the fob, hold its button,\n"
        "and sweep the Flipper along\n"
        "the seams, zip and corners.\n"
        "The meter, the word and the\n"
        "clicks all peak over the\n"
        "escaping spot. OK resets\n"
        "the peak.\n"
        "\n"
        "\e#Saved results\n"
        "Every finished test is\n"
        "appended to a CSV on the\n"
        "SD card, so you can\n"
        "measure three pouches and\n"
        "compare them later instead\n"
        "of trusting memory.\n"
        "\n"
        "\e#Honest limits\n"
        "- Relative, not a lab spec.\n"
        "  It compares two readings\n"
        "  from one setup, so keep\n"
        "  the distance and angle\n"
        "  constant or the number is\n"
        "  noise.\n"
        "- Sub-GHz needs YOUR fob\n"
        "  to transmit. No press, no\n"
        "  read.\n"
        "- NFC needs an external\n"
        "  reader field to measure\n"
        "  against.\n"
        "- A >= prefix means the\n"
        "  signal fell below the noise\n"
        "  floor: at least that good,\n"
        "  possibly better.\n"
        "- 13.56 MHz only. No 125\n"
        "  kHz.\n"
        "\n"
        "Listen-only. Faraday never\n"
        "transmits.\n"
        "\n"
        "\e#Credits\n"
        "by at0m-b0mb\n"
        "MIT licensed\n"
        "github.com/at0m-b0mb/\n"
        "Faraday-FlipperZero\n");

    /* the widget copies the string into its own element */
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewAbout);
}

bool faraday_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void faraday_scene_about_on_exit(void* context) {
    FaradayApp* app = context;
    widget_reset(app->widget);
}
